/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 *
 * The contents of this file are subject to the Netscape Public
 * License Version 1.1 (the "License"); you may not use this file
 * except in compliance with the License. You may obtain a copy of
 * the License at http://www.mozilla.org/NPL/
 *
 * Software distributed under the License is distributed on an "AS
 * IS" basis, WITHOUT WARRANTY OF ANY KIND, either express oqr
 * implied. See the License for the specific language governing
 * rights and limitations under the License.
 *
 * The Original Code is Mozilla Communicator client code, released
 * March 31, 1998.
 *
 * The Initial Developer of the Original Code is Netscape
 * Communications Corporation.  Portions created by Netscape are
 * Copyright (C) 1998 Netscape Communications Corporation. All
 * Rights Reserved.
 *
 * Contributor(s): 
 *   John Bandhauer <jband@netscape.com>
 *
 * Alternatively, the contents of this file may be used under the
 * terms of the GNU Public License (the "GPL"), in which case the
 * provisions of the GPL are applicable instead of those above.
 * If you wish to allow use of your version of this file only
 * under the terms of the GPL and not to allow others to use your
 * version of this file under the NPL, indicate your decision by
 * deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL.  If you do not delete
 * the provisions above, a recipient may use your version of this
 * file under either the NPL or the GPL.
 */

/* Implement global service to track stack of JSContext per thread. */

#include "xpcprivate.h"

/*
* This object holds state that we don't want to lose!
*
* The plan is that once created this object never goes away. We do an
* intentional extra addref at construction to keep it around even if no one
* is using it.
*/

NS_IMPL_THREADSAFE_ISUPPORTS2(nsXPCThreadJSContextStackImpl, 
        nsIThreadJSContextStack, nsIJSContextStack)

static nsXPCThreadJSContextStackImpl* gXPCThreadJSContextStack = nsnull;

nsXPCThreadJSContextStackImpl::nsXPCThreadJSContextStackImpl()
{
    NS_INIT_ISUPPORTS();
}

nsXPCThreadJSContextStackImpl::~nsXPCThreadJSContextStackImpl()
{
    gXPCThreadJSContextStack = nsnull;
}

//static
nsXPCThreadJSContextStackImpl*
nsXPCThreadJSContextStackImpl::GetSingleton()
{
    if(!gXPCThreadJSContextStack)
    {
        gXPCThreadJSContextStack = new nsXPCThreadJSContextStackImpl();
        if(gXPCThreadJSContextStack)
        {
            // hold an extra reference to lock it down    
            NS_ADDREF(gXPCThreadJSContextStack);
        }
    }
    NS_IF_ADDREF(gXPCThreadJSContextStack);
    return gXPCThreadJSContextStack;
}

void
nsXPCThreadJSContextStackImpl::FreeSingleton()
{
    nsXPCThreadJSContextStackImpl* tcs = gXPCThreadJSContextStack;
    if (tcs) {
        nsrefcnt cnt;
        NS_RELEASE2(tcs, cnt);
#ifdef XPC_DUMP_AT_SHUTDOWN
        if (0 != cnt) {
            printf("*** dangling reference to nsXPCThreadJSContextStackImpl: refcnt=%d\n", cnt);
        }
#endif
    }
}

/* readonly attribute PRInt32 Count; */
NS_IMETHODIMP
nsXPCThreadJSContextStackImpl::GetCount(PRInt32 *aCount)
{
    if(!aCount)
        return NS_ERROR_NULL_POINTER;

    nsDeque* myStack = GetStackForCurrentThread();

    if(!myStack)
    {
        *aCount = 0;
        return NS_ERROR_FAILURE;
    }

    *aCount = myStack->GetSize();
    return NS_OK;
}

/* JSContext Peek (); */
NS_IMETHODIMP
nsXPCThreadJSContextStackImpl::Peek(JSContext * *_retval)
{
    if(!_retval)
        return NS_ERROR_NULL_POINTER;

    nsDeque* myStack = GetStackForCurrentThread();

    if(!myStack)
    {
        *_retval = nsnull;
        return NS_ERROR_FAILURE;
    }

    PRInt32 size = myStack->GetSize();
    if(size > 0)
        *_retval = (JSContext*) myStack->ObjectAt(size - 1);
    else
        *_retval = nsnull;
    return NS_OK;
}

