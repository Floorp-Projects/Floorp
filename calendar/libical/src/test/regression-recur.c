/* -*- Mode: C -*-
  ======================================================================
  FILE: regression-recur.c
  CREATOR: ebusboom 8jun00
  
  DESCRIPTION:

  (C) COPYRIGHT 1999 Eric Busboom 
  http://www.softwarestudio.org

  The contents of this file are subject to the Mozilla Public License
  Version 1.0 (the "License"); you may not use this file except in
  compliance with the License. You may obtain a copy of the License at
  http://www.mozilla.org/MPL/
 
  Software distributed under the License is distributed on an "AS IS"
  basis, WITHOUT WARRANTY OF ANY KIND, either express or implied. See
  the License for the specific language governing rights and
  limitations under the License.

  ======================================================================*/

#include <assert.h>
#include <string.h> /* for strdup */
#include <stdlib.h> /* for malloc */
#include <stdio.h>  /* for printf */
#include <time.h>   /* for time() */
#include <signal.h> /* for signal */
#ifndef WIN32
#include <unistd.h> /* for alarm */
#endif

#include "ical.h"
#include "icalss.h"
#include "regression.h"

extern int VERBOSE;

#ifdef WIN32
#define snprintf	_snprintf
#define strcasecmp	stricmp
#endif


static void sig_alrm(int i){
    fprintf(stderr,"Could not get lock on file\n");
    exit(1);
}

/* Get the expected result about the purpose of the property*/

static int get_expected_numevents(icalcomponent *c)
{
    icalproperty *p;
    const char* note = 0;
    int num_events = 0;

    if(c != 0){
        for(p = icalcomponent_get_first_property(c,ICAL_X_PROPERTY);
            p!= 0;
            p = icalcomponent_get_next_property(c,ICAL_X_PROPERTY)){
            if(strcmp(icalproperty_get_x_name(p),"X-EXPECT-NUMEVENTS")==0){
	      note = icalproperty_get_x(p);
            }
        }
    } 
    
    if(note != 0){
      num_events = atoi(note);
    }
    
    
    return num_events;
}



static void recur_callback(icalcomponent *comp,
			   struct icaltime_span *span,
			   void *data)
{
  int *num_recurs = data;

  if (VERBOSE) {
    printf("recur: %s", ctime(&span->start));
    printf("       %s", ctime(&span->end));
  }
  *num_recurs = *num_recurs + 1;
}

void test_recur_file()
{
    icalset *cin = 0;
    struct icaltimetype next;
    icalcomponent *itr;
    icalproperty *desc, *dtstart, *rrule;
    struct icalrecurrencetype recur;
    icalrecur_iterator* ritr;
    time_t tt;
    char* file; 
    int num_recurs_found = 0;
	
    icalerror_set_error_state(ICAL_PARSE_ERROR, ICAL_ERROR_NONFATAL);
	
#ifndef WIN32
    signal(SIGALRM,sig_alrm);
#endif
    file = getenv("ICAL_RECUR_FILE");
    if (!file)
      file = "../../test-data/recur.txt";
	
#ifndef WIN32
    alarm(15); /* to get file lock */
#endif
    cin = icalfileset_new(file);
#ifndef WIN32
    alarm(0);
#endif
	
    ok("opening file with recurring events", (cin!=NULL));
    assert(cin!=NULL);
	
    for (itr = icalfileset_get_first_component(cin);
	itr != 0;
	itr = icalfileset_get_next_component(cin)){
      int badcomp = 0;
      int expected_events = 0;
      char msg[128];


      struct icaltimetype start = icaltime_null_time();
      struct icaltimetype startmin = icaltime_from_timet(1,0);
      struct icaltimetype endmax = icaltime_null_time();
      const char *desc_str = "malformed component";

      desc = icalcomponent_get_first_property(itr,ICAL_DESCRIPTION_PROPERTY);
      dtstart = icalcomponent_get_first_property(itr,ICAL_DTSTART_PROPERTY);
      rrule = icalcomponent_get_first_property(itr,ICAL_RRULE_PROPERTY);
      if (desc) {
	desc_str = icalproperty_get_description(desc);
      }
      
      ok((char*)desc_str, !(desc == 0 || dtstart == 0 || rrule == 0));

      if (desc == 0 || dtstart == 0 || rrule == 0) {
	badcomp = 1;
	if (VERBOSE) {
	  printf("\n******** Error in input component ********\n");
	  printf("The following component is malformed:\n %s\n", desc_str);
	}
	continue;
      }
      if (VERBOSE) {
	printf("\n\n#### %s\n",desc_str);
	printf("#### %s\n",icalvalue_as_ical_string(icalproperty_get_value(rrule)));
      }
      
      recur = icalproperty_get_rrule(rrule);
      start = icalproperty_get_dtstart(dtstart);
      
      ritr = icalrecur_iterator_new(recur,start);
      
      tt = icaltime_as_timet(start);
      
      if (VERBOSE)
	printf("#### %s\n",ctime(&tt ));
      
      icalrecur_iterator_free(ritr);
      
      for(ritr = icalrecur_iterator_new(recur,start),
	    next = icalrecur_iterator_next(ritr); 
	  !icaltime_is_null_time(next);
	  next = icalrecur_iterator_next(ritr)){
	
	tt = icaltime_as_timet(next);
	
	if (VERBOSE)
	  printf("  %s",ctime(&tt ));		
	
      }

      icalrecur_iterator_free(ritr);
      num_recurs_found = 0;
      expected_events = get_expected_numevents(itr);

      icalcomponent_foreach_recurrence(itr, startmin, endmax, 
				       recur_callback, &num_recurs_found);
      
      sprintf(msg,"   expecting total of %d events", expected_events);
      int_is(msg, num_recurs_found, expected_events);
    }
    
    icalset_free(cin);
}
