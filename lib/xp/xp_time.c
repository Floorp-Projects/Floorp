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

/* *
 * 
 *
 * xp_time.c --- parsing dates and timzones and stuff
 *
 * Created: Jamie Zawinski <jwz@netscape.com>, 3-Aug-95
 */

#include "xp_time.h"


/* Returns the number of minutes difference between the local time and GMT.
   This takes into effect daylight savings time.  This is the value that
   should show up in outgoing mail headers, etc.
 */

/* For speed, this function memorizes itself (the value is computed once, and
   cached.)  Calling time(), localtime(), and gmtime() every time we want to
   parse a date is too much overhead.

   However, our offset from GMT can change, each time the Daylight Savings
   Time state flips (twice a year.)  To avoid starting to mis-parse times if
   the same process was running both before and after the flip, we discard
   the cache once a minute.  This is done efficiently: one minute after
   the local zone offset has most recently been computed, a timer will go off,
   setting a bit invalidating the cache.  The next time we need the zone
   offset, we will recompute it.  This does NOT cause a timer to go off every
   minute no matter what; it only goes off once, a minute after Netscape has
   gone idle (or otherwise stopped needing to parse times.)

   Making the cache lifetime be longer than a minute wouldn't be a significant
   savings; it could actually be much smaller (a few seconds) without any
   performance hit.
 */

void do_timer(void* closure)
{
  void** timer = (void**) closure;
  *timer = NULL;
}

PRIVATE int
xp_internal_zone_offset (time_t date_then)
{
  struct tm local, gmt, *tm;
  int zone = 0;

  tm = localtime (&date_then);
  if (!tm) return 0;
  local = *tm;

  tm = gmtime (&date_then);
  if (!tm) return 0;
  gmt = *tm;

  /* Assume we are never more than 24 hours away. */
  zone = local.tm_yday - gmt.tm_yday;
  if (zone > 1)
	zone = -24;
  else if (zone < -1)
	zone = 24;
  else
	zone *= 24;

  /* Scale in the hours and minutes; ignore seconds. */
  zone -= gmt.tm_hour - local.tm_hour;
  zone *= 60;
  zone -= gmt.tm_min - local.tm_min;

  return zone;
}

PUBLIC int
XP_LocalZoneOffset(void)
{
  static int zone = 0;
  static void* timer = NULL;

  /* Our "invalidation timer" is still ticking, so the results of last time
     is still valid. */
  if (timer) {
    return zone;
  }

#ifdef MOZILLA_CLIENT
  timer = FE_SetTimeout(do_timer, &timer, 60L * 1000L);    /* 60 seconds */
#endif /* MOZILLA_CLIENT */

  zone = xp_internal_zone_offset(time(NULL));

  return zone;
}

PRIVATE short monthOffset[] = {
    0,          /* 31   January */
    31,         /* 28   February */
    59,         /* 31   March */
    90,         /* 30   April */
    120,            /* 31   May */
    151,            /* 30   June */
    181,            /* 31   July */
    212,            /* 31   August */
    243,            /* 30   September */
    273,            /* 31   October */
    304,            /* 30   November */
    334         /* 31   December */
    /* 365 */
};

static time_t
xp_compute_UTC_from_GMT(const struct tm *tm)
{
	int32 secs;
	int32 day;

	XP_ASSERT(tm->tm_mon > -1 && tm->tm_mon < 12);

	day = (tm->tm_mday
            + monthOffset[tm->tm_mon]
            + ((tm->tm_year & 3) != 0
               || (tm->tm_year % 100 == 0 && (tm->tm_year + 300) % 400 != 0)
               || tm->tm_mon < 2
               ? -1 : 0)/* convert day-of-month to 0 based range,
                 * except following February in a leap year,
                 * in which case we skip the conversion to
                 * account for the extra day in February */
            + (tm->tm_year - 70) * 365L    /* days per year */
            + (tm->tm_year - 69) / 4   /* plus leap days */
            - (tm->tm_year - 1) / 100  /* no leap on century years */
            + (tm->tm_year + 299) / 400);  /* except %400 years */

	if(day < 0)
		return(0);

	secs = tm->tm_sec + (60L * (tm->tm_min + (60L * tm->tm_hour)));

	if(day == 0 && secs < 0)
		return(0);
	return ((time_t) (secs + ((60L * 60L * 24L) * day)));
}


