/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*-
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
#ifdef mktime
#undef mktime
#undef ctime
#undef localtime
_C_LIB_DECL	/* declarations */
time_t mktime(struct tm *);
char *ctime(const time_t *);
struct tm *localtime(const time_t *);
_END_C_LIB_DECL
#endif

#ifndef UNIXMINUSMACTIME
#define UNIXMINUSMACTIME 2082844800UL
#endif
#include <time.h>

// Because serial port and SLIP conflict with ReadXPram calls,
// we cache the call here
// The symptoms are the 
void MyReadLocation(MachineLocation * loc);
long GMTDelta();


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

Boolean DaylightSavings()
{
	MachineLocation loc;
	unsigned char dlsDelta;
	MyReadLocation(&loc);
	dlsDelta = 	loc.u.dlsDelta;
	return (dlsDelta != 0);
}

// This routine is copied straight out of stdio sources.
// The only difference is that we use MyReadLocation instead of ReadLocation.
struct tm *(gmtime)(const time_t *tod)
{	/* convert to Greenwich Mean Time (UTC) */
	MachineLocation myLocation;
	long int internalGmtDelta;
	time_t ltime;
	MyReadLocation(&myLocation);
	internalGmtDelta = myLocation.u.gmtDelta & 0x00ffffff;
	if (internalGmtDelta & 0x00800000)
		internalGmtDelta = internalGmtDelta | 0xff000000;
	ltime = (*tod) - internalGmtDelta;
	return (localtime(&(ltime)));
}


// We need to do this, because we wrap localtime as a macro in
#ifdef localtime
#undef localtime
#endif
struct tm *localtime(const time_t *tp)
	{
	DateTimeRec dtr;
	MachineLocation loc;
	static struct tm statictime;
	static const short monthday[12] =
		{0, 31, 59, 90, 120, 151, 181, 212, 243, 273, 304, 334};

	SecondsToDate(*tp, &dtr);
	statictime.tm_sec = dtr.second;
	statictime.tm_min = dtr.minute;
	statictime.tm_hour = dtr.hour;
	statictime.tm_mday = dtr.day;
	statictime.tm_mon = dtr.month - 1;
	statictime.tm_year = dtr.year - 1900;
	statictime.tm_wday = dtr.dayOfWeek - 1;
	statictime.tm_yday = monthday[statictime.tm_mon]
		+ statictime.tm_mday - 1;
	if (2 < statictime.tm_mon && !(statictime.tm_year & 3))
		++statictime.tm_yday;
	MyReadLocation(&loc);
	statictime.tm_isdst = loc.u.dlsDelta;
	return(&statictime);
}


// current local time = GMTDelta() + GMT
// GMT = local time - GMTDelta()
long GMTDelta()
{
	MachineLocation loc;
	long gmtDelta;
	
	MyReadLocation(&loc);
	gmtDelta = loc.u.gmtDelta & 0x00FFFFFF;
	if (BitTst((Ptr)&gmtDelta, 23))
		gmtDelta = gmtDelta | 0xFF000000;
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

