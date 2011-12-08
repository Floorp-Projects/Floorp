/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: set ts=8 sw=4 et tw=80:
 *
 * ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is Mozilla Communicator client code, released
 * March 31, 1998.
 *
 * The Initial Developer of the Original Code is
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 1998
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   John Bandhauer <jband@netscape.com> (original author)
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either of the GNU General Public License Version 2 or later (the "GPL"),
 * or the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

/* Implement global service to track stack of JSContext per thread. */

#include "xpcprivate.h"
#include "XPCWrapper.h"
#include "mozilla/Mutex.h"
#include "nsDOMJSUtils.h"
#include "nsIScriptGlobalObject.h"
#include "nsNullPrincipal.h"

using namespace mozilla;

/***************************************************************************/

XPCJSContextStack::XPCJSContextStack()
    : mStack(),
      mSafeJSContext(nsnull),
      mOwnSafeJSContext(nsnull)
{
    // empty...
}

XPCJSContextStack::~XPCJSContextStack()
{
    if (mOwnSafeJSContext) {
        JS_SetContextThread(mOwnSafeJSContext);
        JS_DestroyContext(mOwnSafeJSContext);
        mOwnSafeJSContext = nsnull;
    }
}

/* readonly attribute PRInt32 count; */
NS_IMETHODIMP
XPCJSContextStack::GetCount(PRInt32 *aCount)
{
    *aCount = mStack.Length();
    return NS_OK;
}

/* JSContext peek (); */
NS_IMETHODIMP
XPCJSContextStack::Peek(JSContext * *_retval)
{
    *_retval = mStack.IsEmpty() ? nsnull : mStack[mStack.Length() - 1].cx;
    return NS_OK;
}

/* JSContext pop (); */
NS_IMETHODIMP
XPCJSContextStack::Pop(JSContext * *_retval)
{
    NS_ASSERTION(!mStack.IsEmpty(), "ThreadJSContextStack underflow");

    PRUint32 idx = mStack.Length() - 1; // The thing we're popping

    if (_retval)
        *_retval = mStack[idx].cx;

    mStack.RemoveElementAt(idx);
    if (idx > 0) {
        --idx; // Advance to new top of the stack

        XPCJSContextInfo & e = mStack[idx];
        NS_ASSERTION(!e.suspendDepth || e.cx, "Shouldn't have suspendDepth without a cx!");
        if (e.cx) {
            if (e.suspendDepth) {
                JS_ResumeRequest(e.cx, e.suspendDepth);
                e.suspendDepth = 0;
            }

            if (e.savedFrameChain) {
                // Pop() can be called outside any request for e.cx.
                JSAutoRequest ar(e.cx);
                JS_RestoreFrameChain(e.cx);
                e.savedFrameChain = false;
            }
        }
    }
    return NS_OK;
}

static nsIPrincipal*
GetPrincipalFromCx(JSContext *cx)
{
    nsIScriptContextPrincipal* scp = GetScriptContextPrincipalFromJSContext(cx);
    if (scp) {
        nsIScriptObjectPrincipal* globalData = scp->GetObjectPrincipal();
        if (globalData)
            return globalData->GetPrincipal();
    }
    return nsnull;
}

/* void push (in JSContext cx); */
NS_IMETHODIMP
XPCJSContextStack::Push(JSContext * cx)
{
    JS_ASSERT_IF(cx, JS_GetContextThread(cx));
    if (mStack.Length() > 0) {
        XPCJSContextInfo & e = mStack[mStack.Length() - 1];
        if (e.cx) {
            if (e.cx == cx) {
                nsIScriptSecurityManager* ssm = XPCWrapper::GetSecurityManager();
                if (ssm) {
                    if (nsIPrincipal* globalObjectPrincipal = GetPrincipalFromCx(cx)) {
                        nsIPrincipal* subjectPrincipal = ssm->GetCxSubjectPrincipal(cx);
                        bool equals = false;
                        globalObjectPrincipal->Equals(subjectPrincipal, &equals);
                        if (equals) {
                            goto append;
                        }
                    }
                }
            }

            {
                // Push() can be called outside any request for e.cx.
                JSAutoRequest ar(e.cx);
                if (!JS_SaveFrameChain(e.cx))
                    return NS_ERROR_OUT_OF_MEMORY;
                e.savedFrameChain = true;
            }

            if (!cx)
                e.suspendDepth = JS_SuspendRequest(e.cx);
        }
    }

  append:
    if (!mStack.AppendElement(cx))
        return NS_ERROR_OUT_OF_MEMORY;
    return NS_OK;
}