/*
I lifted this list of time zone abbreviations out of a UNIX computers setup 
file (specifically, from an AT&T StarServer running UNIX System V Release 4, 
in the /usr/lib/local/TZ directory).

The list is by no means complete or comprehensive, as much of it comes out 
of scripts designed to adjust the time on computers when Daylight Savings 
Time (DST) rolls around. Also, I would consider it at least a little 
suspect. First, because it was compiled by Americans, and you know how us 
Americans are with geography :). Second, the data looks to be a little old, 
circa 1991 (note the reference to the "Soviet Union").

The first column is an approximate name for the time zone described, the 
second column gives the time relative to GMT, the third column takes a stab 
at listing the country that the time zone is in, and the final column gives 
one or more abbreviations that apply to that time zone (note that 
abbreviations that end with "DST" or with "S" as the middle letter indicate 
Daylight Savings Time is in effect).

I've also tried to roughly divide the listings into geographical groupings.

Hope this helps,

Raymond McCauley
Texas A&M University
scooter@tamu.edu

<List follows...>
=================

Europe

Great Britain 0:00 GB-Eire GMT, BST
Western European nations +0:00 W-Eur WET, WET DST
Iceland +0:00 - WET
Middle European nations +1:00 M-Eur MET, MET DST
Poland +1:00 W-Eur MET, MET DST
Eastern European nations +2:00 E-Eur EET, EET DST
Turkey +3:00 Turkey EET, EET DST
Warsaw Pact/Soviet Union +3:00 M-Eur ????

North America

Canada/Newfoundland -3:30 Canada NST, NDT
Canada/Atlantic -4:00 Canada AST, ADT
Canada/Eastern -5:00 Canada EST, EDT
Canada/Central -6:00 Canada CST, CDT
Canada/East-Saskatchewan -6:00 Canada CST
Canada/Mountain -7:00 Canada MST, MDT
Canada/Pacific -8:00 Canada PST, PDT
Canada/Yukon -9:00 Canada YST, YDT

US/Eastern -5:00 US EST, EDT
US/Central -6:00 US CST, CDT
US/Mountain -7:00 US MST, MDT
US/Pacific -8:00 US PST, PDT
US/Yukon -9:00 US YST, YDT
US/Hawaii -10:00 US HST, PST, PDT, PPET

Mexico/BajaNorte -8:00 Mexico PST, PDT
Mexico/BajaSur -7:00 Mexico MST
Mexico/General -6:00 Mexico CST

South America

Brazil/East -3:00 Brazil EST, EDT
Brazil/West -4:00 Brazil WST, WDT
Brazil/Acre -5:00 Brazil AST, ADT
Brazil/DeNoronha -2:00 Brazil FST, FDT

Chile/Continental -4:00 Chile CST, CDT
Chile/EasterIsland -6:00 Chile EST, EDT


Asia

People's Repub. of China +8:00 PRC CST, CDT
(Yes, they really have only one time zone.)

Republic of Korea +9:00 ROK KST, KDT

Japan +9:00 Japan JST
Singapore +8:00 Singapore SST
Hongkong +8:00 U.K. HKT
ROC +8:00 - CST

Middle East

Israel +3:00 Israel IST, IDT
(This was the only listing I found)

Australia

Australia/Tasmania +10:00 Oz EST
Australia/Queensland +10:00 Oz EST
Australia/North +9:30 Oz CST
Australia/West +8:00 Oz WST
Australia/South +9:30 Oz CST



Hour    TZN     DZN     Zone    Example
0       GMT             Greenwich Mean Time             GMT0
0       UTC             Universal Coordinated Time      UTC0
2       FST     FDT     Fernando De Noronha Std         FST2FDT
3       BST             Brazil Standard Time            BST3
3       EST     EDT     Eastern Standard (Brazil)       EST3EDT
3       GST             Greenland Standard Time         GST3
3:30    NST     NDT     Newfoundland Standard Time      NST3:30NDT
4       AST     ADT     Atlantic Standard Time          AST4ADT
4       WST     WDT     Western Standard (Brazil)       WST4WDT
5       EST     EDT     Eastern Standard Time           EST5EDT
5       CST     CDT     Chile Standard Time             CST5CDT

Hour    TZN     DZN     Zone    Example
5       AST     ADT     Acre Standard Time              AST5ADT
5       CST     CDT     Cuba Standard Time              CST5CDT
6       CST     CDT     Central Standard Time           CST6CDT
6       EST     EDT     Easter Island Standard          EST6EDT
7       MST     MDT     Mountain Standard Time          MST7MDT
8       PST     PDT     Pacific Standard Time           PST8PDT
9       AKS     AKD     Alaska Standard Time            AKS9AKD
9       YST     YDT     Yukon Standard Time             YST9YST
10      HST     HDT     Hawaii Standard Time            HST10HDT
11      SST             Somoa Standard Time             SST11
-12     NZS     NZD     New Zealand Standard Time       NZS-12NZD
-10     GST             Guam Standard Time              GST-10
-10     EAS     EAD     Eastern Australian Standard     EAS-10EAD
-9:30   CAS     CAD     Central Australian Standard     CAS-9:30CAD
-9      JST             Japan Standard Time             JST-9
-9      KST     KDT     Korean Standard Time            KST-9KDT
-8      CCT             China Coast Time                CCT-8
-8      HKT             Hong Kong Time                  HKT-8
-8      SST             Singapore Standard Time         SST-8
-8      WAS     WAD     Western Australian Standard     WAS-8WAD
-7:30   JT              Java Standard Time              JST-7:30
-7      NST             North Sumatra Time              NST-7
-5:30   IST             Indian Standard Time            IST-5:30
-3:30   IST     IDT     Iran Standard Time              IST-3:30IDT
-3      MSK     MSD     Moscow Winter Time              MSK-3MSD
-2      EET             Eastern Europe Time             EET-2
-2      IST     IDT     Israel Standard Time            IST-2IDT
-1      MEZ     MES     Middle European Time            MEZ-1MES
-1      SWT     SST     Swedish Winter Time             SWT-1SST
-1      FWT     FST     French Winter Time              FWT-1FST
-1      CET     CES     Central European Time           CET-1CES
-1      WAT             West African Time               WAT-1
 */


