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

#include "xpcprivate.h"
#include "nsCRT.h"

class XPCIDispatchScriptableHelper : public nsIXPCScriptable
{
public:
    /**
     * Returns a single instance of XPCIDispatchScriptableHelper
     * @return the lone instance
     */
    static XPCIDispatchScriptableHelper* GetSingleton();
    /**
     * Releases our hold on the instance
     */
    static void FreeSingleton();
    NS_DECL_ISUPPORTS
    NS_DECL_NSIXPCSCRIPTABLE
private:
    /**
     * Only our methods create and destroy instances
     */
    XPCIDispatchScriptableHelper();
    virtual ~XPCIDispatchScriptableHelper();

    static XPCIDispatchScriptableHelper*  sInstance;
};

XPCIDispatchScriptableHelper* XPCIDispatchScriptableHelper::sInstance = nsnull;

NS_IMPL_ISUPPORTS1(XPCIDispatchScriptableHelper, nsIXPCScriptable)

XPCIDispatchScriptableHelper * XPCIDispatchScriptableHelper::GetSingleton()
{
    if (!sInstance)
    {
        sInstance = new XPCIDispatchScriptableHelper;
        NS_IF_ADDREF(sInstance);
    }
    NS_IF_ADDREF(sInstance);
    return sInstance;
}

void XPCIDispatchScriptableHelper::FreeSingleton()
{
    NS_IF_RELEASE(sInstance);
}

XPCIDispatchScriptableHelper::XPCIDispatchScriptableHelper()
{
  /* member initializers and constructor code */
}

XPCIDispatchScriptableHelper::~XPCIDispatchScriptableHelper()
{
  /* destructor code */
}

/* readonly attribute string className; */
NS_IMETHODIMP
XPCIDispatchScriptableHelper::GetClassName(char * *aClassName)
{
    static const char className[] = "IDispatch";
    if(!aClassName)
        return NS_ERROR_NULL_POINTER;
    *aClassName = (char*) nsMemory::Clone(className, sizeof(className));
    return NS_OK;
}

/* readonly attribute PRUint32 scriptableFlags; */
NS_IMETHODIMP
XPCIDispatchScriptableHelper::GetScriptableFlags(PRUint32 *aScriptableFlags)
{
    *aScriptableFlags = DONT_SHARE_PROTOTYPE | DONT_ENUM_QUERY_INTERFACE |
                        WANT_ENUMERATE | WANT_NEWRESOLVE | 
                        ALLOW_PROP_MODS_DURING_RESOLVE | 
                        DONT_ASK_INSTANCE_FOR_SCRIPTABLE;
    return NS_OK;
}

/* void preCreate (in nsISupports nativeObj, in JSContextPtr cx, in JSObjectPtr globalObj, out JSObjectPtr parentObj); */
NS_IMETHODIMP
XPCIDispatchScriptableHelper::PreCreate(nsISupports *nativeObj, JSContext * cx,
                                        JSObject * globalObj,
                                        JSObject * *parentObj)
{
    return NS_ERROR_NOT_IMPLEMENTED;
}

/* void create (in nsIXPConnectWrappedNative wrapper, in JSContextPtr cx, in JSObjectPtr obj); */
NS_IMETHODIMP
XPCIDispatchScriptableHelper::Create(nsIXPConnectWrappedNative *wrapper,
                                     JSContext * cx, JSObject * obj)
{
    return NS_ERROR_NOT_IMPLEMENTED;
}

/* void postCreate (in nsIXPConnectWrappedNative wrapper, in JSContextPtr cx, in JSObjectPtr obj); */
NS_IMETHODIMP
XPCIDispatchScriptableHelper::PostCreate(nsIXPConnectWrappedNative *wrapper,
                                         JSContext * cx, JSObject * obj)
{
    return NS_ERROR_NOT_IMPLEMENTED;
}

/* PRBool addProperty (in nsIXPConnectWrappedNative wrapper, in JSContextPtr cx, in JSObjectPtr obj, in JSVal id, in JSValPtr vp); */
NS_IMETHODIMP
XPCIDispatchScriptableHelper::AddProperty(nsIXPConnectWrappedNative *wrapper,
                                          JSContext * cx, JSObject * obj,
                                          jsval id, jsval * vp, PRBool *retval)
{
    return NS_ERROR_NOT_IMPLEMENTED;
}

