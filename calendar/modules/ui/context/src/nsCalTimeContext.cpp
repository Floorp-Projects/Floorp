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

#include "nsCalTimeContext.h"
#include "nsCalUICIID.h"
#include "nsCalUtilCIID.h"
#include "nsIXPFCCommand.h"
#include "nsCalDurationCommand.h"
#include "nsCalFetchEventsCommand.h"
#include "nsCalToolkit.h"
#include "nsDateTime.h"
#include "math.h"
#include "nscalstrings.h"
#include "nsIXMLParserObject.h"
#include "nsxpfcCIID.h"
#include "nsIXPFCObserverManager.h"
#include "nsIServiceManager.h"


static NS_DEFINE_IID(kXPFCSubjectIID, NS_IXPFC_SUBJECT_IID);
static NS_DEFINE_IID(kXPFCCommandIID, NS_IXPFC_COMMAND_IID);
static NS_DEFINE_IID(kXPFCCommandCID, NS_XPFC_COMMAND_CID);

static NS_DEFINE_IID(kISupportsIID,  NS_ISUPPORTS_IID);
static NS_DEFINE_IID(kCalTimeContextIID, NS_ICAL_TIME_CONTEXT_IID);

static NS_DEFINE_IID(kCalDateTimeCID, NS_DATETIME_CID);
static NS_DEFINE_IID(kCalDateTimeIID, NS_IDATETIME_IID);

static NS_DEFINE_IID(kCXPFCObserverManagerCID, NS_XPFC_OBSERVERMANAGER_CID);
static NS_DEFINE_IID(kIXPFCObserverManagerIID, NS_IXPFC_OBSERVERMANAGER_IID);

// XXX: TODO: Simplify this code!
// XXX: TODO: Much of this code should be in a nsCalTimebarContext
// XXX: TODO: DO NOT USE DateTime objects for all the private data
//            elements.  The overhead is not needed

nsCalTimeContext :: nsCalTimeContext()
{
  NS_INIT_REFCNT();

  mStartTime = nsnull;
  mEndTime = nsnull;
  mFirstVisibleTime = nsnull;
  mLastVisibleTime = nsnull;
  mMajorIncrement = nsnull;
  mMinorIncrement = nsnull;
  mPeriodFormat = nsCalPeriodFormat_kHour;
  mDate = nsnull;

}

nsCalTimeContext :: ~nsCalTimeContext()  
{

  nsIXPFCObserverManager* om;
  nsServiceManager::GetService(kCXPFCObserverManagerCID, kIXPFCObserverManagerIID, (nsISupports**)&om);

  om->Unregister((nsISupports*)(nsICalTimeContext*)this);

  nsServiceManager::ReleaseService(kCXPFCObserverManagerCID, om);

  NS_IF_RELEASE(mStartTime);
  NS_IF_RELEASE(mEndTime);
  NS_IF_RELEASE(mFirstVisibleTime);
  NS_IF_RELEASE(mLastVisibleTime);
  NS_IF_RELEASE(mMajorIncrement);
  NS_IF_RELEASE(mMinorIncrement);
  NS_IF_RELEASE(mDate);

}

NS_IMPL_ADDREF(nsCalTimeContext)
NS_IMPL_RELEASE(nsCalTimeContext)

nsresult nsCalTimeContext::QueryInterface(REFNSIID aIID, void** aInstancePtr)      
{                                                                        

  if (NULL == aInstancePtr) {                                            
    return NS_ERROR_NULL_POINTER;                                        
  }                                                                      
  static NS_DEFINE_IID(kISupportsIID, NS_ISUPPORTS_IID);                 
  static NS_DEFINE_IID(kClassIID, kCalTimeContextIID);                         
  static NS_DEFINE_IID(kXPFCObserverIID, NS_IXPFC_OBSERVER_IID);
  static NS_DEFINE_IID(kXPFCCommandReceiverIID, NS_IXPFC_COMMANDRECEIVER_IID);

  if (aIID.Equals(kClassIID)) {                                          
    *aInstancePtr = (void*) this;                                        
    AddRef();                                                            
    return NS_OK;                                                        
  }                                                                      
  if (aIID.Equals(kXPFCObserverIID)) {                                          
    *aInstancePtr = (void*)(nsIXPFCObserver *) this;   
    AddRef();                                                            
    return NS_OK;                                                        
  }                                                                      
  if (aIID.Equals(kXPFCSubjectIID)) {                                          
    *aInstancePtr = (void*) (nsIXPFCSubject *)this;                                        
    AddRef();                                                            
    return NS_OK;                                                        
  }                                                                      
  if (aIID.Equals(kXPFCCommandReceiverIID)) {                                          
    *aInstancePtr = (void*)(nsIXPFCCommandReceiver *) this;   
    AddRef();                                                            
    return NS_OK;                                                        
  }                                                                      
  if (aIID.Equals(kISupportsIID)) {                                      
    *aInstancePtr = (void*) (this);                        
    AddRef();                                                            
    return NS_OK;                                                        
  }                                                                      

  return (NS_NOINTERFACE);

}

