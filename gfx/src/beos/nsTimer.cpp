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
#include <OS.h>
#include <Application.h>
#include <Message.h>
#include "nsTimer.h"

static NS_DEFINE_IID(kITimerIID, NS_ITIMER_IID);

struct ThreadInterfaceData
{
	void	*data;
	int32	sync;
};

static sem_id my_find_sem(const char *name)
{
	sem_id	ret = B_ERROR;

	/* Get the sem_info for every sempahore in this team. */
	sem_info info;
	int32 cookie = 0;

	while(get_next_sem_info(0, &cookie, &info) == B_OK)
		if(strcmp(name, info.name) == 0)
		{
			ret = info.sem;
			break;
		}

	return ret;
}

void TimerImpl::FireTimeout()
{
	if(! canceled)
	{
		if (mFunc != NULL)
			(*mFunc)(this, mClosure);	    // If there's a function, call it.
		else if (mCallback != NULL)
			mCallback->Notify(this);	    // But if there's an interface, notify it.
	}
}

TimerImpl::TimerImpl()
{
	NS_INIT_REFCNT();
	mFunc = NULL;
	mCallback = NULL;
	mNext = NULL;
	mDelay = 0;
	mClosure = NULL;
	sleepsem = B_ERROR;
	canceled = false;
}

TimerImpl::~TimerImpl()
{
	Cancel();
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
    mDelay = aDelay;
    NS_ADDREF(this);	// this is for clients of the timer

	mThread = PR_GetCurrentThread();
	sleepsem = create_sem(0, "sleep sem");
	mThreadID = spawn_thread(Sleepy, "Mozilla Timer", B_URGENT_DISPLAY_PRIORITY, this);
	if(mThreadID > 0)
	{
	    NS_ADDREF(this);	// this is for the timer thread
		resume_thread(mThreadID);

	    return NS_OK;
	}
	else
		return NS_ERROR_FAILURE;
}

NS_IMPL_ISUPPORTS(TimerImpl, kITimerIID)


void
TimerImpl::Cancel()
{
	delete_sem(sleepsem);
	sleepsem = B_ERROR;
	canceled = true;
}

long TimerImpl::Sleepy( void *args )
{
	TimerImpl *tobj = (TimerImpl *)args;

	char		portname[64];
	char		semname[64];

	sprintf(portname, "event%lx", tobj->mThread);
	sprintf(semname, "sync%lx", tobj->mThread);

	port_id eventport = find_port(portname);
	sem_id syncsem = my_find_sem(semname);

	bigtime_t sleeptime = (long long)(tobj->mDelay) * 1000L;
	if(sleeptime == 0) sleeptime = 10;
	if(acquire_sem_etc(tobj->sleepsem, 1, B_TIMEOUT, sleeptime) == B_TIMED_OUT)
	{
		// call timer synchronously so we're sure tobj is alive
		ThreadInterfaceData	 id;
		id.data = tobj;
		id.sync = true;
		if(write_port(eventport, WM_TIMER, &id, sizeof(id)) == B_OK)
			while(acquire_sem(syncsem) == B_INTERRUPTED)
				;
	}
	delete_sem(tobj->sleepsem);
	tobj->sleepsem = B_ERROR;

	NS_RELEASE(tobj);

	return 0;
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


void nsTimerExpired(void *aCallData)
{
  TimerImpl* timer = (TimerImpl *)aCallData;
  timer->FireTimeout();
}
