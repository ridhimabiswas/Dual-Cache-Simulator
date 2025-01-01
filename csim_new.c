/*
FOR UNIQUE CACHE
GROUP MEMBERS: Ridhima Biswas, Jessica Bohn, Armaan Saleem,
Luke Busacca, Nikitha Cherukupalli

LOCATION: Rutgers University
CLASS: CS211 Computer Architecture
SESSION: Fall 2024
PROFESSOR: Dr. Tina Burns

DESCRIPTION:
This program implements a simulated dual cache that uses
frequency and recency conditions to move and evict data.

NOTES:
freqCheck() - checks if data falls in top or bottom 50%
              of frequency in cache
initCache () - initializes dual caches
freeCache() - frees dual caches
accessData() - checks for hits and misses in caches, moves
               data across caches and evicts
replayTrace() - gets data and calls accessData() for
                each line in trace file
See comments preceding functions for more details.

 */
#include <getopt.h>
#include <stdlib.h>
#include <unistd.h>
#include <stdio.h>
#include <assert.h>
#include <math.h>
#include <limits.h>
#include <string.h>
#include <errno.h>
#include "cachelab.h"


//defining the length of each addresses
#define ADDRESS_LENGTH 64

//variable to store address
typedef unsigned long long int mem_addr_t;


//in a 2d array we have an item of type cache_line in each cell
//in a cache_line, the following components we care about are:
typedef struct cache_line {
  char valid; //validity
  mem_addr_t tag; //the memory address of data storing
  unsigned long long int frq; //frequency counter: increments whenever we hit the data
  int accessed_time; //stores the time when it was last accessed
} cache_line_t;

//defining the 2-d array structure
typedef cache_line_t* cache_set_t;
typedef cache_set_t* cache_t;


//input that is store in the variables
int s = 0;
int b = 0;
int E = 0;
char* trace_file = NULL;

//(later in code) - derived from inputs s and b
int S;
int B;


//what we are interested in keeping track of
int eviction_count = 0;
int miss_count = 0;
int hit_count = 0;

mem_addr_t set_index_mask;

//our cache simulation
cache_t w_cache; //2-d array west cache: for newer less used data
cache_t e_cache;//2-d array east cache: accessed earlier but frquency is high
int time = 0; //keeps track of # accessdata calls to compare against time of access
int n = 0;//number of items in west cache



/*

FREQCHECK - method to check whether given data falls in top or bottom 50% frequency


Just using existing west cache, keeping count of how many items are greater than the
cache line we are interested in, dividing this count and comparing it with n/2
(where n is number of items in west cache will give if top or bottom 50% frequency)


*/
int freqCheck(cache_line_t cache_line_of_interest, int index_interest) {

	int num_items_greater = 0; //keep track of # of items > frequency threshold


	mem_addr_t tag = cache_line_of_interest.tag; //extract the tag address
	int freq = cache_line_of_interest.frq; //extract the frequency of the input cache line


	for (int i = 0; i < S; i++) { //looping through the whole 2-d west cache
		for (int j = 0; j < E; j++) {
			if ((!(i == index_interest && w_cache[i][j].tag == tag)) && (w_cache[i][j].frq > freq)) {
				num_items_greater++; //increment if greater than freq of input
			}
		}
	}
	if (num_items_greater > (n/2)) {
		return 0; //bottom frequency
	}
	return 1; //top 50%
}

