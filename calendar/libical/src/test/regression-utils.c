#include "ical.h"

#include <stdlib.h>	/* abort() */
#include <string.h>	/* strcmp() */

static char ictt_str[1024];
int VERBOSE = 0;
int QUIET = 0;

const char* ical_timet_string(const time_t t)
{
    struct tm stm = *(gmtime(&t));    

    sprintf(ictt_str,"%02d-%02d-%02d %02d:%02d:%02d Z",stm.tm_year+1900,
	    stm.tm_mon+1,stm.tm_mday,stm.tm_hour,stm.tm_min,stm.tm_sec);

    return ictt_str;
    
}

const char* ictt_as_string(struct icaltimetype t)
{
    const char *zone = icaltimezone_get_tzid((icaltimezone *)t.zone);

    if (icaltime_is_utc(t))
	sprintf(ictt_str,"%02d-%02d-%02d %02d:%02d:%02d Z UTC",
	t.year,t.month,t.day, t.hour,t.minute,t.second);
    else
	sprintf(ictt_str,"%02d-%02d-%02d %02d:%02d:%02d %s",
	t.year,t.month,t.day, t.hour,t.minute,t.second,
	zone == NULL? "(floating)": zone);

    return ictt_str;
}

char* icaltime_as_ctime(struct icaltimetype t)
{
    time_t tt;
 
    tt = icaltime_as_timet(t);
    sprintf(ictt_str,"%s",ctime(&tt));

    return ictt_str;
}


/** This variable is used to control whether we core dump on errors **/
static int die_on_errors = 0;
static int testnumber    = 0;
static int failed        = 0;
static int current_set   = 0;

static struct {int set; int test;} failed_tests[1024];

void die_on_errors_set(int val) {
  die_on_errors = 1;
}

void _ok(char* test_name, int success, char *file, int linenum, char *test) {
  testnumber++;

  if (!QUIET || (QUIET && !success))
  printf("%sok %d - %s\n", (success)?"" : "not ", testnumber, test_name);
  if (!success) {
    failed_tests[failed].set  = current_set;
    failed_tests[failed].test = testnumber;
    failed++;

    printf("# test failed: \"%s\"\n", test);
    printf("#          at: %s:%-d\n", file, linenum);
  }

  if (die_on_errors == 1 && !success) {
    abort();
  }
}

void _is(char* test_name, const char* str1, const char* str2, char *file, int linenum) {
  int diff;
  
  if (str1 == NULL || str2 == NULL) {
    diff = 1;
  } else {
    diff = strcmp(str1, str2);
  }

  if (!test_name) test_name = "()";

  _ok(test_name, (diff==0), file, linenum, "");
  
  if (diff) {
    printf("#      got: %s\n", str1 ? str1 : "(null)");
    printf("# expected: %s\n", str2 ? str2 : "(null)");
  }
}

void _int_is(char* test_name, int i1, int i2, char *file, int linenum) {
  _ok(test_name, (i1==i2), file, linenum, "");
  
  if (i1!=i2) {
    printf("#      got: %d\n", i1);
    printf("# expected: %d\n", i2);
  }
}


void verbose(int newval) {
  VERBOSE = newval;
}

void test_start(int numtests) {
  if (numtests) {
    if (!QUIET)
    printf("1..%-d\n", numtests);
  } else {
    if (!QUIET)
    printf("1..\n");
  }
}

void test_header(char *header, int set) {
  if (!QUIET)
  printf("########## %-40s (%d) ##########\n", header, set);
  current_set = set;
}

void test_end(void) {
  int pct;

  if (failed) {
    int i, oldset = 0;
    
    pct = ((testnumber - failed)*100)/testnumber;
    printf("\n        Failed %d/%d tests, %2d%% okay\n", failed, testnumber, pct);
    printf("\n        Failed tests:\n          ");
    for (i = 0; i < failed; i++) {
      int this_set = failed_tests[i].set;
      char *prefix = "";
      if (this_set != oldset) {
	prefix = "\n          ";
	oldset = this_set;
      }
      
      printf("%s%d/%d ", prefix, this_set, failed_tests[i].test);
    }
    printf("\n");
      
  } else {
    printf("\n        All Tests Successful.\n");
  }
}


void test_run(char *test_name, 
	      void (*test_fcn)(void), 
	      int do_test, int headeronly) 
{
  static int test_set = 1;

  if (headeronly || do_test == 0 || do_test == test_set)
    test_header(test_name, test_set);
  
  if (!headeronly && (do_test==0 || do_test == test_set)) {
    (*test_fcn)();
    if (!QUIET)
    printf("\n");
  }
  test_set++;
}
