/* -*- Mode: C -*-
    ======================================================================
    FILE: icalclassify.c
    CREATOR: ebusboom 23 aug 2000
  
    $Id: icalclassify.c,v 1.14 2003/11/17 22:51:53 gray-john Exp $
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

#include "ical.h"
#include "icalclassify.h"
#include "icalmemory.h"

#include <ctype.h>   /* For tolower() */
#include <string.h> /* for index() */
#include <stdlib.h> /* for malloc and free */



struct icalclassify_parts {
    icalcomponent *c;
    icalcomponent_kind inner_kind;
    icalproperty_method method;
    char* organizer;
    icalparameter_partstat reply_partstat;
    char* reply_attendee;
    char* uid;
    int sequence;
    struct icaltimetype dtstamp;	
    struct icaltimetype recurrence_id;
}; 
	

char* icalclassify_lowercase(const char* str)
{
    char* p = 0;
    char* ret;

    if(str ==0){
	return 0;
    }

    ret = icalmemory_strdup(str);

    for(p = ret; *p!=0; p++){
	*p = tolower(*p);
    }

    return ret;
}

/* Return a set of components that intersect in time with comp. For
component X and Y to intersect:
    X.DTSTART < Y.DTEND && X.DTEND > Y.DTSTART
*/


icalcomponent* icalclassify_find_overlaps(icalset* set, icalcomponent* comp)
{
    icalcomponent *return_set;
    icalcomponent *c;
    struct icaltime_span span,compspan;
    
    icalerror_clear_errno();
    compspan = icalcomponent_get_span(comp);

    if(icalerrno != ICAL_NO_ERROR){
	return 0;
    }


    return_set = icalcomponent_new(ICAL_XROOT_COMPONENT);

    for(c = icalset_get_first_component(set);
	c != 0;
	c = icalset_get_next_component(set)){

	icalerror_clear_errno();

	span = icalcomponent_get_span(c);

	if(icalerrno != ICAL_NO_ERROR){
	    continue;
	}

	if (compspan.start < span.end && 
	    compspan.end > span.start){

	    icalcomponent *clone = icalcomponent_new_clone(c);

	    icalcomponent_add_component(return_set,clone);
	}	
    }

    if(icalcomponent_count_components(return_set,ICAL_ANY_COMPONENT) !=0){
	return return_set;
    } else {
	icalcomponent_free(return_set);
	return 0;
    }
}



icalproperty* icalclassify_find_attendee(icalcomponent *c, 
						  const char* attendee)
{
    icalproperty *p;
    icalcomponent* inner;
    char* lattendee;
    char* upn;

    if(attendee == 0){
        return 0;
    }

    lattendee = icalclassify_lowercase(attendee);
    upn =  strchr(lattendee,':');

    if (upn== 0){
        upn = lattendee;
    } else {
        upn++; /* skip the ";"*/
    }

    inner = icalcomponent_get_first_real_component(c);

    for(p  = icalcomponent_get_first_property(inner,ICAL_ATTENDEE_PROPERTY);
	p != 0;
	p  = icalcomponent_get_next_property(inner,ICAL_ATTENDEE_PROPERTY))
    {
	char* this_attendee
	    = icalclassify_lowercase(icalproperty_get_attendee(p));
	char* this_upn = strchr(this_attendee,':');

        if(this_upn == 0){
            continue;
        } else {
            this_upn++;
        }

	if(strcmp(this_upn,upn)==0){
			free(lattendee);
			free(this_attendee);
            return p;
	}

		free(this_attendee);
    }
	free(lattendee);

    return 0;

}

void icalssutil_free_parts(struct icalclassify_parts *parts)
{
    if(parts == 0){
	return;
    }

    if(parts->organizer != 0){
	free(parts->organizer);
    }

    if(parts->uid != 0){
	free(parts->uid);
    }

    if(parts->reply_attendee){
	free(parts->reply_attendee);
    }
}

void icalssutil_get_parts(icalcomponent* c,  
			  struct icalclassify_parts* parts)
{
    icalproperty *p;
    icalcomponent *inner;

    memset(parts,0,sizeof(struct icalclassify_parts));

    parts->method = ICAL_METHOD_NONE;
    parts->sequence = 0;
    parts->reply_partstat = ICAL_PARTSTAT_NONE;

    if(c == 0){
	return;
    }

    parts->c = c;

