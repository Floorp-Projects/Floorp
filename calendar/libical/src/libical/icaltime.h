/* -*- Mode: C -*- */
/*======================================================================
 FILE: icaltime.h
 CREATOR: eric 02 June 2000


 $Id: icaltime.h,v 1.25 2005/01/24 13:06:10 acampi Exp $
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
 *	- icaltime_from_day_of_year(int doy,  int year)
 *	- icaltime_from_week_number(int week_number, int year)
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
 *	- icaltime_start_doy_week(struct icaltimetype t, int fdow)
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
 *		struct icaltimetype b, icaltimezone *tz)
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
#ifndef ICALTIMEZONE_DEFINED
#define ICALTIMEZONE_DEFINED
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
struct icaltimetype icaltime_from_day_of_year(const int doy,
	const int year);

/**	@brief Contructor (TODO).
 * Create a new time from a weeknumber and a year. */
struct icaltimetype icaltime_from_week_number(const int week_number,
	const int year);

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
int icaltime_day_of_year(const struct icaltimetype t);

/** Return the day of the week of the given time. Sunday is 1 */
int icaltime_day_of_week(const struct icaltimetype t);

/** Return the day of the year for the Sunday of the week that the
   given time is within. */
int icaltime_start_doy_of_week(const struct icaltimetype t);

/** Return the day of the year for the first day of the week that the
   given time is within. */
int icaltime_start_doy_in_week(const struct icaltimetype t, int fdow);

/** Return the day of the year for the first day of the week that the
   given time is within. */
int icaltime_start_doy_week(const struct icaltimetype t, int fdow);

/** Return the week number for the week the given time is within */
int icaltime_week_number(const struct icaltimetype t);

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
	const struct icaltimetype b, icaltimezone *tz);

/** Adds or subtracts a number of days, hours, minutes and seconds. */
void  icaltime_adjust(struct icaltimetype *tt, const int days,
	const int hours, const int minutes, const int seconds);

/** Normalize the icaltime, so that all fields are within the normal range. */
struct icaltimetype icaltime_normalize(const struct icaltimetype t);

/** convert tt, of timezone tzid, into a utc time. Does nothing if the
   time is already UTC.  */
struct icaltimetype icaltime_convert_to_zone(const struct icaltimetype tt,
	icaltimezone *zone);

/** Return the number of days in the given month */
int icaltime_days_in_month(const int month, const int year);


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


