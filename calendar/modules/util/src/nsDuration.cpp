/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
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
#include "nsCalUtilCIID.h"
#include "nsString.h"
#include "nsDuration.h"

static NS_DEFINE_IID(kISupportsIID, NS_ISUPPORTS_IID);
static NS_DEFINE_IID(kCDurationCID, NS_DURATION_CID);

nsDuration :: nsDuration()
{
  NS_INIT_REFCNT();
  Init();
}

nsDuration :: ~nsDuration()
{
  if (mDuration) {
    delete mDuration;
    mDuration = nsnull;
  }
}

NS_IMPL_QUERY_INTERFACE(nsDuration, kCDurationCID)
NS_IMPL_ADDREF(nsDuration)
NS_IMPL_RELEASE(nsDuration)


nsresult nsDuration :: Init()
{
  mDuration = new nsCalDuration();

  return NS_OK ;
}

PRUint32 nsDuration :: GetYear()
{
  return (mDuration->getYear());
}

PRUint32 nsDuration :: GetMonth()
{
  return (mDuration->getMonth());
}

PRUint32 nsDuration :: GetDay()
{
  PRUint32 day = mDuration->getDay();
  return (day);
}

PRUint32 nsDuration :: GetHour()
{
  return (mDuration->getHour());
}

PRUint32 nsDuration :: GetMinute()
{
  return (mDuration->getMinute());
}

PRUint32 nsDuration :: GetSecond()
{
  return (mDuration->getSecond());
}

nsresult nsDuration :: SetYear(PRUint32 aYear)
{
  t_bool bNegativeDuration = FALSE;

  if (aYear < 0)
    bNegativeDuration = TRUE;

  mDuration->set(aYear,
                 GetMonth(),
                 GetDay(),
                 GetHour(),
                 GetMinute(),
                 GetSecond(),
                 bNegativeDuration);

  return NS_OK;
}

nsresult nsDuration :: SetMonth(PRUint32 aMonth)
{
  t_bool bNegativeDuration = FALSE;

  if (aMonth < 0)
    bNegativeDuration = TRUE;

  mDuration->set(GetYear(),
                 aMonth,
                 GetDay(),
                 GetHour(),
                 GetMinute(),
                 GetSecond(),
                 bNegativeDuration);
  return NS_OK;
}

nsresult nsDuration :: SetDay(PRUint32 aDay)
{
  t_bool bNegativeDuration = FALSE;

  if (aDay < 0)
    bNegativeDuration = TRUE;

  mDuration->set(GetYear(),
                 GetMonth(),
                 aDay,
                 GetHour(),
                 GetMinute(),
                 GetSecond(),
                 bNegativeDuration);
  return NS_OK;
}

nsresult nsDuration :: SetHour(PRUint32 aHour)
{
  t_bool bNegativeDuration = FALSE;

  if (aHour < 0)
    bNegativeDuration = TRUE;

  mDuration->set(GetYear(),
                 GetMonth(),
                 GetDay(),
                 aHour,
                 GetMinute(),
                 GetSecond(),
                 bNegativeDuration);
  return NS_OK;
}

nsresult nsDuration :: SetMinute(PRUint32 aMinute)
{
  t_bool bNegativeDuration = FALSE;

  if (aMinute < 0)
    bNegativeDuration = TRUE;

  mDuration->set(GetYear(),
                 GetMonth(),
                 GetDay(),
                 GetHour(),
                 aMinute,
                 GetSecond(),
                 bNegativeDuration);
  return NS_OK;
}

nsresult nsDuration :: SetSecond(PRUint32 aSecond)
{
  t_bool bNegativeDuration = FALSE;

  if (aSecond < 0)
    bNegativeDuration = TRUE;

  mDuration->set(GetYear(),
                 GetMonth(),
                 GetDay(),
                 GetHour(),
                 GetMinute(),
                 aSecond,
                 bNegativeDuration);
  return NS_OK;
}

nsCalDuration& nsDuration :: GetDuration()
{
  return (*mDuration);
}
