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

// duration.cpp
// John Sun
// 5:52 PM January 28 1998

#include "stdafx.h"
#include "jdefines.h"

#include <assert.h>
#include <ptypes.h>
#include <unistring.h>
#include "duration.h"
#include "jutility.h"

// turn this flag on to allow for parsing of year and month
#define NSCALDURATION_PARSING_YEAR_AND_MONTH 1
//---------------------------------------------------------------------

void nsCalDuration::setInvalidDuration()
{
    m_iYear = -1;
    m_iMonth = -1;
    m_iDay = -1;
    m_iHour = -1;
    m_iMinute = -1;
    m_iSecond = -1;
    m_iWeek = -1;
}

//---------------------------------------------------------------------

void nsCalDuration::parse(UnicodeString & us)
{
    // NOTE: use a better algorithm like a regexp scanf or something.

    UnicodeString sVal;
    UnicodeString sSection;

    t_bool bErrorInParse = FALSE;
    t_int32 startOfParse = 0, indexOfT = 0, endOfParse = 0;
    t_int32 day = 0, hour = 0, minute = 0, second = 0, week = 0;
#if NSCALDURATION_PARSING_YEAR_AND_MONTH
    t_int32 year = 0, month = 0;
    t_int32 indexOfY = 0, indexOfMo = 0;
#endif
    t_int32 indexOfD = 0;
    t_int32 indexOfH = 0, indexOfMi = 0, indexOfS = 0, indexOfW = 0;
    
    char * cc = 0;

    if (us[(TextOffset) 0] != 'P') //!us.startsWith(s_sP))    
    {
        if ('-' == us[(TextOffset) 0] && ('P' == us[(TextOffset) 1]))
        {
            m_NegativeDuration = TRUE;
            startOfParse = 1;
        }
        else if ('+' == us[(TextOffset) 0] && ('P' == us[(TextOffset) 1]))
        {
            m_NegativeDuration = FALSE;
            startOfParse = 1;
        }
        else
            bErrorInParse = TRUE;
    }
    
    if (!bErrorInParse)
    {
        startOfParse++;
        indexOfT = us.indexOf('T');
        endOfParse = us.size();
        indexOfW = us.indexOf('W');
        if (indexOfW == -1)
        {
            // first parse the date section
            if (indexOfT > 0)
            {
                endOfParse = indexOfT;
            }
            sSection = us.extractBetween(startOfParse, endOfParse, sSection);
            startOfParse = 0;
            endOfParse = sSection.size();

#if NSCALDURATION_PARSING_YEAR_AND_MONTH
            indexOfY = sSection.indexOf('Y');
            if (indexOfY >= 0)
            {
                sVal = sSection.extractBetween(startOfParse, indexOfY, sVal);
                cc = sVal.toCString("");
                PR_ASSERT(cc != 0);
                year = nsCalUtility::atot_int32(cc, 
                    bErrorInParse, sVal.size());
                delete [] cc;
                startOfParse = indexOfY + 1;
            }
            indexOfMo = sSection.indexOf('M');
            if (indexOfMo >= 0)
            {
                sVal = sSection.extractBetween(startOfParse, indexOfMo, sVal);
                cc = sVal.toCString("");
                PR_ASSERT(cc != 0);
                month = nsCalUtility::atot_int32(cc, 
                    bErrorInParse, sVal.size());
                delete [] cc;
                startOfParse = indexOfMo + 1;
            }
#endif
            indexOfD = sSection.indexOf('D');
            if (indexOfD >= 0) 
            {
                sVal = sSection.extractBetween(startOfParse, indexOfD, sVal);
                cc = sVal.toCString("");
                PR_ASSERT(cc != 0);
                day = nsCalUtility::atot_int32(cc, 
                    bErrorInParse, sVal.size());
                delete [] cc;
                startOfParse = indexOfD + 1;
            }
#if NSCALDURATION_PARSING_YEAR_AND_MONTH
            if (sSection.size() >= 1 && indexOfY == -1 &&             
                indexOfMo == -1 && indexOfD == -1)
            {
                bErrorInParse = TRUE;
            }
            else if (indexOfY != -1 && indexOfMo != -1 && indexOfY > indexOfMo)
                bErrorInParse = TRUE;
            else if (indexOfY != -1 && indexOfD != -1 && indexOfY > indexOfD)
                bErrorInParse = TRUE;
            else if (indexOfMo != -1 && indexOfD != -1 && indexOfMo > indexOfD)
                bErrorInParse = TRUE;
            else if (indexOfD != -1 && indexOfD != endOfParse -1)
                bErrorInParse = TRUE;
            else if (indexOfMo != -1 && indexOfD == -1 && indexOfMo != endOfParse -1)
                bErrorInParse = TRUE;
            else if (indexOfY != -1 && indexOfD == -1 && indexOfMo == -1 && indexOfY != endOfParse -1)
                bErrorInParse = TRUE;
#else 
            if (sSection.size() >= 1 && indexOfD == -1)
            {
                bErrorInParse = TRUE;
            }
            else if (indexOfD != -1 && indexOfD != endOfParse -1)
                bErrorInParse = TRUE;
#endif

            // now parse time section
            if (indexOfT > 0)
            {
                startOfParse = indexOfT + 1;
                endOfParse = us.size();
                sSection = us.extractBetween(startOfParse, endOfParse, sSection);
                startOfParse = 0;
                endOfParse = sSection.size();
                indexOfH = sSection.indexOf('H');
                if (indexOfH >= 0) {
                    sVal = sSection.extractBetween(startOfParse, indexOfH, sVal);
                    cc = sVal.toCString("");
                    PR_ASSERT(cc != 0);
                    hour = nsCalUtility::atot_int32(cc,
                        bErrorInParse, sVal.size());
                    delete [] cc;
                    startOfParse = indexOfH + 1;
                }
                indexOfMi = sSection.indexOf('M');
                
                if (indexOfMi >= 0) {
                    sVal = sSection.extractBetween(startOfParse, indexOfMi, sVal);
                    cc = sVal.toCString("");
                    PR_ASSERT(cc != 0);
                    minute = nsCalUtility::atot_int32(cc,
                        bErrorInParse, sVal.size());
                    delete [] cc;
                    startOfParse = indexOfMi + 1;
                }
                indexOfS = sSection.indexOf('S');
                if (indexOfS >= 0)
                {
                    sVal = sSection.extractBetween(startOfParse, indexOfS, sVal);
                    cc = sVal.toCString("");
                    PR_ASSERT(cc != 0);
                    second = nsCalUtility::atot_int32(cc,
                        bErrorInParse, sVal.size());
                    delete [] cc;
                    startOfParse = indexOfS + 1;                
                }
                if (sSection.size() > 0 && indexOfH == -1 &&
                    indexOfMi == -1 && indexOfS == -1)
                {
                    bErrorInParse = TRUE;
                }
                else if (indexOfH != -1 && indexOfMi != -1 && indexOfH > indexOfMi)
                    bErrorInParse = TRUE;
                else if (indexOfH != -1 && indexOfS != -1 && indexOfH > indexOfS)
                    bErrorInParse = TRUE;
                else if (indexOfMi != -1 && indexOfS != -1 && indexOfMi > indexOfS)
                    bErrorInParse = TRUE;
                else if (indexOfS != -1 && indexOfS != endOfParse -1)
                    bErrorInParse = TRUE;
                else if (indexOfMi != -1 && indexOfS == -1 && indexOfMi != endOfParse -1)
                    bErrorInParse = TRUE;
                else if (indexOfH != -1 && indexOfS == -1 && indexOfMi == -1 && indexOfH != endOfParse -1)
                    bErrorInParse = TRUE;
            }
        }
        else
        {
            // week parse
            if (us.size() - 1 != indexOfW)
                bErrorInParse = TRUE;
          
            sVal = us.extractBetween(1, us.size() - 1, sVal);
            cc = sVal.toCString("");
                    PR_ASSERT(cc != 0);
            week = nsCalUtility::atot_int32(cc,
                bErrorInParse, sVal.size());
            delete [] cc;           
        }
   } 
   //if (FALSE) TRACE("parse: %d W, %d Y %d M %d D T %d H %d M %d S\r\n", week, year, month, day, hour, minute, second);
#if NSCALDURATION_PARSING_YEAR_AND_MONTH
   if (year < 0 || month < 0 || day < 0 ||
       hour < 0 || minute < 0 || second < 0 || week < 0)
       bErrorInParse = TRUE;
#else
    if (day < 0 || hour < 0 || minute < 0 || second < 0 || week < 0)
       bErrorInParse = TRUE;
#endif

   
   if (bErrorInParse)
   {
        // LOG the error
        setInvalidDuration();
   }
   else
   {
#if NSCALDURATION_PARSING_YEAR_AND_MONTH

        m_iYear = year;
        m_iMonth = month;
#else
        m_iYear = 0;
        m_iMonth = 0;
#endif
        m_iDay = day;
        m_iHour = hour;
        m_iMinute = minute;
        m_iSecond = second;
        m_iWeek = week;
        normalize();
   }
}

