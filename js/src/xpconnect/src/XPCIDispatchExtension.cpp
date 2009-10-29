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

#include "xpcprivate.h"

static const char* const IDISPATCH_NAME = "IDispatch";

#define XPC_IDISPATCH_CTOR_MAX_ARG_LEN 2000

PRBool XPCIDispatchExtension::mIsEnabled = PR_TRUE;

JSBool XPCIDispatchExtension::DefineProperty(XPCCallContext & ccx, 
                                             JSObject *obj, jsval idval,
                                             XPCWrappedNative* wrapperToReflectInterfaceNames,
                                             uintN propFlags, JSBool* resolved)
{
    if(!JSVAL_IS_STRING(idval))
        return JS_FALSE;
    // Look up the native interface for IDispatch and then find a tearoff
    XPCNativeInterface* iface = XPCNativeInterface::GetNewOrUsed(ccx,
                                                                 "IDispatch");
    // The native interface isn't defined so just exit with an error
    if(iface == nsnull)
        return JS_FALSE;
    XPCWrappedNativeTearOff* to = 
        wrapperToReflectInterfaceNames->LocateTearOff(ccx, iface);
    // This object has no IDispatch interface so bail.
    if(to == nsnull)
        return JS_FALSE;
    // Look up the member in the interface
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
    // Protect the jsval 
    AUTO_MARK_JSVAL(ccx, funval);
    // clone a function we can use for this object 
    JSObject* funobj = xpc_CloneJSFunction(ccx, JSVAL_TO_OBJECT(funval), obj);
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
               JS_DefinePropertyById(ccx, obj, id, OBJECT_TO_JSVAL(funobj),
                                     nsnull, nsnull, propFlags);
    }
    // Define the property on the object
    NS_ASSERTION(member->IsProperty(), "way broken!");
    propFlags |= JSPROP_GETTER | JSPROP_SHARED;
    JSPropertyOp getter = JS_DATA_TO_FUNC_PTR(JSPropertyOp, funobj);
    JSPropertyOp setter;
    if(member->IsSetter())
    {
        propFlags |= JSPROP_SETTER;
        propFlags &= ~JSPROP_READONLY;
        setter = getter;
    }
    else
    {
        setter = js_GetterOnlyPropertyStub;
    }
    AutoResolveName arn(ccx, idval);
    if(resolved)
        *resolved = JS_TRUE;
    return JS_ValueToId(ccx, idval, &id) &&
           JS_DefinePropertyById(ccx, obj, id, JSVAL_VOID, getter, setter,
                                 propFlags);

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

    if(!tearoff->IsIDispatch())
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
    XPCDispatchTearOff* tearOff = new XPCDispatchTearOff(root);
    if(!tearOff)
        return NS_ERROR_OUT_OF_MEMORY;
    tearOff->AddRef();
    *aInstancePtr = tearOff;
    
    return NS_OK;
}
