/* -*- Mode: C; tab-width: 8 -*-
 * Copyright © 1996 Netscape Communications Corporation, All Rights Reserved.
 */

/*
 * PR time code.
 */
#ifdef SOLARIS
#define _REENTRANT 1
#endif
#include <string.h>
#include <time.h>
#include "prtypes.h"
#ifndef NSPR20
#include "prosdep.h"
#else
#ifdef XP_MAC
#include "prosdep.h"
#else
#include "md/prosdep.h"
#endif
#endif
#include "prprf.h"
#include "prmjtime.h"

#define PRMJ_DO_MILLISECONDS 1

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

#endif /* XP_UNIX */

#ifdef XP_MAC
extern UnsignedWide		dstLocalBaseMicroseconds;
extern PRUintn			gJanuaryFirst1970Seconds;
extern void MyReadLocation(MachineLocation* l);
#endif

#define IS_LEAP(year) \
   (year != 0 && ((((year & 0x3) == 0) &&  \
		   ((year - ((year/100) * 100)) != 0)) || \
		  (year - ((year/400) * 400)) == 0))

#define PRMJ_HOUR_SECONDS  3600L
#define PRMJ_DAY_SECONDS  (24L * PRMJ_HOUR_SECONDS)
#define PRMJ_YEAR_SECONDS (PRMJ_DAY_SECONDS * 365L)
#define PRMJ_MAX_UNIX_TIMET 2145859200L /*time_t value equiv. to 12/31/2037 */
/* function prototypes */
static void PRMJ_basetime(PRInt64 tsecs, PRMJTime *prtm);
/*
 * get the difference in seconds between this time zone and UTC (GMT)
 */
PR_IMPLEMENT(time_t) PRMJ_LocalGMTDifference()
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
    PRUint64	     gmtOffsetSeconds;
    PRUint64	     gmtDelta;
    PRUint64	     dlsOffset;
    PRInt32	     offset;

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
    }
    else {

	gmtOffsetSeconds.lo = (machineLocation.u.gmtDelta & 0x00FFFFFF);
	gmtOffsetSeconds.hi = 0;
	LL_UI2L(gmtDelta,PRMJ_DAY_SECONDS);
    }
    /* normalize time to be positive if you are behind GMT. gmtDelta will always
     * be positive.
     */
    LL_SUB(gmtDelta,gmtDelta,gmtOffsetSeconds);

    /* Is Daylight Savings On?  If so, we need to add an hour to the offset. */
    if (machineLocation.u.dlsDelta != 0) {
	LL_UI2L(dlsOffset, PRMJ_HOUR_SECONDS);
    }
    else
	LL_I2L(dlsOffset, 0);

    LL_ADD(gmtDelta,gmtDelta, dlsOffset);
    LL_L2I(offset,gmtDelta);

    zone = offset;
    return (time_t)offset;
#endif
}

/*
 * get information about the DST status of this time zone
 */
PR_IMPLEMENT(void)
PRMJ_setDST(PRMJTime *prtm)
{
#ifdef XP_MAC
    MachineLocation machineLocation;

    if(prtm->tm_isdst < 0){
	MyReadLocation(&machineLocation);
	/* Figure out daylight savings time. */
	prtm->tm_isdst = (machineLocation.u.dlsDelta != 0);
    }
#else
    struct tm time;

    if(prtm->tm_isdst < 0){
	if(prtm->tm_year >= 1970 && prtm->tm_year <= 2037){
	    time.tm_sec  = prtm->tm_sec ;
	    time.tm_min  = prtm->tm_min ;
	    time.tm_hour = prtm->tm_hour;
	    time.tm_mday = prtm->tm_mday;
	    time.tm_mon  = prtm->tm_mon ;
	    time.tm_wday = prtm->tm_wday;
	    time.tm_year = prtm->tm_year-1900;
	    time.tm_yday = prtm->tm_yday;
	    time.tm_isdst = -1;
	    mktime(&time);
	    prtm->tm_isdst = time.tm_isdst;
	}
	else {
	    prtm->tm_isdst = 0;
	}
    }
#endif /* XP_MAC */
}

/* Constants for GMT offset from 1970 */
#define G1970GMTMICROHI        0x00dcdcad /* micro secs to 1970 hi */
#define G1970GMTMICROLOW       0x8b3fa000 /* micro secs to 1970 low */

