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
 * Copyright (C) 1999 Netscape Communications Corporation. All
 * Rights Reserved.
 *
 * Contributor(s): 
 *   John Bandhauer <jband@netscape.com>
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

/* Implements nsXPCArbitraryScriptable */

#include "xpcprivate.h"

extern "C" JS_IMPORT_DATA(JSObjectOps) js_ObjectOps;

NS_IMPL_THREADSAFE_ISUPPORTS1(nsXPCArbitraryScriptable, nsIXPCScriptable)

NS_IMETHODIMP
nsXPCArbitraryScriptable::Create(JSContext *cx, JSObject *obj,
                  nsIXPConnectWrappedNative* wrapper,
                  nsIXPCScriptable* arbitrary)
{
    NS_PRECONDITION(wrapper, "bad param");
    NS_PRECONDITION(cx, "bad param");
    NS_PRECONDITION(obj, "bad param");
    return NS_OK;
}

NS_IMETHODIMP
nsXPCArbitraryScriptable::GetFlags(JSContext *cx, JSObject *obj,
                  nsIXPConnectWrappedNative* wrapper,
                  JSUint32* flagsp,
                  nsIXPCScriptable* arbitrary)
{
    NS_PRECONDITION(wrapper, "bad param");
    NS_PRECONDITION(cx, "bad param");
    NS_PRECONDITION(obj, "bad param");
    NS_PRECONDITION(flagsp, "bad param");
    *flagsp = 0;
    return NS_OK;
}

NS_IMETHODIMP
nsXPCArbitraryScriptable::LookupProperty(JSContext *cx, JSObject *obj, jsid id,
                          JSObject **objp, JSProperty **propp,
                          nsIXPConnectWrappedNative* wrapper,
                          nsIXPCScriptable* arbitrary,
                          JSBool* retval)
{
    NS_PRECONDITION(wrapper, "bad param");
    NS_PRECONDITION(retval, "bad param");
    NS_PRECONDITION(cx, "bad param");
    NS_PRECONDITION(obj, "bad param");
    *retval = js_ObjectOps.lookupProperty(cx, obj, id, objp, propp
#if defined JS_THREADSAFE && defined DEBUG
			    , "unknown file", 1
#endif
                            );
    return NS_OK;
}

NS_IMETHODIMP
nsXPCArbitraryScriptable::DefineProperty(JSContext *cx, JSObject *obj,
                          jsid id, jsval value,
                          JSPropertyOp getter, JSPropertyOp setter,
                          uintN attrs, JSProperty **propp,
                          nsIXPConnectWrappedNative* wrapper,
                          nsIXPCScriptable* arbitrary,
                          JSBool* retval)
{
    NS_PRECONDITION(wrapper, "bad param");
    NS_PRECONDITION(retval, "bad param");
    NS_PRECONDITION(cx, "bad param");
    NS_PRECONDITION(obj, "bad param");
    *retval = js_ObjectOps.defineProperty(cx, obj, id, value, getter, setter,
                                          attrs, propp);
    return NS_OK;
}

NS_IMETHODIMP
nsXPCArbitraryScriptable::GetProperty(JSContext *cx, JSObject *obj,
                       jsid id, jsval *vp,
                       nsIXPConnectWrappedNative* wrapper,
                       nsIXPCScriptable* arbitrary,
                       JSBool* retval)
{
    NS_PRECONDITION(wrapper, "bad param");
    NS_PRECONDITION(retval, "bad param");
    NS_PRECONDITION(cx, "bad param");
    NS_PRECONDITION(obj, "bad param");
    *retval = js_ObjectOps.getProperty(cx, obj, id, vp);
    return NS_OK;
}

NS_IMETHODIMP
nsXPCArbitraryScriptable::SetProperty(JSContext *cx, JSObject *obj,
                       jsid id, jsval *vp,
                       nsIXPConnectWrappedNative* wrapper,
                       nsIXPCScriptable* arbitrary,
                       JSBool* retval)
{
    NS_PRECONDITION(wrapper, "bad param");
    NS_PRECONDITION(retval, "bad param");
    NS_PRECONDITION(cx, "bad param");
    NS_PRECONDITION(obj, "bad param");
    *retval = js_ObjectOps.setProperty(cx, obj, id, vp);
    return NS_OK;
}

NS_IMETHODIMP
nsXPCArbitraryScriptable::GetAttributes(JSContext *cx, JSObject *obj, jsid id,
                         JSProperty *prop, uintN *attrsp,
                         nsIXPConnectWrappedNative* wrapper,
                         nsIXPCScriptable* arbitrary,
                         JSBool* retval)
{
    NS_PRECONDITION(wrapper, "bad param");
    NS_PRECONDITION(retval, "bad param");
    NS_PRECONDITION(cx, "bad param");
    NS_PRECONDITION(obj, "bad param");
    *retval = js_ObjectOps.getAttributes(cx, obj, id, prop, attrsp);
    return NS_OK;
}

NS_IMETHODIMP
nsXPCArbitraryScriptable::SetAttributes(JSContext *cx, JSObject *obj, jsid id,
                         JSProperty *prop, uintN *attrsp,
                         nsIXPConnectWrappedNative* wrapper,
                         nsIXPCScriptable* arbitrary,
                         JSBool* retval)
{
    NS_PRECONDITION(wrapper, "bad param");
    NS_PRECONDITION(retval, "bad param");
    NS_PRECONDITION(cx, "bad param");
    NS_PRECONDITION(obj, "bad param");
    *retval = js_ObjectOps.setAttributes(cx, obj, id, prop, attrsp);
    return NS_OK;
}

