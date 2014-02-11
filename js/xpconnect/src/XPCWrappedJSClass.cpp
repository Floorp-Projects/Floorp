/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: set ts=8 sw=4 et tw=78:
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/* Sharable code and data for wrapper around JSObjects. */

#include "xpcprivate.h"
#include "jsprf.h"
#include "nsArrayEnumerator.h"
#include "nsContentUtils.h"
#include "nsWrapperCache.h"
#include "AccessCheck.h"
#include "nsJSUtils.h"
#include "mozilla/Attributes.h"
#include "mozilla/dom/BindingUtils.h"
#include "mozilla/dom/DOMException.h"
#include "mozilla/dom/DOMExceptionBinding.h"

#include "jsapi.h"
#include "jsfriendapi.h"

using namespace xpc;
using namespace JS;
using namespace mozilla;

NS_IMPL_ISUPPORTS1(nsXPCWrappedJSClass, nsIXPCWrappedJSClass)

// the value of this variable is never used - we use its address as a sentinel
static uint32_t zero_methods_descriptor;

bool AutoScriptEvaluate::StartEvaluating(HandleObject scope, JSErrorReporter errorReporter)
{
    NS_PRECONDITION(!mEvaluated, "AutoScriptEvaluate::Evaluate should only be called once");

    if (!mJSContext)
        return true;

    mEvaluated = true;
    if (!JS_GetErrorReporter(mJSContext)) {
        JS_SetErrorReporter(mJSContext, errorReporter);
        mErrorReporterSet = true;
    }

    JS_BeginRequest(mJSContext);
    mAutoCompartment.construct(mJSContext, scope);

    // Saving the exception state keeps us from interfering with another script
    // that may also be running on this context.  This occurred first with the
    // js debugger, as described in
    // http://bugzilla.mozilla.org/show_bug.cgi?id=88130 but presumably could
    // show up in any situation where a script calls into a wrapped js component
    // on the same context, while the context has a nonzero exception state.
    // Because JS_SaveExceptionState/JS_RestoreExceptionState use malloc
    // and addroot, we avoid them if possible by returning null (as opposed to
    // a JSExceptionState with no information) when there is no pending
    // exception.
    if (JS_IsExceptionPending(mJSContext)) {
        mState = JS_SaveExceptionState(mJSContext);
        JS_ClearPendingException(mJSContext);
    }

    return true;
}

AutoScriptEvaluate::~AutoScriptEvaluate()
{
    if (!mJSContext || !mEvaluated)
        return;
    if (mState)
        JS_RestoreExceptionState(mJSContext, mState);
    else
        JS_ClearPendingException(mJSContext);

    JS_EndRequest(mJSContext);

    if (mErrorReporterSet)
        JS_SetErrorReporter(mJSContext, nullptr);
}

// It turns out that some errors may be not worth reporting. So, this
// function is factored out to manage that.
bool xpc_IsReportableErrorCode(nsresult code)
{
    if (NS_SUCCEEDED(code))
        return false;

    switch (code) {
        // Error codes that we don't want to report as errors...
        // These generally indicate bad interface design AFAIC.
        case NS_ERROR_FACTORY_REGISTER_AGAIN:
        case NS_BASE_STREAM_WOULD_BLOCK:
            return false;
        default:
            return true;
    }
}

// static
nsresult
nsXPCWrappedJSClass::GetNewOrUsed(JSContext* cx, REFNSIID aIID,
                                  nsXPCWrappedJSClass** resultClasp)
{
    XPCJSRuntime* rt = nsXPConnect::GetRuntimeInstance();
    IID2WrappedJSClassMap* map = rt->GetWrappedJSClassMap();
    nsRefPtr<nsXPCWrappedJSClass> clasp = map->Find(aIID);

    if (!clasp) {
        nsCOMPtr<nsIInterfaceInfo> info;
        nsXPConnect::XPConnect()->GetInfoForIID(&aIID, getter_AddRefs(info));
        if (info) {
            bool canScript, isBuiltin;
            if (NS_SUCCEEDED(info->IsScriptable(&canScript)) && canScript &&
                NS_SUCCEEDED(info->IsBuiltinClass(&isBuiltin)) && !isBuiltin &&
                nsXPConnect::IsISupportsDescendant(info))
            {
                clasp = new nsXPCWrappedJSClass(cx, aIID, info);
                if (!clasp->mDescriptors)
                    clasp = nullptr;
            }
        }
    }
    clasp.forget(resultClasp);
    return NS_OK;
}

nsXPCWrappedJSClass::nsXPCWrappedJSClass(JSContext* cx, REFNSIID aIID,
                                         nsIInterfaceInfo* aInfo)
    : mRuntime(nsXPConnect::GetRuntimeInstance()),
      mInfo(aInfo),
      mName(nullptr),
      mIID(aIID),
      mDescriptors(nullptr)
{
    mRuntime->GetWrappedJSClassMap()->Add(this);

    uint16_t methodCount;
    if (NS_SUCCEEDED(mInfo->GetMethodCount(&methodCount))) {
        if (methodCount) {
            int wordCount = (methodCount/32)+1;
            if (nullptr != (mDescriptors = new uint32_t[wordCount])) {
                int i;
                // init flags to 0;
                for (i = wordCount-1; i >= 0; i--)
                    mDescriptors[i] = 0;

                for (i = 0; i < methodCount; i++) {
                    const nsXPTMethodInfo* info;
                    if (NS_SUCCEEDED(mInfo->GetMethodInfo(i, &info)))
                        SetReflectable(i, XPCConvert::IsMethodReflectable(*info));
                    else {
                        delete [] mDescriptors;
                        mDescriptors = nullptr;
                        break;
                    }
                }
            }
        } else {
            mDescriptors = &zero_methods_descriptor;
        }
    }
}

nsXPCWrappedJSClass::~nsXPCWrappedJSClass()
{
    if (mDescriptors && mDescriptors != &zero_methods_descriptor)
        delete [] mDescriptors;
    if (mRuntime)
        mRuntime->GetWrappedJSClassMap()->Remove(this);

    if (mName)
        nsMemory::Free(mName);
}

