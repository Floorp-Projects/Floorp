/* -*- Mode: C; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
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
 * PR time code.
 * XXXbe PR_DSTOffset uses PR_basetime, should use A.D.Olson code instead
 */
#ifdef SOLARIS
#define _REENTRANT 1
#endif
#include <string.h>
#include <time.h>
#include "prtypes.h"
#include "prosdep.h"
#include "prprintf.h"
#include "prtime.h"

#define PR_DO_MILLISECONDS 1

#ifdef XP_PC
#include <sys/timeb.h>
#endif

#ifdef XP_MAC
#include <OSUtils.h>
#include <TextUtils.h>
#include <Resources.h>
#include <Timer.h>
#endif

#ifdef XP_UNIX

#ifdef SOLARIS
extern int gettimeofday(struct timeval *tv);
#endif

#include <sys/time.h>

#ifdef NEED_TIME_R
/* Awful hack, but... */
struct tm *gmtime_r(const time_t *a, struct tm *b)
{
    *b = *gmtime(a);
    return b;
}

struct tm *localtime_r(const time_t *a, struct tm *b)
{
    *b = *localtime(a);
    return b;
}
#endif /* NEED_TIME_R */

#endif /* XP_UNIX */

#ifdef XP_MAC
UnsignedWide		dstLocalBaseMicroseconds;
unsigned long		gJanuaryFirst1970Seconds;

static void MacintoshInitializeTime(void)
{
	UnsignedWide			upTime;
	unsigned long			currentLocalTimeSeconds,
							startupTimeSeconds;
	uint64					startupTimeMicroSeconds;
	uint32					upTimeSeconds;
	uint64					oneMillion, upTimeSecondsLong, microSecondsToSeconds;
	DateTimeRec				firstSecondOfUnixTime;

	//	Figure out in local time what time the machine
	//	started up.  This information can be added to
	//	upTime to figure out the current local time
	//	as well as GMT.

	Microseconds(&upTime);

	GetDateTime(&currentLocalTimeSeconds);

	LL_I2L(microSecondsToSeconds, PR_USEC_PER_SEC);
	LL_DIV(upTimeSecondsLong,  *((uint64 *)&upTime), microSecondsToSeconds);
	LL_L2I(upTimeSeconds, upTimeSecondsLong);

	startupTimeSeconds = currentLocalTimeSeconds - upTimeSeconds;

	//	Make sure that we normalize the macintosh base seconds
	//	to the unix base of January 1, 1970.

	firstSecondOfUnixTime.year = 1970;
	firstSecondOfUnixTime.month = 1;
	firstSecondOfUnixTime.day = 1;
	firstSecondOfUnixTime.hour = 0;
	firstSecondOfUnixTime.minute = 0;
	firstSecondOfUnixTime.second = 0;
	firstSecondOfUnixTime.dayOfWeek = 0;

	DateToSeconds(&firstSecondOfUnixTime, &gJanuaryFirst1970Seconds);

	startupTimeSeconds -= gJanuaryFirst1970Seconds;

	//	Now convert the startup time into a wide so that we
	//	can figure out GMT and DST.

	LL_I2L(startupTimeMicroSeconds, startupTimeSeconds);
	LL_I2L(oneMillion, PR_USEC_PER_SEC);
	LL_MUL(dstLocalBaseMicroseconds, oneMillion, startupTimeMicroSeconds);
}

// Because serial port and SLIP conflict with ReadXPram calls,
// we cache the call here

static void MyReadLocation(MachineLocation * loc)
{
	static MachineLocation storedLoc;	// InsideMac, OSUtilities, page 4-20
	static PRBool didReadLocation = PR_FALSE;
	if (!didReadLocation)
	{
		MacintoshInitializeTime();
		ReadLocation(&storedLoc);
		didReadLocation = PR_TRUE;
	}
	*loc = storedLoc;
}

#endif

#define IS_LEAP(year) \
   (year != 0 && ((((year & 0x3) == 0) &&  \
		   ((year - ((year/100) * 100)) != 0)) || \
		  (year - ((year/400) * 400)) == 0))