    p = icalcomponent_get_first_property(c,ICAL_METHOD_PROPERTY);
    if(p!=0){
	parts->method = icalproperty_get_method(p);
    }

    inner = icalcomponent_get_first_real_component(c);

    parts->inner_kind = icalcomponent_isa(inner);

    p = icalcomponent_get_first_property(inner,ICAL_ORGANIZER_PROPERTY);
    if(p!=0){
	parts->organizer = strdup(icalproperty_get_organizer(p));
    }

    p = icalcomponent_get_first_property(inner,ICAL_SEQUENCE_PROPERTY);    
    if(p!=0){
	parts->sequence = icalproperty_get_sequence(p);
    }

    p = icalcomponent_get_first_property(inner,ICAL_UID_PROPERTY);
    if(p!=0){
	parts->uid = strdup(icalproperty_get_uid(p));
    }

    p = icalcomponent_get_first_property(inner,ICAL_RECURRENCEID_PROPERTY);
    if(p!=0){
	parts->recurrence_id = icalproperty_get_recurrenceid(p);
    }

    p = icalcomponent_get_first_property(inner,ICAL_DTSTAMP_PROPERTY);
    if(p!=0){
	parts->dtstamp = icalproperty_get_dtstamp(p);
    }

    if(parts->method==ICAL_METHOD_REPLY){
	icalparameter *param;
	p  = icalcomponent_get_first_property(inner,ICAL_ATTENDEE_PROPERTY);

	if(p!=0){

	    param = icalproperty_get_first_parameter(p,ICAL_PARTSTAT_PARAMETER);
	    
	    if(param != 0){
		parts->reply_partstat = 
		    icalparameter_get_partstat(param);
	    }
	    
	    parts->reply_attendee = strdup(icalproperty_get_attendee(p));
	}

    }    


}


int icalssutil_is_rescheduled(icalcomponent* a,icalcomponent* b)
{
    icalproperty *p1,*p2;
    icalcomponent *i1,*i2;
    int i;

    icalproperty_kind kind_array[] = {
	ICAL_DTSTART_PROPERTY,
	ICAL_DTEND_PROPERTY,
	ICAL_DURATION_PROPERTY,
	ICAL_DUE_PROPERTY,
	ICAL_RRULE_PROPERTY,
	ICAL_RDATE_PROPERTY,
	ICAL_EXRULE_PROPERTY,
	ICAL_EXDATE_PROPERTY,
	ICAL_NO_PROPERTY
    };

    i1 = icalcomponent_get_first_real_component(a);
    i2 = icalcomponent_get_first_real_component(b);

    for(i =0; kind_array[i] != ICAL_NO_PROPERTY; i++){
	p1 = icalcomponent_get_first_property(i1,kind_array[i]);
	p2 = icalcomponent_get_first_property(i2,kind_array[i]);
	
	if( (p1!=0)^(p2!=0) ){
	    /* Return true if the property exists in one component and not
	       the other */
	    return 1;
	}
	
	if(p1 && strcmp(icalproperty_as_ical_string(p1),
			icalproperty_as_ical_string(p2)) != 0){
	    return 1;
	}
    }

    return 0;
    
}

#define icalclassify_pre \
    int rtrn =0; 

#define icalclassify_post \
    return rtrn;


int icalclassify_publish_new(struct icalclassify_parts *comp, 
				struct icalclassify_parts *match, 
				const char* user)
{
    icalclassify_pre;

    if(comp->method == ICAL_METHOD_PUBLISH &&
	match == 0 && comp->inner_kind != ICAL_VFREEBUSY_COMPONENT){
	rtrn = 1;
    }
	
    icalclassify_post;

}

int icalclassify_publish_update(struct icalclassify_parts *comp, 
				struct icalclassify_parts *match, 
				const char* user)
{
    icalclassify_pre;

    if(comp->method == ICAL_METHOD_PUBLISH &&
	match !=0 && comp->inner_kind != ICAL_VFREEBUSY_COMPONENT){
	rtrn = 1;
    }
	
    icalclassify_post;

}

int icalclassify_publish_freebusy(struct icalclassify_parts *comp, 
				struct icalclassify_parts *match, 
				const char* user)
{
    icalclassify_pre;

    if(comp->method == ICAL_METHOD_PUBLISH &&
       comp->inner_kind == ICAL_VFREEBUSY_COMPONENT){
	rtrn = 1;
    }
	
    icalclassify_post;

}