/*

initialization of both west and east cache

*/
void initCache()
{
	//initialize both west and east caches
	int i, j;
	w_cache = (cache_set_t*) malloc(sizeof(cache_set_t) * S);
	e_cache = (cache_set_t*) malloc(sizeof(cache_set_t) * S);
	
	for (i = 0; i < S; i++){
		w_cache[i]=(cache_line_t*) malloc(sizeof(cache_line_t) * E);
		e_cache[i]=(cache_line_t*) malloc(sizeof(cache_line_t)* E);
		
		for (j=0; j<E; j++){
			w_cache[i][j].valid = 0;
			e_cache[i][j].valid = 0;
			w_cache[i][j].tag = 0;
			e_cache[i][j].tag = 0;
			w_cache[i][j].frq = 0;
			e_cache[i][j].frq = 0;
			w_cache[i][j].accessed_time = 0;
			e_cache[i][j].accessed_time = 0;
		}
	}

	/* Computes set index mask */
	set_index_mask = (mem_addr_t) (pow(2, s) - 1);
}



/*

When we are done using both the west and east caches,
we just have to free both west and east cache arrays.

*/
void freeCache()
{

	// must free both caches
	for (int i = 0; i < S; i++){
		free(w_cache[i]);
		free(e_cache[i]);
	}
	free(w_cache);
	free(e_cache);
}


/*

1. increment time, extract necesary information (set_index, tag, west and east cache sets)

2. first we search both caches at the same time to search for HITS
	- if there is a hit in either the west or east cache, increment the hitcount
	- if there is a hit in west cache -> increment the frequency by 1 in the frequency
          tracker AND set the access_time = time
	- RETURN (do not do the below steps)

3. if we are in step 3, we know that there is a miss so increment miss count

4. check if there is any empty spots in the w_cache with the valid tag -> if there is just put
the address in the spot and RETURN

****at this point, will be determining which w_cache_line to kick out and where to put it

5. traverse through the data to see which one has the longest time since it has been accessed
(find the smallest accessed_time variable)

6. when we find this longest time since last accessed (call it old_data) -> subject to the
frequency check (call the frquenecy check function)

7. frequency check results:
	- if the old_data is in BOTTOM 50% (returns 0 from freq function) -> we evict it entirely
		- increase eviction count
		- replace old_data line with information of the new data we want to make space for
	- if the old_data is in the top (return 1) -> put into east cache
		- find a spot for this old_data to be put into in the east cache
			- if there are empty spots in east cache -> put it there
			- if there are no empty spots, use a replacement policy to kick out
                          something from east_cache and find a spot for the old data
*/
void accessData(mem_addr_t addr)
{

	time++;
	mem_addr_t set_index = (addr >> b) & set_index_mask; //extract set bit
	mem_addr_t tag = addr >> (s+b); //extract tag

	cache_set_t w_cache_set = w_cache[set_index];
	cache_set_t e_cache_set = e_cache[set_index];


/*	Check if data is in the set */
	for(int i = 0; i < E; i++){
        	if(w_cache_set[i].tag==tag && w_cache_set[i].valid){
            		hit_count++;
            		w_cache_set[i].frq = w_cache_set[i].frq + 1; //update frequency
			w_cache_set[i].accessed_time = time; //update the time this data was accessed
            		return;
        	}
		if (e_cache_set[i].tag == tag && e_cache_set[i].valid) {
			hit_count++;
			e_cache_set[i].frq = e_cache_set[i].frq + 1; //update freq
                        e_cache_set[i].accessed_time = time; //update time
                        return;

		}
	}

/*	MISS SECTION 	*/

	miss_count++; //At this point we will have a miss

	//Check empty spots in the west cache
	for (int i = 0; i < E; i++) {
		//If empty spot, add this data
		if (!w_cache_set[i].valid) {
			w_cache_set[i].valid = 1;
			w_cache_set[i].tag = tag;
			w_cache_set[i].frq = 1;
			w_cache_set[i].accessed_time = time;
			n++;
			return;
		}
	}

	//No empty spots in west cache, have to kick out
	//Find the oldest data
	int oldest = 0;

	for (int i = 1; i < E; i++) {
		if (w_cache_set[i].accessed_time < w_cache_set[oldest].accessed_time) {
			oldest = i;
		}
	}

	//Subject oldest data to a frequency check
	if (!(freqCheck(w_cache_set[oldest], set_index))) { //If returning bottom 50%  (returning 1)
		//Evict entirely from west cache
		w_cache_set[oldest].valid = 1;
		w_cache_set[oldest].tag = tag;
		w_cache_set[oldest].frq = 1;
		w_cache_set[oldest].accessed_time = time;
		
		eviction_count++;
		return;
	}
	else { //Otherwise it will be the top 50% of the west cache
		//Check if empty spots in the east cache
		for (int i = 0; i < E; i++) {
			if (!e_cache_set[i].valid) { //If there is an empty spot in the east cache

                e_cache_set[i].valid = 1;
                e_cache_set[i].tag = w_cache_set[oldest].tag;
                e_cache_set[i].frq = w_cache_set[oldest].frq;
                e_cache_set[i].accessed_time = w_cache_set[oldest].accessed_time;

				//Set the west cache to the data that was a miss
				w_cache_set[oldest].valid = 1;
                w_cache_set[oldest].tag = tag;
                w_cache_set[oldest].frq = 1;
                w_cache_set[oldest].accessed_time = time;

                return;
            }
		}

		/* At this point, we have a full west cache as well at that index
        Eviction policy for the east cache that right now I am implementing is least_used data 
		(find the minimum frequency: Can change this later when we get the code to work) */
		
		int least_used = 0;
		//Find the minimum frequency in the set
		for (int i = 1; i<E; i++) {
            if (w_cache_set[i].frq < w_cache_set[least_used].frq) {
                least_used = i;
            }
        }
		//Now that we have the least used data, kick it out
        e_cache_set[oldest].valid = 1;
        e_cache_set[least_used].tag = w_cache_set[oldest].tag;
        e_cache_set[least_used].frq = w_cache_set[oldest].frq;
        e_cache_set[least_used].accessed_time = w_cache_set[oldest].accessed_time;
		
		eviction_count++;

		//Add the new data to the oldest index
		w_cache_set[oldest].valid = 1;
		w_cache_set[oldest].tag = tag;
		w_cache_set[oldest].frq = 1;
		w_cache_set[oldest].accessed_time = time;
	}

}


