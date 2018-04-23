/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* vim: set ts=8 sts=4 et sw=4 tw=99: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/* Sharable code and data for wrapper around JSObjects. */

#include "xpcprivate.h"
#include "js/Printf.h"
#include "nsArrayEnumerator.h"
#include "nsINamed.h"
#include "nsIScriptError.h"
#include "nsWrapperCache.h"
#include "AccessCheck.h"
#include "nsJSUtils.h"
#include "nsPrintfCString.h"
#include "mozilla/Attributes.h"
#include "mozilla/dom/BindingUtils.h"
#include "mozilla/dom/DOMException.h"
#include "mozilla/dom/DOMExceptionBinding.h"
#include "mozilla/dom/DOMPrefs.h"
#include "mozilla/dom/MozQueryInterface.h"
#include "mozilla/jsipc/CrossProcessObjectWrappers.h"

#include "jsapi.h"
#include "jsfriendapi.h"

using namespace xpc;
using namespace JS;
using namespace mozilla;
using namespace mozilla::dom;

NS_IMPL_ISUPPORTS(nsXPCWrappedJSClass, nsIXPCWrappedJSClass)

// the value of this variable is never used - we use its address as a sentinel
static uint32_t zero_methods_descriptor;

bool AutoScriptEvaluate::StartEvaluating(HandleObject scope)
{
    MOZ_ASSERT(!mEvaluated, "AutoScriptEvaluate::Evaluate should only be called once");

    if (!mJSContext)
        return true;

    mEvaluated = true;

    JS_BeginRequest(mJSContext);
    mAutoCompartment.emplace(mJSContext, scope);

    // Saving the exception state keeps us from interfering with another script
    // that may also be running on this context.  This occurred first with the
    // js debugger, as described in
    // http://bugzilla.mozilla.org/show_bug.cgi?id=88130 but presumably could
    // show up in any situation where a script calls into a wrapped js component
    // on the same context, while the context has a nonzero exception state.
    mState.emplace(mJSContext);

    return true;
}

