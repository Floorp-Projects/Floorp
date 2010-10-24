/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
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
#include "xpcJSWeakReference.h"

xpcJSWeakReference::xpcJSWeakReference()
{
}

NS_IMPL_ISUPPORTS1(xpcJSWeakReference, xpcIJSWeakReference)

nsresult xpcJSWeakReference::Init()
{
    nsresult rv;
    
    nsXPConnect* xpc = nsXPConnect::GetXPConnect();
    if (!xpc) return NS_ERROR_UNEXPECTED;
    
    nsAXPCNativeCallContext *cc = nsnull;
    rv = xpc->GetCurrentNativeCallContext(&cc);
    NS_ENSURE_SUCCESS(rv, rv);

    JSContext *cx = nsnull;
    rv = cc->GetJSContext(&cx);
    NS_ENSURE_SUCCESS(rv, rv);

    PRUint32 argc = 0;
    rv = cc->GetArgc(&argc);
    NS_ENSURE_SUCCESS(rv, rv);

    if (argc != 1) return NS_ERROR_FAILURE;
    
    jsval *argv = nsnull;
    rv = cc->GetArgvPtr(&argv);
    NS_ENSURE_SUCCESS(rv, rv);

    JSAutoRequest ar(cx);

    if (JSVAL_IS_NULL(argv[0])) return NS_ERROR_FAILURE;
    
    JSObject *obj;
    if (!JS_ValueToObject(cx, argv[0], &obj))
        return NS_ERROR_FAILURE;
    
    XPCCallContext ccx(NATIVE_CALLER, cx);
    
    nsRefPtr<nsXPCWrappedJS> wrapped;
    rv = nsXPCWrappedJS::GetNewOrUsed(ccx,
                                      obj,
                                      NS_GET_IID(nsISupports),
                                      nsnull,
                                      getter_AddRefs(wrapped));
    if (!wrapped) {
        NS_ERROR("can't get nsISupportsWeakReference wrapper for obj");
        return rv;
    }

    return static_cast<nsISupportsWeakReference*>(wrapped)->
        GetWeakReference(getter_AddRefs(mWrappedJSObject));
}

NS_IMETHODIMP
xpcJSWeakReference::Get()
{
    nsresult rv;

    nsXPConnect* xpc = nsXPConnect::GetXPConnect();
    if (!xpc)
        return NS_ERROR_UNEXPECTED;

    nsAXPCNativeCallContext* cc = nsnull;
    rv = xpc->GetCurrentNativeCallContext(&cc);
    NS_ENSURE_SUCCESS(rv, rv);

    JSContext *cx;
    cc->GetJSContext(&cx);
    if (!cx)
        return NS_ERROR_UNEXPECTED;

    jsval *retval = nsnull;
    cc->GetRetValPtr(&retval);
    if (!retval)
        return NS_ERROR_UNEXPECTED;
    *retval = JSVAL_NULL;

    nsCOMPtr<nsIXPConnectWrappedJS> wrappedObj;

    if (mWrappedJSObject &&
        NS_SUCCEEDED(mWrappedJSObject->QueryReferent(NS_GET_IID(nsIXPConnectWrappedJS), getter_AddRefs(wrappedObj))) &&
        wrappedObj) {
        JSObject *obj;
        wrappedObj->GetJSObject(&obj);
        if (obj)
        {
            // Most users of XPCWrappedJS don't need to worry about
            // re-wrapping because things are implicitly rewrapped by
            // xpcconvert. However, because we're doing this directly
            // through the native call context, we need to call
            // JS_WrapObject().

            if (!JS_WrapObject(cx, &obj))
            {
                return NS_ERROR_FAILURE;
            }

            *retval = OBJECT_TO_JSVAL(obj);
        }
    }

    return NS_OK;
}
