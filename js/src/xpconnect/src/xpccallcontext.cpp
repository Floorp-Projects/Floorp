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
 *   John Bandhauer <jband@netscape.com> (original author)
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

/* Call context. */

#include "xpcprivate.h"

XPCCallContext::XPCCallContext(XPCContext::LangType callerLanguage,
                               JSContext* cx    /* = nsnull  */,
                               JSObject* obj    /* = nsnull  */,
                               JSObject* funobj /* = nsnull  */,
                               jsval name       /* = 0       */,
                               uintN argc       /* = NO_ARGS */,
                               jsval *argv      /* = nsnull  */,
                               jsval *rval      /* = nsnull  */)
    :   mState(INIT_FAILED),
        mXPC(nsXPConnect::GetXPConnect()),
        mThreadData(nsnull),
        mXPCContext(nsnull),
        mJSContext(cx),
        mContextPopRequired(JS_FALSE),
        mDestroyJSContextInDestructor(JS_FALSE),
        mCallerLanguage(callerLanguage)
{
    NS_INIT_ISUPPORTS();

    if(!mXPC)
        return;

    NS_ADDREF(mXPC);

    if(!(mThreadData = XPCPerThreadData::GetData()))
        return;

    XPCJSContextStack* stack = mThreadData->GetJSContextStack();
    JSContext* topJSContext;

    if(!stack || NS_FAILED(stack->Peek(&topJSContext)))
    {
        NS_ERROR("bad!");
        mJSContext = nsnull;
        return;
    }

    if(!mJSContext)
    {
        // This is slightly questionable. If called without an explicit
        // JSContext (generally a call to a wrappedJS) we will use the JSContext
        // on the top of the JSContext stack - if there is one - *before*
        // falling back on the safe JSContext.
        // This is good AND bad because it makes calls from JS -> native -> JS
        // have JS stack 'continuity' for purposes of stack traces etc.
        // Note: this *is* what the pre-XPCCallContext xpconnect did too.

        if(topJSContext)
            mJSContext = topJSContext;
        else if(NS_FAILED(stack->GetSafeJSContext(&mJSContext)) || !mJSContext)
            return;
    }

    // Get into the request as early as we can to avoid problems with scanning
    // callcontexts on other threads from within the gc callbacks.

    if(mCallerLanguage == NATIVE_CALLER && JS_GetContextThread(mJSContext))
        JS_BeginRequest(mJSContext);

    if(topJSContext != mJSContext)
    {
        if(NS_FAILED(stack->Push(mJSContext)))
        {
            NS_ERROR("bad!");
            return;
        }
        mContextPopRequired = JS_TRUE;
    }

    // Try to get the JSContext -> XPCContext mapping from the cache.
    // FWIW... quicky tests show this hitting ~ 95% of the time.
    // That is a *lot* of locking we can skip in nsXPConnect::GetContext.
    mXPCContext = mThreadData->GetRecentXPCContext(mJSContext);

    if(!mXPCContext)
    {
        if(!(mXPCContext = nsXPConnect::GetContext(mJSContext, mXPC)))
            return;

        // Fill the cache.
        mThreadData->SetRecentContext(mJSContext, mXPCContext);
    }

    mPrevCallerLanguage = mXPCContext->SetCallingLangType(mCallerLanguage);

    // hook into call context chain for our thread
    mPrevCallContext = mThreadData->SetCallContext(this);

    mState = HAVE_CONTEXT;

    if(!obj)
        return;

    mMethodIndex = 0xDEAD;
    mOperandJSObject = obj;

    mState = HAVE_OBJECT;

    mTearOff = nsnull;
    mWrapper = XPCWrappedNative::GetWrappedNativeOfJSObject(mJSContext, obj,
                                                            funobj,
                                                            &mCurrentJSObject,
                                                            &mTearOff);
    if(!mWrapper)
        return;

    DEBUG_CheckWrapperThreadSafety(mWrapper);

    mFlattenedJSObject = mWrapper->GetFlatJSObject();

    if(mTearOff)
        mScriptableInfo = nsnull;
    else
        mScriptableInfo = mWrapper->GetScriptableInfo();

    if(name)
        SetName(name);

    if(argc != NO_ARGS)
        SetArgsAndResultPtr(argc, argv, rval);

    CHECK_STATE(HAVE_OBJECT);
}