#ifdef DEBUG
JSBool
XPCJSContextStack::DEBUG_StackHasJSContext(JSContext*  aJSContext)
{
    for (PRUint32 i = 0; i < mStack.Length(); i++)
        if (aJSContext == mStack[i].cx)
            return true;
    return false;
}
#endif

static JSBool
SafeGlobalResolve(JSContext *cx, JSObject *obj, jsid id)
{
    JSBool resolved;
    return JS_ResolveStandardClass(cx, obj, id, &resolved);
}

static void
SafeFinalize(JSContext* cx, JSObject* obj)
{
    nsIScriptObjectPrincipal* sop =
        static_cast<nsIScriptObjectPrincipal*>(xpc_GetJSPrivate(obj));
    NS_IF_RELEASE(sop);
}

static JSClass global_class = {
    "global_for_XPCJSContextStack_SafeJSContext",
    XPCONNECT_GLOBAL_FLAGS,
    JS_PropertyStub, JS_PropertyStub, JS_PropertyStub, JS_StrictPropertyStub,
    JS_EnumerateStub, SafeGlobalResolve, JS_ConvertStub, SafeFinalize,
    JSCLASS_NO_OPTIONAL_MEMBERS
};

// We just use the same reporter as the component loader
// XXX #include angels cry.
extern void
mozJSLoaderErrorReporter(JSContext *cx, const char *message, JSErrorReport *rep);

/* attribute JSContext safeJSContext; */
NS_IMETHODIMP
XPCJSContextStack::GetSafeJSContext(JSContext * *aSafeJSContext)
{
    if (!mSafeJSContext) {
        // Start by getting the principal holder and principal for this
        // context.  If we can't manage that, don't bother with the rest.
        nsRefPtr<nsNullPrincipal> principal = new nsNullPrincipal();
        nsCOMPtr<nsIScriptObjectPrincipal> sop;
        if (principal) {
            nsresult rv = principal->Init();
            if (NS_SUCCEEDED(rv))
              sop = new PrincipalHolder(principal);
        }
        if (!sop) {
            *aSafeJSContext = nsnull;
            return NS_ERROR_FAILURE;
        }

        JSRuntime *rt;
        XPCJSRuntime* xpcrt;

        nsXPConnect* xpc = nsXPConnect::GetXPConnect();
        nsCOMPtr<nsIXPConnect> xpcholder(static_cast<nsIXPConnect*>(xpc));

        if (xpc && (xpcrt = xpc->GetRuntime()) && (rt = xpcrt->GetJSRuntime())) {
            JSObject *glob;
            mSafeJSContext = JS_NewContext(rt, 8192);
            if (mSafeJSContext) {
                // scoped JS Request
                JSAutoRequest req(mSafeJSContext);

                JS_SetErrorReporter(mSafeJSContext, mozJSLoaderErrorReporter);

                // Because we can run off the main thread, we create an MT
                // global object. Our principal is the unique key.
                JSCompartment *compartment;
                nsresult rv = xpc_CreateMTGlobalObject(mSafeJSContext,
                                                       &global_class,
                                                       principal, &glob,
                                                       &compartment);
                if (NS_FAILED(rv))
                    glob = nsnull;

                if (glob) {
                    // Make sure the context is associated with a proper compartment
                    // and not the default compartment.
                    JS_SetGlobalObject(mSafeJSContext, glob);

                    // Note: make sure to set the private before calling
                    // InitClasses
                    nsIScriptObjectPrincipal* priv = nsnull;
                    sop.swap(priv);
                    if (!JS_SetPrivate(mSafeJSContext, glob, priv)) {
                        // Drop the whole thing
                        NS_RELEASE(priv);
                        glob = nsnull;
                    }
                }

                // After this point either glob is null and the
                // nsIScriptObjectPrincipal ownership is either handled by the
                // nsCOMPtr or dealt with, or we'll release in the finalize
                // hook.
                if (glob && NS_FAILED(xpc->InitClasses(mSafeJSContext, glob))) {
                    glob = nsnull;
                }

            }
            if (mSafeJSContext && !glob) {
                // Destroy the context outside the scope of JSAutoRequest that
                // uses the context in its destructor.
                JS_DestroyContext(mSafeJSContext);
                mSafeJSContext = nsnull;
            }
            // Save it off so we can destroy it later.
            mOwnSafeJSContext = mSafeJSContext;
        }
    }

    *aSafeJSContext = mSafeJSContext;
    return mSafeJSContext ? NS_OK : NS_ERROR_UNEXPECTED;
}

