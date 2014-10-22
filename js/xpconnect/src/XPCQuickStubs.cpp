/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* vim: set ts=8 sts=4 et sw=4 tw=99: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "jsapi.h"
#include "jsfriendapi.h"
#include "jsprf.h"
#include "nsCOMPtr.h"
#include "AccessCheck.h"
#include "WrapperFactory.h"
#include "xpcprivate.h"
#include "XPCInlines.h"
#include "XPCQuickStubs.h"
#include "mozilla/dom/BindingUtils.h"
#include "mozilla/dom/Exceptions.h"

using namespace mozilla;
using namespace JS;

static nsresult
getNative(nsISupports *idobj,
          HandleObject obj,
          const nsIID &iid,
          void **ppThis,
          nsISupports **pThisRef,
          jsval *vp)
{
    nsresult rv = idobj->QueryInterface(iid, ppThis);
    *pThisRef = static_cast<nsISupports*>(*ppThis);
    if (NS_SUCCEEDED(rv))
        *vp = OBJECT_TO_JSVAL(obj);
    return rv;
}

static inline nsresult
getNativeFromWrapper(JSContext *cx,
                     XPCWrappedNative *wrapper,
                     const nsIID &iid,
                     void **ppThis,
                     nsISupports **pThisRef,
                     jsval *vp)
{
    RootedObject obj(cx, wrapper->GetFlatJSObject());
    return getNative(wrapper->GetIdentityObject(), obj, iid, ppThis, pThisRef,
                     vp);
}


static nsresult
getWrapper(JSContext *cx,
           JSObject *obj,
           XPCWrappedNative **wrapper,
           JSObject **cur,
           XPCWrappedNativeTearOff **tearoff)
{
    // We can have at most three layers in need of unwrapping here:
    // * A (possible) security wrapper
    // * A (possible) Xray waiver
    // * A (possible) outer window
    //
    // If we pass stopAtOuter == false, we can handle all three with one call
    // to js::CheckedUnwrap.
    if (js::IsWrapper(obj)) {
        JSObject* inner = js::CheckedUnwrap(obj, /* stopAtOuter = */ false);

        // The safe unwrap might have failed if we encountered an object that
        // we're not allowed to unwrap. If it didn't fail though, we should be
        // done with wrappers.
        if (!inner)
            return NS_ERROR_XPC_SECURITY_MANAGER_VETO;
        MOZ_ASSERT(!js::IsWrapper(inner));

        obj = inner;
    }

    // Start with sane values.
    *wrapper = nullptr;
    *cur = nullptr;
    *tearoff = nullptr;

    if (dom::IsDOMObject(obj)) {
        *cur = obj;

        return NS_OK;
    }

    // Handle tearoffs.
    //
    // If |obj| is of the tearoff class, that means we're dealing with a JS
    // object reflection of a particular interface (ie, |foo.nsIBar|). These
    // JS objects are parented to their wrapper, so we snag the tearoff object
    // along the way (if desired), and then set |obj| to its parent.
    const js::Class* clasp = js::GetObjectClass(obj);
    if (clasp == &XPC_WN_Tearoff_JSClass) {
        *tearoff = (XPCWrappedNativeTearOff*) js::GetObjectPrivate(obj);
        obj = js::GetObjectParent(obj);
    }

    // If we've got a WN, store things the way callers expect. Otherwise, leave
    // things null and return.
    if (IS_WN_CLASS(clasp))
        *wrapper = XPCWrappedNative::Get(obj);

    return NS_OK;
}

static nsresult
castNative(JSContext *cx,
           XPCWrappedNative *wrapper,
           JSObject *curArg,
           XPCWrappedNativeTearOff *tearoff,
           const nsIID &iid,
           void **ppThis,
           nsISupports **pThisRef,
           MutableHandleValue vp)
{
    RootedObject cur(cx, curArg);
    if (wrapper) {
        nsresult rv = getNativeFromWrapper(cx,wrapper, iid, ppThis, pThisRef,
                                           vp.address());

        if (rv != NS_ERROR_NO_INTERFACE)
            return rv;
    } else if (cur) {
        nsISupports *native;
        if (!(native = mozilla::dom::UnwrapDOMObjectToISupports(cur))) {
            *pThisRef = nullptr;
            return NS_ERROR_ILLEGAL_VALUE;
        }

        if (NS_SUCCEEDED(getNative(native, cur, iid, ppThis, pThisRef, vp.address()))) {
            return NS_OK;
        }
    }

    *pThisRef = nullptr;
    return NS_ERROR_XPC_BAD_OP_ON_WN_PROTO;
}

nsresult
xpc_qsUnwrapArgImpl(JSContext *cx,
                    HandleValue v,
                    const nsIID &iid,
                    void **ppArg,
                    nsISupports **ppArgRef,
                    MutableHandleValue vp)
{
    MOZ_ASSERT(v.isObject());
    RootedObject src(cx, &v.toObject());

    XPCWrappedNative *wrapper;
    XPCWrappedNativeTearOff *tearoff;
    JSObject *obj2;
    nsresult rv = getWrapper(cx, src, &wrapper, &obj2, &tearoff);
    NS_ENSURE_SUCCESS(rv, rv);

    if (wrapper || obj2) {
        if (NS_FAILED(castNative(cx, wrapper, obj2, tearoff, iid, ppArg,
                                 ppArgRef, vp)))
            return NS_ERROR_XPC_BAD_CONVERT_JS;
        return NS_OK;
    }
    // else...
    // Slow path.

    // Try to unwrap a slim wrapper.
    nsISupports *iface;
    if (XPCConvert::GetISupportsFromJSObject(src, &iface)) {
        if (!iface || NS_FAILED(iface->QueryInterface(iid, ppArg))) {
            *ppArgRef = nullptr;
            return NS_ERROR_XPC_BAD_CONVERT_JS;
        }

        *ppArgRef = static_cast<nsISupports*>(*ppArg);
        return NS_OK;
    }

    // Create the ccx needed for quick stubs.
    XPCCallContext ccx(JS_CALLER, cx);
    if (!ccx.IsValid()) {
        *ppArgRef = nullptr;
        return NS_ERROR_XPC_BAD_CONVERT_JS;
    }

    nsRefPtr<nsXPCWrappedJS> wrappedJS;
    rv = nsXPCWrappedJS::GetNewOrUsed(src, iid, getter_AddRefs(wrappedJS));
    if (NS_FAILED(rv) || !wrappedJS) {
        *ppArgRef = nullptr;
        return rv;
    }

    // We need to go through the QueryInterface logic to make this return
    // the right thing for the various 'special' interfaces; e.g.
    // nsIPropertyBag. We must use AggregatedQueryInterface in cases where
    // there is an outer to avoid nasty recursion.
    rv = wrappedJS->QueryInterface(iid, ppArg);
    if (NS_SUCCEEDED(rv)) {
        *ppArgRef = static_cast<nsISupports*>(*ppArg);
        vp.setObjectOrNull(wrappedJS->GetJSObject());
    }
    return rv;
}

namespace xpc {

bool
NonVoidStringToJsval(JSContext *cx, nsAString &str, MutableHandleValue rval)
{
    nsStringBuffer* sharedBuffer;
    if (!XPCStringConvert::ReadableToJSVal(cx, str, &sharedBuffer, rval))
      return false;

    if (sharedBuffer) {
        // The string was shared but ReadableToJSVal didn't addref it.
        // Move the ownership from str to jsstr.
        str.ForgetSharedBuffer();
    }
    return true;
}

} // namespace xpc