NS_IMETHODIMP
nsXPCArbitraryScriptable::DeleteProperty(JSContext *cx, JSObject *obj,
                          jsid id, jsval *vp,
                          nsIXPConnectWrappedNative* wrapper,
                          nsIXPCScriptable* arbitrary,
                          JSBool* retval)
{
    NS_PRECONDITION(wrapper, "bad param");
    NS_PRECONDITION(retval, "bad param");
    NS_PRECONDITION(cx, "bad param");
    NS_PRECONDITION(obj, "bad param");
    *retval = js_ObjectOps.deleteProperty(cx, obj, id, vp);
    return NS_OK;
}

NS_IMETHODIMP
nsXPCArbitraryScriptable::DefaultValue(JSContext *cx, JSObject *obj,
                        JSType type, jsval *vp,
                        nsIXPConnectWrappedNative* wrapper,
                        nsIXPCScriptable* arbitrary,
                        JSBool* retval)
{
    NS_PRECONDITION(wrapper, "bad param");
    NS_PRECONDITION(retval, "bad param");
    NS_PRECONDITION(cx, "bad param");
    NS_PRECONDITION(obj, "bad param");
    *retval = js_ObjectOps.defaultValue(cx, obj, type, vp);
    return NS_OK;
}

NS_IMETHODIMP
nsXPCArbitraryScriptable::Enumerate(JSContext *cx, JSObject *obj,
                     JSIterateOp enum_op,
                     jsval *statep, jsid *idp,
                     nsIXPConnectWrappedNative* wrapper,
                     nsIXPCScriptable* arbitrary,
                     JSBool* retval)
{
    NS_PRECONDITION(wrapper, "bad param");
    NS_PRECONDITION(retval, "bad param");
    NS_PRECONDITION(cx, "bad param");
    NS_PRECONDITION(obj, "bad param");
    *retval = js_ObjectOps.enumerate(cx, obj, enum_op, statep, idp);
    return NS_OK;
}

NS_IMETHODIMP
nsXPCArbitraryScriptable::CheckAccess(JSContext *cx, JSObject *obj, jsid id,
                       JSAccessMode mode, jsval *vp, uintN *attrsp,
                       nsIXPConnectWrappedNative* wrapper,
                       nsIXPCScriptable* arbitrary,
                       JSBool* retval)
{
    NS_PRECONDITION(wrapper, "bad param");
    NS_PRECONDITION(retval, "bad param");
    NS_PRECONDITION(cx, "bad param");
    NS_PRECONDITION(obj, "bad param");
    *retval = js_ObjectOps.checkAccess(cx, obj, id, mode, vp, attrsp);
    return NS_OK;
}

NS_IMETHODIMP
nsXPCArbitraryScriptable::Call(JSContext *cx, JSObject *obj,
                uintN argc, jsval *argv,
                jsval *rval,
                nsIXPConnectWrappedNative* wrapper,
                nsIXPCScriptable* arbitrary,
                JSBool* retval)
{
    NS_PRECONDITION(wrapper, "bad param");
    NS_PRECONDITION(retval, "bad param");
    NS_PRECONDITION(cx, "bad param");
    NS_PRECONDITION(obj, "bad param");
    *retval = js_ObjectOps.call(cx, obj, argc, argv, rval);
    return NS_OK;
}

NS_IMETHODIMP
nsXPCArbitraryScriptable::Construct(JSContext *cx, JSObject *obj,
                     uintN argc, jsval *argv,
                     jsval *rval,
                     nsIXPConnectWrappedNative* wrapper,
                     nsIXPCScriptable* arbitrary,
                     JSBool* retval)
{
    NS_PRECONDITION(wrapper, "bad param");
    NS_PRECONDITION(retval, "bad param");
    NS_PRECONDITION(cx, "bad param");
    NS_PRECONDITION(obj, "bad param");
    *retval = js_ObjectOps.construct(cx, obj, argc, argv, rval);
    return NS_OK;
}

NS_IMETHODIMP
nsXPCArbitraryScriptable::HasInstance(JSContext *cx, JSObject *obj,
                           jsval v, JSBool *bp,
                           nsIXPConnectWrappedNative* wrapper,
                           nsIXPCScriptable* arbitrary,
                           JSBool* retval)
{
    NS_PRECONDITION(wrapper, "bad param");
    NS_PRECONDITION(bp, "bad param");
    NS_PRECONDITION(retval, "bad param");
    NS_PRECONDITION(cx, "bad param");
    NS_PRECONDITION(obj, "bad param");
    *retval = js_ObjectOps.hasInstance(cx, obj, v, bp);
    return NS_OK;
}

NS_IMETHODIMP
nsXPCArbitraryScriptable::Finalize(JSContext *cx, JSObject *obj,
                    nsIXPConnectWrappedNative* wrapper,
                    nsIXPCScriptable* arbitrary)
{
    NS_PRECONDITION(wrapper, "bad param");
    NS_PRECONDITION(cx, "bad param");
    NS_PRECONDITION(obj, "bad param");
    /* XPConnect does the finalization on the wrapper itself anyway */
    return NS_OK;
}
