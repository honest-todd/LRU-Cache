/*
 * csim.c - A cache simulator that can replay traces from Valgrind
 *     and output statistics such as number of hits, misses, and
 *     evictions.  The replacement policy is LRU.
 *
 * Implementation and assumptions:
 *  1. Each load/store can cause at most one cache miss. (I examined the trace,
 *  the largest request I saw was for 8 bytes).
 *  2. Instruction loads (I) are ignored, since we are interested in evaluating
 *  trans.c in terms of its data cache performance.
 *  3. data modify (M) is treated as a load followed by a store to the same
 *  address. Hence, an M operation can result in two cache hits, or a miss and a
 *  hit plus an possible eviction.
 *
 * The function printSummary() is given to print output.
 * Please use this function to print the number of hits, misses and evictions.
 * This is crucial for the driver to evaluate your work.
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

//#define DEBUG_ON
#define ADDRESS_LENGTH 64



/* Type: Memory address */
typedef unsigned long long int mem_addr_t;

/*
 * Data structures to represent the cache we are simulating
 *
 * TODO: Define your own!
 *
 * E.g., Types: cache, cache line, cache set
 * (you can use a counter to implement LRU replacement policy)
 */
 typedef struct {
   mem_addr_t tag;
   char used;
   unsigned int lru_counter;
 } cache_block_t;


/* Globals set by command line args */
int verbosity = 0; /* print trace if set */
int s = 0; /* set index bits */
int b = 0; /* block offset bits */
int E = 0; /* associativity */
char* trace_file = NULL;

/* Derived from command line args */
int S; /* number of sets */
int B; /* block size (bytes) */

/* Counters used to record cache statistics */
int miss_count = 0;
int hit_count = 0;
int eviction_count = 0;

cache_block_t* cache_set;
cache_block_t** cache;
int g_lru_counter;
int g_min_lru_counter;
/*
 * initCache - Allocate memory (with malloc) for cache data structures (i.e., for each of the sets and lines per set),
 * writing 0's for valid and tag and LRU
 *
 * TODO: Implement
 *
 */
void initCache()
{
  g_lru_counter = 0;
  cache = (cache_block_t** )calloc(sizeof(cache_block_t*), S );
  for (int i=0; i < S; i++ ){
    cache [i] = (cache_block_t*)calloc(sizeof(cache_block_t), E);
  }
}


/*
 * freeCache - free allocated memory
 *
 * This function deallocates (with free) the cache data structures of each
 * set and line.
 *
 * TODO: Implement
 */
void freeCache()
{
  for (int i = 0; i < S;i++) {
    free(cache[i]);
  }
  free(cache);
}
/*
 * accessData - Access data at memory address addr
 *   If it is already in cache, increase hit_count
 *   If it is not in cache, bring it in cache, increase miss count.
 *   Also increase eviction_count if a line is evicted.
 *
 * TODO: Implement
notes:
 */

void accessData(mem_addr_t addr)
{
  mem_addr_t set_index = (addr >> b) & (mem_addr_t)(pow(2, s) - 1);
  mem_addr_t new_tag = addr >> (s + b);
  cache_block_t* cur_set = cache [set_index];
//
// HIT
// Block already in our cache
//
  for (int i = 0; i < E; i++ )
  {
    if (cur_set[i].tag == new_tag && cur_set[i].used == 1 )
    {
      hit_count++;
      cur_set[i].lru_counter = g_lru_counter++;
      return;
    }
  }

  miss_count++;
//
// MISS
//
  unsigned int page_to_insert = 0;
  char is_found = 0;
//
// COMPULSORY MISS: first access of a block of data.
//    Not all blocks are full....
for (int i = 0; i < E  && is_found == 0; i++ )
   {
    if ( cur_set[i].used == 0 )
    {
      page_to_insert = i;
      is_found = 1;
    }
  }

//
// AT this point each block should be full.
//
// CAPACITY MISS: each block is full,
//
// evict page with lowest lru counter (should be at back of array)
// and update things accordingly
//

if(is_found == 0) {
  eviction_count++;
  int min_lru_counter = cur_set[0].lru_counter;

  for (int i = 0; i < E && is_found == 0; i++)
  {
    if(cur_set[i].lru_counter < min_lru_counter ) {
      page_to_insert = i;
    }
  }

  if ( cur_set[page_to_insert].used == 1 )
    {
      is_found = 1;
    }
  }
//
// INSERT NEW BLOCK
//
  if(is_found == 1)
    {
      cur_set[page_to_insert].tag = new_tag;
      cur_set[page_to_insert].used = 1;
      cur_set[page_to_insert].lru_counter = g_lru_counter++;
    }

}


