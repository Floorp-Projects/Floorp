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

#include <time.h>
#include "xp_mcom.h"

/* Note that the ordering of the '#include "xp_mcom.h"' and the '#undef ctime' is
   important.  xp_mcom.h defines ctime as Macctime.  Macctime calls ctime.  The result
   was endless recursion. -km */

#ifdef mktime
#undef mktime
#undef ctime
#undef localtime
__extern_c	/* declarations */
time_t mktime(struct tm *);
char *ctime(const time_t *);
struct tm *localtime(const time_t *);
__end_extern_c
#endif


// Because serial port and SLIP conflict with ReadXPram calls,
// we cache the call here so we don't hang on calling ReadLocation()
void MyReadLocation(MachineLocation * loc);

long GMTDelta();
Boolean DaylightSaving();
time_t	GetTimeMac();
time_t Mactime(time_t *timer);
time_t Macmktime (struct tm *timeptr);
char * Macctime(const time_t * t);
struct tm *Macgmtime (const time_t *clock);


void MyReadLocation(MachineLocation * loc)
{
	static MachineLocation storedLoc;	// InsideMac, OSUtilities, page 4-20
	static Boolean didReadLocation = FALSE;
	if (!didReadLocation)
	{	
		ReadLocation(&storedLoc);
		didReadLocation = TRUE;
	}
	*loc = storedLoc;
}

Boolean DaylightSaving()
{
	MachineLocation loc;
	unsigned char dlsDelta;
	MyReadLocation(&loc);
	dlsDelta = 	loc.u.dlsDelta;
	return (dlsDelta != 0);
}


// We need to do this, because we wrap localtime as a macro in
#ifdef localtime
#undef localtime
#endif

// current local time = GMTDelta() + GMT
// GMT = local time - GMTDelta()
long GMTDelta()
{
	MachineLocation loc;
	long gmtDelta;
	
	MyReadLocation(&loc);
	gmtDelta = loc.u.gmtDelta & 0x00FFFFFF;
	if ((gmtDelta & 0x00800000) != 0)
		gmtDelta |= 0xFF000000;
	return gmtDelta;
}


// This routine simulates stdclib time(), time in seconds since 1.1.1970
// The time is in GMT
time_t	GetTimeMac()
{
	unsigned long maclocal;
	// Get Mac local time
	GetDateTime(&maclocal); 
	// Get Mac GMT	
	maclocal -= GMTDelta();
	// return unix GMT
	return (maclocal - UNIXMINUSMACTIME);
}

// Returns the GMT times
time_t Mactime(time_t *timer)
{
	time_t t = GetTimeMac();
	if (timer != NULL)
		*timer = t;
	return t;
}

time_t Macmktime (struct tm *timeptr)
{
	time_t theTime;
	
	// еее HACK to work around the bug in mktime
	int negativeDiff = 0;
	if (timeptr->tm_sec < 0)
	{
		negativeDiff += timeptr->tm_sec;
		timeptr->tm_sec = 0;	
	}
	if (timeptr->tm_min < 0)
	{
		negativeDiff += timeptr->tm_min*60;
		timeptr->tm_min = 0;	
	}
	if (timeptr->tm_hour < 0)
	{
		negativeDiff += timeptr->tm_hour*60*60;
		timeptr->tm_hour = 0;	
	}
	if (timeptr->tm_mday < 0)
	{
		negativeDiff += timeptr->tm_mday*60*60*24;
		timeptr->tm_mday = 0;	
	}
	
	// local/Mac
	theTime = mktime(timeptr);
	// mktime does not care what the daylightsavings flag is
	timeptr->tm_isdst = 0;//DaylightSavings(); 
	theTime += negativeDiff;

	// GMT/Mac
	theTime -= GMTDelta();
	// unix/GMT
	return theTime - UNIXMINUSMACTIME;
}

//  
char * Macctime(const time_t * t)
{
	// GMT Mac
	time_t macTime = *t + UNIXMINUSMACTIME;
	// local Mac
	macTime += GMTDelta();
	
	return ctime(&macTime);
}

struct tm *Maclocaltime(const time_t * t)
{
	// GMT Mac
	time_t macLocal = *t + UNIXMINUSMACTIME;
	// local Mac
	macLocal += GMTDelta();
	return localtime(&macLocal);
}

// -> unix GMT
struct tm *Macgmtime (const time_t *clock)
{
	// GMT Mac
	time_t macGMT =  *clock + UNIXMINUSMACTIME;
	return localtime(&macGMT);
}

