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

#include <KernelKit.h>
#include <prthread.h>

#define WM_TIMER		'WMti'
#include "nsITimer.h"

class TimerImpl : public nsITimer {
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
    virtual void SetDelay(PRUint32 aDelay) { mDelay=aDelay; };
    virtual void* GetClosure() { return mClosure; }

    void FireTimeout();

private:
    nsresult	Init(PRUint32 aDelay);	// Initialize the timer.
    static long	Sleepy( void *args);	// The tiny sleeping function.

    PRUint32		mDelay;	    // The delay set in Init()
    nsTimerCallbackFunc	mFunc;	    // The function to call back when expired
    void		*mClosure;  // The argumnet to pass it.
    nsITimerCallback	*mCallback; // An interface to notify when expired.
//  PRBool		mRepeat;    // A repeat, not implemented yet.
    TimerImpl		*mNext;	    // Do we even use this? ***
    thread_id		mThreadID;  // The ID of the thread we spawn off.
    sem_id			sleepsem;
    PRThread*		mThread;
	bool			canceled;
};