#define PR_HOUR_SECONDS  3600L
#define PR_DAY_SECONDS  (24L * PR_HOUR_SECONDS)
#define PR_YEAR_SECONDS (PR_DAY_SECONDS * 365L)
#define PR_MAX_UNIX_TIMET 2145859200L /*time_t value equiv. to 12/31/2037 */

/* function prototypes */
static void PR_basetime(int64 tsecs, PRTime *prtm);

/*
 * get the difference in seconds between this time zone and UTC (GMT)
 */
PR_PUBLIC_API(time_t)
PR_LocalGMTDifference()
{
#if defined(XP_UNIX) || defined(XP_PC)
    struct tm ltime;

    /* get the difference between this time zone and GMT */
    memset((char *)&ltime,0,sizeof(ltime));
    ltime.tm_mday = 2;
    ltime.tm_year = 70;
#ifdef SUNOS4
    ltime.tm_zone = 0;
    ltime.tm_gmtoff = 0;
    return timelocal(&ltime) - (24 * 3600);
#else
    return mktime(&ltime) - (24L * 3600L);
#endif
#endif
#if defined(XP_MAC)
    static time_t    zone = -1L;
    MachineLocation  machineLocation;
    uint64           gmtOffsetSeconds;
    uint64           gmtDelta;
    uint64           dlsOffset;
    int32            offset;

    /* difference has been set no need to recalculate */
    if(zone != -1)
	return zone;

    /* Get the information about the local machine, including
     * its GMT offset and its daylight savings time info.
     * Convert each into wides that we can add to
     * startupTimeMicroSeconds.
     */

    MyReadLocation(&machineLocation);

    /* Mask off top eight bits of gmtDelta, sign extend lower three. */

    if ((machineLocation.u.gmtDelta & 0x00800000) != 0) {
	gmtOffsetSeconds.lo = (machineLocation.u.gmtDelta & 0x00FFFFFF) | 0xFF000000;
	gmtOffsetSeconds.hi = 0xFFFFFFFF;
	LL_UI2L(gmtDelta,0);
    } else {
	gmtOffsetSeconds.lo = (machineLocation.u.gmtDelta & 0x00FFFFFF);
	gmtOffsetSeconds.hi = 0;
	LL_UI2L(gmtDelta,PR_DAY_SECONDS);
    }

    /*
     * Normalize time to be positive if you are behind GMT. gmtDelta will
     * always be positive.
     */
    LL_SUB(gmtDelta,gmtDelta,gmtOffsetSeconds);

    /* Is Daylight Savings On?  If so, we need to add an hour to the offset. */
    if (machineLocation.u.dlsDelta != 0) {
	LL_UI2L(dlsOffset, PR_HOUR_SECONDS);
    } else {
	LL_I2L(dlsOffset, 0);
    }

    LL_ADD(gmtDelta,gmtDelta, dlsOffset);
    LL_L2I(offset,gmtDelta);

    zone = offset;
    return (time_t)offset;
#endif
}

/* Constants for GMT offset from 1970 */
#define G1970GMTMICROHI        0x00dcdcad /* micro secs to 1970 hi */
#define G1970GMTMICROLOW       0x8b3fa000 /* micro secs to 1970 low */

#define G2037GMTMICROHI        0x00e45fab /* micro secs to 2037 high */
#define G2037GMTMICROLOW       0x7a238000 /* micro secs to 2037 low */

/* Convert from base time to extended time */
static int64
PR_ToExtendedTime(int32 time)
{
    int64 exttime;
    int64 g1970GMTMicroSeconds;
    int64 low;
    time_t diff;
    int64  tmp;
    int64  tmp1;

    diff = PR_LocalGMTDifference();
    LL_UI2L(tmp, PR_USEC_PER_SEC);
    LL_I2L(tmp1,diff);
    LL_MUL(tmp,tmp,tmp1);

    LL_UI2L(g1970GMTMicroSeconds,G1970GMTMICROHI);
    LL_UI2L(low,G1970GMTMICROLOW);
#ifndef HAVE_LONG_LONG
    LL_SHL(g1970GMTMicroSeconds,g1970GMTMicroSeconds,16);
    LL_SHL(g1970GMTMicroSeconds,g1970GMTMicroSeconds,16);
#else
    LL_SHL(g1970GMTMicroSeconds,g1970GMTMicroSeconds,32);
#endif
    LL_ADD(g1970GMTMicroSeconds,g1970GMTMicroSeconds,low);

    LL_I2L(exttime,time);
    LL_ADD(exttime,exttime,g1970GMTMicroSeconds);
    LL_SUB(exttime,exttime,tmp);
    return exttime;
}