/*
 * replayTrace - replays the given trace file against the cache
 *
 * This function:
 * - opens file trace_fn for reading (using fopen)
 * - reads lines (e.g., using fgets) from the file handle (may name `trace_fp` variable)
 * - skips lines not starting with ` S`, ` L` or ` M`
 * - parses the memory address (unsigned long, in hex) and len (unsigned int, in decimal)
 *   from each input line
 * - calls `access_data(address)` for each access to a cache line
 *
 */
void replayTrace(char* trace_fn) {
  char* buffer = (char*)malloc(sizeof(char) * 20);
  FILE* f = fopen(trace_fn, "r");
  while(!feof(f)) {
    fgets(buffer, 20, f);
    char* token = strtok(&buffer[1], " ");
    if (token) {
      int modify = !strcmp(token, "M");
      if (strcmp(token, "S") && strcmp(token, "L") && strcmp(token, "M")) {
        continue;
      }

      token = strtok(NULL, ",");
      if (!token) {
        continue;
      }

      unsigned long address = strtol(token, NULL, 16);

      // We ignore the length part of the trace

      accessData(address);
      if (modify) {
        accessData(address);
      }
    }
  }
}

/*
 * printUsage - Print usage info
 */
void printUsage(char* argv[])
{
    printf("Usage: %s [-hv] -s <num> -E <num> -b <num> -t <file>\n", argv[0]);
    printf("Options:\n");
    printf("  -h         Print this help message.\n");
    printf("  -v         Optional verbose flag.\n");
    printf("  -s <num>   Number of set index bits.\n");
    printf("  -E <num>   Number of lines per set.\n");
    printf("  -b <num>   Number of block offset bits.\n");
    printf("  -t <file>  Trace file.\n");
    printf("\nExamples:\n");
    printf("  linux>  %s -s 4 -E 1 -b 4 -t traces/yi.trace\n", argv[0]);
    printf("  linux>  %s -v -s 8 -E 2 -b 4 -t traces/yi.trace\n", argv[0]);
    exit(0);
}

/*
 *
 * !! DO NOT MODIFY !!
 *
 * printSummary - Summarize the cache simulation statistics. Student cache simulators
 *                must call this function in order to be properly autograded.
 */
void printSummary(int hits, int misses, int evictions)
{
    printf("hits:%d misses:%d evictions:%d\n", hits, misses, evictions);
    FILE* output_fp = fopen(".csim_results", "w");
    assert(output_fp);
    fprintf(output_fp, "%d %d %d\n", hits, misses, evictions);
    fclose(output_fp);
}

/*
 * main - Main routine
 */
int main(int argc, char* argv[])
{
    char c;

    while( (c=getopt(argc,argv,"s:E:b:t:vh")) != -1){
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
        case 'v':
            verbosity = 1;
            break;
        case 'h':
            printUsage(argv);
            exit(0);
        default:
            printUsage(argv);
            exit(1);
        }
    }

    /* Make sure that all required command line args were specified */
    if (s == 0 || E == 0 || b == 0 || trace_file == NULL) {
        printf("%s: Missing required command line argument\n", argv[0]);
        printUsage(argv);
        exit(1);
    }

    /* Compute S, E and B from command line args */
    S = (unsigned int) pow(2, s);
    B = (unsigned int) pow(2, b);

    /* Initialize cache */
    initCache();

#ifdef DEBUG_ON
    printf("DEBUG: S:%u E:%u B:%u trace:%s\n", S, E, B, trace_file);
#endif

    replayTrace(trace_file);

    /* Free allocated memory */
    freeCache();

    /* Output the hit and miss statistics for the autograder */
    printSummary(hit_count, miss_count, eviction_count);
    return 0;
}
