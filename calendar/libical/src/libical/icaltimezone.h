/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 4 -*- */
/*======================================================================
 FILE: icaltimezone.h
 CREATOR: Damon Chaplin 15 March 2001


 $Id: icaltimezone.h,v 1.10 2002/11/03 23:51:55 acampi Exp $
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
#include "icaltime.h"
#include "icalarray.h"
#include "icalcomponent.h"


#ifndef ICALTIMEZONE_DEFINED
#define ICALTIMEZONE_DEFINED
/** @brief An opaque struct representing a timezone.  
 * We declare this here to avoid a circular dependancy. 
 */
typedef struct _icaltimezone		icaltimezone;
#endif

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
void free_zone_directory(void);

/*
 * @par Debugging Output.
 */

/** Dumps information about changes in the timezone up to and including
   max_year. */
int	icaltimezone_dump_changes		(icaltimezone	*zone,
						 int		 max_year,
						 FILE		*fp);

#endif /* ICALTIMEZONE_H */
