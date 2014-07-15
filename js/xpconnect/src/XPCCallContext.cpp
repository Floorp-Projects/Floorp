/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* vim: set ts=8 sts=4 et sw=4 tw=99: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/* Call context. */

#include "xpcprivate.h"
#include "jswrapper.h"

using namespace mozilla;
using namespace xpc;
using namespace JS;

#define IS_TEAROFF_CLASS(clazz) ((clazz) == &XPC_WN_Tearoff_JSClass)

XPCCallContext::XPCCallContext(XPCContext::LangType callerLanguage,
                               JSContext* cx,
                               HandleObject obj    /* = nullptr               */,
                               HandleObject funobj /* = nullptr               */,
                               HandleId name       /* = JSID_VOID             */,
                               unsigned argc       /* = NO_ARGS               */,
                               jsval *argv         /* = nullptr               */,
                               jsval *rval         /* = nullptr               */)
    :   mAr(cx),
        mState(INIT_FAILED),
        mXPC(nsXPConnect::XPConnect()),
        mXPCContext(nullptr),
        mJSContext(cx),
        mCallerLanguage(callerLanguage),
        mWrapper(nullptr),
        mTearOff(nullptr),
        mName(cx)
{
    MOZ_ASSERT(cx);
    MOZ_ASSERT(cx == XPCJSRuntime::Get()->GetJSContextStack()->Peek());

    if (!mXPC)
        return;

    mXPCContext = XPCContext::GetXPCContext(mJSContext);
    mPrevCallerLanguage = mXPCContext->SetCallingLangType(mCallerLanguage);

    // hook into call context chain.
    mPrevCallContext = XPCJSRuntime::Get()->SetCallContext(this);

    mState = HAVE_CONTEXT;

    if (!obj)
        return;

    mMethodIndex = 0xDEAD;

    mState = HAVE_OBJECT;

    mTearOff = nullptr;

    JSObject *unwrapped = js::CheckedUnwrap(obj, /* stopAtOuter = */ false);
    if (!unwrapped) {
        JS_ReportError(mJSContext, "Permission denied to call method on |this|");
        mState = INIT_FAILED;
        return;
    }
    const js::Class *clasp = js::GetObjectClass(unwrapped);
    if (IS_WN_CLASS(clasp)) {
        mWrapper = XPCWrappedNative::Get(unwrapped);
    } else if (IS_TEAROFF_CLASS(clasp)) {
        mTearOff = (XPCWrappedNativeTearOff*)js::GetObjectPrivate(unwrapped);
        mWrapper = XPCWrappedNative::Get(js::GetObjectParent(unwrapped));
    }
    if (mWrapper) {
        if (mTearOff)
            mScriptableInfo = nullptr;
        else
            mScriptableInfo = mWrapper->GetScriptableInfo();
    }

    if (!JSID_IS_VOID(name))
        SetName(name);

    if (argc != NO_ARGS)
        SetArgsAndResultPtr(argc, argv, rval);

    CHECK_STATE(HAVE_OBJECT);
}

// static
JSContext *
XPCCallContext::GetDefaultJSContext()
{
    // This is slightly questionable. If called without an explicit
    // JSContext (generally a call to a wrappedJS) we will use the JSContext
    // on the top of the JSContext stack - if there is one - *before*
    // falling back on the safe JSContext.
    // This is good AND bad because it makes calls from JS -> native -> JS
    // have JS stack 'continuity' for purposes of stack traces etc.
    // Note: this *is* what the pre-XPCCallContext xpconnect did too.

    XPCJSContextStack* stack = XPCJSRuntime::Get()->GetJSContextStack();
    JSContext *topJSContext = stack->Peek();

    return topJSContext ? topJSContext : stack->GetSafeJSContext();
}

void
XPCCallContext::SetName(jsid name)
{
    CHECK_STATE(HAVE_OBJECT);

    mName = name;

    if (mTearOff) {
        mSet = nullptr;
        mInterface = mTearOff->GetInterface();
        mMember = mInterface->FindMember(mName);
        mStaticMemberIsLocal = true;
        if (mMember && !mMember->IsConstant())
            mMethodIndex = mMember->GetIndex();
    } else {
        mSet = mWrapper ? mWrapper->GetSet() : nullptr;

        if (mSet &&
            mSet->FindMember(mName, &mMember, &mInterface,
                             mWrapper->HasProto() ?
                             mWrapper->GetProto()->GetSet() :
                             nullptr,
                             &mStaticMemberIsLocal)) {
            if (mMember && !mMember->IsConstant())
                mMethodIndex = mMember->GetIndex();
        } else {
            mMember = nullptr;
            mInterface = nullptr;
            mStaticMemberIsLocal = false;
        }
    }

    mState = HAVE_NAME;
}