AutoScriptEvaluate::~AutoScriptEvaluate()
{
    if (!mJSContext || !mEvaluated)
        return;
    mState->restore();

    JS_EndRequest(mJSContext);
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

// A little stack-based RAII class to help management of the XPCJSContext
// PendingResult.
class MOZ_STACK_CLASS AutoSavePendingResult {
public:
    explicit AutoSavePendingResult(XPCJSContext* xpccx) :
        mXPCContext(xpccx)
    {
        // Save any existing pending result and reset to NS_OK for this invocation.
        mSavedResult = xpccx->GetPendingResult();
        xpccx->SetPendingResult(NS_OK);
    }
    ~AutoSavePendingResult() {
        mXPCContext->SetPendingResult(mSavedResult);
    }
private:
    XPCJSContext* mXPCContext;
    nsresult mSavedResult;
};

// static
already_AddRefed<nsXPCWrappedJSClass>
nsXPCWrappedJSClass::GetNewOrUsed(JSContext* cx, REFNSIID aIID, bool allowNonScriptable)
{
    XPCJSRuntime* xpcrt = nsXPConnect::GetRuntimeInstance();
    IID2WrappedJSClassMap* map = xpcrt->GetWrappedJSClassMap();
    RefPtr<nsXPCWrappedJSClass> clasp = map->Find(aIID);

    if (!clasp) {
        const nsXPTInterfaceInfo* info = nsXPTInterfaceInfo::ByIID(aIID);
        if (info) {
            bool canScript, isBuiltin;
            if (NS_SUCCEEDED(info->IsScriptable(&canScript)) && (canScript || allowNonScriptable) &&
                NS_SUCCEEDED(info->IsBuiltinClass(&isBuiltin)) && !isBuiltin &&
                nsXPConnect::IsISupportsDescendant(info))
            {
                clasp = new nsXPCWrappedJSClass(cx, aIID, info);
                if (!clasp->mDescriptors)
                    clasp = nullptr;
            }
        }
    }
    return clasp.forget();
}

nsXPCWrappedJSClass::nsXPCWrappedJSClass(JSContext* cx, REFNSIID aIID,
                                         const nsXPTInterfaceInfo* aInfo)
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
        free(mName);
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

    // In bug 503926, we added a security check to make sure that we don't
    // invoke content QI functions. In the modern world, this is probably
    // unnecessary, because invoking QI involves passing an IID object to
    // content, which will fail. But we do a belt-and-suspenders check to
    // make sure that content can never trigger the rat's nest of code below.
    // Once we completely turn off XPConnect for the web, this can definitely
    // go away.
    if (!AccessCheck::isChrome(jsobj) ||
        !AccessCheck::isChrome(js::UncheckedUnwrap(jsobj)))
    {
        return nullptr;
    }

    // OK, it looks like we'll be calling into JS code.
    AutoScriptEvaluate scriptEval(cx);

    // XXX we should install an error reporter that will send reports to
    // the JS error console service.
    if (!scriptEval.StartEvaluating(jsobj))
        return nullptr;

    // check upfront for the existence of the function property
    HandleId funid = mRuntime->GetStringID(XPCJSContext::IDX_QUERY_INTERFACE);
    if (!JS_GetPropertyById(cx, jsobj, funid, &fun) || fun.isPrimitive())
        return nullptr;

    // Ensure that we are asking for a scriptable interface.
    // NB:  It's important for security that this check is here rather
    // than later, since it prevents untrusted objects from implementing
    // some interfaces in JS and aggregating a trusted object to
    // implement intentionally (for security) unscriptable interfaces.
    // We so often ask for nsISupports that we can short-circuit the test...
    if (!aIID.Equals(NS_GET_IID(nsISupports))) {
        bool allowNonScriptable = mozilla::jsipc::IsWrappedCPOW(jsobj);

        const nsXPTInterfaceInfo* info = nsXPTInterfaceInfo::ByIID(aIID);
        if (!info || info->IsBuiltinClass() ||
            (!info->IsScriptable() && !allowNonScriptable))
        {
            return nullptr;
        }
    }

    dom::MozQueryInterface* mozQI = nullptr;
    if (NS_SUCCEEDED(UNWRAP_OBJECT(MozQueryInterface, &fun, mozQI))) {
        if (mozQI->QueriesTo(aIID))
            return jsobj.get();
        return nullptr;
    }

    if ((id = xpc_NewIDObject(cx, jsobj, aIID))) {
        // Throwing NS_NOINTERFACE is the prescribed way to fail QI from JS. It
        // is not an exception that is ever worth reporting, but we don't want
        // to eat all exceptions either.

        {
            RootedValue arg(cx, JS::ObjectValue(*id));
            success = JS_CallFunctionValue(cx, jsobj, fun, HandleValueArray(arg), &retval);
        }

        if (!success && JS_IsExceptionPending(cx)) {
            RootedValue jsexception(cx, NullValue());

            if (JS_GetPendingException(cx, &jsexception)) {
                if (jsexception.isObject()) {
                    // XPConnect may have constructed an object to represent a
                    // C++ QI failure. See if that is the case.
                    JS::Rooted<JSObject*> exceptionObj(cx, &jsexception.toObject());
                    Exception* e = nullptr;
                    UNWRAP_OBJECT(Exception, &exceptionObj, e);

                    if (e && e->GetResult() == NS_NOINTERFACE) {
                        JS_ClearPendingException(cx);
                    }
                } else if (jsexception.isNumber()) {
                    nsresult rv;
                    // JS often throws an nsresult.
                    if (jsexception.isDouble())
                        // Visual Studio 9 doesn't allow casting directly from
                        // a double to an enumeration type, contrary to
                        // 5.2.9(10) of C++11, so add an intermediate cast.
                        rv = (nsresult)(uint32_t)(jsexception.toDouble());
                    else
                        rv = (nsresult)(jsexception.toInt32());

                    if (rv == NS_NOINTERFACE)
                        JS_ClearPendingException(cx);
                }
            }
        } else if (!success) {
            NS_WARNING("QI hook ran OOMed - this is probably a bug!");
        }

        if (success)
            success = JS_ValueToObject(cx, retval, &retObj);
    }

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
    nsXPTType type = { TD_INTERFACE_TYPE };
    RootedValue val(ccx);

    return JS_GetPropertyById(ccx, aJSObj, aName, &val) &&
           XPCConvert::JSData2Native(aResult, val, type,
                                     &NS_GET_IID(nsIVariant), 0, pErr);
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

    // Wrap the string in a Value after the AutoScriptEvaluate, so that the
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

    Rooted<IdVector> idArray(cx, IdVector(cx));
    if (!JS_Enumerate(cx, aJSObj, &idArray))
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

        nsAutoJSString autoStr;
        if (!autoStr.init(cx, name))
            return NS_ERROR_FAILURE;

        nsCOMPtr<nsIProperty> property =
            new xpcProperty(autoStr.get(), (uint32_t)autoStr.Length(), value);

        if (!propertyArray.AppendObject(property))
            return NS_ERROR_FAILURE;
    }

    return NS_NewArrayEnumerator(aEnumerate, propertyArray);
}

