/* -*- Mode: C -*-
    ======================================================================
    FILE: icalmessage.c
    CREATOR: ebusboom 07 Nov 2000
  
    $Id: icalmessage.c,v 1.7 2005/01/24 14:02:20 acampi Exp $
    $Locker:  $
    
    (C) COPYRIGHT 2000, Eric Busboom, http://www.softwarestudio.org
    
    This program is free software; you can redistribute it and/or modify
    it under the terms of either: 
    
    The LGPL as published by the Free Software Foundation, version
    2.1, available at: http://www.fsf.org/copyleft/lesser.html
    
    Or:
    
    The Mozilla Public License Version 1.0. You may obtain a copy of
    the License at http://www.mozilla.org/MPL/


    ======================================================================*/

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "icalmessage.h"
#include "icalenums.h"
#include <ctype.h>  /* for tolower()*/
#include <string.h> /* for strstr */
#include <stdlib.h> /* for free(), malloc() */
icalcomponent* icalmessage_get_inner(icalcomponent* comp)
{
    if (icalcomponent_isa(comp) == ICAL_VCALENDAR_COMPONENT){
	return icalcomponent_get_first_real_component(comp);
    } else {
	return comp;
    }
}

static char* lowercase(const char* str)
{
    char* p = 0;
    char* n = icalmemory_strdup(str);

    if(str ==0){
	return 0;
    }

    for(p = n; *p!=0; p++){
	*p = tolower(*p);
    }

    return n;
}

icalproperty* icalmessage_find_attendee(icalcomponent* comp, const char* user)
{
    icalcomponent *inner = icalmessage_get_inner(comp);
    icalproperty *p,*attendee = 0;
    char* luser = lowercase(user);

    for(p = icalcomponent_get_first_property(inner, ICAL_ATTENDEE_PROPERTY);
	p != 0;
	p = icalcomponent_get_next_property(inner, ICAL_ATTENDEE_PROPERTY)
	){

	char* lattendee;

	lattendee = lowercase(icalproperty_get_attendee(p));

	if (strstr(lattendee,user) != 0){
	    attendee = p;
	    break;
	}

	free(lattendee);

    }
    
    free(luser);

    return attendee;

}

void icalmessage_copy_properties(icalcomponent* to, icalcomponent* from, 
		icalproperty_kind kind)
{
    icalcomponent *to_inner = icalmessage_get_inner(to);
    icalcomponent *from_inner = icalmessage_get_inner(from);

    if (to_inner == 0 && from_inner == 0){
	icalerror_set_errno(ICAL_MALFORMEDDATA_ERROR);
	return;
    }

    if(!icalcomponent_get_first_property(from_inner,kind)){
	return;
    }

    icalcomponent_add_property(to_inner,
	       icalproperty_new_clone(
		   icalcomponent_get_first_property(
		       from_inner,
		       kind)
		   )
	);
}

icalcomponent *icalmessage_new_reply_base(icalcomponent* c,
					    const char* user,
					    const char* msg)
{
    icalproperty *attendee;
    char tmp[1024];

    icalcomponent *reply = icalcomponent_vanew(
	ICAL_VCALENDAR_COMPONENT,
	icalproperty_new_method(ICAL_METHOD_REPLY),
	icalcomponent_vanew(
	    ICAL_VEVENT_COMPONENT,
	    icalproperty_new_dtstamp(icaltime_from_timet(time(0),0)),
	    0),
	0);

    icalcomponent *inner = icalmessage_get_inner(reply);

    icalerror_check_arg_rz(c,"c");

    icalmessage_copy_properties(reply,c,ICAL_UID_PROPERTY);
    icalmessage_copy_properties(reply,c,ICAL_ORGANIZER_PROPERTY);
    icalmessage_copy_properties(reply,c,ICAL_RECURRENCEID_PROPERTY);
    icalmessage_copy_properties(reply,c,ICAL_SUMMARY_PROPERTY);
    icalmessage_copy_properties(reply,c,ICAL_SEQUENCE_PROPERTY);

    icalcomponent_set_dtstamp(reply,icaltime_from_timet(time(0),0));

    if(msg != 0){
	icalcomponent_add_property(inner,icalproperty_new_comment(msg));
    }

    /* Copy this user's attendee property */

    attendee = icalmessage_find_attendee(c,user);

    if (attendee == 0){
	icalerror_set_errno(ICAL_MALFORMEDDATA_ERROR);
	icalcomponent_free(reply);
	return 0;
    }

    icalcomponent_add_property(inner,icalproperty_new_clone(attendee));

    /* Add PRODID and VERSION */

    icalcomponent_add_property(reply,icalproperty_new_version("2.0"));
    
#ifndef WIN32    
    snprintf(tmp,sizeof(tmp),
           "-//SoftwareStudio//NONSGML %s %s //EN",PACKAGE,VERSION);
#else
    snprintf(tmp,sizeof(tmp),
           "-//SoftwareStudio//NONSGML %s %s //EN",ICAL_PACKAGE,ICAL_VERSION);
#endif
    icalcomponent_add_property(reply,icalproperty_new_prodid(tmp));

    return reply;

}