void
XPCCallContext::SetCallInfo(XPCNativeInterface* iface, XPCNativeMember* member,
                            bool isSetter)
{
    CHECK_STATE(HAVE_CONTEXT);

    // We are going straight to the method info and need not do a lookup
    // by id.

    // don't be tricked if method is called with wrong 'this'
    if (mTearOff && mTearOff->GetInterface() != iface)
        mTearOff = nullptr;

    mSet = nullptr;
    mInterface = iface;
    mMember = member;
    mMethodIndex = mMember->GetIndex() + (isSetter ? 1 : 0);
    mName = mMember->GetName();

    if (mState < HAVE_NAME)
        mState = HAVE_NAME;
}

void
XPCCallContext::SetArgsAndResultPtr(unsigned argc,
                                    jsval *argv,
                                    jsval *rval)
{
    CHECK_STATE(HAVE_OBJECT);

    if (mState < HAVE_NAME) {
        mSet = nullptr;
        mInterface = nullptr;
        mMember = nullptr;
        mStaticMemberIsLocal = false;
    }

    mArgc   = argc;
    mArgv   = argv;
    mRetVal = rval;

    mState = HAVE_ARGS;
}

nsresult
XPCCallContext::CanCallNow()
{
    nsresult rv;

    if (!HasInterfaceAndMember())
        return NS_ERROR_UNEXPECTED;
    if (mState < HAVE_ARGS)
        return NS_ERROR_UNEXPECTED;

    if (!mTearOff) {
        mTearOff = mWrapper->FindTearOff(mInterface, false, &rv);
        if (!mTearOff || mTearOff->GetInterface() != mInterface) {
            mTearOff = nullptr;
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
    mXPCContext = nullptr;
    mState = SYSTEM_SHUTDOWN;
    if (mPrevCallContext)
        mPrevCallContext->SystemIsBeingShutDown();
}

XPCCallContext::~XPCCallContext()
{
    if (mXPCContext) {
        mXPCContext->SetCallingLangType(mPrevCallerLanguage);

        DebugOnly<XPCCallContext*> old = XPCJSRuntime::Get()->SetCallContext(mPrevCallContext);
        MOZ_ASSERT(old == this, "bad pop from per thread data");
    }
}

/* readonly attribute nsISupports Callee; */
NS_IMETHODIMP
XPCCallContext::GetCallee(nsISupports * *aCallee)
{
    nsCOMPtr<nsISupports> rval = mWrapper ? mWrapper->GetIdentityObject() : nullptr;
    rval.forget(aCallee);
    return NS_OK;
}

/* readonly attribute uint16_t CalleeMethodIndex; */
NS_IMETHODIMP
XPCCallContext::GetCalleeMethodIndex(uint16_t *aCalleeMethodIndex)
{
    *aCalleeMethodIndex = mMethodIndex;
    return NS_OK;
}

/* readonly attribute XPCNativeInterface CalleeInterface; */
NS_IMETHODIMP
XPCCallContext::GetCalleeInterface(nsIInterfaceInfo * *aCalleeInterface)
{
    nsCOMPtr<nsIInterfaceInfo> rval = mInterface->GetInterfaceInfo();
    rval.forget(aCalleeInterface);
    return NS_OK;
}

/* readonly attribute nsIClassInfo CalleeClassInfo; */
NS_IMETHODIMP
XPCCallContext::GetCalleeClassInfo(nsIClassInfo * *aCalleeClassInfo)
{
    nsCOMPtr<nsIClassInfo> rval = mWrapper ? mWrapper->GetClassInfo() : nullptr;
    rval.forget(aCalleeClassInfo);
    return NS_OK;
}

/* readonly attribute JSContextPtr JSContext; */
NS_IMETHODIMP
XPCCallContext::GetJSContext(JSContext * *aJSContext)
{
    JS_AbortIfWrongThread(JS_GetRuntime(mJSContext));
    *aJSContext = mJSContext;
    return NS_OK;
}

/* readonly attribute uint32_t Argc; */
NS_IMETHODIMP
XPCCallContext::GetArgc(uint32_t *aArgc)
{
    *aArgc = (uint32_t) mArgc;
    return NS_OK;
}

/* readonly attribute JSValPtr ArgvPtr; */
NS_IMETHODIMP
XPCCallContext::GetArgvPtr(jsval * *aArgvPtr)
{
    *aArgvPtr = mArgv;
    return NS_OK;
}

NS_IMETHODIMP
XPCCallContext::GetPreviousCallContext(nsAXPCNativeCallContext **aResult)
{
  NS_ENSURE_ARG_POINTER(aResult);
  *aResult = GetPrevCallContext();
  return NS_OK;
}

NS_IMETHODIMP
XPCCallContext::GetLanguage(uint16_t *aResult)
{
  NS_ENSURE_ARG_POINTER(aResult);
  *aResult = GetCallerLanguage();
  return NS_OK;
}