PR_PUBLIC_API(int64)
PR_Now(void)
{
#ifdef XP_PC
    int64 s, us, ms2us, s2us;
    struct timeb b;
#endif /* XP_PC */
#ifdef XP_UNIX
    struct timeval tv;
    int64 s, us, s2us;
#endif /* XP_UNIX */
#ifdef XP_MAC
    UnsignedWide upTime;
    int64        localTime;
    int64        gmtOffset;
    int64        dstOffset;
    time_t       gmtDiff;
    int64        s2us;
#endif /* XP_MAC */

#ifdef XP_PC
    ftime(&b);
    LL_UI2L(ms2us, PR_USEC_PER_MSEC);
    LL_UI2L(s2us, PR_USEC_PER_SEC);
    LL_UI2L(s, b.time);
    LL_UI2L(us, b.millitm);
    LL_MUL(us, us, ms2us);
    LL_MUL(s, s, s2us);
    LL_ADD(s, s, us);
    return s;
#endif

#ifdef XP_UNIX
#if defined(SOLARIS)
    gettimeofday(&tv);
#else
    gettimeofday(&tv, 0);
#endif /* SOLARIS */
    LL_UI2L(s2us, PR_USEC_PER_SEC);
    LL_UI2L(s, tv.tv_sec);
    LL_UI2L(us, tv.tv_usec);
    LL_MUL(s, s, s2us);
    LL_ADD(s, s, us);
    return s;
#endif /* XP_UNIX */
#ifdef XP_MAC
    LL_UI2L(localTime,0);
    gmtDiff = PR_LocalGMTDifference();
    LL_I2L(gmtOffset,gmtDiff);
    LL_UI2L(s2us, PR_USEC_PER_SEC);
    LL_MUL(gmtOffset,gmtOffset,s2us);
    LL_UI2L(dstOffset,0);
    dstOffset = PR_DSTOffset(dstOffset);
    LL_SUB(gmtOffset,gmtOffset,dstOffset);
    /* don't adjust for DST since it sets ctime and gmtime off on the MAC */
    Microseconds(&upTime);
    LL_ADD(localTime,localTime,gmtOffset);
    LL_ADD(localTime,localTime, *((uint64 *)&dstLocalBaseMicroseconds));
    LL_ADD(localTime,localTime, *((uint64 *)&upTime));

    return *((uint64 *)&localTime);
#endif /* XP_MAC */
}

/* Get the DST timezone offset for the time passed in
 */