JSObject*
nsXPCWrappedJSClass::CallQueryInterfaceOnJSObject(JSContext* cx,
                                                  JSObject* jsobjArg,
                                                  REFNSIID aIID)
{
    RootedObject jsobj(cx, jsobjArg);
    JSObject* id;
    RootedValue retval(cx);
    RootedObject retObj(cx);
    bool success = false;
    RootedValue fun(cx);

    // Don't call the actual function on a content object. We'll determine
    // whether or not a content object is capable of implementing the
    // interface (i.e. whether the interface is scriptable) and most content
    // objects don't have QI implementations anyway. Also see bug 503926.
    if (!xpc::AccessCheck::isChrome(js::GetObjectCompartment(jsobj))) {
        return nullptr;
    }

    // OK, it looks like we'll be calling into JS code.
    AutoScriptEvaluate scriptEval(cx);

    // XXX we should install an error reporter that will send reports to
    // the JS error console service.
    if (!scriptEval.StartEvaluating(jsobj))
        return nullptr;

    // check upfront for the existence of the function property
    HandleId funid = mRuntime->GetStringID(XPCJSRuntime::IDX_QUERY_INTERFACE);
    if (!JS_GetPropertyById(cx, jsobj, funid, &fun) || JSVAL_IS_PRIMITIVE(fun))
        return nullptr;

    // Ensure that we are asking for a scriptable interface.
    // NB:  It's important for security that this check is here rather
    // than later, since it prevents untrusted objects from implementing
    // some interfaces in JS and aggregating a trusted object to
    // implement intentionally (for security) unscriptable interfaces.
    // We so often ask for nsISupports that we can short-circuit the test...
    if (!aIID.Equals(NS_GET_IID(nsISupports))) {
        nsCOMPtr<nsIInterfaceInfo> info;
        nsXPConnect::XPConnect()->GetInfoForIID(&aIID, getter_AddRefs(info));
        if (!info)
            return nullptr;
        bool canScript, isBuiltin;
        if (NS_FAILED(info->IsScriptable(&canScript)) || !canScript ||
            NS_FAILED(info->IsBuiltinClass(&isBuiltin)) || isBuiltin)
            return nullptr;
    }

    id = xpc_NewIDObject(cx, jsobj, aIID);
    if (id) {
        // Throwing NS_NOINTERFACE is the prescribed way to fail QI from JS. It
        // is not an exception that is ever worth reporting, but we don't want
        // to eat all exceptions either.

        {
            AutoSaveContextOptions asco(cx);
            ContextOptionsRef(cx).setDontReportUncaught(true);
            RootedValue arg(cx, JS::ObjectValue(*id));
            success = JS_CallFunctionValue(cx, jsobj, fun, arg, retval.address());
        }

        if (!success && JS_IsExceptionPending(cx)) {
            RootedValue jsexception(cx, NullValue());

            if (JS_GetPendingException(cx, &jsexception)) {
                nsresult rv;
                if (jsexception.isObject()) {
                    // XPConnect may have constructed an object to represent a
                    // C++ QI failure. See if that is the case.
                    using namespace mozilla::dom;
                    Exception *e = nullptr;
                    UNWRAP_OBJECT(Exception, &jsexception.toObject(), e);

                    if (e &&
                        NS_SUCCEEDED(e->GetResult(&rv)) &&
                        rv == NS_NOINTERFACE) {
                        JS_ClearPendingException(cx);
                    }
                } else if (JSVAL_IS_NUMBER(jsexception)) {
                    // JS often throws an nsresult.
                    if (JSVAL_IS_DOUBLE(jsexception))
                        // Visual Studio 9 doesn't allow casting directly from
                        // a double to an enumeration type, contrary to
                        // 5.2.9(10) of C++11, so add an intermediate cast.
                        rv = (nsresult)(uint32_t)(JSVAL_TO_DOUBLE(jsexception));
                    else
                        rv = (nsresult)(JSVAL_TO_INT(jsexception));

                    if (rv == NS_NOINTERFACE)
                        JS_ClearPendingException(cx);
                }
            }

            // Don't report if reporting was disabled by someone else.
            if (!ContextOptionsRef(cx).dontReportUncaught())
                JS_ReportPendingException(cx);
        } else if (!success) {
            NS_WARNING("QI hook ran OOMed - this is probably a bug!");
        }
    }

    if (success)
        success = JS_ValueToObject(cx, retval, &retObj);

    return success ? retObj.get() : nullptr;
}

/***************************************************************************/

static bool
GetNamedPropertyAsVariantRaw(XPCCallContext& ccx,
                             HandleObject aJSObj,
                             HandleId aName,
                             nsIVariant** aResult,
                             nsresult* pErr)
{
    nsXPTType type = nsXPTType((uint8_t)TD_INTERFACE_TYPE);
    RootedValue val(ccx);

    return JS_GetPropertyById(ccx, aJSObj, aName, &val) &&
           // Note that this always takes the T_INTERFACE path through
           // JSData2Native, so the value passed for useAllocator
           // doesn't really matter. We pass true for consistency.
           XPCConvert::JSData2Native(aResult, val, type, true,
                                     &NS_GET_IID(nsIVariant), pErr);
}

// static
nsresult
nsXPCWrappedJSClass::GetNamedPropertyAsVariant(XPCCallContext& ccx,
                                               JSObject* aJSObjArg,
                                               const nsAString& aName,
                                               nsIVariant** aResult)
{
    JSContext* cx = ccx.GetJSContext();
    RootedObject aJSObj(cx, aJSObjArg);

    AutoScriptEvaluate scriptEval(cx);
    if (!scriptEval.StartEvaluating(aJSObj))
        return NS_ERROR_FAILURE;

    // Wrap the string in a jsval after the AutoScriptEvaluate, so that the
    // resulting value ends up in the correct compartment.
    nsStringBuffer* buf;
    RootedValue value(cx);
    if (!XPCStringConvert::ReadableToJSVal(ccx, aName, &buf, &value))
        return NS_ERROR_OUT_OF_MEMORY;
    if (buf)
        buf->AddRef();

    RootedId id(cx);
    nsresult rv = NS_OK;
    if (!JS_ValueToId(cx, value, &id) ||
        !GetNamedPropertyAsVariantRaw(ccx, aJSObj, id, aResult, &rv)) {
        if (NS_FAILED(rv))
            return rv;
        return NS_ERROR_FAILURE;
    }
    return NS_OK;
}

/***************************************************************************/

// static
nsresult
nsXPCWrappedJSClass::BuildPropertyEnumerator(XPCCallContext& ccx,
                                             JSObject* aJSObjArg,
                                             nsISimpleEnumerator** aEnumerate)
{
    JSContext* cx = ccx.GetJSContext();
    RootedObject aJSObj(cx, aJSObjArg);

    AutoScriptEvaluate scriptEval(cx);
    if (!scriptEval.StartEvaluating(aJSObj))
        return NS_ERROR_FAILURE;

    AutoIdArray idArray(cx, JS_Enumerate(cx, aJSObj));
    if (!idArray)
        return NS_ERROR_FAILURE;

    nsCOMArray<nsIProperty> propertyArray(idArray.length());
    RootedId idName(cx);
    for (size_t i = 0; i < idArray.length(); i++) {
        idName = idArray[i];

        nsCOMPtr<nsIVariant> value;
        nsresult rv;
        if (!GetNamedPropertyAsVariantRaw(ccx, aJSObj, idName,
                                          getter_AddRefs(value), &rv)) {
            if (NS_FAILED(rv))
                return rv;
            return NS_ERROR_FAILURE;
        }

        RootedValue jsvalName(cx);
        if (!JS_IdToValue(cx, idName, &jsvalName))
            return NS_ERROR_FAILURE;

        JSString* name = ToString(cx, jsvalName);
        if (!name)
            return NS_ERROR_FAILURE;

        size_t length;
        const jschar *chars = JS_GetStringCharsAndLength(cx, name, &length);
        if (!chars)
            return NS_ERROR_FAILURE;

        nsCOMPtr<nsIProperty> property =
            new xpcProperty(chars, (uint32_t) length, value);

        if (!propertyArray.AppendObject(property))
            return NS_ERROR_FAILURE;
    }

    return NS_NewArrayEnumerator(aEnumerate, propertyArray);
}

