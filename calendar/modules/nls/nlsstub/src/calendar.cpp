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


#include "nscore.h"
#include "calendar.h"
#include "prtime.h"

Calendar::Calendar()
{
  mTimeZone = nsnull;
  LL_I2L(mTime, 0);
  PR_ExplodeTime(mTime, PR_GMTParameters, &mExplodedTime);
}

Calendar::~Calendar()
{
}

Date Calendar::getNow()
{
  Date d;

  PRTime t;

  t = (PRTime) PR_Now();

  LL_L2D(d,t); 
  
  return ((Date)d);
}

void Calendar::setTimeZone(const TimeZone& aZone)
{
  mTimeZone = (TimeZone *)(&aZone);
  return ;
}

Date Calendar::getTime(ErrorCode& aStatus) const
{
  Date d;

  LL_L2D(d,mTime);

  return ((Date)d);
}


PRInt32 Calendar::get(EDateFields aField, ErrorCode& aStatus) const
{
  PRInt32 field = 0;

  PR_ExplodeTime(mTime, PR_GMTParameters, (PRExplodedTime*)(&mExplodedTime));

  switch(aField)
  {
    case SECOND:
      field = mExplodedTime.tm_sec;
      break;

    case DAY_OF_WEEK_IN_MONTH:
      field = 0;
      break;

    case MINUTE:
      field = mExplodedTime.tm_min;
      break;

    case HOUR:
      field = mExplodedTime.tm_hour;
      break;

    case DAY_OF_YEAR:
      field = mExplodedTime.tm_yday;
      break;

    case WEEK_OF_YEAR:
      field = 0;
      break;

    case MONTH:
      field = mExplodedTime.tm_month;
      break;

    case DATE:
      field = mExplodedTime.tm_mday;
      break;

    case DAY_OF_WEEK:
      field = mExplodedTime.tm_wday;
      break;

    case DAY_OF_MONTH:
      field = 0;
      break;

    case HOUR_OF_DAY:
      field = mExplodedTime.tm_hour;
      break;

    case YEAR:
      field = mExplodedTime.tm_year;
      break;

    default:
      field = 0;
      break;

  }

  return (field);
}

void Calendar::setTime(Date aDate, ErrorCode& aStatus)
{
  LL_D2L(mTime,aDate);
  PR_ExplodeTime(mTime, PR_GMTParameters, &mExplodedTime);
  return;
}

void Calendar::set(EDateFields aField, PRInt32 aValue)
{
  switch(aField)
  {
    case SECOND:
      mExplodedTime.tm_sec = aValue;
      break;

    case DAY_OF_WEEK_IN_MONTH:
      break;

    case MINUTE:
      mExplodedTime.tm_min = aValue;
      break;

    case HOUR:
      mExplodedTime.tm_hour = aValue;
      break;

    case DAY_OF_YEAR:
      break;

    case WEEK_OF_YEAR:
      break;

    case MONTH:
      mExplodedTime.tm_month = aValue;
      break;

    case DATE:
      mExplodedTime.tm_mday = aValue;
      break;

    case DAY_OF_WEEK:
      break;

    case DAY_OF_MONTH:
      break;

    case HOUR_OF_DAY:
      mExplodedTime.tm_hour = aValue;
      break;

    case YEAR:
      mExplodedTime.tm_year = aValue;
      break;

    default:
      break;

  }

  mTime = PR_ImplodeTime(&mExplodedTime);
  PR_ExplodeTime(mTime, PR_GMTParameters, &mExplodedTime);

  return ;
}

void Calendar::set(PRInt32 aYear, PRInt32 aMonth, PRInt32 aDate)
{
  mExplodedTime.tm_year  = aYear;
  mExplodedTime.tm_month = aMonth;
  mExplodedTime.tm_mday  = aDate;

  mTime = PR_ImplodeTime(&mExplodedTime);

  PR_ExplodeTime(mTime, PR_GMTParameters, &mExplodedTime);

  return ;
}

void Calendar::set(PRInt32 aYear, PRInt32 aMonth, PRInt32 aDate, PRInt32 aHour, PRInt32 aMinute)
{
  mExplodedTime.tm_year  = aYear;
  mExplodedTime.tm_month = aMonth;
  mExplodedTime.tm_mday  = aDate;
  mExplodedTime.tm_hour  = aHour;
  mExplodedTime.tm_min   = aMinute;

  mTime = PR_ImplodeTime(&mExplodedTime);

  PR_ExplodeTime(mTime, PR_GMTParameters, &mExplodedTime);

  return ;
}

void Calendar::set(PRInt32 aYear, PRInt32 aMonth, PRInt32 aDate, PRInt32 aHour, PRInt32 aMinute, PRInt32 aSecond)
{
  mExplodedTime.tm_year  = aYear;
  mExplodedTime.tm_month = aMonth;
  mExplodedTime.tm_mday  = aDate;
  mExplodedTime.tm_hour  = aHour;
  mExplodedTime.tm_min   = aMinute;
  mExplodedTime.tm_sec   = aSecond;

  mTime = PR_ImplodeTime(&mExplodedTime);

  PR_ExplodeTime(mTime, PR_GMTParameters, &mExplodedTime);
  return ;
}

void Calendar::clear()
{
  LL_I2L(mTime, 0);
  PR_ExplodeTime(mTime, PR_GMTParameters, &mExplodedTime);
  return ;
}

void Calendar::clear(EDateFields aField)
{
  set(aField,0);
  return ;
}

