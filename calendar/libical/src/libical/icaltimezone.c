/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 4 -*- */
/*======================================================================
 FILE: icaltimezone.c
 CREATOR: Damon Chaplin 15 March 2001

 $Id: icaltimezone.c,v 1.34 2002/11/04 00:01:21 acampi Exp $
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

/** @file icaltimezone.c
 *  @brief implementation of timezone handling routines
 **/

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "icalproperty.h"
#include "icalarray.h"
#include "icalerror.h"
#include "icalparser.h"
#include "icaltimezone.h"

#ifdef WIN32
#define snprintf _snprintf
#define PACKAGE_DATA_DIR "/Projects/libical"
#endif

/** This is the toplevel directory where the timezone data is installed in. */
#define ZONEINFO_DIRECTORY	PACKAGE_DATA_DIR "/zoneinfo"

/** The prefix we use to uniquely identify TZIDs. */
#define TZID_PREFIX		"/softwarestudio.org/"
#define TZID_PREFIX_LEN		20

/** This is the filename of the file containing the city names and
    coordinates of all the builtin timezones. */
#define ZONES_TAB_FILENAME	"zones.tab"

/** This is the number of years of extra coverage we do when expanding
    the timezone changes. */
#define ICALTIMEZONE_EXTRA_COVERAGE	5

/** This is the maximum year we will expand to. time_t values only go up to
    somewhere around 2037. */
#define ICALTIMEZONE_MAX_YEAR		2035

struct _icaltimezone {
    char		*tzid;
    /**< The unique ID of this timezone,
       e.g. "/softwarestudio.org/Olson_20010601_1/Africa/Banjul".
       This should only be used to identify a VTIMEZONE. It is not
       meant to be displayed to the user in any form. */

    char		*location;
    /**< The location for the timezone, e.g. "Africa/Accra" for the
       Olson database. We look for this in the "LOCATION" or
       "X-LIC-LOCATION" properties of the VTIMEZONE component. It
       isn't a standard property yet. This will be NULL if no location
       is found in the VTIMEZONE. */

    char		*tznames;
    /**< This will be set to a combination of the TZNAME properties
       from the last STANDARD and DAYLIGHT components in the
       VTIMEZONE, e.g. "EST/EDT".  If they both use the same TZNAME,
       or only one type of component is found, then only one TZNAME
       will appear, e.g. "AZOT". If no TZNAME is found this will be
       NULL. */

    double		 latitude;
    double		 longitude;
    /**< The coordinates of the city, in degrees. */

    icalcomponent	*component;
    /**< The toplevel VTIMEZONE component loaded from the .ics file for this
         timezone. If we need to regenerate the changes data we need this. */

    icaltimezone	*builtin_timezone;
    /**< If this is not NULL it points to the builtin icaltimezone
       that the above TZID refers to. This icaltimezone should be used
       instead when accessing the timezone changes data, so that the
       expanded timezone changes data is shared between calendar
       components. */

    int			 end_year;
    /**< This is the last year for which we have expanded the data to.
       If we need to calculate a date past this we need to expand the
       timezone component data from scratch. */

    icalarray		*changes;
    /**< A dynamically-allocated array of time zone changes, sorted by the
       time of the change in local time. So we can do fast binary-searches
       to convert from local time to UTC. */
};

typedef struct _icaltimezonechange	icaltimezonechange;

struct _icaltimezonechange {
    int		 utc_offset;
    /**< The offset to add to UTC to get local time, in seconds. */

    int		 prev_utc_offset;
    /**< The offset to add to UTC, before this change, in seconds. */

    int		 year;		/**< Actual year, e.g. 2001. */
    int		 month;		/**< 1 (Jan) to 12 (Dec). */
    int		 day;
    int		 hour;
    int		 minute;
    int		 second;
    /**< The time that the change came into effect, in UTC.
       Note that the prev_utc_offset applies to this local time,
       since we haven't changed to the new offset yet. */

    int		 is_daylight;
    /**< Whether this is STANDARD or DAYLIGHT time. */
};


/** An array of icaltimezones for the builtin timezones. */
static icalarray *builtin_timezones = NULL;

/** This is the special UTC timezone, which isn't in builtin_timezones. */
static icaltimezone utc_timezone = { 0, 0, 0, 0, 0, 0, 0, 0, 0 };

static char* zone_files_directory = NULL;

static void  icaltimezone_reset			(icaltimezone   *zone);
static char* icaltimezone_get_location_from_vtimezone (icalcomponent *component);
static char* icaltimezone_get_tznames_from_vtimezone (icalcomponent *component);
static void  icaltimezone_expand_changes	(icaltimezone	*zone,
						 int		 end_year);
static void  icaltimezone_expand_vtimezone	(icalcomponent	*comp,
						 int		 end_year,
						 icalarray	*changes);
static int   icaltimezone_compare_change_fn	(const void	*elem1,
						 const void	*elem2);

static int   icaltimezone_find_nearby_change	(icaltimezone	*zone,
						 icaltimezonechange *change);

static void  icaltimezone_adjust_change		(icaltimezonechange *tt,
						 int		 days,
						 int		 hours,
						 int		 minutes,
						 int		 seconds);

static void  icaltimezone_init			(icaltimezone	*zone);

/** Gets the TZID, LOCATION/X-LIC-LOCATION, and TZNAME properties from the
   VTIMEZONE component and places them in the icaltimezone. It returns 1 on
   success, or 0 if the TZID can't be found. */
static int   icaltimezone_get_vtimezone_properties (icaltimezone  *zone,
						    icalcomponent *component);


static void  icaltimezone_load_builtin_timezone	(icaltimezone	*zone);

static void  icaltimezone_ensure_coverage	(icaltimezone	*zone,
						 int		 end_year);


static void  icaltimezone_init_builtin_timezones(void);

static void  icaltimezone_parse_zone_tab	(void);

static char* icaltimezone_load_get_line_fn	(char		*s,
						 size_t		 size,
						 void		*data);

static void  format_utc_offset			(int		 utc_offset,
						 char		*buffer);

static char* get_zone_directory(void);


/** Creates a new icaltimezone. */
icaltimezone*
icaltimezone_new			(void)
{
    icaltimezone *zone;

    zone = (icaltimezone*) malloc (sizeof (icaltimezone));
    if (!zone) {
	icalerror_set_errno (ICAL_NEWFAILED_ERROR);
	return NULL;
    }

    icaltimezone_init (zone);

    return zone;
}