/***************************************************************************/

NS_IMPL_ISUPPORTS(xpcProperty, nsIProperty)

xpcProperty::xpcProperty(const char16_t* aName, uint32_t aNameLen,
                         nsIVariant* aValue)
    : mName(aName, aNameLen), mValue(aValue)
{
}

NS_IMETHODIMP xpcProperty::GetName(nsAString & aName)
{
    aName.Assign(mName);
    return NS_OK;
}

NS_IMETHODIMP xpcProperty::GetValue(nsIVariant * *aValue)
{
    nsCOMPtr<nsIVariant> rval = mValue;
    rval.forget(aValue);
    return NS_OK;
}

/***************************************************************************/

namespace {

class WrappedJSNamed final : public nsINamed
{
    nsCString mName;

    ~WrappedJSNamed() {}

public:
    NS_DECL_ISUPPORTS

    explicit WrappedJSNamed(const nsACString& aName) : mName(aName) {}

    NS_IMETHOD GetName(nsACString& aName) override
    {
        aName = mName;
        aName.AppendLiteral(":JS");
        return NS_OK;
    }
};

NS_IMPL_ISUPPORTS(WrappedJSNamed, nsINamed)

nsCString
GetFunctionName(JSContext* cx, HandleObject obj)
{
    RootedObject inner(cx, js::UncheckedUnwrap(obj));
    JSAutoCompartment ac(cx, inner);

    RootedFunction fun(cx, JS_GetObjectFunction(inner));
    if (!fun) {
        // If the object isn't a function, it's likely that it has a single
        // function property (for things like nsITimerCallback). In this case,
        // return the name of that function property.

        Rooted<IdVector> idArray(cx, IdVector(cx));
        if (!JS_Enumerate(cx, inner, &idArray)) {
            JS_ClearPendingException(cx);
            return nsCString("error");
        }

        if (idArray.length() != 1)
            return nsCString("nonfunction");

        RootedId id(cx, idArray[0]);
        RootedValue v(cx);
        if (!JS_GetPropertyById(cx, inner, id, &v)) {
            JS_ClearPendingException(cx);
            return nsCString("nonfunction");
        }

        if (!v.isObject())
            return nsCString("nonfunction");

        RootedObject vobj(cx, &v.toObject());
        return GetFunctionName(cx, vobj);
    }

    RootedString funName(cx, JS_GetFunctionDisplayId(fun));
    RootedScript script(cx, JS_GetFunctionScript(cx, fun));
    const char* filename = script ? JS_GetScriptFilename(script) : "anonymous";
    const char* filenameSuffix = strrchr(filename, '/');

    if (filenameSuffix) {
        filenameSuffix++;
    } else {
        filenameSuffix = filename;
    }

    nsCString displayName("anonymous");
    if (funName) {
        RootedValue funNameVal(cx, StringValue(funName));
        if (!XPCConvert::JSData2Native(&displayName, funNameVal,
                                       { nsXPTType::T_UTF8STRING },
                                       nullptr, 0, nullptr))
        {
            JS_ClearPendingException(cx);
            return nsCString("anonymous");
        }
    }

    displayName.Append('[');
    displayName.Append(filenameSuffix, strlen(filenameSuffix));
    displayName.Append(']');
    return displayName;
}

} // anonymous namespace

