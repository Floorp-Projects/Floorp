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

#ifndef nsCalTimeContext_h___
#define nsCalTimeContext_h___

#include "nsICalTimeContext.h"
#include "nsXPFCCanvas.h"
#include "nsIDateTime.h"
#include "nsIXPFCObserver.h"
#include "nsIXPFCCommandReceiver.h"
#include "nsIXPFCSubject.h"
#include "nsIXPFCCommand.h"
#include "nsCalDurationCommand.h"
#include "nsCalDayListCommand.h"
#include "nsIXMLParserObject.h"


class nsCalTimeContext : public nsICalTimeContext, 
                         public nsIXPFCObserver,
                         public nsIXPFCSubject,
                         public nsIXPFCCommandReceiver,
                         public nsIXMLParserObject
{
public:
  nsCalTimeContext();

  NS_DECL_ISUPPORTS

  NS_IMETHOD Init() ;

  NS_IMETHOD SetDefaultDateTime();

  NS_IMETHOD SetStartTime(PRUint32 aYear, 
                          PRUint32 aMonth,
                          PRUint32 aDay,
                          PRUint32 aHour,
                          PRUint32 aMinute,
                          PRUint32 aSecond
                          );

  NS_IMETHOD SetEndTime(PRUint32 aYear, 
                        PRUint32 aMonth,
                        PRUint32 aDay,
                        PRUint32 aHour,
                        PRUint32 aMinute,
                        PRUint32 aSecond
                        );

  NS_IMETHOD SetFirstVisibleTime(PRUint32 aYear, 
                                 PRUint32 aMonth,
                                 PRUint32 aDay,
                                 PRUint32 aHour,
                                 PRUint32 aMinute,
                                 PRUint32 aSecond
                                 );

  NS_IMETHOD SetLastVisibleTime(PRUint32 aYear, 
                                PRUint32 aMonth,
                                PRUint32 aDay,
                                PRUint32 aHour,
                                PRUint32 aMinute,
                                PRUint32 aSecond
                                );

  NS_IMETHOD SetMajorIncrement(PRUint32 aYear, 
                               PRUint32 aMonth,
                               PRUint32 aDay,
                               PRUint32 aHour,
                               PRUint32 aMinute,
                               PRUint32 aSecond
                               );

  NS_IMETHOD SetMinorIncrement(PRUint32 aYear, 
                               PRUint32 aMonth,
                               PRUint32 aDay,
                               PRUint32 aHour,
                               PRUint32 aMinute,
                               PRUint32 aSecond
                               );

  NS_IMETHOD  SetPeriodFormat(nsCalPeriodFormat aPeriodFormat);
  NS_IMETHOD_(nsCalPeriodFormat)  GetPeriodFormat();
  NS_IMETHOD_(PRUint32) GetVisibleTimeDifference() ;
  NS_IMETHOD_(PRUint32) GetFirstVisibleTime() ;
  NS_IMETHOD_(PRUint32) GetLastVisibleTime() ;
  NS_IMETHOD_(PRUint32) GetVisibleTimeDifference(nsCalPeriodFormat aFormat) ;
  NS_IMETHOD_(PRUint32) GetFirstVisibleTime(nsCalPeriodFormat aFormat) ;
  NS_IMETHOD_(PRUint32) GetLastVisibleTime(nsCalPeriodFormat aFormat) ;

  NS_IMETHOD_(nsIDateTime *) GetDTStart() ;
  NS_IMETHOD_(nsIDateTime *) GetDTEnd() ;
  NS_IMETHOD_(nsIDateTime *) GetDTFirstVisible();
  NS_IMETHOD_(nsIDateTime *) GetDTLastVisible();
  NS_IMETHOD_(nsIDateTime *) GetDTMajorIncrement();
  NS_IMETHOD_(nsIDateTime *) GetDTMinorIncrement();


  NS_IMETHOD_(PRUint32) GetMinorIncrementInterval() ;
  NS_IMETHOD_(PRUint32) GetMajorIncrementInterval() ;

  NS_IMETHOD_(nsIDateTime *) GetDate() ;
  NS_IMETHOD SetDate(nsIDateTime * aDateTime) ;

  // nsIXPFCObserver methods
  NS_IMETHOD_(nsEventStatus) Update(nsIXPFCSubject * aSubject, nsIXPFCCommand * aCommand);

  // nsIXPFCCommandReceiver methods
  NS_IMETHOD_(nsEventStatus) Action(nsIXPFCCommand * aCommand);

  // nsIXPFCSubject methods
  NS_IMETHOD Attach(nsIXPFCObserver * aObserver);
  NS_IMETHOD Detach(nsIXPFCObserver * aObserver);
  NS_IMETHOD Notify(nsIXPFCCommand * aCommand);

  NS_IMETHOD AddPeriod(nsCalPeriodFormat aFormat, PRUint32 aPeriod) ;

  NS_IMETHOD Copy(nsICalTimeContext * aContext);

  // nsIXMLParserObject methods
  NS_IMETHOD SetParameter(nsString& aKey, nsString& aValue) ;

protected:
  ~nsCalTimeContext();

private:
  NS_IMETHOD_(nsEventStatus)  HandleDurationCommand(nsCalDurationCommand * aDurationCommand);
  NS_IMETHOD_(nsEventStatus)  HandleDayListCommand(nsCalDayListCommand * aDayListCommand);

private:
  nsIDateTime * mStartTime;
  nsIDateTime * mEndTime;
  nsIDateTime * mFirstVisibleTime;
  nsIDateTime * mLastVisibleTime;
  nsIDateTime * mMajorIncrement;
  nsIDateTime * mMinorIncrement;
  nsCalPeriodFormat mPeriodFormat;
  nsIDateTime * mDate;

};

#endif /* nsCalTimeContext_h___ */