/** Frees all memory used for the icaltimezone. */
void
icaltimezone_free			(icaltimezone *zone,
					 int	       free_struct)
{
    icaltimezone_reset (zone);
    if (free_struct)
	free (zone);
}


/** Resets the icaltimezone to the initial state, freeing most of the fields. */
static void
icaltimezone_reset			(icaltimezone *zone)
{
    if (zone->tzid)
		free (zone->tzid);
    if (zone->location)
		free (zone->location);
    if (zone->tznames)
		free (zone->tznames);
    if (zone->component)
		icalcomponent_free (zone->component);
    if (zone->changes)
		icalarray_free (zone->changes);
	
    icaltimezone_init (zone);
}


/** Initializes an icaltimezone. */
static void
icaltimezone_init			(icaltimezone	*zone)
{
    zone->tzid = NULL;
    zone->location = NULL;
    zone->tznames = NULL;
    zone->latitude = 0.0;
    zone->longitude = 0.0;
    zone->component = NULL;
    zone->builtin_timezone = NULL;
    zone->end_year = 0;
    zone->changes = NULL;
}


/** Gets the TZID, LOCATION/X-LIC-LOCATION and TZNAME properties of
   the VTIMEZONE component and stores them in the icaltimezone.  It
   returns 1 on success, or 0 if the TZID can't be found.  Note that
   it expects the zone to be initialized or reset - it doesn't free
   any old values. */
static int
icaltimezone_get_vtimezone_properties	(icaltimezone	*zone,
					 icalcomponent	*component)
{
    icalproperty *prop;
    const char *tzid;
 
    prop = icalcomponent_get_first_property (component, ICAL_TZID_PROPERTY);
    if (!prop)
	return 0;

    /* A VTIMEZONE MUST have a TZID, or a lot of our code won't work. */
    tzid = icalproperty_get_tzid (prop);
    if (!tzid)
	return 0;

    zone->tzid = strdup (tzid);
    zone->component = component;
	if ( zone->location != 0 ) free ( zone->location );
    zone->location = icaltimezone_get_location_from_vtimezone (component);
    zone->tznames = icaltimezone_get_tznames_from_vtimezone (component);

    return 1;
}

/** Gets the LOCATION or X-LIC-LOCATION property from a VTIMEZONE. */
static char*
icaltimezone_get_location_from_vtimezone (icalcomponent *component)
{
    icalproperty *prop;
    const char *location;
    const char *name;

    prop = icalcomponent_get_first_property (component,
					     ICAL_LOCATION_PROPERTY);
    if (prop) {
	location = icalproperty_get_location (prop);
	if (location)
	    return strdup (location);
    }

    prop = icalcomponent_get_first_property (component, ICAL_X_PROPERTY);
    while (prop) {
	name = icalproperty_get_x_name (prop);
	if (name && !strcmp (name, "X-LIC-LOCATION")) {
	    location = icalproperty_get_x (prop);
	    if (location)
		return strdup (location);
	}
	prop = icalcomponent_get_next_property (component,
						ICAL_X_PROPERTY);
    }

    return NULL;
}


/** Gets the TZNAMEs used for the last STANDARD & DAYLIGHT components
   in a VTIMEZONE. If both STANDARD and DAYLIGHT components use the
   same TZNAME, it returns that. If they use different TZNAMEs, it
   formats them like "EST/EDT". The returned string should be freed by
   the caller. */
static char*
icaltimezone_get_tznames_from_vtimezone (icalcomponent *component)
{
    icalcomponent *comp;
    icalcomponent_kind type;
    icalproperty *prop;
    struct icaltimetype dtstart;
    struct icaldatetimeperiodtype rdate;
    const char *current_tzname;
    const char *standard_tzname = NULL, *daylight_tzname = NULL;
    struct icaltimetype standard_max_date, daylight_max_date;
    struct icaltimetype current_max_date;

    standard_max_date = icaltime_null_time();
    daylight_max_date = icaltime_null_time();

    /* Step through the STANDARD & DAYLIGHT subcomponents. */
    comp = icalcomponent_get_first_component (component, ICAL_ANY_COMPONENT);
    while (comp) {
	type = icalcomponent_isa (comp);
	if (type == ICAL_XSTANDARD_COMPONENT
	    || type == ICAL_XDAYLIGHT_COMPONENT) {
	    current_max_date = icaltime_null_time ();
	    current_tzname = NULL;

	    /* Step through the properties. We want to find the TZNAME, and
	       the largest DTSTART or RDATE. */
	    prop = icalcomponent_get_first_property (comp, ICAL_ANY_PROPERTY);
	    while (prop) {
		switch (icalproperty_isa (prop)) {
		case ICAL_TZNAME_PROPERTY:
		    current_tzname = icalproperty_get_tzname (prop);
		    break;

		case ICAL_DTSTART_PROPERTY:
		    dtstart = icalproperty_get_dtstart (prop);
		    if (icaltime_compare (dtstart, current_max_date) > 0)
			current_max_date = dtstart;

		    break;

		case ICAL_RDATE_PROPERTY:
		    rdate = icalproperty_get_rdate (prop);
		    if (icaltime_compare (rdate.time, current_max_date) > 0)
			current_max_date = rdate.time;

		    break;

		default:
		    break;
		}

		prop = icalcomponent_get_next_property (comp,
							ICAL_ANY_PROPERTY);
	    }

	    if (current_tzname) {
		if (type == ICAL_XSTANDARD_COMPONENT) {
		    if (!standard_tzname
			|| icaltime_compare (current_max_date,
					     standard_max_date) > 0) {
			standard_max_date = current_max_date;
			standard_tzname = current_tzname;
		    }
		} else {
		    if (!daylight_tzname
			|| icaltime_compare (current_max_date,
					     daylight_max_date) > 0) {
			daylight_max_date = current_max_date;
			daylight_tzname = current_tzname;
		    }
		}
	    }
	}

        comp = icalcomponent_get_next_component (component,
						 ICAL_ANY_COMPONENT);
    }

    /* Outlook (2000) places "Standard Time" and "Daylight Time" in the TZNAME
       strings, which is totally useless. So we return NULL in that case. */
    if (standard_tzname && !strcmp (standard_tzname, "Standard Time"))
	return NULL;

    /* If both standard and daylight TZNAMEs were found, if they are the same
       we return just one, else we format them like "EST/EDT". */
    if (standard_tzname && daylight_tzname) {
	unsigned int standard_len, daylight_len;
	char *tznames;

	if (!strcmp (standard_tzname, daylight_tzname))
	    return strdup (standard_tzname);

	standard_len = strlen (standard_tzname);
	daylight_len = strlen (daylight_tzname);
	tznames = malloc (standard_len + daylight_len + 2);
	strcpy (tznames, standard_tzname);
	tznames[standard_len] = '/';
	strcpy (tznames + standard_len + 1, daylight_tzname);
	return tznames;
    } else {
	const char *tznames;

	/* If either of the TZNAMEs was found just return that, else NULL. */
	tznames = standard_tzname ? standard_tzname : daylight_tzname;
	return tznames ? strdup (tznames) : NULL;
    }
}