nsresult nsCalTimeContext::Copy(nsICalTimeContext * aContext)
{
  mStartTime          = aContext->GetDTStart()->Copy();
  mEndTime            = aContext->GetDTEnd()->Copy();
  mFirstVisibleTime   = aContext->GetDTFirstVisible()->Copy();
  mLastVisibleTime    = aContext->GetDTLastVisible()->Copy();
  mMajorIncrement     = aContext->GetDTMajorIncrement()->Copy();
  mMinorIncrement     = aContext->GetDTMinorIncrement()->Copy();
  mDate               = aContext->GetDate()->Copy();
  return NS_OK;
}

nsresult nsCalTimeContext::Init()
{
  /*
   * Set the default to be today from 9am to 5pm
   */

  SetDefaultDateTime();

  return NS_OK;
}

nsIDateTime * nsCalTimeContext::GetDate()
{
  return (mDate);
}

nsresult nsCalTimeContext::SetDate(nsIDateTime * aDateTime)
{
  mDate = aDateTime;
  return (NS_OK);
}

nsIDateTime * nsCalTimeContext::GetDTStart()
{
  return (mStartTime);
}

nsIDateTime * nsCalTimeContext::GetDTEnd()
{
  return (mEndTime);
}

nsIDateTime * nsCalTimeContext::GetDTFirstVisible()
{
  return (mFirstVisibleTime);
}

nsIDateTime * nsCalTimeContext::GetDTLastVisible()
{
  return (mLastVisibleTime);
}

nsIDateTime * nsCalTimeContext::GetDTMajorIncrement()
{
  return (mMajorIncrement);
}

nsIDateTime * nsCalTimeContext::GetDTMinorIncrement()
{
  return (mMinorIncrement);
}


nsresult nsCalTimeContext::SetDefaultDateTime()
{

  nsresult res;

  PRUint32 y,d,mo,h,mi,s;

  NS_IF_RELEASE(mDate);
 
  res = nsRepository::CreateInstance(kCalDateTimeCID, 
                                     nsnull, 
                                     kCalDateTimeCID, 
                                     (void **)&mDate);

  if (NS_OK != res)
    return res;

  y  = mDate->GetYear();
  mo = mDate->GetMonth();
  d  = mDate->GetDay();
  h  = mDate->GetHour();
  mi = mDate->GetMinute();
  s  = mDate->GetSecond();
    
  SetStartTime(y,mo,d,0,0,0);
  SetEndTime(y,mo,d+1,0,0,0);
  SetFirstVisibleTime(y,mo,d,9,0,0);
  SetLastVisibleTime(y,mo,d,17,0,0);

  SetMajorIncrement(0,0,0,1,0,0);
  SetMinorIncrement(0,0,0,0,15,0);

  mPeriodFormat = nsCalPeriodFormat_kHour;

  return res;
}

nsresult nsCalTimeContext::SetStartTime(PRUint32 aYear, 
                                    PRUint32 aMonth,
                                    PRUint32 aDay,
                                    PRUint32 aHour,
                                    PRUint32 aMinute,
                                    PRUint32 aSecond
                                    )
{

  if (nsnull == mStartTime)
  {
    nsresult res;

    NS_IF_RELEASE(mStartTime);

    res = nsRepository::CreateInstance(kCalDateTimeCID, 
                                       nsnull, 
                                       kCalDateTimeCID, 
                                       (void **)&mStartTime);

    if (NS_OK != res)
      return res;
  }

  mStartTime->SetYear(aYear);
  mStartTime->SetMonth(aMonth);
  mStartTime->SetDay(aDay);
  mStartTime->SetHour(aHour);
  mStartTime->SetMinute(aMinute);
  mStartTime->SetSecond(aSecond);
  return NS_OK;
}

