/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 4 -*-
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

#include "nsIThread.h"
#include <stdio.h>

class nsRunner : public nsIRunnable {
public:
    NS_DECL_ISUPPORTS

    NS_IMETHOD Run() {
        printf("running %d\n", mNum);
        nsIThread* thread;
        nsresult rv = nsIThread::GetCurrent(&thread);
        if (NS_FAILED(rv)) {
            printf("failed to get current thread\n");
            return rv;
        }
        NS_RELEASE(thread);

        // if we don't do something slow, we'll never see the other
        // worker threads run
        PR_Sleep(100);

        return rv;
    }

    nsRunner(int num) : mNum(num) {
        NS_INIT_REFCNT();
    }

protected:
    int mNum;
};

NS_IMPL_ISUPPORTS(nsRunner, nsIRunnable::GetIID());

nsresult
TestThreads()
{
    nsresult rv;

    nsIThread* runner;
    rv = NS_NewThread(&runner, new nsRunner(0));
    if (NS_FAILED(rv)) {
        printf("failed to create thread\n");
        return rv;
    }

    nsIThread* thread;
    rv = nsIThread::GetCurrent(&thread);
    if (NS_FAILED(rv)) {
        printf("failed to get current thread\n");
        return rv;
    }

    PRThreadScope scope;
    rv = runner->GetScope(&scope);
    if (NS_FAILED(rv)) {
        printf("runner already exited\n");        
    }

    rv = runner->Join();     // wait for the runner to die before quitting
    if (NS_FAILED(rv)) {
        printf("join failed\n");        
    }

    rv = runner->GetScope(&scope);      // this should fail after Join
    if (NS_SUCCEEDED(rv)) {
        printf("get scope failed\n");        
    }

    rv = runner->Interrupt();   // this should fail after Join
    if (NS_SUCCEEDED(rv)) {
        printf("interrupt failed\n");        
    }

    NS_RELEASE(runner);
    NS_RELEASE(thread);

    ////////////////////////////////////////////////////////////////////////////
    // try an unjoinable thread 
    rv = NS_NewThread(&runner, new nsRunner(1), 0, PR_USER_THREAD,
                      PR_PRIORITY_NORMAL, PR_GLOBAL_THREAD, PR_UNJOINABLE_THREAD);
    if (NS_FAILED(rv)) {
        printf("failed to create thread\n");
        return rv;
    }

    rv = runner->Join();     // wait for the runner to die before quitting
    if (NS_SUCCEEDED(rv)) {
        printf("shouldn't have been able to join an unjoinable thread\n");        
    }
    NS_RELEASE(runner);

    return NS_OK;
}

nsresult
TestThreadPools()
{
    nsIThreadPool* pool;
    nsresult rv = NS_NewThreadPool(&pool, 4, 4);
    if (NS_FAILED(rv)) {
        printf("failed to create thead pool\n");
        return rv;
    }

    for (PRUint32 i = 0; i < 100; i++) {
        rv = pool->DispatchRequest(new nsRunner(i+2));
    }
    rv = pool->Join();
    return rv;
}

int
main()
{
    nsresult rv = TestThreads();
    if (NS_FAILED(rv)) return -1;

    rv = TestThreadPools();
    if (NS_FAILED(rv)) return -1;

    return 0;
}
