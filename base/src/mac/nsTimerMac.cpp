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
 
//
// Mac implementation of the nsITimer interface
//



#include "nsITimer.h"
#include "nsITimerCallback.h"
#include "prlog.h"
#include "nsRepeater.h"

#include <list.h>
#include <Events.h>



#pragma mark class TimerImpl

//========================================================================================
class TimerImpl : public nsITimer 
// TimerImpl implements nsITimer API
//========================================================================================
{
    friend class TimerPeriodical;
    
  private:
    nsTimerCallbackFunc mCallbackFunc;
    nsITimerCallback * mCallbackObject;
    void * mClosure;
    PRUint32 mDelay;
    PRUint32 mFireTime;  // Timer should fire when TickCount >= this number
    TimerImpl * mPrev; 
    TimerImpl * mNext; 
  
  public:

  // constructors

    TimerImpl();

    virtual ~TimerImpl();
      
     NS_DECL_ISUPPORTS

    PRUint32 GetFireTime() const { return mFireTime; }

    void Fire();

  // nsITimer overrides

    virtual nsresult Init(nsTimerCallbackFunc aFunc,
                          void *aClosure,
                          PRUint32 aDelay);

    virtual nsresult Init(nsITimerCallback *aCallback,
                          PRUint32 aDelay);

    virtual void Cancel();

    virtual PRUint32 GetDelay();

    virtual void SetDelay(PRUint32 aDelay);

    virtual void* GetClosure();
  
#if DEBUG
	enum {
		eGoodTimerSignature = 'Barf',
		eDeletedTimerSignature = 'oops'
	};
	
  	Boolean			IsGoodTimer() const { return (mSignature == eGoodTimerSignature); }
#endif
  	
  private:
  // Calculates mFireTime too
    void SetDelaySelf( PRUint32 aDelay );
    
#if DEBUG
    UInt32		mSignature;
#endif

};

#pragma mark class TimerPeriodical

//========================================================================================
class TimerPeriodical  : public Repeater
// TimerPeriodical is a singleton Repeater subclass that fires
// off TimerImpl. The firing is done on idle.
//========================================================================================
{      
    static TimerPeriodical * gPeriodical;
    
    TimerImpl* mTimers;
  
  public:
    // Returns the singleton instance
    static TimerPeriodical * GetPeriodical();

    TimerPeriodical();

    virtual ~TimerPeriodical();
    
    virtual  void RepeatAction( const EventRecord &inMacEvent);
    
    nsresult AddTimer( TimerImpl * aTimer);
    
    nsresult RemoveTimer( TimerImpl * aTimer);

};


//========================================================================================
// TimerImpl implementation
//========================================================================================

NS_IMPL_ISUPPORTS(TimerImpl, nsITimer::GetIID())

//----------------------------------------------------------------------------------------
TimerImpl::TimerImpl()
//----------------------------------------------------------------------------------------
:  mCallbackFunc(nsnull)
,  mCallbackObject(nsnull)
,  mClosure(nsnull)
,  mDelay(0)
,  mFireTime(0)
,  mPrev(nsnull)
,  mNext(nsnull)
#if DEBUG
,  mSignature(eGoodTimerSignature)
#endif
{
  NS_INIT_REFCNT();
}

//----------------------------------------------------------------------------------------
TimerImpl::~TimerImpl()
//----------------------------------------------------------------------------------------
{
  Cancel();
  NS_IF_RELEASE(mCallbackObject);
#if DEBUG
  mSignature = eDeletedTimerSignature;
#endif
}

//----------------------------------------------------------------------------------------
nsresult TimerImpl::Init(nsTimerCallbackFunc aFunc,
                          void *aClosure,
                          PRUint32 aDelay)
//----------------------------------------------------------------------------------------
{
  mCallbackFunc = aFunc;
  mClosure = aClosure;
  SetDelaySelf(aDelay);
  return TimerPeriodical::GetPeriodical()->AddTimer(this);
}

//----------------------------------------------------------------------------------------
nsresult TimerImpl::Init(nsITimerCallback *aCallback,
                          PRUint32 aDelay)
//----------------------------------------------------------------------------------------
{
  NS_ADDREF(aCallback);
  mCallbackObject = aCallback;
  SetDelaySelf(aDelay);
  return TimerPeriodical::GetPeriodical()->AddTimer(this);
}

//----------------------------------------------------------------------------------------
void TimerImpl::Cancel()
//----------------------------------------------------------------------------------------
{
  TimerPeriodical::GetPeriodical()->RemoveTimer(this);
}