int icalclassify_request_new(struct icalclassify_parts *comp, 
				    struct icalclassify_parts *match, 
				    const char* user)
{
    /* Method is  REQUEST, and there is no match */

    icalclassify_pre

    if(match->c==0 && comp->method == ICAL_METHOD_REQUEST){
	rtrn = 1;
    }

    icalclassify_post

}

int icalclassify_request_update(
    struct icalclassify_parts *comp,
    struct icalclassify_parts *match, 
    const char* user)
{
    /* REQUEST method, Higher SEQUENCE than match, and all
       time-related properties are unchanged */
    
    icalclassify_pre

    if (match != 0 &&
	comp->sequence >= match->sequence &&
	!icalssutil_is_rescheduled(comp->c,match->c)){
	rtrn = 1;
    }

    icalclassify_post

}

int icalclassify_request_reschedule(
    struct icalclassify_parts *comp,
    struct icalclassify_parts *match, 
    const char* user)
{
    /* REQUEST method, Higher SEQUENCE than match, and one or more
       time-related properties are changed */
    icalclassify_pre

    if (match->c != 0 &&
	comp->sequence > match->sequence &&
	icalssutil_is_rescheduled(comp->c,match->c)){
	rtrn = 1;
    }

    icalclassify_post

}

int icalclassify_request_delegate(
    struct icalclassify_parts *comp,
    struct icalclassify_parts *match, 
    const char* user)
{
    icalproperty* attendee;
    icalparameter* param;
    icalclassify_pre;

    attendee = icalclassify_find_attendee(comp->c,user);

    if(attendee == 0){
        return 0;
    }

    param = icalproperty_get_first_parameter(attendee,ICAL_DELEGATEDFROM_PARAMETER);
	    
    if (param != 0){
	rtrn = 1;
    }

    icalclassify_post

}

int icalclassify_request_new_organizer(
    struct icalclassify_parts *comp,
    struct icalclassify_parts *match, 
    const char* user)
{
    /*   Organizer has changed between match and component */
    icalclassify_pre
    icalerror_set_errno(ICAL_UNIMPLEMENTED_ERROR);
    icalclassify_post

}

int icalclassify_request_status(
    struct icalclassify_parts *comp,
    struct icalclassify_parts *match, 
    const char* user)
{
    icalclassify_pre
    icalerror_set_errno(ICAL_UNIMPLEMENTED_ERROR);
    icalclassify_post
}

int icalclassify_request_forward(
    struct icalclassify_parts *comp,
    struct icalclassify_parts *match, 
    const char* user)
{
    icalclassify_pre
    icalerror_set_errno(ICAL_UNIMPLEMENTED_ERROR);
    icalclassify_post
}

int icalclassify_request_freebusy(
    struct icalclassify_parts *comp,
    struct icalclassify_parts *match, 
    const char* user)
{
    icalclassify_pre
    icalerror_set_errno(ICAL_UNIMPLEMENTED_ERROR);
    icalclassify_post
}

