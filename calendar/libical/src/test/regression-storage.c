/* -*- Mode: C -*-
  ======================================================================
  FILE: regression-storage.c
  CREATOR: eric 03 April 1999
  
  DESCRIPTION:
  
  $Id: regression-storage.c,v 1.3 2002/08/07 17:19:55 acampi Exp $
  $Locker:  $

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

  The original author is Eric Busboom
  The original code is usecases.c

    
  ======================================================================*/

#include <assert.h>
#include <string.h> /* for strdup */
#include <stdlib.h> /* for malloc */
#include <stdio.h> /* for printf */
#include <time.h> /* for time() */

#include "ical.h"
#include "icalss.h"
#include "regression.h"

#define OUTPUT_FILE "filesetout.ics"

/* define sample calendar struct */
struct calendar {
  int ID;
  int total_size;

  /* offsets */
  int total_size_offset;
  int vcalendar_size_offset;
  int vcalendar_offset;
  int title_size_offset;
  int title_offset;

  /* data */
  int vcalendar_size;
  char *vcalendar;

  int title_size;
  char *title;

};

int vcalendar_init(struct calendar **cal, char *vcalendar, char *title);

#ifdef WITH_BDB
#include <db.h>

int get_title(DB *dbp, const DBT *pkey, const DBT *pdata, DBT *skey);
char * parse_vcalendar(const DBT *dbt) ;
char * pack_calendar(struct calendar *cal, int size);
struct calendar * unpack_calendar(char *str, int size);
#endif

static char str[] = "BEGIN:VCALENDAR\n\
PRODID:\"-//RDU Software//NONSGML HandCal//EN\"\n\
VERSION:2.0\n\
BEGIN:VTIMEZONE\n\
TZID:US-Eastern\n\
BEGIN:STANDARD\n\
DTSTART:19981025T020000\n\
RDATE:19981025T020000\n\
TZOFFSETFROM:-0400\n\
TZOFFSETTO:-0500\n\
TZNAME:EST\n\
END:STANDARD\n\
BEGIN:DAYLIGHT\n\
DTSTART:19990404T020000\n\
RDATE:19990404T020000\n\
TZOFFSETFROM:-0500\n\
TZOFFSETTO:-0400\n\
TZNAME:EDT\n\
END:DAYLIGHT\n\
END:VTIMEZONE\n\
BEGIN:VEVENT\n\
DTSTAMP:19980309T231000Z\n\
UID:guid-1.host1.com\n\
ORGANIZER;ROLE=CHAIR:MAILTO:mrbig@host.com\n\
ATTENDEE;RSVP=TRUE;ROLE=REQ-PARTICIPANT;CUTYPE=GROUP:MAILTO:employee-A@host.com\n\
DESCRIPTION:Project XYZ Review Meeting\n\
CATEGORIES:MEETING\n\
CLASS:PUBLIC\n\
CREATED:19980309T130000Z\n\
SUMMARY:XYZ Project Review\n\
DTSTART;TZID=US-Eastern:19980312T083000\n\
DTEND;TZID=US-Eastern:19980312T093000\n\
LOCATION:1CP Conference Room 4350\n\
END:VEVENT\n\
BEGIN:BOOGA\n\
DTSTAMP:19980309T231000Z\n\
X-LIC-FOO:Booga\n\
DTSTOMP:19980309T231000Z\n\
UID:guid-1.host1.com\n\
END:BOOGA\n\
END:VCALENDAR";

char str2[] = "BEGIN:VCALENDAR\n\
PRODID:\"-//RDU Software//NONSGML HandCal//EN\"\n\
VERSION:2.0\n\
BEGIN:VEVENT\n\
DTSTAMP:19980309T231000Z\n\
UID:guid-1.host1.com\n\
ORGANIZER;ROLE=CHAIR:MAILTO:mrbig@host.com\n\
ATTENDEE;RSVP=TRUE;ROLE=REQ-PARTICIPANT;CUTYPE=GROUP:MAILTO:employee-A@host.com\n\
DESCRIPTION:Project XYZ Review Meeting\n\
CATEGORIES:MEETING\n\
CLASS:PUBLIC\n\
CREATED:19980309T130000Z\n\
SUMMARY:XYZ Project Review\n\
DTSTART;TZID=US-Eastern:19980312T083000\n\
DTEND;TZID=US-Eastern:19980312T093000\n\
LOCATION:1CP Conference Room 4350\n\
END:VEVENT\n\
END:VCALENDAR\n\
";


