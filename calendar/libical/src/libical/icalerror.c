/* -*- Mode: C -*-
  ======================================================================
  FILE: icalerror.c
  CREATOR: eric 16 May 1999
  
  $Id: icalerror.c,v 1.17 2002/10/09 20:48:08 acampi Exp $
  $Locker:  $
    

 (C) COPYRIGHT 2000, Eric Busboom, http://www.softwarestudio.org

 This program is free software; you can redistribute it and/or modify
 it under the terms of either: 

    The LGPL as published by the Free Software Foundation, version
    2.1, available at: http://www.fsf.org/copyleft/lesser.html

  Or:

    The Mozilla Public License Version 1.0. You may obtain a copy of
    the License at http://www.mozilla.org/MPL/

  The original code is icalerror.c

 ======================================================================*/


#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <stdlib.h>		/* for malloc() */
#include <string.h>		/* for strcmp */
#include "icalerror.h"

#ifdef HAVE_PTHREAD
#include <pthread.h>

static pthread_key_t  icalerrno_key;
static pthread_once_t icalerrno_key_once = PTHREAD_ONCE_INIT;

static void icalerrno_destroy(void* buf) {
  free(buf);
  pthread_setspecific(icalerrno_key, NULL);
}

static void icalerrno_key_alloc(void) {
  pthread_key_create(&icalerrno_key, icalerrno_destroy);
}

icalerrorenum *icalerrno_return(void) {
  icalerrorenum *_errno;

  pthread_once(&icalerrno_key_once, icalerrno_key_alloc);
  
  _errno = (icalerrorenum*) pthread_getspecific(icalerrno_key);

  if (!_errno) {
    _errno = malloc(sizeof(icalerrorenum));
    *_errno = ICAL_NO_ERROR;
    pthread_setspecific(icalerrno_key, _errno);
  }
  return _errno;
}

#else

static icalerrorenum icalerrno_storage = ICAL_NO_ERROR;

icalerrorenum *icalerrno_return(void) {
   return &icalerrno_storage;
}

#endif


static int foo;

void icalerror_stop_here(void)
{
    foo++; /* Keep optimizers from removing routine */
}

void icalerror_crash_here(void)
{
    int *p=0;
    *p = 1;

    assert( *p);
}

#ifdef ICAL_SETERROR_ISFUNC
void icalerror_set_errno(icalerrorenum x) 
{
    icalerrno = x; 
    if(icalerror_get_error_state(x)==ICAL_ERROR_FATAL || 
       (icalerror_get_error_state(x)==ICAL_ERROR_DEFAULT && 
        icalerror_errors_are_fatal == 1 )){ 
        icalerror_warn(icalerror_strerror(x)); 
        assert(0); 
    } 

}
#endif 

void icalerror_clear_errno() {
    
    icalerrno = ICAL_NO_ERROR;
}

#ifdef ICAL_ERRORS_ARE_FATAL
int icalerror_errors_are_fatal = 1;
#else
int icalerror_errors_are_fatal = 0;
#endif

struct icalerror_state {
    icalerrorenum error;
    icalerrorstate state; 
};

static struct icalerror_state error_state_map[] = 
{ 
    { ICAL_BADARG_ERROR,ICAL_ERROR_DEFAULT},
    { ICAL_NEWFAILED_ERROR,ICAL_ERROR_DEFAULT},
    { ICAL_ALLOCATION_ERROR,ICAL_ERROR_DEFAULT},
    { ICAL_MALFORMEDDATA_ERROR,ICAL_ERROR_DEFAULT}, 
    { ICAL_PARSE_ERROR,ICAL_ERROR_DEFAULT},
    { ICAL_INTERNAL_ERROR,ICAL_ERROR_DEFAULT}, 
    { ICAL_FILE_ERROR,ICAL_ERROR_DEFAULT},
    { ICAL_USAGE_ERROR,ICAL_ERROR_DEFAULT},
    { ICAL_UNIMPLEMENTED_ERROR,ICAL_ERROR_DEFAULT},
    { ICAL_UNKNOWN_ERROR,ICAL_ERROR_DEFAULT},
    { ICAL_NO_ERROR,ICAL_ERROR_DEFAULT}

};

