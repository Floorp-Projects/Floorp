#ifdef __cplusplus
extern "C" {
#endif
/* config.h.  Generated automatically by configure.  */
/* config.h.in.  Generated automatically from configure.in by autoheader.  */

/* Define to empty if the keyword does not work.  */
/* #undef const */

/* Define to `unsigned' if <sys/types.h> doesn't define.  */
/* #undef size_t */

/* Define if you have the ANSI C header files.  */
#define STDC_HEADERS 1

/* Define if your <sys/time.h> declares struct tm.  */
/* #undef TM_IN_SYS_TIME */

/* Define if lex declares yytext as a char * by default, not a char[].  */
#define YYTEXT_POINTER 1

/* Define if you have the gmtime_r function.  */
#define HAVE_GMTIME_R 1

/* Define if you have the iswspace function.  */
#define HAVE_ISWSPACE 1

/* Define if you have the snprintf function.  */
#define HAVE_SNPRINTF 1

/* Define if you have the strdup function.  */
#define HAVE_STRDUP 1

/* Define if you have the <assert.h> header file.  */
#define HAVE_ASSERT_H 1

/* Define if you have the <dlfcn.h> header file.  */
#define HAVE_DLFCN_H 1

/* Define if you have the <pthread.h> header file.  */
#define HAVE_PTHREAD_H 1

/* Define if you have the <sys/types.h> header file.  */
#define HAVE_SYS_TYPES_H 1

/* Define if you have the <time.h> header file.  */
#define HAVE_TIME_H 1

/* Define if you have the <unistd.h> header file.  */
#define HAVE_UNISTD_H 1

/* Define if you have the <wctype.h> header file.  */
#define HAVE_WCTYPE_H 1

/* Name of package */
#define PACKAGE "libical"

/* Version number of package */
#define VERSION "0.24"

/* Define if we want _REENTRANT */
/* #undef ICAL_REENTRANT */

/* Define to make icalerror_* calls abort instead of internally signalling an error */
#define ICAL_ERRORS_ARE_FATAL 1

/* Define to 1 if you DO NOT WANT to see deprecated messages */
#define NO_WARN_DEPRECATED 1

/* Define to 1 if you DO NO WANT to see the warning messages related to ICAL_MALFORMEDDATA_ERROR and parsing .ics zoneinfo files */
#define NO_WARN_ICAL_MALFORMEDDATA_ERROR_HACK 1

/* Define to terminate lines with n instead of rn */
#undef ICAL_UNIX_NEWLINE

/* Define if we have pthread. */
#define HAVE_PTHREAD 

/* Define if we have pthread. */
#define HAVE_PTHREAD 

#ifndef ICAL_VERSION_H
#define ICAL_VERSION_H

#define ICAL_PACKAGE "libical"
#define ICAL_VERSION "0.24"

#endif
/* -*- Mode: C -*- */
/*======================================================================
 FILE: icaltime.h
 CREATOR: eric 02 June 2000


 $Id: ical.h,v 1.6 2002/10/10 00:41:34 acampi Exp $
 $Locker:  $

 (C) COPYRIGHT 2000, Eric Busboom, http://www.softwarestudio.org

 This program is free software; you can redistribute it and/or modify
 it under the terms of either: 

    The LGPL as published by the Free Software Foundation, version
    2.1, available at: http://www.fsf.org/copyleft/lesser.html

  Or:

    The Mozilla Public License Version 1.0. You may obtain a copy of
    the License at http://www.mozilla.org/MPL/

 The Original Code is eric. The Initial Developer of the Original
 Code is Eric Busboom


======================================================================*/

/**	@file icaltime.h
 *	@brief struct icaltimetype is a pseudo-object that abstracts time
 *	handling.
 *
 *	It can represent either a DATE or a DATE-TIME (floating, UTC or in a
 *	given timezone), and it keeps track internally of its native timezone.
 *
 *	The typical usage is to call the correct constructor specifying the
 *	desired timezone. If this is not known until a later time, the
 *	correct behavior is to specify a NULL timezone and call
 *	icaltime_convert_to_zone() at a later time.
 *
 *	There are several ways to create a new icaltimetype:
 *
 *	- icaltime_null_time()
 *	- icaltime_null_date()
 *	- icaltime_current_time_with_zone()
 *	- icaltime_today()
 *	- icaltime_from_timet_with_zone(time_t tm, int is_date,
 *		icaltimezone *zone)
 * 	- icaltime_from_string_with_zone(const char* str, icaltimezone *zone)
 *	- icaltime_from_day_of_year(short doy,  short year)
 *	- icaltime_from_week_number(short week_number, short year)
 *
 *	italtimetype objects can be converted to different formats:
 *
 *	- icaltime_as_timet(struct icaltimetype tt)
 *	- icaltime_as_timet_with_zone(struct icaltimetype tt,
 *		icaltimezone *zone)
 *	- icaltime_as_ical_string(struct icaltimetype tt)
 *
 *	Accessor methods include:
 *
 *	- icaltime_get_timezone(struct icaltimetype t)
 *	- icaltime_get_tzid(struct icaltimetype t)
 *	- icaltime_set_timezone(struct icaltimetype t, const icaltimezone *zone)
 *	- icaltime_day_of_year(struct icaltimetype t)
 *	- icaltime_day_of_week(struct icaltimetype t)
 *	- icaltime_start_doy_of_week(struct icaltimetype t)
 *	- icaltime_week_number(struct icaltimetype t)
 *
 *	Query methods include:
 *
 *	- icaltime_is_null_time(struct icaltimetype t)
 *	- icaltime_is_valid_time(struct icaltimetype t)
 *	- icaltime_is_date(struct icaltimetype t)
 *	- icaltime_is_utc(struct icaltimetype t)
 *	- icaltime_is_floating(struct icaltimetype t)
 *
 *	Modify, compare and utility methods include:
 *
 *	- icaltime_add(struct icaltimetype t, struct icaldurationtype  d)
 *	- icaltime_subtract(struct icaltimetype t1, struct icaltimetype t2)
 *      - icaltime_compare_with_zone(struct icaltimetype a,struct icaltimetype b)
 *	- icaltime_compare(struct icaltimetype a,struct icaltimetype b)
 *	- icaltime_compare_date_only(struct icaltimetype a,
 *		struct icaltimetype b)
 *	- icaltime_adjust(struct icaltimetype *tt, int days, int hours,
 *		int minutes, int seconds);
 *	- icaltime_normalize(struct icaltimetype t);
 *	- icaltime_convert_to_zone(const struct icaltimetype tt,
 *		icaltimezone *zone);
 */

#ifndef ICALTIME_H
#define ICALTIME_H

#include <time.h>

/* An opaque struct representing a timezone. We declare this here to avoid
   a circular dependancy. */
#ifndef ICALTIMEONE_DEFINED
#define ICALTIMEONE_DEFINED
typedef struct _icaltimezone		icaltimezone;
#endif

/** icaltime_span is returned by icalcomponent_get_span() */
struct icaltime_span {
	time_t start;   /**< in UTC */
	time_t end;     /**< in UTC */
	int is_busy;    /**< 1->busy time, 0-> free time */
};

typedef struct icaltime_span icaltime_span;

/*
 *	FIXME
 *
 *	is_utc is redundant, and might be considered a minor optimization.
 *	It might be deprecated, so you should use icaltime_is_utc() instead.
 */
struct icaltimetype
{
	int year;	/**< Actual year, e.g. 2001. */
	int month;	/**< 1 (Jan) to 12 (Dec). */
	int day;
	int hour;
	int minute;
	int second;

	int is_utc;     /**< 1-> time is in UTC timezone */

	int is_date;    /**< 1 -> interpret this as date. */
   
	int is_daylight; /**< 1 -> time is in daylight savings time. */
   
	const icaltimezone *zone;	/**< timezone */
};	

typedef struct icaltimetype icaltimetype;

/** Return a null time, which indicates no time has been set. 
    This time represent the beginning of the epoch */
struct icaltimetype icaltime_null_time(void);

/** Return a null date */
struct icaltimetype icaltime_null_date(void);

/** Returns the current time in the given timezone, as an icaltimetype. */
struct icaltimetype icaltime_current_time_with_zone(const icaltimezone *zone);

/** Returns the current day as an icaltimetype, with is_date set. */
struct icaltimetype icaltime_today(void);

/** Convert seconds past UNIX epoch to a timetype*/
struct icaltimetype icaltime_from_timet(const time_t v, const int is_date);

/** Convert seconds past UNIX epoch to a timetype, using timezones. */
struct icaltimetype icaltime_from_timet_with_zone(const time_t tm,
	const int is_date, const icaltimezone *zone);

/** create a time from an ISO format string */
struct icaltimetype icaltime_from_string(const char* str);

/** create a time from an ISO format string */
struct icaltimetype icaltime_from_string_with_zone(const char* str,
	const icaltimezone *zone);

/** Create a new time, given a day of year and a year. */
struct icaltimetype icaltime_from_day_of_year(const short doy,
	const short year);

/**	@brief Contructor (TODO).
 * Create a new time from a weeknumber and a year. */
struct icaltimetype icaltime_from_week_number(const short week_number,
	const short year);

/** Return the time as seconds past the UNIX epoch */
time_t icaltime_as_timet(const struct icaltimetype);

/** Return the time as seconds past the UNIX epoch, using timezones. */
time_t icaltime_as_timet_with_zone(const struct icaltimetype tt,
	const icaltimezone *zone);

/** Return a string represention of the time, in RFC2445 format. The
   string is owned by libical */
const char* icaltime_as_ical_string(const struct icaltimetype tt);

/** @brief Return the timezone */
const icaltimezone *icaltime_get_timezone(const struct icaltimetype t);

/** @brief Return the tzid, or NULL for a floating time */
char *icaltime_get_tzid(const struct icaltimetype t);

/** @brief Set the timezone */
struct icaltimetype icaltime_set_timezone(struct icaltimetype *t,
	const icaltimezone *zone);

/** Return the day of the year of the given time */
short icaltime_day_of_year(const struct icaltimetype t);

/** Return the day of the week of the given time. Sunday is 1 */
short icaltime_day_of_week(const struct icaltimetype t);

/** Return the day of the year for the Sunday of the week that the
   given time is within. */
short icaltime_start_doy_of_week(const struct icaltimetype t);

/** Return the week number for the week the given time is within */
short icaltime_week_number(const struct icaltimetype t);

/** Return true of the time is null. */
int icaltime_is_null_time(const struct icaltimetype t);

/** Returns false if the time is clearly invalid, but is not null. This
   is usually the result of creating a new time type buy not clearing
   it, or setting one of the flags to an illegal value. */
int icaltime_is_valid_time(const struct icaltimetype t);

/** @brief Returns true if time is of DATE type, false if DATE-TIME */
int icaltime_is_date(const struct icaltimetype t);

/** @brief Returns true if time is relative to UTC zone */
int icaltime_is_utc(const struct icaltimetype t);

/** @brief Returns true if time is a floating time */
int icaltime_is_floating(const struct icaltimetype t);

/** Return -1, 0, or 1 to indicate that a<b, a==b or a>b */
int icaltime_compare_with_zone(const struct icaltimetype a,
        const struct icaltimetype b);

/** Return -1, 0, or 1 to indicate that a<b, a==b or a>b */
int icaltime_compare(const struct icaltimetype a,
	const struct icaltimetype b);

/** like icaltime_compare, but only use the date parts. */
int icaltime_compare_date_only(const struct icaltimetype a,
	const struct icaltimetype b);

/** Adds or subtracts a number of days, hours, minutes and seconds. */
void  icaltime_adjust(struct icaltimetype *tt, const int days,
	const int hours, const int minutes, const int seconds);

/** Normalize the icaltime, so that all fields are within the normal range. */
struct icaltimetype icaltime_normalize(const struct icaltimetype t);

/** convert tt, of timezone tzid, into a utc time. Does nothing if the
   time is already UTC.  */
struct icaltimetype icaltime_convert_to_zone(const struct icaltimetype tt,
	const icaltimezone *zone);

/** Return the number of days in the given month */
short icaltime_days_in_month(const short month, const short year);


/** @brief calculate an icaltimespan given a start and end time. */
struct icaltime_span icaltime_span_new(struct icaltimetype dtstart,
				       struct icaltimetype dtend,
				       int is_busy);

/** @brief Returns true if the two spans overlap **/
int icaltime_span_overlaps(icaltime_span *s1, 
			   icaltime_span *s2);

/** @brief Returns true if the span is totally within the containing
 *  span 
 */
int icaltime_span_contains(icaltime_span *s,
			   icaltime_span *container);


#endif /* !ICALTIME_H */


/* -*- Mode: C -*- */
/*======================================================================
 FILE: icalduration.h
 CREATOR: eric 26 Jan 2001


 $Id: ical.h,v 1.6 2002/10/10 00:41:34 acampi Exp $
 $Locker:  $

 (C) COPYRIGHT 2000, Eric Busboom, http://www.softwarestudio.org

 This program is free software; you can redistribute it and/or modify
 it under the terms of either: 

    The LGPL as published by the Free Software Foundation, version
    2.1, available at: http://www.fsf.org/copyleft/lesser.html

  Or:

    The Mozilla Public License Version 1.0. You may obtain a copy of
    the License at http://www.mozilla.org/MPL/

 The Original Code is eric. The Initial Developer of the Original
 Code is Eric Busboom


======================================================================*/

#ifndef ICALDURATION_H
#define ICALDURATION_H


struct icaldurationtype
{
	int is_neg;
	unsigned int days;
	unsigned int weeks;
	unsigned int hours;
	unsigned int minutes;
	unsigned int seconds;
};

struct icaldurationtype icaldurationtype_from_int(int t);
struct icaldurationtype icaldurationtype_from_string(const char*);
int icaldurationtype_as_int(struct icaldurationtype duration);
char* icaldurationtype_as_ical_string(struct icaldurationtype d);
struct icaldurationtype icaldurationtype_null_duration(void);
int icaldurationtype_is_null_duration(struct icaldurationtype d);

struct icaltimetype  icaltime_add(struct icaltimetype t,
				  struct icaldurationtype  d);

struct icaldurationtype  icaltime_subtract(struct icaltimetype t1,
					   struct icaltimetype t2);

#endif /* !ICALDURATION_H */



/* -*- Mode: C -*- */
/*======================================================================
 FILE: icalperiod.h
 CREATOR: eric 26 Jan 2001


 $Id: ical.h,v 1.6 2002/10/10 00:41:34 acampi Exp $
 $Locker:  $

 (C) COPYRIGHT 2000, Eric Busboom, http://www.softwarestudio.org

 This program is free software; you can redistribute it and/or modify
 it under the terms of either: 

    The LGPL as published by the Free Software Foundation, version
    2.1, available at: http://www.fsf.org/copyleft/lesser.html

  Or:

    The Mozilla Public License Version 1.0. You may obtain a copy of
    the License at http://www.mozilla.org/MPL/

 The Original Code is eric. The Initial Developer of the Original
 Code is Eric Busboom


======================================================================*/

#ifndef ICALPERIOD_H
#define ICALPERIOD_H


struct icalperiodtype 
{
	struct icaltimetype start; 
	struct icaltimetype end; 
	struct icaldurationtype duration;
};

struct icalperiodtype icalperiodtype_from_string (const char* str);

const char* icalperiodtype_as_ical_string(struct icalperiodtype p);

struct icalperiodtype icalperiodtype_null_period(void);

int icalperiodtype_is_null_period(struct icalperiodtype p);

int icalperiodtype_is_valid_period(struct icalperiodtype p);

#endif /* !ICALTIME_H */




/* -*- Mode: C -*-*/
/*======================================================================
 FILE: icalenums.h

 

 (C) COPYRIGHT 2000, Eric Busboom, http://www.softwarestudio.org

 This program is free software; you can redistribute it and/or modify
 it under the terms of either: 

    The LGPL as published by the Free Software Foundation, version
    2.1, available at: http://www.fsf.org/copyleft/lesser.html

  Or:

    The Mozilla Public License Version 1.0. You may obtain a copy of
    the License at http://www.mozilla.org/MPL/

  The original code is icalenums.h

  Contributions from:
     Graham Davison (g.m.davison@computer.org)

======================================================================*/

#ifndef ICALENUMS_H
#define ICALENUMS_H



/***********************************************************************
 * Component enumerations
**********************************************************************/

typedef enum icalcomponent_kind {
    ICAL_NO_COMPONENT,
    ICAL_ANY_COMPONENT,	/* Used to select all components*/
    ICAL_XROOT_COMPONENT,
    ICAL_XATTACH_COMPONENT, /* MIME attached data, returned by parser. */
    ICAL_VEVENT_COMPONENT,
    ICAL_VTODO_COMPONENT,
    ICAL_VJOURNAL_COMPONENT,
    ICAL_VCALENDAR_COMPONENT,
    ICAL_VAGENDA_COMPONENT,
    ICAL_VFREEBUSY_COMPONENT,
    ICAL_VALARM_COMPONENT,
    ICAL_XAUDIOALARM_COMPONENT,  
    ICAL_XDISPLAYALARM_COMPONENT,
    ICAL_XEMAILALARM_COMPONENT,
    ICAL_XPROCEDUREALARM_COMPONENT,
    ICAL_VTIMEZONE_COMPONENT,
    ICAL_XSTANDARD_COMPONENT,
    ICAL_XDAYLIGHT_COMPONENT,
    ICAL_X_COMPONENT,
    ICAL_VSCHEDULE_COMPONENT,
    ICAL_VQUERY_COMPONENT,
    ICAL_VCAR_COMPONENT,
    ICAL_VCOMMAND_COMPONENT,
    ICAL_XLICINVALID_COMPONENT,
    ICAL_XLICMIMEPART_COMPONENT /* a non-stardard component that mirrors
				structure of MIME data */

} icalcomponent_kind;



/***********************************************************************
 * Request Status codes
 **********************************************************************/

typedef enum icalrequeststatus {
    ICAL_UNKNOWN_STATUS,
    ICAL_2_0_SUCCESS_STATUS,
    ICAL_2_1_FALLBACK_STATUS,
    ICAL_2_2_IGPROP_STATUS,
    ICAL_2_3_IGPARAM_STATUS,
    ICAL_2_4_IGXPROP_STATUS,
    ICAL_2_5_IGXPARAM_STATUS,
    ICAL_2_6_IGCOMP_STATUS,
    ICAL_2_7_FORWARD_STATUS,
    ICAL_2_8_ONEEVENT_STATUS,
    ICAL_2_9_TRUNC_STATUS,
    ICAL_2_10_ONETODO_STATUS,
    ICAL_2_11_TRUNCRRULE_STATUS,
    ICAL_3_0_INVPROPNAME_STATUS,
    ICAL_3_1_INVPROPVAL_STATUS,
    ICAL_3_2_INVPARAM_STATUS,
    ICAL_3_3_INVPARAMVAL_STATUS,
    ICAL_3_4_INVCOMP_STATUS,
    ICAL_3_5_INVTIME_STATUS,
    ICAL_3_6_INVRULE_STATUS,
    ICAL_3_7_INVCU_STATUS,
    ICAL_3_8_NOAUTH_STATUS,
    ICAL_3_9_BADVERSION_STATUS,
    ICAL_3_10_TOOBIG_STATUS,
    ICAL_3_11_MISSREQCOMP_STATUS,
    ICAL_3_12_UNKCOMP_STATUS,
    ICAL_3_13_BADCOMP_STATUS,
    ICAL_3_14_NOCAP_STATUS,
    ICAL_3_15_INVCOMMAND,
    ICAL_4_0_BUSY_STATUS,
    ICAL_4_1_STORE_ACCESS_DENIED,
    ICAL_4_2_STORE_FAILED,
    ICAL_4_3_STORE_NOT_FOUND,
    ICAL_5_0_MAYBE_STATUS,
    ICAL_5_1_UNAVAIL_STATUS,
    ICAL_5_2_NOSERVICE_STATUS,
    ICAL_5_3_NOSCHED_STATUS,
    ICAL_6_1_CONTAINER_NOT_FOUND,
	ICAL_9_0_UNRECOGNIZED_COMMAND
} icalrequeststatus;


const char* icalenum_reqstat_desc(icalrequeststatus stat);
short icalenum_reqstat_major(icalrequeststatus stat);
short icalenum_reqstat_minor(icalrequeststatus stat);
icalrequeststatus icalenum_num_to_reqstat(short major, short minor);
char* icalenum_reqstat_code(icalrequeststatus stat);

/***********************************************************************
 * Conversion functions
**********************************************************************/


/* Thse routines used to be in icalenums.c, but were moved into the
   icalproperty, icalparameter, icalvalue, or icalcomponent modules. */

/* const char* icalproperty_kind_to_string(icalproperty_kind kind);*/
#define icalenum_property_kind_to_string(x) icalproperty_kind_to_string(x)

/*icalproperty_kind icalproperty_string_to_kind(const char* string)*/
#define icalenum_string_to_property_kind(x) icalproperty_string_to_kind(x)

/*icalvalue_kind icalproperty_kind_to_value_kind(icalproperty_kind kind);*/
#define icalenum_property_kind_to_value_kind(x) icalproperty_kind_to_value_kind(x)

/*const char* icalenum_method_to_string(icalproperty_method);*/
#define icalenum_method_to_string(x) icalproperty_method_to_string(x)

/*icalproperty_method icalenum_string_to_method(const char* string);*/
#define icalenum_string_to_method(x) icalproperty_string_to_method(x)

/*const char* icalenum_status_to_string(icalproperty_status);*/
#define icalenum_status_to_string(x) icalproperty_status_to_string(x)

/*icalproperty_status icalenum_string_to_status(const char* string);*/
#define icalenum_string_to_status(x) icalproperty_string_to_status(x)

/*icalvalue_kind icalenum_string_to_value_kind(const char* str);*/
#define icalenum_string_to_value_kind(x) icalvalue_string_to_kind(x)

/*const char* icalenum_value_kind_to_string(icalvalue_kind kind);*/
#define icalenum_value_kind_to_string(x) icalvalue_kind_to_string(x)

/*const char* icalenum_component_kind_to_string(icalcomponent_kind kind);*/
#define icalenum_component_kind_to_string(x) icalcomponent_kind_to_string(x)

/*icalcomponent_kind icalenum_string_to_component_kind(const char* string);*/
#define icalenum_string_to_component_kind(x) icalcomponent_string_to_kind(x)


#endif /* !ICALENUMS_H */

/* -*- Mode: C -*- */
/*======================================================================
 FILE: icaltypes.h
 CREATOR: eric 20 March 1999


 (C) COPYRIGHT 2000, Eric Busboom, http://www.softwarestudio.org

 This program is free software; you can redistribute it and/or modify
 it under the terms of either: 

    The LGPL as published by the Free Software Foundation, version
    2.1, available at: http://www.fsf.org/copyleft/lesser.html

  Or:

    The Mozilla Public License Version 1.0. You may obtain a copy of
    the License at http://www.mozilla.org/MPL/

  The original code is icaltypes.h

======================================================================*/

#ifndef ICALTYPES_H
#define ICALTYPES_H

#include <time.h>


struct icalgeotype 
{
	float lat;
	float lon;
};


struct icaldatetimeperiodtype 
{
	struct icaltimetype time;
	struct icalperiodtype period;
};


struct icaltriggertype 
{
	struct icaltimetype time; 
	struct icaldurationtype duration;
};

struct icaltriggertype icaltriggertype_from_int(const int reltime);
struct icaltriggertype icaltriggertype_from_string(const char* str);

int icaltriggertype_is_null_trigger(struct icaltriggertype tr);

/* struct icalreqstattype. This struct contains two string pointers,
but don't try to free either of them. The "desc" string is a pointer
to a static table inside the library.  Don't try to free it. The
"debug" string is a pointer into the string that the called passed
into to icalreqstattype_from_string. Don't try to free it either, and
don't use it after the original string has been freed.

BTW, you would get that original string from
*icalproperty_get_requeststatus() or icalvalue_get_text(), when
operating on a the value of a request_status property. */

struct icalreqstattype {

	icalrequeststatus code;
	const char* desc;
	const char* debug;
};

struct icalreqstattype icalreqstattype_from_string(const char* str);
const char* icalreqstattype_as_string(struct icalreqstattype);



struct icaltimezonephase {
    const char* tzname;
    int is_stdandard; /* 1 = standard tme, 0 = daylight savings time */
    struct icaltimetype dtstart;
    int offsetto;
    int tzoffsetfrom;
    const char* comment;
    struct icaldatetimeperiodtype rdate;
    const char* rrule;    
};


struct icaltimezonetype {
    const char* tzid;
    struct icaltimetype last_mod;
    const char* tzurl;
    
    /* Array of phases. The end of the array is a phase with tzname == 0 */
    struct icaltimezonephase *phases;
};

void icaltimezonetype_free(struct icaltimezonetype tzt);


#endif /* !ICALTYPES_H */
/* -*- Mode: C -*- */
/*======================================================================
 FILE: icalrecur.h
 CREATOR: eric 20 March 2000


 (C) COPYRIGHT 2000, Eric Busboom, http://www.softwarestudio.org

 This program is free software; you can redistribute it and/or modify
 it under the terms of either: 

    The LGPL as published by the Free Software Foundation, version
    2.1, available at: http://www.fsf.org/copyleft/lesser.html

  Or:

    The Mozilla Public License Version 1.0. You may obtain a copy of
    the License at http://www.mozilla.org/MPL/
*/

/**
@file icalrecur.h
@brief Routines for dealing with recurring time

How to use: 

1) Get a rule and a start time from a component

@code
        icalproperty rrule;
        struct icalrecurrencetype recur;
        struct icaltimetype dtstart;

	rrule = icalcomponent_get_first_property(comp,ICAL_RRULE_PROPERTY);
	recur = icalproperty_get_rrule(rrule);
	start = icalproperty_get_dtstart(dtstart);
@endcode

Or, just make them up: 

@code
        recur = icalrecurrencetype_from_string("FREQ=YEARLY;BYDAY=SU,WE");
        dtstart = icaltime_from_string("19970101T123000")
@endcode

2) Create an iterator

@code
        icalrecur_iterator* ritr;
        ritr = icalrecur_iterator_new(recur,start);
@endcode

3) Iterator over the occurrences

@code
        struct icaltimetype next;
        while (next = icalrecur_iterator_next(ritr) 
               && !icaltime_is_null_time(next){
                Do something with next
        }
@endcode

Note that that the time returned by icalrecur_iterator_next is in
whatever timezone that dtstart is in.

======================================================================*/

#ifndef ICALRECUR_H
#define ICALRECUR_H

#include <time.h>

/***********************************************************************
 * Recurrance enumerations
**********************************************************************/

typedef enum icalrecurrencetype_frequency
{
    /* These enums are used to index an array, so don't change the
       order or the integers */

    ICAL_SECONDLY_RECURRENCE=0,
    ICAL_MINUTELY_RECURRENCE=1,
    ICAL_HOURLY_RECURRENCE=2,
    ICAL_DAILY_RECURRENCE=3,
    ICAL_WEEKLY_RECURRENCE=4,
    ICAL_MONTHLY_RECURRENCE=5,
    ICAL_YEARLY_RECURRENCE=6,
    ICAL_NO_RECURRENCE=7

} icalrecurrencetype_frequency;

typedef enum icalrecurrencetype_weekday
{
    ICAL_NO_WEEKDAY,
    ICAL_SUNDAY_WEEKDAY,
    ICAL_MONDAY_WEEKDAY,
    ICAL_TUESDAY_WEEKDAY,
    ICAL_WEDNESDAY_WEEKDAY,
    ICAL_THURSDAY_WEEKDAY,
    ICAL_FRIDAY_WEEKDAY,
    ICAL_SATURDAY_WEEKDAY
} icalrecurrencetype_weekday;

enum {
    ICAL_RECURRENCE_ARRAY_MAX = 0x7f7f,
    ICAL_RECURRENCE_ARRAY_MAX_BYTE = 0x7f
};



/********************** Recurrence type routines **************/

/* See RFC 2445 Section 4.3.10, RECUR Value, for an explaination of
   the values and fields in struct icalrecurrencetype */

#define ICAL_BY_SECOND_SIZE 61
#define ICAL_BY_MINUTE_SIZE 61
#define ICAL_BY_HOUR_SIZE 25
#define ICAL_BY_DAY_SIZE 364 /* 7 days * 52 weeks */
#define ICAL_BY_MONTHDAY_SIZE 32
#define ICAL_BY_YEARDAY_SIZE 367
#define ICAL_BY_WEEKNO_SIZE 54
#define ICAL_BY_MONTH_SIZE 13
#define ICAL_BY_SETPOS_SIZE 367

/** Main struct for holding digested recurrence rules */
struct icalrecurrencetype 
{
	icalrecurrencetype_frequency freq;


	/* until and count are mutually exclusive. */
       	struct icaltimetype until; 
	int count;

	short interval;
	
	icalrecurrencetype_weekday week_start;
	
	/* The BY* parameters can each take a list of values. Here I
	 * assume that the list of values will not be larger than the
	 * range of the value -- that is, the client will not name a
	 * value more than once. 
	 
	 * Each of the lists is terminated with the value
	 * ICAL_RECURRENCE_ARRAY_MAX unless the the list is full.
	 */

	short by_second[ICAL_BY_SECOND_SIZE];
	short by_minute[ICAL_BY_MINUTE_SIZE];
	short by_hour[ICAL_BY_HOUR_SIZE];
	short by_day[ICAL_BY_DAY_SIZE]; /* Encoded value, see below */
	short by_month_day[ICAL_BY_MONTHDAY_SIZE];
	short by_year_day[ ICAL_BY_YEARDAY_SIZE];
	short by_week_no[ICAL_BY_WEEKNO_SIZE];
	short by_month[ICAL_BY_MONTH_SIZE];
	short by_set_pos[ICAL_BY_SETPOS_SIZE];
};


void icalrecurrencetype_clear(struct icalrecurrencetype *r);

/**
 * Array Encoding
 *
 * The 'day' element of the by_day array is encoded to allow
 * representation of both the day of the week ( Monday, Tueday), but also
 * the Nth day of the week ( First tuesday of the month, last thursday of
 * the year) These routines decode the day values 
 */

/** 1 == Monday, etc. */
enum icalrecurrencetype_weekday icalrecurrencetype_day_day_of_week(short day);

/** 0 == any of day of week. 1 == first, 2 = second, -2 == second to last, etc */
short icalrecurrencetype_day_position(short day);


/***********************************************************************
 * Recurrance rule parser
**********************************************************************/

/** Convert between strings and recurrencetype structures. */
struct icalrecurrencetype icalrecurrencetype_from_string(const char* str);
char* icalrecurrencetype_as_string(struct icalrecurrencetype *recur);


/********** recurrence iteration routines ********************/

typedef struct icalrecur_iterator_impl  icalrecur_iterator;

/** Create a new recurrence rule iterator */
icalrecur_iterator* icalrecur_iterator_new(struct icalrecurrencetype rule, 
                                           struct icaltimetype dtstart);

/** Get the next occurrence from an iterator */
struct icaltimetype icalrecur_iterator_next(icalrecur_iterator*);

/** Free the iterator */
void icalrecur_iterator_free(icalrecur_iterator*);

/**
 * Fills array up with at most 'count' time_t values, each
 *  representing an occurrence time in seconds past the POSIX epoch 
 */
int icalrecur_expand_recurrence(char* rule, time_t start, 
				int count, time_t* array);


#endif
/* -*- Mode: C -*- */
/*======================================================================
 FILE: icalattach.h
 CREATOR: acampi 28 May 02


 (C) COPYRIGHT 2002, Andrea Campi

 This program is free software; you can redistribute it and/or modify
 it under the terms of either: 

    The LGPL as published by the Free Software Foundation, version
    2.1, available at: http://www.fsf.org/copyleft/lesser.html

  Or:

    The Mozilla Public License Version 1.0. You may obtain a copy of
    the License at http://www.mozilla.org/MPL/

  The original code is icalattach.h

======================================================================*/

#ifndef ICALATTACH_H
#define ICALATTACH_H


typedef struct icalattach_impl icalattach;

typedef void (* icalattach_free_fn_t) (unsigned char *data, void *user_data);

icalattach *icalattach_new_from_url (const char *url);
icalattach *icalattach_new_from_data (const unsigned char *data,
	icalattach_free_fn_t free_fn, void *free_fn_data);

void icalattach_ref (icalattach *attach);
void icalattach_unref (icalattach *attach);

int icalattach_get_is_url (icalattach *attach);
const char *icalattach_get_url (icalattach *attach);
unsigned char *icalattach_get_data (icalattach *attach);

struct icalattachtype* icalattachtype_new(void);
void  icalattachtype_add_reference(struct icalattachtype* v);
void icalattachtype_free(struct icalattachtype* v);

void icalattachtype_set_url(struct icalattachtype* v, char* url);
char* icalattachtype_get_url(struct icalattachtype* v);

void icalattachtype_set_base64(struct icalattachtype* v, char* base64,
				int owns);
char* icalattachtype_get_base64(struct icalattachtype* v);

void icalattachtype_set_binary(struct icalattachtype* v, char* binary,
				int owns);
void* icalattachtype_get_binary(struct icalattachtype* v);



#endif /* !ICALATTACH_H */
/* -*- Mode: C -*- */
/*======================================================================
  FILE: icalvalue.h
  CREATOR: eric 20 March 1999


  $Id: ical.h,v 1.6 2002/10/10 00:41:34 acampi Exp $
  $Locker:  $

  

 (C) COPYRIGHT 2000, Eric Busboom, http://www.softwarestudio.org

 This program is free software; you can redistribute it and/or modify
 it under the terms of either: 

    The LGPL as published by the Free Software Foundation, version
    2.1, available at: http://www.fsf.org/copyleft/lesser.html

  Or:

    The Mozilla Public License Version 1.0. You may obtain a copy of
    the License at http://www.mozilla.org/MPL/

  The original code is icalvalue.h

  ======================================================================*/

#ifndef ICALDERIVEDVALUE_H
#define ICALDERIVEDVALUE_H

     
typedef struct icalvalue_impl icalvalue;



void icalvalue_set_x(icalvalue* value, const char* v);
icalvalue* icalvalue_new_x(const char* v);
const char* icalvalue_get_x(const icalvalue* value);

icalvalue* icalvalue_new_recur (struct icalrecurrencetype v);
void icalvalue_set_recur(icalvalue* value, struct icalrecurrencetype v);
struct icalrecurrencetype icalvalue_get_recur(const icalvalue* value);

icalvalue* icalvalue_new_trigger (struct icaltriggertype v);
void icalvalue_set_trigger(icalvalue* value, struct icaltriggertype v);
struct icaltriggertype icalvalue_get_trigger(const icalvalue* value);

icalvalue* icalvalue_new_datetimeperiod (struct icaldatetimeperiodtype v);
void icalvalue_set_datetimeperiod(icalvalue* value, struct icaldatetimeperiodtype v);
struct icaldatetimeperiodtype icalvalue_get_datetimeperiod(const icalvalue* value);

icalvalue *icalvalue_new_attach (icalattach *attach);
void icalvalue_set_attach (icalvalue *value, icalattach *attach);
icalattach *icalvalue_get_attach (const icalvalue *value);

void icalvalue_reset_kind(icalvalue* value);

typedef enum icalvalue_kind {
   ICAL_ANY_VALUE=5000,
    ICAL_BOOLEAN_VALUE=5001,
    ICAL_UTCOFFSET_VALUE=5002,
    ICAL_XLICCLASS_VALUE=5003,
    ICAL_RECUR_VALUE=5004,
    ICAL_METHOD_VALUE=5005,
    ICAL_CALADDRESS_VALUE=5006,
    ICAL_PERIOD_VALUE=5007,
    ICAL_STATUS_VALUE=5008,
    ICAL_BINARY_VALUE=5009,
    ICAL_TEXT_VALUE=5010,
    ICAL_DURATION_VALUE=5011,
    ICAL_DATETIMEPERIOD_VALUE=5012,
    ICAL_INTEGER_VALUE=5013,
    ICAL_URI_VALUE=5014,
    ICAL_TRIGGER_VALUE=5015,
    ICAL_ATTACH_VALUE=5016,
    ICAL_CLASS_VALUE=5017,
    ICAL_FLOAT_VALUE=5018,
    ICAL_QUERY_VALUE=5019,
    ICAL_STRING_VALUE=5020,
    ICAL_TRANSP_VALUE=5021,
    ICAL_X_VALUE=5022,
    ICAL_DATETIME_VALUE=5023,
    ICAL_REQUESTSTATUS_VALUE=5024,
    ICAL_GEO_VALUE=5025,
    ICAL_DATE_VALUE=5026,
    ICAL_ACTION_VALUE=5027,
   ICAL_NO_VALUE=5028
} icalvalue_kind ;

#define ICALPROPERTY_FIRST_ENUM 10000

typedef enum icalproperty_action {
    ICAL_ACTION_X = 10000,
    ICAL_ACTION_AUDIO = 10001,
    ICAL_ACTION_DISPLAY = 10002,
    ICAL_ACTION_EMAIL = 10003,
    ICAL_ACTION_PROCEDURE = 10004,
    ICAL_ACTION_NONE = 10005
} icalproperty_action;

typedef enum icalproperty_class {
    ICAL_CLASS_X = 10006,
    ICAL_CLASS_PUBLIC = 10007,
    ICAL_CLASS_PRIVATE = 10008,
    ICAL_CLASS_CONFIDENTIAL = 10009,
    ICAL_CLASS_NONE = 10010
} icalproperty_class;

typedef enum icalproperty_method {
    ICAL_METHOD_X = 10011,
    ICAL_METHOD_PUBLISH = 10012,
    ICAL_METHOD_REQUEST = 10013,
    ICAL_METHOD_REPLY = 10014,
    ICAL_METHOD_ADD = 10015,
    ICAL_METHOD_CANCEL = 10016,
    ICAL_METHOD_REFRESH = 10017,
    ICAL_METHOD_COUNTER = 10018,
    ICAL_METHOD_DECLINECOUNTER = 10019,
    ICAL_METHOD_CREATE = 10020,
    ICAL_METHOD_READ = 10021,
    ICAL_METHOD_RESPONSE = 10022,
    ICAL_METHOD_MOVE = 10023,
    ICAL_METHOD_MODIFY = 10024,
    ICAL_METHOD_GENERATEUID = 10025,
    ICAL_METHOD_DELETE = 10026,
    ICAL_METHOD_NONE = 10027
} icalproperty_method;

typedef enum icalproperty_status {
    ICAL_STATUS_X = 10028,
    ICAL_STATUS_TENTATIVE = 10029,
    ICAL_STATUS_CONFIRMED = 10030,
    ICAL_STATUS_COMPLETED = 10031,
    ICAL_STATUS_NEEDSACTION = 10032,
    ICAL_STATUS_CANCELLED = 10033,
    ICAL_STATUS_INPROCESS = 10034,
    ICAL_STATUS_DRAFT = 10035,
    ICAL_STATUS_FINAL = 10036,
    ICAL_STATUS_NONE = 10037
} icalproperty_status;

typedef enum icalproperty_transp {
    ICAL_TRANSP_X = 10038,
    ICAL_TRANSP_OPAQUE = 10039,
    ICAL_TRANSP_OPAQUENOCONFLICT = 10040,
    ICAL_TRANSP_TRANSPARENT = 10041,
    ICAL_TRANSP_TRANSPARENTNOCONFLICT = 10042,
    ICAL_TRANSP_NONE = 10043
} icalproperty_transp;

typedef enum icalproperty_xlicclass {
    ICAL_XLICCLASS_X = 10044,
    ICAL_XLICCLASS_PUBLISHNEW = 10045,
    ICAL_XLICCLASS_PUBLISHUPDATE = 10046,
    ICAL_XLICCLASS_PUBLISHFREEBUSY = 10047,
    ICAL_XLICCLASS_REQUESTNEW = 10048,
    ICAL_XLICCLASS_REQUESTUPDATE = 10049,
    ICAL_XLICCLASS_REQUESTRESCHEDULE = 10050,
    ICAL_XLICCLASS_REQUESTDELEGATE = 10051,
    ICAL_XLICCLASS_REQUESTNEWORGANIZER = 10052,
    ICAL_XLICCLASS_REQUESTFORWARD = 10053,
    ICAL_XLICCLASS_REQUESTSTATUS = 10054,
    ICAL_XLICCLASS_REQUESTFREEBUSY = 10055,
    ICAL_XLICCLASS_REPLYACCEPT = 10056,
    ICAL_XLICCLASS_REPLYDECLINE = 10057,
    ICAL_XLICCLASS_REPLYDELEGATE = 10058,
    ICAL_XLICCLASS_REPLYCRASHERACCEPT = 10059,
    ICAL_XLICCLASS_REPLYCRASHERDECLINE = 10060,
    ICAL_XLICCLASS_ADDINSTANCE = 10061,
    ICAL_XLICCLASS_CANCELEVENT = 10062,
    ICAL_XLICCLASS_CANCELINSTANCE = 10063,
    ICAL_XLICCLASS_CANCELALL = 10064,
    ICAL_XLICCLASS_REFRESH = 10065,
    ICAL_XLICCLASS_COUNTER = 10066,
    ICAL_XLICCLASS_DECLINECOUNTER = 10067,
    ICAL_XLICCLASS_MALFORMED = 10068,
    ICAL_XLICCLASS_OBSOLETE = 10069,
    ICAL_XLICCLASS_MISSEQUENCED = 10070,
    ICAL_XLICCLASS_UNKNOWN = 10071,
    ICAL_XLICCLASS_NONE = 10072
} icalproperty_xlicclass;

#define ICALPROPERTY_LAST_ENUM 10073


 /* BOOLEAN */ 
icalvalue* icalvalue_new_boolean(int v); 
int icalvalue_get_boolean(const icalvalue* value); 
void icalvalue_set_boolean(icalvalue* value, int v);


 /* UTC-OFFSET */ 
icalvalue* icalvalue_new_utcoffset(int v); 
int icalvalue_get_utcoffset(const icalvalue* value); 
void icalvalue_set_utcoffset(icalvalue* value, int v);


 /* X-LIC-CLASS */ 
icalvalue* icalvalue_new_xlicclass(enum icalproperty_xlicclass v); 
enum icalproperty_xlicclass icalvalue_get_xlicclass(const icalvalue* value); 
void icalvalue_set_xlicclass(icalvalue* value, enum icalproperty_xlicclass v);


 /* METHOD */ 
icalvalue* icalvalue_new_method(enum icalproperty_method v); 
enum icalproperty_method icalvalue_get_method(const icalvalue* value); 
void icalvalue_set_method(icalvalue* value, enum icalproperty_method v);


 /* CAL-ADDRESS */ 
icalvalue* icalvalue_new_caladdress(const char* v); 
const char* icalvalue_get_caladdress(const icalvalue* value); 
void icalvalue_set_caladdress(icalvalue* value, const char* v);


 /* PERIOD */ 
icalvalue* icalvalue_new_period(struct icalperiodtype v); 
struct icalperiodtype icalvalue_get_period(const icalvalue* value); 
void icalvalue_set_period(icalvalue* value, struct icalperiodtype v);


 /* STATUS */ 
icalvalue* icalvalue_new_status(enum icalproperty_status v); 
enum icalproperty_status icalvalue_get_status(const icalvalue* value); 
void icalvalue_set_status(icalvalue* value, enum icalproperty_status v);


 /* BINARY */ 
icalvalue* icalvalue_new_binary(const char* v); 
const char* icalvalue_get_binary(const icalvalue* value); 
void icalvalue_set_binary(icalvalue* value, const char* v);


 /* TEXT */ 
icalvalue* icalvalue_new_text(const char* v); 
const char* icalvalue_get_text(const icalvalue* value); 
void icalvalue_set_text(icalvalue* value, const char* v);


 /* DURATION */ 
icalvalue* icalvalue_new_duration(struct icaldurationtype v); 
struct icaldurationtype icalvalue_get_duration(const icalvalue* value); 
void icalvalue_set_duration(icalvalue* value, struct icaldurationtype v);


 /* INTEGER */ 
icalvalue* icalvalue_new_integer(int v); 
int icalvalue_get_integer(const icalvalue* value); 
void icalvalue_set_integer(icalvalue* value, int v);


 /* URI */ 
icalvalue* icalvalue_new_uri(const char* v); 
const char* icalvalue_get_uri(const icalvalue* value); 
void icalvalue_set_uri(icalvalue* value, const char* v);


 /* CLASS */ 
icalvalue* icalvalue_new_class(enum icalproperty_class v); 
enum icalproperty_class icalvalue_get_class(const icalvalue* value); 
void icalvalue_set_class(icalvalue* value, enum icalproperty_class v);


 /* FLOAT */ 
icalvalue* icalvalue_new_float(float v); 
float icalvalue_get_float(const icalvalue* value); 
void icalvalue_set_float(icalvalue* value, float v);


 /* QUERY */ 
icalvalue* icalvalue_new_query(const char* v); 
const char* icalvalue_get_query(const icalvalue* value); 
void icalvalue_set_query(icalvalue* value, const char* v);


 /* STRING */ 
icalvalue* icalvalue_new_string(const char* v); 
const char* icalvalue_get_string(const icalvalue* value); 
void icalvalue_set_string(icalvalue* value, const char* v);


 /* TRANSP */ 
icalvalue* icalvalue_new_transp(enum icalproperty_transp v); 
enum icalproperty_transp icalvalue_get_transp(const icalvalue* value); 
void icalvalue_set_transp(icalvalue* value, enum icalproperty_transp v);


 /* DATE-TIME */ 
icalvalue* icalvalue_new_datetime(struct icaltimetype v); 
struct icaltimetype icalvalue_get_datetime(const icalvalue* value); 
void icalvalue_set_datetime(icalvalue* value, struct icaltimetype v);


 /* REQUEST-STATUS */ 
icalvalue* icalvalue_new_requeststatus(struct icalreqstattype v); 
struct icalreqstattype icalvalue_get_requeststatus(const icalvalue* value); 
void icalvalue_set_requeststatus(icalvalue* value, struct icalreqstattype v);


 /* GEO */ 
icalvalue* icalvalue_new_geo(struct icalgeotype v); 
struct icalgeotype icalvalue_get_geo(const icalvalue* value); 
void icalvalue_set_geo(icalvalue* value, struct icalgeotype v);


 /* DATE */ 
icalvalue* icalvalue_new_date(struct icaltimetype v); 
struct icaltimetype icalvalue_get_date(const icalvalue* value); 
void icalvalue_set_date(icalvalue* value, struct icaltimetype v);


 /* ACTION */ 
icalvalue* icalvalue_new_action(enum icalproperty_action v); 
enum icalproperty_action icalvalue_get_action(const icalvalue* value); 
void icalvalue_set_action(icalvalue* value, enum icalproperty_action v);

#endif /*ICALVALUE_H*/
/* -*- Mode: C -*- */
/*======================================================================
  FILE: icalparam.h
  CREATOR: eric 20 March 1999


  $Id: ical.h,v 1.6 2002/10/10 00:41:34 acampi Exp $
  $Locker:  $

  

 (C) COPYRIGHT 2000, Eric Busboom, http://www.softwarestudio.org

 This program is free software; you can redistribute it and/or modify
 it under the terms of either: 

    The LGPL as published by the Free Software Foundation, version
    2.1, available at: http://www.fsf.org/copyleft/lesser.html

  Or:

    The Mozilla Public License Version 1.0. You may obtain a copy of
    the License at http://www.mozilla.org/MPL/

  The original code is icalparam.h

  ======================================================================*/

#ifndef ICALDERIVEDPARAMETER_H
#define ICALDERIVEDPARAMETER_H


typedef struct icalparameter_impl icalparameter;

const char* icalparameter_enum_to_string(int e);
int icalparameter_string_to_enum(const char* str); 

typedef enum icalparameter_kind {
    ICAL_ANY_PARAMETER = 0,
    ICAL_ALTREP_PARAMETER, 
    ICAL_CN_PARAMETER, 
    ICAL_CUTYPE_PARAMETER, 
    ICAL_DELEGATEDFROM_PARAMETER, 
    ICAL_DELEGATEDTO_PARAMETER, 
    ICAL_DIR_PARAMETER, 
    ICAL_ENCODING_PARAMETER, 
    ICAL_FBTYPE_PARAMETER, 
    ICAL_FMTTYPE_PARAMETER, 
    ICAL_LANGUAGE_PARAMETER, 
    ICAL_MEMBER_PARAMETER, 
    ICAL_PARTSTAT_PARAMETER, 
    ICAL_RANGE_PARAMETER, 
    ICAL_RELATED_PARAMETER, 
    ICAL_RELTYPE_PARAMETER, 
    ICAL_ROLE_PARAMETER, 
    ICAL_RSVP_PARAMETER, 
    ICAL_SENTBY_PARAMETER, 
    ICAL_TZID_PARAMETER, 
    ICAL_VALUE_PARAMETER, 
    ICAL_X_PARAMETER, 
    ICAL_XLICCOMPARETYPE_PARAMETER, 
    ICAL_XLICERRORTYPE_PARAMETER, 
    ICAL_NO_PARAMETER
} icalparameter_kind;

#define ICALPARAMETER_FIRST_ENUM 20000

typedef enum icalparameter_cutype {
    ICAL_CUTYPE_X = 20000,
    ICAL_CUTYPE_INDIVIDUAL = 20001,
    ICAL_CUTYPE_GROUP = 20002,
    ICAL_CUTYPE_RESOURCE = 20003,
    ICAL_CUTYPE_ROOM = 20004,
    ICAL_CUTYPE_UNKNOWN = 20005,
    ICAL_CUTYPE_NONE = 20006
} icalparameter_cutype;

typedef enum icalparameter_encoding {
    ICAL_ENCODING_X = 20007,
    ICAL_ENCODING_8BIT = 20008,
    ICAL_ENCODING_BASE64 = 20009,
    ICAL_ENCODING_NONE = 20010
} icalparameter_encoding;

typedef enum icalparameter_fbtype {
    ICAL_FBTYPE_X = 20011,
    ICAL_FBTYPE_FREE = 20012,
    ICAL_FBTYPE_BUSY = 20013,
    ICAL_FBTYPE_BUSYUNAVAILABLE = 20014,
    ICAL_FBTYPE_BUSYTENTATIVE = 20015,
    ICAL_FBTYPE_NONE = 20016
} icalparameter_fbtype;

typedef enum icalparameter_partstat {
    ICAL_PARTSTAT_X = 20017,
    ICAL_PARTSTAT_NEEDSACTION = 20018,
    ICAL_PARTSTAT_ACCEPTED = 20019,
    ICAL_PARTSTAT_DECLINED = 20020,
    ICAL_PARTSTAT_TENTATIVE = 20021,
    ICAL_PARTSTAT_DELEGATED = 20022,
    ICAL_PARTSTAT_COMPLETED = 20023,
    ICAL_PARTSTAT_INPROCESS = 20024,
    ICAL_PARTSTAT_NONE = 20025
} icalparameter_partstat;

typedef enum icalparameter_range {
    ICAL_RANGE_X = 20026,
    ICAL_RANGE_THISANDPRIOR = 20027,
    ICAL_RANGE_THISANDFUTURE = 20028,
    ICAL_RANGE_NONE = 20029
} icalparameter_range;

typedef enum icalparameter_related {
    ICAL_RELATED_X = 20030,
    ICAL_RELATED_START = 20031,
    ICAL_RELATED_END = 20032,
    ICAL_RELATED_NONE = 20033
} icalparameter_related;

typedef enum icalparameter_reltype {
    ICAL_RELTYPE_X = 20034,
    ICAL_RELTYPE_PARENT = 20035,
    ICAL_RELTYPE_CHILD = 20036,
    ICAL_RELTYPE_SIBLING = 20037,
    ICAL_RELTYPE_NONE = 20038
} icalparameter_reltype;

typedef enum icalparameter_role {
    ICAL_ROLE_X = 20039,
    ICAL_ROLE_CHAIR = 20040,
    ICAL_ROLE_REQPARTICIPANT = 20041,
    ICAL_ROLE_OPTPARTICIPANT = 20042,
    ICAL_ROLE_NONPARTICIPANT = 20043,
    ICAL_ROLE_NONE = 20044
} icalparameter_role;

typedef enum icalparameter_rsvp {
    ICAL_RSVP_X = 20045,
    ICAL_RSVP_TRUE = 20046,
    ICAL_RSVP_FALSE = 20047,
    ICAL_RSVP_NONE = 20048
} icalparameter_rsvp;

typedef enum icalparameter_value {
    ICAL_VALUE_X = 20049,
    ICAL_VALUE_BINARY = 20050,
    ICAL_VALUE_BOOLEAN = 20051,
    ICAL_VALUE_DATE = 20052,
    ICAL_VALUE_DURATION = 20053,
    ICAL_VALUE_FLOAT = 20054,
    ICAL_VALUE_INTEGER = 20055,
    ICAL_VALUE_PERIOD = 20056,
    ICAL_VALUE_RECUR = 20057,
    ICAL_VALUE_TEXT = 20058,
    ICAL_VALUE_URI = 20059,
    ICAL_VALUE_ERROR = 20060,
    ICAL_VALUE_DATETIME = 20061,
    ICAL_VALUE_UTCOFFSET = 20062,
    ICAL_VALUE_CALADDRESS = 20063,
    ICAL_VALUE_NONE = 20064
} icalparameter_value;

typedef enum icalparameter_xliccomparetype {
    ICAL_XLICCOMPARETYPE_X = 20065,
    ICAL_XLICCOMPARETYPE_EQUAL = 20066,
    ICAL_XLICCOMPARETYPE_NOTEQUAL = 20067,
    ICAL_XLICCOMPARETYPE_LESS = 20068,
    ICAL_XLICCOMPARETYPE_GREATER = 20069,
    ICAL_XLICCOMPARETYPE_LESSEQUAL = 20070,
    ICAL_XLICCOMPARETYPE_GREATEREQUAL = 20071,
    ICAL_XLICCOMPARETYPE_REGEX = 20072,
    ICAL_XLICCOMPARETYPE_ISNULL = 20073,
    ICAL_XLICCOMPARETYPE_ISNOTNULL = 20074,
    ICAL_XLICCOMPARETYPE_NONE = 20075
} icalparameter_xliccomparetype;

typedef enum icalparameter_xlicerrortype {
    ICAL_XLICERRORTYPE_X = 20076,
    ICAL_XLICERRORTYPE_COMPONENTPARSEERROR = 20077,
    ICAL_XLICERRORTYPE_PROPERTYPARSEERROR = 20078,
    ICAL_XLICERRORTYPE_PARAMETERNAMEPARSEERROR = 20079,
    ICAL_XLICERRORTYPE_PARAMETERVALUEPARSEERROR = 20080,
    ICAL_XLICERRORTYPE_VALUEPARSEERROR = 20081,
    ICAL_XLICERRORTYPE_INVALIDITIP = 20082,
    ICAL_XLICERRORTYPE_UNKNOWNVCALPROPERROR = 20083,
    ICAL_XLICERRORTYPE_MIMEPARSEERROR = 20084,
    ICAL_XLICERRORTYPE_VCALPROPPARSEERROR = 20085,
    ICAL_XLICERRORTYPE_NONE = 20086
} icalparameter_xlicerrortype;

#define ICALPARAMETER_LAST_ENUM 20087

/* DELEGATED-FROM */
icalparameter* icalparameter_new_delegatedfrom(const char* v);
const char* icalparameter_get_delegatedfrom(const icalparameter* value);
void icalparameter_set_delegatedfrom(icalparameter* value, const char* v);

/* RELATED */
icalparameter* icalparameter_new_related(icalparameter_related v);
icalparameter_related icalparameter_get_related(const icalparameter* value);
void icalparameter_set_related(icalparameter* value, icalparameter_related v);

/* SENT-BY */
icalparameter* icalparameter_new_sentby(const char* v);
const char* icalparameter_get_sentby(const icalparameter* value);
void icalparameter_set_sentby(icalparameter* value, const char* v);

/* LANGUAGE */
icalparameter* icalparameter_new_language(const char* v);
const char* icalparameter_get_language(const icalparameter* value);
void icalparameter_set_language(icalparameter* value, const char* v);

/* RELTYPE */
icalparameter* icalparameter_new_reltype(icalparameter_reltype v);
icalparameter_reltype icalparameter_get_reltype(const icalparameter* value);
void icalparameter_set_reltype(icalparameter* value, icalparameter_reltype v);

/* ENCODING */
icalparameter* icalparameter_new_encoding(icalparameter_encoding v);
icalparameter_encoding icalparameter_get_encoding(const icalparameter* value);
void icalparameter_set_encoding(icalparameter* value, icalparameter_encoding v);

/* ALTREP */
icalparameter* icalparameter_new_altrep(const char* v);
const char* icalparameter_get_altrep(const icalparameter* value);
void icalparameter_set_altrep(icalparameter* value, const char* v);

/* FMTTYPE */
icalparameter* icalparameter_new_fmttype(const char* v);
const char* icalparameter_get_fmttype(const icalparameter* value);
void icalparameter_set_fmttype(icalparameter* value, const char* v);

/* FBTYPE */
icalparameter* icalparameter_new_fbtype(icalparameter_fbtype v);
icalparameter_fbtype icalparameter_get_fbtype(const icalparameter* value);
void icalparameter_set_fbtype(icalparameter* value, icalparameter_fbtype v);

/* RSVP */
icalparameter* icalparameter_new_rsvp(icalparameter_rsvp v);
icalparameter_rsvp icalparameter_get_rsvp(const icalparameter* value);
void icalparameter_set_rsvp(icalparameter* value, icalparameter_rsvp v);

/* RANGE */
icalparameter* icalparameter_new_range(icalparameter_range v);
icalparameter_range icalparameter_get_range(const icalparameter* value);
void icalparameter_set_range(icalparameter* value, icalparameter_range v);

/* DELEGATED-TO */
icalparameter* icalparameter_new_delegatedto(const char* v);
const char* icalparameter_get_delegatedto(const icalparameter* value);
void icalparameter_set_delegatedto(icalparameter* value, const char* v);

/* CN */
icalparameter* icalparameter_new_cn(const char* v);
const char* icalparameter_get_cn(const icalparameter* value);
void icalparameter_set_cn(icalparameter* value, const char* v);

/* ROLE */
icalparameter* icalparameter_new_role(icalparameter_role v);
icalparameter_role icalparameter_get_role(const icalparameter* value);
void icalparameter_set_role(icalparameter* value, icalparameter_role v);

/* X-LIC-COMPARETYPE */
icalparameter* icalparameter_new_xliccomparetype(icalparameter_xliccomparetype v);
icalparameter_xliccomparetype icalparameter_get_xliccomparetype(const icalparameter* value);
void icalparameter_set_xliccomparetype(icalparameter* value, icalparameter_xliccomparetype v);

/* PARTSTAT */
icalparameter* icalparameter_new_partstat(icalparameter_partstat v);
icalparameter_partstat icalparameter_get_partstat(const icalparameter* value);
void icalparameter_set_partstat(icalparameter* value, icalparameter_partstat v);

/* X-LIC-ERRORTYPE */
icalparameter* icalparameter_new_xlicerrortype(icalparameter_xlicerrortype v);
icalparameter_xlicerrortype icalparameter_get_xlicerrortype(const icalparameter* value);
void icalparameter_set_xlicerrortype(icalparameter* value, icalparameter_xlicerrortype v);

/* MEMBER */
icalparameter* icalparameter_new_member(const char* v);
const char* icalparameter_get_member(const icalparameter* value);
void icalparameter_set_member(icalparameter* value, const char* v);

/* X */
icalparameter* icalparameter_new_x(const char* v);
const char* icalparameter_get_x(const icalparameter* value);
void icalparameter_set_x(icalparameter* value, const char* v);

/* CUTYPE */
icalparameter* icalparameter_new_cutype(icalparameter_cutype v);
icalparameter_cutype icalparameter_get_cutype(const icalparameter* value);
void icalparameter_set_cutype(icalparameter* value, icalparameter_cutype v);

/* TZID */
icalparameter* icalparameter_new_tzid(const char* v);
const char* icalparameter_get_tzid(const icalparameter* value);
void icalparameter_set_tzid(icalparameter* value, const char* v);

/* VALUE */
icalparameter* icalparameter_new_value(icalparameter_value v);
icalparameter_value icalparameter_get_value(const icalparameter* value);
void icalparameter_set_value(icalparameter* value, icalparameter_value v);

/* DIR */
icalparameter* icalparameter_new_dir(const char* v);
const char* icalparameter_get_dir(const icalparameter* value);
void icalparameter_set_dir(icalparameter* value, const char* v);

#endif /*ICALPARAMETER_H*/

/* Everything below this line is machine generated. Do not edit. */
/* -*- Mode: C -*- */
/*======================================================================
  FILE: icalvalue.h
  CREATOR: eric 20 March 1999


  $Id: ical.h,v 1.6 2002/10/10 00:41:34 acampi Exp $
  $Locker:  $

 (C) COPYRIGHT 2000, Eric Busboom, http://www.softwarestudio.org

 This program is free software; you can redistribute it and/or modify
 it under the terms of either: 

    The LGPL as published by the Free Software Foundation, version
    2.1, available at: http://www.fsf.org/copyleft/lesser.html

  Or:

    The Mozilla Public License Version 1.0. You may obtain a copy of
    the License at http://www.mozilla.org/MPL/

  The original code is icalvalue.h

  ======================================================================*/

#ifndef ICALVALUE_H
#define ICALVALUE_H

#include <time.h>
                          
/* Defined in icalderivedvalue.h */
/*typedef struct icalvalue_impl icalvalue;*/

icalvalue* icalvalue_new(icalvalue_kind kind);

icalvalue* icalvalue_new_clone(const icalvalue* value);

icalvalue* icalvalue_new_from_string(icalvalue_kind kind, const char* str);

void icalvalue_free(icalvalue* value);

int icalvalue_is_valid(const icalvalue* value);

const char* icalvalue_as_ical_string(const icalvalue* value);

icalvalue_kind icalvalue_isa(const icalvalue* value);

int icalvalue_isa_value(void*);

icalparameter_xliccomparetype icalvalue_compare(const icalvalue* a, const icalvalue *b);


/* Special, non autogenerated value accessors */

icalvalue* icalvalue_new_recur (struct icalrecurrencetype v);
void icalvalue_set_recur(icalvalue* value, struct icalrecurrencetype v);
struct icalrecurrencetype icalvalue_get_recur(const icalvalue* value);

icalvalue* icalvalue_new_trigger (struct icaltriggertype v);
void icalvalue_set_trigger(icalvalue* value, struct icaltriggertype v);
struct icaltriggertype icalvalue_get_trigger(const icalvalue* value);

icalvalue* icalvalue_new_datetimeperiod (struct icaldatetimeperiodtype v);
void icalvalue_set_datetimeperiod(icalvalue* value, 
				  struct icaldatetimeperiodtype v);
struct icaldatetimeperiodtype icalvalue_get_datetimeperiod(const icalvalue* value);

/* Convert enumerations */

icalvalue_kind icalvalue_string_to_kind(const char* str);
const char* icalvalue_kind_to_string(const icalvalue_kind kind);

/** Check validity of a specific icalvalue_kind **/
int icalvalue_kind_is_valid(const icalvalue_kind kind);

/** Encode a character string in ical format, esacpe certain characters, etc. */
int icalvalue_encode_ical_string(const char *szText, char *szEncText, int MaxBufferLen);

#endif /*ICALVALUE_H*/
/* -*- Mode: C -*- */
/*======================================================================
  FILE: icalparam.h
  CREATOR: eric 20 March 1999


  $Id: ical.h,v 1.6 2002/10/10 00:41:34 acampi Exp $
  $Locker:  $

  

 (C) COPYRIGHT 2000, Eric Busboom, http://www.softwarestudio.org

 This program is free software; you can redistribute it and/or modify
 it under the terms of either: 

    The LGPL as published by the Free Software Foundation, version
    2.1, available at: http://www.fsf.org/copyleft/lesser.html

  Or:

    The Mozilla Public License Version 1.0. You may obtain a copy of
    the License at http://www.mozilla.org/MPL/

  The original code is icalparam.h

  ======================================================================*/

#ifndef ICALPARAM_H
#define ICALPARAM_H


/* Declared in icalderivedparameter.h */
/*typedef struct icalparameter_impl icalparameter;*/

icalparameter* icalparameter_new(icalparameter_kind kind);
icalparameter* icalparameter_new_clone(icalparameter* p);

/* Create from string of form "PARAMNAME=VALUE" */
icalparameter* icalparameter_new_from_string(const char* value);

/* Create from just the value, the part after the "=" */
icalparameter* icalparameter_new_from_value_string(icalparameter_kind kind, const char* value);

void icalparameter_free(icalparameter* parameter);

char* icalparameter_as_ical_string(icalparameter* parameter);

int icalparameter_is_valid(icalparameter* parameter);

icalparameter_kind icalparameter_isa(icalparameter* parameter);

int icalparameter_isa_parameter(void* param);

/* Acess the name of an X parameer */
void icalparameter_set_xname (icalparameter* param, const char* v);
const char* icalparameter_get_xname(icalparameter* param);
void icalparameter_set_xvalue (icalparameter* param, const char* v);
const char* icalparameter_get_xvalue(icalparameter* param);

/* Convert enumerations */

const char* icalparameter_kind_to_string(icalparameter_kind kind);
icalparameter_kind icalparameter_string_to_kind(const char* string);



#endif 
/* -*- Mode: C -*-
  ======================================================================
  FILE: icalderivedproperties.{c,h}
  CREATOR: eric 09 May 1999
  
  $Id: ical.h,v 1.6 2002/10/10 00:41:34 acampi Exp $
    
 (C) COPYRIGHT 2000, Eric Busboom, http://www.softwarestudio.org
 ======================================================================*/


#ifndef ICALDERIVEDPROPERTY_H
#define ICALDERIVEDPROPERTY_H

#include <time.h>

typedef struct icalproperty_impl icalproperty;

typedef enum icalproperty_kind {
    ICAL_ANY_PROPERTY = 0,
    ICAL_ACTION_PROPERTY, 
    ICAL_ALLOWCONFLICT_PROPERTY, 
    ICAL_ATTACH_PROPERTY, 
    ICAL_ATTENDEE_PROPERTY, 
    ICAL_CALID_PROPERTY, 
    ICAL_CALMASTER_PROPERTY, 
    ICAL_CALSCALE_PROPERTY, 
    ICAL_CARID_PROPERTY, 
    ICAL_CATEGORIES_PROPERTY, 
    ICAL_CLASS_PROPERTY, 
    ICAL_COMMENT_PROPERTY, 
    ICAL_COMPLETED_PROPERTY, 
    ICAL_CONTACT_PROPERTY, 
    ICAL_CREATED_PROPERTY, 
    ICAL_DECREED_PROPERTY, 
    ICAL_DEFAULTCHARSET_PROPERTY, 
    ICAL_DEFAULTLOCALE_PROPERTY, 
    ICAL_DEFAULTTZID_PROPERTY, 
    ICAL_DESCRIPTION_PROPERTY, 
    ICAL_DTEND_PROPERTY, 
    ICAL_DTSTAMP_PROPERTY, 
    ICAL_DTSTART_PROPERTY, 
    ICAL_DUE_PROPERTY, 
    ICAL_DURATION_PROPERTY, 
    ICAL_EXDATE_PROPERTY, 
    ICAL_EXPAND_PROPERTY, 
    ICAL_EXRULE_PROPERTY, 
    ICAL_FREEBUSY_PROPERTY, 
    ICAL_GEO_PROPERTY, 
    ICAL_LASTMODIFIED_PROPERTY, 
    ICAL_LOCATION_PROPERTY, 
    ICAL_MAXRESULTS_PROPERTY, 
    ICAL_MAXRESULTSSIZE_PROPERTY, 
    ICAL_METHOD_PROPERTY, 
    ICAL_ORGANIZER_PROPERTY, 
    ICAL_OWNER_PROPERTY, 
    ICAL_PERCENTCOMPLETE_PROPERTY, 
    ICAL_PRIORITY_PROPERTY, 
    ICAL_PRODID_PROPERTY, 
    ICAL_QUERY_PROPERTY, 
    ICAL_QUERYNAME_PROPERTY, 
    ICAL_RDATE_PROPERTY, 
    ICAL_RECURRENCEID_PROPERTY, 
    ICAL_RELATEDTO_PROPERTY, 
    ICAL_RELCALID_PROPERTY, 
    ICAL_REPEAT_PROPERTY, 
    ICAL_REQUESTSTATUS_PROPERTY, 
    ICAL_RESOURCES_PROPERTY, 
    ICAL_RRULE_PROPERTY, 
    ICAL_SCOPE_PROPERTY, 
    ICAL_SEQUENCE_PROPERTY, 
    ICAL_STATUS_PROPERTY, 
    ICAL_SUMMARY_PROPERTY, 
    ICAL_TARGET_PROPERTY, 
    ICAL_TRANSP_PROPERTY, 
    ICAL_TRIGGER_PROPERTY, 
    ICAL_TZID_PROPERTY, 
    ICAL_TZNAME_PROPERTY, 
    ICAL_TZOFFSETFROM_PROPERTY, 
    ICAL_TZOFFSETTO_PROPERTY, 
    ICAL_TZURL_PROPERTY, 
    ICAL_UID_PROPERTY, 
    ICAL_URL_PROPERTY, 
    ICAL_VERSION_PROPERTY, 
    ICAL_X_PROPERTY, 
    ICAL_XLICCLASS_PROPERTY, 
    ICAL_XLICCLUSTERCOUNT_PROPERTY, 
    ICAL_XLICERROR_PROPERTY, 
    ICAL_XLICMIMECHARSET_PROPERTY, 
    ICAL_XLICMIMECID_PROPERTY, 
    ICAL_XLICMIMECONTENTTYPE_PROPERTY, 
    ICAL_XLICMIMEENCODING_PROPERTY, 
    ICAL_XLICMIMEFILENAME_PROPERTY, 
    ICAL_XLICMIMEOPTINFO_PROPERTY, 
    ICAL_NO_PROPERTY
} icalproperty_kind;


/* ACTION */
icalproperty* icalproperty_new_action(enum icalproperty_action v);
void icalproperty_set_action(icalproperty* prop, enum icalproperty_action v);
enum icalproperty_action icalproperty_get_action(const icalproperty* prop);icalproperty* icalproperty_vanew_action(enum icalproperty_action v, ...);

/* ALLOW-CONFLICT */
icalproperty* icalproperty_new_allowconflict(const char* v);
void icalproperty_set_allowconflict(icalproperty* prop, const char* v);
const char* icalproperty_get_allowconflict(const icalproperty* prop);icalproperty* icalproperty_vanew_allowconflict(const char* v, ...);

/* ATTACH */
icalproperty* icalproperty_new_attach(icalattach * v);
void icalproperty_set_attach(icalproperty* prop, icalattach * v);
icalattach * icalproperty_get_attach(const icalproperty* prop);icalproperty* icalproperty_vanew_attach(icalattach * v, ...);

/* ATTENDEE */
icalproperty* icalproperty_new_attendee(const char* v);
void icalproperty_set_attendee(icalproperty* prop, const char* v);
const char* icalproperty_get_attendee(const icalproperty* prop);icalproperty* icalproperty_vanew_attendee(const char* v, ...);

/* CALID */
icalproperty* icalproperty_new_calid(const char* v);
void icalproperty_set_calid(icalproperty* prop, const char* v);
const char* icalproperty_get_calid(const icalproperty* prop);icalproperty* icalproperty_vanew_calid(const char* v, ...);

/* CALMASTER */
icalproperty* icalproperty_new_calmaster(const char* v);
void icalproperty_set_calmaster(icalproperty* prop, const char* v);
const char* icalproperty_get_calmaster(const icalproperty* prop);icalproperty* icalproperty_vanew_calmaster(const char* v, ...);

/* CALSCALE */
icalproperty* icalproperty_new_calscale(const char* v);
void icalproperty_set_calscale(icalproperty* prop, const char* v);
const char* icalproperty_get_calscale(const icalproperty* prop);icalproperty* icalproperty_vanew_calscale(const char* v, ...);

/* CARID */
icalproperty* icalproperty_new_carid(const char* v);
void icalproperty_set_carid(icalproperty* prop, const char* v);
const char* icalproperty_get_carid(const icalproperty* prop);icalproperty* icalproperty_vanew_carid(const char* v, ...);

/* CATEGORIES */
icalproperty* icalproperty_new_categories(const char* v);
void icalproperty_set_categories(icalproperty* prop, const char* v);
const char* icalproperty_get_categories(const icalproperty* prop);icalproperty* icalproperty_vanew_categories(const char* v, ...);

/* CLASS */
icalproperty* icalproperty_new_class(enum icalproperty_class v);
void icalproperty_set_class(icalproperty* prop, enum icalproperty_class v);
enum icalproperty_class icalproperty_get_class(const icalproperty* prop);icalproperty* icalproperty_vanew_class(enum icalproperty_class v, ...);

/* COMMENT */
icalproperty* icalproperty_new_comment(const char* v);
void icalproperty_set_comment(icalproperty* prop, const char* v);
const char* icalproperty_get_comment(const icalproperty* prop);icalproperty* icalproperty_vanew_comment(const char* v, ...);

/* COMPLETED */
icalproperty* icalproperty_new_completed(struct icaltimetype v);
void icalproperty_set_completed(icalproperty* prop, struct icaltimetype v);
struct icaltimetype icalproperty_get_completed(const icalproperty* prop);icalproperty* icalproperty_vanew_completed(struct icaltimetype v, ...);

/* CONTACT */
icalproperty* icalproperty_new_contact(const char* v);
void icalproperty_set_contact(icalproperty* prop, const char* v);
const char* icalproperty_get_contact(const icalproperty* prop);icalproperty* icalproperty_vanew_contact(const char* v, ...);

/* CREATED */
icalproperty* icalproperty_new_created(struct icaltimetype v);
void icalproperty_set_created(icalproperty* prop, struct icaltimetype v);
struct icaltimetype icalproperty_get_created(const icalproperty* prop);icalproperty* icalproperty_vanew_created(struct icaltimetype v, ...);

/* DECREED */
icalproperty* icalproperty_new_decreed(const char* v);
void icalproperty_set_decreed(icalproperty* prop, const char* v);
const char* icalproperty_get_decreed(const icalproperty* prop);icalproperty* icalproperty_vanew_decreed(const char* v, ...);

/* DEFAULT-CHARSET */
icalproperty* icalproperty_new_defaultcharset(const char* v);
void icalproperty_set_defaultcharset(icalproperty* prop, const char* v);
const char* icalproperty_get_defaultcharset(const icalproperty* prop);icalproperty* icalproperty_vanew_defaultcharset(const char* v, ...);

/* DEFAULT-LOCALE */
icalproperty* icalproperty_new_defaultlocale(const char* v);
void icalproperty_set_defaultlocale(icalproperty* prop, const char* v);
const char* icalproperty_get_defaultlocale(const icalproperty* prop);icalproperty* icalproperty_vanew_defaultlocale(const char* v, ...);

/* DEFAULT-TZID */
icalproperty* icalproperty_new_defaulttzid(const char* v);
void icalproperty_set_defaulttzid(icalproperty* prop, const char* v);
const char* icalproperty_get_defaulttzid(const icalproperty* prop);icalproperty* icalproperty_vanew_defaulttzid(const char* v, ...);

/* DESCRIPTION */
icalproperty* icalproperty_new_description(const char* v);
void icalproperty_set_description(icalproperty* prop, const char* v);
const char* icalproperty_get_description(const icalproperty* prop);icalproperty* icalproperty_vanew_description(const char* v, ...);

/* DTEND */
icalproperty* icalproperty_new_dtend(struct icaltimetype v);
void icalproperty_set_dtend(icalproperty* prop, struct icaltimetype v);
struct icaltimetype icalproperty_get_dtend(const icalproperty* prop);icalproperty* icalproperty_vanew_dtend(struct icaltimetype v, ...);

/* DTSTAMP */
icalproperty* icalproperty_new_dtstamp(struct icaltimetype v);
void icalproperty_set_dtstamp(icalproperty* prop, struct icaltimetype v);
struct icaltimetype icalproperty_get_dtstamp(const icalproperty* prop);icalproperty* icalproperty_vanew_dtstamp(struct icaltimetype v, ...);

/* DTSTART */
icalproperty* icalproperty_new_dtstart(struct icaltimetype v);
void icalproperty_set_dtstart(icalproperty* prop, struct icaltimetype v);
struct icaltimetype icalproperty_get_dtstart(const icalproperty* prop);icalproperty* icalproperty_vanew_dtstart(struct icaltimetype v, ...);

/* DUE */
icalproperty* icalproperty_new_due(struct icaltimetype v);
void icalproperty_set_due(icalproperty* prop, struct icaltimetype v);
struct icaltimetype icalproperty_get_due(const icalproperty* prop);icalproperty* icalproperty_vanew_due(struct icaltimetype v, ...);

/* DURATION */
icalproperty* icalproperty_new_duration(struct icaldurationtype v);
void icalproperty_set_duration(icalproperty* prop, struct icaldurationtype v);
struct icaldurationtype icalproperty_get_duration(const icalproperty* prop);icalproperty* icalproperty_vanew_duration(struct icaldurationtype v, ...);

/* EXDATE */
icalproperty* icalproperty_new_exdate(struct icaltimetype v);
void icalproperty_set_exdate(icalproperty* prop, struct icaltimetype v);
struct icaltimetype icalproperty_get_exdate(const icalproperty* prop);icalproperty* icalproperty_vanew_exdate(struct icaltimetype v, ...);

/* EXPAND */
icalproperty* icalproperty_new_expand(int v);
void icalproperty_set_expand(icalproperty* prop, int v);
int icalproperty_get_expand(const icalproperty* prop);icalproperty* icalproperty_vanew_expand(int v, ...);

/* EXRULE */
icalproperty* icalproperty_new_exrule(struct icalrecurrencetype v);
void icalproperty_set_exrule(icalproperty* prop, struct icalrecurrencetype v);
struct icalrecurrencetype icalproperty_get_exrule(const icalproperty* prop);icalproperty* icalproperty_vanew_exrule(struct icalrecurrencetype v, ...);

/* FREEBUSY */
icalproperty* icalproperty_new_freebusy(struct icalperiodtype v);
void icalproperty_set_freebusy(icalproperty* prop, struct icalperiodtype v);
struct icalperiodtype icalproperty_get_freebusy(const icalproperty* prop);icalproperty* icalproperty_vanew_freebusy(struct icalperiodtype v, ...);

/* GEO */
icalproperty* icalproperty_new_geo(struct icalgeotype v);
void icalproperty_set_geo(icalproperty* prop, struct icalgeotype v);
struct icalgeotype icalproperty_get_geo(const icalproperty* prop);icalproperty* icalproperty_vanew_geo(struct icalgeotype v, ...);

/* LAST-MODIFIED */
icalproperty* icalproperty_new_lastmodified(struct icaltimetype v);
void icalproperty_set_lastmodified(icalproperty* prop, struct icaltimetype v);
struct icaltimetype icalproperty_get_lastmodified(const icalproperty* prop);icalproperty* icalproperty_vanew_lastmodified(struct icaltimetype v, ...);

/* LOCATION */
icalproperty* icalproperty_new_location(const char* v);
void icalproperty_set_location(icalproperty* prop, const char* v);
const char* icalproperty_get_location(const icalproperty* prop);icalproperty* icalproperty_vanew_location(const char* v, ...);

/* MAXRESULTS */
icalproperty* icalproperty_new_maxresults(int v);
void icalproperty_set_maxresults(icalproperty* prop, int v);
int icalproperty_get_maxresults(const icalproperty* prop);icalproperty* icalproperty_vanew_maxresults(int v, ...);

/* MAXRESULTSSIZE */
icalproperty* icalproperty_new_maxresultssize(int v);
void icalproperty_set_maxresultssize(icalproperty* prop, int v);
int icalproperty_get_maxresultssize(const icalproperty* prop);icalproperty* icalproperty_vanew_maxresultssize(int v, ...);

/* METHOD */
icalproperty* icalproperty_new_method(enum icalproperty_method v);
void icalproperty_set_method(icalproperty* prop, enum icalproperty_method v);
enum icalproperty_method icalproperty_get_method(const icalproperty* prop);icalproperty* icalproperty_vanew_method(enum icalproperty_method v, ...);

/* ORGANIZER */
icalproperty* icalproperty_new_organizer(const char* v);
void icalproperty_set_organizer(icalproperty* prop, const char* v);
const char* icalproperty_get_organizer(const icalproperty* prop);icalproperty* icalproperty_vanew_organizer(const char* v, ...);

/* OWNER */
icalproperty* icalproperty_new_owner(const char* v);
void icalproperty_set_owner(icalproperty* prop, const char* v);
const char* icalproperty_get_owner(const icalproperty* prop);icalproperty* icalproperty_vanew_owner(const char* v, ...);

/* PERCENT-COMPLETE */
icalproperty* icalproperty_new_percentcomplete(int v);
void icalproperty_set_percentcomplete(icalproperty* prop, int v);
int icalproperty_get_percentcomplete(const icalproperty* prop);icalproperty* icalproperty_vanew_percentcomplete(int v, ...);

/* PRIORITY */
icalproperty* icalproperty_new_priority(int v);
void icalproperty_set_priority(icalproperty* prop, int v);
int icalproperty_get_priority(const icalproperty* prop);icalproperty* icalproperty_vanew_priority(int v, ...);

/* PRODID */
icalproperty* icalproperty_new_prodid(const char* v);
void icalproperty_set_prodid(icalproperty* prop, const char* v);
const char* icalproperty_get_prodid(const icalproperty* prop);icalproperty* icalproperty_vanew_prodid(const char* v, ...);

/* QUERY */
icalproperty* icalproperty_new_query(const char* v);
void icalproperty_set_query(icalproperty* prop, const char* v);
const char* icalproperty_get_query(const icalproperty* prop);icalproperty* icalproperty_vanew_query(const char* v, ...);

/* QUERYNAME */
icalproperty* icalproperty_new_queryname(const char* v);
void icalproperty_set_queryname(icalproperty* prop, const char* v);
const char* icalproperty_get_queryname(const icalproperty* prop);icalproperty* icalproperty_vanew_queryname(const char* v, ...);

/* RDATE */
icalproperty* icalproperty_new_rdate(struct icaldatetimeperiodtype v);
void icalproperty_set_rdate(icalproperty* prop, struct icaldatetimeperiodtype v);
struct icaldatetimeperiodtype icalproperty_get_rdate(const icalproperty* prop);icalproperty* icalproperty_vanew_rdate(struct icaldatetimeperiodtype v, ...);

/* RECURRENCE-ID */
icalproperty* icalproperty_new_recurrenceid(struct icaltimetype v);
void icalproperty_set_recurrenceid(icalproperty* prop, struct icaltimetype v);
struct icaltimetype icalproperty_get_recurrenceid(const icalproperty* prop);icalproperty* icalproperty_vanew_recurrenceid(struct icaltimetype v, ...);

/* RELATED-TO */
icalproperty* icalproperty_new_relatedto(const char* v);
void icalproperty_set_relatedto(icalproperty* prop, const char* v);
const char* icalproperty_get_relatedto(const icalproperty* prop);icalproperty* icalproperty_vanew_relatedto(const char* v, ...);

/* RELCALID */
icalproperty* icalproperty_new_relcalid(const char* v);
void icalproperty_set_relcalid(icalproperty* prop, const char* v);
const char* icalproperty_get_relcalid(const icalproperty* prop);icalproperty* icalproperty_vanew_relcalid(const char* v, ...);

/* REPEAT */
icalproperty* icalproperty_new_repeat(int v);
void icalproperty_set_repeat(icalproperty* prop, int v);
int icalproperty_get_repeat(const icalproperty* prop);icalproperty* icalproperty_vanew_repeat(int v, ...);

/* REQUEST-STATUS */
icalproperty* icalproperty_new_requeststatus(struct icalreqstattype v);
void icalproperty_set_requeststatus(icalproperty* prop, struct icalreqstattype v);
struct icalreqstattype icalproperty_get_requeststatus(const icalproperty* prop);icalproperty* icalproperty_vanew_requeststatus(struct icalreqstattype v, ...);

/* RESOURCES */
icalproperty* icalproperty_new_resources(const char* v);
void icalproperty_set_resources(icalproperty* prop, const char* v);
const char* icalproperty_get_resources(const icalproperty* prop);icalproperty* icalproperty_vanew_resources(const char* v, ...);

/* RRULE */
icalproperty* icalproperty_new_rrule(struct icalrecurrencetype v);
void icalproperty_set_rrule(icalproperty* prop, struct icalrecurrencetype v);
struct icalrecurrencetype icalproperty_get_rrule(const icalproperty* prop);icalproperty* icalproperty_vanew_rrule(struct icalrecurrencetype v, ...);

/* SCOPE */
icalproperty* icalproperty_new_scope(const char* v);
void icalproperty_set_scope(icalproperty* prop, const char* v);
const char* icalproperty_get_scope(const icalproperty* prop);icalproperty* icalproperty_vanew_scope(const char* v, ...);

/* SEQUENCE */
icalproperty* icalproperty_new_sequence(int v);
void icalproperty_set_sequence(icalproperty* prop, int v);
int icalproperty_get_sequence(const icalproperty* prop);icalproperty* icalproperty_vanew_sequence(int v, ...);

/* STATUS */
icalproperty* icalproperty_new_status(enum icalproperty_status v);
void icalproperty_set_status(icalproperty* prop, enum icalproperty_status v);
enum icalproperty_status icalproperty_get_status(const icalproperty* prop);icalproperty* icalproperty_vanew_status(enum icalproperty_status v, ...);

/* SUMMARY */
icalproperty* icalproperty_new_summary(const char* v);
void icalproperty_set_summary(icalproperty* prop, const char* v);
const char* icalproperty_get_summary(const icalproperty* prop);icalproperty* icalproperty_vanew_summary(const char* v, ...);

/* TARGET */
icalproperty* icalproperty_new_target(const char* v);
void icalproperty_set_target(icalproperty* prop, const char* v);
const char* icalproperty_get_target(const icalproperty* prop);icalproperty* icalproperty_vanew_target(const char* v, ...);

/* TRANSP */
icalproperty* icalproperty_new_transp(enum icalproperty_transp v);
void icalproperty_set_transp(icalproperty* prop, enum icalproperty_transp v);
enum icalproperty_transp icalproperty_get_transp(const icalproperty* prop);icalproperty* icalproperty_vanew_transp(enum icalproperty_transp v, ...);

/* TRIGGER */
icalproperty* icalproperty_new_trigger(struct icaltriggertype v);
void icalproperty_set_trigger(icalproperty* prop, struct icaltriggertype v);
struct icaltriggertype icalproperty_get_trigger(const icalproperty* prop);icalproperty* icalproperty_vanew_trigger(struct icaltriggertype v, ...);

/* TZID */
icalproperty* icalproperty_new_tzid(const char* v);
void icalproperty_set_tzid(icalproperty* prop, const char* v);
const char* icalproperty_get_tzid(const icalproperty* prop);icalproperty* icalproperty_vanew_tzid(const char* v, ...);

/* TZNAME */
icalproperty* icalproperty_new_tzname(const char* v);
void icalproperty_set_tzname(icalproperty* prop, const char* v);
const char* icalproperty_get_tzname(const icalproperty* prop);icalproperty* icalproperty_vanew_tzname(const char* v, ...);

/* TZOFFSETFROM */
icalproperty* icalproperty_new_tzoffsetfrom(int v);
void icalproperty_set_tzoffsetfrom(icalproperty* prop, int v);
int icalproperty_get_tzoffsetfrom(const icalproperty* prop);icalproperty* icalproperty_vanew_tzoffsetfrom(int v, ...);

/* TZOFFSETTO */
icalproperty* icalproperty_new_tzoffsetto(int v);
void icalproperty_set_tzoffsetto(icalproperty* prop, int v);
int icalproperty_get_tzoffsetto(const icalproperty* prop);icalproperty* icalproperty_vanew_tzoffsetto(int v, ...);

/* TZURL */
icalproperty* icalproperty_new_tzurl(const char* v);
void icalproperty_set_tzurl(icalproperty* prop, const char* v);
const char* icalproperty_get_tzurl(const icalproperty* prop);icalproperty* icalproperty_vanew_tzurl(const char* v, ...);

/* UID */
icalproperty* icalproperty_new_uid(const char* v);
void icalproperty_set_uid(icalproperty* prop, const char* v);
const char* icalproperty_get_uid(const icalproperty* prop);icalproperty* icalproperty_vanew_uid(const char* v, ...);

/* URL */
icalproperty* icalproperty_new_url(const char* v);
void icalproperty_set_url(icalproperty* prop, const char* v);
const char* icalproperty_get_url(const icalproperty* prop);icalproperty* icalproperty_vanew_url(const char* v, ...);

/* VERSION */
icalproperty* icalproperty_new_version(const char* v);
void icalproperty_set_version(icalproperty* prop, const char* v);
const char* icalproperty_get_version(const icalproperty* prop);icalproperty* icalproperty_vanew_version(const char* v, ...);

/* X */
icalproperty* icalproperty_new_x(const char* v);
void icalproperty_set_x(icalproperty* prop, const char* v);
const char* icalproperty_get_x(const icalproperty* prop);icalproperty* icalproperty_vanew_x(const char* v, ...);

/* X-LIC-CLASS */
icalproperty* icalproperty_new_xlicclass(enum icalproperty_xlicclass v);
void icalproperty_set_xlicclass(icalproperty* prop, enum icalproperty_xlicclass v);
enum icalproperty_xlicclass icalproperty_get_xlicclass(const icalproperty* prop);icalproperty* icalproperty_vanew_xlicclass(enum icalproperty_xlicclass v, ...);

/* X-LIC-CLUSTERCOUNT */
icalproperty* icalproperty_new_xlicclustercount(const char* v);
void icalproperty_set_xlicclustercount(icalproperty* prop, const char* v);
const char* icalproperty_get_xlicclustercount(const icalproperty* prop);icalproperty* icalproperty_vanew_xlicclustercount(const char* v, ...);

/* X-LIC-ERROR */
icalproperty* icalproperty_new_xlicerror(const char* v);
void icalproperty_set_xlicerror(icalproperty* prop, const char* v);
const char* icalproperty_get_xlicerror(const icalproperty* prop);icalproperty* icalproperty_vanew_xlicerror(const char* v, ...);

/* X-LIC-MIMECHARSET */
icalproperty* icalproperty_new_xlicmimecharset(const char* v);
void icalproperty_set_xlicmimecharset(icalproperty* prop, const char* v);
const char* icalproperty_get_xlicmimecharset(const icalproperty* prop);icalproperty* icalproperty_vanew_xlicmimecharset(const char* v, ...);

/* X-LIC-MIMECID */
icalproperty* icalproperty_new_xlicmimecid(const char* v);
void icalproperty_set_xlicmimecid(icalproperty* prop, const char* v);
const char* icalproperty_get_xlicmimecid(const icalproperty* prop);icalproperty* icalproperty_vanew_xlicmimecid(const char* v, ...);

/* X-LIC-MIMECONTENTTYPE */
icalproperty* icalproperty_new_xlicmimecontenttype(const char* v);
void icalproperty_set_xlicmimecontenttype(icalproperty* prop, const char* v);
const char* icalproperty_get_xlicmimecontenttype(const icalproperty* prop);icalproperty* icalproperty_vanew_xlicmimecontenttype(const char* v, ...);

/* X-LIC-MIMEENCODING */
icalproperty* icalproperty_new_xlicmimeencoding(const char* v);
void icalproperty_set_xlicmimeencoding(icalproperty* prop, const char* v);
const char* icalproperty_get_xlicmimeencoding(const icalproperty* prop);icalproperty* icalproperty_vanew_xlicmimeencoding(const char* v, ...);

/* X-LIC-MIMEFILENAME */
icalproperty* icalproperty_new_xlicmimefilename(const char* v);
void icalproperty_set_xlicmimefilename(icalproperty* prop, const char* v);
const char* icalproperty_get_xlicmimefilename(const icalproperty* prop);icalproperty* icalproperty_vanew_xlicmimefilename(const char* v, ...);

/* X-LIC-MIMEOPTINFO */
icalproperty* icalproperty_new_xlicmimeoptinfo(const char* v);
void icalproperty_set_xlicmimeoptinfo(icalproperty* prop, const char* v);
const char* icalproperty_get_xlicmimeoptinfo(const icalproperty* prop);icalproperty* icalproperty_vanew_xlicmimeoptinfo(const char* v, ...);


#endif /*ICALPROPERTY_H*/
/* -*- Mode: C -*- */
/*======================================================================
  FILE: icalproperty.h
  CREATOR: eric 20 March 1999


  $Id: ical.h,v 1.6 2002/10/10 00:41:34 acampi Exp $
  $Locker:  $

  

 (C) COPYRIGHT 2000, Eric Busboom, http://www.softwarestudio.org

 This program is free software; you can redistribute it and/or modify
 it under the terms of either: 

    The LGPL as published by the Free Software Foundation, version
    2.1, available at: http://www.fsf.org/copyleft/lesser.html

  Or:

    The Mozilla Public License Version 1.0. You may obtain a copy of
    the License at http://www.mozilla.org/MPL/

  The original code is icalparam.h

  ======================================================================*/


#ifndef ICALPROPERTY_H
#define ICALPROPERTY_H

#include <time.h>
#include <stdarg.h>  /* for va_... */



/* Actually in icalderivedproperty.h:
   typedef struct icalproperty_impl icalproperty; */


icalproperty* icalproperty_new(icalproperty_kind kind);

icalproperty* icalproperty_new_clone(icalproperty * prop);

icalproperty* icalproperty_new_from_string(const char* str);

const char* icalproperty_as_ical_string(icalproperty* prop);

void  icalproperty_free(icalproperty* prop);

icalproperty_kind icalproperty_isa(icalproperty* property);
int icalproperty_isa_property(void* property);

void icalproperty_add_parameters(struct icalproperty_impl *prop,va_list args);
void icalproperty_add_parameter(icalproperty* prop,icalparameter* parameter);
void icalproperty_set_parameter(icalproperty* prop,icalparameter* parameter);
void icalproperty_set_parameter_from_string(icalproperty* prop,
                                            const char* name, const char* value);
const char* icalproperty_get_parameter_as_string(icalproperty* prop,
                                                 const char* name);

void icalproperty_remove_parameter(icalproperty* prop,
				   icalparameter_kind kind);

void icalproperty_remove_parameter_by_kind(icalproperty* prop,
					   icalparameter_kind kind);

void icalproperty_remove_parameter_by_name(icalproperty* prop,
					   const char *name);

void icalproperty_remove_parameter_by_ref(icalproperty* prop,
					  icalparameter *param);



int icalproperty_count_parameters(const icalproperty* prop);

/* Iterate through the parameters */
icalparameter* icalproperty_get_first_parameter(icalproperty* prop,
						icalparameter_kind kind);
icalparameter* icalproperty_get_next_parameter(icalproperty* prop,
						icalparameter_kind kind);
/* Access the value of the property */
void icalproperty_set_value(icalproperty* prop, icalvalue* value);
void icalproperty_set_value_from_string(icalproperty* prop,const char* value, const char* kind);

icalvalue* icalproperty_get_value(const icalproperty* prop);
const char* icalproperty_get_value_as_string(const icalproperty* prop);

/* Deal with X properties */

void icalproperty_set_x_name(icalproperty* prop, const char* name);
const char* icalproperty_get_x_name(icalproperty* prop);

/** Return the name of the property -- the type name converted to a
 *  string, or the value of _get_x_name if the type is and X
 *  property 
 */
const char* icalproperty_get_property_name (const icalproperty* prop);

icalvalue_kind icalparameter_value_to_value_kind(icalparameter_value value);

/* Convert kinds to string and get default value type */

icalvalue_kind icalproperty_kind_to_value_kind(icalproperty_kind kind);
icalproperty_kind icalproperty_value_kind_to_kind(icalvalue_kind kind);
const char* icalproperty_kind_to_string(icalproperty_kind kind);
icalproperty_kind icalproperty_string_to_kind(const char* string);

/** Check validity of a specific icalproperty_kind **/
int icalproperty_kind_is_valid(const icalproperty_kind kind);

icalproperty_method icalproperty_string_to_method(const char* str);
const char* icalproperty_method_to_string(icalproperty_method method);


const char* icalproperty_enum_to_string(int e);
int icalproperty_string_to_enum(const char* str);
int icalproperty_kind_and_string_to_enum(const int kind, const char* str);

const char* icalproperty_status_to_string(icalproperty_status);
icalproperty_status icalproperty_string_to_status(const char* string);

int icalproperty_enum_belongs_to_property(icalproperty_kind kind, int e);




#endif /*ICALPROPERTY_H*/
/*======================================================================
 FILE: pvl.h
 CREATOR: eric November, 1995


 (C) COPYRIGHT 2000, Eric Busboom, http://www.softwarestudio.org
======================================================================*/


#ifndef __PVL_H__
#define __PVL_H__

typedef struct pvl_list_t* pvl_list;
typedef struct pvl_elem_t* pvl_elem;

/**
 * This type is private. Always use pvl_elem instead. The struct would
 * not even appear in this header except to make code in the USE_MACROS
 * blocks work
 */

typedef struct pvl_elem_t
{
	int MAGIC;			/**< Magic Identifier */
	void *d;			/**< Pointer to data user is storing */
	struct pvl_elem_t *next;	/**< Next element */
	struct pvl_elem_t *prior;	/**< Prior element */
} pvl_elem_t;



/**
 * This global is incremented for each call to pvl_new_element(); it gives each
 * list a unique identifer 
 */

extern int  pvl_elem_count;
extern int  pvl_list_count;

/* Create new lists or elements */
pvl_elem pvl_new_element(void* d, pvl_elem next,pvl_elem prior);
pvl_list pvl_newlist(void);
void pvl_free(pvl_list);

/* Add, remove, or get the head of the list */
void pvl_unshift(pvl_list l,void *d);
void* pvl_shift(pvl_list l);
pvl_elem pvl_head(pvl_list);

/* Add, remove or get the tail of the list */
void pvl_push(pvl_list l,void *d);
void* pvl_pop(pvl_list l);
pvl_elem pvl_tail(pvl_list);

/* Insert elements in random places */
typedef int (*pvl_comparef)(void* a, void* b); /* a, b are of the data type*/
void pvl_insert_ordered(pvl_list l,pvl_comparef f,void *d);
void pvl_insert_after(pvl_list l,pvl_elem e,void *d);
void pvl_insert_before(pvl_list l,pvl_elem e,void *d);

/* Remove an element, or clear the entire list */
void* pvl_remove(pvl_list,pvl_elem); /* Remove element, return data */
void pvl_clear(pvl_list); /* Remove all elements, de-allocate all data */

int pvl_count(pvl_list);

/* Navagate the list */
pvl_elem pvl_next(pvl_elem e);
pvl_elem pvl_prior(pvl_elem e);

/* get the data in the list */
#ifndef PVL_USE_MACROS
void* pvl_data(pvl_elem);
#else
#define pvl_data(x) x==0 ? 0 : ((struct pvl_elem_t *)x)->d;
#endif


/* Find an element for which a function returns true */
typedef int (*pvl_findf)(void* a, void* b); /*a is list elem, b is other data*/
pvl_elem pvl_find(pvl_list l,pvl_findf f,void* v);
pvl_elem pvl_find_next(pvl_list l,pvl_findf f,void* v);

/**
 * Pass each element in the list to a function
 * a is list elem, b is other data
 */
typedef void (*pvl_applyf)(void* a, void* b); 
void pvl_apply(pvl_list l,pvl_applyf f, void *v);


#endif /* __PVL_H__ */





/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 4 -*- */
/*======================================================================
 FILE: icalarray.h
 CREATOR: Damon Chaplin 07 March 2001


 $Id: ical.h,v 1.6 2002/10/10 00:41:34 acampi Exp $
 $Locker:  $

 (C) COPYRIGHT 2001, Ximian, Inc.

 This program is free software; you can redistribute it and/or modify
 it under the terms of either: 

    The LGPL as published by the Free Software Foundation, version
    2.1, available at: http://www.fsf.org/copyleft/lesser.html

  Or:

    The Mozilla Public License Version 1.0. You may obtain a copy of
    the License at http://www.mozilla.org/MPL/


======================================================================*/


#ifndef ICALARRAY_H
#define ICALARRAY_H

/** @file icalarray.h 
 *
 *  @brief An array of arbitrarily-sized elements which grows
 *  dynamically as elements are added. 
 */

typedef struct _icalarray icalarray;
struct _icalarray {
    int		 element_size;
    int		 increment_size;
    int		 num_elements;
    int		 space_allocated;
    void	*data;
};



icalarray *icalarray_new		(int		 element_size,
					 int		 increment_size);
void	   icalarray_free		(icalarray	*array);

void	   icalarray_append		(icalarray	*array,
					 void		*element);
void	   icalarray_remove_element_at	(icalarray	*array,
					 int		 position);

void	  *icalarray_element_at		(icalarray	*array,
					 int		 position);

void	   icalarray_sort		(icalarray	*array,
					 int	       (*compare) (const void *, const void *));


#endif /* ICALARRAY_H */
/* -*- Mode: C -*- */
/*======================================================================
 FILE: icalcomponent.h
 CREATOR: eric 20 March 1999


 (C) COPYRIGHT 2000, Eric Busboom, http://www.softwarestudio.org

 This program is free software; you can redistribute it and/or modify
 it under the terms of either: 

    The LGPL as published by the Free Software Foundation, version
    2.1, available at: http://www.fsf.org/copyleft/lesser.html

  Or:

    The Mozilla Public License Version 1.0. You may obtain a copy of
    the License at http://www.mozilla.org/MPL/

  The original code is icalcomponent.h

======================================================================*/

#ifndef ICALCOMPONENT_H
#define ICALCOMPONENT_H


typedef struct icalcomponent_impl icalcomponent;

#ifndef ICALTIMEONE_DEFINED
#define ICALTIMEONE_DEFINED
/** @brief An opaque struct representing a timezone.  
 * We declare this here to avoid a circular dependancy. 
 */
typedef struct _icaltimezone		icaltimezone;
#endif


/* This is exposed so that callers will not have to allocate and
   deallocate iterators. Pretend that you can't see it. */
typedef struct icalcompiter
{
	icalcomponent_kind kind;
	pvl_elem iter;

} icalcompiter;

icalcomponent* icalcomponent_new(icalcomponent_kind kind);
icalcomponent* icalcomponent_new_clone(icalcomponent* component);
icalcomponent* icalcomponent_new_from_string(char* str);
icalcomponent* icalcomponent_vanew(icalcomponent_kind kind, ...);
void icalcomponent_free(icalcomponent* component);

char* icalcomponent_as_ical_string(icalcomponent* component);

int icalcomponent_is_valid(icalcomponent* component);

icalcomponent_kind icalcomponent_isa(const icalcomponent* component);

int icalcomponent_isa_component (void* component);

/* 
 * Working with properties
 */

void icalcomponent_add_property(icalcomponent* component,
				icalproperty* property);

void icalcomponent_remove_property(icalcomponent* component,
				   icalproperty* property);

int icalcomponent_count_properties(icalcomponent* component,
				   icalproperty_kind kind);

/* Iterate through the properties */
icalproperty* icalcomponent_get_current_property(icalcomponent* component);

icalproperty* icalcomponent_get_first_property(icalcomponent* component,
					      icalproperty_kind kind);
icalproperty* icalcomponent_get_next_property(icalcomponent* component,
					      icalproperty_kind kind);


/* 
 * Working with components
 */ 


/* Return the first VEVENT, VTODO or VJOURNAL sub-component of cop, or
   comp if it is one of those types */

icalcomponent* icalcomponent_get_inner(icalcomponent* comp);


void icalcomponent_add_component(icalcomponent* parent,
				icalcomponent* child);

void icalcomponent_remove_component(icalcomponent* parent,
				icalcomponent* child);

int icalcomponent_count_components(icalcomponent* component,
				   icalcomponent_kind kind);

/**
   This takes 2 VCALENDAR components and merges the second one into the first,
   resolving any problems with conflicting TZIDs. comp_to_merge will no
   longer exist after calling this function. */
void icalcomponent_merge_component(icalcomponent* comp,
				   icalcomponent* comp_to_merge);


/* Iteration Routines. There are two forms of iterators, internal and
external. The internal ones came first, and are almost completely
sufficient, but they fail badly when you want to construct a loop that
removes components from the container.*/


/* Iterate through components */
icalcomponent* icalcomponent_get_current_component (icalcomponent* component);

icalcomponent* icalcomponent_get_first_component(icalcomponent* component,
					      icalcomponent_kind kind);
icalcomponent* icalcomponent_get_next_component(icalcomponent* component,
					      icalcomponent_kind kind);

/* Using external iterators */
icalcompiter icalcomponent_begin_component(icalcomponent* component,
					   icalcomponent_kind kind);
icalcompiter icalcomponent_end_component(icalcomponent* component,
					 icalcomponent_kind kind);
icalcomponent* icalcompiter_next(icalcompiter* i);
icalcomponent* icalcompiter_prior(icalcompiter* i);
icalcomponent* icalcompiter_deref(icalcompiter* i);


/* Working with embedded error properties */


/* Check the component against itip rules and insert error properties*/
/* Working with embedded error properties */
int icalcomponent_check_restrictions(icalcomponent* comp);

/** Count embedded errors. */
int icalcomponent_count_errors(icalcomponent* component);

/** Remove all X-LIC-ERROR properties*/
void icalcomponent_strip_errors(icalcomponent* component);

/** Convert some X-LIC-ERROR properties into RETURN-STATUS properties*/
void icalcomponent_convert_errors(icalcomponent* component);

/* Internal operations. They are private, and you should not be using them. */
icalcomponent* icalcomponent_get_parent(icalcomponent* component);
void icalcomponent_set_parent(icalcomponent* component, 
			      icalcomponent* parent);

/* Kind conversion routines */

int icalcomponent_kind_is_valid(const icalcomponent_kind kind);

icalcomponent_kind icalcomponent_string_to_kind(const char* string);

const char* icalcomponent_kind_to_string(icalcomponent_kind kind);


/************* Derived class methods.  ****************************

If the code was in an OO language, the remaining routines would be
members of classes derived from icalcomponent. Don't call them on the
wrong component subtypes. */

/** For VCOMPONENT: Return a reference to the first VEVENT, VTODO or
   VJOURNAL */
icalcomponent* icalcomponent_get_first_real_component(icalcomponent *c);

/** For VEVENT, VTODO, VJOURNAL and VTIMEZONE: report the start and end
   times of an event in UTC */
struct icaltime_span icalcomponent_get_span(icalcomponent* comp);

/******************** Convienience routines **********************/

void icalcomponent_set_dtstart(icalcomponent* comp, struct icaltimetype v);
struct icaltimetype icalcomponent_get_dtstart(icalcomponent* comp);

/* For the icalcomponent routines only, dtend and duration are tied
   together. If you call the set routine for one and the other exists,
   the routine will calculate the change to the other. That is, if
   there is a DTEND and you call set_duration, the routine will modify
   DTEND to be the sum of DTSTART and the duration. If you call a get
   routine for one and the other exists, the routine will calculate
   the return value. If you call a set routine and neither exists, the
   routine will create the apcompriate comperty */


struct icaltimetype icalcomponent_get_dtend(icalcomponent* comp);
void icalcomponent_set_dtend(icalcomponent* comp, struct icaltimetype v);

struct icaltimetype icalcomponent_get_due(icalcomponent* comp);
void icalcomponent_set_due(icalcomponent* comp, struct icaltimetype v);

void icalcomponent_set_duration(icalcomponent* comp, 
				struct icaldurationtype v);
struct icaldurationtype icalcomponent_get_duration(icalcomponent* comp);

void icalcomponent_set_method(icalcomponent* comp, icalproperty_method method);
icalproperty_method icalcomponent_get_method(icalcomponent* comp);

struct icaltimetype icalcomponent_get_dtstamp(icalcomponent* comp);
void icalcomponent_set_dtstamp(icalcomponent* comp, struct icaltimetype v);

void icalcomponent_set_summary(icalcomponent* comp, const char* v);
const char* icalcomponent_get_summary(icalcomponent* comp);

void icalcomponent_set_comment(icalcomponent* comp, const char* v);
const char* icalcomponent_get_comment(icalcomponent* comp);

void icalcomponent_set_uid(icalcomponent* comp, const char* v);
const char* icalcomponent_get_uid(icalcomponent* comp);

void icalcomponent_set_relcalid(icalcomponent* comp, const char* v);
const char* icalcomponent_get_relcalid(icalcomponent* comp);

void icalcomponent_set_recurrenceid(icalcomponent* comp, 
				    struct icaltimetype v);
struct icaltimetype icalcomponent_get_recurrenceid(icalcomponent* comp);

void icalcomponent_set_description(icalcomponent* comp, const char* v);
const char* icalcomponent_get_description(icalcomponent* comp);

void icalcomponent_set_location(icalcomponent* comp, const char* v);
const char* icalcomponent_get_location(icalcomponent* comp);

void icalcomponent_set_organizer(icalcomponent* comp, 
				 struct icalorganizertype org);
struct icalorganizertype icalcomponent_get_organizer(icalcomponent* comp);


void icalcomponent_set_sequence(icalcomponent* comp, int v);
int icalcomponent_get_sequence(icalcomponent* comp);

void icalcomponent_set_status(icalcomponent* comp, int v);
int icalcomponent_get_status(icalcomponent* comp);


/** Calls the given function for each TZID parameter found in the
    component, and any subcomponents. */
void icalcomponent_foreach_tzid(icalcomponent* comp,
				void (*callback)(icalparameter *param, void *data),
				void *callback_data);

/** Returns the icaltimezone in the component corresponding to the
    TZID, or NULL if it can't be found. */
icaltimezone* icalcomponent_get_timezone(icalcomponent* comp,
					 const char *tzid);

int icalproperty_recurrence_is_excluded(icalcomponent *comp,
                                       struct icaltimetype *dtstart,
                                       struct icaltimetype *recurtime); 

void icalcomponent_foreach_recurrence(icalcomponent* comp,
				      struct icaltimetype start,
				      struct icaltimetype end,
			void (*callback)(icalcomponent *comp, 
                                         struct icaltime_span *span, 
                                         void *data),
			      void *callback_data);


/*************** Type Specific routines ***************/

icalcomponent* icalcomponent_new_vcalendar();
icalcomponent* icalcomponent_new_vevent();
icalcomponent* icalcomponent_new_vtodo();
icalcomponent* icalcomponent_new_vjournal();
icalcomponent* icalcomponent_new_valarm();
icalcomponent* icalcomponent_new_vfreebusy();
icalcomponent* icalcomponent_new_vtimezone();
icalcomponent* icalcomponent_new_xstandard();
icalcomponent* icalcomponent_new_xdaylight();
icalcomponent* icalcomponent_new_vagenda();
icalcomponent* icalcomponent_new_vquery();

#endif /* !ICALCOMPONENT_H */
/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 4 -*- */
/*======================================================================
 FILE: icaltimezone.h
 CREATOR: Damon Chaplin 15 March 2001


 $Id: ical.h,v 1.6 2002/10/10 00:41:34 acampi Exp $
 $Locker:  $

 (C) COPYRIGHT 2001, Damon Chaplin

 This program is free software; you can redistribute it and/or modify
 it under the terms of either: 

    The LGPL as published by the Free Software Foundation, version
    2.1, available at: http://www.fsf.org/copyleft/lesser.html

  Or:

    The Mozilla Public License Version 1.0. You may obtain a copy of
    the License at http://www.mozilla.org/MPL/


======================================================================*/
/**
 * @file icaltimezone.h
 * @brief timezone handling routines
 */

#ifndef ICALTIMEZONE_H
#define ICALTIMEZONE_H

#include <stdio.h> /* For FILE* */


/**
 * @par Creating/Destroying individual icaltimezones.
 */

/** Creates a new icaltimezone. */
icaltimezone *icaltimezone_new			(void);

/** Frees all memory used for the icaltimezone. Set free_struct to free the
   icaltimezone struct as well. */
void icaltimezone_free				(icaltimezone *zone,
						 int free_struct);


/**
 * @par Accessing timezones.
 */

/** Free any builtin timezone information **/
void icaltimezone_free_builtin_timezones(void);

/** Returns the array of builtin icaltimezones. */
icalarray* icaltimezone_get_builtin_timezones	(void);

/** Returns a single builtin timezone, given its Olson city name. */
icaltimezone* icaltimezone_get_builtin_timezone	(const char *location);

/** Returns a single builtin timezone, given its TZID. */
icaltimezone* icaltimezone_get_builtin_timezone_from_tzid (const char *tzid);

/** Returns the UTC timezone. */
icaltimezone* icaltimezone_get_utc_timezone	(void);

/** Returns the TZID of a timezone. */
char*	icaltimezone_get_tzid			(icaltimezone	*zone);

/** Returns the city name of a timezone. */
char*	icaltimezone_get_location		(icaltimezone	*zone);

/** Returns the TZNAME properties used in the latest STANDARD and DAYLIGHT
   components. If they are the same it will return just one, e.g. "LMT".
   If they are different it will format them like "EST/EDT". Note that this
   may also return NULL. */
char*	icaltimezone_get_tznames		(icaltimezone	*zone);

/** Returns the latitude of a builtin timezone. */
double	icaltimezone_get_latitude		(icaltimezone	*zone);

/** Returns the longitude of a builtin timezone. */
double	icaltimezone_get_longitude		(icaltimezone	*zone);

/** Returns the VTIMEZONE component of a timezone. */
icalcomponent*	icaltimezone_get_component	(icaltimezone	*zone);

/** Sets the VTIMEZONE component of an icaltimezone, initializing the tzid,
   location & tzname fields. It returns 1 on success or 0 on failure, i.e.
   no TZID was found. */
int	icaltimezone_set_component		(icaltimezone	*zone,
						 icalcomponent	*comp);

/**
 * @par Converting times between timezones.
 */

void	icaltimezone_convert_time		(struct icaltimetype *tt,
						 icaltimezone	*from_zone,
						 icaltimezone	*to_zone);


/**
 * @par Getting offsets from UTC.
 */

/** Calculates the UTC offset of a given local time in the given
   timezone.  It is the number of seconds to add to UTC to get local
   time.  The is_daylight flag is set to 1 if the time is in
   daylight-savings time. */
int	icaltimezone_get_utc_offset		(icaltimezone	*zone,
						 struct icaltimetype *tt,
						 int		*is_daylight);

/** Calculates the UTC offset of a given UTC time in the given
   timezone.  It is the number of seconds to add to UTC to get local
   time.  The is_daylight flag is set to 1 if the time is in
   daylight-savings time. */
int	icaltimezone_get_utc_offset_of_utc_time	(icaltimezone	*zone,
						 struct icaltimetype *tt,
						 int		*is_daylight);



/*
 * Handling arrays of timezones. Mainly for internal use.
 */
icalarray*  icaltimezone_array_new		(void);

void	    icaltimezone_array_append_from_vtimezone (icalarray	    *timezones,
						      icalcomponent *child);
void	    icaltimezone_array_free		(icalarray	*timezones);


/*
 * @par Handling the default location the timezone files
 */

/** Set the directory to look for the zonefiles */
void set_zone_directory(char *path);

/** Free memory dedicated to the zonefile directory */
void free_zone_directory();

/*
 * @par Debugging Output.
 */

/** Dumps information about changes in the timezone up to and including
   max_year. */
int	icaltimezone_dump_changes		(icaltimezone	*zone,
						 int		 max_year,
						 FILE		*fp);

#endif /* ICALTIMEZONE_H */
/* -*- Mode: C -*- */
/*======================================================================
  FILE: icalparser.h
  CREATOR: eric 20 April 1999
  
  $Id: ical.h,v 1.6 2002/10/10 00:41:34 acampi Exp $


 (C) COPYRIGHT 2000, Eric Busboom, http://www.softwarestudio.org

 This program is free software; you can redistribute it and/or modify
 it under the terms of either: 

    The LGPL as published by the Free Software Foundation, version
    2.1, available at: http://www.fsf.org/copyleft/lesser.html

  Or:

    The Mozilla Public License Version 1.0. You may obtain a copy of
    the License at http://www.mozilla.org/MPL/

  The original code is icalparser.h

======================================================================*/


#ifndef ICALPARSER_H
#define ICALPARSER_H


#include <stdio.h> /* For FILE* */

typedef struct icalparser_impl icalparser;


/**
 * @file  icalparser.h
 * @brief Line-oriented parsing. 
 * 
 * Create a new parser via icalparse_new_parser, then add lines one at
 * a time with icalparse_add_line(). icalparser_add_line() will return
 * non-zero when it has finished with a component.
 */

typedef enum icalparser_state {
    ICALPARSER_ERROR,
    ICALPARSER_SUCCESS,
    ICALPARSER_BEGIN_COMP,
    ICALPARSER_END_COMP,
    ICALPARSER_IN_PROGRESS
} icalparser_state;

icalparser* icalparser_new(void);
icalcomponent* icalparser_add_line(icalparser* parser, char* str );
icalcomponent* icalparser_clean(icalparser* parser);
icalparser_state icalparser_get_state(icalparser* parser);
void icalparser_free(icalparser* parser);


/**
 * Message oriented parsing.  icalparser_parse takes a string that
 * holds the text ( in RFC 2445 format ) and returns a pointer to an
 * icalcomponent. The caller owns the memory. line_gen_func is a
 * pointer to a function that returns one content line per invocation
 */

icalcomponent* icalparser_parse(icalparser *parser,
	char* (*line_gen_func)(char *s, size_t size, void *d));

/**
   Set the data that icalparser_parse will give to the line_gen_func
   as the parameter 'd'
 */
void icalparser_set_gen_data(icalparser* parser, void* data);


icalcomponent* icalparser_parse_string(const char* str);


/***********************************************************************
 * Parser support functions
 ***********************************************************************/

/** Use the flex/bison parser to turn a string into a value type */
icalvalue*  icalparser_parse_value(icalvalue_kind kind, 
				   const char* str, icalcomponent** errors);

/** Given a line generator function, return a single iCal content line.*/
char* icalparser_get_line(icalparser* parser, char* (*line_gen_func)(char *s, size_t size, void *d));

char* icalparser_string_line_generator(char *out, size_t buf_size, void *d);

#endif /* !ICALPARSE_H */
/* -*- Mode: C -*- */
/*======================================================================
 FILE: icalmemory.h
 CREATOR: eric 30 June 1999


 $Id: ical.h,v 1.6 2002/10/10 00:41:34 acampi Exp $
 $Locker:  $

 This program is free software; you can redistribute it and/or modify
 it under the terms of either: 

    The LGPL as published by the Free Software Foundation, version
    2.1, available at: http://www.fsf.org/copyleft/lesser.html

  Or:

    The Mozilla Public License Version 1.0. You may obtain a copy of
    the License at http://www.mozilla.org/MPL/

 The Initial Developer of the Original Code is Eric Busboom

 (C) COPYRIGHT 2000, Eric Busboom, http://www.softwarestudio.org
======================================================================*/

#ifndef ICALMEMORY_H
#define ICALMEMORY_H

#ifndef WIN32
#include <sys/types.h> /* for size_t */
#else
#include <stddef.h>
#endif

/* Tmp buffers are managed by ical. References can be returned to the
   caller, although the caller will not own the memory. */

void* icalmemory_tmp_buffer(size_t size);
char* icalmemory_tmp_copy(const char* str);

/** Add an externally allocated buffer to the ring. */
void  icalmemory_add_tmp_buffer(void*);


/** Free all memory used in the ring */
void icalmemory_free_ring(void);

/* Non-tmp buffers must be freed. These are mostly wrappers around
 * malloc, etc, but are used so the caller can change the memory
 * allocators in a future version of the library */

void* icalmemory_new_buffer(size_t size);
void* icalmemory_resize_buffer(void* buf, size_t size);
void icalmemory_free_buffer(void* buf);

/**
   icalmemory_append_string will copy the string 'string' to the
   buffer 'buf' starting at position 'pos', reallocing 'buf' if it is
   too small. 'buf_size' is the size of 'buf' and will be changed if
   'buf' is reallocated. 'pos' will point to the last byte of the new
   string in 'buf', usually a '\0' */

/* THESE ROUTINES CAN NOT BE USED ON TMP BUFFERS. Only use them on
   normally allocated memory, or on buffers created from
   icalmemory_new_buffer, never with buffers created by
   icalmemory_tmp_buffer. If icalmemory_append_string has to resize a
   buffer on the ring, the ring will loose track of it an you will
   have memory problems. */

void icalmemory_append_string(char** buf, char** pos, size_t* buf_size, 
			      const char* string);

/**  icalmemory_append_char is similar, but is appends a character instead of a string */
void icalmemory_append_char(char** buf, char** pos, size_t* buf_size, 
			      char ch);

/** A wrapper around strdup. Partly to trap calls to strdup, partly
    because in -ansi, gcc on Red Hat claims that strdup is undeclared */
char* icalmemory_strdup(const char *s);

#endif /* !ICALMEMORY_H */



/* -*- Mode: C -*- */
/*======================================================================
  FILE: icalerror.h
  CREATOR: eric 09 May 1999
  
  $Id: ical.h,v 1.6 2002/10/10 00:41:34 acampi Exp $


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



/* -*- Mode: C -*- */
/*======================================================================
  FILE: icalrestriction.h
  CREATOR: eric 24 April 1999
  
  $Id: ical.h,v 1.6 2002/10/10 00:41:34 acampi Exp $


 (C) COPYRIGHT 2000, Eric Busboom, http://www.softwarestudio.org

 This program is free software; you can redistribute it and/or modify
 it under the terms of either: 

    The LGPL as published by the Free Software Foundation, version
    2.1, available at: http://www.fsf.org/copyleft/lesser.html

  Or:

    The Mozilla Public License Version 1.0. You may obtain a copy of
    the License at http://www.mozilla.org/MPL/

  The original code is icalrestriction.h

  Contributions from:
     Graham Davison (g.m.davison@computer.org)


======================================================================*/


#ifndef ICALRESTRICTION_H
#define ICALRESTRICTION_H

/* These must stay in this order for icalrestriction_compare to work */
typedef enum icalrestriction_kind {
    ICAL_RESTRICTION_NONE=0,		/* 0 */
    ICAL_RESTRICTION_ZERO,		/* 1 */
    ICAL_RESTRICTION_ONE,		/* 2 */
    ICAL_RESTRICTION_ZEROPLUS,		/* 3 */
    ICAL_RESTRICTION_ONEPLUS,		/* 4 */
    ICAL_RESTRICTION_ZEROORONE,		/* 5 */
    ICAL_RESTRICTION_ONEEXCLUSIVE,	/* 6 */
    ICAL_RESTRICTION_ONEMUTUAL,		/* 7 */
    ICAL_RESTRICTION_UNKNOWN		/* 8 */
} icalrestriction_kind;

int 
icalrestriction_compare(icalrestriction_kind restr, int count);


int
icalrestriction_is_parameter_allowed(icalproperty_kind property,
                                       icalparameter_kind parameter);

int icalrestriction_check(icalcomponent* comp);


#endif /* !ICALRESTRICTION_H */



/* -*- Mode: C -*-
  ======================================================================
  FILE: sspm.h Mime Parser
  CREATOR: eric 25 June 2000
  
  $Id: ical.h,v 1.6 2002/10/10 00:41:34 acampi Exp $
  $Locker:  $
    
 The contents of this file are subject to the Mozilla Public License
 Version 1.0 (the "License"); you may not use this file except in
 compliance with the License. You may obtain a copy of the License at
 http://www.mozilla.org/MPL/
 
 Software distributed under the License is distributed on an "AS IS"
 basis, WITHOUT WARRANTY OF ANY KIND, either express or implied. See
 the License for the specific language governing rights and
 limitations under the License.
 

 This program is free software; you can redistribute it and/or modify
 it under the terms of either: 

    The LGPL as published by the Free Software Foundation, version
    2.1, available at: http://www.fsf.org/copyleft/lesser.html

  Or:

    The Mozilla Public License Version 1.0. You may obtain a copy of
    the License at http://www.mozilla.org/MPL/

  The Initial Developer of the Original Code is Eric Busboom

 (C) COPYRIGHT 2000, Eric Busboom, http://www.softwarestudio.org
 ======================================================================*/

#ifndef SSPM_H
#define SSPM_H

enum sspm_major_type {
    SSPM_NO_MAJOR_TYPE,
    SSPM_TEXT_MAJOR_TYPE,
    SSPM_IMAGE_MAJOR_TYPE,
    SSPM_AUDIO_MAJOR_TYPE,
    SSPM_VIDEO_MAJOR_TYPE,
    SSPM_APPLICATION_MAJOR_TYPE,
    SSPM_MULTIPART_MAJOR_TYPE,
    SSPM_MESSAGE_MAJOR_TYPE,
    SSPM_UNKNOWN_MAJOR_TYPE
};

enum sspm_minor_type {
    SSPM_NO_MINOR_TYPE,
    SSPM_ANY_MINOR_TYPE,
    SSPM_PLAIN_MINOR_TYPE,
    SSPM_RFC822_MINOR_TYPE,
    SSPM_DIGEST_MINOR_TYPE,
    SSPM_CALENDAR_MINOR_TYPE,
    SSPM_MIXED_MINOR_TYPE,
    SSPM_RELATED_MINOR_TYPE,
    SSPM_ALTERNATIVE_MINOR_TYPE,
    SSPM_PARALLEL_MINOR_TYPE,
    SSPM_UNKNOWN_MINOR_TYPE
};

enum sspm_encoding {
    SSPM_NO_ENCODING,
    SSPM_QUOTED_PRINTABLE_ENCODING,
    SSPM_8BIT_ENCODING,
    SSPM_7BIT_ENCODING,
    SSPM_BINARY_ENCODING,
    SSPM_BASE64_ENCODING,
    SSPM_UNKNOWN_ENCODING
};

enum sspm_error{
    SSPM_NO_ERROR,
    SSPM_UNEXPECTED_BOUNDARY_ERROR,
    SSPM_WRONG_BOUNDARY_ERROR,
    SSPM_NO_BOUNDARY_ERROR,
    SSPM_NO_HEADER_ERROR,
    SSPM_MALFORMED_HEADER_ERROR
};


struct sspm_header
{
	int def;
	char* boundary;
	enum sspm_major_type major;
	enum sspm_minor_type minor;
	char *minor_text;
	char ** content_type_params;
	char* charset;
	enum sspm_encoding encoding;
	char* filename;
	char* content_id;
	enum sspm_error error;
	char* error_text;
};

struct sspm_part {
	struct sspm_header header;
	int level;
	size_t data_size;
	void *data;
};

struct sspm_action_map {
	enum sspm_major_type major;
	enum sspm_minor_type minor;
	void* (*new_part)();
	void (*add_line)(void *part, struct sspm_header *header, 
			 char* line, size_t size);
	void* (*end_part)(void* part);
	void (*free_part)(void *part);
};

char* sspm_major_type_string(enum sspm_major_type type);
char* sspm_minor_type_string(enum sspm_minor_type type);
char* sspm_encoding_string(enum sspm_encoding type);

int sspm_parse_mime(struct sspm_part *parts, 
		    size_t max_parts,
		    struct sspm_action_map *actions,
		    char* (*get_string)(char *s, size_t size, void* data),
		    void *get_string_data,
		    struct sspm_header *first_header
    );

void sspm_free_parts(struct sspm_part *parts, size_t max_parts);

char *decode_quoted_printable(char *dest, 
				       char *src,
				       size_t *size);
char *decode_base64(char *dest, 
			     char *src,
			     size_t *size);


int sspm_write_mime(struct sspm_part *parts,size_t num_parts,
		    char **output_string, char* header);

#endif /*SSPM_H*/
/* -*- Mode: C -*- */
/*======================================================================
 FILE: icalmime.h
 CREATOR: eric 26 July 2000


 $Id: ical.h,v 1.6 2002/10/10 00:41:34 acampi Exp $
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

#ifndef ICALMIME_H
#define ICALMIME_H


icalcomponent* icalmime_parse(	char* (*line_gen_func)(char *s, size_t size, 
						       void *d),
				void *data);

/* The inverse of icalmime_parse, not implemented yet. Use sspm.h directly.  */
char* icalmime_as_mime_string(char* component);



#endif /* !ICALMIME_H */



/* -*- Mode: C -*-
  ======================================================================
  FILE: icallangbind.h
  CREATOR: eric 25 jan 2001
  
  DESCRIPTION:
  
  $Id: ical.h,v 1.6 2002/10/10 00:41:34 acampi Exp $
  $Locker:  $

  (C) COPYRIGHT 1999 Eric Busboom 
  http://www.softwarestudio.org
  
  This package is free software and is provided "as is" without
  express or implied warranty.  It may be used, redistributed and/or
  modified under the same terms as perl itself. ( Either the Artistic
  License or the GPL. )

  ======================================================================*/

#ifndef __ICALLANGBIND_H__
#define __ICALLANGBIND_H__

int* icallangbind_new_array(int size);
void icallangbind_free_array(int* array);
int icallangbind_access_array(int* array, int index);
icalproperty* icallangbind_get_property(icalcomponent *c, int n, const char* prop);
const char* icallangbind_get_property_val(icalproperty* p);
const char* icallangbind_get_parameter(icalproperty *p, const char* parameter);
icalcomponent* icallangbind_get_component(icalcomponent *c, const char* comp);

icalproperty* icallangbind_get_first_property(icalcomponent *c,
                                              const char* prop);

icalproperty* icallangbind_get_next_property(icalcomponent *c,
                                              const char* prop);

icalcomponent* icallangbind_get_first_component(icalcomponent *c,
                                              const char* comp);

icalcomponent* icallangbind_get_next_component(icalcomponent *c,
                                              const char* comp);

icalparameter* icallangbind_get_first_parameter(icalproperty *prop);

icalparameter* icallangbind_get_next_parameter(icalproperty *prop);

const char* icallangbind_property_eval_string(icalproperty* prop, char* sep);


int icallangbind_string_to_open_flag(const char* str);
#endif /*__ICALLANGBIND_H__*/
#ifdef __cplusplus
};
#endif