void test_fileset_extended(void)
{
    icalset *cout;
    int month = 0;
    int count=0;
    struct icaltimetype start, end;
    icalcomponent *c,*clone, *itr;
    icalsetiter iter;

    start = icaltime_from_timet( time(0),0);
    end = start;
    end.hour++;

    cout = icalfileset_new(OUTPUT_FILE);
    ok("Opening output file", (cout != 0));
    assert(cout!=0);

    c = icalparser_parse_string(str2);
    ok("Parsing str2", (c!=0));
    assert(c != 0);

    icalset_free(cout);

    /* Add data to the file */

    for(month = 1; month < 10; month++){
	icalcomponent *event;
	icalproperty *dtstart, *dtend;

        cout = icalfileset_new(OUTPUT_FILE);
	ok("Opening output file", (cout != 0));
        assert(cout != 0);

	start.month = month; 
	end.month = month;
	
	clone = icalcomponent_new_clone(c);
	ok("Making clone of output file", (clone!=0));
	assert(clone !=0);

	event = icalcomponent_get_first_component(clone,ICAL_VEVENT_COMPONENT);
	ok("Getting first event from clone", (event!=0));
	assert(event != 0);

	dtstart = icalcomponent_get_first_property(event,ICAL_DTSTART_PROPERTY);
	ok("find DTSTART", (dtstart !=0));
	assert(dtstart!=0);

	icalproperty_set_dtstart(dtstart,start);
        
	dtend = icalcomponent_get_first_property(event,ICAL_DTEND_PROPERTY);
	ok("find DTEND", (dtend !=0));

	assert(dtend!=0);

	icalproperty_set_dtend(dtend,end);
	
	icalfileset_add_component(cout,clone);
	icalfileset_commit(cout);

        icalset_free(cout);
    }

    /* Print them out */

    cout = icalfileset_new(OUTPUT_FILE);

    ok("Opening output file", (cout != 0));
    assert(cout != 0);
    
    for (iter = icalfileset_begin_component(cout, ICAL_ANY_COMPONENT, 0);
         icalsetiter_deref(&iter) != 0; icalsetiter_next(&iter)) {
      icalcomponent *event;
      icalproperty *dtstart, *dtend;

      itr = icalsetiter_deref(&iter);
      count++;

      event = icalcomponent_get_first_component(itr,ICAL_VEVENT_COMPONENT);

      dtstart = icalcomponent_get_first_property(event,ICAL_DTSTART_PROPERTY);
      dtend = icalcomponent_get_first_property(event,ICAL_DTEND_PROPERTY);
      
      if (VERBOSE)
	printf("%d %s %s\n",count, icalproperty_as_ical_string(dtstart),
	       icalproperty_as_ical_string(dtend));

    }

    /* Remove all of them */

    icalset_free(cout);

    cout = icalfileset_new(OUTPUT_FILE);
    ok("Opening output file", (cout!=0));
    assert(cout != 0);

    /* need to advance the iterator first before calling remove_componenet() */
    /* otherwise, iter will contain a "removed" component and icalsetiter_next(&iter) */
    /* will fail. */

    iter = icalfileset_begin_component(cout, ICAL_ANY_COMPONENT, 0);
    itr = icalsetiter_deref(&iter);
    while (itr != 0) {
        icalsetiter_next(&iter);
        icalfileset_remove_component(cout, itr);
	icalcomponent_free(itr);
        itr = icalsetiter_deref(&iter);
    }
    
    icalset_free(cout);

    /* Print them out again */

    cout = icalfileset_new(OUTPUT_FILE);
    ok("Opening output file", (cout != 0));
    assert(cout != 0);
    count =0;
    
    for (itr = icalfileset_get_first_component(cout);
         itr != 0;
         itr = icalfileset_get_next_component(cout)){

      icalcomponent *event;
      icalproperty *dtstart, *dtend;

      count++;

      event = icalcomponent_get_first_component(itr,ICAL_VEVENT_COMPONENT);

      dtstart = icalcomponent_get_first_property(event,ICAL_DTSTART_PROPERTY);
      dtend = icalcomponent_get_first_property(event,ICAL_DTEND_PROPERTY);
      
      printf("%d %s %s\n",count, icalproperty_as_ical_string(dtstart),
             icalproperty_as_ical_string(dtend));

    }

    icalset_free(cout);
    icalcomponent_free(c);
}


