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


/*
  xp_time.c --- parsing dates and timzones and stuff
  Created: Jamie Zawinski <jwz@netscape.com>, 3-Aug-95
*/

#include "xp.h"
#include <time.h>

XP_BEGIN_PROTOS

/* Returns the number of minutes difference between the local time and GMT.
   This takes into effect daylight savings time.  This is the value that
   should show up in outgoing mail headers, etc.
 */
extern int XP_LocalZoneOffset (void);

/* This parses a time/date string into a time_t
   (seconds after "1-Jan-1970 00:00:00 GMT")
   If it can't be parsed, 0 is returned.

   Many formats are handled, including:

     14 Apr 89 03:20:12
     14 Apr 89 03:20 GMT
     Fri, 17 Mar 89 4:01:33
     Fri, 17 Mar 89 4:01 GMT
     Mon Jan 16 16:12 PDT 1989
     Mon Jan 16 16:12 +0130 1989
     6 May 1992 16:41-JST (Wednesday)
     22-AUG-1993 10:59:12.82
     22-AUG-1993 10:59pm
     22-AUG-1993 12:59am
     22-AUG-1993 12:59 PM
     Friday, August 04, 1995 3:54 PM
     06/21/95 04:24:34 PM
     20/06/95 21:07
     95-06-08 19:32:48 EDT

  If the input string doesn't contain a description of the timezone,
  we consult the `default_to_gmt' to decide whether the string should
  be interpreted relative to the local time zone (FALSE) or GMT (TRUE).
  The correct value for this argument depends on what standard specified
  the time string which you are parsing.
 */
extern time_t XP_ParseTimeString (const char *string, XP_Bool default_to_gmt);

XP_END_PROTOS