#define G2037GMTMICROHI        0x00e45fab /* micro secs to 2037 high */
#define G2037GMTMICROLOW       0x7a238000 /* micro secs to 2037 low */
/* Convert from extended time to base time (time since Jan 1 1970) it
*  truncates dates if time is before 1970 and after 2037.
 */

PR_IMPLEMENT(PRInt32)
PRMJ_ToBaseTime(PRInt64 time)
{
    PRInt64 g1970GMTMicroSeconds;
    PRInt64 g2037GMTMicroSeconds;
    PRInt64 low;
    PRInt32 result;

    LL_UI2L(g1970GMTMicroSeconds,G1970GMTMICROHI);
    LL_UI2L(low,G1970GMTMICROLOW);
#ifndef HAVE_LONG_LONG
    LL_SHL(g1970GMTMicroSeconds,g1970GMTMicroSeconds,16);
    LL_SHL(g1970GMTMicroSeconds,g1970GMTMicroSeconds,16);
#else
    LL_SHL(g1970GMTMicroSeconds,g1970GMTMicroSeconds,32);
#endif
    LL_ADD(g1970GMTMicroSeconds,g1970GMTMicroSeconds,low);

    LL_UI2L(g2037GMTMicroSeconds,G2037GMTMICROHI);
    LL_UI2L(low,G2037GMTMICROLOW);
#ifndef HAVE_LONG_LONG
    LL_SHL(g2037GMTMicroSeconds,g2037GMTMicroSeconds,16);
    LL_SHL(g2037GMTMicroSeconds,g2037GMTMicroSeconds,16);
#else
    LL_SHL(g2037GMTMicroSeconds,g2037GMTMicroSeconds,32);
#endif

    LL_ADD(g2037GMTMicroSeconds,g2037GMTMicroSeconds,low);


    if(LL_CMP(time, <, g1970GMTMicroSeconds) ||
       LL_CMP(time, >, g2037GMTMicroSeconds)){
	return -1;
    }

    LL_SUB(time,time,g1970GMTMicroSeconds);
    LL_L2I(result,time);
    return result;
}