#ifdef WITH_BDB

/*
   In this example, we're storing a calendar with several components
   under the reference id "calendar_7286" and retrieving records based
   on title, "month_1" through "month_10".  We use a number of the 
   "optional" arguments to specify secondary indices, sub-databases
   (i.e. having multiple databases residing within a single Berkeley
   DB file), and keys for storage and retrieval.
*/

void test_bdbset()
{
    icalset *cout;
    int month = 0;
    int count=0;
    int num_components=0;
    int szdata_len=0;
    int ret=0;
    char *subdb, *szdata, *szpacked_data;
    char uid[255];
    struct icaltimetype start, end;
    icalcomponent *c,*clone, *itr;
    DBT key, data;
    DBC *dbcp;

    struct calendar *cal;
    int cal_size;

    return; // for now... TODO fix these broken tests..



    start = icaltime_from_timet( time(0),0);
    end = start;
    end.hour++;

    /* Note: as per the Berkeley DB ref pages: 
     *
     * The database argument is optional, and allows applications to
     * have multiple databases in a single file. Although no database
     * argument needs to be specified, it is an error to attempt to
     * open a second database in a file that was not initially created
     * using a database name. 
     *
     */

    subdb = "calendar_id";

    /* open database, using subdb */
    cout = icalbdbset_new("calendar.db", ICALBDB_EVENTS, DB_HASH, 0);
    /*
    sdbp = icalbdbset_secondary_open(dbp, 
    				     DATABASE, 
    				     "title", 
				     get_title, 
				     DB_HASH); 
    */

    c = icalparser_parse_string(str2);

    assert(c != 0);

    /* Add data to the file */

    for(month = 1; month < 10; month++){
      icalcomponent *event;
      icalproperty *dtstart, *dtend, *location;

      /* retrieve data */
      //      cout = icalbdbset_new(dbp, sdbp, NULL);  
      //      assert(cout != 0);

      start.month = month; 
      end.month = month;
		
      clone = icalcomponent_new_clone(c);
      assert(clone !=0);
      event = icalcomponent_get_first_component(clone,
						ICAL_VEVENT_COMPONENT);
      assert(event != 0);

      dtstart = icalcomponent_get_first_property(event,
						 ICAL_DTSTART_PROPERTY);
      assert(dtstart!=0);
      icalproperty_set_dtstart(dtstart,start);
        
      dtend = icalcomponent_get_first_property(event,ICAL_DTEND_PROPERTY);
      assert(dtend!=0);
      icalproperty_set_dtend(dtend,end);

      location = icalcomponent_get_first_property(event, ICAL_LOCATION_PROPERTY);
      assert(location!=0);

#if 0
      /* change the uid to include the month */
      sprintf(uid, "%s_%d", icalcomponent_get_uid(clone), month);
      icalcomponent_set_uid(clone, uid);
#endif

      icalbdbset_add_component(cout,clone);

      /* commit changes */
      icalbdbset_commit(cout);

      num_components = icalcomponent_count_components(clone, ICAL_ANY_COMPONENT);

      icalset_free(cout); 

    }

    /* try out the cursor operations */
    memset(&key, 0, sizeof(DBT));
    memset(&data, 0, sizeof(DBT));

#if 0    
    ret = icalbdbset_acquire_cursor(dbp, &dbcp);
    ret = icalbdbset_get_first(dbcp, &key, &data);
    ret = icalbdbset_get_next(dbcp, &key, &data);
    ret = icalbdbset_get_last(dbcp, &key, &data);
#endif
    /* Print them out */

    for(month = 1, count=0; month < 10; month++){
      char *title;

      icalcomponent *event;
      icalproperty *dtstart, *dtend;

      for (itr = icalbdbset_get_first_component(cout);
	   itr != 0;
	   itr = icalbdbset_get_next_component(cout)){

	icalcomponent *event;
	icalproperty *dtstart, *dtend;

	count++;

      event = icalcomponent_get_first_component(itr,ICAL_VEVENT_COMPONENT);

      dtstart = icalcomponent_get_first_property(event,ICAL_DTSTART_PROPERTY);
      dtend = icalcomponent_get_first_property(event,ICAL_DTEND_PROPERTY);
      
      printf("%d %s %s\n",count, icalproperty_as_ical_string(dtstart),
             icalproperty_as_ical_string(dtend));

      }
      icalset_free(cout);
    }

    /* open database */
    //    cout = icalbdbset_bdb_open("calendar.db", "title", DB_HASH, 0644);
    /*    sdbp = icalbdbset_secondary_open(dbp, 
				     DATABASE, 
				     "title", 
				     get_title, 
				     DB_HASH); 
    */
    /* Remove all of them */
    for(month = 1; month < 10; month++){
      for (itr = icalbdbset_get_first_component(cout);
	   itr != 0;
	   itr = icalbdbset_get_next_component(cout)){
	
	icalbdbset_remove_component(cout, itr);
      }

      icalbdbset_commit(cout);
      icalset_free(cout);

    }

    /* Print them out again */

    for(month = 1, count=0; month < 10; month++){
      char *title;

      icalcomponent *event;
      icalproperty *dtstart, *dtend;

      for (itr = icalbdbset_get_first_component(cout);
	   itr != 0;
	   itr = icalbdbset_get_next_component(cout)){

	icalcomponent *event;
	icalproperty *dtstart, *dtend;

	count++;

      event = icalcomponent_get_first_component(itr,ICAL_VEVENT_COMPONENT);

      dtstart = icalcomponent_get_first_property(event,ICAL_DTSTART_PROPERTY);
      dtend = icalcomponent_get_first_property(event,ICAL_DTEND_PROPERTY);
      
      printf("%d %s %s\n",count, icalproperty_as_ical_string(dtstart),
             icalproperty_as_ical_string(dtend));

      }
      icalset_free(cout);
    }
}
#endif