typedef enum
{
  TT_UNKNOWN,

  TT_SUN, TT_MON, TT_TUE, TT_WED, TT_THU, TT_FRI, TT_SAT,

  TT_JAN, TT_FEB, TT_MAR, TT_APR, TT_MAY, TT_JUN,
  TT_JUL, TT_AUG, TT_SEP, TT_OCT, TT_NOV, TT_DEC,

  TT_PST, TT_PDT, TT_MST, TT_MDT, TT_CST, TT_CDT, TT_EST, TT_EDT,
  TT_AST, TT_NST, TT_GMT, TT_BST, TT_MET, TT_EET, TT_JST
} TIME_TOKEN;


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
time_t
XP_ParseTimeString (const char *string, XP_Bool default_to_gmt)
{
  struct tm tm;
  TIME_TOKEN dotw = TT_UNKNOWN;
  TIME_TOKEN month = TT_UNKNOWN;
  TIME_TOKEN zone = TT_UNKNOWN;
  int zone_offset = -1;
  int date = -1;
  int32 year = -1;
  int hour = -1;
  int min = -1;
  int sec = -1;
  time_t result;

  const char *rest = string;

#ifdef DEBUG
  int iterations = 0;
#endif

  XP_ASSERT(string);
  if (!string) return 0;

  while (*rest)
	{

#ifdef DEBUG
	  if (iterations++ > 1000)
		{
		  XP_ASSERT(0);
		  return 0;
		}
#endif

	  switch (*rest)
		{
		case 'a': case 'A':
		  if (month == TT_UNKNOWN &&
			  (rest[1] == 'p' || rest[1] == 'P') &&
			  (rest[2] == 'r' || rest[2] == 'R'))
			month = TT_APR;
		  else if (zone == TT_UNKNOWN &&
				   (rest[1] == 's' || rest[1] == 's') &&
				   (rest[2] == 't' || rest[2] == 'T'))
			zone = TT_AST;
		  else if (month == TT_UNKNOWN &&
				   (rest[1] == 'u' || rest[1] == 'U') &&
				   (rest[2] == 'g' || rest[2] == 'G'))
			month = TT_AUG;
		  break;
		case 'b': case 'B':
		  if (zone == TT_UNKNOWN &&
			  (rest[1] == 's' || rest[1] == 'S') &&
			  (rest[2] == 't' || rest[2] == 'T'))
			zone = TT_BST;
		  break;
		case 'c': case 'C':
		  if (zone == TT_UNKNOWN &&
			  (rest[1] == 'd' || rest[1] == 'D') &&
			  (rest[2] == 't' || rest[2] == 'T'))
			zone = TT_CDT;
		  else if (zone == TT_UNKNOWN &&
				   (rest[1] == 's' || rest[1] == 'S') &&
				   (rest[2] == 't' || rest[2] == 'T'))
			zone = TT_CST;
		  break;
		case 'd': case 'D':
		  if (month == TT_UNKNOWN &&
			  (rest[1] == 'e' || rest[1] == 'E') &&
			  (rest[2] == 'c' || rest[2] == 'C'))
			month = TT_DEC;
		  break;
		case 'e': case 'E':
		  if (zone == TT_UNKNOWN &&
			  (rest[1] == 'd' || rest[1] == 'D') &&
			  (rest[2] == 't' || rest[2] == 'T'))
			zone = TT_EDT;
		  else if (zone == TT_UNKNOWN &&
				   (rest[1] == 'e' || rest[1] == 'E') &&
				   (rest[2] == 't' || rest[2] == 'T'))
			zone = TT_EET;
		  else if (zone == TT_UNKNOWN &&
				   (rest[1] == 's' || rest[1] == 'S') &&
				   (rest[2] == 't' || rest[2] == 'T'))
			zone = TT_EST;
		  break;
		case 'f': case 'F':
		  if (month == TT_UNKNOWN &&
			  (rest[1] == 'e' || rest[1] == 'E') &&
			  (rest[2] == 'b' || rest[2] == 'B'))
			month = TT_FEB;
		  else if (dotw == TT_UNKNOWN &&
				   (rest[1] == 'r' || rest[1] == 'R') &&
				   (rest[2] == 'i' || rest[2] == 'I'))
			dotw = TT_FRI;
		  break;
		case 'g': case 'G':
		  if (zone == TT_UNKNOWN &&
			  (rest[1] == 'm' || rest[1] == 'M') &&
			  (rest[2] == 't' || rest[2] == 'T'))
			zone = TT_GMT;
		  break;
		case 'j': case 'J':
		  if (month == TT_UNKNOWN &&
			  (rest[1] == 'a' || rest[1] == 'A') &&
			  (rest[2] == 'n' || rest[2] == 'N'))
			month = TT_JAN;
		  else if (zone == TT_UNKNOWN &&
				   (rest[1] == 's' || rest[1] == 'S') &&
				   (rest[2] == 't' || rest[2] == 'T'))
			zone = TT_JST;
		  else if (month == TT_UNKNOWN &&
				   (rest[1] == 'u' || rest[1] == 'U') &&
				   (rest[2] == 'l' || rest[2] == 'L'))
			month = TT_JUL;
		  else if (month == TT_UNKNOWN &&
				   (rest[1] == 'u' || rest[1] == 'U') &&
				   (rest[2] == 'n' || rest[2] == 'N'))
			month = TT_JUN;
		  break;
		case 'm': case 'M':
		  if (month == TT_UNKNOWN &&
			  (rest[1] == 'a' || rest[1] == 'A') &&
			  (rest[2] == 'r' || rest[2] == 'R'))
			month = TT_MAR;
		  else if (month == TT_UNKNOWN &&
				   (rest[1] == 'a' || rest[1] == 'A') &&
				   (rest[2] == 'y' || rest[2] == 'Y'))
			month = TT_MAY;
		  else if (zone == TT_UNKNOWN &&
				   (rest[1] == 'd' || rest[1] == 'D') &&
				   (rest[2] == 't' || rest[2] == 'T'))
			zone = TT_MDT;
		  else if (zone == TT_UNKNOWN &&
				   (rest[1] == 'e' || rest[1] == 'E') &&
				   (rest[2] == 't' || rest[2] == 'T'))
			zone = TT_MET;
		  else if (dotw == TT_UNKNOWN &&
				   (rest[1] == 'o' || rest[1] == 'O') &&
				   (rest[2] == 'n' || rest[2] == 'N'))
			dotw = TT_MON;
		  else if (zone == TT_UNKNOWN &&
				   (rest[1] == 's' || rest[1] == 'S') &&
				   (rest[2] == 't' || rest[2] == 'T'))
			zone = TT_MST;
		  break;
		case 'n': case 'N':
		  if (month == TT_UNKNOWN &&
			  (rest[1] == 'o' || rest[1] == 'O') &&
			  (rest[2] == 'v' || rest[2] == 'V'))
			month = TT_NOV;
		  else if (zone == TT_UNKNOWN &&
				   (rest[1] == 's' || rest[1] == 'S') &&
				   (rest[2] == 't' || rest[2] == 'T'))
			zone = TT_NST;
		  break;
		case 'o': case 'O':
		  if (month == TT_UNKNOWN &&
			  (rest[1] == 'c' || rest[1] == 'C') &&
			  (rest[2] == 't' || rest[2] == 'T'))
			month = TT_OCT;
		  break;
		case 'p': case 'P':
		  if (zone == TT_UNKNOWN &&
			  (rest[1] == 'd' || rest[1] == 'D') &&
			  (rest[2] == 't' || rest[2] == 'T'))
			zone = TT_PDT;
		  else if (zone == TT_UNKNOWN &&
				   (rest[1] == 's' || rest[1] == 'S') &&
				   (rest[2] == 't' || rest[2] == 'T'))
			zone = TT_PST;
		  break;
		case 's': case 'S':
		  if (dotw == TT_UNKNOWN &&
			  (rest[1] == 'a' || rest[1] == 'A') &&
			  (rest[2] == 't' || rest[2] == 'T'))
			dotw = TT_SAT;
		  else if (month == TT_UNKNOWN &&
				   (rest[1] == 'e' || rest[1] == 'E') &&
				   (rest[2] == 'p' || rest[2] == 'P'))
			month = TT_SEP;
		  else if (dotw == TT_UNKNOWN &&
				   (rest[1] == 'u' || rest[1] == 'U') &&
				   (rest[2] == 'n' || rest[2] == 'N'))
			dotw = TT_SUN;
		  break;
		case 't': case 'T':
		  if (dotw == TT_UNKNOWN &&
			  (rest[1] == 'h' || rest[1] == 'H') &&
			  (rest[2] == 'u' || rest[2] == 'U'))
			dotw = TT_THU;
		  else if (dotw == TT_UNKNOWN &&
				   (rest[1] == 'u' || rest[1] == 'U') &&
				   (rest[2] == 'e' || rest[2] == 'E'))
			dotw = TT_TUE;
		  break;
		case 'u': case 'U':
		  if (zone == TT_UNKNOWN &&
			  (rest[1] == 't' || rest[1] == 'T') &&
			  !(rest[2] >= 'A' && rest[2] <= 'Z') &&
			  !(rest[2] >= 'a' && rest[2] <= 'z'))
			/* UT is the same as GMT but UTx is not. */
			zone = TT_GMT;
		  break;
		case 'w': case 'W':
		  if (dotw == TT_UNKNOWN &&
			  (rest[1] == 'e' || rest[1] == 'E') &&
			  (rest[2] == 'd' || rest[2] == 'D'))
			dotw = TT_WED;
		  break;

		case '+': case '-':
		  {
			const char *end;
			int sign;
			if (zone_offset >= 0)
			  {
				/* already got one... */
				rest++;
				break;
			  }
			if (zone != TT_UNKNOWN && zone != TT_GMT)
			  {
				/* GMT+0300 is legal, but PST+0300 is not. */
				rest++;
				break;
			  }

			sign = ((*rest == '+') ? 1 : -1);
			rest++; /* move over sign */
			end = rest;
			while (*end >= '0' && *end <= '9')
			  end++;
			if (rest == end) /* no digits here */
			  break;

			if ((end - rest) == 4)
			  /* offset in HHMM */
			  zone_offset = (((((rest[0]-'0')*10) + (rest[1]-'0')) * 60) +
							 (((rest[2]-'0')*10) + (rest[3]-'0')));
			else if ((end - rest) == 2)
			  /* offset in hours */
			  zone_offset = (((rest[0]-'0')*10) + (rest[1]-'0')) * 60;
			else if ((end - rest) == 1)
			  /* offset in hours */
			  zone_offset = (rest[0]-'0') * 60;
			else
			  /* 3 or >4 */
			  break;

			zone_offset *= sign;
			zone = TT_GMT;
			break;
		  }

		case '0': case '1': case '2': case '3': case '4':
		case '5': case '6': case '7': case '8': case '9':
		  {
			int tmp_hour = -1;
			int tmp_min = -1;
			int tmp_sec = -1;
			const char *end = rest + 1;
			while (*end >= '0' && *end <= '9')
			  end++;

			/* end is now the first character after a range of digits. */

			if (*end == ':')
			  {
				if (hour > 0 && min > 0) /* already got it */
				  break;

				/* We have seen "[0-9]+:", so this is probably HH:MM[:SS] */
				if ((end - rest) > 2)
				  /* it is [0-9][0-9][0-9]+: */
				  break;
				else if (rest[1] != ':' &&
						 rest[2] != ':')
				  /* it is not [0-9]: or [0-9][0-9]: */
				  break;
				else if ((end - rest) == 2)
				  tmp_hour = ((rest[0]-'0')*10 +
							  (rest[1]-'0'));
				else
				  tmp_hour = (rest[0]-'0');

				while (*rest && *rest != ':')
				  rest++;
				rest++;

				/* move over the colon, and parse minutes */

				end = rest + 1;
				while (*end >= '0' && *end <= '9')
				  end++;

				if (end == rest)
				  /* no digits after first colon? */
				  break;
				else if ((end - rest) > 2)
				  /* it is [0-9][0-9][0-9]+: */
				  break;
				else if ((end - rest) == 2)
				  tmp_min = ((rest[0]-'0')*10 +
							 (rest[1]-'0'));
				else
				  tmp_min = (rest[0]-'0');

				/* now go for seconds */
				rest = end;
				if (*rest == ':')
				  rest++;
				end = rest;
				while (*end >= '0' && *end <= '9')
				  end++;

				if (end == rest)
				  /* no digits after second colon - that's ok. */
				  ;
				else if ((end - rest) > 2)
				  /* it is [0-9][0-9][0-9]+: */
				  break;
				else if ((end - rest) == 2)
				  tmp_sec = ((rest[0]-'0')*10 +
							 (rest[1]-'0'));
				else
				  tmp_sec = (rest[0]-'0');

				/* If we made it here, we've parsed hour and min,
				   and possibly sec, so it worked as a unit. */

				/* skip over whitespace and see if there's an AM or PM
				   directly following the time.
				 */
				if (tmp_hour <= 12)
				  {
					const char *s = end;
					while (*s && (*s == ' ' || *s == '\t'))
					  s++;
					if ((s[0] == 'p' || s[0] == 'P') &&
						(s[1] == 'm' || s[1] == 'M'))
					  /* 10:05pm == 22:05, and 12:05pm == 12:05 */
					  tmp_hour = (tmp_hour == 12 ? 12 : tmp_hour + 12);
					else if (tmp_hour == 12 &&
							 (s[0] == 'a' || s[0] == 'A') &&
							 (s[1] == 'm' || s[1] == 'M'))
					  /* 12:05am == 00:05 */
					  tmp_hour = 0;
				  }

				hour = tmp_hour;
				min = tmp_min;
				sec = tmp_sec;
				rest = end;
				break;
			  }
			else if ((*end == '/' || *end == '-') &&
					 end[1] >= '0' && end[1] <= '9')
			  {
				/* Perhaps this is 6/16/95, 16/6/95, 6-16-95, or 16-6-95
				   or even 95-06-05...
				   #### But it doesn't handle 1995-06-22.
				 */
				int n1, n2, n3;
				const char *s;

				if (month != TT_UNKNOWN)
				  /* if we saw a month name, this can't be. */
				  break;

				s = rest;

				n1 = (*s++ - '0');				/* first 1 or 2 digits */
				if (*s >= '0' && *s <= '9')
				  n1 = n1*10 + (*s++ - '0');

				if (*s != '/' && *s != '-')		/* slash */
				  break;
				s++;

				if (*s < '0' || *s > '9')		/* second 1 or 2 digits */
				  break;
				n2 = (*s++ - '0');
				if (*s >= '0' && *s <= '9')
				  n2 = n2*10 + (*s++ - '0');

				if (*s != '/' && *s != '-')		/* slash */
				  break;
				s++;

				if (*s < '0' || *s > '9')		/* third 1, 2, or 4 digits */
				  break;
				n3 = (*s++ - '0');
				if (*s >= '0' && *s <= '9')
				  n3 = n3*10 + (*s++ - '0');

				if (*s >= '0' && *s <= '9')		/* optional digits 3 and 4 */
				  {
					n3 = n3*10 + (*s++ - '0');
					if (*s < '0' || *s > '9')
					  break;
					n3 = n3*10 + (*s++ - '0');
				  }

				if ((*s >= '0' && *s <= '9') ||	/* followed by non-alphanum */
					(*s >= 'A' && *s <= 'Z') ||
					(*s >= 'a' && *s <= 'z'))
				  break;

				/* Ok, we parsed three 1-2 digit numbers, with / or -
				   between them.  Now decide what the hell they are
				   (DD/MM/YY or MM/DD/YY or YY/MM/DD.)
				 */

				if (n1 > 70)	/* must be YY/MM/DD */
				  {
					if (n2 > 12) break;
					if (n3 > 31) break;
					year = n1;
					if (year < 1900) year += 1900;
					month = (TIME_TOKEN)(n2 + ((int)TT_JAN) - 1);
					date = n3;
					rest = s;
					break;
				  }

				if (n3 < 70 ||		/* before epoch - can't represent it. */
					(n1 > 12 && n2 > 12))	/* illegal */
				  {
					rest = s;
					break;
				  }

				if (n3 < 1900) n3 += 1900;

				if (n1 > 12)  /* must be DD/MM/YY */
				  {
					date = n1;
					month = (TIME_TOKEN)(n2 + ((int)TT_JAN) - 1);
					year = n3;
				  }
				else		  /* assume MM/DD/YY */
				  {
					/* #### In the ambiguous case, should we consult the
					   locale to find out the local default? */
					month = (TIME_TOKEN)(n1 + ((int)TT_JAN) - 1);
					date = n2;
					year = n3;
				  }
				rest = s;
			  }
			else if ((*end >= 'A' && *end <= 'Z') ||
					 (*end >= 'a' && *end <= 'z'))
			  /* Digits followed by non-punctuation - what's that? */
			  ;
			else if ((end - rest) == 4)		/* four digits is a year */
			  year = (year < 0
					  ? ((rest[0]-'0')*1000L +
						 (rest[1]-'0')*100L +
						 (rest[2]-'0')*10L +
						 (rest[3]-'0'))
					  : year);
			else if ((end - rest) == 2)		/* two digits - date or year */
			  {
				int n = ((rest[0]-'0')*10 +
						 (rest[1]-'0'));
				/* If we don't have a date (day of the month) and we see a number
				     less than 32, then assume that is the date.

					 Otherwise, if we have a date and not a year, assume this is the
					 year.  If it is less than 70, then assume it refers to the 21st
					 century.  If it is two digits (>= 70), assume it refers to this
					 century.  Otherwise, assume it refers to an unambiguous year.

					 The world will surely end soon.
				   */
				if (date < 0 && n < 32)
				  date = n;
				else if (year < 0)
				  {
					if (n < 70)
					  year = 2000 + n;
					else if (n < 100)
					  year = 1900 + n;
					else
					  year = n;
				  }
				/* else what the hell is this. */
			  }
			else if ((end - rest) == 1)		/* one digit - date */
			  date = (date < 0 ? (rest[0]-'0') : date);
			/* else, three or more than four digits - what's that? */

			break;
		  }
		}

	  /* Skip to the end of this token, whether we parsed it or not.
		 Tokens are delimited by whitespace, or ,;-/
		 But explicitly not :+-.
	   */
	  while (*rest &&
			 *rest != ' ' && *rest != '\t' &&
			 *rest != ',' && *rest != ';' &&
			 *rest != '-' && *rest != '+' &&
			 *rest != '/' &&
			 *rest != '(' && *rest != ')' && *rest != '[' && *rest != ']')
		rest++;
	  /* skip over uninteresting chars. */
	SKIP_MORE:
	  while (*rest &&
			 (*rest == ' ' || *rest == '\t' ||
			  *rest == ',' || *rest == ';' || *rest == '/' ||
			  *rest == '(' || *rest == ')' || *rest == '[' || *rest == ']'))
		rest++;

	  /* "-" is ignored at the beginning of a token if we have not yet
		 parsed a year, or if the character after the dash is not a digit. */
	  if (*rest == '-' && (year < 0 || rest[1] < '0' || rest[1] > '9'))
		{
		  rest++;
		  goto SKIP_MORE;
		}

	}

  if (zone != TT_UNKNOWN && zone_offset == -1)
	{
	  switch (zone)
		{
		case TT_PST: zone_offset = -8 * 60; break;
		case TT_PDT: zone_offset = -7 * 60; break;
		case TT_MST: zone_offset = -7 * 60; break;
		case TT_MDT: zone_offset = -6 * 60; break;
		case TT_CST: zone_offset = -6 * 60; break;
		case TT_CDT: zone_offset = -5 * 60; break;
		case TT_EST: zone_offset = -5 * 60; break;
		case TT_EDT: zone_offset = -4 * 60; break;
		case TT_AST: zone_offset = -4 * 60; break;
		case TT_NST: zone_offset = -3 * 60 - 30; break;
		case TT_GMT: zone_offset =  0 * 60; break;
		case TT_BST: zone_offset =  1 * 60; break;
		case TT_MET: zone_offset =  1 * 60; break;
		case TT_EET: zone_offset =  2 * 60; break;
		case TT_JST: zone_offset =  9 * 60; break;
		default:
		  XP_ASSERT (0);
		  break;
		}
	}

#ifdef DEBUG_TIME
  XP_TRACE(("%s\n"
		   "  dotw: %s\n"
		   "  mon:  %s\n"
		   "  date: %d\n"
		   "  year: %d\n"
		   "  hour: %d\n"
		   "  min:  %d\n"
		   "  sec:  %d\n"
		   "  zone: %s / %c%02d%02d (%d)\n",
		   string,
		   (dotw == TT_SUN ? "SUN" :
			dotw == TT_MON ? "MON" :
			dotw == TT_TUE ? "TUE" :
			dotw == TT_WED ? "WED" :
			dotw == TT_THU ? "THU" :
			dotw == TT_FRI ? "FRI" :
			dotw == TT_SAT ? "SAT" : "???"),
		   (month == TT_JAN ? "JAN" :
			month == TT_FEB ? "FEB" :
			month == TT_MAR ? "MAR" :
			month == TT_APR ? "APR" :
			month == TT_MAY ? "MAY" :
			month == TT_JUN ? "JUN" :
			month == TT_JUL ? "JUL" :
			month == TT_AUG ? "AUG" :
			month == TT_SEP ? "SEP" :
			month == TT_OCT ? "OCT" :
			month == TT_NOV ? "NOV" :
			month == TT_DEC ? "DEC" : "???"),
		   date, year, hour, min, sec,
		   (zone == TT_PST ? "PST" :
			zone == TT_PDT ? "PDT" :
			zone == TT_MST ? "MST" :
			zone == TT_MDT ? "MDT" :
			zone == TT_CST ? "CST" :
			zone == TT_CDT ? "CDT" :
			zone == TT_EST ? "EST" :
			zone == TT_EDT ? "EDT" :
			zone == TT_AST ? "AST" :
			zone == TT_NST ? "NST" :
			zone == TT_GMT ? "GMT" :
			zone == TT_BST ? "BST" :
			zone == TT_MET ? "MET" :
			zone == TT_EET ? "EET" :
			zone == TT_JST ? "JST" : "???"),
		   (zone_offset >= 0 ? '+' : '-'),
		   ((zone_offset >= 0 ? zone_offset : -zone_offset) / 60),
		   ((zone_offset >= 0 ? zone_offset : -zone_offset) % 60),
		   zone_offset));
#endif /* DEBUG_TIME */


  /* If we didn't find a year, month, or day-of-the-month, we can't
	 possibly parse this, and in fact, mktime() will do something random
	 (I'm seeing it return "Tue Feb  5 06:28:16 2036", which is no doubt
	 a numerologically significant date... */
  if (month == TT_UNKNOWN || date == -1 || year == -1)
	{
	  result = 0;
	  goto FAIL;
	}

  XP_MEMSET (&tm, 0, sizeof(tm));
  if (sec != -1)
	tm.tm_sec = sec;
  if (min != -1)
  tm.tm_min = min;
  if (hour != -1)
	tm.tm_hour = hour;
  if (date != -1)
	tm.tm_mday = date;
  if (month != TT_UNKNOWN)
	tm.tm_mon = (((int)month) - ((int)TT_JAN));
  if (year != -1)
	tm.tm_year = year - 1900;
  if (dotw != TT_UNKNOWN)
	tm.tm_wday = (((int)dotw) - ((int)TT_SUN));

  /* Set this to -1 to tell gmtime "I don't care".  If you set it to 0 or 1,
	 you are making assertions about whether the date you are handing it is
	 in daylight savings mode or not; and if you're wrong, it will "fix" it
	 for you. */
  tm.tm_isdst = -1;

  if (zone == TT_UNKNOWN && default_to_gmt)
	{
	  /* No zone was specified, so pretend the zone was GMT. */
	  zone = TT_GMT;
	  zone_offset = 0;
	}

  if (zone_offset == -1)
 	{
      /* no zone was specified, and we're to assume that everything
	     is local.  Add the gmt offset for the date we are parsing 
	     to transform the date to GMT */
	  struct tm tmp_tm = tm;

	  XP_ASSERT(tm.tm_mon > -1 
			   	&& tm.tm_mday > 0 
			   	&& tm.tm_hour > -1
			   	&& tm.tm_min > -1
			   	&& tm.tm_sec > -1);

  	  /* see if we are on 1-Jan-1970 or before 
	   * if we are we need to return zero here because
	   * mktime will crash on win16 if we call it.
	   */
  	  if(tm.tm_year < 70)
		{
	  	  /* month, day, hours, mins and secs are always non-negative
		   * so we dont need to worry about them.
	   	   */ 
	  	  return(0);  /* return the lowest possible date */
		} 

	  zone_offset = xp_internal_zone_offset(mktime(&tmp_tm));
	}

	/* Adjust the hours and minutes before handing them to the UTC parser.
	   Note that it's ok for them to be <0 or >24/60 

	   We need to adjust the time before going in to the UTC parser 
	   because it assumes its input is in GMT time.  The zone_offset
	   represents the difference between the time zone parsed and GMT
	
	 */
	tm.tm_hour -= (zone_offset / 60);
	tm.tm_min  -= (zone_offset % 60);

#if 0
#ifdef DEBUG_TIME
  XP_TRACE(("\n tm.tm_sec = %d\n tm.tm_min = %d\n tm.tm_hour = %d\n"
			" tm.tm_mday = %d\n tm.tm_mon = %d\n tm.tm_year = %d\n"
			" tm.tm_wday = %d\n tm.tm_yday = %d\n tm.tm_isdst = %d\n",
			tm.tm_sec, tm.tm_min, tm.tm_hour, tm.tm_mday, tm.tm_mon,
			tm.tm_year, tm.tm_wday, tm.tm_yday, tm.tm_isdst));
#endif /* DEBUG_TIME */
#endif /* 0 */

  result = xp_compute_UTC_from_GMT (&tm);

#if 0
#ifdef DEBUG_TIME
  XP_TRACE(("\n tm.tm_sec = %d\n tm.tm_min = %d\n tm.tm_hour = %d\n"
			" tm.tm_mday = %d\n tm.tm_mon = %d\n tm.tm_year = %d\n"
			" tm.tm_wday = %d\n tm.tm_yday = %d\n tm.tm_isdst = %d\n",
			tm.tm_sec, tm.tm_min, tm.tm_hour, tm.tm_mday, tm.tm_mon,
			tm.tm_year, tm.tm_wday, tm.tm_yday, tm.tm_isdst));
#endif /* DEBUG_TIME */
#endif /* 0 */

  if (result == ((time_t) -1))
	result = 0;

  /* Bug #36544 */
  if (result < 0)
    result = 0;

 FAIL:
#ifdef DEBUG_TIME
  XP_TRACE (("  %lu: %s", result, ctime(&result)));
#endif /* DEBUG_TIME */

  return result;
}





#if 0
main ()
{
  FILE *f = fopen("/u/jwz/DATES", "r");	/* This is a file containing the
										   228128 occurences of "^Date:"
										   that were in our news spool in
										   late June 1994. */
  char buf [255];
  char *s;
  XP_Bool flip = TRUE;
  int line = 0;
  int count = 0;
  int failed = 0;
  int dubious = 0;
  while ((s = fgets (buf, sizeof(buf)-1, f)))
    {
      if (s[0] == 'D' || s[0] == 'd')
		{
		  s = XP_STRCHR(s, ':');
		  if (s && *s)
			{
			  time_t t = XP_ParseTimeString (++s, (flip = !flip));
			  count++;
			  if (t < 757411200L) /* "jan  1 00:00:00 1994" */
				{
				  if (t < 1)
					failed++;
				  else
					dubious++;
				  fprintf (stderr, " %6d: %10lu: %s", line, t, s);
				}
			}
		}
      line++;
    }
  fprintf (stderr, "\ndates: %d; failed: %d; dubious = %d\n",
	   count, failed, dubious);
  fclose(f);
}
#endif /* 0 */
