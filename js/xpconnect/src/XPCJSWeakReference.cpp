/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* ***** BEGIN LICENSE BLOCK *****
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
 * The Original Code is Mozilla code.
 *
 * The Initial Developer of the Original Code is Alex Fritze.
 * Portions created by the Initial Developer are Copyright (C) 2004
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Alex Fritze <alex@croczilla.com> (original author)
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
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

#include "xpcprivate.h"
#include "XPCJSWeakReference.h"

#include "nsContentUtils.h"

xpcJSWeakReference::xpcJSWeakReference()
{
}

NS_IMPL_ISUPPORTS1(xpcJSWeakReference, xpcIJSWeakReference)

nsresult xpcJSWeakReference::Init(JSContext* cx, const JS::Value& object)
{
    JSAutoRequest ar(cx);

    if (!object.isObject())
        return NS_OK;

    JSObject& obj = object.toObject();

    XPCCallContext ccx(NATIVE_CALLER, cx);

    // See if the object is a wrapped native that supports weak references.
    nsISupports* supports =
        nsXPConnect::GetXPConnect()->GetNativeOfWrapper(cx, &obj);
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
    nsRefPtr<nsXPCWrappedJS> wrapped;
    nsresult rv = nsXPCWrappedJS::GetNewOrUsed(ccx,
                                               &obj,
                                               NS_GET_IID(nsISupports),
                                               nsnull,
                                               getter_AddRefs(wrapped));
    if (!wrapped) {
        NS_ERROR("can't get nsISupportsWeakReference wrapper for obj");
        return rv;
    }

    return wrapped->GetWeakReference(getter_AddRefs(mReferent));
}

NS_IMETHODIMP
xpcJSWeakReference::Get(JSContext* aCx, JS::Value* aRetval)
{
    *aRetval = JSVAL_NULL;

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
        return nsContentUtils::WrapNative(aCx, JS_GetGlobalForScopeChain(aCx),
                                          supports, &NS_GET_IID(nsISupports),
                                          aRetval);
    }

    JSObject *obj;
    wrappedObj->GetJSObject(&obj);
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

    *aRetval = OBJECT_TO_JSVAL(obj);
    return NS_OK;
}
