/* -*- Mode: C -*-
  ======================================================================
  FILE: regression-classify.c
  CREATOR: eric 11 February 2000
  
  $Id: regression-classify.c,v 1.3 2002/08/07 17:18:13 acampi Exp $
  $Locker:  $
    
 (C) COPYRIGHT 2000 Eric Busboom
 http://www.softwarestudio.org

 The contents of this file are subject to the Mozilla Public License
 Version 1.0 (the "License"); you may not use this file except in
 compliance with the License. You may obtain a copy of the License at
 http://www.mozilla.org/MPL/
 
 Software distributed under the License is distributed on an "AS IS"
 basis, WITHOUT WARRANTY OF ANY KIND, either express or implied. See
 the License for the specific language governing rights and
 limitations under the License.
 
 The Original Code is eric. The Initial Developer of the Original
 Code is Eric Busboom


 ======================================================================*/
#include <stdio.h> /* for printf */
#include <errno.h>
#include <string.h> /* For strerror */

#include "ical.h"
#include "icalss.h"
#include "regression.h"

extern int VERBOSE;

/* Get a note about the purpose of the property*/
static const char* get_note(icalcomponent *c)
{
    icalproperty *p;
    const char* note = 0;

    if(c != 0){
        for(p = icalcomponent_get_first_property(c,ICAL_X_PROPERTY);
            p!= 0;
            p = icalcomponent_get_next_property(c,ICAL_X_PROPERTY)){
            if(strcmp(icalproperty_get_x_name(p),"X-LIC-NOTE")==0){
            note = icalproperty_get_x(p);
            }
        }
    } 
    
    if(note == 0){
        note = "None";
    }
    
    return note;
}

/* Get the expected result about the purpose of the property*/

static const char* get_expect(icalcomponent *c)
{
    icalproperty *p;
    const char* note = 0;

    if(c != 0){
        for(p = icalcomponent_get_first_property(c,ICAL_X_PROPERTY);
            p!= 0;
            p = icalcomponent_get_next_property(c,ICAL_X_PROPERTY)){
            if(strcmp(icalproperty_get_x_name(p),"X-LIC-EXPECT")==0){
            note = icalproperty_get_x(p);
            }
        }
    } 
    
    if(note == 0){
        note = "None";
    }
    
    return note;
}

void test_classify(void)
{
    icalcomponent *c,*match; 
    int i=0;
    int error_count = 0;
    /* Open up the two storage files, one for the incomming components, 
       one for the calendar */
    icalset* incoming = icalset_new_file("../../test-data/incoming.ics");
    icalset* cal = icalset_new_file("../../test-data/calendar.ics");
    icalset* f = icalset_new_file("../../test-data/classify.ics");

    ok("opening file classify.ics", (f!=0));
    ok("opening file calendar.ics", (cal!=0));
    ok("opening file incoming.ics", (incoming!=0));

    /* some basic tests.. */
    if (f) {
      c = icalset_get_first_component(f);
      match = icalset_get_next_component(f);
    
      ok("test two vcalendars for SEQUENCE with icalclassify()",
	 (icalclassify(c,match,"A@example.com") == ICAL_XLICCLASS_REQUESTRESCHEDULE));
      
      icalset_free(f);
    }

    assert(incoming!= 0);
    assert(cal!=0);

    /* Iterate through all of the incoming components */
    for(c=icalset_get_first_component(incoming);c!=0;
	c=icalset_get_next_component(incoming)){
	
	icalproperty_xlicclass class;
	icalcomponent *match = 0;
        const char* this_uid;
	const char* this_note = get_note(c);
	const char* expected_result = get_expect(c);
	const char* actual_result;
	const char* match_note;
	char msg[128];

	i++;

        /* Check this component against the restrictions imposed by
           iTIP. An errors will be inserted as X-LIC-ERROR properties
           in the component. The Parser will also insert errors if it
           cannot parse the component */
        icalcomponent_check_restrictions(c);

        /* If there are any errors, print out the component */
	
	error_count = icalcomponent_count_errors(c);
	sprintf(msg, "%s - parsing", this_note);
	int_is(msg, error_count, 0);

	if (error_count !=0) {
	  if (VERBOSE) printf("----- Component has errors ------- \n%s-----------------\n",
			      icalcomponent_as_ical_string(c));
        }

        /* Use one of the icalcomponent convenience routines to get
           the UID. This routine will save you from having to use
           icalcomponent_get_inner(),
           icalcomponent_get_first_property(), checking the return
           value, and then calling icalproperty_get_uid. There are
           several other convenience routines for DTSTART, DTEND,
           DURATION, SUMMARY, METHOD, and COMMENT */
	this_uid = icalcomponent_get_uid(c);

        if(this_uid != 0){
            /* Look in the calendar for a component with the same UID
               as the incomming component. We should reall also be
               checking the RECURRENCE-ID. Another way to do this
               operation is to us icalset_find_match(), which does use
               the RECURRENCE-ID. */
            match = icalset_fetch(cal,this_uid);
        }

	
        /* Classify the incoming component. The third argument is the
           calid of the user who owns the calendar. In a real program,
           you would probably switch() on the class.*/
	class = icalclassify(c,match,"A@example.com");
	/** eventually test this too.. **/
        match_note = get_note(match);
	actual_result = icalproperty_enum_to_string(class);
	sprintf(msg, "expecting %s", expected_result);
	is(msg, expected_result, actual_result);

	if (VERBOSE) printf("Test %d\n\
Incoming:      %s\n\
Matched:       %s\n\
Classification: %s\n\n",
               i,this_note,get_note(match),
               icalproperty_enum_to_string(class));	
    }

	icalset_free(incoming);
	icalset_free(cal);

}