/* JSContext Pop (); */
NS_IMETHODIMP
nsXPCThreadJSContextStackImpl::Pop(JSContext * *_retval)
{
    nsDeque* myStack = GetStackForCurrentThread();

    if(!myStack)
    {
        if(_retval)
            *_retval = nsnull;
        return NS_ERROR_FAILURE;
    }

    NS_ASSERTION(myStack->GetSize() > 0, "ThreadJSContextStack underflow");

    if(_retval)
        *_retval = (JSContext*) myStack->Pop();
    else
        myStack->Pop();
    return NS_OK;
}

/* void Push (in JSContext cx); */
NS_IMETHODIMP
nsXPCThreadJSContextStackImpl::Push(JSContext * cx)
{
    nsDeque* myStack = GetStackForCurrentThread();

    if(!myStack)
        return NS_ERROR_FAILURE;

    myStack->Push(cx);
    return NS_OK;
}

/* readonly attribute JSContext SafeJSContext; */
NS_IMETHODIMP 
nsXPCThreadJSContextStackImpl::GetSafeJSContext(JSContext * *aSafeJSContext)
{
    NS_ASSERTION(aSafeJSContext, "loser!");

    xpcPerThreadData* data = xpcPerThreadData::GetData();
    if(!data)
    {
        *aSafeJSContext = nsnull;
        return NS_ERROR_FAILURE;
    }

    JSContext* ptr = *aSafeJSContext = data->GetSafeJSContext();
    return ptr ? NS_OK : NS_ERROR_FAILURE;
}

/***************************************************************************/

static const PRUintn BAD_TLS_INDEX = (PRUintn) -1;

PRUintn           xpcPerThreadData::gTLSIndex = BAD_TLS_INDEX;
PRLock*           xpcPerThreadData::gLock     = nsnull;
xpcPerThreadData* xpcPerThreadData::gThreads  = nsnull;

xpcPerThreadData::xpcPerThreadData()
    :   mException(nsnull),
        mJSContextStack(new nsDeque(nsnull)),
        mSafeJSContext(nsnull),
        mOwnSafeJSContext(nsnull),
        mNextThread(nsnull)
{
    if(gLock)
    {
        nsAutoLock lock(gLock);
        mNextThread = gThreads;
        gThreads = this;
    }
}

void 
xpcPerThreadData::Cleanup()
{
    NS_IF_RELEASE(mException);

    if(mJSContextStack)
    {
        delete mJSContextStack;
        mJSContextStack = nsnull;
    }

    if(mOwnSafeJSContext)
    {
        JS_DestroyContext(mOwnSafeJSContext);
        mOwnSafeJSContext = nsnull;
    }
}

xpcPerThreadData::~xpcPerThreadData()
{
    Cleanup();

    // Unlink 'this' from the list of threads.
    if(gLock)
    {
        nsAutoLock lock(gLock);
        if(gThreads == this)
            gThreads = mNextThread;
        else
        {
            xpcPerThreadData* cur = gThreads;
            while(cur)
            {
                if(cur->mNextThread == this)
                {
                    cur->mNextThread = mNextThread;
                    break;            
                }
                cur = cur->mNextThread;    
            }        
        }
    }

    if(gLock && !gThreads)
    {
        PR_DestroyLock(gLock);
        gLock = nsnull;
    }
}

PRBool 
xpcPerThreadData::IsValid() const 
{
    return mJSContextStack != nsnull;
}

nsIXPCException*
xpcPerThreadData::GetException()
{
    NS_IF_ADDREF(mException);
    return mException;
}

void
xpcPerThreadData::SetException(nsIXPCException* aException)
{
    NS_IF_ADDREF(aException);
    NS_IF_RELEASE(mException);
    mException = aException;
}

nsDeque*
xpcPerThreadData::GetJSContextStack()
{
    return mJSContextStack;
}

/**************************/

