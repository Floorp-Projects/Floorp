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

#ifndef nsIThread_h__
#define nsIThread_h__

#include "nsISupports.h"
#include "prthread.h"
#include "nscore.h"

////////////////////////////////////////////////////////////////////////////////

// XXX regenerate:
#define NS_IRUNNABLE_IID                             \
{ /* 677d9a90-93ee-11d2-816a-006008119d7a */         \
    0x677d9a90,                                      \
    0x93ee,                                          \
    0x11d2,                                          \
    {0x81, 0x6a, 0x00, 0x60, 0x19, 0x11, 0x9d, 0x7a} \
}

class nsIRunnable : public nsISupports
{
public:
    NS_DEFINE_STATIC_IID_ACCESSOR(NS_IRUNNABLE_IID);

    NS_IMETHOD Run() = 0;

};

////////////////////////////////////////////////////////////////////////////////

// XXX regenerate:
#define NS_ITHREAD_IID                               \
{ /* 677d9a90-93ee-11d2-816a-006008119d7a */         \
    0x677d9a90,                                      \
    0x93ee,                                          \
    0x11d2,                                          \
    {0x81, 0x6a, 0x00, 0x60, 0x29, 0x11, 0x9d, 0x7a} \
}

class nsIThread : public nsISupports 
{
public:
    NS_DEFINE_STATIC_IID_ACCESSOR(NS_ITHREAD_IID);

    static NS_BASE nsresult GetCurrent(nsIThread* *result);

    NS_IMETHOD Join() = 0;

    NS_IMETHOD GetPriority(PRThreadPriority *result) = 0;
    NS_IMETHOD SetPriority(PRThreadPriority value) = 0;

    NS_IMETHOD Interrupt() = 0;

    NS_IMETHOD GetScope(PRThreadScope *result) = 0;
    NS_IMETHOD GetType(PRThreadType *result) = 0;
    NS_IMETHOD GetState(PRThreadState *result) = 0;

    NS_IMETHOD GetPRThread(PRThread* *result) = 0;
};

extern NS_BASE nsresult
NS_NewThread(nsIThread* *result, 
             nsIRunnable* runnable,
             PRUint32 stackSize = 0,
             PRThreadType type = PR_USER_THREAD,
             PRThreadPriority priority = PR_PRIORITY_NORMAL,
             PRThreadScope scope = PR_GLOBAL_THREAD,
             PRThreadState state = PR_JOINABLE_THREAD);

////////////////////////////////////////////////////////////////////////////////

// XXX regenerate:
#define NS_ITHREADPOOL_IID                           \
{ /* 677d9a90-93ee-11d2-816a-006008119d7a */         \
    0x677d9a90,                                      \
    0x93ee,                                          \
    0x11d2,                                          \
    {0x81, 0x6a, 0x00, 0x60, 0x39, 0x11, 0x9d, 0x7a} \
}

/**
 * A thread pool is used to create a group of worker threads that
 * share in the servicing of requests. Requests are run in the 
 * context of the worker thread.
 */
class nsIThreadPool : public nsISupports
{
public:
    NS_DEFINE_STATIC_IID_ACCESSOR(NS_ITHREADPOOL_IID);

    NS_IMETHOD DispatchRequest(nsIRunnable* runnable) = 0;

    NS_IMETHOD Join() = 0;

    NS_IMETHOD Interrupt() = 0;
};

extern NS_BASE nsresult
NS_NewThreadPool(nsIThreadPool* *result,
                 PRUint32 minThreads, PRUint32 maxThreads,
                 PRUint32 stackSize = 0,
                 PRThreadType type = PR_USER_THREAD,
                 PRThreadPriority priority = PR_PRIORITY_NORMAL,
                 PRThreadScope scope = PR_GLOBAL_THREAD,
                 PRThreadState state = PR_JOINABLE_THREAD);

////////////////////////////////////////////////////////////////////////////////

#endif // nsIThread_h__