int icalclassify_reply_accept(
    struct icalclassify_parts *comp,
    struct icalclassify_parts *match, 
    const char* user)
{
    icalproperty* attendee;
    icalclassify_pre;

    attendee = icalclassify_find_attendee(match->c,comp->reply_attendee);

    if(attendee != 0&&
       comp->reply_partstat == ICAL_PARTSTAT_ACCEPTED){
	rtrn = 1;
    }

    icalclassify_post
}
int icalclassify_reply_decline(
    struct icalclassify_parts *comp,
    struct icalclassify_parts *match, 
    const char* user)
{
    icalproperty* attendee;
    icalclassify_pre;

    attendee = icalclassify_find_attendee(match->c,comp->reply_attendee);


    if( attendee != 0 &&
       comp->reply_partstat == ICAL_PARTSTAT_DECLINED){
	rtrn = 1;
    }
    icalclassify_post
}
int icalclassify_reply_delegate(
    struct icalclassify_parts *comp,
    struct icalclassify_parts *match, 
    const char* user)
{
    icalproperty* attendee;
    icalclassify_pre;

    attendee = icalclassify_find_attendee(match->c,comp->reply_attendee);

    if( attendee != 0 &&
       comp->reply_partstat == ICAL_PARTSTAT_DELEGATED){
	rtrn = 1;
    }
    icalclassify_post
}
int icalclassify_reply_crasher_accept(
    struct icalclassify_parts *comp,
    struct icalclassify_parts *match, 
    const char* user)
{
    icalproperty* attendee;
    icalclassify_pre;

    attendee= icalclassify_find_attendee(match->c,comp->reply_attendee);

    if(attendee == 0 &&
       comp->reply_partstat == ICAL_PARTSTAT_ACCEPTED){
	rtrn = 1;
    }
    icalclassify_post
}
int icalclassify_reply_crasher_decline(
    struct icalclassify_parts *comp,
    struct icalclassify_parts *match, 
    const char* user)
{
    icalproperty* attendee;
    icalclassify_pre;


    attendee = icalclassify_find_attendee(match->c,comp->reply_attendee);

    if(attendee == 0 &&
       comp->reply_partstat == ICAL_PARTSTAT_DECLINED){
	rtrn = 1;
    }
    icalclassify_post
}
int icalclassify_add_instance(
    struct icalclassify_parts *comp,
    struct icalclassify_parts *match, 
    const char* user)
{
    icalclassify_pre
    if(comp->method == ICAL_METHOD_ADD){
	rtrn = 1;
    }
    icalclassify_post
}
int icalclassify_cancel_event(
    struct icalclassify_parts *comp,
    struct icalclassify_parts *match, 
    const char* user)
{
    icalclassify_pre
    if(comp->method == ICAL_METHOD_CANCEL){
	rtrn = 1;
    }
    icalclassify_post
}
int icalclassify_cancel_instance(
    struct icalclassify_parts *comp,
    struct icalclassify_parts *match, 
    const char* user)
{
    icalclassify_pre
    if(comp->method == ICAL_METHOD_CANCEL){
	rtrn = 1;
    }
    icalclassify_post
}
int icalclassify_cancel_all(
    struct icalclassify_parts *comp,
    struct icalclassify_parts *match, 
    const char* user)
{
    icalclassify_pre
    if(comp->method == ICAL_METHOD_CANCEL){
	rtrn = 1;
    }
    icalclassify_post
}
int icalclassify_refesh(
    struct icalclassify_parts *comp,
    struct icalclassify_parts *match, 
    const char* user)
{
    icalclassify_pre
    if(comp->method == ICAL_METHOD_REFRESH){
	rtrn = 1;
    }
    icalclassify_post
}
int icalclassify_counter(
    struct icalclassify_parts *comp,
    struct icalclassify_parts *match, 
    const char* user)
{
    icalclassify_pre
    if(comp->method == ICAL_METHOD_COUNTER){
	rtrn = 1;
    }
    icalclassify_post
}
int icalclassify_delinecounter(
    struct icalclassify_parts *comp,
    struct icalclassify_parts *match, 
    const char* user)
{
    icalclassify_pre

    if(comp->method == ICAL_METHOD_DECLINECOUNTER){
	rtrn = 1;
    }
	
    icalclassify_post
}