PR_PUBLIC_API(int64)
PR_DSTOffset(int64 time)
{
    int64 us2s;
#ifdef XP_MAC
    MachineLocation machineLocation;
    int64 dlsOffset;

    /*
     * Get the information about the local machine, including
     * its GMT offset and its daylight savings time info.
     * Convert each into wides that we can add to
     * startupTimeMicroSeconds.
     */
    MyReadLocation(&machineLocation);

    /* Is Daylight Savings On?  If so, we need to add an hour to the offset. */
    if (machineLocation.u.dlsDelta != 0) {
	LL_UI2L(us2s, PR_USEC_PER_SEC);         /* seconds in a microseconds */
	LL_UI2L(dlsOffset, PR_HOUR_SECONDS);    /* seconds in one hour */
	LL_MUL(dlsOffset, dlsOffset, us2s);
    } else {
	LL_I2L(dlsOffset, 0);
    }
    return(dlsOffset);
#else  /* XP_PC || XP_UNIX */
    time_t local;
    int32 diff;
    int64  maxtimet;
    struct tm tm;
    PRTime prtm;
#if defined( XP_PC ) || defined( FREEBSD ) || defined ( HPUX9 ) || defined ( SNI )
    struct tm *ptm;
#endif

    LL_UI2L(us2s, PR_USEC_PER_SEC);
    LL_DIV(time, time, us2s);

    /* get the maximum of time_t value */
    LL_UI2L(maxtimet,PR_MAX_UNIX_TIMET);

    if (LL_CMP(time,>,maxtimet)) {
	LL_UI2L(time,PR_MAX_UNIX_TIMET);
    } else if (!LL_GE_ZERO(time) || LL_IS_ZERO(time)) {
	/* go ahead a day to make localtime work (does not work with 0) */
	LL_UI2L(time,PR_DAY_SECONDS);
    }
    LL_L2UI(local,time);
    PR_basetime(time,&prtm);
#if defined( XP_PC ) || defined( FREEBSD ) || defined ( HPUX9 ) || defined ( SNI )
    ptm = localtime(&local);
    if (!ptm)
	return LL_ZERO;
    tm = *ptm;
#else
    localtime_r(&local,&tm); /* get dst information */
#endif

    diff = ((tm.tm_hour - prtm.tm_hour) * PR_HOUR_SECONDS)
	   + ((tm.tm_min - prtm.tm_min) * 60);

    if (diff < 0)
	diff += PR_DAY_SECONDS;

    LL_UI2L(time,diff);

    LL_MUL(time,time,us2s);

    return(time);
#endif /* XP_PC || XP_UNIX */
}

/* Format a time value into a buffer. Same semantics as strftime() */
PR_PUBLIC_API(size_t)
PR_FormatTime(char *buf, int buflen, char *fmt, PRTime *prtm)
{
#if defined(XP_UNIX) || defined(XP_PC) || defined(XP_MAC)
    struct tm a;

    /* Zero out the tm struct.  Linux, SunOS 4 struct tm has extra members int
     * tm_gmtoff, char *tm_zone; when tm_zone is garbage, strftime gets
     * confused and dumps core.  NSPR20 prtime.c attempts to fill these in by
     * calling mktime on the partially filled struct, but this doesn't seem to
     * work as well; the result string has "can't get timezone" for ECMA-valid
     * years.  Might still make sense to use this, but find the range of years
     * for which valid tz information exists, and map (per ECMA hint) from the
     * given year into that range.
     
     * N.B. This hasn't been tested with anything that actually _uses_
     * tm_gmtoff; zero might be the wrong thing to set it to if you really need
     * to format a time.  This fix is for jsdate.c, which only uses
     * PR_FormatTime to get a string representing the time zone.  */
    memset(&a, 0, sizeof(struct tm));

    a.tm_sec = prtm->tm_sec;
    a.tm_min = prtm->tm_min;
    a.tm_hour = prtm->tm_hour;
    a.tm_mday = prtm->tm_mday;
    a.tm_mon = prtm->tm_mon;
    a.tm_wday = prtm->tm_wday;
    a.tm_year = prtm->tm_year - 1900;
    a.tm_yday = prtm->tm_yday;
    a.tm_isdst = prtm->tm_isdst;

    /* Even with the above, SunOS 4 seems to detonate if tm_zone and tm_gmtoff
     * are null.  This doesn't quite work, though - the timezone is off by
     * tzoff + dst.  (And mktime seems to return -1 for the exact dst
     * changeover time.)

     * Still not sure if MKLINUX is necessary; this is borrowed from the NSPR20
     * prtime.c.  I'm leaving it out - My Linux does the right thing without it
     * (and the wrong thing with it) even though it has the tm_gmtoff, tm_zone
     * fields.  Linux seems to be happy so long as the tm struct is zeroed out.
     * The #ifdef in nspr is:
     * #if defined(SUNOS4) || defined(MKLINUX) || defined (__GLIBC >= 2)
     */

#if defined(SUNOS4)
    if (mktime(&a) == -1) {
        /* Seems to fail whenever the requested date is outside of the 32-bit
         * UNIX epoch.  We could proceed at this point (setting a.tm_zone to
         * "") but then strftime returns a string with a 2-digit field of
         * garbage for the year.  So we return 0 and hope jsdate.c
         * will fall back on toString.
         */
        return 0;
    }
#endif

    return strftime(buf, buflen, fmt, &a);
#endif
}