//----------------------------------------------------------------------------------------
PRUint32 TimerImpl::GetDelay()
//----------------------------------------------------------------------------------------
{
  return mDelay;
}

//----------------------------------------------------------------------------------------
void TimerImpl::SetDelay(PRUint32 aDelay)
//----------------------------------------------------------------------------------------
{
  SetDelaySelf(aDelay);
}

//----------------------------------------------------------------------------------------
void* TimerImpl::GetClosure()
//----------------------------------------------------------------------------------------
{
  return mClosure;
}

//----------------------------------------------------------------------------------------
void TimerImpl::Fire()
//----------------------------------------------------------------------------------------
{
  NS_PRECONDITION(mRefCnt > 0, "Firing a disposed Timer!");
  if (mCallbackFunc != NULL) {
    (*mCallbackFunc)(this, mClosure);
  }
  else if (mCallbackObject != NULL) {
    mCallbackObject->Notify(this); // Fire the timer
  }
}

//----------------------------------------------------------------------------------------
void TimerImpl::SetDelaySelf( PRUint32 aDelay )
//----------------------------------------------------------------------------------------
{

  mDelay = aDelay;
  mFireTime = TickCount() + (mDelay * 3) / 50;  // We need mFireTime in ticks (1/60th)
                          //  but aDelay is in 1000th (60/1000 = 3/50)
}

TimerPeriodical * TimerPeriodical::gPeriodical = nsnull;

TimerPeriodical * TimerPeriodical::GetPeriodical()
{
  if (gPeriodical == NULL)
    gPeriodical = new TimerPeriodical();
  return gPeriodical;
}

TimerPeriodical::TimerPeriodical()
{
  mTimers = nsnull;
}

TimerPeriodical::~TimerPeriodical()
{
  PR_ASSERT(mTimers == 0);
}

nsresult TimerPeriodical::AddTimer( TimerImpl * aTimer)
{
  // make sure it's not already there
  RemoveTimer(aTimer);
  // keep list sorted by fire time
  if (mTimers)
  {
    if (aTimer->GetFireTime() < mTimers->GetFireTime())
    {
      mTimers->mPrev = aTimer;
      aTimer->mNext = mTimers;
      mTimers = aTimer;
    }
    else
    {
      TimerImpl *t = mTimers;
      TimerImpl *prevt;
      // we know we will enter the while loop at least the first
      // time, and thus prevt will be initialized
      while (t && (t->GetFireTime() <= aTimer->GetFireTime()))
      {
        prevt = t;
        t = t->mNext;
      }
      aTimer->mPrev = prevt;
      aTimer->mNext = prevt->mNext;
      prevt->mNext = aTimer;
      if (aTimer->mNext) aTimer->mNext->mPrev = aTimer;
    }
  }
  else mTimers = aTimer;
  
  StartRepeating();
  return NS_OK;
}

nsresult TimerPeriodical::RemoveTimer( TimerImpl * aTimer)
{
  TimerImpl* t = mTimers;
  TimerImpl* next_t = nsnull;
  if (t) next_t = t->mNext;
  while (t)
  {
    if (t == aTimer)
    {
      if (mTimers == t) mTimers = t->mNext;
      if (t->mPrev) t->mPrev->mNext = t->mNext;
      if (t->mNext) t->mNext->mPrev = t->mPrev;
      t->mNext = nsnull;
      t->mPrev = nsnull;
    }
    t = next_t;
    if (t) next_t = t->mNext;
  }
 
  if ( mTimers == nsnull )
    StopRepeating();
  return NS_OK;
}

// Called through every event loop
// Loops through the list of available timers, and 
// fires off the appropriate ones
void  TimerPeriodical::RepeatAction( const EventRecord &inMacEvent)
{
  PRBool done = false;  
  while (!done)
  {
    TimerImpl* t = mTimers;
    while (t)
    {
      NS_ASSERTION(t->IsGoodTimer(), "Bad timer!");
    
      if (t->GetFireTime() <= inMacEvent.when)
      {
        RemoveTimer(t);
        t->Fire();
        break;
      }
      t = t->mNext;
    }
    done = true;
  }
}

                
NS_BASE nsresult NS_NewTimer(nsITimer** aInstancePtrResult)
{
    NS_PRECONDITION(nsnull != aInstancePtrResult, "null ptr");
    if (nsnull == aInstancePtrResult) {
      return NS_ERROR_NULL_POINTER;
    }  
    TimerImpl *timer = new TimerImpl();
    if (nsnull == timer) {
        return NS_ERROR_OUT_OF_MEMORY;
    }

    return timer->QueryInterface(nsITimer::GetIID(), (void **) aInstancePtrResult);
}