struct icalerror_string_map {
    const char* str;
    icalerrorenum error;
    char name[160];
};

static struct icalerror_string_map string_map[] = 
{
    {"BADARG",ICAL_BADARG_ERROR,"BADARG: Bad argument to function"},
    { "NEWFAILED",ICAL_NEWFAILED_ERROR,"NEWFAILED: Failed to create a new object via a *_new() routine"},
    { "ALLOCATION",ICAL_ALLOCATION_ERROR,"ALLOCATION: Failed to allocate new memory"},
    {"MALFORMEDDATA",ICAL_MALFORMEDDATA_ERROR,"MALFORMEDDATA: An input string was not correctly formed or a component has missing or extra properties"},
    { "PARSE",ICAL_PARSE_ERROR,"PARSE: Failed to parse a part of an iCal component"},
    {"INTERNAL",ICAL_INTERNAL_ERROR,"INTERNAL: Random internal error. This indicates an error in the library code, not an error in use"}, 
    { "FILE",ICAL_FILE_ERROR,"FILE: An operation on a file failed. Check errno for more detail."},
    { "USAGE",ICAL_USAGE_ERROR,"USAGE: Failed to propertyl sequence calls to a set of interfaces"},
    { "UNIMPLEMENTED",ICAL_UNIMPLEMENTED_ERROR,"UNIMPLEMENTED: This feature has not been implemented"},
    { "NO",ICAL_NO_ERROR,"NO: No error"},
    {"UNKNOWN",ICAL_UNKNOWN_ERROR,"UNKNOWN: Unknown error type -- icalerror_strerror() was probably given bad input"}
};


icalerrorenum icalerror_error_from_string(const char* str){
 
    icalerrorenum e;
    int i = 0;

    for( i = 0; string_map[i].error != ICAL_NO_ERROR; i++){
        if (strcmp(string_map[i].str,str) == 0){
            e = string_map[i].error;
        }
    }

    return e;
}

icalerrorstate icalerror_supress(const char* error){

    icalerrorenum e = icalerror_error_from_string(error);
    icalerrorstate es;

     if (e == ICAL_NO_ERROR){
        return ICAL_ERROR_UNKNOWN;
    }


    es = icalerror_get_error_state(e);
    icalerror_set_error_state(e,ICAL_ERROR_NONFATAL);

    return es;
}

char* icalerror_perror()
{
    return icalerror_strerror(icalerrno);
}

void icalerror_restore(const char* error, icalerrorstate es){


    icalerrorenum e = icalerror_error_from_string(error);

    if (e != ICAL_NO_ERROR){
        icalerror_set_error_state(e,es);
    }

}



void icalerror_set_error_state( icalerrorenum error, 
				icalerrorstate state)
{
    int i;

    for(i = 0; error_state_map[i].error!= ICAL_NO_ERROR;i++){
	if(error_state_map[i].error == error){
	    error_state_map[i].state = state; 	
	}
    }
}

icalerrorstate icalerror_get_error_state( icalerrorenum error)
{
    int i;

    for(i = 0; error_state_map[i].error!= ICAL_NO_ERROR;i++){
	if(error_state_map[i].error == error){
	    return error_state_map[i].state; 	
	}
    }

    return ICAL_ERROR_UNKNOWN;	
}




char* icalerror_strerror(icalerrorenum e) {

    int i;

    for (i=0; string_map[i].error != ICAL_UNKNOWN_ERROR; i++) {
	if (string_map[i].error == e) {
	    return string_map[i].name;
	}
    }

    return string_map[i].name; /* Return string for ICAL_UNKNOWN_ERROR*/
    
}



