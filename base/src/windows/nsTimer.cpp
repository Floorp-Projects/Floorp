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
#include "nsITimer.h"
#include "nsITimerCallback.h"
#include "nsCRT.h"
#include "prlog.h"
#include <stdio.h>
#include <windows.h>
#include <limits.h>

static NS_DEFINE_IID(kITimerIID, NS_ITIMER_IID);

/*
 * Implementation of timers lifted from Windows front-end file timer.cpp
 */
class TimerImpl : public nsITimer {
public:
  static TimerImpl *gTimerList;
  static UINT gWindowsTimer;
  static DWORD gNextFire;
  static DWORD gSyncHack;

  static void ProcessTimeouts(DWORD aNow);
  static void SyncTimeoutPeriod(DWORD aTickCount);

public:
  TimerImpl();
  ~TimerImpl();

  virtual nsresult Init(nsTimerCallbackFunc aFunc,
                void *aClosure,
//              PRBool aRepeat, 
                PRUint32 aDelay);

  virtual nsresult Init(nsITimerCallback *aCallback,
//              PRBool aRepeat, 
                PRUint32 aDelay);

  NS_DECL_ISUPPORTS

  virtual void Cancel();
  void Fire(DWORD aNow);

  virtual PRUint32 GetDelay() { return mDelay; }
  virtual void SetDelay(PRUint32 aDelay) {};

private:
  nsresult Init(PRUint32 aDelay);

  PRUint32 mDelay;
  nsTimerCallbackFunc mFunc;
  void *mClosure;
  nsITimerCallback *mCallback;
  DWORD mFireTime;
  // PRBool mRepeat;
  TimerImpl *mNext;
};

TimerImpl *TimerImpl::gTimerList = NULL;
UINT TimerImpl::gWindowsTimer = 0;
DWORD TimerImpl::gNextFire = (DWORD)-1;
DWORD TimerImpl::gSyncHack = 0;

void CALLBACK FireTimeout(HWND aWindow, 
                          UINT aMessage, 
                          UINT aTimerID, 
                          DWORD aTime)
{
  static BOOL bCanEnter = TRUE;

  //  Don't allow old timer messages in here.
  if(aMessage != WM_TIMER)    {
    PR_ASSERT(0);
    return;
  }

  if(aTimerID != TimerImpl::gWindowsTimer)   {
    return;
  }
  
  //  Block only one entry into this function, or else.
  if(bCanEnter)   {
    bCanEnter = FALSE;
    // see if we need to fork off any timeout functions
    if(TimerImpl::gTimerList)    {
      TimerImpl::ProcessTimeouts(aTime);
    }
    bCanEnter = TRUE;
  }
}

//  Function to correctly have the timer be set.
void 
TimerImpl::SyncTimeoutPeriod(DWORD aTickCount)
{
    //  May want us to set tick count ourselves.
    if(aTickCount == 0)    {
        if(gSyncHack == 0) {
            aTickCount = ::GetTickCount();
        }
        else    {
            aTickCount = gSyncHack;
        }
    }

    //  If there's no list, we should clear the timer.
    if(!gTimerList) {
        if(gWindowsTimer) {
            ::KillTimer(NULL, gWindowsTimer);
            gWindowsTimer = 0;
            gNextFire = (DWORD)-1;
        }
    }
    else {
        //  See if we need to clear the current timer.
        //  Curcumstances are that if the timer will not
        //      fire on time for the next timeout.
        BOOL bSetTimer = FALSE;
        TimerImpl *pTimeout = gTimerList;
        if(gWindowsTimer)   {
            if(pTimeout->mFireTime != gNextFire)   {
                ::KillTimer(NULL, gWindowsTimer);
                gWindowsTimer = 0;
                gNextFire = (DWORD)-1;

                //  Set the timer.
                bSetTimer = TRUE;
            }
        }
        else    {
            //  No timer set, attempt.
            bSetTimer = TRUE;
        }

        if(bSetTimer)   {
            DWORD dwFireWhen = pTimeout->mFireTime > aTickCount ?
                pTimeout->mFireTime - aTickCount : 0;
            if(dwFireWhen > UINT_MAX)   {
                dwFireWhen = UINT_MAX;
            }
            UINT uFireWhen = (UINT)dwFireWhen;

            PR_ASSERT(gWindowsTimer == 0);
            gWindowsTimer = ::SetTimer(NULL, 0, uFireWhen, (TIMERPROC)FireTimeout);

            if(gWindowsTimer)   {
                //  Set the fire time.
                gNextFire = pTimeout->mFireTime;
            }
        }
    }
}

