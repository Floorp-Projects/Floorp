/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/* Call context. */

#include "mozilla/Util.h"
#include "AccessCheck.h"

#include "xpcprivate.h"

using namespace mozilla;
using namespace xpc;
using namespace JS;

XPCCallContext::XPCCallContext(XPCContext::LangType callerLanguage,
                               JSContext* cx       /* = GetDefaultJSContext() */,
                               HandleObject obj    /* = nullptr               */,
                               HandleObject funobj /* = nullptr               */,
                               HandleId name       /* = JSID_VOID             */,
                               unsigned argc       /* = NO_ARGS               */,
                               jsval *argv         /* = nullptr               */,
                               jsval *rval         /* = nullptr               */)
    :   mState(INIT_FAILED),
        mXPC(nsXPConnect::GetXPConnect()),
        mXPCContext(nullptr),
        mJSContext(cx),
        mContextPopRequired(false),
        mDestroyJSContextInDestructor(false),
        mCallerLanguage(callerLanguage),
        mScopeForNewJSObjects(cx),
        mFlattenedJSObject(cx),
        mWrapper(nullptr),
        mTearOff(nullptr),
        mName(cx)
{
    MOZ_ASSERT(cx);
    Init(callerLanguage, callerLanguage == NATIVE_CALLER, obj, funobj,
         INIT_SHOULD_LOOKUP_WRAPPER, name, argc, argv, rval);
}

XPCCallContext::XPCCallContext(XPCContext::LangType callerLanguage,
                               JSContext* cx,
                               JSBool callBeginRequest,
                               HandleObject obj,
                               HandleObject flattenedJSObject,
                               XPCWrappedNative* wrapper,
                               XPCWrappedNativeTearOff* tearOff)
    :   mState(INIT_FAILED),
        mXPC(nsXPConnect::GetXPConnect()),
        mXPCContext(nullptr),
        mJSContext(cx),
        mContextPopRequired(false),
        mDestroyJSContextInDestructor(false),
        mCallerLanguage(callerLanguage),
        mScopeForNewJSObjects(cx),
        mFlattenedJSObject(cx, flattenedJSObject),
        mWrapper(wrapper),
        mTearOff(tearOff),
        mName(cx)
{
    MOZ_ASSERT(cx);
    Init(callerLanguage, callBeginRequest, obj, NullPtr(),
         WRAPPER_PASSED_TO_CONSTRUCTOR, JSID_VOIDHANDLE, NO_ARGS,
         nullptr, nullptr);
}