static JSClass global_class = {
    "global_for_xpcPerThreadData_SafeJSContext", 0,
    JS_PropertyStub,  JS_PropertyStub,  JS_PropertyStub,  JS_PropertyStub,
    JS_EnumerateStub, JS_ResolveStub,   JS_ConvertStub,   JS_FinalizeStub
};

JSContext*
xpcPerThreadData::GetSafeJSContext()
{
    if(!mSafeJSContext)
    {
        JSRuntime *rt;
        nsCOMPtr<nsIJSRuntimeService> rtsvc = 
            do_GetService("@mozilla.org/js/xpc/RuntimeService;1");
        if(rtsvc && NS_SUCCEEDED(rtsvc->GetRuntime(&rt)) && rt)
        {
            nsCOMPtr<nsIXPConnect> xpc = do_GetService(nsIXPConnect::GetCID());
            if(xpc)
            {
                mSafeJSContext = JS_NewContext(rt, 8192);
                if(mSafeJSContext)
                {
                    JSObject *glob;
                    glob = JS_NewObject(mSafeJSContext, &global_class, NULL, NULL);
                    if(!glob || 
                       !JS_InitStandardClasses(mSafeJSContext, glob) ||
                       NS_FAILED(xpc->InitClasses(mSafeJSContext, glob)))
                    {
                        JS_DestroyContext(mSafeJSContext);
                        mSafeJSContext = nsnull;
                    }
                    // Save it off so we can destroy it later, even if
                    // mSafeJSContext has been set to another context
                    // via SetSafeJSContext.  If we don't get here,
                    // then mSafeJSContext must have been set via
                    // SetSafeJSContext, and we're not responsible for
                    // destroying the passed-in context.
                    mOwnSafeJSContext = mSafeJSContext;
                }
            }
        }
    }
    return mSafeJSContext;
}        

nsresult
xpcPerThreadData::SetSafeJSContext(JSContext *cx)
{
    if(mOwnSafeJSContext && 
       mOwnSafeJSContext == mSafeJSContext &&
       mOwnSafeJSContext != cx)
    {
        JS_DestroyContext(mOwnSafeJSContext);
        mOwnSafeJSContext = nsnull;
    }

    mSafeJSContext = cx;
    return NS_OK;
}

JS_STATIC_DLL_CALLBACK(void)
xpc_ThreadDataDtorCB(void* ptr)
{
    xpcPerThreadData* data = (xpcPerThreadData*) ptr;
    if(data)
        delete data;
}

// static 
xpcPerThreadData*
xpcPerThreadData::GetData()
{
    xpcPerThreadData* data;

    if(!gLock)
    {
        gLock = PR_NewLock();
        if(!gLock)
            return nsnull;
    }
    
    if(gTLSIndex == BAD_TLS_INDEX)
    {
        nsAutoLock lock(gLock);
        // check again now that we have the lock...
        if(gTLSIndex == BAD_TLS_INDEX)
        {
            if(PR_FAILURE == 
               PR_NewThreadPrivateIndex(&gTLSIndex, xpc_ThreadDataDtorCB))
            {
                NS_ASSERTION(0, "PR_NewThreadPrivateIndex failed!");
                gTLSIndex = BAD_TLS_INDEX;
                return nsnull;
            }
        }
    }

    data = (xpcPerThreadData*) PR_GetThreadPrivate(gTLSIndex);
    if(!data)
    {
        data = new xpcPerThreadData();
        if(!data || !data->IsValid())
        {
            NS_ASSERTION(0, "new xpcPerThreadData() failed!");
            if(data) 
                delete data;
            return nsnull;
        }
        if(PR_FAILURE == PR_SetThreadPrivate(gTLSIndex, data))
        {
            NS_ASSERTION(0, "PR_SetThreadPrivate failed!");
            delete data;
            return nsnull;
        }
    }
    return data;
}

// static 
void 
xpcPerThreadData::CleanupAllThreads()
{
    if(gLock)
    {
        nsAutoLock lock(gLock);

        for(xpcPerThreadData* cur = gThreads; cur; cur = cur->mNextThread)
            cur->Cleanup();
    }
}

