/* -*- Mode: C; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * The contents of this file are subject to the Netscape Public License
 * Version 1.0 (the "NPL"); you may not use this file except in
 * compliance with the NPL.  You may obtain a copy of the NPL at
 * http://www.mozilla.org/NPL/
 *
 * Software distributed under the NPL is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the NPL
 * for the specific language governing rights and limitations under the
 * NPL.
 *
 * The Initial Developer of this code under the NPL is Netscape
 * Communications Corporation.  Portions created by Netscape are
 * Copyright (C) 1998 Netscape Communications Corporation.  All Rights
 * Reserved.
 */


#include <string.h>
#include <time.h>
 
#include "xp_core.h"
#include "ntypes.h"
#ifndef __XPLOCALE__
#define __XPLOCALE__

XP_BEGIN_PROTOS


/**@name Locale Sensitive Operations */
/*@{*/
 
/**
 * Collate strings according to global locale.
 *
 * Compares two strings in the global locale.
 * Returns a number less than 0 if the second string is greater, 
 * 0 if they are the same, and greater than 0 if the first string is 
 * greater, according to the sorting rules appropriate for the current 
 * locale.
 * This routine currently does not handle multiple charsets.
 * The locale is controlled by the platform through a control panel or 
 * the LC_TIME environment variable.
 *
 * @param    s1 Specifies string 1, in the global locale's charset
 * @param    s2 Specifies string 2, in the global locale's charset
 * @return   0 if s1 is equal to s2,
 *           less than 0 if s2 is greater,
 *           greater than 0 if s1 is greater
 */
PUBLIC int XP_StrColl(
    const char *s1, 
    const char *s2
);


/**
 * Constants for XP_StrfTime.
 *
 * <UL>
 * <LI>
 * XP_TIME_FORMAT - format the date/time into time format.
 * <LI>
 * XP_TIME_WEEKDAY_TIME_FORMAT - format the date/time into "weekday name plus
 *                               time" format.
 * <LI>
 * XP_DATE_TIME_FORMAT - format the date/time into "date plus time" format.
 * <LI>
 * XP_LONG_DATE_TIME_FORMAT - format the date/time into "long date plus time"
 *                            format.
 * </UL>
 */
enum XP_TIME_FORMAT_TYPE {
	XP_TIME_FORMAT = 0,
	XP_WEEKDAY_TIME_FORMAT = 1,
	XP_DATE_TIME_FORMAT = 2,
	XP_LONG_DATE_TIME_FORMAT = 3
};

/**
 * Format date/time string.
 * 
 * This routine takes a context as argument and figures out what charset the 
 * context is in. Then it formats the specified time into a string using
 * the platform's date/time formatting support. The locale is controlled by
 * the platform through a control panel or the LC_TIME environment variable.
 *
 * @param context Specifies the context to access charset information
 * @param result Returns the formatted date/time string
 * @param maxsize Specifies the size of the result buffer
 * @param format Specifies the desired format
 * @param timeptr Specifies the date/time
 * @return the size of the formatted date/time string.
 * @see INTL_ctime
 */
PUBLIC size_t XP_StrfTime(
    MWContext *context, 
    char *result, 
    size_t maxsize, 
    int format,
    const struct tm *timeptr
);

/**
 * Locale sensitive version of ANSI C ctime().
 *
 * This routine converts the specified date/time into a string using the
 * platform's date/time formatting support. The returned string is similar to
 * that returned by the ctime function, except that the locale is respected.
 * The locale is controlled by the platform through a control panel or the
 * LC_TIME environment variable.
 *
 * @param context Specifies the context to access charset information
 * @param date Specifies the date/time
 * @return the formatted date/time string in ctime style.
 * @see XP_StrfTime
 */
PUBLIC const char *INTL_ctime(
    MWContext *context, 
    time_t *date
);

/**
 * The Front End function that implements XP_StrColl.
 *
 * Don't call this API. Use XP_StrColl instead.
 *
 * @see XP_StrColl
 */
MODULE_PRIVATE extern int FE_StrColl(
    const char *s1, 
    const char *s2
);

/**
 * The Front End function that implements XP_StrfTime.
 *
 * Don't call this API. Use XP_StrfTime instead.
 *
 * @see XP_StrfTime
 */
MODULE_PRIVATE extern size_t FE_StrfTime(
    MWContext *context,  
    char *result, 
    size_t maxsize, 
    int format,
    const struct tm *timeptr
);
/*@}*/


XP_END_PROTOS

#endif