/* Convert from base time to extended time */
PR_IMPLEMENT(PRInt64)
PRMJ_ToExtendedTime(PRInt32 time)
{
    PRInt64 exttime;
    PRInt64 g1970GMTMicroSeconds;
    PRInt64 low;
    time_t diff;
    PRInt64  tmp;
    PRInt64  tmp1;

    diff = PRMJ_LocalGMTDifference();
    LL_UI2L(tmp, PRMJ_USEC_PER_SEC);
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

PR_IMPLEMENT(PRInt64)
PRMJ_Now(void)
{
#ifdef XP_PC
    PRInt64 s, us, ms2us, s2us;
    struct timeb b;
#endif /* XP_PC */
#ifdef XP_UNIX
    struct timeval tv;
    PRInt64 s, us, s2us;
#endif /* XP_UNIX */
#ifdef XP_MAC
    UnsignedWide upTime;
    PRInt64	 localTime;
    PRInt64       gmtOffset;
    PRInt64    dstOffset;
    time_t       gmtDiff;
    PRInt64	 s2us;
#endif /* XP_MAC */

#ifdef XP_PC
    ftime(&b);
    LL_UI2L(ms2us, PRMJ_USEC_PER_MSEC);
    LL_UI2L(s2us, PRMJ_USEC_PER_SEC);
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
    LL_UI2L(s2us, PRMJ_USEC_PER_SEC);
    LL_UI2L(s, tv.tv_sec);
    LL_UI2L(us, tv.tv_usec);
    LL_MUL(s, s, s2us);
    LL_ADD(s, s, us);
    return s;
#endif /* XP_UNIX */
#ifdef XP_MAC
    LL_UI2L(localTime,0);
    gmtDiff = PRMJ_LocalGMTDifference();
    LL_I2L(gmtOffset,gmtDiff);
    LL_UI2L(s2us, PRMJ_USEC_PER_SEC);
    LL_MUL(gmtOffset,gmtOffset,s2us);
    LL_UI2L(dstOffset,0);
    dstOffset = PRMJ_DSTOffset(dstOffset);
    LL_SUB(gmtOffset,gmtOffset,dstOffset);
    /* don't adjust for DST since it sets ctime and gmtime off on the MAC */
    Microseconds(&upTime);
    LL_ADD(localTime,localTime,gmtOffset);
    LL_ADD(localTime,localTime, *((PRUint64 *)&dstLocalBaseMicroseconds));
    LL_ADD(localTime,localTime, *((PRUint64 *)&upTime));

    return *((PRUint64 *)&localTime);
#endif /* XP_MAC */
}
/*
 * Return the current local time in milli-seconds.
 */
PR_IMPLEMENT(PRInt64)
PRMJ_NowMS(void)
{
    PRInt64 us, us2ms;

    us = PRMJ_Now();
    LL_UI2L(us2ms, PRMJ_USEC_PER_MSEC);
    LL_DIV(us, us, us2ms);

    return us;
}

/*
 * Return the current local time in seconds.
 */
PR_IMPLEMENT(PRInt64)
PRMJ_NowS(void)
{
    PRInt64 us, us2s;

    us = PRMJ_Now();
    LL_UI2L(us2s, PRMJ_USEC_PER_SEC);
    LL_DIV(us, us, us2s);
    return us;
}

/* Get the DST timezone offset for the time passed in
 */
PR_IMPLEMENT(PRInt64)
PRMJ_DSTOffset(PRInt64 time)
{
    PRInt64 us2s;
#ifdef XP_MAC
    MachineLocation  machineLocation;
    PRInt64 dlsOffset;
    /*	Get the information about the local machine, including
     *	its GMT offset and its daylight savings time info.
     *	Convert each into wides that we can add to
     *	startupTimeMicroSeconds.
     */
    MyReadLocation(&machineLocation);
    /* Is Daylight Savings On?  If so, we need to add an hour to the offset. */
    if (machineLocation.u.dlsDelta != 0) {
	LL_UI2L(us2s, PRMJ_USEC_PER_SEC); /* seconds in a microseconds */
	LL_UI2L(dlsOffset, PRMJ_HOUR_SECONDS);  /* seconds in one hour       */
	LL_MUL(dlsOffset, dlsOffset, us2s);
    }
    else
	LL_I2L(dlsOffset, 0);
    return(dlsOffset);
#else
    time_t local;
    PRInt32 diff;
    PRInt64  maxtimet;
    struct tm tm;
#if defined( XP_PC ) || defined( FREEBSD ) || defined ( HPUX9 )
    struct tm *ptm;
#endif
    PRMJTime prtm;


    LL_UI2L(us2s, PRMJ_USEC_PER_SEC);
    LL_DIV(time, time, us2s);
    /* get the maximum of time_t value */
    LL_UI2L(maxtimet,PRMJ_MAX_UNIX_TIMET);

    if(LL_CMP(time,>,maxtimet)){
      LL_UI2L(time,PRMJ_MAX_UNIX_TIMET);
    } else if(!LL_GE_ZERO(time)){
      /*go ahead a day to make localtime work (does not work with 0) */
      LL_UI2L(time,PRMJ_DAY_SECONDS);
    }
    LL_L2UI(local,time);
    PRMJ_basetime(time,&prtm);
#if defined( XP_PC ) || defined( FREEBSD ) || defined ( HPUX9 )
    ptm = localtime(&local);
    if(!ptm){
      return LL_ZERO;
    }
    tm = *ptm;
#else
    localtime_r(&local,&tm); /* get dst information */
#endif

    diff = ((tm.tm_hour - prtm.tm_hour) * PRMJ_HOUR_SECONDS) +
	((tm.tm_min - prtm.tm_min) * 60);

    if(diff < 0){
	diff += PRMJ_DAY_SECONDS;
    }

    LL_UI2L(time,diff);

    LL_MUL(time,time,us2s);

    return(time);
#endif
}

PR_IMPLEMENT(PRInt64)
PRMJ_ToGMT(PRInt64 time)
{
    time_t gmtDiff;
    PRInt64 s2us, diff;
    PRInt64 dstdiff;

    gmtDiff = PRMJ_LocalGMTDifference();
    LL_I2L(diff, gmtDiff);
    LL_UI2L(s2us, PRMJ_USEC_PER_SEC);
    LL_MUL(diff,diff,s2us);
    dstdiff = PRMJ_DSTOffset(time);
    LL_SUB(diff,diff,dstdiff);
    LL_ADD(time,time,diff);
    return time;
}

/* Convert a GMT time value into a local time value */
PR_IMPLEMENT(PRInt64)
PRMJ_ToLocal(PRInt64 time)
{
    time_t gmtDiff;
    PRInt64 s2us, diff, dstdiff;

    gmtDiff = PRMJ_LocalGMTDifference();

    dstdiff = PRMJ_DSTOffset(time);
    LL_I2L(diff, gmtDiff);
    LL_UI2L(s2us, PRMJ_USEC_PER_SEC);
    LL_MUL(diff,diff,s2us);
    LL_SUB(time,time,diff);
    LL_ADD(time,time,dstdiff);
    return time;
}


/* Explode a 64 bit time value into its components */
PR_IMPLEMENT(void)
PRMJ_ExplodeTime(PRMJTime *to, PRInt64 time)
{
    PRInt64 s, us2s;
#ifdef PRMJ_DO_MILLISECONDS
    PRInt64 round, stime, us;
#endif

    /* Convert back to seconds since 0 */
    LL_UI2L(us2s,  PRMJ_USEC_PER_SEC);

#ifdef PRMJ_DO_MILLISECONDS
    if (!LL_GE_ZERO(time)) {
	/* correct for rounding of negative numbers */
	LL_UI2L(round, PRMJ_USEC_PER_SEC-1);
	LL_UI2L(stime,0);
	LL_ADD(stime,time,stime);
	LL_SUB(time,time,round);
	LL_DIV(s, time, us2s);
	LL_MUL(time,s,us2s);
	LL_SUB(us,stime,time);
    } else {
	LL_MOD(us,time,us2s);
	LL_DIV(s, time, us2s);
    }
#else
    LL_DIV(s, time, us2s);
#endif
    PRMJ_localtime(s,to);
#ifdef PRMJ_DO_MILLISECONDS
    LL_L2I(to->tm_usec, us);
#endif
}

/* Compute the 64 bit time value from the components */
PR_IMPLEMENT(PRInt64)
PRMJ_ComputeTime(PRMJTime *prtm)
{
    PRInt64 s, s2us;

    s = PRMJ_mktime(prtm);

    LL_UI2L(s2us, PRMJ_USEC_PER_SEC);
    LL_MUL(s, s, s2us);

#ifdef PRMJ_DO_MILLISECONDS
    {
	PRInt64 us;
	LL_UI2L(us, prtm->tm_usec);
	LL_ADD(s, s, us);
    }
#endif
    return s;
}

/* Format a time value into a buffer. Same semantics as strftime() */
PR_IMPLEMENT(size_t)
PRMJ_FormatTime(char *buf, int buflen, char *fmt, PRMJTime *prtm)
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
     * fields.
     */

#if defined(SUNOS4)
    if (mktime(&a) == -1) {
        PR_snprintf(buf, buflen, "can't get timezone");
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
 * This function replaces the mktime on each platform. mktime unfortunately
 * only handles time from January 1st 1970 until January 1st 2038. This
 * is not sufficient for most applications. This application will produce
 * time in seconds for any date.
 * XXX We also need to account for leap seconds...
 */
PR_IMPLEMENT(PRInt64)
PRMJ_mktime(PRMJTime *prtm)
{
  PRInt64 seconds;
  PRInt64 result;
  PRInt64 result1;
  PRInt64 result2;
  PRInt64 base;
  time_t secs;
  PRInt32   year = prtm->tm_year;
  PRInt32   month = prtm->tm_mon;
  PRInt32   day   = prtm->tm_mday;
  PRInt32   isleap = IS_LEAP(year);
  struct tm time;
#ifdef XP_MAC
  PRInt64  dstdiff;
  PRInt64  s2us;
  PRInt32  dstOffset;
#endif

  /* if between years we support just use standard mktime */
  if(year >= 1970 && year <= 2037){
      time.tm_sec  = prtm->tm_sec ;
      time.tm_min  = prtm->tm_min ;
      time.tm_hour = prtm->tm_hour;
      time.tm_mday = prtm->tm_mday;
      time.tm_mon  = prtm->tm_mon ;
      time.tm_wday = prtm->tm_wday;
      time.tm_year = prtm->tm_year-1900;
      time.tm_yday = prtm->tm_yday;
      time.tm_isdst = prtm->tm_isdst;
      if((secs = mktime(&time)) < 0){
	/* out of range use extended time */
	goto extended_time;
      }
#ifdef XP_MAC
      /* adjust MAC time to make relative to UNIX epoch */
      secs -= gJanuaryFirst1970Seconds;
      secs += PRMJ_LocalGMTDifference();
      LL_UI2L(dstdiff,0);
      dstdiff = PRMJ_DSTOffset(dstdiff);
      LL_UI2L(s2us,  PRMJ_USEC_PER_SEC);
      LL_DIV(dstdiff,dstdiff,s2us);
      LL_L2I(dstOffset,dstdiff);
      secs -= dstOffset;
      PRMJ_setDST(prtm);
#else
      prtm->tm_isdst = time.tm_isdst;
#endif
      prtm->tm_mday = time.tm_mday;
      prtm->tm_wday = time.tm_wday;
      prtm->tm_yday = time.tm_yday;
      prtm->tm_hour = time.tm_hour;
      prtm->tm_min  = time.tm_min;
      prtm->tm_mon  = time.tm_mon;

      LL_UI2L(seconds,secs);
      return(seconds);
  }

extended_time:
  LL_UI2L(seconds,0);
  LL_UI2L(result,0);
  LL_UI2L(result1,0);
  LL_UI2L(result2,0);

  /* calculate seconds in years */
  if(year > 0){
    LL_UI2L(result,year);
    LL_UI2L(result1,(365L * 24L));
    LL_MUL(result,result,result1);
    LL_UI2L(result1,PRMJ_HOUR_SECONDS);
    LL_MUL(result,result,result1);
    LL_UI2L(result1,((year-1)/4 - (year-1)/100 + (year-1)/400));
    LL_UI2L(result2,PRMJ_DAY_SECONDS);
    LL_MUL(result1,result1,result2);
    LL_ADD(seconds,result,result1);
  }

  /* calculate seconds in months */
  month--;

  for(;month >= 0; month--){
    LL_UI2L(result,(PRMJ_DAY_SECONDS * mtab[month]));
    LL_ADD(seconds,seconds,result);
    /* it's  a Feb */
    if(month == 1 && isleap != 0){
      LL_UI2L(result,PRMJ_DAY_SECONDS);
      LL_ADD(seconds,seconds,result);
    }
  }

  /* get the base time via UTC */
  base = PRMJ_ToExtendedTime(0);
  LL_UI2L(result,  PRMJ_USEC_PER_SEC);
  LL_DIV(base,base,result);

  /* calculate seconds for days */
  LL_UI2L(result,((day-1) * PRMJ_DAY_SECONDS));
  LL_ADD(seconds,seconds,result);
  /* calculate seconds for hours, minutes and seconds */
  LL_UI2L(result, (prtm->tm_hour * PRMJ_HOUR_SECONDS + prtm->tm_min * 60 +
		   prtm->tm_sec));
  LL_ADD(seconds,seconds,result);
  /* normalize to time base on positive for 1970, - for before that period */
  LL_SUB(seconds,seconds,base);

  /* set dst information */
  if(prtm->tm_isdst < 0)
      prtm->tm_isdst = 0;
  return seconds;
}

/*
 * basic time calculation functionality for localtime and gmtime
 * setups up prtm argument with correct values based upon input number
 * of seconds.
 */
static void
PRMJ_basetime(PRInt64 tsecs, PRMJTime *prtm)
{
    /* convert tsecs back to year,month,day,hour,secs */
    PRInt32 year    = 0;
    PRInt32 month   = 0;
    PRInt32 yday    = 0;
    PRInt32 mday    = 0;
    PRInt32 wday    = 6; /* start on a Sunday */
    PRInt32 days    = 0;
    PRInt32 seconds = 0;
    PRInt32 minutes = 0;
    PRInt32 hours   = 0;
    PRInt32 isleap  = 0;
    PRInt64 result;
    PRInt64	result1;
    PRInt64	result2;
    PRInt64 base;

    LL_UI2L(result,0);
    LL_UI2L(result1,0);
    LL_UI2L(result2,0);

    /* get the base time via UTC */
    base = PRMJ_ToExtendedTime(0);
    LL_UI2L(result,  PRMJ_USEC_PER_SEC);
    LL_DIV(base,base,result);
    LL_ADD(tsecs,tsecs,base);

    LL_UI2L(result, PRMJ_YEAR_SECONDS);
    LL_UI2L(result1,PRMJ_DAY_SECONDS);
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

    LL_UI2L(result1,PRMJ_DAY_SECONDS);

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
    LL_UI2L(result1,PRMJ_HOUR_SECONDS);
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
    prtm->tm_sec   = (PRInt8)seconds;
    prtm->tm_min   = (PRInt8)minutes;
    prtm->tm_hour  = (PRInt8)hours;
    prtm->tm_mday  = (PRInt8)mday;
    prtm->tm_mon   = (PRInt8)month;
    prtm->tm_wday  = (PRInt8)wday;
    prtm->tm_year  = (PRInt16)year;
    prtm->tm_yday  = (PRInt16)yday;
}

/*
 * This function replaces the localtime on each platform. localtime
 * unfortunately only handles time from January 1st 1970 until January 1st
 * 2038. This is not sufficient for most applications. This application will
 * produce time in seconds for any date.
 * TO Fix:
 * We also need to account for leap seconds...
 */
PR_IMPLEMENT(void)
PRMJ_localtime(PRInt64 tsecs,PRMJTime *prtm)
{
    time_t seconds;
    PRInt32    year;
    struct tm lt;
#ifdef XP_MAC
    PRInt64  dstdiff;
    PRInt64  s2us;
    PRInt32  dstOffset;
#endif
#ifdef XP_MAC
    LL_UI2L(dstdiff,0);
    dstdiff = PRMJ_DSTOffset(dstdiff);
    LL_UI2L(s2us,  PRMJ_USEC_PER_SEC);
    LL_DIV(dstdiff,dstdiff,s2us);
    LL_L2I(dstOffset,dstdiff);
    LL_ADD(tsecs,tsecs,dstdiff);
#endif
    PRMJ_basetime(tsecs,prtm);

    /* Adjust DST information in prtm
     * possible for us now only between 1970 and 2037
      */
    if((year = prtm->tm_year) >= 1970 && year <= 2037){
	LL_L2I(seconds,tsecs);
#ifdef XP_MAC
	/* adjust to the UNIX epoch  and add DST*/
	seconds += gJanuaryFirst1970Seconds;
	seconds -= PRMJ_LocalGMTDifference();

/* On the Mac PRMJ_LocalGMTDifference adjusts for DST */
/*	seconds += dstOffset;*/

#endif
#if defined(XP_PC) || defined(XP_MAC) || defined( FREEBSD ) || defined( HPUX9 )
	lt = *localtime(&seconds);
#else
	localtime_r(&seconds,&lt);
#endif
#ifdef XP_MAC
	PRMJ_setDST(prtm);
#else
	prtm->tm_isdst = lt.tm_isdst;
#endif
	prtm->tm_mday  = lt.tm_mday;
	prtm->tm_wday  = lt.tm_wday;
	prtm->tm_yday  = lt.tm_yday;
	prtm->tm_hour  = lt.tm_hour;
	prtm->tm_min   = lt.tm_min;
	prtm->tm_mon   = lt.tm_mon;
	prtm->tm_sec   = lt.tm_sec;
	prtm->tm_usec  = 0;
    }
    else
	prtm->tm_isdst = 0;
}



/*
 * This function takes the local time unadjusted for DST and returns
 * the GMT time.
 */
PR_IMPLEMENT(void)
PRMJ_gmtime(PRInt64 tsecs,PRMJTime *prtm)
{
    PRInt64 s2us;

    LL_UI2L(s2us, PRMJ_USEC_PER_SEC);
    LL_MUL(tsecs,tsecs,s2us);

    tsecs = PRMJ_ToGMT(tsecs);

    LL_DIV(tsecs,tsecs,s2us);
    PRMJ_basetime(tsecs,prtm);
}



/* The following string arrays and macros are used by PRMJ_FormatTimeUSEnglish().
 * XXX export for use by jsdate.c etc.
 */

static const char* abbrevDays[] =
{
   "Sun","Mon","Tue","Wed","Thu","Fri","Sat"
};

static const char* days[] =
{
   "Sunday","Monday","Tuesday","Wednesday","Thursday","Friday","Saturday"
};

static const char* abbrevMonths[] =
{
   "Jan", "Feb", "Mar", "Apr", "May", "Jun", "Jul", "Aug", "Sep", "Oct", "Nov", "Dec"
};

static const char* months[] =
 {
    "January", "February", "March", "April", "May", "June",
    "July", "August", "September", "October", "November", "December"
};


/* Add a single character to the given buffer, incrementing the buffer pointer
 * and decrementing the buffer size. Return 0 on error.
 */
#define ADDCHAR( buf, bufSize, ch )             \
do                                              \
{                                               \
   if( bufSize < 1 )                            \
   {                                            \
      *(--buf) = '\0';                          \
      return 0;                                 \
   }                                            \
   *buf++ = ch;                                 \
   bufSize--;                                   \
}                                               \
while(0)


/* Add a string to the given buffer, incrementing the buffer pointer and decrementing
 * the buffer size appropriately. Return 0 on error.
 */
#define ADDSTR( buf, bufSize, str )             \
do                                              \
{                                               \
   size_t strSize = strlen( str );              \
   if( strSize > bufSize )                      \
   {                                            \
      if( bufSize==0 )                          \
         *(--buf) = '\0';                       \
      else                                      \
         *buf = '\0';                           \
      return 0;                                 \
   }                                            \
   memcpy(buf, str, strSize);                   \
   buf += strSize;                              \
   bufSize -= strSize;                          \
}                                               \
while(0)

/* Needed by PR_FormatTimeUSEnglish() */
static unsigned int  pr_WeekOfYear(const PRMJTime* time, unsigned int firstDayOfWeek);


/***********************************************************************************
 *
 * Description:
 *  This is a dumbed down version of strftime that will format the date in US
 *  English regardless of the setting of the global locale.  This functionality is
 *  needed to write things like MIME headers which must always be in US English.
 *
 **********************************************************************************/

PR_IMPLEMENT(size_t)
PRMJ_FormatTimeUSEnglish( char* buf, size_t bufSize,
                        const char* format, const PRMJTime* time )
{
   char*         bufPtr = buf;
   const char*   fmtPtr;
   char          tmpBuf[ 40 ];
   const int     tmpBufSize = sizeof( tmpBuf );


   for( fmtPtr=format; *fmtPtr != '\0'; fmtPtr++ )
   {
      if( *fmtPtr != '%' )
      {
         ADDCHAR( bufPtr, bufSize, *fmtPtr );
      }
      else
      {
         switch( *(++fmtPtr) )
         {
         case '%':
            /* escaped '%' character */
            ADDCHAR( bufPtr, bufSize, '%' );
            break;

         case 'a':
            /* abbreviated weekday name */
            ADDSTR( bufPtr, bufSize, abbrevDays[ time->tm_wday ] );
            break;

         case 'A':
            /* full weekday name */
            ADDSTR( bufPtr, bufSize, days[ time->tm_wday ] );
            break;

         case 'b':
            /* abbreviated month name */
            ADDSTR( bufPtr, bufSize, abbrevMonths[ time->tm_mon ] );
            break;

         case 'B':
            /* full month name */
            ADDSTR(bufPtr, bufSize,  months[ time->tm_mon ] );
            break;

         case 'c':
            /* Date and time. */
            PRMJ_FormatTimeUSEnglish( tmpBuf, tmpBufSize, "%a %b %d %H:%M:%S %Y", time );
            ADDSTR( bufPtr, bufSize, tmpBuf );
            break;

         case 'd':
            /* day of month ( 01 - 31 ) */
            PR_snprintf(tmpBuf,tmpBufSize,"%.2d",time->tm_mday );
            ADDSTR( bufPtr, bufSize, tmpBuf );
            break;

         case 'H':
            /* hour ( 00 - 23 ) */
            PR_snprintf(tmpBuf,tmpBufSize,"%.2d",time->tm_hour );
            ADDSTR( bufPtr, bufSize, tmpBuf );
            break;

         case 'I':
            /* hour ( 01 - 12 ) */
            PR_snprintf(tmpBuf,tmpBufSize,"%.2d",
                        (time->tm_hour%12) ? time->tm_hour%12 : 12 );
            ADDSTR( bufPtr, bufSize, tmpBuf );
            break;

         case 'j':
            /* day number of year ( 001 - 366 ) */
            PR_snprintf(tmpBuf,tmpBufSize,"%.3d",time->tm_yday + 1);
            ADDSTR( bufPtr, bufSize, tmpBuf );
            break;

         case 'm':
            /* month number ( 01 - 12 ) */
            PR_snprintf(tmpBuf,tmpBufSize,"%.2d",time->tm_mon+1);
            ADDSTR( bufPtr, bufSize, tmpBuf );
            break;

         case 'M':
            /* minute ( 00 - 59 ) */
            PR_snprintf(tmpBuf,tmpBufSize,"%.2d",time->tm_min );
            ADDSTR( bufPtr, bufSize, tmpBuf );
            break;

         case 'p':
            /* locale's equivalent of either AM or PM */
            ADDSTR( bufPtr, bufSize, (time->tm_hour<12)?"AM":"PM" );
            break;

         case 'S':
            /* seconds ( 00 - 61 ), allows for leap seconds */
            PR_snprintf(tmpBuf,tmpBufSize,"%.2d",time->tm_sec );
            ADDSTR( bufPtr, bufSize, tmpBuf );
            break;

         case 'U':
            /* week number of year ( 00 - 53  ),  Sunday  is  the first day of week 1 */
            PR_snprintf(tmpBuf,tmpBufSize,"%.2d", pr_WeekOfYear( time, 0 ) );
            ADDSTR( bufPtr, bufSize, tmpBuf );
            break;

         case 'w':
            /* weekday number ( 0 - 6 ), Sunday = 0 */
            PR_snprintf(tmpBuf,tmpBufSize,"%d",time->tm_wday );
            ADDSTR( bufPtr, bufSize, tmpBuf );
            break;

         case 'W':
            /* Week number of year ( 00 - 53  ),  Monday  is  the first day of week 1 */
            PR_snprintf(tmpBuf,tmpBufSize,"%.2d", pr_WeekOfYear( time, 1 ) );
            ADDSTR( bufPtr, bufSize, tmpBuf );
            break;

         case 'x':
            /* Date representation */
            PRMJ_FormatTimeUSEnglish( tmpBuf, tmpBufSize, "%m/%d/%y", time );
            ADDSTR( bufPtr, bufSize, tmpBuf );
            break;

         case 'X':
            /* Time representation. */
            PRMJ_FormatTimeUSEnglish( tmpBuf, tmpBufSize, "%H:%M:%S", time );
            ADDSTR( bufPtr, bufSize, tmpBuf );
            break;

         case 'y':
            /* year within century ( 00 - 99 ) */
            PR_snprintf(tmpBuf,tmpBufSize,"%.2d",time->tm_year % 100 );
            ADDSTR( bufPtr, bufSize, tmpBuf );
            break;

         case 'Y':
            /* year as ccyy ( for example 1986 ) */
            PR_snprintf(tmpBuf,tmpBufSize,"%.4d",time->tm_year );
            ADDSTR( bufPtr, bufSize, tmpBuf );
            break;

         case 'Z':
            /* Time zone name or no characters if  no  time  zone exists.
             * Since time zone name is supposed to be independant of locale, we
             * defer to PR_FormatTime() for this option.
             */
            PRMJ_FormatTime( tmpBuf, tmpBufSize, "%Z", (PRMJTime*)time );
            ADDSTR( bufPtr, bufSize, tmpBuf );
            break;

         default:
            /* Unknown format.  Simply copy format into output buffer. */
            ADDCHAR( bufPtr, bufSize, '%' );
            ADDCHAR( bufPtr, bufSize, *fmtPtr );
            break;

         }
      }
   }

   ADDCHAR( bufPtr, bufSize, '\0' );
   return (size_t)(bufPtr - buf - 1);
}



/***********************************************************************************
 *
 * Description:
 *  Returns the week number of the year (0-53) for the given time.  firstDayOfWeek
 *  is the day on which the week is considered to start (0=Sun, 1=Mon, ...).
 *  Week 1 starts the first time firstDayOfWeek occurs in the year.  In other words,
 *  a partial week at the start of the year is considered week 0.
 *
 **********************************************************************************/

static unsigned int pr_WeekOfYear(const PRMJTime* time, unsigned int firstDayOfWeek)
{
   int dayOfWeek;
   int dayOfYear;

  /* Get the day of the year for the given time then adjust it to represent the
   * first day of the week containing the given time.
   */
  dayOfWeek = time->tm_wday - firstDayOfWeek;
  if (dayOfWeek < 0)
    dayOfWeek += 7;

  dayOfYear = time->tm_yday - dayOfWeek;


  if( dayOfYear <= 0 )
  {
     /* If dayOfYear is <= 0, it is in the first partial week of the year. */
     return 0;
  }
  else
  {
     /* Count the number of full weeks ( dayOfYear / 7 ) then add a week if there
      * are any days left over ( dayOfYear % 7 ).  Because we are only counting to
      * the first day of the week containing the given time, rather than to the
      * actual day representing the given time, any days in week 0 will be "absorbed"
      * as extra days in the given week.
      */
     return (dayOfYear / 7) + ( (dayOfYear % 7) == 0 ? 0 : 1 );
  }
}

