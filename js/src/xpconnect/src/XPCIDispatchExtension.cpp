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
 * The Original Code is the IDispatch implementation for XPConnect
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

#include "xpcprivate.h"

static const char* const IDISPATCH_NAME = "IDispatch";

PRBool XPCIDispatchExtension::mIsEnabled = PR_TRUE;

static JSBool
CommonConstructor(JSContext *cx, int name, JSObject *obj, uintN argc,
                  jsval *argv, jsval *rval, PRBool enforceSecurity)
{
    XPCCallContext ccx(JS_CALLER, cx, JS_GetGlobalObject(cx));
    XPCJSRuntime *rt = ccx.GetRuntime();
    if (!rt)
    {
        XPCThrower::Throw(NS_ERROR_UNEXPECTED, ccx);
        return JS_FALSE;
    } 
    nsIXPCSecurityManager* sm = ccx.GetXPCContext()
        ->GetAppropriateSecurityManager(nsIXPCSecurityManager::HOOK_CALL_METHOD);
    XPCWrappedNative * wrapper = ccx.GetWrapper();
    if(sm && NS_FAILED(sm->CanAccess(nsIXPCSecurityManager::ACCESS_CALL_METHOD,
                                      &ccx, ccx, ccx.GetFlattenedJSObject(),
                                      wrapper->GetIdentityObject(),
                                      wrapper->GetClassInfo(),
                                      rt->GetStringJSVal(name),
                                      wrapper->GetSecurityInfoAddr())))
    {
        // Security manager will have set an exception
        return JS_FALSE;
    }
    // Check if IDispatch is enabled, fail if not
    if(!nsXPConnect::IsIDispatchEnabled())
    {
        XPCThrower::Throw(NS_ERROR_XPC_IDISPATCH_NOT_ENABLED, ccx);
        return JS_FALSE;
    }
    // Make sure we were called with one string parameter
    if(argc != 1 || (argc == 1 && !JSVAL_IS_STRING(argv[0])))
    {
        XPCThrower::Throw(NS_ERROR_XPC_COM_INVALID_CLASS_ID, ccx);
        return JS_FALSE;
    }
    PRUint32 len;
    jschar * className = xpc_JSString2String(ccx, argv[0], &len);
    CComBSTR bstrClassName(len, className);
    if(!className)
    {
        XPCThrower::Throw(NS_ERROR_XPC_COM_INVALID_CLASS_ID, ccx);
        return JS_FALSE;
    }
    // Instantiate the desired COM object
    CComPtr<IDispatch> pDispatch;
    HRESULT rv = XPCDispObject::COMCreateInstance(ccx, bstrClassName,
                                                  enforceSecurity, &pDispatch);
    if(FAILED(rv))
    {
        XPCThrower::ThrowCOMError(ccx, rv, NS_ERROR_XPC_COM_CREATE_FAILED);
        return JS_FALSE;
    }
    // Get a wrapper for our object
    nsCOMPtr<nsIXPConnectJSObjectHolder> holder;
    nsresult nsrv = ccx.GetXPConnect()->WrapNative(
        ccx, ccx.GetOperandJSObject(), NS_REINTERPRET_CAST(nsISupports*, pDispatch.p),
        NSID_IDISPATCH, getter_AddRefs(holder));
    if(NS_FAILED(nsrv))
    {
        XPCThrower::Throw(nsrv, ccx);
        return JS_FALSE;
    }
    // get and return the JS object wrapper
    JSObject * jsobj;
    nsrv = holder->GetJSObject(&jsobj);
    if(NS_FAILED(nsrv))
    {
        XPCThrower::Throw(nsrv, ccx);
        return JS_FALSE;
    }
    *rval = OBJECT_TO_JSVAL(jsobj);
    return JS_TRUE;
}

JS_STATIC_DLL_CALLBACK(JSBool)
COMObjectConstructor(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, 
                     jsval *rval)
{
    return CommonConstructor(cx, XPCJSRuntime::IDX_COM_OBJECT, obj, argc,
                             argv, rval, PR_FALSE);
}

JS_STATIC_DLL_CALLBACK(JSBool)
ActiveXConstructor(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, 
                   jsval *rval)
{
    return CommonConstructor(cx, XPCJSRuntime::IDX_ACTIVEX_OBJECT, obj, argc, argv,
                             rval, PR_TRUE);
}

JSBool XPCIDispatchExtension::Initialize(JSContext * aJSContext,
                                         JSObject * aGlobalJSObj)
{
    JSBool result = JS_DefineFunction(aJSContext, aGlobalJSObj, 
                                      nsXPConnect::GetRuntime()->GetStringName(
                                          XPCJSRuntime::IDX_ACTIVEX_OBJECT),
                                      ActiveXConstructor, 1, 0) != nsnull;
#ifdef XPC_COMOBJECT
    if(result)
       result = JS_DefineFunction(aJSContext, aGlobalJSObj, 
                                  nsXPConnect::GetRuntime()->GetStringName(
                                      XPCJSRuntime::IDX_COM_OBJECT),
                                  COMObjectConstructor, 1, 0) != nsnull;
#endif
    return result;
}

nsresult XPCIDispatchExtension::IDispatchQIWrappedJS(nsXPCWrappedJS * self, 
                                                     void ** aInstancePtr)
{
    // Lookup the root and create a tearoff based on that
    nsXPCWrappedJS* root = self->GetRootWrapper();

    if(!root->IsValid())
    {
        *aInstancePtr = nsnull;
        return NS_NOINTERFACE;
    }
    XPCDispatchTearOff* tearOff = new XPCDispatchTearOff(root);
    if(!tearOff)
        return NS_ERROR_OUT_OF_MEMORY;
    tearOff->AddRef();
    *aInstancePtr = tearOff;
    
    return NS_OK;
}