/***************************************************************************/

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

    // QI on an XPCWrappedJS can run script, so we need an AutoEntryScript.
    // This is inherently Gecko-specific.
    // We check both nativeGlobal and nativeGlobal->GetGlobalJSObject() even
    // though we have derived nativeGlobal from the JS global, because we know
    // there are cases where this can happen. See bug 1094953.
    nsIGlobalObject* nativeGlobal =
      NativeGlobal(js::GetGlobalForObjectCrossCompartment(self->GetJSObject()));
    NS_ENSURE_TRUE(nativeGlobal, NS_ERROR_FAILURE);
    NS_ENSURE_TRUE(nativeGlobal->GetGlobalJSObject(), NS_ERROR_FAILURE);
    AutoEntryScript aes(nativeGlobal, "XPCWrappedJS QueryInterface",
                        /* aIsMainThread = */ true);
    XPCCallContext ccx(aes.cx());
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

    // Check if the desired interface is a function interface. If so, we don't
    // want to QI, because the function almost certainly doesn't have a QueryInterface
    // property, and doesn't need one.
    const nsXPTInterfaceInfo* info = nsXPTInterfaceInfo::ByIID(aIID);
    if (info && info->IsFunction()) {
        RefPtr<nsXPCWrappedJS> wrapper;
        RootedObject obj(RootingCx(), self->GetJSObject());
        nsresult rv = nsXPCWrappedJS::GetNewOrUsed(obj, aIID, getter_AddRefs(wrapper));

        // Do the same thing we do for the "check for any existing wrapper" case above.
        if (NS_SUCCEEDED(rv) && wrapper) {
            *aInstancePtr = wrapper.forget().take()->GetXPTCStub();
        }
        return rv;
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
        RefPtr<nsXPCWrappedJS> wrapper;
        nsresult rv = nsXPCWrappedJS::GetNewOrUsed(jsobj, aIID, getter_AddRefs(wrapper));
        if (NS_SUCCEEDED(rv) && wrapper) {
            // We need to go through the QueryInterface logic to make
            // this return the right thing for the various 'special'
            // interfaces; e.g.  nsIPropertyBag.
            rv = wrapper->QueryInterface(aIID, aInstancePtr);
            return rv;
        }
    }

    // If we're asked to QI to nsINamed, we pretend that this is possible. We'll
    // try to return a name that makes sense for the wrapped JS value.
    if (aIID.Equals(NS_GET_IID(nsINamed))) {
        RootedObject obj(RootingCx(), self->GetJSObject());
        nsCString name = GetFunctionName(ccx, obj);
        RefPtr<WrappedJSNamed> named = new WrappedJSNamed(name);
        *aInstancePtr = named.forget().take();
        return NS_OK;
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
        result = aJSObj;
    JSObject* inner = js::UncheckedUnwrap(result);
    if (inner)
        return inner;
    return result;
}

bool
nsXPCWrappedJSClass::GetArraySizeFromParam(const nsXPTMethodInfo* method,
                                           const nsXPTType& type,
                                           nsXPTCMiniVariant* nativeParams,
                                           uint32_t* result) const
{
    if (type.Tag() != nsXPTType::T_ARRAY &&
        type.Tag() != nsXPTType::T_PSTRING_SIZE_IS &&
        type.Tag() != nsXPTType::T_PWSTRING_SIZE_IS) {
        *result = 0;
        return true;
    }

    uint8_t argnum = type.ArgNum();
    const nsXPTParamInfo& param = method->Param(argnum);

    // This should be enforced by the xpidl compiler, but it's not.
    // See bug 695235.
    if (param.Type().Tag() != nsXPTType::T_U32) {
        return false;
    }

    // If the length is passed indirectly (as an outparam), dereference by an
    // extra level.
    if (param.IsIndirect()) {
        *result = *(uint32_t*)nativeParams[argnum].val.p;
    } else {
        *result = nativeParams[argnum].val.u32;
    }
    return true;
}