nsresult nsCalTimeContext::SetEndTime(PRUint32 aYear, 
                                      PRUint32 aMonth,
                                      PRUint32 aDay,
                                      PRUint32 aHour,
                                      PRUint32 aMinute,
                                      PRUint32 aSecond
                                      )
{
  if (nsnull == mEndTime)
  {
    nsresult res;

    NS_IF_RELEASE(mEndTime);

    res = nsRepository::CreateInstance(kCalDateTimeCID, 
                                       nsnull, 
                                       kCalDateTimeCID, 
                                       (void **)&mEndTime);

    if (NS_OK != res)
      return res;
  }

  mEndTime->SetYear(aYear);
  mEndTime->SetMonth(aMonth);
  mEndTime->SetDay(aDay);
  mEndTime->SetHour(aHour);
  mEndTime->SetMinute(aMinute);
  mEndTime->SetSecond(aSecond);
  return NS_OK;
}

nsresult nsCalTimeContext::SetFirstVisibleTime(PRUint32 aYear, 
                                           PRUint32 aMonth,
                                           PRUint32 aDay,
                                           PRUint32 aHour,
                                           PRUint32 aMinute,
                                           PRUint32 aSecond
                                           )
{
  if (nsnull == mFirstVisibleTime)
  {
    nsresult res;

    NS_IF_RELEASE(mFirstVisibleTime);

    res = nsRepository::CreateInstance(kCalDateTimeCID, 
                                       nsnull, 
                                       kCalDateTimeCID, 
                                       (void **)&mFirstVisibleTime);

    if (NS_OK != res)
      return res;
  }

  mFirstVisibleTime->SetYear(aYear);
  mFirstVisibleTime->SetMonth(aMonth);
  mFirstVisibleTime->SetDay(aDay);
  mFirstVisibleTime->SetHour(aHour);
  mFirstVisibleTime->SetMinute(aMinute);
  mFirstVisibleTime->SetSecond(aSecond);
  return NS_OK;
}

nsresult nsCalTimeContext::SetLastVisibleTime(PRUint32 aYear, 
                                          PRUint32 aMonth,
                                          PRUint32 aDay,
                                          PRUint32 aHour,
                                          PRUint32 aMinute,
                                          PRUint32 aSecond
                                          )
{
  if (nsnull == mLastVisibleTime)
  {
    nsresult res;

    NS_IF_RELEASE(mLastVisibleTime);

    res = nsRepository::CreateInstance(kCalDateTimeCID, 
                                       nsnull, 
                                       kCalDateTimeCID, 
                                       (void **)&mLastVisibleTime);

    if (NS_OK != res)
      return res;
  }

  mLastVisibleTime->SetYear(aYear);
  mLastVisibleTime->SetMonth(aMonth);
  mLastVisibleTime->SetDay(aDay);
  mLastVisibleTime->SetHour(aHour);
  mLastVisibleTime->SetMinute(aMinute);
  mLastVisibleTime->SetSecond(aSecond);
  return NS_OK;
}

nsresult nsCalTimeContext::SetMajorIncrement(PRUint32 aYear, 
                                         PRUint32 aMonth,
                                         PRUint32 aDay,
                                         PRUint32 aHour,
                                         PRUint32 aMinute,
                                         PRUint32 aSecond
                                         )
{
  if (nsnull == mMajorIncrement)
  {
    nsresult res;

    NS_IF_RELEASE(mMajorIncrement);

    res = nsRepository::CreateInstance(kCalDateTimeCID, 
                                       nsnull, 
                                       kCalDateTimeCID, 
                                       (void **)&mMajorIncrement);

    if (NS_OK != res)
      return res;
  }
  mMajorIncrement->SetYear(aYear);
  mMajorIncrement->SetMonth(aMonth);
  mMajorIncrement->SetDay(aDay);
  mMajorIncrement->SetHour(aHour);
  mMajorIncrement->SetMinute(aMinute);
  mMajorIncrement->SetSecond(aSecond);
  return NS_OK;
}

