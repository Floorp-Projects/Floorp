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

#ifndef nsICalTimeContext_h___
#define nsICalTimeContext_h___

#include "nsISupports.h"
#include "nsIDateTime.h"

//91649a00-e9e6-11d1-9244-00805f8a7ab6
#define NS_ICAL_TIME_CONTEXT_IID   \
{ 0x91649a00, 0xe9e6, 0x11d1,    \
{ 0x92, 0x44, 0x00, 0x80, 0x5f, 0x8a, 0x7a, 0xb6 } }

#include "nsCalPeriodFormat.h"


class nsICalTimeContext : public nsISupports
{

public:

  NS_IMETHOD Init() = 0;

  NS_IMETHOD SetDefaultDateTime() = 0;

  NS_IMETHOD SetStartTime(PRUint32 aYear, 
                          PRUint32 aMonth,
                          PRUint32 aDay,
                          PRUint32 aHour,
                          PRUint32 aMinute,
                          PRUint32 aSecond
                          ) = 0;

  NS_IMETHOD SetEndTime(PRUint32 aYear, 
                        PRUint32 aMonth,
                        PRUint32 aDay,
                        PRUint32 aHour,
                        PRUint32 aMinute,
                        PRUint32 aSecond
                        ) = 0;

  NS_IMETHOD SetFirstVisibleTime(PRUint32 aYear, 
                                 PRUint32 aMonth,
                                 PRUint32 aDay,
                                 PRUint32 aHour,
                                 PRUint32 aMinute,
                                 PRUint32 aSecond
                                 ) = 0;

  NS_IMETHOD SetLastVisibleTime(PRUint32 aYear, 
                                PRUint32 aMonth,
                                PRUint32 aDay,
                                PRUint32 aHour,
                                PRUint32 aMinute,
                                PRUint32 aSecond
                                ) = 0;

  NS_IMETHOD SetMajorIncrement(PRUint32 aYear, 
                               PRUint32 aMonth,
                               PRUint32 aDay,
                               PRUint32 aHour,
                               PRUint32 aMinute,
                               PRUint32 aSecond
                               ) = 0;

  NS_IMETHOD SetMinorIncrement(PRUint32 aYear, 
                               PRUint32 aMonth,
                               PRUint32 aDay,
                               PRUint32 aHour,
                               PRUint32 aMinute,
                               PRUint32 aSecond
                               ) = 0;

  NS_IMETHOD_(nsIDateTime *) GetDTStart() = 0;
  NS_IMETHOD_(nsIDateTime *) GetDTEnd() = 0;
  NS_IMETHOD_(nsIDateTime *) GetDTFirstVisible() = 0;
  NS_IMETHOD_(nsIDateTime *) GetDTLastVisible() = 0;
  NS_IMETHOD_(nsIDateTime *) GetDTMajorIncrement() = 0;
  NS_IMETHOD_(nsIDateTime *) GetDTMinorIncrement() = 0;

  NS_IMETHOD  SetPeriodFormat(nsCalPeriodFormat aPeriodFormat) = 0;
  NS_IMETHOD_(nsCalPeriodFormat)  GetPeriodFormat() = 0;

  NS_IMETHOD_(PRUint32) GetVisibleTimeDifference() = 0;
  NS_IMETHOD_(PRUint32) GetFirstVisibleTime() = 0;
  NS_IMETHOD_(PRUint32) GetLastVisibleTime() = 0;
  NS_IMETHOD_(PRUint32) GetVisibleTimeDifference(nsCalPeriodFormat aFormat) = 0;
  NS_IMETHOD_(PRUint32) GetFirstVisibleTime(nsCalPeriodFormat aFormat) = 0;
  NS_IMETHOD_(PRUint32) GetLastVisibleTime(nsCalPeriodFormat aFormat) = 0;

  NS_IMETHOD_(PRUint32) GetMinorIncrementInterval() = 0;
  NS_IMETHOD_(PRUint32) GetMajorIncrementInterval() = 0;

  NS_IMETHOD_(nsIDateTime *) GetDate() = 0;
  NS_IMETHOD SetDate(nsIDateTime * aDateTime) = 0;

  NS_IMETHOD AddPeriod(nsCalPeriodFormat aFormat, PRUint32 aPeriod) = 0;

  NS_IMETHOD Copy(nsICalTimeContext * aContext) = 0;
};

#endif /* nsICalTimeContext_h___ */