int vcalendar_init(struct calendar **rcal, char *vcalendar, char *title) 
{
  int vcalendar_size, title_size, total_size;
  struct calendar *cal;

  if(vcalendar) 
    vcalendar_size = strlen(vcalendar);
  else {
    vcalendar = "";
    vcalendar_size = strlen(vcalendar);
  }

  if(title) 
    title_size = strlen(title);
  else {
    title = "";
    title_size = strlen(title);
  }

  total_size = sizeof(struct calendar) + vcalendar_size + title_size;

  if((cal = (struct calendar *)malloc(total_size))==NULL)
    return 0;
  memset(cal, 0, total_size);

  /* offsets */
  cal->total_size_offset     = sizeof(int);
  cal->vcalendar_size_offset = (sizeof(int) * 7);
  cal->vcalendar_offset      = cal->vcalendar_size_offset + sizeof(int);
  cal->title_size_offset     = cal->vcalendar_offset + vcalendar_size;
  cal->title_offset          = cal->title_size_offset + sizeof(int);

  /* sizes */
  cal->total_size            = total_size;
  cal->vcalendar_size        = vcalendar_size;
  cal->title_size            = title_size;

  if (vcalendar && *vcalendar) 
    cal->vcalendar = strdup(vcalendar);

  if (title && *title)
    cal->title     = strdup(title);

  *rcal = cal;

  return 0;
}

/* get_title -- extracts a secondary key (the vcalendar)
 * from a primary key/data pair */

/* just create a random title for now */
#ifdef WITH_BDB