static void
icaltimezone_ensure_coverage		(icaltimezone	*zone,
					 int		 end_year)
{
    /* When we expand timezone changes we always expand at least up to this
       year, plus ICALTIMEZONE_EXTRA_COVERAGE. */
    static int icaltimezone_minimum_expansion_year = -1;

    int changes_end_year;

    if (!zone->component)
	icaltimezone_load_builtin_timezone (zone);

    if (icaltimezone_minimum_expansion_year == -1) {
	struct icaltimetype today = icaltime_today();
	icaltimezone_minimum_expansion_year = today.year;
    }

    changes_end_year = end_year;
    if (changes_end_year < icaltimezone_minimum_expansion_year)
	changes_end_year = icaltimezone_minimum_expansion_year;

    changes_end_year += ICALTIMEZONE_EXTRA_COVERAGE;

    if (changes_end_year > ICALTIMEZONE_MAX_YEAR)
	changes_end_year = ICALTIMEZONE_MAX_YEAR;

    if (!zone->changes || zone->end_year < end_year)
	icaltimezone_expand_changes (zone, changes_end_year);
}


static void
icaltimezone_expand_changes		(icaltimezone	*zone,
					 int		 end_year)
{
    icalarray *changes;
    icalcomponent *comp;

#if 0
    printf ("\nExpanding changes for: %s to year: %i\n", zone->tzid, end_year);
#endif

    changes = icalarray_new (sizeof (icaltimezonechange), 32);
    if (!changes)
	return;

    /* Scan the STANDARD and DAYLIGHT subcomponents. */
    comp = icalcomponent_get_first_component (zone->component,
					      ICAL_ANY_COMPONENT);
    while (comp) {
	icaltimezone_expand_vtimezone (comp, end_year, changes);
	comp = icalcomponent_get_next_component (zone->component,
						 ICAL_ANY_COMPONENT);
    }

    /* Sort the changes. We may have duplicates but I don't think it will
       matter. */
    icalarray_sort (changes, icaltimezone_compare_change_fn);

    if (zone->changes)
	icalarray_free (zone->changes);

    zone->changes = changes;
    zone->end_year = end_year;
}


static void
icaltimezone_expand_vtimezone		(icalcomponent	*comp,
					 int		 end_year,
					 icalarray	*changes)
{
    icaltimezonechange change;
    icalproperty *prop;
    struct icaltimetype dtstart, occ;
    struct icalrecurrencetype rrule;
    icalrecur_iterator* rrule_iterator;
    struct icaldatetimeperiodtype rdate;
    int found_dtstart = 0, found_tzoffsetto = 0, found_tzoffsetfrom = 0;
    int has_recurrence = 0;

    /* First we check if it is a STANDARD or DAYLIGHT component, and
       just return if it isn't. */
    if (icalcomponent_isa (comp) == ICAL_XSTANDARD_COMPONENT)
	change.is_daylight = 0;
    else if (icalcomponent_isa (comp) == ICAL_XDAYLIGHT_COMPONENT)
	change.is_daylight = 1;
    else 
	return;

    /* Step through each of the properties to find the DTSTART,
       TZOFFSETFROM and TZOFFSETTO. We can't expand recurrences here
       since we need these properties before we can do that. */
    prop = icalcomponent_get_first_property (comp, ICAL_ANY_PROPERTY);
    while (prop) {
	switch (icalproperty_isa (prop)) {
	case ICAL_DTSTART_PROPERTY:
	    dtstart = icalproperty_get_dtstart (prop);
	    found_dtstart = 1;
	    break;
	case ICAL_TZOFFSETTO_PROPERTY:
	    change.utc_offset = icalproperty_get_tzoffsetto (prop);
	    /*printf ("Found TZOFFSETTO: %i\n", change.utc_offset);*/
	    found_tzoffsetto = 1;
	    break;
	case ICAL_TZOFFSETFROM_PROPERTY:
	    change.prev_utc_offset = icalproperty_get_tzoffsetfrom (prop);
	    /*printf ("Found TZOFFSETFROM: %i\n", change.prev_utc_offset);*/
	    found_tzoffsetfrom = 1;
	    break;
	case ICAL_RDATE_PROPERTY:
	case ICAL_RRULE_PROPERTY:
	    has_recurrence = 1;
	    break;
	default:
	    /* Just ignore any other properties. */
	    break;
	}

	prop = icalcomponent_get_next_property (comp, ICAL_ANY_PROPERTY);
    }

    /* If we didn't find a DTSTART, TZOFFSETTO and TZOFFSETFROM we have to
       ignore the component. FIXME: Add an error property? */
    if (!found_dtstart || !found_tzoffsetto || !found_tzoffsetfrom)
	return;

#if 0
    printf ("\n Expanding component DTSTART (Y/M/D): %i/%i/%i %i:%02i:%02i\n",
	    dtstart.year, dtstart.month, dtstart.day,
	    dtstart.hour, dtstart.minute, dtstart.second);
#endif

    /* If the STANDARD/DAYLIGHT component has no recurrence data, we just add
       a single change for the DTSTART. */
    if (!has_recurrence) {
	change.year   = dtstart.year;
	change.month  = dtstart.month;
	change.day    = dtstart.day;
	change.hour   = dtstart.hour;
	change.minute = dtstart.minute;
	change.second = dtstart.second;

	/* Convert to UTC. */
	icaltimezone_adjust_change (&change, 0, 0, 0, -change.prev_utc_offset);

#if 0
	printf ("  Appending single DTSTART (Y/M/D): %i/%02i/%02i %i:%02i:%02i\n",
		change.year, change.month, change.day,
		change.hour, change.minute, change.second);
#endif

	/* Add the change to the array. */
	icalarray_append (changes, &change);
	return;
    }

    /* The component has recurrence data, so we expand that now. */
    prop = icalcomponent_get_first_property (comp, ICAL_ANY_PROPERTY);
    while (prop) {
#if 0
	printf ("Expanding property...\n");
#endif
	switch (icalproperty_isa (prop)) {
	case ICAL_RDATE_PROPERTY:
	    rdate = icalproperty_get_rdate (prop);
	    change.year   = rdate.time.year;
	    change.month  = rdate.time.month;
	    change.day    = rdate.time.day;
	    /* RDATEs with a DATE value inherit the time from
	       the DTSTART. */
	    if (icaltime_is_date(rdate.time)) {
		change.hour   = dtstart.hour;
		change.minute = dtstart.minute;
		change.second = dtstart.second;
	    } else {
		change.hour   = rdate.time.hour;
		change.minute = rdate.time.minute;
		change.second = rdate.time.second;

		/* The spec was a bit vague about whether RDATEs were in local
		   time or UTC so we support both to be safe. So if it is in
		   UTC we have to add the UTC offset to get a local time. */
		if (!icaltime_is_utc(rdate.time))
		    icaltimezone_adjust_change (&change, 0, 0, 0,
						-change.prev_utc_offset);
	    }

#if 0
	    printf ("  Appending RDATE element (Y/M/D): %i/%02i/%02i %i:%02i:%02i\n",
		    change.year, change.month, change.day,
		    change.hour, change.minute, change.second);
#endif

	    icalarray_append (changes, &change);
	    break;
	case ICAL_RRULE_PROPERTY:
	    rrule = icalproperty_get_rrule (prop);

	    /* If the rrule UNTIL value is set and is in UTC, we convert it to
	       a local time, since the recurrence code has no way to convert
	       it itself. */
	    if (!icaltime_is_null_time (rrule.until) && rrule.until.is_utc) {
#if 0
		printf ("  Found RRULE UNTIL in UTC.\n");
#endif

		/* To convert from UTC to a local time, we use the TZOFFSETFROM
		   since that is the offset from UTC that will be in effect
		   when each of the RRULE occurrences happens. */
		icaltime_adjust (&rrule.until, 0, 0, 0,
				 change.prev_utc_offset);
		rrule.until.is_utc = 0;
	    }

	    rrule_iterator = icalrecur_iterator_new (rrule, dtstart);
	    for (;;) {
		occ = icalrecur_iterator_next (rrule_iterator);
		if (occ.year > end_year || icaltime_is_null_time (occ))
		    break;

		change.year   = occ.year;
		change.month  = occ.month;
		change.day    = occ.day;
		change.hour   = occ.hour;
		change.minute = occ.minute;
		change.second = occ.second;

#if 0
		printf ("  Appending RRULE element (Y/M/D): %i/%02i/%02i %i:%02i:%02i\n",
			change.year, change.month, change.day,
			change.hour, change.minute, change.second);
#endif

		icaltimezone_adjust_change (&change, 0, 0, 0,
					    -change.prev_utc_offset);

		icalarray_append (changes, &change);
	    }

	    icalrecur_iterator_free (rrule_iterator);
	    break;
	default:
	    break;
	}

	prop = icalcomponent_get_next_property (comp, ICAL_ANY_PROPERTY);
    }
}