/* table for number of days in a month */
static int mtab[] = {
  /* jan, feb,mar,apr,may,jun */
  31,28,31,30,31,30,
  /* july,aug,sep,oct,nov,dec */
  31,31,30,31,30,31
};

/*
 * basic time calculation functionality for localtime and gmtime
 * setups up prtm argument with correct values based upon input number
 * of seconds.
 */
static void
PR_basetime(int64 tsecs, PRTime *prtm)
{
    /* convert tsecs back to year,month,day,hour,secs */
    int32 year    = 0;
    int32 month   = 0;
    int32 yday    = 0;
    int32 mday    = 0;
    int32 wday    = 6; /* start on a Sunday */
    int32 days    = 0;
    int32 seconds = 0;
    int32 minutes = 0;
    int32 hours   = 0;
    int32 isleap  = 0;
    int64 result;
    int64 result1;
    int64 result2;
    int64 base;

    LL_UI2L(result,0);
    LL_UI2L(result1,0);
    LL_UI2L(result2,0);

    /* get the base time via UTC */
    base = PR_ToExtendedTime(0);
    LL_UI2L(result, PR_USEC_PER_SEC);
    LL_DIV(base,base,result);
    LL_ADD(tsecs,tsecs,base);

    LL_UI2L(result, PR_YEAR_SECONDS);
    LL_UI2L(result1,PR_DAY_SECONDS);
    LL_ADD(result2,result,result1);

  /* get the year */
    while ((isleap == 0) ? !LL_CMP(tsecs,<,result) : !LL_CMP(tsecs,<,result2)) {
	/* subtract a year from tsecs */
	LL_SUB(tsecs,tsecs,result);
	days += 365;
	/* is it a leap year ? */
	if(IS_LEAP(year)){
	    LL_SUB(tsecs,tsecs,result1);
	    days++;
	}
	year++;
	isleap = IS_LEAP(year);
    }

    LL_UI2L(result1,PR_DAY_SECONDS);

    LL_DIV(result,tsecs,result1);
    LL_L2I(mday,result);

  /* let's find the month */
    while(((month == 1 && isleap) ?
	   (mday >= mtab[month] + 1) :
	   (mday >= mtab[month]))){
	yday += mtab[month];
	days += mtab[month];

	mday -= mtab[month];

    /* it's a Feb, check if this is a leap year */
	if(month == 1 && isleap != 0){
	    yday++;
	    days++;
	    mday--;
	}
	month++;
    }

    /* now adjust tsecs */
    LL_MUL(result,result,result1);
    LL_SUB(tsecs,tsecs,result);

    mday++; /* day of month always start with 1 */
    days += mday;
    wday = (days + wday) % 7;

    yday += mday;

    /* get the hours */
    LL_UI2L(result1,PR_HOUR_SECONDS);
    LL_DIV(result,tsecs,result1);
    LL_L2I(hours,result);
    LL_MUL(result,result,result1);
    LL_SUB(tsecs,tsecs,result);

    /* get minutes */
    LL_UI2L(result1,60);
    LL_DIV(result,tsecs,result1);
    LL_L2I(minutes,result);
    LL_MUL(result,result,result1);
    LL_SUB(tsecs,tsecs,result);

    LL_L2I(seconds,tsecs);

    prtm->tm_usec  = 0L;
    prtm->tm_sec   = (int8)seconds;
    prtm->tm_min   = (int8)minutes;
    prtm->tm_hour  = (int8)hours;
    prtm->tm_mday  = (int8)mday;
    prtm->tm_mon   = (int8)month;
    prtm->tm_wday  = (int8)wday;
    prtm->tm_year  = (int16)year;
    prtm->tm_yday  = (int16)yday;
}