//---------------------------------------------------------------------

//////////////////////////////
// CONSTRUCTORS
//////////////////////////////

//---------------------------------------------------------------------

nsCalDuration::nsCalDuration()
:
    m_iYear(0),m_iMonth(0),m_iDay(0),m_iHour(0),
    m_iMinute(0),m_iSecond(0), m_iWeek(0),
    m_NegativeDuration(FALSE)
{
}

//---------------------------------------------------------------------

nsCalDuration::nsCalDuration(UnicodeString & us)
:
    m_iYear(0),m_iMonth(0),m_iDay(0),m_iHour(0),
    m_iMinute(0),m_iSecond(0), m_iWeek(0),
    m_NegativeDuration(FALSE)
{
    parse(us);
}

//---------------------------------------------------------------------

nsCalDuration::nsCalDuration(t_int32 type, t_int32 value)
:
    m_iYear(0),m_iMonth(0),m_iDay(0),m_iHour(0),
    m_iMinute(0),m_iSecond(0), m_iWeek(0),
    m_NegativeDuration(FALSE)
{
    switch (type)
    {
    case nsCalUtility::RT_MINUTELY:
        m_iMinute = value;
        break;
    case nsCalUtility::RT_HOURLY:
        m_iHour = value;
        break;
    case nsCalUtility::RT_DAILY:
        m_iDay = value;
        break;
    case nsCalUtility::RT_WEEKLY:
        m_iWeek = value;
        break;
    case nsCalUtility::RT_MONTHLY:
        m_iMonth = value;
        break;
    case nsCalUtility::RT_YEARLY:
        m_iYear = value;
        break;
    default:
        break;
    }
}

