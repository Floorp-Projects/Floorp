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

// nsMacTimerPeriodical idles, 


#include "nsITimer.h"
#include "nsITimerCallback.h"
#include "prlog.h"
#include <LPeriodical.h>
#include <LArray.h>
#include <LArrayIterator.h>
#include <LComparator.h>

#pragma mark class TimerImpl
//
// TimerImpl implements nsITimer API
// 
class TimerImpl : public nsITimer 
{
	private:
		nsTimerCallbackFunc mCallbackFunc;
		nsITimerCallback * mCallbackObject;
  	void * mClosure;
  	PRUint32 mDelay;
  	UInt32 mFireTime;	// Timer should fire when TickCount >= this number
	
	public:

	// constructors

	  TimerImpl();

	  virtual ~TimerImpl(){};

	 	NS_DECL_ISUPPORTS

		UInt32 GetFireTime() { return mFireTime; }

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
	
	private:
	// Calculates mFireTime too
		void SetDelaySelf( PRUint32 aDelay );
};

#pragma mark class TimerPeriodical
//
// TimerPeriodical is a singleton LPeriodical subclass that fires
// off TimerImpl. The firing is done on idle
// 
class TimerPeriodical	: public LPeriodical
{
		static TimerPeriodical * gPeriodical;
		
		LArray * mTimers;	// List of TimerImpl *
	
	public:
		// Returns the singleton instance
		static TimerPeriodical * GetPeriodical();

		TimerPeriodical();

		virtual ~TimerPeriodical();
		
		nsresult AddTimer( TimerImpl * aTimer);
		
		nsresult RemoveTimer( TimerImpl * aTimer);

		virtual	void	SpendTime( const EventRecord &inMacEvent);
};

#pragma mark class TimerImplComparator
// 
// TimerImplComparator compares two TimerImpl
//
class TimerImplComparator	: public LComparator
{
	virtual Int32		Compare(
								const void*			inItemOne,
								const void* 		inItemTwo,
								Uint32				inSizeOne,
								Uint32				inSizeTwo) const;
};


// 
// TimerImpl implementation
//

static NS_DEFINE_IID(kITimerIID, NS_ITIMER_IID);
NS_IMPL_ISUPPORTS(TimerImpl, kITimerIID)

TimerImpl::TimerImpl()
{
	NS_INIT_REFCNT();
	mCallbackFunc = NULL;
	mCallbackObject = NULL;
	mClosure = NULL;
	mDelay = 0;
	mFireTime = 0;
}

nsresult TimerImpl::Init(nsTimerCallbackFunc aFunc,
	                        void *aClosure,
	                        PRUint32 aDelay)
{
	mCallbackFunc = aFunc;
	mClosure = aClosure;
	SetDelaySelf(aDelay);
	return TimerPeriodical::GetPeriodical()->AddTimer(this);
}

nsresult TimerImpl::Init(nsITimerCallback *aCallback,
	                        PRUint32 aDelay)
{
	mCallbackObject = aCallback;
	SetDelaySelf(aDelay);
	return TimerPeriodical::GetPeriodical()->AddTimer(this);
}

void TimerImpl::Cancel()
{
	TimerPeriodical::GetPeriodical()->RemoveTimer(this);
}

PRUint32 TimerImpl::GetDelay()
{
	return mDelay;
}

void TimerImpl::SetDelay(PRUint32 aDelay)
{
	SetDelaySelf(aDelay);
	// Make sure that timer was sorted
	NS_ADDREF(this);
	TimerPeriodical::GetPeriodical()->RemoveTimer(this);
	TimerPeriodical::GetPeriodical()->AddTimer(this);
	NS_RELEASE(this);
}

void TimerImpl::Fire()
{
	if (mCallbackFunc != NULL) {
		(*mCallbackFunc)(this, mClosure);
  }
  else if (mCallbackObject != NULL) {
    mCallbackObject->Notify(this); // Fire the timer
  }
}

void TimerImpl::SetDelaySelf( PRUint32 aDelay )
{
	mDelay = aDelay;
	mFireTime = TickCount() + mDelay / 100 * 6;	// We need mFireTime in ticks, 1/60th of a second
}

TimerPeriodical * TimerPeriodical::gPeriodical = NULL;

TimerPeriodical * TimerPeriodical::GetPeriodical()
{
	if (gPeriodical == NULL)
		gPeriodical = new TimerPeriodical();
	return gPeriodical;
}

TimerPeriodical::TimerPeriodical()
{
	mTimers = new LArray( sizeof(TimerImpl*));
	mTimers->SetComparator( new TimerImplComparator());
	mTimers->SetKeepSorted( true );
}

TimerPeriodical::~TimerPeriodical()
{
	PR_ASSERT(mTimers->GetCount() == 0);
	delete mTimers;
}

nsresult TimerPeriodical::AddTimer( TimerImpl * aTimer)
{
	try
	{
		NS_ADDREF(aTimer);
		mTimers->AddItem( aTimer );
		StartRepeating();
	}
	catch(...)
	{
		return NS_ERROR_UNEXPECTED;
	}
	return NS_OK;
}

nsresult TimerPeriodical::RemoveTimer( TimerImpl * aTimer)
{
	mTimers->Remove( aTimer );
	NS_RELEASE( aTimer );
	if ( mTimers->GetCount() == 0 )
		StopRepeating();
	return NS_OK;
}

// Called through every event loop
// Loops through the list of available timers, and 
// fires off the available ones
void	TimerPeriodical::SpendTime( const EventRecord &inMacEvent)
{
	LArrayIterator iter( *mTimers, LArrayIterator::from_Start);
	TimerImpl * timer;
	while (iter.Next( &timer))
	{
		if ( timer->GetFireTime() <= inMacEvent.when )
		{
			NS_ADDREF(timer);
			RemoveTimer(timer);
			timer->Fire();
			NS_RELEASE(timer);
		}
		else
			break;	// Items are sorted, so we do not need to iterate until the end
	}
}

//
//  class TimerImplComparator implementation
//
Int32 TimerImplComparator::Compare(
								const void*			inItemOne,
								const void* 		inItemTwo,
								Uint32				inSizeOne,
								Uint32				inSizeTwo) const
{
	return (( TimerImpl *) inItemOne)->GetFireTime() - (( TimerImpl *) inItemTwo)->GetFireTime();
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