void
XPCCallContext::SetName(jsval name)
{
    CHECK_STATE(HAVE_OBJECT);

    mName = name;

    if(mTearOff)
    {
        mSet = nsnull;
        mInterface = mTearOff->GetInterface();
        mMember = mInterface->FindMember(name);
        mStaticMemberIsLocal = JS_TRUE;
        if(mMember && !mMember->IsConstant())
            mMethodIndex = mMember->GetIndex();
    }
    else
    {
        mSet = mWrapper->GetSet();

        if(mSet->FindMember(name, &mMember, &mInterface,
                            mWrapper->HasProto() ?
                                mWrapper->GetProto()->GetSet() :
                                nsnull,
                            &mStaticMemberIsLocal))
        {
            if(mMember && !mMember->IsConstant())
                mMethodIndex = mMember->GetIndex();
        }
        else
        {
            mMember = nsnull;
            mInterface = nsnull;
            mStaticMemberIsLocal = JS_FALSE;
        }
    }

    mState = HAVE_NAME;
}

void
XPCCallContext::SetCallInfo(XPCNativeInterface* iface, XPCNativeMember* member,
                            JSBool isSetter)
{
    // We are going straight to the method info and need not do a lookup
    // by id.

    // don't be tricked if method is called with wrong 'this'
    if(mTearOff && mTearOff->GetInterface() != iface)
        mTearOff = nsnull;

    mSet = nsnull;
    mInterface = iface;
    mMember = member;
    mMethodIndex = mMember->GetIndex() + (isSetter ? 1 : 0);
    mName = mMember->GetName();

    if(mState < HAVE_NAME)
        mState = HAVE_NAME;
}

void
XPCCallContext::SetArgsAndResultPtr(uintN argc,
                                    jsval *argv,
                                    jsval *rval)
{
    CHECK_STATE(HAVE_OBJECT);

    mArgc   = argc;
    mArgv   = argv;
    mRetVal = rval;

    mExceptionWasThrown = mReturnValueWasSet = JS_FALSE;
    mState = HAVE_ARGS;
}

nsresult
XPCCallContext::CanCallNow()
{
    nsresult rv;
    
    if(!HasInterfaceAndMember())
        return NS_ERROR_UNEXPECTED;
    if(mState < HAVE_ARGS)
        return NS_ERROR_UNEXPECTED;

    if(!mTearOff)
    {
        mTearOff = mWrapper->FindTearOff(*this, mInterface, JS_FALSE, &rv);
        if(!mTearOff || mTearOff->GetInterface() != mInterface)
        {
            mTearOff = nsnull;    
            return NS_FAILED(rv) ? rv : NS_ERROR_UNEXPECTED;
        }
    }

    // Refresh in case FindTearOff extended the set
    mSet = mWrapper->GetSet();

    mState = READY_TO_CALL;
    return NS_OK;
}

void
XPCCallContext::SystemIsBeingShutDown()
{
    // XXX This is pretty questionable since the per thread cleanup stuff
    // can be making this call on one thread for call contexts on another
    // thread.
    NS_WARNING("Shutting Down XPConnect even through there is a live XPCCallContext");
    mThreadData = nsnull;
    mXPCContext = nsnull;
    mState = SYSTEM_SHUTDOWN;
    if(mPrevCallContext)
        mPrevCallContext->SystemIsBeingShutDown();
}

XPCCallContext::~XPCCallContext()
{
    NS_ASSERTION(mRefCnt == 0, "Someone is holding a bad reference to a XPCCallContext");

    // do cleanup...

    if(mXPCContext)
    {
        mXPCContext->SetCallingLangType(mPrevCallerLanguage);

        if(mContextPopRequired)
        {
            XPCJSContextStack* stack = mThreadData->GetJSContextStack();
            if(stack)
            {
#ifdef DEBUG
                JSContext* poppedCX;
                nsresult rv = stack->Pop(&poppedCX);
                NS_ASSERTION(NS_SUCCEEDED(rv) && poppedCX == mJSContext, "bad pop");
#else
                (void) stack->Pop(nsnull);
#endif
            }
            else
            {
                NS_ASSERTION(0, "bad!");
            }
        }

#ifdef DEBUG
        XPCCallContext* old = mThreadData->SetCallContext(mPrevCallContext);
        NS_ASSERTION(old == this, "bad pop from per thread data");
#else
        (void) mThreadData->SetCallContext(mPrevCallContext);
#endif
    }

    if(mJSContext)
    {
        if(mCallerLanguage == NATIVE_CALLER && JS_GetContextThread(mJSContext))
            JS_EndRequest(mJSContext);
        
        if(mDestroyJSContextInDestructor)
        {
#ifdef DEBUG_xpc_hacker
            printf("!xpc - doing deferred destruction of JSContext @ %0x\n", 
                   mJSContext);
#endif
            NS_ASSERTION(!mThreadData->GetJSContextStack() || 
                         !mThreadData->GetJSContextStack()->
                            DEBUG_StackHasJSContext(mJSContext),
                         "JSContext still in threadjscontextstack!");
        
            JS_DestroyContext(mJSContext);
            mXPC->SyncJSContexts();
        }
    }

    NS_IF_RELEASE(mXPC);
}