//---------------------------------------------------------------------

nsCalDuration::nsCalDuration(nsCalDuration & d)
:
    m_iYear(0),m_iMonth(0),m_iDay(0),m_iHour(0),
    m_iMinute(0),m_iSecond(0), m_iWeek(0), 
    m_NegativeDuration(FALSE)
{
    t_bool b;

    m_iYear = d.m_iYear;
    m_iMonth = d.m_iMonth;
    m_iDay = d.m_iDay;
    m_iHour = d.m_iHour;
    m_iMinute = d.m_iMinute;
    m_iSecond = d.m_iSecond;
    m_iWeek = d.m_iWeek;
    m_NegativeDuration = d.m_NegativeDuration;

    // check postcondition
    b = ((m_iWeek != 0) && (m_iYear != 0 || m_iMonth != 0 || 
        m_iDay != 0 || m_iHour !=0 || m_iMinute != 0 || 
        m_iSecond != 0));

    if (!b) {
        //PR_ASSERT(!b);
        //JLog::Instance()->logString(ms_sDurationAssertionFailed);
        //setInvalidDuration();
    }
}
//---------------------------------------------------------------------

nsCalDuration::nsCalDuration(t_int32 year, t_int32 month, t_int32 day, 
                   t_int32 hour, t_int32 min, t_int32 sec, 
                   t_int32 week, t_bool bNegativeDuration)   
{
    if (week > 0)
    {
        set(year, month, day, hour, min, sec, bNegativeDuration);
    }
    else
    {
        set(week, bNegativeDuration);
    }
}