/** A function to compare 2 icaltimezonechange elements, used for qsort(). */
static int
icaltimezone_compare_change_fn		(const void	*elem1,
					 const void	*elem2)
{
    const icaltimezonechange *change1, *change2;
    int retval;

    change1 = elem1;
    change2 = elem2;

    if (change1->year < change2->year)
	retval = -1;
    else if (change1->year > change2->year)
	retval = 1;

    else if (change1->month < change2->month)
	retval = -1;
    else if (change1->month > change2->month)
	retval = 1;

    else if (change1->day < change2->day)
	retval = -1;
    else if (change1->day > change2->day)
	retval = 1;

    else if (change1->hour < change2->hour)
	retval = -1;
    else if (change1->hour > change2->hour)
	retval = 1;

    else if (change1->minute < change2->minute)
	retval = -1;
    else if (change1->minute > change2->minute)
	retval = 1;

    else if (change1->second < change2->second)
	retval = -1;
    else if (change1->second > change2->second)
	retval = 1;

    else
	retval = 0;

    return retval;
}



void
icaltimezone_convert_time		(struct icaltimetype *tt,
					 icaltimezone	*from_zone,
					 icaltimezone	*to_zone)
{
    int utc_offset, is_daylight;

    /* If the time is a DATE value or both timezones are the same, or we are
       converting a floating time, we don't need to do anything. */
    if (icaltime_is_date(*tt) || from_zone == to_zone || from_zone == NULL)
	return;

    /* Convert the time to UTC by getting the UTC offset and subtracting it. */
    utc_offset = icaltimezone_get_utc_offset (from_zone, tt, NULL);
    icaltime_adjust (tt, 0, 0, 0, -utc_offset);

    /* Now we convert the time to the new timezone by getting the UTC offset
       of our UTC time and adding it. */       
    utc_offset = icaltimezone_get_utc_offset_of_utc_time (to_zone, tt,
							  &is_daylight);
    tt->is_daylight = is_daylight;
    icaltime_adjust (tt, 0, 0, 0, utc_offset);
}




/** @deprecated This API wasn't updated when we changed icaltimetype to contain its own
    timezone. Also, this takes a pointer instead of the struct. */
/* Calculates the UTC offset of a given local time in the given
   timezone.  It is the number of seconds to add to UTC to get local
   time.  The is_daylight flag is set to 1 if the time is in
   daylight-savings time. */
