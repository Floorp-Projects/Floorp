/* -*- Mode: C -*- */
/*======================================================================
  FILE: icalerror.h
  CREATOR: eric 09 May 1999
  
  $Id: icalerror.h,v 1.14 2002/07/21 14:58:17 lindner Exp $


 (C) COPYRIGHT 2000, Eric Busboom, http://www.softwarestudio.org

 This program is free software; you can redistribute it and/or modify
 it under the terms of either: 

    The LGPL as published by the Free Software Foundation, version
    2.1, available at: http://www.fsf.org/copyleft/lesser.html

  Or:

    The Mozilla Public License Version 1.0. You may obtain a copy of
    the License at http://www.mozilla.org/MPL/

  The original code is icalerror.h

======================================================================*/


#ifndef ICALERROR_H
#define ICALERROR_H

#include <assert.h>
#include <stdio.h> /* For icalerror_warn() */


#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#define ICAL_SETERROR_ISFUNC


/** This routine is called before any error is triggered. It is called
   by icalerror_set_errno, so it does not appear in all of the macros
   below */
void icalerror_stop_here(void);

void icalerror_crash_here(void);

typedef enum icalerrorenum {
    ICAL_NO_ERROR,     /* icalerrno may not be initialized - put it first so and pray that the compiler initialize things to zero */    
    ICAL_BADARG_ERROR,
    ICAL_NEWFAILED_ERROR,
    ICAL_ALLOCATION_ERROR,
    ICAL_MALFORMEDDATA_ERROR, 
    ICAL_PARSE_ERROR,
    ICAL_INTERNAL_ERROR, /* Like assert --internal consist. prob */
    ICAL_FILE_ERROR,
    ICAL_USAGE_ERROR,
    ICAL_UNIMPLEMENTED_ERROR,
    ICAL_UNKNOWN_ERROR  /* Used for problems in input to icalerror_strerror()*/

} icalerrorenum;

icalerrorenum * icalerrno_return(void);
#define icalerrno (*(icalerrno_return()))

/** If true, libicl aborts after a call to icalerror_set_error
 *
 *  @warning NOT THREAD SAFE -- recommended that you do not change
 *           this in a multithreaded program.
 */
extern int icalerror_errors_are_fatal;

/* Warning messages */

#ifdef __GNUC__ca
#define icalerror_warn(message) {fprintf(stderr,"%s(), %s:%d: %s\n",__FUNCTION__,__FILE__,__LINE__,message);}
#else /* __GNU_C__ */
#define icalerror_warn(message) {fprintf(stderr,"%s:%d: %s\n",__FILE__,__LINE__,message);}
#endif /* __GNU_C__ */


void icalerror_clear_errno(void);
void _icalerror_set_errno(icalerrorenum);

/* Make an individual error fatal or non-fatal. */
typedef enum icalerrorstate { 
    ICAL_ERROR_FATAL,     /* Not fata */
    ICAL_ERROR_NONFATAL,  /* Fatal */
    ICAL_ERROR_DEFAULT,   /* Use the value of icalerror_errors_are_fatal*/
    ICAL_ERROR_UNKNOWN    /* Asked state for an unknown error type */
} icalerrorstate ;

char* icalerror_strerror(icalerrorenum e);
char* icalerror_perror();
void icalerror_set_error_state( icalerrorenum error, icalerrorstate);
icalerrorstate icalerror_get_error_state( icalerrorenum error);

#ifndef ICAL_SETERROR_ISFUNC
#define icalerror_set_errno(x) \
icalerrno = x; \
if(icalerror_get_error_state(x)==ICAL_ERROR_FATAL || \
   (icalerror_get_error_state(x)==ICAL_ERROR_DEFAULT && \
    icalerror_errors_are_fatal == 1 )){ \
   icalerror_warn(icalerror_strerror(x)); \
   assert(0); \
} 
#else
void icalerror_set_errno(icalerrorenum x); 
#endif

#ifdef ICAL_ERRORS_ARE_FATAL
#undef NDEBUG
#endif

#define icalerror_check_value_type(value,type);
#define icalerror_check_property_type(value,type);
#define icalerror_check_parameter_type(value,type);
#define icalerror_check_component_type(value,type);

/* Assert with a message */
#ifdef ICAL_ERRORS_ARE_FATAL

#ifdef __GNUC__
#define icalerror_assert(test,message) if(!(test)){fprintf(stderr,"%s(), %s:%d: %s\n",__FUNCTION__,__FILE__,__LINE__,message);icalerror_stop_here(); abort();}
#else /*__GNUC__*/
#define icalerror_assert(test,message) if(!(test)){fprintf(stderr,"%s:%d: %s\n",__FILE__,__LINE__,message);icalerror_stop_here(); abort();}
#endif /*__GNUC__*/

#else /* ICAL_ERRORS_ARE_FATAL */
#define icalerror_assert(test,message) 
#endif /* ICAL_ERRORS_ARE_FATAL */

/* Check & abort if check fails */
#define icalerror_check_arg(test,arg) if(!(test)) { icalerror_set_errno(ICAL_BADARG_ERROR); }

/* Check & return void if check fails*/
#define icalerror_check_arg_rv(test,arg) if(!(test)) {icalerror_set_errno(ICAL_BADARG_ERROR); return; }

/* Check & return 0 if check fails*/
#define icalerror_check_arg_rz(test,arg) if(!(test)) { icalerror_set_errno(ICAL_BADARG_ERROR); return 0;}

/* Check & return an error if check fails*/
#define icalerror_check_arg_re(test,arg,error) if(!(test)) { icalerror_stop_here(); assert(0); return error;}

/* Check & return something*/
#define icalerror_check_arg_rx(test,arg,x) if(!(test)) { icalerror_set_errno(ICAL_BADARG_ERROR); return x;}



/* String interfaces to set an error to NONFATAL and restore it to its
   original value */

icalerrorstate icalerror_supress(const char* error);
void icalerror_restore(const char* error, icalerrorstate es);


#endif /* !ICALERROR_H */