int get_title(DB *dbp, const DBT *pkey, const DBT *pdata, DBT *skey)
{
  icalcomponent *cl;
  char title[255];

  memset(skey, 0, sizeof(DBT)); 

  cl = icalparser_parse_string((char *)pdata->data);
  sprintf(title, "title_%s", icalcomponent_get_uid(cl));

  skey->data = strdup(title);
  skey->size = strlen(skey->data);
  return (0); 
}

char * pack_calendar(struct calendar *cal, int size) 
{
  char *str;

  if((str = (char *)malloc(sizeof(char) * size))==NULL)
    return 0;

  /* ID */
  memcpy(str, &cal->ID, sizeof(cal->ID));

  /* total_size */
  memcpy(str + cal->total_size_offset,
	 &cal->total_size,
	 sizeof(cal->total_size));

  /* vcalendar_size */
  memcpy(str + cal->vcalendar_size_offset, 
         &cal->vcalendar_size, 
         sizeof(cal->vcalendar_size));

  /* vcalendar */
  memcpy(str + cal->vcalendar_offset, 
         cal->vcalendar, 
         cal->vcalendar_size);

  /* title_size */
  memcpy(str + cal->title_size_offset,
	 &cal->title_size,
	 sizeof(cal->title_size));

  /* title */
  memcpy(str + cal->title_offset,
	 cal->title,
	 cal->title_size);

  return str;
}

struct calendar * unpack_calendar(char *str, int size)
{
  struct calendar *cal;
  if((cal = (struct calendar *) malloc(size))==NULL)
    return 0;
  memset(cal, 0, size);

  /* offsets */
  cal->total_size_offset     = sizeof(int);
  cal->vcalendar_size_offset = (sizeof(int) * 7);
  cal->vcalendar_offset      = cal->vcalendar_size_offset + sizeof(int);

  /* ID */
  memcpy(&cal->ID, str, sizeof(cal->ID));

  /* total_size */
  memcpy(&cal->total_size,
	 str + cal->total_size_offset,
	 sizeof(cal->total_size));

  /* vcalendar_size */
  memcpy(&cal->vcalendar_size, 
	 str + cal->vcalendar_size_offset, 
	 sizeof(cal->vcalendar_size));

  if((cal->vcalendar = (char *)malloc(sizeof(char) * 
				      cal->vcalendar_size))==NULL)
    return 0;

  /* vcalendar */
  memcpy(cal->vcalendar, 
	 (char *)(str + cal->vcalendar_offset), 
	 cal->vcalendar_size);

  cal->title_size_offset     = cal->vcalendar_offset + cal->vcalendar_size;
  cal->title_offset          = cal->title_size_offset + sizeof(int);

  /* title_size */
  memcpy(&cal->title_size,
	 str + cal->title_size_offset,
	 sizeof(cal->title_size));

  if((cal->title = (char *)malloc(sizeof(char) *
				  cal->title_size))==NULL)
    return 0;

  /* title*/
  memcpy(cal->title,
	 (char *)(str + cal->title_offset),
	 cal->title_size);

  return cal;
}

char * parse_vcalendar(const DBT *dbt) 
{
  char *str;
  struct calendar *cal;

  str = (char *)dbt->data;
  cal = unpack_calendar(str, dbt->size);

  return cal->vcalendar;
}
#endif

