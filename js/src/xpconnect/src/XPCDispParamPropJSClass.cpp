/* -*- Mode: C; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * ***** BEGIN LICENSE BLOCK *****
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
 * The Original Code is the IDispatch implementation for XPConnect.
 *
 * The Initial Developer of the Original Code is
 * David Bradley.
 * Portions created by the Initial Developer are Copyright (C) 2002
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
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

/** \file XPCDispParamPropJSClass.cpp
 * Implementation for the XPCDispParamPropJSClass class
 * This file contains the implementation of the XPCDispParamPropJSClass class
 */

#include "xpcprivate.h"

/**
 * Helper function to retrieve the parameterized property instance
 * from the JS object's private slot
 * @param cx a JS context
 * @param obj the JS object to retrieve the instance from
 * @return the parameterized property class
 */
inline
XPCDispParamPropJSClass* GetParamProp(JSContext* cx, JSObject* obj)
{
    return reinterpret_cast<XPCDispParamPropJSClass*>(xpc_GetJSPrivate(obj));
}

/**
 * Handles getting a property via a parameterized property.
 * This object is used as part of the parameterized property mechanism.
 * property get requests are forward to our owner and on to IDispatch's
 * Invoke
 * @param cx A pointer to a JS context
 * @param obj The object to perform the get on
 * @param id ID of the parameter to get
 * @param vp Pointer to the return value
 * @return JSBool JS_TRUE if property was retrieved
 */
static JSBool
XPC_PP_GetProperty(JSContext *cx, JSObject *obj, jsval id, jsval *vp)
{
    XPCDispParamPropJSClass* paramProp = GetParamProp(cx, obj);
    JSObject* originalObj = paramProp->GetWrapper()->GetFlatJSObject();
    XPCCallContext ccx(JS_CALLER, cx, originalObj, nsnull, id, 
                       paramProp->GetParams()->GetParamCount(), nsnull, vp);
    return paramProp->Invoke(ccx, XPCDispObject::CALL_GETTER, vp);
}

/**
 * Handles getting a property via a parameterized property.
 * This object is used as part of the parameterized property mechanism.
 * property get requests are forward to our owner and on to IDispatch's
 * Invoke
 * @param cx A pointer to a JS context
 * @param obj The object to perform the get on
 * @param id ID of the parameter to get
 * @param vp Pointer to the return value
 * @return JSBool JS_TRUE if property was retrieved
 */
static JSBool
XPC_PP_SetProperty(JSContext *cx, JSObject *obj, jsval id, jsval *vp)
{
    XPCDispParamPropJSClass* paramProp = GetParamProp(cx, obj);
    JSObject* originalObj = paramProp->GetWrapper()->GetFlatJSObject();
    XPCCallContext ccx(JS_CALLER, cx, originalObj, nsnull, id, 
                       paramProp->GetParams()->GetParamCount(), nsnull, vp);
    _variant_t var;
    uintN err;
    if(!XPCDispConvert::JSToCOM(ccx, *vp, var, err))
        return JS_FALSE;
    XPCDispParams* params = paramProp->GetParams();
    params->SetNamedPropID();
    // params will own var
    params->InsertParam(var);
    // Save off the value passed in so we can return it.
    // Invoke resets the value to what ever IDispatch returns, in this
    // case empty, converted to undefined
    jsval retJSVal = *vp;
    AUTO_MARK_JSVAL(ccx, retJSVal);
    if (paramProp->Invoke(ccx, XPCDispObject::CALL_SETTER, vp))
    {
        *vp = retJSVal;
        return JS_TRUE;
    }
    return JS_FALSE;
}

/**
 * Handles getting a property via a parameterized property.
 * This object is used as part of the parameterized property mechanism.
 * property get requests are forward to our owner and on to IDispatch's
 * Invoke
 * @param cx A pointer to a JS context
 * @param obj The object to perform the get on
 * @param id ID of the parameter to get
 * @param vp Pointer to the return value
 * @return JSBool JS_TRUE if property was retrieved
 */
static void
XPC_PP_Finalize(JSContext *cx, JSObject *obj)
{
    delete GetParamProp(cx, obj);
}

/**
 * Is called to trace things that the object holds.
 * @param trc the tracing structure
 * @param obj the object being marked
 */
static void
XPC_PP_Trace(JSTracer *trc, JSObject *obj)
{
    XPCDispParamPropJSClass* paramProp = GetParamProp(trc->context, obj);
    if(paramProp)
    {
        XPCWrappedNative* wrapper = paramProp->GetWrapper();
        if(wrapper && wrapper->IsValid())
            xpc_TraceForValidWrapper(trc, wrapper);
    }
}

/**
 * Our JSClass used by XPCDispParamPropClass
 * @see XPCDispParamPropClass
 */
static JSClass ParamPropClass = {
    "XPCDispParamPropJSCass",   // Name 
    JSCLASS_HAS_PRIVATE | JSCLASS_MARK_IS_TRACE, // flags

    /* Mandatory non-null function pointer members. */
    JS_PropertyStub,            // addProperty
    JS_PropertyStub,            // delProperty
    XPC_PP_GetProperty,         // getProperty
    XPC_PP_SetProperty,         // setProperty
    JS_EnumerateStub,           // enumerate
    JS_ResolveStub,             // resolve
    JS_ConvertStub,             // convert
    XPC_PP_Finalize,            // finalize

    /* Optionally non-null members start here. */
    nsnull,                     // getObjectOps;
    nsnull,                     // checkAccess;
    nsnull,                     // call;
    nsnull,                     // construct;
    nsnull,                     // xdrObject;
    nsnull,                     // hasInstance;
    JS_CLASS_TRACE(XPC_PP_Trace), // mark/trace;
    nsnull                      // spare;
};

// static
JSBool XPCDispParamPropJSClass::NewInstance(XPCCallContext& ccx,
                                             XPCWrappedNative* wrapper, 
                                             PRUint32 dispID, 
                                             XPCDispParams* dispParams, 
                                             jsval* paramPropObj)
{
    XPCDispParamPropJSClass* pDispParam =
        new XPCDispParamPropJSClass(wrapper, ccx.GetTearOff()->GetNative(), 
                                    dispID, dispParams);
    if(!pDispParam)
        return JS_FALSE;
    JSObject * obj = JS_NewObject(ccx, &ParamPropClass, nsnull, nsnull);
    if(!obj)
        return JS_FALSE;
    if(!JS_SetPrivate(ccx, obj, pDispParam))
        return JS_FALSE;
    *paramPropObj = OBJECT_TO_JSVAL(obj);
    return JS_TRUE;
}

XPCDispParamPropJSClass::XPCDispParamPropJSClass(XPCWrappedNative* wrapper, 
                                                 nsISupports * dispObj, 
                                                 PRUint32 dispID,
                                                 XPCDispParams* dispParams) :
    mWrapper(wrapper),
    mDispID(dispID),
    mDispParams(dispParams),
    mDispObj(nsnull)
{
    NS_ADDREF(mWrapper);
    dispObj->QueryInterface(NSID_IDISPATCH, 
                                              reinterpret_cast<void**>
                                                              (&mDispObj));
}

XPCDispParamPropJSClass::~XPCDispParamPropJSClass()
{
    delete mDispParams;
    // release our members
    NS_IF_RELEASE(mWrapper);
    NS_IF_RELEASE(mDispObj);
}