int
icaltimezone_get_utc_offset		(icaltimezone	*zone,
					 struct icaltimetype	*tt,
					 int		*is_daylight)
{
    icaltimezonechange *zone_change, *prev_zone_change, tt_change, tmp_change;
    int change_num, step, utc_offset_change, cmp;
    int change_num_to_use;
    int want_daylight;

    if (tt == NULL)
	return 0;

    if (is_daylight)
	*is_daylight = 0;

    /* For local times and UTC return 0. */
    if (zone == NULL || zone == &utc_timezone)
	return 0;

    /* Use the builtin icaltimezone if possible. */
    if (zone->builtin_timezone)
	zone = zone->builtin_timezone;

    /* Make sure the changes array is expanded up to the given time. */
    icaltimezone_ensure_coverage (zone, tt->year);

    if (!zone->changes || zone->changes->num_elements == 0)
	return 0;

    /* Copy the time parts of the icaltimetype to an icaltimezonechange so we
       can use our comparison function on it. */
    tt_change.year   = tt->year;
    tt_change.month  = tt->month;
    tt_change.day    = tt->day;
    tt_change.hour   = tt->hour;
    tt_change.minute = tt->minute;
    tt_change.second = tt->second;

    /* This should find a change close to the time, either the change before
       it or the change after it. */
    change_num = icaltimezone_find_nearby_change (zone, &tt_change);

    /* Sanity check. */
    icalerror_assert (change_num >= 0,
		      "Negative timezone change index");
    icalerror_assert (change_num < zone->changes->num_elements,
		      "Timezone change index out of bounds");

    /* Now move backwards or forwards to find the timezone change that applies
       to tt. It should only have to do 1 or 2 steps. */
    zone_change = icalarray_element_at (zone->changes, change_num);
    step = 1;
    change_num_to_use = -1;
    for (;;) {
	/* Copy the change, so we can adjust it. */
	tmp_change = *zone_change;

	/* If the clock is going backward, check if it is in the region of time
	   that is used twice. If it is, use the change with the daylight
	   setting which matches tt, or use standard if we don't know. */
	if (tmp_change.utc_offset < tmp_change.prev_utc_offset) {
	    /* If the time change is at 2:00AM local time and the clock is
	       going back to 1:00AM we adjust the change to 1:00AM. We may
	       have the wrong change but we'll figure that out later. */
	    icaltimezone_adjust_change (&tmp_change, 0, 0, 0,
					tmp_change.utc_offset);
	} else {
	    icaltimezone_adjust_change (&tmp_change, 0, 0, 0,
					tmp_change.prev_utc_offset);
	}

	cmp = icaltimezone_compare_change_fn (&tt_change, &tmp_change);

	/* If the given time is on or after this change, then this change may
	   apply, but we continue as a later change may be the right one.
	   If the given time is before this change, then if we have already
	   found a change which applies we can use that, else we need to step
	   backwards. */
	if (cmp >= 0)
	    change_num_to_use = change_num;
	else
	    step = -1;

	/* If we are stepping backwards through the changes and we have found
	   a change that applies, then we know this is the change to use so
	   we exit the loop. */
	if (step == -1 && change_num_to_use != -1)
	    break;

	change_num += step;

	/* If we go past the start of the changes array, then we have no data
	   for this time so we return a UTC offset of 0. */
	if (change_num < 0)
	    return 0;

	if (change_num >= zone->changes->num_elements)
	    break;

	zone_change = icalarray_element_at (zone->changes, change_num);
    }

    /* If we didn't find a change to use, then we have a bug! */
    icalerror_assert (change_num_to_use != -1,
		      "No applicable timezone change found");

    /* Now we just need to check if the time is in the overlapped region of
       time when clocks go back. */
    zone_change = icalarray_element_at (zone->changes, change_num_to_use);

    utc_offset_change = zone_change->utc_offset - zone_change->prev_utc_offset;
    if (utc_offset_change < 0 && change_num_to_use > 0) {
	tmp_change = *zone_change;
	icaltimezone_adjust_change (&tmp_change, 0, 0, 0,
				    tmp_change.prev_utc_offset);

	if (icaltimezone_compare_change_fn (&tt_change, &tmp_change) < 0) {
	    /* The time is in the overlapped region, so we may need to use
	       either the current zone_change or the previous one. If the
	       time has the is_daylight field set we use the matching change,
	       else we use the change with standard time. */
	    prev_zone_change = icalarray_element_at (zone->changes,
						     change_num_to_use - 1);

	    /* I was going to add an is_daylight flag to struct icaltimetype,
	       but iCalendar doesn't let us distinguish between standard and
	       daylight time anyway, so there's no point. So we just use the
	       standard time instead. */
	    want_daylight = (tt->is_daylight == 1) ? 1 : 0;

#if 0
	    if (zone_change->is_daylight == prev_zone_change->is_daylight)
		printf (" **** Same is_daylight setting\n");
#endif

	    if (zone_change->is_daylight != want_daylight
		&& prev_zone_change->is_daylight == want_daylight)
		zone_change = prev_zone_change;
	}
    }

    /* Now we know exactly which timezone change applies to the time, so
       we can return the UTC offset and whether it is a daylight time. */
    if (is_daylight)
	*is_daylight = zone_change->is_daylight;
    return zone_change->utc_offset;
}


/** @deprecated This API wasn't updated when we changed icaltimetype to contain its own
    timezone. Also, this takes a pointer instead of the struct. */
/** Calculates the UTC offset of a given UTC time in the given
   timezone.  It is the number of seconds to add to UTC to get local
   time.  The is_daylight flag is set to 1 if the time is in
   daylight-savings time. */
int
icaltimezone_get_utc_offset_of_utc_time	(icaltimezone	*zone,
					 struct icaltimetype	*tt,
					 int		*is_daylight)
{
    icaltimezonechange *zone_change, tt_change, tmp_change;
    int change_num, step, change_num_to_use;

    if (is_daylight)
	*is_daylight = 0;

    /* For local times and UTC return 0. */
    if (zone == NULL || zone == &utc_timezone)
	return 0;

    /* Use the builtin icaltimezone if possible. */
    if (zone->builtin_timezone)
	zone = zone->builtin_timezone;

    /* Make sure the changes array is expanded up to the given time. */
    icaltimezone_ensure_coverage (zone, tt->year);

    if (!zone->changes || zone->changes->num_elements == 0)
	return 0;

    /* Copy the time parts of the icaltimetype to an icaltimezonechange so we
       can use our comparison function on it. */
    tt_change.year   = tt->year;
    tt_change.month  = tt->month;
    tt_change.day    = tt->day;
    tt_change.hour   = tt->hour;
    tt_change.minute = tt->minute;
    tt_change.second = tt->second;

    /* This should find a change close to the time, either the change before
       it or the change after it. */
    change_num = icaltimezone_find_nearby_change (zone, &tt_change);

    /* Sanity check. */
    icalerror_assert (change_num >= 0,
		      "Negative timezone change index");
    icalerror_assert (change_num < zone->changes->num_elements,
		      "Timezone change index out of bounds");

    /* Now move backwards or forwards to find the timezone change that applies
       to tt. It should only have to do 1 or 2 steps. */
    zone_change = icalarray_element_at (zone->changes, change_num);
    step = 1;
    change_num_to_use = -1;
    for (;;) {
	/* Copy the change and adjust it to UTC. */
	tmp_change = *zone_change;

	/* If the given time is on or after this change, then this change may
	   apply, but we continue as a later change may be the right one.
	   If the given time is before this change, then if we have already
	   found a change which applies we can use that, else we need to step
	   backwards. */
	if (icaltimezone_compare_change_fn (&tt_change, &tmp_change) >= 0)
	    change_num_to_use = change_num;
	else
	    step = -1;

	/* If we are stepping backwards through the changes and we have found
	   a change that applies, then we know this is the change to use so
	   we exit the loop. */
	if (step == -1 && change_num_to_use != -1)
	    break;

	change_num += step;

	/* If we go past the start of the changes array, then we have no data
	   for this time so we return a UTC offset of 0. */
	if (change_num < 0)
	    return 0;

	if (change_num >= zone->changes->num_elements)
	    break;

	zone_change = icalarray_element_at (zone->changes, change_num);
    }

    /* If we didn't find a change to use, then we have a bug! */
    icalerror_assert (change_num_to_use != -1,
		      "No applicable timezone change found");

    /* Now we know exactly which timezone change applies to the time, so
       we can return the UTC offset and whether it is a daylight time. */
    zone_change = icalarray_element_at (zone->changes, change_num_to_use);
    if (is_daylight)
	*is_daylight = zone_change->is_daylight;

    return zone_change->utc_offset;
}