void test_dirset_extended(void)
{

    icalcomponent *c;
    icalgauge *gauge;
    icalerrorenum error;
    icalcomponent *itr;
    icalset* cluster;
    struct icalperiodtype rtime;
    icalset *s = icaldirset_new("store");
    icalset *s2 = icaldirset_new("store-new");
    int i, count = 0;

    ok("Open dirset 'store'", (s!=0));
    assert(s != 0);

    rtime.start = icaltime_from_timet( time(0),0);

    cluster = icalfileset_new(OUTPUT_FILE);

    ok("Open fileset to duplicate 4 times", (cluster != 0));
    assert(cluster != 0);

#define NUMCOMP 4

    /* Duplicate every component in the cluster NUMCOMP times */

    icalerror_clear_errno();

    for (i = 1; i<NUMCOMP+1; i++){

	/*rtime.start.month = i%12;*/
	rtime.start.month = i;
	rtime.end = rtime.start;
	rtime.end.hour++;
	
	for (itr = icalfileset_get_first_component(cluster);
	     itr != 0;
	     itr = icalfileset_get_next_component(cluster)){
	    icalcomponent *clone, *inner;
	    icalproperty *p;

	    inner = icalcomponent_get_first_component(itr,ICAL_VEVENT_COMPONENT);
            if (inner == 0){
              continue;
            }

	    /* Change the dtstart and dtend times in the component
               pointed to by Itr*/

	    clone = icalcomponent_new_clone(itr);
	    inner = icalcomponent_get_first_component(itr,ICAL_VEVENT_COMPONENT);

	    ok("Duplicating component...", 
	       (icalerrno == ICAL_NO_ERROR)&&(inner!=0));

	    assert(icalerrno == ICAL_NO_ERROR);
	    assert(inner !=0);

	    /* DTSTART*/
	    p = icalcomponent_get_first_property(inner,ICAL_DTSTART_PROPERTY);
	    ok("Fetching DTSTART", (icalerrno == ICAL_NO_ERROR));
	    assert(icalerrno  == ICAL_NO_ERROR);

	    if (p == 0){
		p = icalproperty_new_dtstart(rtime.start);
		icalcomponent_add_property(inner,p);
	    } else {
		icalproperty_set_dtstart(p,rtime.start);
	    }

	    ok("Adding DTSTART property", (icalerrno == ICAL_NO_ERROR));
	    assert(icalerrno  == ICAL_NO_ERROR);

	    /* DTEND*/
	    p = icalcomponent_get_first_property(inner,ICAL_DTEND_PROPERTY);
	    ok("Fetching DTEND property", (icalerrno == ICAL_NO_ERROR));
	    assert(icalerrno  == ICAL_NO_ERROR);

	    if (p == 0){
		p = icalproperty_new_dtstart(rtime.end);
		icalcomponent_add_property(inner,p);
	    } else {
		icalproperty_set_dtstart(p,rtime.end);
	    }
	    ok("Setting DTEND property", (icalerrno == ICAL_NO_ERROR));
	    assert(icalerrno  == ICAL_NO_ERROR);
	    
	    if (VERBOSE)
	      printf("\n----------\n%s\n---------\n",icalcomponent_as_ical_string(inner));

	    error = icaldirset_add_component(s,
					     icalcomponent_new_clone(itr));
	    
	    ok("Adding component to dirset", (icalerrno == ICAL_NO_ERROR));
	    assert(icalerrno  == ICAL_NO_ERROR);
	}

    }
    
    gauge = icalgauge_new_from_sql("SELECT * FROM VEVENT WHERE VEVENT.SUMMARY = 'Submit Income Taxes' OR VEVENT.SUMMARY = 'Bastille Day Party'", 0);

    ok("Creating complex Gauge", (gauge!=0));

    icaldirset_select(s,gauge);

    for(c = icaldirset_get_first_component(s); c != 0; 
	c = icaldirset_get_next_component(s)){
	
	printf("Got one! (%d)\n", count++);
	
	if (c != 0){
	    printf("%s", icalcomponent_as_ical_string(c));;
	    if (icaldirset_add_component(s2,c) == 0){
		printf("Failed to write!\n");
	    }
	    icalcomponent_free(c);
	} else {
	    printf("Failed to get component\n");
	}
    }

    icalset_free(s2);

    for(c = icaldirset_get_first_component(s); 
	c != 0; 
	c = icaldirset_get_next_component(s)){

	if (c != 0){
	    printf("%s", icalcomponent_as_ical_string(c));;
	} else {
	    printf("Failed to get component\n");
	}

    }

    /* Remove all of the components */
    i=0;
    while((c=icaldirset_get_current_component(s)) != 0 ){
	i++;

	icaldirset_remove_component(s,c);
    }
	

    icalset_free(s);
    icalset_free(cluster);
}