/* PRBool delProperty (in nsIXPConnectWrappedNative wrapper, in JSContextPtr cx, in JSObjectPtr obj, in JSVal id, in JSValPtr vp); */
NS_IMETHODIMP
XPCIDispatchScriptableHelper::DelProperty(nsIXPConnectWrappedNative *wrapper,
                                          JSContext * cx, JSObject * obj,
                                          jsval id, jsval * vp, PRBool *retval)
{
    return NS_ERROR_NOT_IMPLEMENTED;
}

/* PRBool getProperty (in nsIXPConnectWrappedNative wrapper, in JSContextPtr cx, in JSObjectPtr obj, in JSVal id, in JSValPtr vp); */
NS_IMETHODIMP
XPCIDispatchScriptableHelper::GetProperty(nsIXPConnectWrappedNative *wrapper,
                                          JSContext * cx, JSObject * obj,
                                          jsval id, jsval * vp, PRBool *retval)
{
    return NS_ERROR_NOT_IMPLEMENTED;
}

/* PRBool setProperty (in nsIXPConnectWrappedNative wrapper, in JSContextPtr cx, in JSObjectPtr obj, in JSVal id, in JSValPtr vp); */
NS_IMETHODIMP
XPCIDispatchScriptableHelper::SetProperty(nsIXPConnectWrappedNative *wrapper,
                                          JSContext * cx, JSObject * obj,
                                          jsval id, jsval * vp, PRBool *retval)
{
    return NS_ERROR_NOT_IMPLEMENTED;
}

/* PRBool enumerate (in nsIXPConnectWrappedNative wrapper, in JSContextPtr cx, in JSObjectPtr obj); */
NS_IMETHODIMP
XPCIDispatchScriptableHelper::Enumerate(nsIXPConnectWrappedNative *wrapper,
                                        JSContext * cx, JSObject * obj,
                                        PRBool *retval)
{
    XPCCallContext ccx(JS_CALLER, cx, obj);
    XPCNativeInterface* iface = XPCNativeInterface::GetNewOrUsed(
        ccx,&NSID_IDISPATCH);
    if(!iface)
        return JS_FALSE;

    XPCWrappedNativeTearOff* tearoff = ccx.GetWrapper()->FindTearOff(ccx, iface);
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
    *retval = PR_TRUE;
    return NS_OK;
}

/* PRBool newEnumerate (in nsIXPConnectWrappedNative wrapper, in JSContextPtr cx, in JSObjectPtr obj, in PRUint32 enum_op, in JSValPtr statep, out JSID idp); */
NS_IMETHODIMP
XPCIDispatchScriptableHelper::NewEnumerate(nsIXPConnectWrappedNative *wrapper,
                                           JSContext * cx, JSObject * obj,
                                           PRUint32 enum_op, jsval * statep,
                                           jsid *idp, PRBool *retval)
{
    return NS_ERROR_NOT_IMPLEMENTED;
}