#define IS_TEAROFF_CLASS(clazz) ((clazz) == &XPC_WN_Tearoff_JSClass)


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
XPCCallContext::Init(XPCContext::LangType callerLanguage,
                     JSBool callBeginRequest,
                     HandleObject obj,
                     HandleObject funobj,
                     WrapperInitOptions wrapperInitOptions,
                     HandleId name,
                     unsigned argc,
                     jsval *argv,
                     jsval *rval)
{
    NS_ASSERTION(mJSContext, "No JSContext supplied to XPCCallContext");

    if (!mXPC)
        return;

    XPCJSContextStack* stack = XPCJSRuntime::Get()->GetJSContextStack();
    JSContext *topJSContext = stack->Peek();

    if (topJSContext != mJSContext) {
        if (!stack->Push(mJSContext)) {
            NS_ERROR("bad!");
            return;
        }
        mContextPopRequired = true;
    }

    // Get into the request as early as we can to avoid problems with scanning
    // callcontexts on other threads from within the gc callbacks.

    NS_ASSERTION(!callBeginRequest || mCallerLanguage == NATIVE_CALLER,
                 "Don't call JS_BeginRequest unless the caller is native.");
    if (callBeginRequest)
        JS_BeginRequest(mJSContext);

    mXPCContext = XPCContext::GetXPCContext(mJSContext);
    mPrevCallerLanguage = mXPCContext->SetCallingLangType(mCallerLanguage);

    // hook into call context chain.
    mPrevCallContext = XPCJSRuntime::Get()->SetCallContext(this);

    // We only need to addref xpconnect once so only do it if this is the first
    // context in the chain.
    if (!mPrevCallContext)
        NS_ADDREF(mXPC);

    mState = HAVE_CONTEXT;

    if (!obj)
        return;

    mScopeForNewJSObjects = obj;

    mState = HAVE_SCOPE;

    mMethodIndex = 0xDEAD;

    mState = HAVE_OBJECT;

    mTearOff = nullptr;
    if (wrapperInitOptions == INIT_SHOULD_LOOKUP_WRAPPER) {
        // If the object is a security wrapper, GetWrappedNativeOfJSObject can't
        // handle it. Do special handling here to make cross-origin Xrays work.
        JSObject *unwrapped = js::CheckedUnwrap(obj, /* stopAtOuter = */ false);
        if (!unwrapped) {
            mWrapper = UnwrapThisIfAllowed(obj, funobj, argc);
            if (!mWrapper) {
                JS_ReportError(mJSContext, "Permission denied to call method on |this|");
                mState = INIT_FAILED;
                return;
            }
        } else {
            js::Class *clasp = js::GetObjectClass(unwrapped);
            if (IS_WRAPPER_CLASS(clasp)) {
                if (IS_SLIM_WRAPPER_OBJECT(unwrapped))
                    mFlattenedJSObject = unwrapped;
                else
                    mWrapper = XPCWrappedNative::Get(unwrapped);
            } else if (IS_TEAROFF_CLASS(clasp)) {
                mTearOff = (XPCWrappedNativeTearOff*)js::GetObjectPrivate(unwrapped);
                mWrapper = XPCWrappedNative::Get(js::GetObjectParent(unwrapped));
            }
        }
        if (mWrapper) {
            mFlattenedJSObject = mWrapper->GetFlatJSObject();

            if (mTearOff)
                mScriptableInfo = nullptr;
            else
                mScriptableInfo = mWrapper->GetScriptableInfo();
        } else {
            NS_ABORT_IF_FALSE(!mFlattenedJSObject || IS_SLIM_WRAPPER(mFlattenedJSObject),
                              "should have a slim wrapper");
        }
    }

    if (!JSID_IS_VOID(name))
        SetName(name);

    if (argc != NO_ARGS)
        SetArgsAndResultPtr(argc, argv, rval);

    CHECK_STATE(HAVE_OBJECT);
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
                            JSBool isSetter)
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
        mTearOff = mWrapper->FindTearOff(*this, mInterface, false, &rv);
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
    // do cleanup...

    bool shouldReleaseXPC = false;

    if (mXPCContext) {
        mXPCContext->SetCallingLangType(mPrevCallerLanguage);

        DebugOnly<XPCCallContext*> old = XPCJSRuntime::Get()->SetCallContext(mPrevCallContext);
        NS_ASSERTION(old == this, "bad pop from per thread data");

        shouldReleaseXPC = mPrevCallContext == nullptr;
    }

    // NB: Needs to happen before the context stack pop.
    if (mJSContext && mCallerLanguage == NATIVE_CALLER)
        JS_EndRequest(mJSContext);

    if (mContextPopRequired) {
        XPCJSContextStack* stack = XPCJSRuntime::Get()->GetJSContextStack();
        NS_ASSERTION(stack, "bad!");
        if (stack) {
            DebugOnly<JSContext*> poppedCX = stack->Pop();
            NS_ASSERTION(poppedCX == mJSContext, "bad pop");
        }
    }

    if (mJSContext) {
        if (mDestroyJSContextInDestructor) {
#ifdef DEBUG_xpc_hacker
            printf("!xpc - doing deferred destruction of JSContext @ %p\n",
                   mJSContext);
#endif
            NS_ASSERTION(!XPCJSRuntime::Get()->GetJSContextStack()->
                         DEBUG_StackHasJSContext(mJSContext),
                         "JSContext still in threadjscontextstack!");

            JS_DestroyContext(mJSContext);
        }
    }

    if (shouldReleaseXPC && mXPC)
        NS_RELEASE(mXPC);
}

