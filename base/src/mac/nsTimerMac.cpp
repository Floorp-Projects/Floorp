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
  private:
    nsTimerCallbackFunc mCallbackFunc;
    nsITimerCallback * mCallbackObject;
    void * mClosure;
    PRUint32 mDelay;
    PRUint32 mFireTime;  // Timer should fire when TickCount >= this number
  
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
    
        list<TimerImpl*> mTimers;
  
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

static NS_DEFINE_IID(kITimerIID, NS_ITIMER_IID);
NS_IMPL_ISUPPORTS(TimerImpl, kITimerIID)

//----------------------------------------------------------------------------------------
TimerImpl::TimerImpl()
//----------------------------------------------------------------------------------------
:  mCallbackFunc(nsnull)
,  mCallbackObject(nsnull)
,  mClosure(nsnull)
,  mDelay(0)
,  mFireTime(0)
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
  if (mCallbackFunc != NULL)
  {
    (*mCallbackFunc)(this, mClosure);
  }
  else if (mCallbackObject != NULL)
  {
    nsITimerCallback* object = mCallbackObject;
    mCallbackObject = nsnull;
      // because the Notify call will release it.
      // We will release again it in the destructor if
      // it is not null when we go away!
    object->Notify(this); // Fire the timer
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
}

TimerPeriodical::~TimerPeriodical()
{
  PR_ASSERT(mTimers.size() == 0);
}

nsresult TimerPeriodical::AddTimer( TimerImpl * aTimer)
{
  // make sure it's not already there
  mTimers.remove(aTimer);
  mTimers.push_back(aTimer);
  StartRepeating();
  return NS_OK;
}

nsresult TimerPeriodical::RemoveTimer( TimerImpl * aTimer)
{
  mTimers.remove(aTimer);
  if ( mTimers.size() == 0 )
    StopRepeating();
  return NS_OK;
}

// Called through every event loop
// Loops through the list of available timers, and 
// fires off the appropriate ones
void  TimerPeriodical::RepeatAction( const EventRecord &inMacEvent)
{
  list<TimerImpl*>::iterator iter = mTimers.begin();
  list<TimerImpl*> fireList;
  
  while (iter != mTimers.end())
  {
    TimerImpl* timer = *iter;
    
    NS_ASSERTION(timer->IsGoodTimer(), "Bad timer!");
    
    if (timer->GetFireTime() <= inMacEvent.when)
    {
      mTimers.erase(iter++);
      NS_ADDREF(timer);
      fireList.push_back(timer);
    }
    else
      {
        iter++;
      }
  }
  if ( mTimers.size() == 0 )
     StopRepeating();
     
  for (iter=fireList.begin(); iter!=fireList.end(); iter++)
  {
    (*iter)->Fire();
    NS_RELEASE(*iter);
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

    return timer->QueryInterface(kITimerIID, (void **) aInstancePtrResult);
}