/** Returns the index of a timezone change which is close to the time
   given in change. */
static int
icaltimezone_find_nearby_change		(icaltimezone		*zone,
					 icaltimezonechange	*change)
{
    icaltimezonechange *zone_change;
    int lower, upper, middle, cmp;
					 
    /* Do a simple binary search. */
    lower = middle = 0;
    upper = zone->changes->num_elements;

    while (lower < upper) {
	middle = (lower + upper) / 2;
	zone_change = icalarray_element_at (zone->changes, middle);
	cmp = icaltimezone_compare_change_fn (change, zone_change);
	if (cmp == 0)
	    break;
	else if (cmp < 0)
	    upper = middle;
	else
	    lower = middle + 1;
    }

    return middle;
}




/** Adds (or subtracts) a time from a icaltimezonechange.  NOTE: This
   function is exactly the same as icaltime_adjust() except for the
   type of the first parameter. */
static void
icaltimezone_adjust_change		(icaltimezonechange *tt,
					 int		 days,
					 int		 hours,
					 int		 minutes,
					 int		 seconds)
{
    int second, minute, hour, day;
    int minutes_overflow, hours_overflow, days_overflow;
    int days_in_month;

    /* Add on the seconds. */
    second = tt->second + seconds;
    tt->second = second % 60;
    minutes_overflow = second / 60;
    if (tt->second < 0) {
	tt->second += 60;
	minutes_overflow--;
    }

    /* Add on the minutes. */
    minute = tt->minute + minutes + minutes_overflow;
    tt->minute = minute % 60;
    hours_overflow = minute / 60;
    if (tt->minute < 0) {
	tt->minute += 60;
	hours_overflow--;
    }

    /* Add on the hours. */
    hour = tt->hour + hours + hours_overflow;
    tt->hour = hour % 24;
    days_overflow = hour / 24;
    if (tt->hour < 0) {
	tt->hour += 24;
	days_overflow--;
    }

    /* Add on the days. */
    day = tt->day + days + days_overflow;
    if (day > 0) {
	for (;;) {
	    days_in_month = icaltime_days_in_month (tt->month, tt->year);
	    if (day <= days_in_month)
		break;

	    tt->month++;
	    if (tt->month >= 13) {
		tt->year++;
		tt->month = 1;
	    }

	    day -= days_in_month;
	}
    } else {
	while (day <= 0) {
	    if (tt->month == 1) {
		tt->year--;
		tt->month = 12;
	    } else {
		tt->month--;
	    }

	    day += icaltime_days_in_month (tt->month, tt->year);
	}
    }
    tt->day = day;
}


char*
icaltimezone_get_tzid			(icaltimezone	*zone)
{
    /* If this is a floating time, without a timezone, return NULL. */
    if (!zone)
	return NULL;

    if (!zone->tzid)
	icaltimezone_load_builtin_timezone (zone);

    return zone->tzid;
}


char*
icaltimezone_get_location		(icaltimezone	*zone)
{
    /* If this is a floating time, without a timezone, return NULL. */
    if (!zone)
	return NULL;

    /* Note that for builtin timezones this comes from zones.tab so we don't
       need to check the timezone is loaded here. */
    return zone->location;
}


char*
icaltimezone_get_tznames		(icaltimezone	*zone)
{
    /* If this is a floating time, without a timezone, return NULL. */
    if (!zone)
	return NULL;

    if (!zone->component)
	icaltimezone_load_builtin_timezone (zone);

    return zone->tznames;
}


/** Returns the latitude of a builtin timezone. */
double
icaltimezone_get_latitude		(icaltimezone	*zone)
{
    /* If this is a floating time, without a timezone, return 0. */
    if (!zone)
	return 0.0;

    /* Note that for builtin timezones this comes from zones.tab so we don't
       need to check the timezone is loaded here. */
    return zone->latitude;
}


/** Returns the longitude of a builtin timezone. */
double
icaltimezone_get_longitude		(icaltimezone	*zone)
{
    /* If this is a floating time, without a timezone, return 0. */
    if (!zone)
	return 0.0;

    /* Note that for builtin timezones this comes from zones.tab so we don't
       need to check the timezone is loaded here. */
    return zone->longitude;
}


/** Returns the VTIMEZONE component of a timezone. */
icalcomponent*
icaltimezone_get_component		(icaltimezone	*zone)
{
    /* If this is a floating time, without a timezone, return NULL. */
    if (!zone)
	return NULL;

    if (!zone->component)
	icaltimezone_load_builtin_timezone (zone);

    return zone->component;
}


/** Sets the VTIMEZONE component of an icaltimezone, initializing the
   tzid, location & tzname fields. It returns 1 on success or 0 on
   failure, i.e.  no TZID was found. */
int
icaltimezone_set_component		(icaltimezone	*zone,
					 icalcomponent	*comp)
{
    icaltimezone_reset (zone);
    return icaltimezone_get_vtimezone_properties (zone, comp);
}