NS_IMPL_QUERY_INTERFACE1(XPCCallContext, nsIXPCNativeCallContext)
NS_IMPL_ADDREF(XPCCallContext)

NS_IMETHODIMP_(nsrefcnt)
XPCCallContext::Release(void)
{
  NS_PRECONDITION(0 != mRefCnt, "dup release");
  NS_ASSERT_OWNINGTHREAD(_class);
  --mRefCnt;
  NS_LOG_RELEASE(this, mRefCnt, "XPCCallContext");
  // no delete this!
  return mRefCnt;
}

/* readonly attribute nsISupports Callee; */
NS_IMETHODIMP
XPCCallContext::GetCallee(nsISupports * *aCallee)
{
    nsISupports* temp = mWrapper->GetIdentityObject();
    NS_IF_ADDREF(temp);
    *aCallee = temp;
    return NS_OK;
}

/* readonly attribute PRUint16 CalleeMethodIndex; */
NS_IMETHODIMP
XPCCallContext::GetCalleeMethodIndex(PRUint16 *aCalleeMethodIndex)
{
    *aCalleeMethodIndex = mMethodIndex;
    return NS_OK;
}

/* readonly attribute nsIXPConnectWrappedNative CalleeWrapper; */
NS_IMETHODIMP
XPCCallContext::GetCalleeWrapper(nsIXPConnectWrappedNative * *aCalleeWrapper)
{
    nsIXPConnectWrappedNative* temp = mWrapper;
    NS_IF_ADDREF(temp);
    *aCalleeWrapper = temp;
    return NS_OK;
}

/* readonly attribute XPCNativeInterface CalleeInterface; */
NS_IMETHODIMP
XPCCallContext::GetCalleeInterface(nsIInterfaceInfo * *aCalleeInterface)
{
    nsIInterfaceInfo* temp = mInterface->GetInterfaceInfo();
    NS_IF_ADDREF(temp);
    *aCalleeInterface = temp;
    return NS_OK;
}

/* readonly attribute nsIClassInfo CalleeClassInfo; */
NS_IMETHODIMP
XPCCallContext::GetCalleeClassInfo(nsIClassInfo * *aCalleeClassInfo)
{
    nsIClassInfo* temp = mWrapper->GetClassInfo();
    NS_IF_ADDREF(temp);
    *aCalleeClassInfo = temp;
    return NS_OK;
}

/* readonly attribute JSContextPtr JSContext; */
NS_IMETHODIMP
XPCCallContext::GetJSContext(JSContext * *aJSContext)
{
    *aJSContext = mJSContext;
    return NS_OK;
}

/* readonly attribute PRUint32 Argc; */
NS_IMETHODIMP
XPCCallContext::GetArgc(PRUint32 *aArgc)
{
    *aArgc = (PRUint32) mArgc;
    return NS_OK;
}

/* readonly attribute JSValPtr ArgvPtr; */
NS_IMETHODIMP
XPCCallContext::GetArgvPtr(jsval * *aArgvPtr)
{
    *aArgvPtr = mArgv;
    return NS_OK;
}

/* readonly attribute JSValPtr RetValPtr; */
NS_IMETHODIMP
XPCCallContext::GetRetValPtr(jsval * *aRetValPtr)
{
    *aRetValPtr = mRetVal;
    return NS_OK;
}

/* attribute PRBool ExceptionWasThrown; */
NS_IMETHODIMP
XPCCallContext::GetExceptionWasThrown(PRBool *aExceptionWasThrown)
{
    *aExceptionWasThrown = mExceptionWasThrown;
    return NS_OK;
}
NS_IMETHODIMP
XPCCallContext::SetExceptionWasThrown(PRBool aExceptionWasThrown)
{
    mExceptionWasThrown = aExceptionWasThrown;
    return NS_OK;
}

/* attribute PRBool ReturnValueWasSet; */
NS_IMETHODIMP
XPCCallContext::GetReturnValueWasSet(PRBool *aReturnValueWasSet)
{
    *aReturnValueWasSet = mReturnValueWasSet;
    return NS_OK;
}
NS_IMETHODIMP
XPCCallContext::SetReturnValueWasSet(PRBool aReturnValueWasSet)
{
    mReturnValueWasSet = aReturnValueWasSet;
    return NS_OK;
}