void replayTrace(char* trace_fn)
{
    char buf[1000];
    mem_addr_t addr = 0;
    unsigned int len = 0;
    FILE* trace_fp = fopen(trace_fn, "r");

    if(!trace_fp){
        fprintf(stderr, "%s: %s\n", trace_fn, strerror(errno));
        exit(1);
    }

    while( fgets(buf, 1000, trace_fp) != NULL) {
        if(buf[1]=='S' || buf[1]=='L' || buf[1]=='M') {
            sscanf(buf+3, "%llx,%u", &addr, &len);

            accessData(addr);

            /* If the instruction is R/W then access again */
            if(buf[1]=='M')
                accessData(addr);
        }
    }

    fclose(trace_fp);
}


int main(int argc, char* argv[])
{
	char c;

	while ((c=getopt(argc,argv,"s:E:b:t:")) != -1){
		switch(c){
		case 's':
			s = atoi(optarg);
			break;
		case 'E':
			E = atoi(optarg);
			break;
		case 'b':
			b = atoi(optarg);
            		break;
		case 't':
			trace_file = optarg;
			break;
		default:
			exit(1);
		}
	}

	/* Make sure that all required command line args were specified */
	if (s == 0 || E == 0 || b == 0 || trace_file == NULL) {
		printf("%s: Missing required command line argument\n", argv[0]);
		exit(1);
	}

	/* Compute S, E and B from command line args */
	S = (unsigned int) pow(2, s);
	B = (unsigned int) pow(2, b);

	/* Initialize cache */
	initCache();

	replayTrace(trace_file);

	/* Free allocated memory */
 	freeCache();

	/* Output the hit and miss statistics for the autograder */
 	printSummary(hit_count, miss_count, eviction_count);

	return 0;
}