struct icalclassify_map {
	icalproperty_method method;
	int (*fn)(struct icalclassify_parts *comp,struct icalclassify_parts *match, const char* user);
	icalproperty_xlicclass class;
} icalclassify_map[] = 
{ {ICAL_METHOD_PUBLISH,icalclassify_publish_new,ICAL_XLICCLASS_PUBLISHNEW},
 {ICAL_METHOD_PUBLISH,icalclassify_publish_update,ICAL_XLICCLASS_PUBLISHUPDATE},
 {ICAL_METHOD_PUBLISH,icalclassify_publish_freebusy,ICAL_XLICCLASS_PUBLISHFREEBUSY},
  {ICAL_METHOD_REQUEST,icalclassify_request_delegate,ICAL_XLICCLASS_REQUESTDELEGATE},
  {ICAL_METHOD_REQUEST,icalclassify_request_new,ICAL_XLICCLASS_REQUESTNEW},
  {ICAL_METHOD_REQUEST,icalclassify_request_update,ICAL_XLICCLASS_REQUESTUPDATE},
  {ICAL_METHOD_REQUEST,icalclassify_request_reschedule,ICAL_XLICCLASS_REQUESTRESCHEDULE},

  {ICAL_METHOD_REQUEST,icalclassify_request_new_organizer,ICAL_XLICCLASS_REQUESTNEWORGANIZER},
  {ICAL_METHOD_REQUEST,icalclassify_request_forward,ICAL_XLICCLASS_REQUESTFORWARD},
  {ICAL_METHOD_REQUEST,icalclassify_request_status,ICAL_XLICCLASS_REQUESTSTATUS},
  {ICAL_METHOD_REQUEST,icalclassify_request_freebusy,ICAL_XLICCLASS_REQUESTFREEBUSY},

  {ICAL_METHOD_REPLY,icalclassify_reply_accept,ICAL_XLICCLASS_REPLYACCEPT},
  {ICAL_METHOD_REPLY,icalclassify_reply_decline,ICAL_XLICCLASS_REPLYDECLINE},
  {ICAL_METHOD_REPLY,icalclassify_reply_delegate,ICAL_XLICCLASS_REPLYDELEGATE},
  {ICAL_METHOD_REPLY,icalclassify_reply_crasher_accept,ICAL_XLICCLASS_REPLYCRASHERACCEPT},
  {ICAL_METHOD_REPLY,icalclassify_reply_crasher_decline,ICAL_XLICCLASS_REPLYCRASHERDECLINE},

  {ICAL_METHOD_ADD,icalclassify_add_instance,ICAL_XLICCLASS_ADDINSTANCE},

  {ICAL_METHOD_CANCEL,icalclassify_cancel_event,ICAL_XLICCLASS_CANCELEVENT},
  {ICAL_METHOD_CANCEL,icalclassify_cancel_instance,ICAL_XLICCLASS_CANCELINSTANCE},
  {ICAL_METHOD_CANCEL,icalclassify_cancel_all,ICAL_XLICCLASS_CANCELALL},

  {ICAL_METHOD_REFRESH,icalclassify_refesh,ICAL_XLICCLASS_REFRESH},
  {ICAL_METHOD_COUNTER,icalclassify_counter,ICAL_XLICCLASS_COUNTER},
  {ICAL_METHOD_DECLINECOUNTER,icalclassify_delinecounter,ICAL_XLICCLASS_DECLINECOUNTER},
  {ICAL_METHOD_NONE,0,ICAL_XLICCLASS_NONE}
};


icalproperty_xlicclass icalclassify(icalcomponent* c,icalcomponent* match, 
			      const char* user)
{
    icalcomponent *inner;
    icalproperty *p;
    icalproperty_method method;
    icalproperty_xlicclass class = ICAL_XLICCLASS_UNKNOWN;

    int i;

    struct icalclassify_parts comp_parts;
    struct icalclassify_parts match_parts;

    inner = icalcomponent_get_first_real_component(c);
    
    if (inner == 0) {
	return ICAL_XLICCLASS_NONE;
    }

    icalssutil_get_parts(c,&comp_parts);
    icalssutil_get_parts(match,&match_parts);

    /* Determine if the incoming component is obsoleted by the match */
    if(match != 0 && (
	comp_parts.method == ICAL_METHOD_REQUEST
	)){
	assert ( ! ((comp_parts.dtstamp.is_utc==1)^
		    (match_parts.dtstamp.is_utc==1)));

	if( comp_parts.sequence<match_parts.sequence &&
	    icaltime_compare(comp_parts.dtstamp,match_parts.dtstamp)>0)
	{
	    /* comp has a smaller sequence and a later DTSTAMP */
	    class = ICAL_XLICCLASS_MISSEQUENCED;
	    goto CLEANUP;
	}

	if( (comp_parts.sequence<match_parts.sequence )
	     /*&&icaltime_compare(comp_parts.dtstamp,match_parts.dtstamp)<=0*/
	     ||
	   ( comp_parts.sequence == match_parts.sequence &&
	     icaltime_compare(comp_parts.dtstamp,match_parts.dtstamp)<=0)){

	    class = ICAL_XLICCLASS_OBSOLETE;
	    goto CLEANUP;
	}

    }

    p = icalcomponent_get_first_property(c,ICAL_METHOD_PROPERTY);
    if (p == 0) {
	class = ICAL_XLICCLASS_UNKNOWN;
	goto CLEANUP;
    }
    method = icalproperty_get_method(p);

    for (i =0; icalclassify_map[i].method != ICAL_METHOD_NONE; i++){
	if(icalclassify_map[i].method == method){
	    if( (*(icalclassify_map[i].fn))(&comp_parts,&match_parts,user)==1){
		class = icalclassify_map[i].class;
		break;
	    }
	}
    }

CLEANUP:
    icalssutil_free_parts(&comp_parts); 
    icalssutil_free_parts(&match_parts);

    return class;

}