/* PRBool newResolve (in nsIXPConnectWrappedNative wrapper, in JSContextPtr cx, in JSObjectPtr obj, in JSVal id, in PRUint32 flags, out JSObjectPtr objp); */
NS_IMETHODIMP 
XPCIDispatchScriptableHelper::NewResolve(nsIXPConnectWrappedNative *wrappedNative,
                                         JSContext * cx, JSObject * obj,
                                         jsval idval, PRUint32 flags,
                                         JSObject * *objp, PRBool *retval)
{
    XPCCallContext ccx(JS_CALLER, cx, obj);
    XPCWrappedNative * wrapper = ccx.GetWrapper();
    *retval = PR_FALSE;
    // Check to see if there's an IDispatch tearoff     
    if(!JSVAL_IS_STRING(idval))
        return NS_OK;
    // Look up the native interface for IDispatch and then find a tearoff
    XPCNativeInterface* iface = XPCNativeInterface::GetNewOrUsed(ccx,
                                                                 "IDispatch");
    if(iface == nsnull)
        return NS_OK;
    XPCWrappedNativeTearOff* to = wrapper->FindTearOff(ccx, iface, JS_TRUE);
    if(to == nsnull)
        return NS_OK;
    // get the JS Object for the tea
    JSObject* jso = to->GetJSObject();
    if(jso == nsnull)
        return NS_OK;
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
            return NS_OK;
    }
    // Get the function object
    jsval funval;
    if(!member->GetValue(ccx, iface, &funval))
        return NS_OK;
    // Protect the jsval 
    AUTO_MARK_JSVAL(ccx, funval);
    // clone a function we can use for this object 
    JSObject* funobj = JS_CloneFunctionObject(ccx, JSVAL_TO_OBJECT(funval), obj);
    if(!funobj)
        return NS_OK;
    jsid id;
    flags |= JSPROP_ENUMERATE;
    // If this is a function or a parameterized property
    if(member->IsFunction() || member->IsParameterizedProperty())
    {
        // define the function on the object
        AutoResolveName arn(ccx, idval);
        *objp = obj;
        *retval = JS_ValueToId(ccx, idval, &id) &&
               OBJ_DEFINE_PROPERTY(ccx, obj, id, OBJECT_TO_JSVAL(funobj),
                                   nsnull, nsnull, flags, nsnull);
        return NS_OK;
    }
    // Define the property on the object
    NS_ASSERTION(member->IsProperty(), "way broken!");
    flags |= JSPROP_GETTER | JSPROP_SHARED;
    if(member->IsSetter())
    {
        flags |= JSPROP_SETTER;
        flags &= ~JSPROP_READONLY;
    }
    AutoResolveName arn(ccx, idval);
    *retval = JS_ValueToId(ccx, idval, &id) &&
           OBJ_DEFINE_PROPERTY(ccx, obj, id, JSVAL_VOID,
                               (JSPropertyOp) funobj,
                               (JSPropertyOp) funobj,
                               flags, nsnull);
    return NS_OK;
}

/* PRBool convert (in nsIXPConnectWrappedNative wrapper, in JSContextPtr cx, in JSObjectPtr obj, in PRUint32 type, in JSValPtr vp); */
NS_IMETHODIMP
XPCIDispatchScriptableHelper::Convert(nsIXPConnectWrappedNative *wrapper,
                                      JSContext * cx, JSObject * obj,
                                      PRUint32 type, jsval * vp,
                                      PRBool *retval)
{
    return NS_ERROR_NOT_IMPLEMENTED;
}

/* void finalize (in nsIXPConnectWrappedNative wrapper, in JSContextPtr cx, in JSObjectPtr obj); */
NS_IMETHODIMP
XPCIDispatchScriptableHelper::Finalize(nsIXPConnectWrappedNative *wrapper,
                                       JSContext * cx, JSObject * obj)
{
    return NS_ERROR_NOT_IMPLEMENTED;
}

/* PRBool checkAccess (in nsIXPConnectWrappedNative wrapper, in JSContextPtr cx, in JSObjectPtr obj, in JSVal id, in PRUint32 mode, in JSValPtr vp); */
NS_IMETHODIMP
XPCIDispatchScriptableHelper::CheckAccess(nsIXPConnectWrappedNative *wrapper,
                                          JSContext * cx, JSObject * obj,
                                          jsval id, PRUint32 mode, jsval * vp,
                                          PRBool *retval)
{
    return NS_ERROR_NOT_IMPLEMENTED;
}

/* PRBool call (in nsIXPConnectWrappedNative wrapper, in JSContextPtr cx, in JSObjectPtr obj, in PRUint32 argc, in JSValPtr argv, in JSValPtr vp); */
NS_IMETHODIMP
XPCIDispatchScriptableHelper::Call(nsIXPConnectWrappedNative *wrapper,
                                   JSContext * cx, JSObject * obj,
                                   PRUint32 argc, jsval * argv,
                                   jsval * vp, PRBool *retval)
{
    return NS_ERROR_NOT_IMPLEMENTED;
}

/* PRBool construct (in nsIXPConnectWrappedNative wrapper, in JSContextPtr cx, in JSObjectPtr obj, in PRUint32 argc, in JSValPtr argv, in JSValPtr vp); */
NS_IMETHODIMP
XPCIDispatchScriptableHelper::Construct(nsIXPConnectWrappedNative *wrapper,
                                        JSContext * cx, JSObject * obj,
                                        PRUint32 argc, jsval * argv,
                                        jsval * vp, PRBool *retval)
{
    return NS_ERROR_NOT_IMPLEMENTED;
}