nsresult nsCalTimeContext::SetMinorIncrement(PRUint32 aYear, 
                                         PRUint32 aMonth,
                                         PRUint32 aDay,
                                         PRUint32 aHour,
                                         PRUint32 aMinute,
                                         PRUint32 aSecond
                                         )
{
  if (nsnull == mMinorIncrement)
  {
    nsresult res;

    NS_IF_RELEASE(mMinorIncrement);

    res = nsRepository::CreateInstance(kCalDateTimeCID, 
                                       nsnull, 
                                       kCalDateTimeCID, 
                                       (void **)&mMinorIncrement);

    if (NS_OK != res)
      return res;
  }
  mMinorIncrement->SetYear(aYear);
  mMinorIncrement->SetMonth(aMonth);
  mMinorIncrement->SetDay(aDay);
  mMinorIncrement->SetHour(aHour);
  mMinorIncrement->SetMinute(aMinute);
  mMinorIncrement->SetSecond(aSecond);
  return NS_OK;
}

nsresult nsCalTimeContext::SetPeriodFormat(nsCalPeriodFormat aPeriodFormat)
{
  mPeriodFormat = aPeriodFormat;
  return NS_OK;
}

nsCalPeriodFormat nsCalTimeContext::GetPeriodFormat()
{
  return (mPeriodFormat);
}

/*
 * compute the visible time difference in units of mPeriodFormat
 */

PRUint32 nsCalTimeContext::GetVisibleTimeDifference()
{
  return(GetVisibleTimeDifference(mPeriodFormat));
}

/*
 * compute the visible time difference in units of mPeriodFormat
 */

PRUint32 nsCalTimeContext::GetVisibleTimeDifference(nsCalPeriodFormat aFormat)
{
  PRUint32 difference = 0 ;

  switch (aFormat)
  {
    case nsCalPeriodFormat_kYear:
      difference = abs((int)(mLastVisibleTime->GetYear() - mFirstVisibleTime->GetYear()));
      break;

    case nsCalPeriodFormat_kMonth:
      difference = abs((int)(mLastVisibleTime->GetMonth() - mFirstVisibleTime->GetMonth()));
      break;
    
    case nsCalPeriodFormat_kDay:
      difference = abs((int)(mLastVisibleTime->GetDay() - mFirstVisibleTime->GetDay()));
      break;
    
    case nsCalPeriodFormat_kHour:
      difference = abs((int)(mLastVisibleTime->GetHour() - mFirstVisibleTime->GetHour()));
      break;
    
    case nsCalPeriodFormat_kMinute:
      difference = abs((int)(mLastVisibleTime->GetMinute() - mFirstVisibleTime->GetMinute()));
      break;
    
    case nsCalPeriodFormat_kSecond:
      difference = abs((int)(mLastVisibleTime->GetSecond() - mFirstVisibleTime->GetSecond()));
      break;

  }

  return (difference);
}

PRUint32 nsCalTimeContext::GetMajorIncrementInterval()
{
  PRUint32 interval = 0 ;

  switch (mPeriodFormat)
  {
    case nsCalPeriodFormat_kYear:
      interval = mMajorIncrement->GetYear();
      break;

    case nsCalPeriodFormat_kMonth:
      interval = mMajorIncrement->GetMonth();
      break;
    
    case nsCalPeriodFormat_kDay:
      interval = mMajorIncrement->GetDay();
      break;
    
    case nsCalPeriodFormat_kHour:
      interval = mMajorIncrement->GetHour();
      break;
    
    case nsCalPeriodFormat_kMinute:
      interval = mMajorIncrement->GetMinute();
      break;
    
    case nsCalPeriodFormat_kSecond:
      interval = mMajorIncrement->GetSecond();
      break;

  }

  return (interval);
}