icalarray*
icaltimezone_array_new			(void)
{
    return icalarray_new (sizeof (icaltimezone), 16);
}


void
icaltimezone_array_append_from_vtimezone (icalarray	*timezones,
					  icalcomponent	*child)
{
    icaltimezone zone;

    icaltimezone_init (&zone);
    if (icaltimezone_get_vtimezone_properties (&zone, child))
	icalarray_append (timezones, &zone);
}


void
icaltimezone_array_free			(icalarray	*timezones)
{
    icaltimezone *zone;
    int i;

	if ( timezones )
	{
		for (i = 0; i < timezones->num_elements; i++) {
		zone = icalarray_element_at (timezones, i);
		icaltimezone_free (zone, 0);
		}

		icalarray_free (timezones);
	}
}


/*
 * BUILTIN TIMEZONE HANDLING
 */


/** Returns an icalarray of icaltimezone structs, one for each builtin
   timezone.  This will load and parse the zones.tab file to get the
   timezone names and their coordinates. It will not load the
   VTIMEZONE data for any timezones. */
icalarray*
icaltimezone_get_builtin_timezones	(void)
{
    if (!builtin_timezones)
	icaltimezone_init_builtin_timezones ();

    return builtin_timezones;
}

/** Release builtin timezone memory */
void
icaltimezone_free_builtin_timezones(void)
{
	icaltimezone_array_free(builtin_timezones);
}


/** Returns a single builtin timezone, given its Olson city name. */
icaltimezone*
icaltimezone_get_builtin_timezone	(const char *location)
{
    icaltimezone *zone;
    int lower, upper, middle, cmp;
    char *zone_location;

    if (!location || !location[0])
	return NULL;

    if (!strcmp (location, "UTC"))
	return &utc_timezone;

    if (!builtin_timezones)
	icaltimezone_init_builtin_timezones ();

    /* Do a simple binary search. */
    lower = middle = 0;
    upper = builtin_timezones->num_elements;

    while (lower < upper) {
	middle = (lower + upper) / 2;
	zone = icalarray_element_at (builtin_timezones, middle);
	zone_location = icaltimezone_get_location (zone);
	cmp = strcmp (location, zone_location);
	if (cmp == 0)
	    return zone;
	else if (cmp < 0)
	    upper = middle;
	else
	    lower = middle + 1;
    }

    return NULL;
}


/** Returns a single builtin timezone, given its TZID. */
icaltimezone*
icaltimezone_get_builtin_timezone_from_tzid (const char *tzid)
{
    int num_slashes = 0;
    const char *p, *zone_tzid;
    icaltimezone *zone;

    if (!tzid || !tzid[0])
	return NULL;

    /* Check that the TZID starts with our unique prefix. */
    if (strncmp (tzid, TZID_PREFIX, TZID_PREFIX_LEN))
	return NULL;

    /* Get the location, which is after the 3rd '/' character. */
    p = tzid;
    for (p = tzid; *p; p++) {
	if (*p == '/') {
	    num_slashes++;
	    if (num_slashes == 3)
		break;
	}
    }

    if (num_slashes != 3)
	return NULL;

    p++;

    /* Now we can use the function to get the builtin timezone from the
       location string. */
    zone = icaltimezone_get_builtin_timezone (p);
    if (!zone)
	return NULL;

    /* Check that the builtin TZID matches exactly. We don't want to return
       a different version of the VTIMEZONE. */
    zone_tzid = icaltimezone_get_tzid (zone);
    if (!strcmp (zone_tzid, tzid))
	return zone;
    else
	return NULL;
}


/** Returns the special UTC timezone. */
icaltimezone*
icaltimezone_get_utc_timezone		(void)
{
    if (!builtin_timezones)
	icaltimezone_init_builtin_timezones ();

    return &utc_timezone;
}



/** This initializes the builtin timezone data, i.e. the
   builtin_timezones array and the special UTC timezone. It should be
   called before any code that uses the timezone functions. */
static void
icaltimezone_init_builtin_timezones	(void)
{
    /* Initialize the special UTC timezone. */
    utc_timezone.tzid = "UTC";

    icaltimezone_parse_zone_tab ();
}


/** This parses the zones.tab file containing the names and locations
   of the builtin timezones. It creates the builtin_timezones array
   which is an icalarray of icaltimezone structs. It only fills in the
   location, latitude and longtude fields; the rest are left
   blank. The VTIMEZONE component is loaded later if it is needed. The
   timezones in the zones.tab file are sorted by their name, which is
   useful for binary searches. */
static void
icaltimezone_parse_zone_tab		(void)
{
    char *filename;
    FILE *fp;
    char buf[1024];  /* Used to store each line of zones.tab as it is read. */
    char location[1024]; /* Stores the city name when parsing buf. */
    unsigned int filename_len;
    int latitude_degrees, latitude_minutes, latitude_seconds;
    int longitude_degrees, longitude_minutes, longitude_seconds;
    icaltimezone zone;

    icalerror_assert (builtin_timezones == NULL,
		      "Parsing zones.tab file multiple times");

    builtin_timezones = icalarray_new (sizeof (icaltimezone), 32);

    filename_len = strlen (get_zone_directory()) + strlen (ZONES_TAB_FILENAME)
	+ 2;

    filename = (char*) malloc (filename_len);
    if (!filename) {
	icalerror_set_errno(ICAL_NEWFAILED_ERROR);
	return;
    }

    snprintf (filename, filename_len, "%s/%s", get_zone_directory(),
	      ZONES_TAB_FILENAME);

    fp = fopen (filename, "r");
    free (filename);
    if (!fp) {
	icalerror_set_errno(ICAL_FILE_ERROR);
	return;
    }

    while (fgets (buf, sizeof(buf), fp)) {
	if (*buf == '#') continue;

	/* The format of each line is: "latitude longitude location". */
	if (sscanf (buf, "%4d%2d%2d %4d%2d%2d %s",
		    &latitude_degrees, &latitude_minutes,
		    &latitude_seconds,
		    &longitude_degrees, &longitude_minutes,
		    &longitude_seconds,
		    location) != 7) {
	    fprintf (stderr, "Invalid timezone description line: %s\n", buf);
	    continue;
	}

	icaltimezone_init (&zone);
	zone.location = strdup (location);

	if (latitude_degrees >= 0)
	    zone.latitude = (double) latitude_degrees
		+ (double) latitude_minutes / 60
		+ (double) latitude_seconds / 3600;
	else
	    zone.latitude = (double) latitude_degrees
		- (double) latitude_minutes / 60
		- (double) latitude_seconds / 3600;

	if (longitude_degrees >= 0)
	    zone.longitude = (double) longitude_degrees
		+ (double) longitude_minutes / 60
		+ (double) longitude_seconds / 3600;
	else
	    zone.longitude = (double) longitude_degrees
		- (double) longitude_minutes / 60
		- (double) longitude_seconds / 3600;

	icalarray_append (builtin_timezones, &zone);

#if 0
	printf ("Found zone: %s %f %f\n",
		location, zone.latitude, zone.longitude);
#endif
    }

    fclose (fp);
}