/***************************************************************************/

NS_IMPL_ISUPPORTS1(xpcProperty, nsIProperty)

xpcProperty::xpcProperty(const char16_t* aName, uint32_t aNameLen,
                         nsIVariant* aValue)
    : mName(aName, aNameLen), mValue(aValue)
{
}

/* readonly attribute AString name; */
NS_IMETHODIMP xpcProperty::GetName(nsAString & aName)
{
    aName.Assign(mName);
    return NS_OK;
}

/* readonly attribute nsIVariant value; */
NS_IMETHODIMP xpcProperty::GetValue(nsIVariant * *aValue)
{
    nsCOMPtr<nsIVariant> rval = mValue;
    rval.forget(aValue);
    return NS_OK;
}

/***************************************************************************/
// This 'WrappedJSIdentity' class and singleton allow us to figure out if
// any given nsISupports* is implemented by a WrappedJS object. This is done
// using a QueryInterface call on the interface pointer with our ID. If
// that call returns NS_OK and the pointer is to our singleton, then the
// interface must be implemented by a WrappedJS object. NOTE: the
// 'WrappedJSIdentity' object is not a real XPCOM object and should not be
// used for anything else (hence it is declared in this implementation file).

// {5C5C3BB0-A9BA-11d2-BA64-00805F8A5DD7}
#define NS_IXPCONNECT_WRAPPED_JS_IDENTITY_CLASS_IID                           \
{ 0x5c5c3bb0, 0xa9ba, 0x11d2,                                                 \
  { 0xba, 0x64, 0x0, 0x80, 0x5f, 0x8a, 0x5d, 0xd7 } }

class WrappedJSIdentity
{
    // no instance methods...
public:
    NS_DECLARE_STATIC_IID_ACCESSOR(NS_IXPCONNECT_WRAPPED_JS_IDENTITY_CLASS_IID)

    static void* GetSingleton()
    {
        static WrappedJSIdentity* singleton = nullptr;
        if (!singleton)
            singleton = new WrappedJSIdentity();
        return (void*) singleton;
    }
};

NS_DEFINE_STATIC_IID_ACCESSOR(WrappedJSIdentity,
                              NS_IXPCONNECT_WRAPPED_JS_IDENTITY_CLASS_IID)

/***************************************************************************/

// static
bool
nsXPCWrappedJSClass::IsWrappedJS(nsISupports* aPtr)
{
    void* result;
    NS_PRECONDITION(aPtr, "null pointer");
    return aPtr &&
           NS_OK == aPtr->QueryInterface(NS_GET_IID(WrappedJSIdentity), &result) &&
           result == WrappedJSIdentity::GetSingleton();
}

// NB: This will return the top JSContext on the JSContext stack if there is one,
// before attempting to get the context from the wrapped JS object.
static JSContext *
GetContextFromObjectOrDefault(nsXPCWrappedJS* wrapper)
{
    // First, try the cx stack.
    XPCJSContextStack* stack = XPCJSRuntime::Get()->GetJSContextStack();
    if (stack->Peek())
        return stack->Peek();

    // If the cx stack is empty, try the wrapper's JSObject.
    JSCompartment *c = js::GetObjectCompartment(wrapper->GetJSObject());
    XPCContext *xpcc = EnsureCompartmentPrivate(c)->scope->GetContext();
    if (xpcc) {
        JSContext *cx = xpcc->GetJSContext();
        JS_AbortIfWrongThread(JS_GetRuntime(cx));
        return cx;
    }

    // Fall back to the safe JSContext.
    return stack->GetSafeJSContext();
}

NS_IMETHODIMP
nsXPCWrappedJSClass::DelegatedQueryInterface(nsXPCWrappedJS* self,
                                             REFNSIID aIID,
                                             void** aInstancePtr)
{
    if (aIID.Equals(NS_GET_IID(nsIXPConnectJSObjectHolder))) {
        NS_ADDREF(self);
        *aInstancePtr = (void*) static_cast<nsIXPConnectJSObjectHolder*>(self);
        return NS_OK;
    }

    // Objects internal to xpconnect are the only objects that even know *how*
    // to ask for this iid. And none of them bother refcounting the thing.
    if (aIID.Equals(NS_GET_IID(WrappedJSIdentity))) {
        // asking to find out if this is a wrapper object
        *aInstancePtr = WrappedJSIdentity::GetSingleton();
        return NS_OK;
    }

    if (aIID.Equals(NS_GET_IID(nsIPropertyBag))) {
        // We only want to expose one implementation from our aggregate.
        nsXPCWrappedJS* root = self->GetRootWrapper();

        if (!root->IsValid()) {
            *aInstancePtr = nullptr;
            return NS_NOINTERFACE;
        }

        NS_ADDREF(root);
        *aInstancePtr = (void*) static_cast<nsIPropertyBag*>(root);
        return NS_OK;
    }

    // We can't have a cached wrapper.
    if (aIID.Equals(NS_GET_IID(nsWrapperCache))) {
        *aInstancePtr = nullptr;
        return NS_NOINTERFACE;
    }

    AutoPushJSContext context(GetContextFromObjectOrDefault(self));
    XPCCallContext ccx(NATIVE_CALLER, context);
    if (!ccx.IsValid()) {
        *aInstancePtr = nullptr;
        return NS_NOINTERFACE;
    }

    // We support nsISupportsWeakReference iff the root wrapped JSObject
    // claims to support it in its QueryInterface implementation.
    if (aIID.Equals(NS_GET_IID(nsISupportsWeakReference))) {
        // We only want to expose one implementation from our aggregate.
        nsXPCWrappedJS* root = self->GetRootWrapper();

        // Fail if JSObject doesn't claim support for nsISupportsWeakReference
        if (!root->IsValid() ||
            !CallQueryInterfaceOnJSObject(ccx, root->GetJSObject(), aIID)) {
            *aInstancePtr = nullptr;
            return NS_NOINTERFACE;
        }

        NS_ADDREF(root);
        *aInstancePtr = (void*) static_cast<nsISupportsWeakReference*>(root);
        return NS_OK;
    }

    // Checks for any existing wrapper explicitly constructed for this iid.
    // This includes the current 'self' wrapper. This also deals with the
    // nsISupports case (for which it returns mRoot).
    // Also check if asking for an interface from which one of our wrappers inherits.
    if (nsXPCWrappedJS* sibling = self->FindOrFindInherited(aIID)) {
        NS_ADDREF(sibling);
        *aInstancePtr = sibling->GetXPTCStub();
        return NS_OK;
    }

    // else we do the more expensive stuff...

    // check if the JSObject claims to implement this interface
    RootedObject jsobj(ccx, CallQueryInterfaceOnJSObject(ccx, self->GetJSObject(),
                                                         aIID));
    if (jsobj) {
        // We can't use XPConvert::JSObject2NativeInterface() here
        // since that can find a XPCWrappedNative directly on the
        // proto chain, and we don't want that here. We need to find
        // the actual JS object that claimed it supports the interface
        // we're looking for or we'll potentially bypass security
        // checks etc by calling directly through to a native found on
        // the prototype chain.
        //
        // Instead, simply do the nsXPCWrappedJS part of
        // XPConvert::JSObject2NativeInterface() here to make sure we
        // get a new (or used) nsXPCWrappedJS.
        nsXPCWrappedJS* wrapper;
        nsresult rv = nsXPCWrappedJS::GetNewOrUsed(jsobj, aIID, &wrapper);
        if (NS_SUCCEEDED(rv) && wrapper) {
            // We need to go through the QueryInterface logic to make
            // this return the right thing for the various 'special'
            // interfaces; e.g.  nsIPropertyBag.
            rv = wrapper->QueryInterface(aIID, aInstancePtr);
            NS_RELEASE(wrapper);
            return rv;
        }
    }

    // else...
    // no can do
    *aInstancePtr = nullptr;
    return NS_NOINTERFACE;
}