/***************************************************************************/

PRUintn           XPCPerThreadData::gTLSIndex       = BAD_TLS_INDEX;
Mutex*            XPCPerThreadData::gLock           = nsnull;
XPCPerThreadData* XPCPerThreadData::gThreads        = nsnull;
XPCPerThreadData *XPCPerThreadData::sMainThreadData = nsnull;
void *            XPCPerThreadData::sMainJSThread   = nsnull;

XPCPerThreadData::XPCPerThreadData()
    :   mJSContextStack(new XPCJSContextStack()),
        mNextThread(nsnull),
        mCallContext(nsnull),
        mResolveName(JSID_VOID),
        mResolvingWrapper(nsnull),
        mExceptionManager(nsnull),
        mException(nsnull),
        mExceptionManagerNotAvailable(false),
        mAutoRoots(nsnull)
#ifdef XPC_CHECK_WRAPPER_THREADSAFETY
      , mWrappedNativeThreadsafetyReportDepth(0)
#endif
{
    MOZ_COUNT_CTOR(xpcPerThreadData);
    if (gLock) {
        MutexAutoLock lock(*gLock);
        mNextThread = gThreads;
        gThreads = this;
    }
}

void
XPCPerThreadData::Cleanup()
{
    while (mAutoRoots)
        mAutoRoots->Unlink();
    NS_IF_RELEASE(mExceptionManager);
    NS_IF_RELEASE(mException);
    delete mJSContextStack;
    mJSContextStack = nsnull;

    if (mCallContext)
        mCallContext->SystemIsBeingShutDown();
}

XPCPerThreadData::~XPCPerThreadData()
{
    /* Be careful to ensure that both any update to |gThreads| and the
       decision about whether or not to destroy the lock, are done
       atomically.  See bug 557586. */
    bool doDestroyLock = false;

    MOZ_COUNT_DTOR(xpcPerThreadData);

    Cleanup();

    // Unlink 'this' from the list of threads.
    if (gLock) {
        MutexAutoLock lock(*gLock);
        if (gThreads == this)
            gThreads = mNextThread;
        else {
            XPCPerThreadData* cur = gThreads;
            while (cur) {
                if (cur->mNextThread == this) {
                    cur->mNextThread = mNextThread;
                    break;
                }
                cur = cur->mNextThread;
            }
        }
        if (!gThreads)
            doDestroyLock = true;
    }

    if (gLock && doDestroyLock) {
        delete gLock;
        gLock = nsnull;
    }
}

static void
xpc_ThreadDataDtorCB(void* ptr)
{
    XPCPerThreadData* data = (XPCPerThreadData*) ptr;
    delete data;
}

void XPCPerThreadData::TraceJS(JSTracer *trc)
{
#ifdef XPC_TRACK_AUTOMARKINGPTR_STATS
    {
        static int maxLength = 0;
        int length = 0;
        for (AutoMarkingPtr* p = mAutoRoots; p; p = p->GetNext())
            length++;
        if (length > maxLength)
            maxLength = length;
        printf("XPC gc on thread %x with %d AutoMarkingPtrs (%d max so far)\n",
               this, length, maxLength);
    }
#endif

    if (mAutoRoots)
        mAutoRoots->TraceJS(trc);
}

void XPCPerThreadData::MarkAutoRootsAfterJSFinalize()
{
    if (mAutoRoots)
        mAutoRoots->MarkAfterJSFinalize();
}