PRUint32 nsCalTimeContext::GetMinorIncrementInterval()
{
  PRUint32 interval = 0 ;

  switch (mPeriodFormat)
  {
    case nsCalPeriodFormat_kYear:
      interval = mMinorIncrement->GetYear();
      break;

    case nsCalPeriodFormat_kMonth:
      interval = mMinorIncrement->GetMonth();
      break;
    
    case nsCalPeriodFormat_kDay:
      interval = mMinorIncrement->GetDay();
      break;
    
    case nsCalPeriodFormat_kHour:
      interval = mMinorIncrement->GetHour();
      break;
    
    case nsCalPeriodFormat_kMinute:
      interval = mMinorIncrement->GetMinute();
      break;
    
    case nsCalPeriodFormat_kSecond:
      interval = mMinorIncrement->GetSecond();
      break;

  }

  return (interval);
}

PRUint32 nsCalTimeContext::GetFirstVisibleTime()
{
  return (GetFirstVisibleTime(mPeriodFormat));
}

PRUint32 nsCalTimeContext::GetFirstVisibleTime(nsCalPeriodFormat aFormat)
{
  PRUint32 interval = 0 ;

  switch (aFormat)
  {
    case nsCalPeriodFormat_kYear:
      interval = mFirstVisibleTime->GetYear();
      break;

    case nsCalPeriodFormat_kMonth:
      interval = mFirstVisibleTime->GetMonth();
      break;
    
    case nsCalPeriodFormat_kDay:
      interval = mFirstVisibleTime->GetDay();
      break;
    
    case nsCalPeriodFormat_kHour:
      interval = mFirstVisibleTime->GetHour();
      break;
    
    case nsCalPeriodFormat_kMinute:
      interval = mFirstVisibleTime->GetMinute();
      break;
    
    case nsCalPeriodFormat_kSecond:
      interval = mFirstVisibleTime->GetSecond();
      break;

  }

  return (interval);
}

PRUint32 nsCalTimeContext::GetLastVisibleTime()
{
  return (GetLastVisibleTime(mPeriodFormat));
}

PRUint32 nsCalTimeContext::GetLastVisibleTime(nsCalPeriodFormat aFormat)
{
  PRUint32 interval = 0 ;

  switch (aFormat)
  {
    case nsCalPeriodFormat_kYear:
      interval = mLastVisibleTime->GetYear();
      break;

    case nsCalPeriodFormat_kMonth:
      interval = mLastVisibleTime->GetMonth();
      break;
    
    case nsCalPeriodFormat_kDay:
      interval = mLastVisibleTime->GetDay();
      break;
    
    case nsCalPeriodFormat_kHour:
      interval = mLastVisibleTime->GetHour();
      break;
    
    case nsCalPeriodFormat_kMinute:
      interval = mLastVisibleTime->GetMinute();
      break;
    
    case nsCalPeriodFormat_kSecond:
      interval = mLastVisibleTime->GetSecond();
      break;

  }

  return (interval);
}

/*
 * We've just received a notification that someone somewhere wants our
 * context to be updated.  The source of the change is in aSubject
 * and the target operation is in aCommand.
 *
 * We use QueryInterface to decide the actual types of objects.
 *
 * Once we've updated our internal streucture, we need to send a
 * visual notification to all observers of us ... 
 */

nsEventStatus nsCalTimeContext::Update(nsIXPFCSubject * aSubject, nsIXPFCCommand * aCommand)
{

  nsEventStatus status ;
  /*
   * Update our internal structure based on this update
   */

  status = Action(aCommand);

  /*
   * Pass this notification on.  There may be other
   * TimeContext's watching us. It is assumed observer's
   * like Model's will ignore this command
   */

  Notify(aCommand);

  /*
   * If we consumed the event, send a FetchEvents Command to our observers
   * Only Model data want this.  If we get a FetchEvents command, we'll just
   * pass it on (above)
   */

  if (nsEventStatus_eIgnore != status)
  {
    // XXX: Change this to a Fetch for range.  I suppose we just
    //      Want to ask for our current visible range.  Let the 
    //      CacheManager decide what to actually request over the
    //      wire.

    nsCalFetchEventsCommand * fetch_command  = nsnull;
    nsresult res;

    static NS_DEFINE_IID(kCalFetchEventsCommandCID, NS_CAL_FETCHEVENTS_COMMAND_CID);                 

    res = nsRepository::CreateInstance(kCalFetchEventsCommandCID, 
                                       nsnull, 
                                       kXPFCCommandIID, 
                                       (void **)&fetch_command);

    if (NS_OK == res)
    {
      fetch_command->Init();

      // XXX: Are these setup correctly?
      fetch_command->mStartDate = mStartTime; 
      fetch_command->mEndDate = mEndTime;

      Notify(fetch_command);

      NS_IF_RELEASE(fetch_command);
    }
  }

  return (status);
}