//---------------------------------------------------------------------

///////////////////////////////////
// DESTRUCTORS
///////////////////////////////////

//---------------------------------------------------------------------

nsCalDuration::~nsCalDuration() 
{
}

//---------------------------------------------------------------------

///////////////////////////////////
// GETTERS AND SETTERS
///////////////////////////////////
void nsCalDuration::set(t_int32 year, t_int32 month, t_int32 day, 
                    t_int32 hour, t_int32 min, t_int32 sec, t_bool
                    bNegativeDuration)
{
    m_iYear = year;
    m_iMonth = month;
    m_iDay = day;
    m_iHour = hour;
    m_iMinute = min;
    m_iSecond = sec;
    m_NegativeDuration = bNegativeDuration;
    m_iWeek = 0;
}

void nsCalDuration::set(t_int32 week, t_bool bNegativeDuration)
{
    m_iWeek = week;
    m_NegativeDuration = bNegativeDuration;
    m_iYear = 0;
    m_iMonth = 0;
    m_iDay = 0;
    m_iHour = 0;
    m_iMinute = 0;
    m_iSecond = 0;
}

t_int32 nsCalDuration::getYear() const
{
    if (m_NegativeDuration)
        return (0 - m_iYear);
    return m_iYear;
}

t_int32 nsCalDuration::getMonth() const
{
    if (m_NegativeDuration)
        return (0 - m_iMonth);
    return m_iMonth;
}

t_int32 nsCalDuration::getDay() const
{
    if (m_NegativeDuration)
        return (0 - m_iDay);
    return m_iDay;
}

t_int32 nsCalDuration::getHour() const
{
    if (m_NegativeDuration)
        return (0 - m_iHour);
    return m_iHour;
}

t_int32 nsCalDuration::getMinute() const
{
    if (m_NegativeDuration)
        return (0 - m_iMinute);
    return m_iMinute;
}

t_int32 nsCalDuration::getSecond() const
{
    if (m_NegativeDuration)
        return (0 - m_iSecond);
    return m_iSecond;
}

t_int32 nsCalDuration::getWeek() const
{
    if (m_NegativeDuration)
        return (0 - m_iWeek);
    return m_iWeek;
}

//---------------------------------------------------------------------

///////////////////////////////////
// UTILITIES
///////////////////////////////////

//---------------------------------------------------------------------

void nsCalDuration::setDurationString(UnicodeString & s)
{
    parse(s);
}

//---------------------------------------------------------------------

// TODO - will work if only normalize works
t_int32 nsCalDuration::compareTo(const nsCalDuration & d)
{
    normalize();

    if (getYear() != d.getYear())
        return ((getYear() > d.getYear()) ? 1 : -1);
    if (getMonth() != d.getMonth())
        return ((getMonth() > d.getMonth()) ? 1 : -1);
    if (getDay() != d.getDay())
        return ((getDay() > d.getDay()) ? 1 : -1);
    if (getHour() != d.getHour())
        return ((getHour() > d.getHour()) ? 1 : -1);
    if (getMinute() != d.getMinute())
        return ((getMinute() > d.getMinute()) ? 1 : -1);
    if (getSecond() != d.getSecond())
        return ((getSecond() > d.getSecond()) ? 1 : -1);
    if (getWeek() != d.getWeek())
        return ((getWeek() > d.getWeek()) ? 1 : -1);
    return 0;
}

