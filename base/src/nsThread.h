/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/*
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

#ifndef nsThread_h__
#define nsThread_h__

#include "nsIThread.h"
#include "nsISupportsArray.h"

class nsThread : public nsIThread 
{
public:
    NS_DECL_ISUPPORTS

    // nsIThread methods:
    NS_IMETHOD Join();
    NS_IMETHOD GetPriority(PRThreadPriority *result);
    NS_IMETHOD SetPriority(PRThreadPriority value);
    NS_IMETHOD Interrupt();
    NS_IMETHOD GetScope(PRThreadScope *result);
    NS_IMETHOD GetType(PRThreadType *result);
    NS_IMETHOD GetState(PRThreadState *result);

    // nsThread methods:
    nsThread();
    virtual ~nsThread();

    nsresult Init(nsIRunnable* runnable,
                  PRUint32 stackSize,
                  PRThreadType type,
                  PRThreadPriority priority,
                  PRThreadScope scope,
                  PRThreadState state);
    static void Main(void* arg);

protected:
    PRThread*           mThread;
    nsIRunnable*        mRunnable;
};

////////////////////////////////////////////////////////////////////////////////

class nsThreadPool : public nsIThreadPool
{
public:
    NS_DECL_ISUPPORTS

    // nsIThreadPool methods:
    NS_IMETHOD DispatchRequest(nsIRunnable* runnable);

    // nsThreadPool methods:
    nsThreadPool(PRUint32 minThreads, PRUint32 maxThreads);
    virtual ~nsThreadPool();

    nsresult Init(PRUint32 stackSize,
                  PRThreadType type,
                  PRThreadPriority priority,
                  PRThreadScope scope,
                  PRThreadState state);
    nsIRunnable* Dequeue();
    
protected:
    nsISupportsArray*   mThreads;
    nsISupportsArray*   mRequests;
    PRUint32            mMinThreads;
    PRUint32            mMaxThreads;
};

////////////////////////////////////////////////////////////////////////////////

class nsThreadPoolRunnable : public nsIRunnable 
{
public:
    NS_DECL_ISUPPORTS

    // nsIRunnable methods:
    NS_IMETHOD Run();

    // nsThreadPoolRunnable methods:
    nsThreadPoolRunnable(nsThreadPool* pool);
    virtual ~nsThreadPoolRunnable();

protected:
    nsThreadPool*       mPool;

};

////////////////////////////////////////////////////////////////////////////////

#endif // nsThread_h__