JSObject*
nsXPCWrappedJSClass::GetRootJSObject(JSContext* cx, JSObject* aJSObjArg)
{
    RootedObject aJSObj(cx, aJSObjArg);
    JSObject* result = CallQueryInterfaceOnJSObject(cx, aJSObj,
                                                    NS_GET_IID(nsISupports));
    if (!result)
        return aJSObj;
    JSObject* inner = js::UncheckedUnwrap(result);
    if (inner)
        return inner;
    return result;
}

void
xpcWrappedJSErrorReporter(JSContext *cx, const char *message,
                          JSErrorReport *report)
{
    if (report) {
        // If it is an exception report, then we can just deal with the
        // exception later (if not caught in the JS code).
        if (JSREPORT_IS_EXCEPTION(report->flags)) {
            // XXX We have a problem with error reports from uncaught exceptions.
            //
            // http://bugzilla.mozilla.org/show_bug.cgi?id=66453
            //
            // The issue is...
            //
            // We can't assume that the exception will *stay* uncaught. So, if
            // we build an nsIXPCException here and the underlying exception
            // really is caught before our script is done running then we blow
            // it by returning failure to our caller when the script didn't
            // really fail. However, This report contains error location info
            // that is no longer available after the script is done. So, if the
            // exception really is not caught (and is a non-engine exception)
            // then we've lost the oportunity to capture the script location
            // info that we *could* have captured here.
            //
            // This is expecially an issue with nested evaluations.
            //
            // Perhaps we could capture an expception here and store it as
            // 'provisional' and then later if there is a pending exception
            // when the script is done then we could maybe compare that in some
            // way with the 'provisional' one in which we captured location info.
            // We would not want to assume that the one discovered here is the
            // same one that is later detected. This could cause us to lie.
            //
            // The thing is. we do not currently store the right stuff to compare
            // these two nsIXPCExceptions (triggered by the same exception jsval
            // in the engine). Maybe we should store the jsval and compare that?
            // Maybe without even rooting it since we will not dereference it.
            // This is inexact, but maybe the right thing to do?
            //
            // if (report->errorNumber == JSMSG_UNCAUGHT_EXCEPTION)) ...
            //

            return;
        }

        if (JSREPORT_IS_WARNING(report->flags)) {
            // XXX printf the warning (#ifdef DEBUG only!).
            // XXX send the warning to the console service.
            return;
        }
    }

    XPCCallContext ccx(NATIVE_CALLER, cx);
    if (!ccx.IsValid())
        return;

    nsCOMPtr<nsIException> e;
    XPCConvert::JSErrorToXPCException(message, nullptr, nullptr, report,
                                      getter_AddRefs(e));
    if (e)
        ccx.GetXPCContext()->SetException(e);
}

bool
nsXPCWrappedJSClass::GetArraySizeFromParam(JSContext* cx,
                                           const XPTMethodDescriptor* method,
                                           const nsXPTParamInfo& param,
                                           uint16_t methodIndex,
                                           uint8_t paramIndex,
                                           nsXPTCMiniVariant* nativeParams,
                                           uint32_t* result)
{
    uint8_t argnum;
    nsresult rv;

    rv = mInfo->GetSizeIsArgNumberForParam(methodIndex, &param, 0, &argnum);
    if (NS_FAILED(rv))
        return false;

    const nsXPTParamInfo& arg_param = method->params[argnum];

    // This should be enforced by the xpidl compiler, but it's not.
    // See bug 695235.
    MOZ_ASSERT(arg_param.GetType().TagPart() == nsXPTType::T_U32,
               "size_is references parameter of invalid type.");

    if (arg_param.IsIndirect())
        *result = *(uint32_t*)nativeParams[argnum].val.p;
    else
        *result = nativeParams[argnum].val.u32;

    return true;
}

bool
nsXPCWrappedJSClass::GetInterfaceTypeFromParam(JSContext* cx,
                                               const XPTMethodDescriptor* method,
                                               const nsXPTParamInfo& param,
                                               uint16_t methodIndex,
                                               const nsXPTType& type,
                                               nsXPTCMiniVariant* nativeParams,
                                               nsID* result)
{
    uint8_t type_tag = type.TagPart();

    if (type_tag == nsXPTType::T_INTERFACE) {
        if (NS_SUCCEEDED(GetInterfaceInfo()->
                         GetIIDForParamNoAlloc(methodIndex, &param, result))) {
            return true;
        }
    } else if (type_tag == nsXPTType::T_INTERFACE_IS) {
        uint8_t argnum;
        nsresult rv;
        rv = mInfo->GetInterfaceIsArgNumberForParam(methodIndex,
                                                    &param, &argnum);
        if (NS_FAILED(rv))
            return false;

        const nsXPTParamInfo& arg_param = method->params[argnum];
        const nsXPTType& arg_type = arg_param.GetType();

        if (arg_type.TagPart() == nsXPTType::T_IID) {
            if (arg_param.IsIndirect()) {
                nsID** p = (nsID**) nativeParams[argnum].val.p;
                if (!p || !*p)
                    return false;
                *result = **p;
            } else {
                nsID* p = (nsID*) nativeParams[argnum].val.p;
                if (!p)
                    return false;
                *result = *p;
            }
            return true;
        }
    }
    return false;
}

void
nsXPCWrappedJSClass::CleanupPointerArray(const nsXPTType& datum_type,
                                         uint32_t array_count,
                                         void** arrayp)
{
    if (datum_type.IsInterfacePointer()) {
        nsISupports** pp = (nsISupports**) arrayp;
        for (uint32_t k = 0; k < array_count; k++) {
            nsISupports* p = pp[k];
            NS_IF_RELEASE(p);
        }
    } else {
        void** pp = (void**) arrayp;
        for (uint32_t k = 0; k < array_count; k++) {
            void* p = pp[k];
            if (p) nsMemory::Free(p);
        }
    }
}

