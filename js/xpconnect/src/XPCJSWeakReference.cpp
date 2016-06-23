/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* vim: set ts=8 sts=4 et sw=4 tw=99: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "xpcprivate.h"
#include "XPCJSWeakReference.h"

#include "nsContentUtils.h"

using namespace JS;

xpcJSWeakReference::xpcJSWeakReference()
{
}

NS_IMPL_ISUPPORTS(xpcJSWeakReference, xpcIJSWeakReference)

nsresult xpcJSWeakReference::Init(JSContext* cx, const JS::Value& object)
{
    if (!object.isObject())
        return NS_OK;

    JS::RootedObject obj(cx, &object.toObject());

    XPCCallContext ccx(cx);

    // See if the object is a wrapped native that supports weak references.
    nsISupports* supports =
        nsXPConnect::XPConnect()->GetNativeOfWrapper(cx, obj);
    nsCOMPtr<nsISupportsWeakReference> supportsWeakRef =
        do_QueryInterface(supports);
    if (supportsWeakRef) {
        supportsWeakRef->GetWeakReference(getter_AddRefs(mReferent));
        if (mReferent) {
            return NS_OK;
        }
    }
    // If it's not a wrapped native, or it is a wrapped native that does not
    // support weak references, fall back to getting a weak ref to the object.

    // See if object is a wrapped JSObject.
    RefPtr<nsXPCWrappedJS> wrapped;
    nsresult rv = nsXPCWrappedJS::GetNewOrUsed(obj,
                                               NS_GET_IID(nsISupports),
                                               getter_AddRefs(wrapped));
    if (!wrapped) {
        NS_ERROR("can't get nsISupportsWeakReference wrapper for obj");
        return rv;
    }

    return wrapped->GetWeakReference(getter_AddRefs(mReferent));
}

NS_IMETHODIMP
xpcJSWeakReference::Get(JSContext* aCx, MutableHandleValue aRetval)
{
    aRetval.setNull();

    if (!mReferent) {
        return NS_OK;
    }

    nsCOMPtr<nsISupports> supports = do_QueryReferent(mReferent);
    if (!supports) {
        return NS_OK;
    }

    nsCOMPtr<nsIXPConnectWrappedJS> wrappedObj = do_QueryInterface(supports);
    if (!wrappedObj) {
        // We have a generic XPCOM object that supports weak references here.
        // Wrap it and pass it out.
        return nsContentUtils::WrapNative(aCx, supports,
                                          &NS_GET_IID(nsISupports),
                                          aRetval);
    }

    JS::RootedObject obj(aCx, wrappedObj->GetJSObject());
    if (!obj) {
        return NS_OK;
    }

    // Most users of XPCWrappedJS don't need to worry about
    // re-wrapping because things are implicitly rewrapped by
    // xpcconvert. However, because we're doing this directly
    // through the native call context, we need to call
    // JS_WrapObject().
    if (!JS_WrapObject(aCx, &obj)) {
        return NS_ERROR_FAILURE;
    }

    aRetval.setObject(*obj);
    return NS_OK;
}