//---------------------------------------------------------------------

void nsCalDuration::normalize()
{
    // TODO: RISKY, assumes 30 days in a month, 12 months in a year
    t_int32 temp1 = 0, temp2 = 0, temp3 = 0, temp4 = 0, temp5 = 0;
    if (m_iSecond >= 60)
    {
      temp1 = m_iSecond / 60;
      m_iSecond = m_iSecond % 60;
    }
    m_iMinute = m_iMinute + temp1;
    if (m_iMinute >= 60) {
      temp2 = m_iMinute / 60;
      m_iMinute = m_iMinute % 60;
    }
    m_iHour = m_iHour + temp2;
    if (m_iHour >= 24) {
      temp3 = m_iHour / 24;
      m_iHour = m_iHour % 24;
    }
    m_iDay = m_iDay + temp3;
    if (m_iDay >= 30) {
      temp4 = m_iDay / 30;
      m_iDay = m_iDay % 30;
    }
    m_iMonth = m_iMonth + temp4;
    if (m_iMonth >= 12) {
      temp5 = m_iMonth / 12;
      m_iMonth = m_iMonth % 12;
    }
    m_iYear = m_iYear + temp5;
}

//---------------------------------------------------------------------

void nsCalDuration::unnormalize()
{
    // TODO: watch out for overflow
    if (m_iYear > 0)
    {
        m_iMonth += (12 * m_iYear);
        m_iYear = 0;
    }
    if (m_iMonth > 0)
    {
        m_iDay += (30 * m_iMonth);
        m_iMonth = 0;
    }
}

//---------------------------------------------------------------------

t_bool
nsCalDuration::isZeroLength()
{
    if (m_iYear <= 0 && m_iMonth <= 0 && m_iDay <= 0 && 
        m_iHour <= 0 && m_iMinute <= 0 && m_iSecond <= 0 && 
        m_iWeek <= 0)
        return TRUE;
    else 
        return FALSE;
}

//---------------------------------------------------------------------

UnicodeString nsCalDuration::toString()
{
    char sBuf[100];
    char sBuf2[100];

	sBuf[0] = '\0';

    if (m_iWeek != 0)
        sprintf(sBuf, "%d weeks", getWeek());
    else
    {
        // Full form

		// eyork Still looking into Libnls to see if strings for year, month, etc are there
        if (getYear() != 0)	{ sprintf(sBuf2, getYear() > 1 ? "%d years " : "%d year ", getYear()); strcat(sBuf, sBuf2); }
        if (getMonth() != 0) { sprintf(sBuf2, getMonth() > 1 ? "%d months " : "%d month ", getMonth()); strcat(sBuf, sBuf2); }
        if (getDay() != 0) { sprintf(sBuf2, getDay() > 1 ? "%d days " : "%d day ", getDay()); strcat(sBuf, sBuf2); }
        if (getHour() != 0) { sprintf(sBuf2, getHour() > 1 ? "%d hours " : "%d hour ", getHour()); strcat(sBuf, sBuf2); }
        if (getMinute() != 0) { sprintf(sBuf2, getMinute() > 1 ? "%d minutes " : "%d minute ", getMinute()); strcat(sBuf, sBuf2); }
        if (getSecond() != 0) { sprintf(sBuf2, getSecond() > 1 ? "%d seconds " : "%d second ", getSecond()); strcat(sBuf, sBuf2); }

		/*
        sprintf(sBuf, 
        "%d year, %d month , %d day, %d hour, %d mins, %d secs",
            m_iYear, m_iMonth, m_iDay, m_iHour, m_iMinute, m_iSecond);
		*/
    }
    return sBuf;
    // return toICALString();
}