// static
XPCPerThreadData*
XPCPerThreadData::GetDataImpl(JSContext *cx)
{
    XPCPerThreadData* data;

    if (!gLock) {
        gLock = new Mutex("XPCPerThreadData.gLock");
    }

    if (gTLSIndex == BAD_TLS_INDEX) {
        MutexAutoLock lock(*gLock);
        // check again now that we have the lock...
        if (gTLSIndex == BAD_TLS_INDEX) {
            if (PR_FAILURE ==
                PR_NewThreadPrivateIndex(&gTLSIndex, xpc_ThreadDataDtorCB)) {
                NS_ERROR("PR_NewThreadPrivateIndex failed!");
                gTLSIndex = BAD_TLS_INDEX;
                return nsnull;
            }
        }
    }

    data = (XPCPerThreadData*) PR_GetThreadPrivate(gTLSIndex);
    if (!data) {
        data = new XPCPerThreadData();
        if (!data || !data->IsValid()) {
            NS_ERROR("new XPCPerThreadData() failed!");
            delete data;
            return nsnull;
        }
        if (PR_FAILURE == PR_SetThreadPrivate(gTLSIndex, data)) {
            NS_ERROR("PR_SetThreadPrivate failed!");
            delete data;
            return nsnull;
        }
    }

    if (cx && !sMainJSThread && NS_IsMainThread()) {
        sMainJSThread = cx->thread();

        sMainThreadData = data;

        sMainThreadData->mThread = PR_GetCurrentThread();
    }

    return data;
}

// static
void
XPCPerThreadData::CleanupAllThreads()
{
    // I've questioned the sense of cleaning up other threads' data from the
    // start. But I got talked into it. Now I see that we *can't* do all the
    // cleaup while holding this lock. So, we are going to go to the trouble
    // to copy out the data that needs to be cleaned up *outside* of
    // the lock. Yuk!

    XPCJSContextStack** stacks = nsnull;
    int count = 0;
    int i;

    if (gLock) {
        MutexAutoLock lock(*gLock);

        for (XPCPerThreadData* cur = gThreads; cur; cur = cur->mNextThread)
            count++;

        stacks = (XPCJSContextStack**) new XPCJSContextStack*[count] ;
        if (stacks) {
            i = 0;
            for (XPCPerThreadData* cur = gThreads; cur; cur = cur->mNextThread) {
                stacks[i++] = cur->mJSContextStack;
                cur->mJSContextStack = nsnull;
                cur->Cleanup();
            }
        }
    }

    if (stacks) {
        for (i = 0; i < count; i++)
            delete stacks[i];
        delete [] stacks;
    }

    if (gTLSIndex != BAD_TLS_INDEX)
        PR_SetThreadPrivate(gTLSIndex, nsnull);
}

// static
XPCPerThreadData*
XPCPerThreadData::IterateThreads(XPCPerThreadData** iteratorp)
{
    *iteratorp = (*iteratorp == nsnull) ? gThreads : (*iteratorp)->mNextThread;
    return *iteratorp;
}

NS_IMPL_ISUPPORTS1(nsXPCJSContextStackIterator, nsIJSContextStackIterator)

NS_IMETHODIMP
nsXPCJSContextStackIterator::Reset(nsIJSContextStack *aStack)
{
    NS_ASSERTION(aStack == nsXPConnect::GetXPConnect(),
                 "aStack must be implemented by XPConnect singleton");
    XPCPerThreadData* data = XPCPerThreadData::GetData(nsnull);
    if (!data)
        return NS_ERROR_FAILURE;
    mStack = data->GetJSContextStack()->GetStack();
    if (mStack->IsEmpty())
        mStack = nsnull;
    else
        mPosition = mStack->Length() - 1;

    return NS_OK;
}

NS_IMETHODIMP
nsXPCJSContextStackIterator::Done(bool *aDone)
{
    *aDone = !mStack;
    return NS_OK;
}

NS_IMETHODIMP
nsXPCJSContextStackIterator::Prev(JSContext **aContext)
{
    if (!mStack)
        return NS_ERROR_NOT_INITIALIZED;

    *aContext = mStack->ElementAt(mPosition).cx;

    if (mPosition == 0)
        mStack = nsnull;
    else
        --mPosition;

    return NS_OK;
}