void
nsXPCWrappedJSClass::CleanupPointerTypeObject(const nsXPTType& type,
                                              void** pp)
{
    MOZ_ASSERT(pp,"null pointer");
    if (type.IsInterfacePointer()) {
        nsISupports* p = *((nsISupports**)pp);
        if (p) p->Release();
    } else {
        void* p = *((void**)pp);
        if (p) nsMemory::Free(p);
    }
}

class AutoClearPendingException
{
public:
  AutoClearPendingException(JSContext *cx) : mCx(cx) { }
  ~AutoClearPendingException() { JS_ClearPendingException(mCx); }
private:
  JSContext* mCx;
};

nsresult
nsXPCWrappedJSClass::CheckForException(XPCCallContext & ccx,
                                       const char * aPropertyName,
                                       const char * anInterfaceName,
                                       bool aForceReport)
{
    XPCContext * xpcc = ccx.GetXPCContext();
    JSContext * cx = ccx.GetJSContext();
    nsCOMPtr<nsIException> xpc_exception;
    /* this one would be set by our error reporter */

    xpcc->GetException(getter_AddRefs(xpc_exception));
    if (xpc_exception)
        xpcc->SetException(nullptr);

    // get this right away in case we do something below to cause JS code
    // to run on this JSContext
    nsresult pending_result = xpcc->GetPendingResult();

    RootedValue js_exception(cx);
    bool is_js_exception = JS_GetPendingException(cx, &js_exception);

    /* JS might throw an expection whether the reporter was called or not */
    if (is_js_exception) {
        if (!xpc_exception)
            XPCConvert::JSValToXPCException(&js_exception, anInterfaceName,
                                            aPropertyName,
                                            getter_AddRefs(xpc_exception));

        /* cleanup and set failed even if we can't build an exception */
        if (!xpc_exception) {
            XPCJSRuntime::Get()->SetPendingException(nullptr); // XXX necessary?
        }
    }

    AutoClearPendingException acpe(cx);

    if (xpc_exception) {
        nsresult e_result;
        if (NS_SUCCEEDED(xpc_exception->GetResult(&e_result))) {
            // Figure out whether or not we should report this exception.
            bool reportable = xpc_IsReportableErrorCode(e_result);
            if (reportable) {
                // Always want to report forced exceptions and XPConnect's own
                // errors.
                reportable = aForceReport ||
                    NS_ERROR_GET_MODULE(e_result) == NS_ERROR_MODULE_XPCONNECT;

                // See if an environment variable was set or someone has told us
                // that a user pref was set indicating that we should report all
                // exceptions.
                if (!reportable)
                    reportable = nsXPConnect::ReportAllJSExceptions();

                // Finally, check to see if this is the last JS frame on the
                // stack. If so then we always want to report it.
                if (!reportable) {
                    RootedScript ignored(cx);
                    reportable = !JS_DescribeScriptedCaller(cx, &ignored, nullptr);
                }

                // Ugly special case for GetInterface. It's "special" in the
                // same way as QueryInterface in that a failure is not
                // exceptional and shouldn't be reported. We have to do this
                // check here instead of in xpcwrappedjs (like we do for QI) to
                // avoid adding extra code to all xpcwrappedjs objects.
                if (reportable && e_result == NS_ERROR_NO_INTERFACE &&
                    !strcmp(anInterfaceName, "nsIInterfaceRequestor") &&
                    !strcmp(aPropertyName, "getInterface")) {
                    reportable = false;
                }

                // More special case, see bug 877760.
                if (e_result == NS_ERROR_XPC_JSOBJECT_HAS_NO_FUNCTION_NAMED) {
                    reportable = false;
                }
            }

            // Try to use the error reporter set on the context to handle this
            // error if it came from a JS exception.
            if (reportable && is_js_exception &&
                JS_GetErrorReporter(cx) != xpcWrappedJSErrorReporter)
            {
                // If the error reporter ignores the error, it will call
                // xpc->MarkErrorUnreported().
                xpcc->ClearUnreportedError();
                reportable = !JS_ReportPendingException(cx);
                if (!xpcc->WasErrorReported())
                    reportable = true;
            }

            if (reportable) {
                if (nsContentUtils::DOMWindowDumpEnabled()) {
                    static const char line[] =
                        "************************************************************\n";
                    static const char preamble[] =
                        "* Call to xpconnect wrapped JSObject produced this error:  *\n";
                    static const char cant_get_text[] =
                        "FAILED TO GET TEXT FROM EXCEPTION\n";

                    fputs(line, stdout);
                    fputs(preamble, stdout);
                    nsCString text;
                    if (NS_SUCCEEDED(xpc_exception->ToString(text)) &&
                        !text.IsEmpty()) {
                        fputs(text.get(), stdout);
                        fputs("\n", stdout);
                    } else
                        fputs(cant_get_text, stdout);
                    fputs(line, stdout);
                }

                // Log the exception to the JS Console, so that users can do
                // something with it.
                nsCOMPtr<nsIConsoleService> consoleService
                    (do_GetService(XPC_CONSOLE_CONTRACTID));
                if (nullptr != consoleService) {
                    nsresult rv;
                    nsCOMPtr<nsIScriptError> scriptError;
                    nsCOMPtr<nsISupports> errorData;
                    rv = xpc_exception->GetData(getter_AddRefs(errorData));
                    if (NS_SUCCEEDED(rv))
                        scriptError = do_QueryInterface(errorData);

                    if (nullptr == scriptError) {
                        // No luck getting one from the exception, so
                        // try to cook one up.
                        scriptError = do_CreateInstance(XPC_SCRIPT_ERROR_CONTRACTID);
                        if (nullptr != scriptError) {
                            nsCString newMessage;
                            rv = xpc_exception->ToString(newMessage);
                            if (NS_SUCCEEDED(rv)) {
                                // try to get filename, lineno from the first
                                // stack frame location.
                                int32_t lineNumber = 0;
                                nsCString sourceName;

                                nsCOMPtr<nsIStackFrame> location;
                                xpc_exception->
                                    GetLocation(getter_AddRefs(location));
                                if (location) {
                                    // Get line number w/o checking; 0 is ok.
                                    location->GetLineNumber(&lineNumber);

                                    // get a filename.
                                    rv = location->GetFilename(sourceName);
                                }

                                rv = scriptError->InitWithWindowID(NS_ConvertUTF8toUTF16(newMessage),
                                                                   NS_ConvertUTF8toUTF16(sourceName),
                                                                   EmptyString(),
                                                                   lineNumber, 0, 0,
                                                                   "XPConnect JavaScript",
                                                                   nsJSUtils::GetCurrentlyRunningCodeInnerWindowID(cx));
                                if (NS_FAILED(rv))
                                    scriptError = nullptr;
                            }
                        }
                    }
                    if (nullptr != scriptError)
                        consoleService->LogMessage(scriptError);
                }
            }
            // Whether or not it passes the 'reportable' test, it might
            // still be an error and we have to do the right thing here...
            if (NS_FAILED(e_result)) {
                XPCJSRuntime::Get()->SetPendingException(xpc_exception);
                return e_result;
            }
        }
    } else {
        // see if JS code signaled failure result without throwing exception
        if (NS_FAILED(pending_result)) {
            return pending_result;
        }
    }
    return NS_ERROR_FAILURE;
}