nsEventStatus nsCalTimeContext::Action(nsIXPFCCommand * aCommand)
{
  nsCalDurationCommand * duration_command  = nsnull;
  nsCalDayListCommand * daylist_command  = nsnull;
  nsresult res;

  static NS_DEFINE_IID(kCalDurationCommandCID, NS_CAL_DURATION_COMMAND_CID);                 
  
  res = aCommand->QueryInterface(kCalDurationCommandCID,(void**)&duration_command);

  if (NS_OK == res)
    return (HandleDurationCommand(duration_command));


  /*
   * Check for DayList
   */


  static NS_DEFINE_IID(kCalDayListCommandCID, NS_CAL_DAYLIST_COMMAND_CID);                 
  
  res = aCommand->QueryInterface(kCalDayListCommandCID,(void**)&daylist_command);

  if (NS_OK == res)
    return (HandleDayListCommand(daylist_command));


  return nsEventStatus_eIgnore;
}

nsEventStatus nsCalTimeContext::HandleDayListCommand(nsCalDayListCommand * aDayListCommand)
{
  /*
   * Handle the DayList here...
   */

  // Change our date to the first in the list
  nsIIterator * iterator;
  nsDateTime * datetime;
  nsresult res;


  // Iterate through the children
  res = aDayListCommand->CreateIterator(&iterator);

  if (NS_OK != res)
    return nsEventStatus_eIgnore;

  iterator->Init();

  if(!(iterator->IsDone()))
  {
    datetime = (nsDateTime *) iterator->CurrentItem();

    SetDate(datetime);

  }

  NS_IF_RELEASE(iterator);
  NS_IF_RELEASE(aDayListCommand);

  return nsEventStatus_eConsumeDoDefault;
}

nsEventStatus nsCalTimeContext::HandleDurationCommand(nsCalDurationCommand * aDurationCommand)
{
  nsDuration * dtDuration;

  dtDuration = aDurationCommand->GetDuration();

  if (nsnull == dtDuration)
    return nsEventStatus_eIgnore;

  switch (aDurationCommand->GetPeriodFormat())
  {
    case nsCalPeriodFormat_kYear:
      mLastVisibleTime->SetYear(mLastVisibleTime->GetYear() + dtDuration->GetYear());
      mFirstVisibleTime->SetYear(mFirstVisibleTime->GetYear() + dtDuration->GetYear());
      break;

    case nsCalPeriodFormat_kMonth:
        mLastVisibleTime->IncrementYear(dtDuration->GetYear());
        mLastVisibleTime->IncrementMonth(dtDuration->GetMonth());
        mLastVisibleTime->IncrementDay(dtDuration->GetDay());
        mFirstVisibleTime->IncrementYear(dtDuration->GetYear());
        mFirstVisibleTime->IncrementMonth(dtDuration->GetMonth());
        mFirstVisibleTime->IncrementDay(dtDuration->GetDay());
        mStartTime->IncrementYear(dtDuration->GetYear());
        mStartTime->IncrementMonth(dtDuration->GetMonth());
        mStartTime->IncrementDay(dtDuration->GetDay());
        mEndTime->IncrementYear(dtDuration->GetYear());
        mEndTime->IncrementMonth(dtDuration->GetMonth());
        mEndTime->IncrementDay(dtDuration->GetDay());
      break;
    
    case nsCalPeriodFormat_kDay:
        mLastVisibleTime->IncrementDay(dtDuration->GetDay());
        mFirstVisibleTime->IncrementDay(dtDuration->GetDay());
        mStartTime->IncrementDay(dtDuration->GetDay());
        mEndTime->IncrementDay(dtDuration->GetDay());
      break;
    
    case nsCalPeriodFormat_kHour:
      if (mLastVisibleTime->GetHour() != 23) {
        mLastVisibleTime->SetHour(mLastVisibleTime->GetHour() + dtDuration->GetHour());
        mFirstVisibleTime->SetHour(mFirstVisibleTime->GetHour() + dtDuration->GetHour());
      }
      break;
    
    case nsCalPeriodFormat_kMinute:
      mLastVisibleTime->SetMinute(mLastVisibleTime->GetMinute() + dtDuration->GetMinute());
      mFirstVisibleTime->SetMinute(mFirstVisibleTime->GetMinute() + dtDuration->GetMinute());
      break;
    
    case nsCalPeriodFormat_kSecond:
      mLastVisibleTime->SetSecond(mLastVisibleTime->GetSecond() + dtDuration->GetSecond());
      mFirstVisibleTime->SetSecond(mFirstVisibleTime->GetSecond() + dtDuration->GetSecond());
      break;

  }

  NS_IF_RELEASE(aDurationCommand);

  return nsEventStatus_eConsumeDoDefault;
}