/** Loads the builtin VTIMEZONE data for the given timezone. */
static void
icaltimezone_load_builtin_timezone	(icaltimezone	*zone)
{
    char *filename;
    unsigned int filename_len;
    FILE *fp;
    icalparser *parser;
    icalcomponent *comp, *subcomp;

	    /* If the location isn't set, it isn't a builtin timezone. */
    if (!zone->location || !zone->location[0])
	return;

    filename_len = strlen (get_zone_directory()) + strlen (zone->location) + 6;

    filename = (char*) malloc (filename_len);
    if (!filename) {
	icalerror_set_errno(ICAL_NEWFAILED_ERROR);
	return;
    }

    snprintf (filename, filename_len, "%s/%s.ics", get_zone_directory(),
	      zone->location);

    fp = fopen (filename, "r");
    free (filename);
    if (!fp) {
	icalerror_set_errno(ICAL_FILE_ERROR);
	return;
    }

	
	/* ##### B.# Sun, 11 Nov 2001 04:04:29 +1100 
	this is where the MALFORMEDDATA error is being set, after the call to 'icalparser_parse'
	fprintf(stderr, "** WARNING ** %s: %d %s\n", __FILE__, __LINE__, icalerror_strerror(icalerrno));
	*/

    parser = icalparser_new ();
	icalparser_set_gen_data (parser, fp);
	comp = icalparser_parse (parser, icaltimezone_load_get_line_fn);
    icalparser_free (parser);
	fclose (fp);

	
	
    /* Find the VTIMEZONE component inside the VCALENDAR. There should be 1. */
    subcomp = icalcomponent_get_first_component (comp,
						 ICAL_VTIMEZONE_COMPONENT);
    if (!subcomp) {
	icalerror_set_errno(ICAL_PARSE_ERROR);
	return;
    }

    icaltimezone_get_vtimezone_properties (zone, subcomp);

	icalcomponent_remove_component(comp,subcomp);

	icalcomponent_free(comp);

}


/** Callback used from icalparser_parse() */
static char *
icaltimezone_load_get_line_fn		(char		*s,
					 size_t		 size,
					 void		*data)
{
    return fgets (s, size, (FILE*) data);
}




/*
 * DEBUGGING
 */

/**
 * This outputs a list of timezone changes for the given timezone to the
 * given file, up to the maximum year given. We compare this output with the
 * output from 'vzic --dump-changes' to make sure that we are consistent.
 * (vzic is the Olson timezone database to VTIMEZONE converter.)
 * 
 * The output format is:
 *
 *	Zone-Name [tab] Date [tab] Time [tab] UTC-Offset
 *
 * The Date and Time fields specify the time change in UTC.
 *
 * The UTC Offset is for local (wall-clock) time. It is the amount of time
 * to add to UTC to get local time.
 */
int
icaltimezone_dump_changes		(icaltimezone	*zone,
					 int		 max_year,
					 FILE		*fp)
{
    static char *months[] = { "Jan", "Feb", "Mar", "Apr", "May", "Jun",
			      "Jul", "Aug", "Sep", "Oct", "Nov", "Dec" };
    icaltimezonechange *zone_change;
    int change_num;
    char buffer[8];

    /* Make sure the changes array is expanded up to the given time. */
    icaltimezone_ensure_coverage (zone, max_year);

#if 0
    printf ("Num changes: %i\n", zone->changes->num_elements);
#endif

    change_num = 0;
    for (change_num = 0; change_num < zone->changes->num_elements; change_num++) {
	zone_change = icalarray_element_at (zone->changes, change_num);

	if (zone_change->year > max_year)
	    break;

	fprintf (fp, "%s\t%2i %s %04i\t%2i:%02i:%02i",
		zone->location,
		zone_change->day, months[zone_change->month - 1],
		zone_change->year,
		zone_change->hour, zone_change->minute, zone_change->second);

	/* Wall Clock Time offset from UTC. */
	format_utc_offset (zone_change->utc_offset, buffer);
	fprintf (fp, "\t%s", buffer);

	fprintf (fp, "\n");
    }
	return 1;
}


/** This formats a UTC offset as "+HHMM" or "+HHMMSS".
   buffer should have space for 8 characters. */
static void
format_utc_offset			(int		 utc_offset,
					 char		*buffer)
{
  char *sign = "+";
  int hours, minutes, seconds;

  if (utc_offset < 0) {
    utc_offset = -utc_offset;
    sign = "-";
  }

  hours = utc_offset / 3600;
  minutes = (utc_offset % 3600) / 60;
  seconds = utc_offset % 60;

  /* Sanity check. Standard timezone offsets shouldn't be much more than 12
     hours, and daylight saving shouldn't change it by more than a few hours.
     (The maximum offset is 15 hours 56 minutes at present.) */
  if (hours < 0 || hours >= 24 || minutes < 0 || minutes >= 60
      || seconds < 0 || seconds >= 60) {
    fprintf (stderr, "Warning: Strange timezone offset: H:%i M:%i S:%i\n",
	     hours, minutes, seconds);
  }

  if (seconds == 0)
    sprintf (buffer, "%s%02i%02i", sign, hours, minutes);
  else
    sprintf (buffer, "%s%02i%02i%02i", sign, hours, minutes, seconds);
}

static char* get_zone_directory(void)
{
	return zone_files_directory == NULL ? ZONEINFO_DIRECTORY : zone_files_directory;
}

void set_zone_directory(char *path)
{
	zone_files_directory = malloc(strlen(path)+1);
	if ( zone_files_directory != NULL )
	{
		strcpy(zone_files_directory,path);
	}
}

void free_zone_directory(void)
{
	if ( zone_files_directory != NULL )
	{
		free(zone_files_directory);
	}
}
