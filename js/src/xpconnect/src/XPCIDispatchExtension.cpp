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

JS_STATIC_DLL_CALLBACK(JSBool)
COMObjectConstructor(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, 
                     jsval *rval)
{
    // Make sure we were called with one string parameter
    if(argc != 1 || (argc == 1 && !JSVAL_IS_STRING(argv[0])))
    {
        return JS_FALSE;
    }

    JSString * str = JS_ValueToString(cx, argv[0]);
    if(!str)
    {
        // TODO: error reporting
        return JS_FALSE;
    }

    const char * bytes = JS_GetStringBytes(str);
    if(!bytes)
    {
        // TODO: error reporting
        return JS_FALSE;
    }

    // Instantiate the desired COM object
    IDispatch* pDispatch = XPCDispObject::COMCreateInstance(bytes);
    nsCOMPtr<nsIXPConnectJSObjectHolder> holder;
    nsresult rv = nsXPConnect::GetXPConnect()->WrapNative(cx, obj, 
                                  NS_REINTERPRET_CAST(nsISupports*, pDispatch),
                                  NSID_IDISPATCH, getter_AddRefs(holder));
    if(FAILED(rv) || !holder)
    {
        // TODO: error reporting
        return JS_FALSE;
    }
    JSObject * jsobj;
    if(NS_FAILED(holder->GetJSObject(&jsobj)))
        return JS_FALSE;
    *rval = OBJECT_TO_JSVAL(jsobj);
    return JS_TRUE;
}

JSBool XPCIDispatchExtension::Initialize(JSContext * aJSContext,
                                  JSObject * aGlobalJSObj)
{
    // TODO: Cleanup error code
    return JS_DefineFunction(aJSContext, aGlobalJSObj, "COMObject", 
                         COMObjectConstructor, 1, 0) ? PR_TRUE : PR_FALSE;
}

JSBool XPCIDispatchExtension::DefineProperty(XPCCallContext & ccx, JSObject *obj, jsval idval,
                     XPCWrappedNative* wrapperToReflectInterfaceNames,
                     uintN propFlags, JSBool* resolved)
{
    XPCNativeInterface* iface = XPCNativeInterface::GetNewOrUsed(ccx, "IDispatch");
    if (iface == nsnull)
        return JS_FALSE;
    XPCWrappedNativeTearOff* to = 
        wrapperToReflectInterfaceNames->FindTearOff(ccx, iface, JS_TRUE);
    if (to == nsnull)
        return JS_FALSE;

    JSObject* jso = to->GetJSObject();
    if (jso == nsnull)
        return JS_FALSE;

    const XPCDispInterface::Member * member = to->GetIDispatchInfo()->FindMember(idval);
    if (member == nsnull)
        return JS_FALSE;
    jsval funval;
    if(!member->GetValue(ccx, iface, &funval))
        return JS_FALSE;
    JSObject* funobj = JS_CloneFunctionObject(ccx, JSVAL_TO_OBJECT(funval), obj);
    if(!funobj)
        return JS_FALSE;
    jsid id;
    if (member->IsFunction())
    {
        AutoResolveName arn(ccx, idval);
        if(resolved)
            *resolved = JS_TRUE;
        return JS_ValueToId(ccx, idval, &id) &&
               OBJ_DEFINE_PROPERTY(ccx, obj, id, OBJECT_TO_JSVAL(funobj),
                                   nsnull, nsnull, propFlags, nsnull);
    }
    NS_ASSERTION(!member || member->IsSetter(), "way broken!");
    propFlags |= JSPROP_GETTER | JSPROP_SHARED;
    if (member->IsSetter())
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
    if(iface)
    {
        XPCWrappedNativeTearOff* tearoff = wrapper->FindTearOff(ccx, iface);
        if(tearoff)
        {
            XPCDispInterface* pInfo = tearoff->GetIDispatchInfo();
            PRUint32 members = pInfo->GetMemberCount();
            for(PRUint32 index = 0; index < members; ++index)
            {
                const XPCDispInterface::Member & member = pInfo->GetMember(index);
                jsval name = member.GetName();
                if(!xpc_ForcePropertyResolve(ccx, obj, name))
                    return JS_FALSE;
            }
            return JS_TRUE;
        }
    }
    return JS_FALSE;
}

nsresult XPCIDispatchExtension::IDispatchQIWrappedJS(nsXPCWrappedJS * self, 
                                                     void ** aInstancePtr)
{
    nsXPCWrappedJS* root = self->GetRootWrapper();

    if(!root->IsValid())
    {
        *aInstancePtr = nsnull;
        return NS_NOINTERFACE;
    }
    *aInstancePtr = NS_STATIC_CAST(XPCDispatchTearOff*, new XPCDispatchTearOff(root));
    return NS_OK;
}