/* PRBool hasInstance (in nsIXPConnectWrappedNative wrapper, in JSContextPtr cx, in JSObjectPtr obj, in JSVal val, out PRBool bp); */
NS_IMETHODIMP
XPCIDispatchScriptableHelper::HasInstance(nsIXPConnectWrappedNative *wrapper,
                                          JSContext * cx, JSObject * obj,
                                          jsval val, PRBool *bp,
                                          PRBool *retval)
{
    return NS_ERROR_NOT_IMPLEMENTED;
}

/* PRUint32 mark (in nsIXPConnectWrappedNative wrapper, in JSContextPtr cx, in JSObjectPtr obj, in voidPtr arg); */
NS_IMETHODIMP
XPCIDispatchScriptableHelper::Mark(nsIXPConnectWrappedNative *wrapper,
                                   JSContext * cx, JSObject * obj,
                                   void * arg, PRUint32 *retval)
{
    return NS_ERROR_NOT_IMPLEMENTED;
}


NS_IMPL_ISUPPORTS1(XPCIDispatchClassInfo, nsIClassInfo)

XPCIDispatchClassInfo* XPCIDispatchClassInfo::sInstance = nsnull;

XPCIDispatchClassInfo* XPCIDispatchClassInfo::GetSingleton()
{
    if(!sInstance)
    {
        sInstance = new XPCIDispatchClassInfo;
        NS_IF_ADDREF(sInstance);
    }
    NS_IF_ADDREF(sInstance);
    return sInstance;
}

void XPCIDispatchClassInfo::FreeSingleton()
{
    NS_IF_RELEASE(sInstance);
    sInstance = nsnull;
}

/* void getInterfaces (out PRUint32 count, [array, size_is (count), retval] 
                       out nsIIDPtr array); */
NS_IMETHODIMP 
XPCIDispatchClassInfo::GetInterfaces(PRUint32 *count, nsIID * **array)
{
    *count = 1;
    *array = NS_STATIC_CAST(nsIID**, nsMemory::Alloc(1 * sizeof(nsIID*)));
    if(!*array)
        return NS_ERROR_OUT_OF_MEMORY;

    **array = NS_STATIC_CAST(nsIID *, nsMemory::Clone(&NSID_IDISPATCH,
                                                      sizeof(NSID_IDISPATCH)));

    return NS_OK;
}

/* nsISupports getHelperForLanguage (in PRUint32 language); */
NS_IMETHODIMP 
XPCIDispatchClassInfo::GetHelperForLanguage(PRUint32 language, 
                                            nsISupports **retval)
{
    *retval = XPCIDispatchScriptableHelper::GetSingleton();
    return NS_OK;
}

/* readonly attribute string contractID; */
NS_IMETHODIMP 
XPCIDispatchClassInfo::GetContractID(char * *aContractID)
{
    *aContractID = nsnull;
    return NS_OK;
}

/* readonly attribute string classDescription; */
NS_IMETHODIMP 
XPCIDispatchClassInfo::GetClassDescription(char * *aClassDescription)
{
    *aClassDescription = nsCRT::strdup("IDispatch");
    return *aClassDescription ? NS_OK : NS_ERROR_OUT_OF_MEMORY;
}

/* readonly attribute nsCIDPtr classID; */
NS_IMETHODIMP 
XPCIDispatchClassInfo::GetClassID(nsCID * *aClassID)
{
    *aClassID = nsnull;
    return NS_OK;
}

/* readonly attribute PRUint32 implementationLanguage; */
NS_IMETHODIMP 
XPCIDispatchClassInfo::GetImplementationLanguage(
    PRUint32 *aImplementationLanguage)
{
    *aImplementationLanguage = nsIProgrammingLanguage::UNKNOWN;
    return NS_OK;
}

/* readonly attribute PRUint32 flags; */
NS_IMETHODIMP 
XPCIDispatchClassInfo::GetFlags(PRUint32 *aFlags)
{
    *aFlags = nsIClassInfo::DOM_OBJECT;
    return NS_OK;
}

/* [notxpcom] readonly attribute nsCID classIDNoAlloc; */
NS_IMETHODIMP 
XPCIDispatchClassInfo::GetClassIDNoAlloc(nsCID *aClassIDNoAlloc)
{
    return NS_ERROR_NOT_AVAILABLE;
}