icalcomponent* icalmessage_new_accept_reply(icalcomponent* c,
					    const char* user,
					    const char* msg)
{

    icalcomponent *reply;
    icalproperty *attendee;
    icalcomponent *inner;

    icalerror_check_arg_rz(c,"c");
    
    reply = icalmessage_new_reply_base(c,user,msg);

    if(reply == 0){
	return 0;
    }

    inner  = icalmessage_get_inner(reply);

    attendee = icalcomponent_get_first_property(inner,
						ICAL_ATTENDEE_PROPERTY);

    icalproperty_set_parameter(attendee,
	       icalparameter_new_partstat(ICAL_PARTSTAT_ACCEPTED));

    return reply;
}

icalcomponent* icalmessage_new_decline_reply(icalcomponent* c,
					    const char* user,
					    const char* msg)
{
    icalcomponent *reply;
    icalproperty *attendee;
    icalcomponent *inner;

    icalerror_check_arg_rz(c,"c");
    
    reply = icalmessage_new_reply_base(c,user,msg);
    inner = icalmessage_get_inner(reply);
    if(reply == 0){
	return 0;
    }

    attendee = icalcomponent_get_first_property(inner,
						ICAL_ATTENDEE_PROPERTY);

    icalproperty_set_parameter(attendee,
	       icalparameter_new_partstat(ICAL_PARTSTAT_DECLINED));

    return reply;
}

/* New is modified version of old */
icalcomponent* icalmessage_new_counterpropose_reply(icalcomponent* oldc,
						    icalcomponent* newc,
						    const char* user,
						    const char* msg)
{
    icalcomponent *reply;

    icalerror_check_arg_rz(oldc,"oldc");
    icalerror_check_arg_rz(newc,"newc");
    
    reply = icalmessage_new_reply_base(newc,user,msg);

    icalcomponent_set_method(reply,ICAL_METHOD_COUNTER);

    return reply;

}


icalcomponent* icalmessage_new_delegate_reply(icalcomponent* c,
					      const char* user,
					      const char* delegatee,
					      const char* msg)
{

    icalcomponent *reply;
    icalproperty *attendee;
    icalcomponent *inner;

    icalerror_check_arg_rz(c,"c");

    reply =  icalmessage_new_reply_base(c,user,msg);
    inner = icalmessage_get_inner(reply);
    if(reply == 0){
	return 0;
    }

    attendee = icalcomponent_get_first_property(inner,
						ICAL_ATTENDEE_PROPERTY);

    icalproperty_set_parameter(attendee,
	       icalparameter_new_partstat(ICAL_PARTSTAT_DELEGATED));

    icalproperty_set_parameter(attendee,
	       icalparameter_new_delegatedto(delegatee));

    return reply;

}

icalcomponent* icalmessage_new_delegate_request(icalcomponent* c,
					      const char* user,
					      const char* delegatee,
					      const char* msg)
{

    icalcomponent *reply;
    icalproperty *attendee;
    icalcomponent *inner;

    icalerror_check_arg_rz(c,"c");

    reply =  icalmessage_new_reply_base(c,user,msg);
    inner = icalmessage_get_inner(reply);

    if(reply == 0){
	return 0;
    }

    icalcomponent_set_method(reply,ICAL_METHOD_REQUEST);

    attendee = icalcomponent_get_first_property(inner,
						ICAL_ATTENDEE_PROPERTY);

    icalproperty_set_parameter(attendee,
	       icalparameter_new_partstat(ICAL_PARTSTAT_DELEGATED));

    icalproperty_set_parameter(attendee,
	       icalparameter_new_delegatedto(delegatee));

    icalcomponent_add_property(
	inner,
	icalproperty_vanew_attendee(
	    delegatee,
	    icalparameter_new_delegatedfrom(
		icalproperty_get_attendee(attendee)
		),
	    0
	    )
	);
	     

    return reply;

}


icalcomponent* icalmessage_new_cancel_event(icalcomponent* c,
					    const char* user,
					    const char* msg);
icalcomponent* icalmessage_new_cancel_instance(icalcomponent* c,
					    const char* user,
					    const char* msg);
icalcomponent* icalmessage_new_cancel_all(icalcomponent* c,
					    const char* user,
					    const char* msg);



icalcomponent* icalmessage_new_error_reply(icalcomponent* c,
					   const char* user,
					   const char* msg,
					   const char* debug,
					   icalrequeststatus code)
{
    icalcomponent *reply;
    icalcomponent *inner, *cinner;
    struct icalreqstattype rs;

    icalerror_check_arg_rz(c,"c");
    
    reply = icalmessage_new_reply_base(c,user,msg);
    inner = icalmessage_get_inner(reply);
    cinner = icalmessage_get_inner(c);
    if(reply == 0){
	return 0;
    }

    if( code != ICAL_UNKNOWN_STATUS){
	rs.code = code;
	rs.debug = debug;
	
	icalcomponent_add_property(inner, 
				   icalproperty_new_requeststatus(rs));
    } else { /*  code == ICAL_UNKNOWN_STATUS */ 

	/* Copy all of the request status properties */
	icalproperty *p;
	for(p = icalcomponent_get_first_property(cinner,
						 ICAL_REQUESTSTATUS_PROPERTY);
	    p != 0;
	    p = icalcomponent_get_next_property(cinner,
						ICAL_REQUESTSTATUS_PROPERTY)){

	    
	    icalcomponent_add_property(inner,icalproperty_new_clone(p));
	}
    }

    return reply;
}