/* readonly attribute nsISupports Callee; */
NS_IMETHODIMP
XPCCallContext::GetCallee(nsISupports * *aCallee)
{
    nsISupports* temp = mWrapper ? mWrapper->GetIdentityObject() : nullptr;
    NS_IF_ADDREF(temp);
    *aCallee = temp;
    return NS_OK;
}

/* readonly attribute uint16_t CalleeMethodIndex; */
NS_IMETHODIMP
XPCCallContext::GetCalleeMethodIndex(uint16_t *aCalleeMethodIndex)
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
    nsIClassInfo* temp = mWrapper ? mWrapper->GetClassInfo() : nullptr;
    NS_IF_ADDREF(temp);
    *aCalleeClassInfo = temp;
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

#ifdef DEBUG
// static
void
XPCLazyCallContext::AssertContextIsTopOfStack(JSContext* cx)
{
    XPCJSContextStack* stack = XPCJSRuntime::Get()->GetJSContextStack();

    JSContext *topJSContext = stack->Peek();
    NS_ASSERTION(cx == topJSContext, "wrong context on XPCJSContextStack!");
}
#endif

XPCWrappedNative*
XPCCallContext::UnwrapThisIfAllowed(HandleObject obj, HandleObject fun, unsigned argc)
{
    // We should only get here for objects that aren't safe to unwrap.
    MOZ_ASSERT(!js::CheckedUnwrap(obj));
    MOZ_ASSERT(js::IsObjectInContextCompartment(obj, mJSContext));

    // We can't do anything here without a function.
    if (!fun)
        return nullptr;

    // Determine if we're allowed to unwrap the security wrapper to invoke the
    // method.
    //
    // We have the Interface and Member that this corresponds to, but
    // unfortunately our access checks are based on the object class name and
    // property name. So we cheat a little bit here - we verify that the object
    // does indeed implement the method's Interface, and then just check that we
    // can successfully access property with method's name from the object.

    // First, get the XPCWN out of the underlying object. We should have a wrapper
    // here, potentially an outer window proxy, and then an XPCWN.
    MOZ_ASSERT(js::IsWrapper(obj));
    RootedObject unwrapped(mJSContext, js::UncheckedUnwrap(obj, /* stopAtOuter = */ false));
    MOZ_ASSERT(unwrapped == JS_ObjectToInnerObject(mJSContext, js::Wrapper::wrappedObject(obj)));

    // Make sure we have an XPCWN, and grab it.
    MOZ_ASSERT(!IS_SLIM_WRAPPER(unwrapped), "security wrapping morphs slim wrappers");
    if (!IS_WRAPPER_CLASS(js::GetObjectClass(unwrapped)))
        return nullptr;
    XPCWrappedNative *wn = (XPCWrappedNative*)js::GetObjectPrivate(unwrapped);

    // Next, get the call info off the function object.
    XPCNativeInterface *interface;
    XPCNativeMember *member;
    XPCNativeMember::GetCallInfo(fun, &interface, &member);

    // To be extra safe, make sure that the underlying native implements the
    // interface before unwrapping. Even if we didn't check this, we'd still
    // theoretically fail during tearoff lookup for mismatched methods.
    if (!wn->HasInterfaceNoQI(*interface->GetIID()))
        return nullptr;

    // See if the access is permitted.
    //
    // NB: This calculation of SET vs GET is a bit wonky, but that's what
    // XPC_WN_GetterSetter does.
    bool set = argc && argc != NO_ARGS && member->IsWritableAttribute();
    js::Wrapper::Action act = set ? js::Wrapper::SET : js::Wrapper::GET;
    js::Wrapper *handler = js::Wrapper::wrapperHandler(obj);
    bool ignored;
    JS::Rooted<jsid> id(mJSContext, member->GetName());
    if (!handler->enter(mJSContext, obj, id, act, &ignored))
        return nullptr;

    // Ok, this call is safe.
    return wn;
}

