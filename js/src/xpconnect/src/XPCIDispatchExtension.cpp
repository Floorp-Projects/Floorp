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
CommonConstructor(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, 
                     jsval *rval, PRBool testScriptability)
{
    // Make sure we were called with one string parameter
    if(argc != 1 || (argc == 1 && !JSVAL_IS_STRING(argv[0])))
    {
        XPCThrower::Throw(NS_ERROR_XPC_COM_INVALID_CLASS_ID, cx);
        return JS_FALSE;
    }

    const char * bytes = xpc_JSString2Char(cx, argv[0]);
    if(!bytes)
    {
        XPCThrower::Throw(NS_ERROR_XPC_COM_INVALID_CLASS_ID, cx);
        return JS_FALSE;
    }

    // Instantiate the desired COM object
    IDispatch* pDispatch = nsnull;;
    HRESULT rv = XPCDispObject::COMCreateInstance(bytes, testScriptability, &pDispatch);
    if(FAILED(rv))
    {
        XPCThrower::ThrowCOMError(cx, rv, NS_ERROR_XPC_COM_CREATE_FAILED);
        return JS_FALSE;
    }
    // Get a wrapper for our object
    nsCOMPtr<nsIXPConnectJSObjectHolder> holder;
    nsresult nsrv = nsXPConnect::GetXPConnect()->WrapNative(cx, obj, 
                                  NS_REINTERPRET_CAST(nsISupports*, pDispatch),
                                  NSID_IDISPATCH, getter_AddRefs(holder));
    if(NS_FAILED(nsrv))
    {
        XPCThrower::Throw(nsrv, cx);
        return JS_FALSE;
    }
    // get and return the JS object wrapper
    JSObject * jsobj;
    nsrv = holder->GetJSObject(&jsobj);
    if(NS_FAILED(nsrv))
    {
        XPCThrower::Throw(nsrv, cx);
        return JS_FALSE;
    }
    *rval = OBJECT_TO_JSVAL(jsobj);
    return JS_TRUE;
}

JS_STATIC_DLL_CALLBACK(JSBool)
COMObjectConstructor(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, 
                     jsval *rval)
{
    return CommonConstructor(cx, obj, argc, argv, rval, PR_FALSE);
}

JS_STATIC_DLL_CALLBACK(JSBool)
ActiveXConstructor(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, 
                     jsval *rval)
{
    return CommonConstructor(cx, obj, argc, argv, rval, PR_TRUE);
}

JSBool XPCIDispatchExtension::Initialize(JSContext * aJSContext,
                                  JSObject * aGlobalJSObj)
{
    // TODO: Cleanup error code
    JSBool result = JS_DefineFunction(aJSContext, aGlobalJSObj, "ActiveXObject", 
                                      ActiveXConstructor, 1, 0) != nsnull;
#ifdef XPC_COMOBJECT
    if(result)
       result = JS_DefineFunction(aJSContext, aGlobalJSObj, "COMObject", 
                                  COMObjectConstructor, 1, 0) != nsnull;
#endif
    return result;
}

JSBool XPCIDispatchExtension::DefineProperty(XPCCallContext & ccx, 
                                             JSObject *obj, jsval idval,
                                             XPCWrappedNative* wrapperToReflectInterfaceNames,
                                             uintN propFlags, JSBool* resolved)
{
    // Look up the native interface for IDispatch and then find a tearoff
    XPCNativeInterface* iface = XPCNativeInterface::GetNewOrUsed(ccx,
                                                                 "IDispatch");
    if(iface == nsnull)
        return JS_FALSE;
    XPCWrappedNativeTearOff* to = 
        wrapperToReflectInterfaceNames->FindTearOff(ccx, iface, JS_TRUE);
    if(to == nsnull)
        return JS_FALSE;
    // get the JS Object for the tea
    JSObject* jso = to->GetJSObject();
    if(jso == nsnull)
        return JS_FALSE;
    // Look up the member in the interface
    if(!JSVAL_IS_STRING(idval))
        return JS_FALSE;
    const XPCDispInterface::Member * member = to->GetIDispatchInfo()->FindMember(idval);
    if(!member)
    {
        // IDispatch is case insensitive, so if we don't find a case sensitive
        // match, we'll try a more expensive case-insensisitive search
        // TODO: We need to create cleaner solution that doesn't create
        // multiple properties of different case on the JS Object
        member = to->GetIDispatchInfo()->FindMemberCI(ccx, idval);
        if(!member)
            return JS_FALSE;
    }
    // Get the function object
    jsval funval;
    if(!member->GetValue(ccx, iface, &funval))
        return JS_FALSE;
    JSObject* funobj = JS_CloneFunctionObject(ccx, JSVAL_TO_OBJECT(funval), obj);
    if(!funobj)
        return JS_FALSE;
    jsid id;
    // If this is a function or a parameterized property
    if(member->IsFunction() || member->IsParameterizedProperty())
    {
        // define the function on the object
        AutoResolveName arn(ccx, idval);
        if(resolved)
            *resolved = JS_TRUE;
        return JS_ValueToId(ccx, idval, &id) &&
               OBJ_DEFINE_PROPERTY(ccx, obj, id, OBJECT_TO_JSVAL(funobj),
                                   nsnull, nsnull, propFlags, nsnull);
    }
    // Define the property on the object
    NS_ASSERTION(member->IsProperty(), "way broken!");
    propFlags |= JSPROP_GETTER | JSPROP_SHARED;
    if(member->IsSetter())
    {
        propFlags |= JSPROP_SETTER;
        propFlags &= ~JSPROP_READONLY;
    }
    AutoResolveName arn(ccx, idval);
    if(resolved)
        *resolved = JS_TRUE;
    return JS_ValueToId(ccx, idval, &id) &&
           OBJ_DEFINE_PROPERTY(ccx, obj, id, JSVAL_VOID,
                               (JSPropertyOp) funobj,
                               (JSPropertyOp) funobj,
                               propFlags, nsnull);

}

JSBool XPCIDispatchExtension::Enumerate(XPCCallContext& ccx, JSObject* obj,
                                        XPCWrappedNative * wrapper)
{
    XPCNativeInterface* iface = XPCNativeInterface::GetNewOrUsed(
        ccx,&NSID_IDISPATCH);
    if(!iface)
        return JS_FALSE;

    XPCWrappedNativeTearOff* tearoff = wrapper->FindTearOff(ccx, iface);
    if(!tearoff)
        return JS_FALSE;

    XPCDispInterface* pInfo = tearoff->GetIDispatchInfo();
    PRUint32 members = pInfo->GetMemberCount();
    // Iterate over the members and force the properties to be resolved
    for(PRUint32 index = 0; index < members; ++index)
    {
        const XPCDispInterface::Member & member = pInfo->GetMember(index);
        jsval name = member.GetName();
        if(!xpc_ForcePropertyResolve(ccx, obj, name))
            return JS_FALSE;
    }
    return JS_TRUE;
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
    *aInstancePtr = NS_STATIC_CAST(XPCDispatchTearOff*, new XPCDispatchTearOff(root));
    return NS_OK;
}