nsresult nsCalTimeContext :: Attach(nsIXPFCObserver * aObserver)
{
  return NS_OK;
}

nsresult nsCalTimeContext :: Detach(nsIXPFCObserver * aObserver)
{
  return NS_OK;
}

nsresult nsCalTimeContext :: Notify(nsIXPFCCommand * aCommand)
{
  nsIXPFCSubject * subject;

  nsresult res = QueryInterface(kXPFCSubjectIID,(void **)&subject);

  if (res != NS_OK)
    return res;

  nsIXPFCObserverManager* om;
  nsServiceManager::GetService(kCXPFCObserverManagerCID, kIXPFCObserverManagerIID, (nsISupports**)&om);

  res = om->Notify(subject,aCommand);

  nsServiceManager::ReleaseService(kCXPFCObserverManagerCID, om);

  return(res);
}


nsresult nsCalTimeContext::AddPeriod(nsCalPeriodFormat aFormat, PRUint32 aPeriod)
{
  switch (aFormat)
  {
    case nsCalPeriodFormat_kYear:
      mDate->IncrementYear(aPeriod);
      mLastVisibleTime->IncrementYear(aPeriod);
      mFirstVisibleTime->IncrementYear(aPeriod);
      mStartTime->IncrementYear(aPeriod);
      mEndTime->IncrementYear(aPeriod);
      break;

    case nsCalPeriodFormat_kMonth:
      mDate->IncrementMonth(aPeriod);
      mLastVisibleTime->IncrementMonth(aPeriod);
      mFirstVisibleTime->IncrementMonth(aPeriod);
      mStartTime->IncrementMonth(aPeriod);
      mEndTime->IncrementMonth(aPeriod);
      break;
    
    case nsCalPeriodFormat_kDay:
      mDate->IncrementDay(aPeriod);
      mLastVisibleTime->IncrementDay(aPeriod);
      mFirstVisibleTime->IncrementDay(aPeriod);
      mStartTime->IncrementDay(aPeriod);
      mEndTime->IncrementDay(aPeriod);
      break;
    
    case nsCalPeriodFormat_kHour:
      mDate->IncrementHour(aPeriod);
      mLastVisibleTime->IncrementHour(aPeriod);
      mFirstVisibleTime->IncrementHour(aPeriod);
      mStartTime->IncrementHour(aPeriod);
      mEndTime->IncrementHour(aPeriod);
      break;
    
    case nsCalPeriodFormat_kMinute:
      mDate->IncrementMinute(aPeriod);
      mLastVisibleTime->IncrementMinute(aPeriod);
      mFirstVisibleTime->IncrementMinute(aPeriod);
      mStartTime->IncrementMinute(aPeriod);
      mEndTime->IncrementMinute(aPeriod);
      break;
    
    case nsCalPeriodFormat_kSecond:
      mDate->IncrementSecond(aPeriod);
      mLastVisibleTime->IncrementSecond(aPeriod);
      mFirstVisibleTime->IncrementSecond(aPeriod);
      mStartTime->IncrementSecond(aPeriod);
      mEndTime->IncrementSecond(aPeriod);
      break;

  }

  return NS_OK;
}

nsresult nsCalTimeContext :: SetParameter(nsString& aKey, nsString& aValue)
{

  return NS_OK;
}