NS_IMETHODIMP
nsXPCWrappedJSClass::CallMethod(nsXPCWrappedJS* wrapper, uint16_t methodIndex,
                                const XPTMethodDescriptor* info_,
                                nsXPTCMiniVariant* nativeParams)
{
    jsval* sp = nullptr;
    jsval* argv = nullptr;
    uint8_t i;
    nsresult retval = NS_ERROR_FAILURE;
    nsresult pending_result = NS_OK;
    bool success;
    bool readyToDoTheCall = false;
    nsID  param_iid;
    const nsXPTMethodInfo* info = static_cast<const nsXPTMethodInfo*>(info_);
    const char* name = info->name;
    bool foundDependentParam;

    // Make sure not to set the callee on ccx until after we've gone through
    // the whole nsIXPCFunctionThisTranslator bit.  That code uses ccx to
    // convert natives to JSObjects, but we do NOT plan to pass those JSObjects
    // to our real callee.
    AutoPushJSContext context(GetContextFromObjectOrDefault(wrapper));
    XPCCallContext ccx(NATIVE_CALLER, context);
    if (!ccx.IsValid())
        return retval;

    XPCContext *xpcc = ccx.GetXPCContext();
    JSContext *cx = ccx.GetJSContext();

    if (!cx || !xpcc || !IsReflectable(methodIndex))
        return NS_ERROR_FAILURE;

    // [implicit_jscontext] and [optional_argc] have a different calling
    // convention, which we don't support for JS-implemented components.
    if (info->WantsOptArgc() || info->WantsContext()) {
        const char *str = "IDL methods marked with [implicit_jscontext] "
                          "or [optional_argc] may not be implemented in JS";
        // Throw and warn for good measure.
        JS_ReportError(cx, str);
        NS_WARNING(str);
        return NS_ERROR_FAILURE;
    }

    RootedValue fval(cx);
    RootedObject obj(cx, wrapper->GetJSObject());
    RootedObject thisObj(cx, obj);

    JSAutoCompartment ac(cx, obj);

    AutoValueVector args(cx);
    AutoScriptEvaluate scriptEval(cx);

    // XXX ASSUMES that retval is last arg. The xpidl compiler ensures this.
    uint8_t paramCount = info->num_args;
    uint8_t argc = paramCount -
        (paramCount && XPT_PD_IS_RETVAL(info->params[paramCount-1].flags) ? 1 : 0);

    if (!scriptEval.StartEvaluating(obj, xpcWrappedJSErrorReporter))
        goto pre_call_clean_up;

    xpcc->SetPendingResult(pending_result);
    xpcc->SetException(nullptr);
    XPCJSRuntime::Get()->SetPendingException(nullptr);

    // We use js_Invoke so that the gcthings we use as args will be rooted by
    // the engine as we do conversions and prepare to do the function call.

    // setup stack

    // if this isn't a function call then we don't need to push extra stuff
    if (!(XPT_MD_IS_SETTER(info->flags) || XPT_MD_IS_GETTER(info->flags))) {
        // We get fval before allocating the stack to avoid gc badness that can
        // happen if the GetProperty call leaves our request and the gc runs
        // while the stack we allocate contains garbage.

        // If the interface is marked as a [function] then we will assume that
        // our JSObject is a function and not an object with a named method.

        bool isFunction;
        if (NS_FAILED(mInfo->IsFunction(&isFunction)))
            goto pre_call_clean_up;

        // In the xpidl [function] case we are making sure now that the
        // JSObject is callable. If it is *not* callable then we silently
        // fallback to looking up the named property...
        // (because jst says he thinks this fallback is 'The Right Thing'.)
        //
        // In the normal (non-function) case we just lookup the property by
        // name and as long as the object has such a named property we go ahead
        // and try to make the call. If it turns out the named property is not
        // a callable object then the JS engine will throw an error and we'll
        // pass this along to the caller as an exception/result code.

        fval = ObjectValue(*obj);
        if (isFunction &&
            JS_TypeOfValue(ccx, fval) == JSTYPE_FUNCTION) {

            // We may need to translate the 'this' for the function object.

            if (paramCount) {
                const nsXPTParamInfo& firstParam = info->params[0];
                if (firstParam.IsIn()) {
                    const nsXPTType& firstType = firstParam.GetType();

                    if (firstType.IsInterfacePointer()) {
                        nsIXPCFunctionThisTranslator* translator;

                        IID2ThisTranslatorMap* map =
                            mRuntime->GetThisTranslatorMap();

                        translator = map->Find(mIID);

                        if (translator) {
                            nsCOMPtr<nsISupports> newThis;
                            if (NS_FAILED(translator->
                                          TranslateThis((nsISupports*)nativeParams[0].val.p,
                                                        getter_AddRefs(newThis)))) {
                                goto pre_call_clean_up;
                            }
                            if (newThis) {
                                RootedValue v(cx);
                                xpcObjectHelper helper(newThis);
                                bool ok =
                                  XPCConvert::NativeInterface2JSObject(
                                      &v, nullptr, helper, nullptr,
                                      nullptr, false, nullptr);
                                if (!ok) {
                                    goto pre_call_clean_up;
                                }
                                thisObj = JSVAL_TO_OBJECT(v);
                                if (!JS_WrapObject(cx, &thisObj))
                                    goto pre_call_clean_up;
                            }
                        }
                    }
                }
            }
        } else {
            if (!JS_GetProperty(cx, obj, name, &fval))
                goto pre_call_clean_up;
            // XXX We really want to factor out the error reporting better and
            // specifically report the failure to find a function with this name.
            // This is what we do below if the property is found but is not a
            // function. We just need to factor better so we can get to that
            // reporting path from here.

            thisObj = obj;
        }
    }

    if (!args.resize(argc)) {
        retval = NS_ERROR_OUT_OF_MEMORY;
        goto pre_call_clean_up;
    }

    argv = args.begin();
    sp = argv;

    // build the args
    // NB: This assignment *looks* wrong because we haven't yet called our
    // function. However, we *have* already entered the compartmen that we're
    // about to call, and that's the global that we want here. In other words:
    // we're trusting the JS engine to come up with a good global to use for
    // our object (whatever it was).
    for (i = 0; i < argc; i++) {
        const nsXPTParamInfo& param = info->params[i];
        const nsXPTType& type = param.GetType();
        nsXPTType datum_type;
        uint32_t array_count;
        bool isArray = type.IsArray();
        RootedValue val(cx, NullValue());
        bool isSizedString = isArray ?
                false :
                type.TagPart() == nsXPTType::T_PSTRING_SIZE_IS ||
                type.TagPart() == nsXPTType::T_PWSTRING_SIZE_IS;


        // verify that null was not passed for 'out' param
        if (param.IsOut() && !nativeParams[i].val.p) {
            retval = NS_ERROR_INVALID_ARG;
            goto pre_call_clean_up;
        }

        if (isArray) {
            if (NS_FAILED(mInfo->GetTypeForParam(methodIndex, &param, 1,
                                                 &datum_type)))
                goto pre_call_clean_up;
        } else
            datum_type = type;

        if (param.IsIn()) {
            nsXPTCMiniVariant* pv;

            if (param.IsIndirect())
                pv = (nsXPTCMiniVariant*) nativeParams[i].val.p;
            else
                pv = &nativeParams[i];

            if (datum_type.IsInterfacePointer() &&
                !GetInterfaceTypeFromParam(cx, info, param, methodIndex,
                                           datum_type, nativeParams,
                                           &param_iid))
                goto pre_call_clean_up;

            if (isArray || isSizedString) {
                if (!GetArraySizeFromParam(cx, info, param, methodIndex,
                                           i, nativeParams, &array_count))
                    goto pre_call_clean_up;
            }

            if (isArray) {
                if (!XPCConvert::NativeArray2JS(&val,
                                                (const void**)&pv->val,
                                                datum_type, &param_iid,
                                                array_count, nullptr))
                    goto pre_call_clean_up;
            } else if (isSizedString) {
                if (!XPCConvert::NativeStringWithSize2JS(&val,
                                                         (const void*)&pv->val,
                                                         datum_type,
                                                         array_count, nullptr))
                    goto pre_call_clean_up;
            } else {
                if (!XPCConvert::NativeData2JS(&val, &pv->val, type,
                                               &param_iid, nullptr))
                    goto pre_call_clean_up;
            }
        }

        if (param.IsOut() || param.IsDipper()) {
            // create an 'out' object
            RootedObject out_obj(cx, NewOutObject(cx, obj));
            if (!out_obj) {
                retval = NS_ERROR_OUT_OF_MEMORY;
                goto pre_call_clean_up;
            }

            if (param.IsIn()) {
                if (!JS_SetPropertyById(cx, out_obj,
                                        mRuntime->GetStringID(XPCJSRuntime::IDX_VALUE),
                                        val)) {
                    goto pre_call_clean_up;
                }
            }
            *sp++ = OBJECT_TO_JSVAL(out_obj);
        } else
            *sp++ = val;
    }

    readyToDoTheCall = true;

pre_call_clean_up:
    // clean up any 'out' params handed in
    for (i = 0; i < paramCount; i++) {
        const nsXPTParamInfo& param = info->params[i];
        if (!param.IsOut())
            continue;

        const nsXPTType& type = param.GetType();
        if (!type.deprecated_IsPointer())
            continue;
        void* p;
        if (!(p = nativeParams[i].val.p))
            continue;

        if (param.IsIn()) {
            if (type.IsArray()) {
                void** pp;
                if (nullptr != (pp = *((void***)p))) {

                    // we need to get the array length and iterate the items
                    uint32_t array_count;
                    nsXPTType datum_type;

                    if (NS_SUCCEEDED(mInfo->GetTypeForParam(methodIndex, &param,
                                                            1, &datum_type)) &&
                        datum_type.deprecated_IsPointer() &&
                        GetArraySizeFromParam(cx, info, param, methodIndex,
                                              i, nativeParams, &array_count) &&
                        array_count) {

                        CleanupPointerArray(datum_type, array_count, pp);
                    }

                    // always release the array if it is inout
                    nsMemory::Free(pp);
                }
            } else
                CleanupPointerTypeObject(type, (void**)p);
        }
        *((void**)p) = nullptr;
    }

    // Make sure "this" doesn't get deleted during this call.
    nsCOMPtr<nsIXPCWrappedJSClass> kungFuDeathGrip(this);

    if (!readyToDoTheCall)
        return retval;

    // do the deed - note exceptions

    JS_ClearPendingException(cx);

    RootedValue rval(cx);
    if (XPT_MD_IS_GETTER(info->flags)) {
        success = JS_GetProperty(cx, obj, name, &rval);
    } else if (XPT_MD_IS_SETTER(info->flags)) {
        rval = *argv;
        success = JS_SetProperty(cx, obj, name, rval);
    } else {
        if (!JSVAL_IS_PRIMITIVE(fval)) {
            AutoSaveContextOptions asco(cx);
            ContextOptionsRef(cx).setDontReportUncaught(true);

            success = JS_CallFunctionValue(cx, thisObj, fval, args, rval.address());
        } else {
            // The property was not an object so can't be a function.
            // Let's build and 'throw' an exception.

            static const nsresult code =
                    NS_ERROR_XPC_JSOBJECT_HAS_NO_FUNCTION_NAMED;
            static const char format[] = "%s \"%s\"";
            const char * msg;
            char* sz = nullptr;

            if (nsXPCException::NameAndFormatForNSResult(code, nullptr, &msg) && msg)
                sz = JS_smprintf(format, msg, name);

            nsCOMPtr<nsIException> e;

            XPCConvert::ConstructException(code, sz, GetInterfaceName(), name,
                                           nullptr, getter_AddRefs(e), nullptr, nullptr);
            xpcc->SetException(e);
            if (sz)
                JS_smprintf_free(sz);
            success = false;
        }
    }

    if (!success) {
        bool forceReport;
        if (NS_FAILED(mInfo->IsFunction(&forceReport)))
            forceReport = false;

        // May also want to check if we're moving from content->chrome and force
        // a report in that case.

        return CheckForException(ccx, name, GetInterfaceName(), forceReport);
    }

    XPCJSRuntime::Get()->SetPendingException(nullptr); // XXX necessary?

    // convert out args and result
    // NOTE: this is the total number of native params, not just the args
    // Convert independent params only.
    // When we later convert the dependent params (if any) we will know that
    // the params upon which they depend will have already been converted -
    // regardless of ordering.

    foundDependentParam = false;
    for (i = 0; i < paramCount; i++) {
        const nsXPTParamInfo& param = info->params[i];
        MOZ_ASSERT(!param.IsShared(), "[shared] implies [noscript]!");
        if (!param.IsOut() && !param.IsDipper())
            continue;

        const nsXPTType& type = param.GetType();
        if (type.IsDependent()) {
            foundDependentParam = true;
            continue;
        }

        RootedValue val(cx);
        uint8_t type_tag = type.TagPart();
        nsXPTCMiniVariant* pv;

        if (param.IsDipper())
            pv = (nsXPTCMiniVariant*) &nativeParams[i].val.p;
        else
            pv = (nsXPTCMiniVariant*) nativeParams[i].val.p;

        if (param.IsRetval())
            val = rval;
        else if (argv[i].isPrimitive())
            break;
        else {
            RootedObject obj(cx, &argv[i].toObject());
            if (!JS_GetPropertyById(cx, obj,
                                    mRuntime->GetStringID(XPCJSRuntime::IDX_VALUE),
                                    &val))
                break;
        }

        // setup allocator and/or iid

        if (type_tag == nsXPTType::T_INTERFACE) {
            if (NS_FAILED(GetInterfaceInfo()->
                          GetIIDForParamNoAlloc(methodIndex, &param,
                                                &param_iid)))
                break;
        }

        if (!XPCConvert::JSData2Native(&pv->val, val, type,
                                       !param.IsDipper(), &param_iid, nullptr))
            break;
    }

    // if any params were dependent, then we must iterate again to convert them.
    if (foundDependentParam && i == paramCount) {
        for (i = 0; i < paramCount; i++) {
            const nsXPTParamInfo& param = info->params[i];
            if (!param.IsOut())
                continue;

            const nsXPTType& type = param.GetType();
            if (!type.IsDependent())
                continue;

            RootedValue val(cx);
            nsXPTCMiniVariant* pv;
            nsXPTType datum_type;
            uint32_t array_count;
            bool isArray = type.IsArray();
            bool isSizedString = isArray ?
                    false :
                    type.TagPart() == nsXPTType::T_PSTRING_SIZE_IS ||
                    type.TagPart() == nsXPTType::T_PWSTRING_SIZE_IS;

            pv = (nsXPTCMiniVariant*) nativeParams[i].val.p;

            if (param.IsRetval())
                val = rval;
            else {
                RootedObject obj(cx, &argv[i].toObject());
                if (!JS_GetPropertyById(cx, obj,
                                        mRuntime->GetStringID(XPCJSRuntime::IDX_VALUE),
                                        &val))
                    break;
            }

            // setup allocator and/or iid

            if (isArray) {
                if (NS_FAILED(mInfo->GetTypeForParam(methodIndex, &param, 1,
                                                     &datum_type)))
                    break;
            } else
                datum_type = type;

            if (datum_type.IsInterfacePointer()) {
               if (!GetInterfaceTypeFromParam(cx, info, param, methodIndex,
                                              datum_type, nativeParams,
                                              &param_iid))
                   break;
            }

            if (isArray || isSizedString) {
                if (!GetArraySizeFromParam(cx, info, param, methodIndex,
                                           i, nativeParams, &array_count))
                    break;
            }

            if (isArray) {
                if (array_count &&
                    !XPCConvert::JSArray2Native((void**)&pv->val, val,
                                                array_count, datum_type,
                                                &param_iid, nullptr))
                    break;
            } else if (isSizedString) {
                if (!XPCConvert::JSStringWithSize2Native((void*)&pv->val, val,
                                                         array_count, datum_type,
                                                         nullptr))
                    break;
            } else {
                if (!XPCConvert::JSData2Native(&pv->val, val, type,
                                               true, &param_iid,
                                               nullptr))
                    break;
            }
        }
    }

    if (i != paramCount) {
        // We didn't manage all the result conversions!
        // We have to cleanup any junk that *did* get converted.

        for (uint8_t k = 0; k < i; k++) {
            const nsXPTParamInfo& param = info->params[k];
            if (!param.IsOut())
                continue;
            const nsXPTType& type = param.GetType();
            if (!type.deprecated_IsPointer())
                continue;
            void* p;
            if (!(p = nativeParams[k].val.p))
                continue;

            if (type.IsArray()) {
                void** pp;
                if (nullptr != (pp = *((void***)p))) {
                    // we need to get the array length and iterate the items
                    uint32_t array_count;
                    nsXPTType datum_type;

                    if (NS_SUCCEEDED(mInfo->GetTypeForParam(methodIndex, &param,
                                                            1, &datum_type)) &&
                        datum_type.deprecated_IsPointer() &&
                        GetArraySizeFromParam(cx, info, param, methodIndex,
                                              k, nativeParams, &array_count) &&
                        array_count) {

                        CleanupPointerArray(datum_type, array_count, pp);
                    }
                    nsMemory::Free(pp);
                }
            } else
                CleanupPointerTypeObject(type, (void**)p);
            *((void**)p) = nullptr;
        }
    } else {
        // set to whatever the JS code might have set as the result
        retval = pending_result;
    }

    return retval;
}