//---------------------------------------------------------------------

UnicodeString nsCalDuration::toICALString()
{
    //if (FALSE) TRACE("toString: %d W, %d Y %d M %d D T %d H %d M %d S\r\n", m_iYear, m_iMonth, m_iDay, m_iHour, m_im_iMinute, m_iSecond);
    

    if (!isValid())
    {
        return "INVALID_DURATION";
        //sOut = "PT-1H";
        //return sOut;
    }
    else if (m_iWeek != 0)
    {
        char sNum[20];
        sprintf(sNum, "%sP%dW", ((m_NegativeDuration) ? "-" : ""), m_iWeek); 
        return sNum;
    }
    else
    {    

#if 1
        // full form
        char sNum[100];
#if NSCALDURATION_PARSING_YEAR_AND_MONTH
        sprintf(sNum, "%sP%dY%dM%dDT%dH%dM%dS", 
            ((m_NegativeDuration) ? "-" : ""), m_iYear, m_iMonth, m_iDay, m_iHour, m_iMinute, m_iSecond);
#else
        unnormalize();
        sprintf(sNum, "%sP%dDT%dH%dM%dS", 
            ((m_NegativeDuration) ? "-" : ""), m_iDay, m_iHour, m_iMinute, m_iSecond);
#endif
        return sNum;
#else

        // condensed form
        UnicodeString sOut;
        UnicodeString sDate = "", sTime = "";
        t_int32 aiDate[] = { m_iYear, m_iMonth, m_iDay };
        char asDate[] = { 'Y', 'M', 'D' };
        t_int32 aiTime[] = { m_iHour, m_iMinute, m_iSecond };
        char asTime[] = { 'H', 'M', 'S' };
        t_int32 kSize = 3; 
        int i;

        for (i = 0; i < kSize ; i++) {
            if (aiDate[i] != 0) {
                char sNum[20];
                sprintf(sNum, "%d", aiDate[i]); 
                sDate += sNum;
                sDate += asDate[i];
            }   
        }
        for (i = 0; i < kSize ; i++) {
            if (aiTime[i] != 0) {
                char sNum[20];
                sprintf(sNum, "%d", aiTime[i]); 
                sTime += sNum;
                sTime += asTime[i];
            }   
        }
        if (sTime.size() > 0) {
            sTime.insert(0, "T");
        }
        sOut = sDate;
        sOut += sTime;
      
        if (sOut.size() > 0) {
            sOut.insert(0, "P");
            if (m_NegativeDuration)
                sOut.insert(0, "-");
        }
     
        return sOut;

#endif
    }
}

//---------------------------------------------------------------------

t_bool nsCalDuration::isValid()
{
    if (m_iYear < 0 || m_iMonth < 0 || m_iDay < 0 ||
        m_iHour < 0 || m_iMinute < 0 || m_iSecond < 0 || 
        m_iWeek < 0)
      return FALSE;
    else return TRUE;
}
//---------------------------------------------------------------------

////////////////////////////////////
// OVERLOADED OPERATORS
////////////////////////////////////

//---------------------------------------------------------------------

const nsCalDuration & nsCalDuration::operator=(const nsCalDuration& d)
{
    m_iYear = d.m_iYear;
    m_iMonth = d.m_iMonth;
    m_iDay = d.m_iDay;
    m_iHour = d.m_iHour;
    m_iMinute = d.m_iMinute;
    m_iSecond = d.m_iSecond;
    m_iWeek = d.m_iWeek;
    m_NegativeDuration = d.m_NegativeDuration;

    return *this;
}

//---------------------------------------------------------------------

t_bool nsCalDuration::operator==(const nsCalDuration & that) 
{
    return (compareTo(that) == 0);
}

//---------------------------------------------------------------------

t_bool nsCalDuration::operator!=(const nsCalDuration & that) 
{
    return (compareTo(that) != 0);
}

//---------------------------------------------------------------------