// Walk down the timeout list and launch anyone appropriate
void 
TimerImpl::ProcessTimeouts(DWORD aNow)
{
    TimerImpl *p = gTimerList;
    if(aNow == 0)   {
        aNow = ::GetTickCount();
    }

    BOOL bCalledSync = FALSE;

    //  Set the hack, such that when FE_ClearTimeout
    //      calls SyncTimeoutPeriod, that GetTickCount()
    //      overhead is not incurred.
    gSyncHack = aNow;
    
    // loop over all entries
    while(p) {
        // send it
        if(p->mFireTime < aNow) {
            p->Fire(aNow);

            //  Clear the timer.
            //  Period synced.
            p->Cancel();
            bCalledSync = TRUE;

            //  Reset the loop (can't look at p->pNext now, and called
            //      code may have added/cleared timers).
            //  (could do this by going recursive and returning).
            p = gTimerList;
        } else {
            //  Make sure we fire an timer.
            //  Also, we need to check to see if things are backing up (they
            //      may be asking to be fired long before we ever get to them,
            //      and we don't want to pass in negative values to the real
            //      timer code, or it takes days to fire....
            if(bCalledSync == FALSE)    {
                SyncTimeoutPeriod(aNow);
                bCalledSync = TRUE;
            }
            //  Get next timer.
            p = p->mNext;
        }
    }
    gSyncHack = 0;
}


TimerImpl::TimerImpl()
{
  NS_INIT_REFCNT();
  mFunc = NULL;
  mCallback = NULL;
  mNext = NULL;
}

TimerImpl::~TimerImpl()
{
}

nsresult 
TimerImpl::Init(nsTimerCallbackFunc aFunc,
                void *aClosure,
//              PRBool aRepeat, 
                PRUint32 aDelay)
{
    mFunc = aFunc;
    mClosure = aClosure;
    // mRepeat = aRepeat;

    return Init(aDelay);
}

nsresult 
TimerImpl::Init(nsITimerCallback *aCallback,
//              PRBool aRepeat, 
                PRUint32 aDelay)
{
    mCallback = aCallback;
    // mRepeat = aRepeat;

    return Init(aDelay);
}

nsresult
TimerImpl::Init(PRUint32 aDelay)
{
    DWORD dwNow = ::GetTickCount();
  
    mDelay = aDelay;
    mFireTime = (DWORD) aDelay + dwNow;
    mNext = NULL;

    // add it to the list
    if(!gTimerList) {        
        // no list add it
        gTimerList = this;
    } 
    else {

        // is it before everything else on the list?
        if(mFireTime < gTimerList->mFireTime) {

            mNext = gTimerList;
            gTimerList = this;

        } else {

            TimerImpl * pPrev = gTimerList;
            TimerImpl * pCurrent = gTimerList;

            while(pCurrent && (pCurrent->mFireTime <= mFireTime)) {
                pPrev = pCurrent;
                pCurrent = pCurrent->mNext;
            }

            PR_ASSERT(pPrev);

            // insert it after pPrev (this could be at the end of the list)
            mNext = pPrev->mNext;
            pPrev->mNext = this;

        }

    }

    NS_ADDREF(this);

    //  Sync the timer fire period.
    SyncTimeoutPeriod(dwNow);
    
    return NS_OK;
}

NS_IMPL_ISUPPORTS(TimerImpl, kITimerIID)

void
TimerImpl::Fire(DWORD aNow)
{
    if (mFunc != NULL) {
        (*mFunc)(this, mClosure);
    }
    else if (mCallback != NULL) {
         mCallback->Notify(this);
    }
}

void
TimerImpl::Cancel()
{
    TimerImpl *me = this;

    if(gTimerList == this) {

        // first element in the list lossage
        gTimerList = mNext;

    } else {

        // walk until no next pointer
        for(TimerImpl * p = gTimerList; p && p->mNext && (p->mNext != this); p = p->mNext)
            ;

        // if we found something valid pull it out of the list
        if(p && p->mNext && p->mNext == this) {
            p->mNext = mNext;

        } else {
            // get out before we delete something that looks bogus
            return;
        }

    }

    // if we got here it must have been a valid element so trash it
    NS_RELEASE(me);

    //  If there's now no be sure to clear the timer.
    SyncTimeoutPeriod(0);
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