bool
nsXPCWrappedJSClass::GetInterfaceTypeFromParam(const nsXPTMethodInfo* method,
                                               const nsXPTType& type,
                                               nsXPTCMiniVariant* nativeParams,
                                               nsID* result) const
{
    result->Clear();

    const nsXPTType& inner = type.InnermostType();
    if (inner.Tag() == nsXPTType::T_INTERFACE) {
        // Directly get IID from nsXPTInterfaceInfo.
        if (!inner.GetInterface()) {
            return false;
        }

        *result = inner.GetInterface()->IID();
    } else if (inner.Tag() == nsXPTType::T_INTERFACE_IS) {
        // Get IID from a passed parameter.
        const nsXPTParamInfo& param = method->Param(inner.ArgNum());
        if (param.Type().Tag() != nsXPTType::T_IID) {
            return false;
        }

        void* ptr = nativeParams[inner.ArgNum()].val.p;

        // If the IID is passed indirectly (as an outparam), dereference by an
        // extra level.
        if (ptr && param.IsIndirect()) {
            ptr = *(nsID**) ptr;
        }

        if (!ptr) {
            return false;
        }

        *result = *(nsID*) ptr;
    }
    return true;
}

void
nsXPCWrappedJSClass::CleanupOutparams(const nsXPTMethodInfo* info,
                                      nsXPTCMiniVariant* nativeParams,
                                      bool inOutOnly, uint8_t count) const
{
    // clean up any 'out' params handed in
    for (uint8_t i = 0; i < count; i++) {
        const nsXPTParamInfo& param = info->GetParam(i);
        if (!param.IsOut())
            continue;

        // Extract the array length so we can use it in CleanupValue.
        uint32_t arrayLen = 0;
        if (!GetArraySizeFromParam(info, param.Type(), nativeParams, &arrayLen))
            continue;

        MOZ_ASSERT(param.IsIndirect(), "Outparams are always indirect");

        // The inOutOnly flag is necessary because full outparams may contain
        // uninitialized junk before the call is made, and we don't want to try
        // to clean up uninitialized junk.
        if (!inOutOnly || param.IsIn()) {
            xpc::CleanupValue(param.Type(), nativeParams[i].val.p, arrayLen);
        }

        // Even if we didn't call CleanupValue, null out any pointers. This is
        // just to protect C++ callers which may read garbage if they forget to
        // check the error value.
        if (param.Type().HasPointerRepr()) {
            *(void**)nativeParams[i].val.p = nullptr;
        }
    }
}

