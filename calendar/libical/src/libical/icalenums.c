/* -*- Mode: C -*- */
/*======================================================================
  FILE: icalenum.c
  CREATOR: eric 29 April 1999
  
  $Id: icalenums.c,v 1.13 2002/06/11 12:22:25 acampi Exp $


 (C) COPYRIGHT 2000, Eric Busboom, http://www.softwarestudio.org

 This program is free software; you can redistribute it and/or modify
 it under the terms of either: 

    The LGPL as published by the Free Software Foundation, version
    2.1, available at: http://www.fsf.org/copyleft/lesser.html

  Or:

    The Mozilla Public License Version 1.0. You may obtain a copy of
    the License at http://www.mozilla.org/MPL/

  The original code is icalenum.c

  ======================================================================*/


#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "icalenums.h"

#include <stdio.h> /* For fprintf */
#include <stdio.h> /* For stderr */
#include <string.h> /* For strncmp */
#include <assert.h>
#include "icalmemory.h"

/*** @brief Allowed request status values
 */
static struct {
	 enum icalrequeststatus kind;
	int major;
	int minor;
	const char* str;
} request_status_map[] = {
    {ICAL_2_0_SUCCESS_STATUS, 2,0,"Success."},
    {ICAL_2_1_FALLBACK_STATUS, 2,1,"Success but fallback taken  on one or more property  values."},
    {ICAL_2_2_IGPROP_STATUS, 2,2,"Success, invalid property ignored."},
    {ICAL_2_3_IGPARAM_STATUS, 2,3,"Success, invalid property parameter ignored."},
    {ICAL_2_4_IGXPROP_STATUS, 2,4,"Success, unknown non-standard property ignored."},
    {ICAL_2_5_IGXPARAM_STATUS, 2,5,"Success, unknown non standard property value  ignored."},
    {ICAL_2_6_IGCOMP_STATUS, 2,6,"Success, invalid calendar component ignored."},
    {ICAL_2_7_FORWARD_STATUS, 2,7,"Success, request forwarded to Calendar User."},
    {ICAL_2_8_ONEEVENT_STATUS, 2,8,"Success, repeating event ignored. Scheduled as a  single component."},
    {ICAL_2_9_TRUNC_STATUS, 2,9,"Success, truncated end date time to date boundary."},
    {ICAL_2_10_ONETODO_STATUS, 2,10,"Success, repeating VTODO ignored. Scheduled as a  single VTODO."},
    {ICAL_2_11_TRUNCRRULE_STATUS, 2,11,"Success, unbounded RRULE clipped at some finite  number of instances  "},
    {ICAL_3_0_INVPROPNAME_STATUS, 3,0,"Invalid property name."},
    {ICAL_3_1_INVPROPVAL_STATUS, 3,1,"Invalid property value."},
    {ICAL_3_2_INVPARAM_STATUS, 3,2,"Invalid property parameter."},
    {ICAL_3_3_INVPARAMVAL_STATUS, 3,3,"Invalid property parameter  value."},
    {ICAL_3_4_INVCOMP_STATUS, 3,4,"Invalid calendar component."},
    {ICAL_3_5_INVTIME_STATUS, 3,5,"Invalid date or time."},
    {ICAL_3_6_INVRULE_STATUS, 3,6,"Invalid rule."},
    {ICAL_3_7_INVCU_STATUS, 3,7,"Invalid Calendar User."},
    {ICAL_3_8_NOAUTH_STATUS, 3,8,"No authority."},
    {ICAL_3_9_BADVERSION_STATUS, 3,9,"Unsupported version."},
    {ICAL_3_10_TOOBIG_STATUS, 3,10,"Request entity too large."},
    {ICAL_3_11_MISSREQCOMP_STATUS, 3,11,"Required component or property missing."},
    {ICAL_3_12_UNKCOMP_STATUS, 3,12,"Unknown component or property found."},
    {ICAL_3_13_BADCOMP_STATUS, 3,13,"Unsupported component or property found"},
    {ICAL_3_14_NOCAP_STATUS, 3,14,"Unsupported capability."},
    {ICAL_3_15_INVCOMMAND, 3,15,"Invalid command."},
    {ICAL_4_0_BUSY_STATUS, 4,0,"Event conflict. Date/time  is busy."},
    {ICAL_4_1_STORE_ACCESS_DENIED, 4,1,"Store Access Denied."},
    {ICAL_4_2_STORE_FAILED, 4,2,"Store Failed."},
    {ICAL_4_3_STORE_NOT_FOUND, 4,3,"Store not found."},
    {ICAL_5_0_MAYBE_STATUS, 5,0,"Request MAY supported."},
    {ICAL_5_1_UNAVAIL_STATUS, 5,1,"Service unavailable."},
    {ICAL_5_2_NOSERVICE_STATUS, 5,2,"Invalid calendar service."},
    {ICAL_5_3_NOSCHED_STATUS, 5,3,"No scheduling support for  user."},
    {ICAL_6_1_CONTAINER_NOT_FOUND, 6,1,"Container not found."},
    {ICAL_9_0_UNRECOGNIZED_COMMAND, 9,0,"An unrecognized command was received."},
    {ICAL_UNKNOWN_STATUS, 0,0,"Error: Unknown request status"}
};


/*** @brief Return the descriptive text for a request status
 */
const char* icalenum_reqstat_desc(icalrequeststatus stat)
{
    int i;

    for (i=0; request_status_map[i].kind  != ICAL_UNKNOWN_STATUS; i++) {
	if ( request_status_map[i].kind ==  stat) {
	    return request_status_map[i].str;
	}
    }

    return 0;
}

/*** @brief Return the code for a request status
 */
char* icalenum_reqstat_code(icalrequeststatus stat)
{
    int i, major, minor;
    char tmpbuf[36];

    for (i=0; request_status_map[i].kind  != ICAL_UNKNOWN_STATUS; i++) {
	if ( request_status_map[i].kind ==  stat) {
	    major = request_status_map[i].major;
	    minor = request_status_map[i].minor;
	    sprintf(tmpbuf, "%i.%i", major, minor);
	    return icalmemory_tmp_copy(tmpbuf);
	}
    }
    return NULL;
}

/*** @brief Return the major number for a request status
 */
short icalenum_reqstat_major(icalrequeststatus stat)
{
    int i;

    for (i=0; request_status_map[i].kind  != ICAL_UNKNOWN_STATUS; i++) {
	if ( request_status_map[i].kind ==  stat) {
	    return request_status_map[i].major;
	}
    }
    return -1;
}

/*** @brief Return the minor number for a request status
 */
short icalenum_reqstat_minor(icalrequeststatus stat)
{
    int i;

    for (i=0; request_status_map[i].kind  != ICAL_UNKNOWN_STATUS; i++) {
	if ( request_status_map[i].kind ==  stat) {
	    return request_status_map[i].minor;
	}
    }
    return -1;
}


/*** @brief Return a request status for major/minor status numbers
 */
icalrequeststatus icalenum_num_to_reqstat(short major, short minor)
{
    int i;

    for (i=0; request_status_map[i].kind  != ICAL_UNKNOWN_STATUS; i++) {
	if ( request_status_map[i].major ==  major && request_status_map[i].minor ==  minor) {
	    return request_status_map[i].kind;
	}
    }
    return 0;
}