const char*
nsXPCWrappedJSClass::GetInterfaceName()
{
    if (!mName)
        mInfo->GetName(&mName);
    return mName;
}

static void
FinalizeStub(JSFreeOp *fop, JSObject *obj)
{
}

static const JSClass XPCOutParamClass = {
    "XPCOutParam",
    0,
    JS_PropertyStub,
    JS_DeletePropertyStub,
    JS_PropertyStub,
    JS_StrictPropertyStub,
    JS_EnumerateStub,
    JS_ResolveStub,
    JS_ConvertStub,
    FinalizeStub,
    nullptr,   /* call */
    nullptr,   /* hasInstance */
    nullptr,   /* construct */
    nullptr    /* trace */
};

bool
xpc::IsOutObject(JSContext* cx, JSObject* obj)
{
    return js::GetObjectJSClass(obj) == &XPCOutParamClass;
}

JSObject*
xpc::NewOutObject(JSContext* cx, JSObject* scope)
{
    RootedObject global(cx, JS_GetGlobalForObject(cx, scope));
    return JS_NewObject(cx, nullptr, NullPtr(), global);
}


NS_IMETHODIMP
nsXPCWrappedJSClass::DebugDump(int16_t depth)
{
#ifdef DEBUG
    depth-- ;
    XPC_LOG_ALWAYS(("nsXPCWrappedJSClass @ %x with mRefCnt = %d", this, mRefCnt.get()));
    XPC_LOG_INDENT();
        char* name;
        mInfo->GetName(&name);
        XPC_LOG_ALWAYS(("interface name is %s", name));
        if (name)
            nsMemory::Free(name);
        char * iid = mIID.ToString();
        XPC_LOG_ALWAYS(("IID number is %s", iid ? iid : "invalid"));
        if (iid)
            NS_Free(iid);
        XPC_LOG_ALWAYS(("InterfaceInfo @ %x", mInfo.get()));
        uint16_t methodCount = 0;
        if (depth) {
            uint16_t i;
            nsCOMPtr<nsIInterfaceInfo> parent;
            XPC_LOG_INDENT();
            mInfo->GetParent(getter_AddRefs(parent));
            XPC_LOG_ALWAYS(("parent @ %x", parent.get()));
            mInfo->GetMethodCount(&methodCount);
            XPC_LOG_ALWAYS(("MethodCount = %d", methodCount));
            mInfo->GetConstantCount(&i);
            XPC_LOG_ALWAYS(("ConstantCount = %d", i));
            XPC_LOG_OUTDENT();
        }
        XPC_LOG_ALWAYS(("mRuntime @ %x", mRuntime));
        XPC_LOG_ALWAYS(("mDescriptors @ %x count = %d", mDescriptors, methodCount));
        if (depth && mDescriptors && methodCount) {
            depth--;
            XPC_LOG_INDENT();
            for (uint16_t i = 0; i < methodCount; i++) {
                XPC_LOG_ALWAYS(("Method %d is %s%s", \
                                i, IsReflectable(i) ? "":" NOT ","reflectable"));
            }
            XPC_LOG_OUTDENT();
            depth++;
        }
    XPC_LOG_OUTDENT();
#endif
    return NS_OK;
}