nsresult
nsXPCWrappedJSClass::CheckForException(XPCCallContext & ccx,
                                       AutoEntryScript& aes,
                                       const char * aPropertyName,
                                       const char * anInterfaceName,
                                       Exception* aSyntheticException)
{
    JSContext * cx = ccx.GetJSContext();
    MOZ_ASSERT(cx == aes.cx());
    RefPtr<Exception> xpc_exception = aSyntheticException;
    /* this one would be set by our error reporter */

    XPCJSContext* xpccx = ccx.GetContext();

    // Get this right away in case we do something below to cause JS code
    // to run.
    nsresult pending_result = xpccx->GetPendingResult();

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
            xpccx->SetPendingException(nullptr); // XXX necessary?
        }
    }

    // Clear the pending exception now, because xpc_exception might be JS-
    // implemented, so invoking methods on it might re-enter JS, which we can't
    // do with an exception on the stack.
    aes.ClearException();

    if (xpc_exception) {
        nsresult e_result = xpc_exception->GetResult();
        // Figure out whether or not we should report this exception.
        bool reportable = xpc_IsReportableErrorCode(e_result);
        if (reportable) {
            // Ugly special case for GetInterface. It's "special" in the
            // same way as QueryInterface in that a failure is not
            // exceptional and shouldn't be reported. We have to do this
            // check here instead of in xpcwrappedjs (like we do for QI) to
            // avoid adding extra code to all xpcwrappedjs objects.
            if (e_result == NS_ERROR_NO_INTERFACE &&
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
        if (reportable && is_js_exception)
        {
            // Note that we cleared the exception above, so we need to set it again,
            // just so that we can tell the JS engine to pass it back to us via the
            // error reporting callback. This is all very dumb.
            JS_SetPendingException(cx, js_exception);
            aes.ReportException();
            reportable = false;
        }

        if (reportable) {
            if (DOMPrefs::DumpEnabled()) {
                static const char line[] =
                    "************************************************************\n";
                static const char preamble[] =
                    "* Call to xpconnect wrapped JSObject produced this error:  *\n";
                static const char cant_get_text[] =
                    "FAILED TO GET TEXT FROM EXCEPTION\n";

                fputs(line, stdout);
                fputs(preamble, stdout);
                nsCString text;
                xpc_exception->ToString(cx, text);
                if (!text.IsEmpty()) {
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
                nsCOMPtr<nsIScriptError> scriptError =
                    do_QueryInterface(xpc_exception->GetData());

                if (nullptr == scriptError) {
                    // No luck getting one from the exception, so
                    // try to cook one up.
                    scriptError = do_CreateInstance(XPC_SCRIPT_ERROR_CONTRACTID);
                    if (nullptr != scriptError) {
                        nsCString newMessage;
                        xpc_exception->ToString(cx, newMessage);
                        // try to get filename, lineno from the first
                        // stack frame location.
                        int32_t lineNumber = 0;
                        nsString sourceName;

                        nsCOMPtr<nsIStackFrame> location =
                            xpc_exception->GetLocation();
                        if (location) {
                            // Get line number.
                            lineNumber = location->GetLineNumber(cx);

                            // get a filename.
                            location->GetFilename(cx, sourceName);
                        }

                        nsresult rv =
                            scriptError->InitWithWindowID(NS_ConvertUTF8toUTF16(newMessage),
                                                          sourceName,
                                                          EmptyString(),
                                                          lineNumber, 0, 0,
                                                          "XPConnect JavaScript",
                                                          nsJSUtils::GetCurrentlyRunningCodeInnerWindowID(cx));
                        if (NS_FAILED(rv))
                            scriptError = nullptr;
                    }
                }
                if (nullptr != scriptError)
                    consoleService->LogMessage(scriptError);
            }
        }
        // Whether or not it passes the 'reportable' test, it might
        // still be an error and we have to do the right thing here...
        if (NS_FAILED(e_result)) {
            xpccx->SetPendingException(xpc_exception);
            return e_result;
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
                                const nsXPTMethodInfo* info,
                                nsXPTCMiniVariant* nativeParams)
{
    Value* sp = nullptr;
    Value* argv = nullptr;
    uint8_t i;
    nsresult retval = NS_ERROR_FAILURE;
    bool success;
    bool readyToDoTheCall = false;
    nsID  param_iid;
    const char* name = info->GetName();
    bool foundDependentParam;

    // We're about to call into script via an XPCWrappedJS, so we need an
    // AutoEntryScript. This is probably Gecko-specific at this point, and
    // definitely will be when we turn off XPConnect for the web.
    nsIGlobalObject* nativeGlobal =
      NativeGlobal(js::GetGlobalForObjectCrossCompartment(wrapper->GetJSObject()));
    AutoEntryScript aes(nativeGlobal, "XPCWrappedJS method call",
                        /* aIsMainThread = */ true);
    XPCCallContext ccx(aes.cx());
    if (!ccx.IsValid())
        return retval;

    JSContext* cx = ccx.GetJSContext();

    if (!cx || !IsReflectable(methodIndex))
        return NS_ERROR_FAILURE;

    // [implicit_jscontext] and [optional_argc] have a different calling
    // convention, which we don't support for JS-implemented components.
    if (info->WantsOptArgc() || info->WantsContext()) {
        const char* str = "IDL methods marked with [implicit_jscontext] "
                          "or [optional_argc] may not be implemented in JS";
        // Throw and warn for good measure.
        JS_ReportErrorASCII(cx, "%s", str);
        NS_WARNING(str);
        return CheckForException(ccx, aes, name, GetInterfaceName());
    }

    RootedValue fval(cx);
    RootedObject obj(cx, wrapper->GetJSObject());
    RootedObject thisObj(cx, obj);

    JSAutoCompartment ac(cx, obj);

    AutoValueVector args(cx);
    AutoScriptEvaluate scriptEval(cx);

    XPCJSContext* xpccx = ccx.GetContext();
    AutoSavePendingResult apr(xpccx);

    // XXX ASSUMES that retval is last arg. The xpidl compiler ensures this.
    uint8_t paramCount = info->GetParamCount();
    uint8_t argc = paramCount;
    if (info->HasRetval()) {
        argc -= 1;
    }

    if (!scriptEval.StartEvaluating(obj))
        goto pre_call_clean_up;

    xpccx->SetPendingException(nullptr);

    // We use js_Invoke so that the gcthings we use as args will be rooted by
    // the engine as we do conversions and prepare to do the function call.

    // setup stack

    // if this isn't a function call then we don't need to push extra stuff
    if (!(info->IsSetter() || info->IsGetter())) {
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
        if (!isFunction || JS_TypeOfValue(ccx, fval) != JSTYPE_FUNCTION) {
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
        const nsXPTParamInfo& param = info->GetParam(i);
        const nsXPTType& type = param.GetType();
        uint32_t array_count;
        RootedValue val(cx, NullValue());


        // verify that null was not passed for 'out' param
        if (param.IsOut() && !nativeParams[i].val.p) {
            retval = NS_ERROR_INVALID_ARG;
            goto pre_call_clean_up;
        }

        if (param.IsIn()) {
            const void* pv;
            if (param.IsIndirect())
                pv = nativeParams[i].val.p;
            else
                pv = &nativeParams[i];

            if (!GetInterfaceTypeFromParam(info, type, nativeParams, &param_iid) ||
                !GetArraySizeFromParam(info, type, nativeParams, &array_count))
                goto pre_call_clean_up;

            if (!XPCConvert::NativeData2JS(&val, pv, type,
                                            &param_iid, array_count,
                                            nullptr))
                goto pre_call_clean_up;
        }

        if (param.IsOut()) {
            // create an 'out' object
            RootedObject out_obj(cx, NewOutObject(cx));
            if (!out_obj) {
                retval = NS_ERROR_OUT_OF_MEMORY;
                goto pre_call_clean_up;
            }

            if (param.IsIn()) {
                if (!JS_SetPropertyById(cx, out_obj,
                                        mRuntime->GetStringID(XPCJSContext::IDX_VALUE),
                                        val)) {
                    goto pre_call_clean_up;
                }
            }
            *sp++ = JS::ObjectValue(*out_obj);
        } else
            *sp++ = val;
    }

    readyToDoTheCall = true;

pre_call_clean_up:
    // clean up any 'out' params handed in
    CleanupOutparams(info, nativeParams, /* inOutOnly = */ true, paramCount);

    // Make sure "this" doesn't get deleted during this call.
    nsCOMPtr<nsIXPCWrappedJSClass> kungFuDeathGrip(this);

    if (!readyToDoTheCall)
        return retval;

    // do the deed - note exceptions

    MOZ_ASSERT(!aes.HasException());

    RefPtr<Exception> syntheticException;
    RootedValue rval(cx);
    if (info->IsGetter()) {
        success = JS_GetProperty(cx, obj, name, &rval);
    } else if (info->IsSetter()) {
        rval = *argv;
        success = JS_SetProperty(cx, obj, name, rval);
    } else {
        if (!fval.isPrimitive()) {
            success = JS_CallFunctionValue(cx, thisObj, fval, args, &rval);
        } else {
            // The property was not an object so can't be a function.
            // Let's build and 'throw' an exception.

            static const nsresult code =
                    NS_ERROR_XPC_JSOBJECT_HAS_NO_FUNCTION_NAMED;
            static const char format[] = "%s \"%s\"";
            const char * msg;
            UniqueChars sz;

            if (nsXPCException::NameAndFormatForNSResult(code, nullptr, &msg) && msg)
                sz = JS_smprintf(format, msg, name);

            XPCConvert::ConstructException(code, sz.get(), GetInterfaceName(), name,
                                           nullptr, getter_AddRefs(syntheticException),
                                           nullptr, nullptr);
            success = false;
        }
    }

    if (!success)
        return CheckForException(ccx, aes, name, GetInterfaceName(),
                                 syntheticException);

    xpccx->SetPendingException(nullptr); // XXX necessary?

    // convert out args and result
    // NOTE: this is the total number of native params, not just the args
    // Convert independent params only.
    // When we later convert the dependent params (if any) we will know that
    // the params upon which they depend will have already been converted -
    // regardless of ordering.

    foundDependentParam = false;
    for (i = 0; i < paramCount; i++) {
        const nsXPTParamInfo& param = info->GetParam(i);
        MOZ_ASSERT(!param.IsShared(), "[shared] implies [noscript]!");
        if (!param.IsOut())
            continue;

        const nsXPTType& type = param.GetType();
        if (type.IsDependent()) {
            foundDependentParam = true;
            continue;
        }

        RootedValue val(cx);

        if (&param == info->GetRetval())
            val = rval;
        else if (argv[i].isPrimitive())
            break;
        else {
            RootedObject obj(cx, &argv[i].toObject());
            if (!JS_GetPropertyById(cx, obj,
                                    mRuntime->GetStringID(XPCJSContext::IDX_VALUE),
                                    &val))
                break;
        }

        // setup allocator and/or iid

        const nsXPTType& inner = type.InnermostType();
        if (inner.Tag() == nsXPTType::T_INTERFACE) {
            if (!inner.GetInterface())
                break;
            param_iid = inner.GetInterface()->IID();
        }

        MOZ_ASSERT(param.IsIndirect(), "outparams are always indirect");
        if (!XPCConvert::JSData2Native(nativeParams[i].val.p, val, type,
                                       &param_iid, 0, nullptr))
            break;
    }

    // if any params were dependent, then we must iterate again to convert them.
    if (foundDependentParam && i == paramCount) {
        for (i = 0; i < paramCount; i++) {
            const nsXPTParamInfo& param = info->GetParam(i);
            if (!param.IsOut())
                continue;

            const nsXPTType& type = param.GetType();
            if (!type.IsDependent())
                continue;

            RootedValue val(cx);
            uint32_t array_count;

            if (&param == info->GetRetval())
                val = rval;
            else {
                RootedObject obj(cx, &argv[i].toObject());
                if (!JS_GetPropertyById(cx, obj,
                                        mRuntime->GetStringID(XPCJSContext::IDX_VALUE),
                                        &val))
                    break;
            }

            // setup allocator and/or iid

            if (!GetInterfaceTypeFromParam(info, type, nativeParams, &param_iid) ||
                !GetArraySizeFromParam(info, type, nativeParams, &array_count))
                break;

            MOZ_ASSERT(param.IsIndirect(), "outparams are always indirect");
            if (!XPCConvert::JSData2Native(nativeParams[i].val.p, val, type,
                                           &param_iid, array_count, nullptr))
                break;
        }
    }

    if (i != paramCount) {
        // We didn't manage all the result conversions!
        // We have to cleanup any junk that *did* get converted.
        CleanupOutparams(info, nativeParams, /* inOutOnly = */ false, i);
    } else {
        // set to whatever the JS code might have set as the result
        retval = xpccx->GetPendingResult();
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

static const JSClass XPCOutParamClass = {
    "XPCOutParam",
    0,
    JS_NULL_CLASS_OPS
};

bool
xpc::IsOutObject(JSContext* cx, JSObject* obj)
{
    return js::GetObjectJSClass(obj) == &XPCOutParamClass;
}

JSObject*
xpc::NewOutObject(JSContext* cx)
{
    return JS_NewObject(cx, &XPCOutParamClass);
}


NS_IMETHODIMP
nsXPCWrappedJSClass::DebugDump(int16_t depth)
{
#ifdef DEBUG
    depth-- ;
    XPC_LOG_ALWAYS(("nsXPCWrappedJSClass @ %p with mRefCnt = %" PRIuPTR, this, mRefCnt.get()));
    XPC_LOG_INDENT();
        char* name;
        mInfo->GetName(&name);
        XPC_LOG_ALWAYS(("interface name is %s", name));
        if (name)
            free(name);
        char * iid = mIID.ToString();
        XPC_LOG_ALWAYS(("IID number is %s", iid ? iid : "invalid"));
        if (iid)
            free(iid);
        XPC_LOG_ALWAYS(("InterfaceInfo @ %p", mInfo));
        uint16_t methodCount = 0;
        if (depth) {
            uint16_t i;
            const nsXPTInterfaceInfo* parent;
            XPC_LOG_INDENT();
            mInfo->GetParent(&parent);
            XPC_LOG_ALWAYS(("parent @ %p", parent));
            mInfo->GetMethodCount(&methodCount);
            XPC_LOG_ALWAYS(("MethodCount = %d", methodCount));
            mInfo->GetConstantCount(&i);
            XPC_LOG_ALWAYS(("ConstantCount = %d", i));
            XPC_LOG_OUTDENT();
        }
        XPC_LOG_ALWAYS(("mRuntime @ %p", mRuntime));
        XPC_LOG_ALWAYS(("mDescriptors @ %p count = %d", mDescriptors, methodCount));
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
