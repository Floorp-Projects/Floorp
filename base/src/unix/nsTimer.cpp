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
#include <limits.h>

static NS_DEFINE_IID(kITimerIID, NS_ITIMER_IID);

/*
 * Implementation of timers lifted from Windows front-end file timer.cpp
 */
class TimerImpl : public nsITimer {
public:

public:
  TimerImpl();
  virtual ~TimerImpl();

  virtual nsresult Init(nsTimerCallbackFunc aFunc,
                void *aClosure,
//              PRBool aRepeat, 
                PRUint32 aDelay);

  virtual nsresult Init(nsITimerCallback *aCallback,
//              PRBool aRepeat, 
                PRUint32 aDelay);

  NS_DECL_ISUPPORTS

  virtual void Cancel();
  virtual PRUint32 GetDelay() { return mDelay; }
  virtual void SetDelay(PRUint32 aDelay) { };

private:
  nsresult Init(PRUint32 aDelay);

  PRUint32 mDelay;
  nsTimerCallbackFunc mFunc;
  void *mClosure;
  nsITimerCallback *mCallback;
  // PRBool mRepeat;
  TimerImpl *mNext;
};

//TimerImpl *TimerImpl::gTimerList = NULL;


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

    NS_ADDREF(this);

    return NS_OK;
}

NS_IMPL_ISUPPORTS(TimerImpl, kITimerIID)


void
TimerImpl::Cancel()
{

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
