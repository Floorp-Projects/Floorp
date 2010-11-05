/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: set ts=8 sw=4 et tw=78:
 *
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
 * The Original Code is Mozilla Communicator client code, released
 * March 31, 1998.
 *
 * The Initial Developer of the Original Code is
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 1998
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   John Bandhauer <jband@netscape.com> (original author)
 *   Pierre Phaneuf <pp@ludusdesign.com>
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either of the GNU General Public License Version 2 or later (the "GPL"),
 * or the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
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

/* The "Components" xpcom objects for JavaScript. */

#include "xpcprivate.h"
#include "nsReadableUtils.h"
#include "xpcIJSModuleLoader.h"
#include "nsIScriptObjectPrincipal.h"
#include "nsIDOMWindow.h"
#include "xpcJSWeakReference.h"
#include "XPCWrapper.h"
#include "jsproxy.h"
#include "WrapperFactory.h"
#include "XrayWrapper.h"
#include "nsNullPrincipal.h"

#ifdef MOZ_JSLOADER
#include "mozJSComponentLoader.h"
#endif

/***************************************************************************/
// stuff used by all

static nsresult ThrowAndFail(uintN errNum, JSContext* cx, JSBool* retval)
{
    XPCThrower::Throw(errNum, cx);
    *retval = JS_FALSE;
    return NS_OK;
}

static JSBool
JSValIsInterfaceOfType(JSContext *cx, jsval v, REFNSIID iid)
{
    nsCOMPtr<nsIXPConnect> xpc;
    nsCOMPtr<nsIXPConnectWrappedNative> wn;
    nsCOMPtr<nsISupports> sup;
    nsISupports* iface;
    if(!JSVAL_IS_PRIMITIVE(v) &&
       nsnull != (xpc = nsXPConnect::GetXPConnect()) &&
       NS_SUCCEEDED(xpc->GetWrappedNativeOfJSObject(cx, JSVAL_TO_OBJECT(v),
                            getter_AddRefs(wn))) && wn &&
       NS_SUCCEEDED(wn->Native()->QueryInterface(iid, (void**)&iface)) && iface)
    {
        NS_RELEASE(iface);
        return JS_TRUE;
    }
    return JS_FALSE;
}

#ifdef XPC_USE_SECURITY_CHECKED_COMPONENT
char* xpc_CloneAllAccess()
{
    static const char allAccess[] = "AllAccess";
    return (char*)nsMemory::Clone(allAccess, sizeof(allAccess));
}

char * xpc_CheckAccessList(const PRUnichar* wideName, const char* list[])
{
    nsCAutoString asciiName;
    CopyUTF16toUTF8(nsDependentString(wideName), asciiName);

    for(const char** p = list; *p; p++)
        if(!strcmp(*p, asciiName.get()))
            return xpc_CloneAllAccess();

    return nsnull;
}
#endif

/***************************************************************************/

/* void getInterfaces (out PRUint32 count, [array, size_is (count), retval]
                       out nsIIDPtr array); */
NS_IMETHODIMP
nsXPCComponents_Interfaces::GetInterfaces(PRUint32 *aCount, nsIID * **aArray)
{
    PRUint32 count = 2;
#ifdef XPC_USE_SECURITY_CHECKED_COMPONENT
    ++count;
#endif
    *aCount = count;
    nsIID **array;
    *aArray = array = static_cast<nsIID**>(nsMemory::Alloc(count * sizeof(nsIID*)));
    if(!array)
        return NS_ERROR_OUT_OF_MEMORY;

    PRUint32 index = 0;
    nsIID* clone;
#define PUSH_IID(id) \
    clone = static_cast<nsIID *>(nsMemory::Clone(&NS_GET_IID( id ),   \
                                                    sizeof(nsIID)));  \
    if (!clone)                                                       \
        goto oom;                                                     \
    array[index++] = clone;

    PUSH_IID(nsIScriptableInterfaces)
    PUSH_IID(nsIXPCScriptable)
#ifdef XPC_USE_SECURITY_CHECKED_COMPONENT
    PUSH_IID(nsISecurityCheckedComponent)
#endif
#undef PUSH_IID

    return NS_OK;
oom:
    while (index)
        nsMemory::Free(array[--index]);
    nsMemory::Free(array);
    *aArray = nsnull;
    return NS_ERROR_OUT_OF_MEMORY;
}

/* nsISupports getHelperForLanguage (in PRUint32 language); */
NS_IMETHODIMP
nsXPCComponents_Interfaces::GetHelperForLanguage(PRUint32 language,
                                      nsISupports **retval)
{
    *retval = nsnull;
    return NS_OK;
}

/* readonly attribute string contractID; */
NS_IMETHODIMP
nsXPCComponents_Interfaces::GetContractID(char * *aContractID)
{
    *aContractID = nsnull;
    return NS_ERROR_NOT_AVAILABLE;
}

/* readonly attribute string classDescription; */
NS_IMETHODIMP
nsXPCComponents_Interfaces::GetClassDescription(char * *aClassDescription)
{
    static const char classDescription[] = "XPCComponents_Interfaces";
    *aClassDescription = (char*)nsMemory::Clone(classDescription, sizeof(classDescription));
    return *aClassDescription ? NS_OK : NS_ERROR_OUT_OF_MEMORY;
}

/* readonly attribute nsCIDPtr classID; */
NS_IMETHODIMP
nsXPCComponents_Interfaces::GetClassID(nsCID * *aClassID)
{
    *aClassID = nsnull;
    return NS_OK;
}

/* readonly attribute PRUint32 implementationLanguage; */
NS_IMETHODIMP
nsXPCComponents_Interfaces::GetImplementationLanguage(
    PRUint32 *aImplementationLanguage)
{
    *aImplementationLanguage = nsIProgrammingLanguage::CPLUSPLUS;
    return NS_OK;
}

/* readonly attribute PRUint32 flags; */
NS_IMETHODIMP
nsXPCComponents_Interfaces::GetFlags(PRUint32 *aFlags)
{
    *aFlags = nsIClassInfo::THREADSAFE;
    return NS_OK;
}

/* [notxpcom] readonly attribute nsCID classIDNoAlloc; */
NS_IMETHODIMP
nsXPCComponents_Interfaces::GetClassIDNoAlloc(nsCID *aClassIDNoAlloc)
{
    return NS_ERROR_NOT_AVAILABLE;
}

nsXPCComponents_Interfaces::nsXPCComponents_Interfaces() :
    mManager(do_GetService(NS_INTERFACEINFOMANAGER_SERVICE_CONTRACTID))
{
}

nsXPCComponents_Interfaces::~nsXPCComponents_Interfaces()
{
    // empty
}


/* [noscript] attribute nsIInterfaceInfoManager manager; */
NS_IMETHODIMP nsXPCComponents_Interfaces::GetManager(nsIInterfaceInfoManager * *aManager)
{
    *aManager = mManager;
    NS_IF_ADDREF(*aManager);
    return NS_OK;
}
NS_IMETHODIMP nsXPCComponents_Interfaces::SetManager(nsIInterfaceInfoManager * aManager)
{
    mManager = aManager;
    return NS_OK;
}

NS_INTERFACE_MAP_BEGIN(nsXPCComponents_Interfaces)
  NS_INTERFACE_MAP_ENTRY(nsIScriptableInterfaces)
  NS_INTERFACE_MAP_ENTRY(nsIXPCScriptable)
  NS_INTERFACE_MAP_ENTRY(nsIClassInfo)
#ifdef XPC_USE_SECURITY_CHECKED_COMPONENT
  NS_INTERFACE_MAP_ENTRY(nsISecurityCheckedComponent)
#endif
  NS_INTERFACE_MAP_ENTRY_AMBIGUOUS(nsISupports, nsIScriptableInterfaces)
NS_INTERFACE_MAP_END_THREADSAFE

NS_IMPL_THREADSAFE_ADDREF(nsXPCComponents_Interfaces)
NS_IMPL_THREADSAFE_RELEASE(nsXPCComponents_Interfaces)

// The nsIXPCScriptable map declaration that will generate stubs for us...
#define XPC_MAP_CLASSNAME           nsXPCComponents_Interfaces
#define XPC_MAP_QUOTED_CLASSNAME   "nsXPCComponents_Interfaces"
#define                             XPC_MAP_WANT_NEWRESOLVE
#define                             XPC_MAP_WANT_NEWENUMERATE
#define XPC_MAP_FLAGS               nsIXPCScriptable::DONT_ENUM_STATIC_PROPS |\
                                    nsIXPCScriptable::ALLOW_PROP_MODS_DURING_RESOLVE
#include "xpc_map_end.h" /* This will #undef the above */


/* PRBool newEnumerate (in nsIXPConnectWrappedNative wrapper, in JSContextPtr cx, in JSObjectPtr obj, in PRUint32 enum_op, in JSValPtr statep, out JSID idp); */
NS_IMETHODIMP
nsXPCComponents_Interfaces::NewEnumerate(nsIXPConnectWrappedNative *wrapper,
                                         JSContext * cx, JSObject * obj,
                                         PRUint32 enum_op, jsval * statep,
                                         jsid * idp, PRBool *_retval)
{
    nsIEnumerator* e;

    switch(enum_op)
    {
        case JSENUMERATE_INIT:
        case JSENUMERATE_INIT_ALL:
        {
            if(!mManager ||
               NS_FAILED(mManager->EnumerateInterfaces(&e)) || !e ||
               NS_FAILED(e->First()))

            {
                *statep = JSVAL_NULL;
                return NS_ERROR_UNEXPECTED;
            }

            *statep = PRIVATE_TO_JSVAL(e);
            if(idp)
                *idp = INT_TO_JSID(0); // indicate that we don't know the count
            return NS_OK;
        }
        case JSENUMERATE_NEXT:
        {
            nsCOMPtr<nsISupports> isup;

            e = (nsIEnumerator*) JSVAL_TO_PRIVATE(*statep);

            while(1)
            {
                if(NS_ENUMERATOR_FALSE == e->IsDone() &&
                   NS_SUCCEEDED(e->CurrentItem(getter_AddRefs(isup))) && isup)
                {
                    e->Next();
                    nsCOMPtr<nsIInterfaceInfo> iface(do_QueryInterface(isup));
                    if(iface)
                    {
                        JSString* idstr;
                        const char* name;
                        PRBool scriptable;

                        if(NS_SUCCEEDED(iface->IsScriptable(&scriptable)) &&
                           !scriptable)
                        {
                            continue;
                        }

                        if(NS_SUCCEEDED(iface->GetNameShared(&name)) && name &&
                           nsnull != (idstr = JS_NewStringCopyZ(cx, name)) &&
                           JS_ValueToId(cx, STRING_TO_JSVAL(idstr), idp))
                        {
                            return NS_OK;
                        }
                    }
                }
                // else...
                break;
            }
            // FALL THROUGH
        }

        case JSENUMERATE_DESTROY:
        default:
            e = (nsIEnumerator*) JSVAL_TO_PRIVATE(*statep);
            NS_IF_RELEASE(e);
            *statep = JSVAL_NULL;
            return NS_OK;
    }
}

/* PRBool newResolve (in nsIXPConnectWrappedNative wrapper, in JSContextPtr cx, in JSObjectPtr obj, in jsval id, in PRUint32 flags, out JSObjectPtr objp); */
NS_IMETHODIMP
nsXPCComponents_Interfaces::NewResolve(nsIXPConnectWrappedNative *wrapper,
                                       JSContext * cx, JSObject * obj,
                                       jsid id, PRUint32 flags,
                                       JSObject * *objp, PRBool *_retval)
{
    const char* name = nsnull;

    if(mManager &&
       JSID_IS_STRING(id) &&
       nsnull != (name = JS_GetStringBytes(JSID_TO_STRING(id))) &&
       name[0] != '{') // we only allow interfaces by name here
    {
        nsCOMPtr<nsIInterfaceInfo> info;
        mManager->GetInfoForName(name, getter_AddRefs(info));
        if(!info)
            return NS_OK;

        nsCOMPtr<nsIJSIID> nsid =
            dont_AddRef(static_cast<nsIJSIID*>(nsJSIID::NewID(info)));

        if(nsid)
        {
            nsCOMPtr<nsIXPConnect> xpc;
            wrapper->GetXPConnect(getter_AddRefs(xpc));
            if(xpc)
            {
                nsCOMPtr<nsIXPConnectJSObjectHolder> holder;
                if(NS_SUCCEEDED(xpc->WrapNative(cx, obj,
                                                static_cast<nsIJSIID*>(nsid),
                                                NS_GET_IID(nsIJSIID),
                                                getter_AddRefs(holder))))
                {
                    JSObject* idobj;
                    if(holder && NS_SUCCEEDED(holder->GetJSObject(&idobj)))
                    {
                        *objp = obj;
                        *_retval = JS_DefinePropertyById(cx, obj, id,
                                                         OBJECT_TO_JSVAL(idobj),
                                                         nsnull, nsnull,
                                                         JSPROP_ENUMERATE |
                                                         JSPROP_READONLY |
                                                         JSPROP_PERMANENT);
                    }
                }
            }
        }
    }
    return NS_OK;
}

#ifdef XPC_USE_SECURITY_CHECKED_COMPONENT
/* string canCreateWrapper (in nsIIDPtr iid); */
NS_IMETHODIMP
nsXPCComponents_Interfaces::CanCreateWrapper(const nsIID * iid, char **_retval)
{
    // We let anyone do this...
    *_retval = xpc_CloneAllAccess();
    return NS_OK;
}

/* string canCallMethod (in nsIIDPtr iid, in wstring methodName); */
NS_IMETHODIMP
nsXPCComponents_Interfaces::CanCallMethod(const nsIID * iid, const PRUnichar *methodName, char **_retval)
{
    // If you have to ask, then the answer is NO
    *_retval = nsnull;
    return NS_OK;
}

/* string canGetProperty (in nsIIDPtr iid, in wstring propertyName); */
NS_IMETHODIMP
nsXPCComponents_Interfaces::CanGetProperty(const nsIID * iid, const PRUnichar *propertyName, char **_retval)
{
    // If you have to ask, then the answer is NO
    *_retval = nsnull;
    return NS_OK;
}

/* string canSetProperty (in nsIIDPtr iid, in wstring propertyName); */
NS_IMETHODIMP
nsXPCComponents_Interfaces::CanSetProperty(const nsIID * iid, const PRUnichar *propertyName, char **_retval)
{
    // If you have to ask, then the answer is NO
    *_retval = nsnull;
    return NS_OK;
}
#endif

/***************************************************************************/
/***************************************************************************/
/***************************************************************************/

class nsXPCComponents_InterfacesByID :
            public nsIScriptableInterfacesByID,
            public nsIXPCScriptable,
            public nsIClassInfo
#ifdef XPC_USE_SECURITY_CHECKED_COMPONENT
          , public nsISecurityCheckedComponent
#endif
{
public:
    // all the interface method declarations...
    NS_DECL_ISUPPORTS
    NS_DECL_NSISCRIPTABLEINTERFACESBYID
    NS_DECL_NSIXPCSCRIPTABLE
    NS_DECL_NSICLASSINFO
#ifdef XPC_USE_SECURITY_CHECKED_COMPONENT
    NS_DECL_NSISECURITYCHECKEDCOMPONENT
#endif

public:
    nsXPCComponents_InterfacesByID();
    virtual ~nsXPCComponents_InterfacesByID();

private:
    nsCOMPtr<nsIInterfaceInfoManager> mManager;
};

/***************************************************************************/
/* void getInterfaces (out PRUint32 count, [array, size_is (count), retval]
                       out nsIIDPtr array); */
NS_IMETHODIMP
nsXPCComponents_InterfacesByID::GetInterfaces(PRUint32 *aCount, nsIID * **aArray)
{
    PRUint32 count = 2;
#ifdef XPC_USE_SECURITY_CHECKED_COMPONENT
    ++count;
#endif
    *aCount = count;
    nsIID **array;
    *aArray = array = static_cast<nsIID**>(nsMemory::Alloc(count * sizeof(nsIID*)));
    if(!array)
        return NS_ERROR_OUT_OF_MEMORY;

    PRUint32 index = 0;
    nsIID* clone;
#define PUSH_IID(id) \
    clone = static_cast<nsIID *>(nsMemory::Clone(&NS_GET_IID( id ),   \
                                                    sizeof(nsIID)));  \
    if (!clone)                                                       \
        goto oom;                                                     \
    array[index++] = clone;

    PUSH_IID(nsIScriptableInterfacesByID)
    PUSH_IID(nsIXPCScriptable)
#ifdef XPC_USE_SECURITY_CHECKED_COMPONENT
    PUSH_IID(nsISecurityCheckedComponent)
#endif
#undef PUSH_IID

    return NS_OK;
oom:
    while (index)
        nsMemory::Free(array[--index]);
    nsMemory::Free(array);
    *aArray = nsnull;
    return NS_ERROR_OUT_OF_MEMORY;
}

/* nsISupports getHelperForLanguage (in PRUint32 language); */
NS_IMETHODIMP
nsXPCComponents_InterfacesByID::GetHelperForLanguage(PRUint32 language,
                                      nsISupports **retval)
{
    *retval = nsnull;
    return NS_OK;
}

/* readonly attribute string contractID; */
NS_IMETHODIMP
nsXPCComponents_InterfacesByID::GetContractID(char * *aContractID)
{
    *aContractID = nsnull;
    return NS_ERROR_NOT_AVAILABLE;
}

/* readonly attribute string classDescription; */
NS_IMETHODIMP
nsXPCComponents_InterfacesByID::GetClassDescription(char * *aClassDescription)
{
    static const char classDescription[] = "XPCComponents_Interfaces";
    *aClassDescription = (char*)nsMemory::Clone(classDescription, sizeof(classDescription));
    return *aClassDescription ? NS_OK : NS_ERROR_OUT_OF_MEMORY;
}

/* readonly attribute nsCIDPtr classID; */
NS_IMETHODIMP
nsXPCComponents_InterfacesByID::GetClassID(nsCID * *aClassID)
{
    *aClassID = nsnull;
    return NS_OK;
}

/* readonly attribute PRUint32 implementationLanguage; */
NS_IMETHODIMP
nsXPCComponents_InterfacesByID::GetImplementationLanguage(
    PRUint32 *aImplementationLanguage)
{
    *aImplementationLanguage = nsIProgrammingLanguage::CPLUSPLUS;
    return NS_OK;
}

/* readonly attribute PRUint32 flags; */
NS_IMETHODIMP
nsXPCComponents_InterfacesByID::GetFlags(PRUint32 *aFlags)
{
    *aFlags = nsIClassInfo::THREADSAFE;
    return NS_OK;
}

/* [notxpcom] readonly attribute nsCID classIDNoAlloc; */
NS_IMETHODIMP
nsXPCComponents_InterfacesByID::GetClassIDNoAlloc(nsCID *aClassIDNoAlloc)
{
    return NS_ERROR_NOT_AVAILABLE;
}

nsXPCComponents_InterfacesByID::nsXPCComponents_InterfacesByID() :
    mManager(do_GetService(NS_INTERFACEINFOMANAGER_SERVICE_CONTRACTID))
{
}

nsXPCComponents_InterfacesByID::~nsXPCComponents_InterfacesByID()
{
    // empty
}

NS_INTERFACE_MAP_BEGIN(nsXPCComponents_InterfacesByID)
  NS_INTERFACE_MAP_ENTRY(nsIScriptableInterfacesByID)
  NS_INTERFACE_MAP_ENTRY(nsIXPCScriptable)
  NS_INTERFACE_MAP_ENTRY(nsIClassInfo)
#ifdef XPC_USE_SECURITY_CHECKED_COMPONENT
  NS_INTERFACE_MAP_ENTRY(nsISecurityCheckedComponent)
#endif
  NS_INTERFACE_MAP_ENTRY_AMBIGUOUS(nsISupports, nsIScriptableInterfacesByID)
NS_INTERFACE_MAP_END_THREADSAFE

NS_IMPL_THREADSAFE_ADDREF(nsXPCComponents_InterfacesByID)
NS_IMPL_THREADSAFE_RELEASE(nsXPCComponents_InterfacesByID)

// The nsIXPCScriptable map declaration that will generate stubs for us...
#define XPC_MAP_CLASSNAME           nsXPCComponents_InterfacesByID
#define XPC_MAP_QUOTED_CLASSNAME   "nsXPCComponents_InterfacesByID"
#define                             XPC_MAP_WANT_NEWRESOLVE
#define                             XPC_MAP_WANT_NEWENUMERATE
#define XPC_MAP_FLAGS               nsIXPCScriptable::DONT_ENUM_STATIC_PROPS |\
                                    nsIXPCScriptable::ALLOW_PROP_MODS_DURING_RESOLVE
#include "xpc_map_end.h" /* This will #undef the above */

/* PRBool newEnumerate (in nsIXPConnectWrappedNative wrapper, in JSContextPtr cx, in JSObjectPtr obj, in PRUint32 enum_op, in JSValPtr statep, out JSID idp); */
NS_IMETHODIMP
nsXPCComponents_InterfacesByID::NewEnumerate(nsIXPConnectWrappedNative *wrapper,
                                             JSContext * cx, JSObject * obj,
                                             PRUint32 enum_op, jsval * statep,
                                             jsid * idp, PRBool *_retval)
{
    nsIEnumerator* e;

    switch(enum_op)
    {
        case JSENUMERATE_INIT:
        case JSENUMERATE_INIT_ALL:
        {
            if(!mManager ||
               NS_FAILED(mManager->EnumerateInterfaces(&e)) || !e ||
               NS_FAILED(e->First()))

            {
                *statep = JSVAL_NULL;
                return NS_ERROR_UNEXPECTED;
            }

            *statep = PRIVATE_TO_JSVAL(e);
            if(idp)
                *idp = INT_TO_JSID(0); // indicate that we don't know the count
            return NS_OK;
        }
        case JSENUMERATE_NEXT:
        {
            nsCOMPtr<nsISupports> isup;

            e = (nsIEnumerator*) JSVAL_TO_PRIVATE(*statep);

            while(1)
            {
                if(NS_ENUMERATOR_FALSE == e->IsDone() &&
                   NS_SUCCEEDED(e->CurrentItem(getter_AddRefs(isup))) && isup)
                {
                    e->Next();
                    nsCOMPtr<nsIInterfaceInfo> iface(do_QueryInterface(isup));
                    if(iface)
                    {
                        nsIID const *iid;
                        char idstr[NSID_LENGTH];
                        JSString* jsstr;
                        PRBool scriptable;

                        if(NS_SUCCEEDED(iface->IsScriptable(&scriptable)) &&
                           !scriptable)
                        {
                            continue;
                        }

                        if(NS_SUCCEEDED(iface->GetIIDShared(&iid)))
                        {
                            iid->ToProvidedString(idstr);
                            jsstr = JS_NewStringCopyZ(cx, idstr);
                            if (jsstr &&
                                JS_ValueToId(cx, STRING_TO_JSVAL(jsstr), idp))
                            {
                                return NS_OK;
                            }
                        }
                    }
                }
                // else...
                break;
            }
            // FALL THROUGH
        }

        case JSENUMERATE_DESTROY:
        default:
            e = (nsIEnumerator*) JSVAL_TO_PRIVATE(*statep);
            NS_IF_RELEASE(e);
            *statep = JSVAL_NULL;
            return NS_OK;
    }
}

/* PRBool newResolve (in nsIXPConnectWrappedNative wrapper, in JSContextPtr cx, in JSObjectPtr obj, in jsval id, in PRUint32 flags, out JSObjectPtr objp); */
NS_IMETHODIMP
nsXPCComponents_InterfacesByID::NewResolve(nsIXPConnectWrappedNative *wrapper,
                                           JSContext * cx, JSObject * obj,
                                           jsid id, PRUint32 flags,
                                           JSObject * *objp, PRBool *_retval)
{
    const jschar* name = nsnull;

    if(mManager &&
       JSID_IS_STRING(id) &&
       38 == JS_GetStringLength(JSID_TO_STRING(id)) &&
       nsnull != (name = JS_GetStringChars(JSID_TO_STRING(id))))
    {
        nsID iid;
        if (!iid.Parse(NS_ConvertUTF16toUTF8(reinterpret_cast<const PRUnichar*>
                                                             (name)).get()))
            return NS_OK;

        nsCOMPtr<nsIInterfaceInfo> info;
        mManager->GetInfoForIID(&iid, getter_AddRefs(info));
        if(!info)
            return NS_OK;

        nsCOMPtr<nsIJSIID> nsid =
            dont_AddRef(static_cast<nsIJSIID*>(nsJSIID::NewID(info)));

        if (!nsid)
            return NS_ERROR_OUT_OF_MEMORY;

        nsCOMPtr<nsIXPConnect> xpc;
        wrapper->GetXPConnect(getter_AddRefs(xpc));
        if(xpc)
        {
            nsCOMPtr<nsIXPConnectJSObjectHolder> holder;
            if(NS_SUCCEEDED(xpc->WrapNative(cx, obj,
                                            static_cast<nsIJSIID*>(nsid),
                                            NS_GET_IID(nsIJSIID),
                                            getter_AddRefs(holder))))
            {
                JSObject* idobj;
                if(holder && NS_SUCCEEDED(holder->GetJSObject(&idobj)))
                {
                    *objp = obj;
                    *_retval =
                        JS_DefinePropertyById(cx, obj, id,
                                              OBJECT_TO_JSVAL(idobj),
                                              nsnull, nsnull,
                                              JSPROP_ENUMERATE |
                                              JSPROP_READONLY |
                                              JSPROP_PERMANENT);
                }
            }
        }
    }
    return NS_OK;
}

#ifdef XPC_USE_SECURITY_CHECKED_COMPONENT
/* string canCreateWrapper (in nsIIDPtr iid); */
NS_IMETHODIMP
nsXPCComponents_InterfacesByID::CanCreateWrapper(const nsIID * iid, char **_retval)
{
    // We let anyone do this...
    *_retval = xpc_CloneAllAccess();
    return NS_OK;
}

/* string canCallMethod (in nsIIDPtr iid, in wstring methodName); */
NS_IMETHODIMP
nsXPCComponents_InterfacesByID::CanCallMethod(const nsIID * iid, const PRUnichar *methodName, char **_retval)
{
    // If you have to ask, then the answer is NO
    *_retval = nsnull;
    return NS_OK;
}

/* string canGetProperty (in nsIIDPtr iid, in wstring propertyName); */
NS_IMETHODIMP
nsXPCComponents_InterfacesByID::CanGetProperty(const nsIID * iid, const PRUnichar *propertyName, char **_retval)
{
    // If you have to ask, then the answer is NO
    *_retval = nsnull;
    return NS_OK;
}

/* string canSetProperty (in nsIIDPtr iid, in wstring propertyName); */
NS_IMETHODIMP
nsXPCComponents_InterfacesByID::CanSetProperty(const nsIID * iid, const PRUnichar *propertyName, char **_retval)
{
    // If you have to ask, then the answer is NO
    *_retval = nsnull;
    return NS_OK;
}
#endif

/***************************************************************************/
/***************************************************************************/
/***************************************************************************/



class nsXPCComponents_Classes : 
  public nsIXPCComponents_Classes,
  public nsIXPCScriptable,
  public nsIClassInfo
{
public:
    // all the interface method declarations...
    NS_DECL_ISUPPORTS
    NS_DECL_NSIXPCCOMPONENTS_CLASSES
    NS_DECL_NSIXPCSCRIPTABLE
    NS_DECL_NSICLASSINFO

public:
    nsXPCComponents_Classes();
    virtual ~nsXPCComponents_Classes();
};

/***************************************************************************/
/* void getInterfaces (out PRUint32 count, [array, size_is (count), retval] 
                       out nsIIDPtr array); */
NS_IMETHODIMP 
nsXPCComponents_Classes::GetInterfaces(PRUint32 *aCount, nsIID * **aArray)
{
    PRUint32 count = 2;
    *aCount = count;
    nsIID **array;
    *aArray = array = static_cast<nsIID**>(nsMemory::Alloc(count * sizeof(nsIID*)));
    if(!array)
        return NS_ERROR_OUT_OF_MEMORY;

    PRUint32 index = 0;
    nsIID* clone;
#define PUSH_IID(id) \
    clone = static_cast<nsIID *>(nsMemory::Clone(&NS_GET_IID( id ),   \
                                                    sizeof(nsIID)));  \
    if (!clone)                                                       \
        goto oom;                                                     \
    array[index++] = clone;

    PUSH_IID(nsIXPCComponents_Classes)
    PUSH_IID(nsIXPCScriptable)
#undef PUSH_IID

    return NS_OK;
oom:
    while (index)
        nsMemory::Free(array[--index]);
    nsMemory::Free(array);
    *aArray = nsnull;
    return NS_ERROR_OUT_OF_MEMORY;
}

/* nsISupports getHelperForLanguage (in PRUint32 language); */
NS_IMETHODIMP 
nsXPCComponents_Classes::GetHelperForLanguage(PRUint32 language, 
                                      nsISupports **retval)
{
    *retval = nsnull;
    return NS_OK;
}

/* readonly attribute string contractID; */
NS_IMETHODIMP 
nsXPCComponents_Classes::GetContractID(char * *aContractID)
{
    *aContractID = nsnull;
    return NS_ERROR_NOT_AVAILABLE;
}

/* readonly attribute string classDescription; */
NS_IMETHODIMP 
nsXPCComponents_Classes::GetClassDescription(char * *aClassDescription)
{
    static const char classDescription[] = "XPCComponents_Classes";
    *aClassDescription = (char*)nsMemory::Clone(classDescription, sizeof(classDescription));
    return *aClassDescription ? NS_OK : NS_ERROR_OUT_OF_MEMORY;
}

/* readonly attribute nsCIDPtr classID; */
NS_IMETHODIMP 
nsXPCComponents_Classes::GetClassID(nsCID * *aClassID)
{
    *aClassID = nsnull;
    return NS_OK;
}

/* readonly attribute PRUint32 implementationLanguage; */
NS_IMETHODIMP 
nsXPCComponents_Classes::GetImplementationLanguage(
    PRUint32 *aImplementationLanguage)
{
    *aImplementationLanguage = nsIProgrammingLanguage::CPLUSPLUS;
    return NS_OK;
}

/* readonly attribute PRUint32 flags; */
NS_IMETHODIMP 
nsXPCComponents_Classes::GetFlags(PRUint32 *aFlags)
{
    *aFlags = nsIClassInfo::THREADSAFE;
    return NS_OK;
}

/* [notxpcom] readonly attribute nsCID classIDNoAlloc; */
NS_IMETHODIMP 
nsXPCComponents_Classes::GetClassIDNoAlloc(nsCID *aClassIDNoAlloc)
{
    return NS_ERROR_NOT_AVAILABLE;
}

nsXPCComponents_Classes::nsXPCComponents_Classes()
{
}

nsXPCComponents_Classes::~nsXPCComponents_Classes()
{
    // empty
}

NS_INTERFACE_MAP_BEGIN(nsXPCComponents_Classes)
  NS_INTERFACE_MAP_ENTRY(nsIXPCComponents_Classes)
  NS_INTERFACE_MAP_ENTRY(nsIXPCScriptable)
  NS_INTERFACE_MAP_ENTRY(nsIClassInfo)
  NS_INTERFACE_MAP_ENTRY_AMBIGUOUS(nsISupports, nsIXPCComponents_Classes)
NS_INTERFACE_MAP_END_THREADSAFE

NS_IMPL_THREADSAFE_ADDREF(nsXPCComponents_Classes)
NS_IMPL_THREADSAFE_RELEASE(nsXPCComponents_Classes)

// The nsIXPCScriptable map declaration that will generate stubs for us...
#define XPC_MAP_CLASSNAME           nsXPCComponents_Classes
#define XPC_MAP_QUOTED_CLASSNAME   "nsXPCComponents_Classes"
#define                             XPC_MAP_WANT_NEWRESOLVE
#define                             XPC_MAP_WANT_NEWENUMERATE
#define XPC_MAP_FLAGS               nsIXPCScriptable::DONT_ENUM_STATIC_PROPS |\
                                    nsIXPCScriptable::ALLOW_PROP_MODS_DURING_RESOLVE
#include "xpc_map_end.h" /* This will #undef the above */


/* PRBool newEnumerate (in nsIXPConnectWrappedNative wrapper, in JSContextPtr cx, in JSObjectPtr obj, in PRUint32 enum_op, in JSValPtr statep, out JSID idp); */
NS_IMETHODIMP
nsXPCComponents_Classes::NewEnumerate(nsIXPConnectWrappedNative *wrapper,
                                      JSContext * cx, JSObject * obj,
                                      PRUint32 enum_op, jsval * statep,
                                      jsid * idp, PRBool *_retval)
{
    nsISimpleEnumerator* e;

    switch(enum_op)
    {
        case JSENUMERATE_INIT:
        case JSENUMERATE_INIT_ALL:
        {
            nsCOMPtr<nsIComponentRegistrar> compMgr;
            if(NS_FAILED(NS_GetComponentRegistrar(getter_AddRefs(compMgr))) || !compMgr ||
               NS_FAILED(compMgr->EnumerateContractIDs(&e)) || !e )
            {
                *statep = JSVAL_NULL;
                return NS_ERROR_UNEXPECTED;
            }

            *statep = PRIVATE_TO_JSVAL(e);
            if(idp)
                *idp = INT_TO_JSID(0); // indicate that we don't know the count
            return NS_OK;
        }
        case JSENUMERATE_NEXT:
        {
            nsCOMPtr<nsISupports> isup;
            PRBool hasMore;
            e = (nsISimpleEnumerator*) JSVAL_TO_PRIVATE(*statep);

            if(NS_SUCCEEDED(e->HasMoreElements(&hasMore)) && hasMore &&
               NS_SUCCEEDED(e->GetNext(getter_AddRefs(isup))) && isup)
            {
                nsCOMPtr<nsISupportsCString> holder(do_QueryInterface(isup));
                if(holder)
                {
                    nsCAutoString name;
                    if(NS_SUCCEEDED(holder->GetData(name)))
                    {
                        JSString* idstr = JS_NewStringCopyN(cx, name.get(), name.Length());
                        if(idstr &&
                           JS_ValueToId(cx, STRING_TO_JSVAL(idstr), idp))
                        {
                            return NS_OK;
                        }
                    }
                }
            }
            // else... FALL THROUGH
        }

        case JSENUMERATE_DESTROY:
        default:
            e = (nsISimpleEnumerator*) JSVAL_TO_PRIVATE(*statep);
            NS_IF_RELEASE(e);
            *statep = JSVAL_NULL;
            return NS_OK;
    }
}

/* PRBool newResolve (in nsIXPConnectWrappedNative wrapper, in JSContextPtr cx, in JSObjectPtr obj, in jsval id, in PRUint32 flags, out JSObjectPtr objp); */
NS_IMETHODIMP
nsXPCComponents_Classes::NewResolve(nsIXPConnectWrappedNative *wrapper,
                                    JSContext * cx, JSObject * obj,
                                    jsid id, PRUint32 flags,
                                    JSObject * *objp, PRBool *_retval)

{
    const char* name = nsnull;

    if(JSID_IS_STRING(id) &&
       nsnull != (name = JS_GetStringBytes(JSID_TO_STRING(id))) &&
       name[0] != '{') // we only allow contractids here
    {
        nsCOMPtr<nsIJSCID> nsid =
            dont_AddRef(static_cast<nsIJSCID*>(nsJSCID::NewID(name)));
        if(nsid)
        {
            nsCOMPtr<nsIXPConnect> xpc;
            wrapper->GetXPConnect(getter_AddRefs(xpc));
            if(xpc)
            {
                nsCOMPtr<nsIXPConnectJSObjectHolder> holder;
                if(NS_SUCCEEDED(xpc->WrapNative(cx, obj,
                                                static_cast<nsIJSCID*>(nsid),
                                                NS_GET_IID(nsIJSCID),
                                                getter_AddRefs(holder))))
                {
                    JSObject* idobj;
                    if(holder && NS_SUCCEEDED(holder->GetJSObject(&idobj)))
                    {
                        *objp = obj;
                        *_retval = JS_DefinePropertyById(cx, obj, id,
                                                         OBJECT_TO_JSVAL(idobj),
                                                         nsnull, nsnull,
                                                         JSPROP_ENUMERATE |
                                                         JSPROP_READONLY |
                                                         JSPROP_PERMANENT);
                    }
                }
            }
        }
    }
    return NS_OK;
}

/***************************************************************************/
/***************************************************************************/
/***************************************************************************/

class nsXPCComponents_ClassesByID :
  public nsIXPCComponents_ClassesByID,
  public nsIXPCScriptable,
  public nsIClassInfo
{
public:
    // all the interface method declarations...
    NS_DECL_ISUPPORTS
    NS_DECL_NSIXPCCOMPONENTS_CLASSESBYID
    NS_DECL_NSIXPCSCRIPTABLE
    NS_DECL_NSICLASSINFO

public:
    nsXPCComponents_ClassesByID();
    virtual ~nsXPCComponents_ClassesByID();
};

/***************************************************************************/
/* void getInterfaces (out PRUint32 count, [array, size_is (count), retval] 
                       out nsIIDPtr array); */
NS_IMETHODIMP 
nsXPCComponents_ClassesByID::GetInterfaces(PRUint32 *aCount, nsIID * **aArray)
{
    PRUint32 count = 2;
    *aCount = count;
    nsIID **array;
    *aArray = array = static_cast<nsIID**>(nsMemory::Alloc(count * sizeof(nsIID*)));
    if(!array)
        return NS_ERROR_OUT_OF_MEMORY;

    PRUint32 index = 0;
    nsIID* clone;
#define PUSH_IID(id) \
    clone = static_cast<nsIID *>(nsMemory::Clone(&NS_GET_IID( id ),   \
                                                    sizeof(nsIID)));  \
    if (!clone)                                                       \
        goto oom;                                                     \
    array[index++] = clone;

    PUSH_IID(nsIXPCComponents_ClassesByID)
    PUSH_IID(nsIXPCScriptable)
#undef PUSH_IID

    return NS_OK;
oom:
    while (index)
        nsMemory::Free(array[--index]);
    nsMemory::Free(array);
    *aArray = nsnull;
    return NS_ERROR_OUT_OF_MEMORY;
}

/* nsISupports getHelperForLanguage (in PRUint32 language); */
NS_IMETHODIMP 
nsXPCComponents_ClassesByID::GetHelperForLanguage(PRUint32 language, 
                                      nsISupports **retval)
{
    *retval = nsnull;
    return NS_OK;
}

/* readonly attribute string contractID; */
NS_IMETHODIMP 
nsXPCComponents_ClassesByID::GetContractID(char * *aContractID)
{
    *aContractID = nsnull;
    return NS_ERROR_NOT_AVAILABLE;
}

/* readonly attribute string classDescription; */
NS_IMETHODIMP 
nsXPCComponents_ClassesByID::GetClassDescription(char * *aClassDescription)
{
    static const char classDescription[] = "XPCComponents_Interfaces";
    *aClassDescription = (char*)nsMemory::Clone(classDescription, sizeof(classDescription));
    return *aClassDescription ? NS_OK : NS_ERROR_OUT_OF_MEMORY;
}

/* readonly attribute nsCIDPtr classID; */
NS_IMETHODIMP 
nsXPCComponents_ClassesByID::GetClassID(nsCID * *aClassID)
{
    *aClassID = nsnull;
    return NS_OK;
}

/* readonly attribute PRUint32 implementationLanguage; */
NS_IMETHODIMP 
nsXPCComponents_ClassesByID::GetImplementationLanguage(
    PRUint32 *aImplementationLanguage)
{
    *aImplementationLanguage = nsIProgrammingLanguage::CPLUSPLUS;
    return NS_OK;
}

/* readonly attribute PRUint32 flags; */
NS_IMETHODIMP 
nsXPCComponents_ClassesByID::GetFlags(PRUint32 *aFlags)
{
    *aFlags = nsIClassInfo::THREADSAFE;
    return NS_OK;
}

/* [notxpcom] readonly attribute nsCID classIDNoAlloc; */
NS_IMETHODIMP 
nsXPCComponents_ClassesByID::GetClassIDNoAlloc(nsCID *aClassIDNoAlloc)
{
    return NS_ERROR_NOT_AVAILABLE;
}

nsXPCComponents_ClassesByID::nsXPCComponents_ClassesByID()
{
}

nsXPCComponents_ClassesByID::~nsXPCComponents_ClassesByID()
{
    // empty
}

NS_INTERFACE_MAP_BEGIN(nsXPCComponents_ClassesByID)
  NS_INTERFACE_MAP_ENTRY(nsIXPCComponents_ClassesByID)
  NS_INTERFACE_MAP_ENTRY(nsIXPCScriptable)
  NS_INTERFACE_MAP_ENTRY(nsIClassInfo)
  NS_INTERFACE_MAP_ENTRY_AMBIGUOUS(nsISupports, nsIXPCComponents_ClassesByID)
NS_INTERFACE_MAP_END_THREADSAFE

NS_IMPL_THREADSAFE_ADDREF(nsXPCComponents_ClassesByID)
NS_IMPL_THREADSAFE_RELEASE(nsXPCComponents_ClassesByID)

// The nsIXPCScriptable map declaration that will generate stubs for us...
#define XPC_MAP_CLASSNAME           nsXPCComponents_ClassesByID
#define XPC_MAP_QUOTED_CLASSNAME   "nsXPCComponents_ClassesByID"
#define                             XPC_MAP_WANT_NEWRESOLVE
#define                             XPC_MAP_WANT_NEWENUMERATE
#define XPC_MAP_FLAGS               nsIXPCScriptable::DONT_ENUM_STATIC_PROPS |\
                                    nsIXPCScriptable::ALLOW_PROP_MODS_DURING_RESOLVE
#include "xpc_map_end.h" /* This will #undef the above */

/* PRBool newEnumerate (in nsIXPConnectWrappedNative wrapper, in JSContextPtr cx, in JSObjectPtr obj, in PRUint32 enum_op, in JSValPtr statep, out JSID idp); */
NS_IMETHODIMP
nsXPCComponents_ClassesByID::NewEnumerate(nsIXPConnectWrappedNative *wrapper,
                                          JSContext * cx, JSObject * obj,
                                          PRUint32 enum_op, jsval * statep,
                                          jsid * idp, PRBool *_retval)
{
    nsISimpleEnumerator* e;

    switch(enum_op)
    {
        case JSENUMERATE_INIT:
        case JSENUMERATE_INIT_ALL:
        {
            nsCOMPtr<nsIComponentRegistrar> compMgr;
            if(NS_FAILED(NS_GetComponentRegistrar(getter_AddRefs(compMgr))) || !compMgr ||
               NS_FAILED(compMgr->EnumerateCIDs(&e)) || !e )
            {
                *statep = JSVAL_NULL;
                return NS_ERROR_UNEXPECTED;
            }

            *statep = PRIVATE_TO_JSVAL(e);
            if(idp)
                *idp = INT_TO_JSID(0); // indicate that we don't know the count
            return NS_OK;
        }
        case JSENUMERATE_NEXT:
        {
            nsCOMPtr<nsISupports> isup;
            PRBool hasMore;
            e = (nsISimpleEnumerator*) JSVAL_TO_PRIVATE(*statep);

            if(NS_SUCCEEDED(e->HasMoreElements(&hasMore)) && hasMore &&
               NS_SUCCEEDED(e->GetNext(getter_AddRefs(isup))) && isup)
            {
                nsCOMPtr<nsISupportsID> holder(do_QueryInterface(isup));
                if(holder)
                {
                    char* name;
                    if(NS_SUCCEEDED(holder->ToString(&name)) && name)
                    {
                        JSString* idstr = JS_NewStringCopyZ(cx, name);
                        nsMemory::Free(name);
                        if(idstr &&
                           JS_ValueToId(cx, STRING_TO_JSVAL(idstr), idp))
                        {
                            return NS_OK;
                        }
                    }
                }
            }
            // else... FALL THROUGH
        }

        case JSENUMERATE_DESTROY:
        default:
            e = (nsISimpleEnumerator*) JSVAL_TO_PRIVATE(*statep);
            NS_IF_RELEASE(e);
            *statep = JSVAL_NULL;
            return NS_OK;
    }
}

static PRBool
IsRegisteredCLSID(const char* str)
{
    PRBool registered;
    nsID id;

    if(!id.Parse(str))
        return PR_FALSE;

    nsCOMPtr<nsIComponentRegistrar> compMgr;
    if(NS_FAILED(NS_GetComponentRegistrar(getter_AddRefs(compMgr))) || !compMgr ||
       NS_FAILED(compMgr->IsCIDRegistered(id, &registered)))
        return PR_FALSE;

    return registered;
}

/* PRBool newResolve (in nsIXPConnectWrappedNative wrapper, in JSContextPtr cx, in JSObjectPtr obj, in jsval id, in PRUint32 flags, out JSObjectPtr objp); */
NS_IMETHODIMP
nsXPCComponents_ClassesByID::NewResolve(nsIXPConnectWrappedNative *wrapper,
                                        JSContext * cx, JSObject * obj,
                                        jsid id, PRUint32 flags,
                                        JSObject * *objp, PRBool *_retval)
{
    const char* name = nsnull;

    if(JSID_IS_STRING(id) &&
       nsnull != (name = JS_GetStringBytes(JSID_TO_STRING(id))) &&
       name[0] == '{' &&
       IsRegisteredCLSID(name)) // we only allow canonical CLSIDs here
    {
        nsCOMPtr<nsIJSCID> nsid =
            dont_AddRef(static_cast<nsIJSCID*>(nsJSCID::NewID(name)));
        if(nsid)
        {
            nsCOMPtr<nsIXPConnect> xpc;
            wrapper->GetXPConnect(getter_AddRefs(xpc));
            if(xpc)
            {
                nsCOMPtr<nsIXPConnectJSObjectHolder> holder;
                if(NS_SUCCEEDED(xpc->WrapNative(cx, obj,
                                                static_cast<nsIJSCID*>(nsid),
                                                NS_GET_IID(nsIJSCID),
                                                getter_AddRefs(holder))))
                {
                    JSObject* idobj;
                    if(holder && NS_SUCCEEDED(holder->GetJSObject(&idobj)))
                    {
                        *objp = obj;
                        *_retval = JS_DefinePropertyById(cx, obj, id,
                                                         OBJECT_TO_JSVAL(idobj),
                                                         nsnull, nsnull,
                                                         JSPROP_ENUMERATE |
                                                         JSPROP_READONLY |
                                                         JSPROP_PERMANENT);
                    }
                }
            }
        }
    }
    return NS_OK;
}


/***************************************************************************/

// Currently the possible results do not change at runtime, so they are only
// cached once (unlike ContractIDs, CLSIDs, and IIDs)

class nsXPCComponents_Results :
  public nsIXPCComponents_Results,
  public nsIXPCScriptable,
  public nsIClassInfo
{
public:
    // all the interface method declarations...
    NS_DECL_ISUPPORTS
    NS_DECL_NSIXPCCOMPONENTS_RESULTS
    NS_DECL_NSIXPCSCRIPTABLE
    NS_DECL_NSICLASSINFO

public:
    nsXPCComponents_Results();
    virtual ~nsXPCComponents_Results();
};

/***************************************************************************/
/* void getInterfaces (out PRUint32 count, [array, size_is (count), retval] 
                       out nsIIDPtr array); */
NS_IMETHODIMP 
nsXPCComponents_Results::GetInterfaces(PRUint32 *aCount, nsIID * **aArray)
{
    PRUint32 count = 2;
    *aCount = count;
    nsIID **array;
    *aArray = array = static_cast<nsIID**>(nsMemory::Alloc(count * sizeof(nsIID*)));
    if(!array)
        return NS_ERROR_OUT_OF_MEMORY;

    PRUint32 index = 0;
    nsIID* clone;
#define PUSH_IID(id) \
    clone = static_cast<nsIID *>(nsMemory::Clone(&NS_GET_IID( id ),   \
                                                    sizeof(nsIID)));  \
    if (!clone)                                                       \
        goto oom;                                                     \
    array[index++] = clone;

    PUSH_IID(nsIXPCComponents_Results)
    PUSH_IID(nsIXPCScriptable)
#undef PUSH_IID

    return NS_OK;
oom:
    while (index)
        nsMemory::Free(array[--index]);
    nsMemory::Free(array);
    *aArray = nsnull;
    return NS_ERROR_OUT_OF_MEMORY;
}

/* nsISupports getHelperForLanguage (in PRUint32 language); */
NS_IMETHODIMP 
nsXPCComponents_Results::GetHelperForLanguage(PRUint32 language, 
                                      nsISupports **retval)
{
    *retval = nsnull;
    return NS_OK;
}

/* readonly attribute string contractID; */
NS_IMETHODIMP 
nsXPCComponents_Results::GetContractID(char * *aContractID)
{
    *aContractID = nsnull;
    return NS_ERROR_NOT_AVAILABLE;
}

/* readonly attribute string classDescription; */
NS_IMETHODIMP 
nsXPCComponents_Results::GetClassDescription(char * *aClassDescription)
{
    static const char classDescription[] = "XPCComponents_Interfaces";
    *aClassDescription = (char*)nsMemory::Clone(classDescription, sizeof(classDescription));
    return *aClassDescription ? NS_OK : NS_ERROR_OUT_OF_MEMORY;
}

/* readonly attribute nsCIDPtr classID; */
NS_IMETHODIMP 
nsXPCComponents_Results::GetClassID(nsCID * *aClassID)
{
    *aClassID = nsnull;
    return NS_OK;
}

/* readonly attribute PRUint32 implementationLanguage; */
NS_IMETHODIMP 
nsXPCComponents_Results::GetImplementationLanguage(
    PRUint32 *aImplementationLanguage)
{
    *aImplementationLanguage = nsIProgrammingLanguage::CPLUSPLUS;
    return NS_OK;
}

/* readonly attribute PRUint32 flags; */
NS_IMETHODIMP 
nsXPCComponents_Results::GetFlags(PRUint32 *aFlags)
{
    *aFlags = nsIClassInfo::THREADSAFE;
    return NS_OK;
}

/* [notxpcom] readonly attribute nsCID classIDNoAlloc; */
NS_IMETHODIMP 
nsXPCComponents_Results::GetClassIDNoAlloc(nsCID *aClassIDNoAlloc)
{
    return NS_ERROR_NOT_AVAILABLE;
}

nsXPCComponents_Results::nsXPCComponents_Results()
{
}

nsXPCComponents_Results::~nsXPCComponents_Results()
{
    // empty
}

NS_INTERFACE_MAP_BEGIN(nsXPCComponents_Results)
  NS_INTERFACE_MAP_ENTRY(nsIXPCComponents_Results)
  NS_INTERFACE_MAP_ENTRY(nsIXPCScriptable)
  NS_INTERFACE_MAP_ENTRY(nsIClassInfo)
  NS_INTERFACE_MAP_ENTRY_AMBIGUOUS(nsISupports, nsIXPCComponents_Results)
NS_INTERFACE_MAP_END_THREADSAFE

NS_IMPL_THREADSAFE_ADDREF(nsXPCComponents_Results)
NS_IMPL_THREADSAFE_RELEASE(nsXPCComponents_Results)

// The nsIXPCScriptable map declaration that will generate stubs for us...
#define XPC_MAP_CLASSNAME           nsXPCComponents_Results
#define XPC_MAP_QUOTED_CLASSNAME   "nsXPCComponents_Results"
#define                             XPC_MAP_WANT_NEWRESOLVE
#define                             XPC_MAP_WANT_NEWENUMERATE
#define XPC_MAP_FLAGS               nsIXPCScriptable::DONT_ENUM_STATIC_PROPS |\
                                    nsIXPCScriptable::ALLOW_PROP_MODS_DURING_RESOLVE
#include "xpc_map_end.h" /* This will #undef the above */

/* PRBool newEnumerate (in nsIXPConnectWrappedNative wrapper, in JSContextPtr cx, in JSObjectPtr obj, in PRUint32 enum_op, in JSValPtr statep, out JSID idp); */
NS_IMETHODIMP
nsXPCComponents_Results::NewEnumerate(nsIXPConnectWrappedNative *wrapper,
                                      JSContext * cx, JSObject * obj,
                                      PRUint32 enum_op, jsval * statep,
                                      jsid * idp, PRBool *_retval)
{
    void** iter;

    switch(enum_op)
    {
        case JSENUMERATE_INIT:
        case JSENUMERATE_INIT_ALL:
        {
            if(idp)
                *idp = INT_TO_JSID(nsXPCException::GetNSResultCount());

            void** space = (void**) new char[sizeof(void*)];
            *space = nsnull;
            *statep = PRIVATE_TO_JSVAL(space);
            return NS_OK;
        }
        case JSENUMERATE_NEXT:
        {
            const char* name;
            iter = (void**) JSVAL_TO_PRIVATE(*statep);
            if(nsXPCException::IterateNSResults(nsnull, &name, nsnull, iter))
            {
                JSString* idstr = JS_NewStringCopyZ(cx, name);
                if(idstr && JS_ValueToId(cx, STRING_TO_JSVAL(idstr), idp))
                    return NS_OK;
            }
            // else... FALL THROUGH
        }

        case JSENUMERATE_DESTROY:
        default:
            iter = (void**) JSVAL_TO_PRIVATE(*statep);
            delete [] (char*) iter;
            *statep = JSVAL_NULL;
            return NS_OK;
    }
}


/* PRBool newResolve (in nsIXPConnectWrappedNative wrapper, in JSContextPtr cx, in JSObjectPtr obj, in jsval id, in PRUint32 flags, out JSObjectPtr objp); */
NS_IMETHODIMP
nsXPCComponents_Results::NewResolve(nsIXPConnectWrappedNative *wrapper,
                                    JSContext * cx, JSObject * obj,
                                    jsid id, PRUint32 flags,
                                    JSObject * *objp, PRBool *_retval)
{
    const char* name = nsnull;

    if(JSID_IS_STRING(id) &&
       nsnull != (name = JS_GetStringBytes(JSID_TO_STRING(id))))
    {
        const char* rv_name;
        void* iter = nsnull;
        nsresult rv;
        while(nsXPCException::IterateNSResults(&rv, &rv_name, nsnull, &iter))
        {
            if(!strcmp(name, rv_name))
            {
                jsval val;

                *objp = obj;
                if(!JS_NewNumberValue(cx, (jsdouble)rv, &val) ||
                   !JS_DefinePropertyById(cx, obj, id, val,
                                          nsnull, nsnull,
                                          JSPROP_ENUMERATE |
                                          JSPROP_READONLY |
                                          JSPROP_PERMANENT))
                {
                    return NS_ERROR_UNEXPECTED;
                }
            }
        }
    }
    return NS_OK;
}

/***************************************************************************/
// JavaScript Constructor for nsIJSID objects (Components.ID)

class nsXPCComponents_ID :
  public nsIXPCComponents_ID,
  public nsIXPCScriptable,
  public nsIClassInfo
{
public:
    // all the interface method declarations...
    NS_DECL_ISUPPORTS
    NS_DECL_NSIXPCCOMPONENTS_ID
    NS_DECL_NSIXPCSCRIPTABLE
    NS_DECL_NSICLASSINFO


public:
    nsXPCComponents_ID();
    virtual ~nsXPCComponents_ID();

private:
    static nsresult CallOrConstruct(nsIXPConnectWrappedNative *wrapper,
                                    JSContext * cx, JSObject * obj,
                                    PRUint32 argc, jsval * argv,
                                    jsval * vp, PRBool *_retval);
};

/***************************************************************************/
/* void getInterfaces (out PRUint32 count, [array, size_is (count), retval] 
                       out nsIIDPtr array); */
NS_IMETHODIMP 
nsXPCComponents_ID::GetInterfaces(PRUint32 *aCount, nsIID * **aArray)
{
    PRUint32 count = 2;
    *aCount = count;
    nsIID **array;
    *aArray = array = static_cast<nsIID**>(nsMemory::Alloc(count * sizeof(nsIID*)));
    if(!array)
        return NS_ERROR_OUT_OF_MEMORY;

    PRUint32 index = 0;
    nsIID* clone;
#define PUSH_IID(id) \
    clone = static_cast<nsIID *>(nsMemory::Clone(&NS_GET_IID( id ),   \
                                                    sizeof(nsIID)));  \
    if (!clone)                                                       \
        goto oom;                                                     \
    array[index++] = clone;

    PUSH_IID(nsIXPCComponents_ID)
    PUSH_IID(nsIXPCScriptable)
#undef PUSH_IID

    return NS_OK;
oom:
    while (index)
        nsMemory::Free(array[--index]);
    nsMemory::Free(array);
    *aArray = nsnull;
    return NS_ERROR_OUT_OF_MEMORY;
}

/* nsISupports getHelperForLanguage (in PRUint32 language); */
NS_IMETHODIMP 
nsXPCComponents_ID::GetHelperForLanguage(PRUint32 language, 
                                      nsISupports **retval)
{
    *retval = nsnull;
    return NS_OK;
}

/* readonly attribute string contractID; */
NS_IMETHODIMP 
nsXPCComponents_ID::GetContractID(char * *aContractID)
{
    *aContractID = nsnull;
    return NS_ERROR_NOT_AVAILABLE;
}

/* readonly attribute string classDescription; */
NS_IMETHODIMP 
nsXPCComponents_ID::GetClassDescription(char * *aClassDescription)
{
    static const char classDescription[] = "XPCComponents_Interfaces";
    *aClassDescription = (char*)nsMemory::Clone(classDescription, sizeof(classDescription));
    return *aClassDescription ? NS_OK : NS_ERROR_OUT_OF_MEMORY;
}

/* readonly attribute nsCIDPtr classID; */
NS_IMETHODIMP 
nsXPCComponents_ID::GetClassID(nsCID * *aClassID)
{
    *aClassID = nsnull;
    return NS_OK;
}

/* readonly attribute PRUint32 implementationLanguage; */
NS_IMETHODIMP 
nsXPCComponents_ID::GetImplementationLanguage(
    PRUint32 *aImplementationLanguage)
{
    *aImplementationLanguage = nsIProgrammingLanguage::CPLUSPLUS;
    return NS_OK;
}

/* readonly attribute PRUint32 flags; */
NS_IMETHODIMP 
nsXPCComponents_ID::GetFlags(PRUint32 *aFlags)
{
    *aFlags = nsIClassInfo::THREADSAFE;
    return NS_OK;
}

/* [notxpcom] readonly attribute nsCID classIDNoAlloc; */
NS_IMETHODIMP 
nsXPCComponents_ID::GetClassIDNoAlloc(nsCID *aClassIDNoAlloc)
{
    return NS_ERROR_NOT_AVAILABLE;
}

nsXPCComponents_ID::nsXPCComponents_ID()
{
}

nsXPCComponents_ID::~nsXPCComponents_ID()
{
    // empty
}

NS_INTERFACE_MAP_BEGIN(nsXPCComponents_ID)
  NS_INTERFACE_MAP_ENTRY(nsIXPCComponents_ID)
  NS_INTERFACE_MAP_ENTRY(nsIXPCScriptable)
  NS_INTERFACE_MAP_ENTRY(nsIClassInfo)
  NS_INTERFACE_MAP_ENTRY_AMBIGUOUS(nsISupports, nsIXPCComponents_ID)
NS_INTERFACE_MAP_END_THREADSAFE

NS_IMPL_THREADSAFE_ADDREF(nsXPCComponents_ID)
NS_IMPL_THREADSAFE_RELEASE(nsXPCComponents_ID)

// The nsIXPCScriptable map declaration that will generate stubs for us...
#define XPC_MAP_CLASSNAME           nsXPCComponents_ID
#define XPC_MAP_QUOTED_CLASSNAME   "nsXPCComponents_ID"
#define                             XPC_MAP_WANT_CALL
#define                             XPC_MAP_WANT_CONSTRUCT
#define                             XPC_MAP_WANT_HASINSTANCE
#define XPC_MAP_FLAGS               0
#include "xpc_map_end.h" /* This will #undef the above */


/* PRBool call (in nsIXPConnectWrappedNative wrapper, in JSContextPtr cx, in JSObjectPtr obj, in PRUint32 argc, in JSValPtr argv, in JSValPtr vp); */
NS_IMETHODIMP
nsXPCComponents_ID::Call(nsIXPConnectWrappedNative *wrapper, JSContext * cx, JSObject * obj, PRUint32 argc, jsval * argv, jsval * vp, PRBool *_retval)
{
    return CallOrConstruct(wrapper, cx, obj, argc, argv, vp, _retval);

}

/* PRBool construct (in nsIXPConnectWrappedNative wrapper, in JSContextPtr cx, in JSObjectPtr obj, in PRUint32 argc, in JSValPtr argv, in JSValPtr vp); */
NS_IMETHODIMP
nsXPCComponents_ID::Construct(nsIXPConnectWrappedNative *wrapper, JSContext * cx, JSObject * obj, PRUint32 argc, jsval * argv, jsval * vp, PRBool *_retval)
{
    return CallOrConstruct(wrapper, cx, obj, argc, argv, vp, _retval);
}

// static
nsresult
nsXPCComponents_ID::CallOrConstruct(nsIXPConnectWrappedNative *wrapper,
                                    JSContext * cx, JSObject * obj,
                                    PRUint32 argc, jsval * argv,
                                    jsval * vp, PRBool *_retval)
{
    // make sure we have at least one arg

    if(!argc)
        return ThrowAndFail(NS_ERROR_XPC_NOT_ENOUGH_ARGS, cx, _retval);

    XPCCallContext ccx(JS_CALLER, cx);
    if(!ccx.IsValid())
        return ThrowAndFail(NS_ERROR_XPC_UNEXPECTED, cx, _retval);

    XPCContext* xpcc = ccx.GetXPCContext();

    // Do the security check if necessary

    nsIXPCSecurityManager* sm =
            xpcc->GetAppropriateSecurityManager(
                        nsIXPCSecurityManager::HOOK_CREATE_INSTANCE);
    if(sm && NS_FAILED(sm->CanCreateInstance(cx, nsJSID::GetCID())))
    {
        // the security manager vetoed. It should have set an exception.
        *_retval = JS_FALSE;
        return NS_OK;
    }

    // convert the first argument into a string and see if it looks like an id

    JSString* jsstr;
    const char* str;
    nsID id;

    if(!(jsstr = JS_ValueToString(cx, argv[0])) ||
       !(str = JS_GetStringBytes(jsstr)) ||
       ! id.Parse(str))
    {
        return ThrowAndFail(NS_ERROR_XPC_BAD_ID_STRING, cx, _retval);
    }

    // make the new object and return it.

    JSObject* newobj = xpc_NewIDObject(cx, obj, id);

    if(vp)
        *vp = OBJECT_TO_JSVAL(newobj);

    return NS_OK;
}

/* PRBool hasInstance (in nsIXPConnectWrappedNative wrapper, in JSContextPtr cx, in JSObjectPtr obj, in jsval val, out PRBool bp); */
NS_IMETHODIMP
nsXPCComponents_ID::HasInstance(nsIXPConnectWrappedNative *wrapper,
                                JSContext * cx, JSObject * obj,
                                const jsval &val, PRBool *bp, PRBool *_retval)
{
    if(bp)
        *bp = JSValIsInterfaceOfType(cx, val, NS_GET_IID(nsIJSID));
    return NS_OK;
}

/***************************************************************************/
// JavaScript Constructor for nsIXPCException objects (Components.Exception)

class nsXPCComponents_Exception :
  public nsIXPCComponents_Exception,
  public nsIXPCScriptable,
  public nsIClassInfo
{
public:
    // all the interface method declarations...
    NS_DECL_ISUPPORTS
    NS_DECL_NSIXPCCOMPONENTS_EXCEPTION
    NS_DECL_NSIXPCSCRIPTABLE
    NS_DECL_NSICLASSINFO


public:
    nsXPCComponents_Exception();
    virtual ~nsXPCComponents_Exception();

private:
    static nsresult CallOrConstruct(nsIXPConnectWrappedNative *wrapper,
                                    JSContext * cx, JSObject * obj,
                                    PRUint32 argc, jsval * argv,
                                    jsval * vp, PRBool *_retval);
};

/***************************************************************************/
/* void getInterfaces (out PRUint32 count, [array, size_is (count), retval] 
                       out nsIIDPtr array); */
NS_IMETHODIMP 
nsXPCComponents_Exception::GetInterfaces(PRUint32 *aCount, nsIID * **aArray)
{
    PRUint32 count = 2;
    *aCount = count;
    nsIID **array;
    *aArray = array = static_cast<nsIID**>(nsMemory::Alloc(count * sizeof(nsIID*)));
    if(!array)
        return NS_ERROR_OUT_OF_MEMORY;

    PRUint32 index = 0;
    nsIID* clone;
#define PUSH_IID(id) \
    clone = static_cast<nsIID *>(nsMemory::Clone(&NS_GET_IID( id ),   \
                                                    sizeof(nsIID)));  \
    if (!clone)                                                       \
        goto oom;                                                     \
    array[index++] = clone;

    PUSH_IID(nsIXPCComponents_Exception)
    PUSH_IID(nsIXPCScriptable)
#undef PUSH_IID

    return NS_OK;
oom:
    while (index)
        nsMemory::Free(array[--index]);
    nsMemory::Free(array);
    *aArray = nsnull;
    return NS_ERROR_OUT_OF_MEMORY;
}

/* nsISupports getHelperForLanguage (in PRUint32 language); */
NS_IMETHODIMP 
nsXPCComponents_Exception::GetHelperForLanguage(PRUint32 language, 
                                      nsISupports **retval)
{
    *retval = nsnull;
    return NS_OK;
}

/* readonly attribute string contractID; */
NS_IMETHODIMP 
nsXPCComponents_Exception::GetContractID(char * *aContractID)
{
    *aContractID = nsnull;
    return NS_ERROR_NOT_AVAILABLE;
}

/* readonly attribute string classDescription; */
NS_IMETHODIMP 
nsXPCComponents_Exception::GetClassDescription(char * *aClassDescription)
{
    static const char classDescription[] = "XPCComponents_Interfaces";
    *aClassDescription = (char*)nsMemory::Clone(classDescription, sizeof(classDescription));
    return *aClassDescription ? NS_OK : NS_ERROR_OUT_OF_MEMORY;
}

/* readonly attribute nsCIDPtr classID; */
NS_IMETHODIMP 
nsXPCComponents_Exception::GetClassID(nsCID * *aClassID)
{
    *aClassID = nsnull;
    return NS_OK;
}

/* readonly attribute PRUint32 implementationLanguage; */
NS_IMETHODIMP 
nsXPCComponents_Exception::GetImplementationLanguage(
    PRUint32 *aImplementationLanguage)
{
    *aImplementationLanguage = nsIProgrammingLanguage::CPLUSPLUS;
    return NS_OK;
}

/* readonly attribute PRUint32 flags; */
NS_IMETHODIMP 
nsXPCComponents_Exception::GetFlags(PRUint32 *aFlags)
{
    *aFlags = nsIClassInfo::THREADSAFE;
    return NS_OK;
}

/* [notxpcom] readonly attribute nsCID classIDNoAlloc; */
NS_IMETHODIMP 
nsXPCComponents_Exception::GetClassIDNoAlloc(nsCID *aClassIDNoAlloc)
{
    return NS_ERROR_NOT_AVAILABLE;
}

nsXPCComponents_Exception::nsXPCComponents_Exception()
{
}

nsXPCComponents_Exception::~nsXPCComponents_Exception()
{
    // empty
}

NS_INTERFACE_MAP_BEGIN(nsXPCComponents_Exception)
  NS_INTERFACE_MAP_ENTRY(nsIXPCComponents_Exception)
  NS_INTERFACE_MAP_ENTRY(nsIXPCScriptable)
  NS_INTERFACE_MAP_ENTRY(nsIClassInfo)
  NS_INTERFACE_MAP_ENTRY_AMBIGUOUS(nsISupports, nsIXPCComponents_Exception)
NS_INTERFACE_MAP_END_THREADSAFE

NS_IMPL_THREADSAFE_ADDREF(nsXPCComponents_Exception)
NS_IMPL_THREADSAFE_RELEASE(nsXPCComponents_Exception)

// The nsIXPCScriptable map declaration that will generate stubs for us...
#define XPC_MAP_CLASSNAME           nsXPCComponents_Exception
#define XPC_MAP_QUOTED_CLASSNAME   "nsXPCComponents_Exception"
#define                             XPC_MAP_WANT_CALL
#define                             XPC_MAP_WANT_CONSTRUCT
#define                             XPC_MAP_WANT_HASINSTANCE
#define XPC_MAP_FLAGS               0
#include "xpc_map_end.h" /* This will #undef the above */


/* PRBool call (in nsIXPConnectWrappedNative wrapper, in JSContextPtr cx, in JSObjectPtr obj, in PRUint32 argc, in JSValPtr argv, in JSValPtr vp); */
NS_IMETHODIMP
nsXPCComponents_Exception::Call(nsIXPConnectWrappedNative *wrapper, JSContext * cx, JSObject * obj, PRUint32 argc, jsval * argv, jsval * vp, PRBool *_retval)
{
    return CallOrConstruct(wrapper, cx, obj, argc, argv, vp, _retval);

}

/* PRBool construct (in nsIXPConnectWrappedNative wrapper, in JSContextPtr cx, in JSObjectPtr obj, in PRUint32 argc, in JSValPtr argv, in JSValPtr vp); */
NS_IMETHODIMP
nsXPCComponents_Exception::Construct(nsIXPConnectWrappedNative *wrapper, JSContext * cx, JSObject * obj, PRUint32 argc, jsval * argv, jsval * vp, PRBool *_retval)
{
    return CallOrConstruct(wrapper, cx, obj, argc, argv, vp, _retval);
}

// static
nsresult
nsXPCComponents_Exception::CallOrConstruct(nsIXPConnectWrappedNative *wrapper,
                                           JSContext * cx, JSObject * obj,
                                           PRUint32 argc, jsval * argv,
                                           jsval * vp, PRBool *_retval)
{
    XPCCallContext ccx(JS_CALLER, cx);
    if(!ccx.IsValid())
        return ThrowAndFail(NS_ERROR_XPC_UNEXPECTED, cx, _retval);

    nsXPConnect* xpc = ccx.GetXPConnect();
    XPCContext* xpcc = ccx.GetXPCContext();

    // Do the security check if necessary

    nsIXPCSecurityManager* sm =
            xpcc->GetAppropriateSecurityManager(
                        nsIXPCSecurityManager::HOOK_CREATE_INSTANCE);
    if(sm && NS_FAILED(sm->CanCreateInstance(cx, nsXPCException::GetCID())))
    {
        // the security manager vetoed. It should have set an exception.
        *_retval = JS_FALSE;
        return NS_OK;
    }

    // initialization params for the exception object we will create
    const char*             eMsg = "exception";
    nsresult                eResult = NS_ERROR_FAILURE;
    nsCOMPtr<nsIStackFrame> eStack;
    nsCOMPtr<nsISupports>   eData;

    // all params are optional - grab any passed in
    switch(argc)
    {
        default:    // more than 4 - ignore extra
            // ...fall through...
        case 4:     // argv[3] is object for eData
            if(JSVAL_IS_NULL(argv[3]))
            {
                // do nothing, leave eData as null
            }
            else
            {
                if(JSVAL_IS_PRIMITIVE(argv[3]) ||
                   NS_FAILED(xpc->WrapJS(cx, JSVAL_TO_OBJECT(argv[3]),
                                         NS_GET_IID(nsISupports),
                                         (void**)getter_AddRefs(eData))))
                    return ThrowAndFail(NS_ERROR_XPC_BAD_CONVERT_JS, cx, _retval);
            }
            // ...fall through...
        case 3:     // argv[2] is object for eStack
            if(JSVAL_IS_NULL(argv[2]))
            {
                // do nothing, leave eStack as null
            }
            else
            {
                if(JSVAL_IS_PRIMITIVE(argv[2]) ||
                   NS_FAILED(xpc->WrapJS(cx, JSVAL_TO_OBJECT(argv[2]),
                                         NS_GET_IID(nsIStackFrame),
                                         (void**)getter_AddRefs(eStack))))
                    return ThrowAndFail(NS_ERROR_XPC_BAD_CONVERT_JS, cx, _retval);
            }
            // fall through...
        case 2:     // argv[1] is nsresult for eResult
            if(!JS_ValueToECMAInt32(cx, argv[1], (int32*) &eResult))
                return ThrowAndFail(NS_ERROR_XPC_BAD_CONVERT_JS, cx, _retval);
            // ...fall through...
        case 1:     // argv[0] is string for eMsg
            {
                JSString* str = JS_ValueToString(cx, argv[0]);
                if(!str || !(eMsg = JS_GetStringBytes(str)))
                    return ThrowAndFail(NS_ERROR_XPC_BAD_CONVERT_JS, cx, _retval);
            }
            // ...fall through...
        case 0: // this case required so that 'default' does not include zero.
            ;   // -- do nothing --
    }

    nsCOMPtr<nsIException> e;
    nsXPCException::NewException(eMsg, eResult, eStack, eData, getter_AddRefs(e));
    if(!e)
        return ThrowAndFail(NS_ERROR_XPC_UNEXPECTED, cx, _retval);

    nsCOMPtr<nsIXPConnectJSObjectHolder> holder;
    JSObject* newObj = nsnull;

    if(NS_FAILED(xpc->WrapNative(cx, obj, e, NS_GET_IID(nsIXPCException),
                 getter_AddRefs(holder))) || !holder ||
       NS_FAILED(holder->GetJSObject(&newObj)) || !newObj)
    {
        return ThrowAndFail(NS_ERROR_XPC_CANT_CREATE_WN, cx, _retval);
    }

    if(vp)
        *vp = OBJECT_TO_JSVAL(newObj);

    return NS_OK;
}

/* PRBool hasInstance (in nsIXPConnectWrappedNative wrapper, in JSContextPtr cx, in JSObjectPtr obj, in jsval val, out PRBool bp); */
NS_IMETHODIMP
nsXPCComponents_Exception::HasInstance(nsIXPConnectWrappedNative *wrapper,
                                       JSContext * cx, JSObject * obj,
                                       const jsval &val, PRBool *bp,
                                       PRBool *_retval)
{
    if(bp)
        *bp = JSValIsInterfaceOfType(cx, val, NS_GET_IID(nsIException));
    return NS_OK;
}

/***************************************************************************/
// This class is for the thing returned by "new Component.Constructor".

// XXXjband we use this CID for security check, but security system can't see
// it since it has no registed factory. Security really kicks in when we try
// to build a wrapper around an instance.

// {B4A95150-E25A-11d3-8F61-0010A4E73D9A}
#define NS_XPCCONSTRUCTOR_CID  \
{ 0xb4a95150, 0xe25a, 0x11d3, \
    { 0x8f, 0x61, 0x0, 0x10, 0xa4, 0xe7, 0x3d, 0x9a } }

class nsXPCConstructor :
  public nsIXPCConstructor,
  public nsIXPCScriptable,
  public nsIClassInfo
{
public:
    NS_DEFINE_STATIC_CID_ACCESSOR(NS_XPCCONSTRUCTOR_CID)
public:
    // all the interface method declarations...
    NS_DECL_ISUPPORTS
    NS_DECL_NSIXPCCONSTRUCTOR
    NS_DECL_NSIXPCSCRIPTABLE
    NS_DECL_NSICLASSINFO

public:
    nsXPCConstructor(); // not implemented
    nsXPCConstructor(nsIJSCID* aClassID,
                     nsIJSIID* aInterfaceID,
                     const char* aInitializer);
    virtual ~nsXPCConstructor();

private:
    nsresult CallOrConstruct(nsIXPConnectWrappedNative *wrapper,
                             JSContext * cx, JSObject * obj,
                             PRUint32 argc, jsval * argv,
                             jsval * vp, PRBool *_retval);
private:
    nsIJSCID* mClassID;
    nsIJSIID* mInterfaceID;
    char*     mInitializer;
};

/***************************************************************************/
/* void getInterfaces (out PRUint32 count, [array, size_is (count), retval] 
                       out nsIIDPtr array); */
NS_IMETHODIMP 
nsXPCConstructor::GetInterfaces(PRUint32 *aCount, nsIID * **aArray)
{
    PRUint32 count = 2;
    *aCount = count;
    nsIID **array;
    *aArray = array = static_cast<nsIID**>(nsMemory::Alloc(count * sizeof(nsIID*)));
    if(!array)
        return NS_ERROR_OUT_OF_MEMORY;

    PRUint32 index = 0;
    nsIID* clone;
#define PUSH_IID(id) \
    clone = static_cast<nsIID *>(nsMemory::Clone(&NS_GET_IID( id ),   \
                                                    sizeof(nsIID)));  \
    if (!clone)                                                       \
        goto oom;                                                     \
    array[index++] = clone;

    PUSH_IID(nsIXPCConstructor)
    PUSH_IID(nsIXPCScriptable)
#undef PUSH_IID

    return NS_OK;
oom:
    while (index)
        nsMemory::Free(array[--index]);
    nsMemory::Free(array);
    *aArray = nsnull;
    return NS_ERROR_OUT_OF_MEMORY;
}

/* nsISupports getHelperForLanguage (in PRUint32 language); */
NS_IMETHODIMP 
nsXPCConstructor::GetHelperForLanguage(PRUint32 language, 
                                      nsISupports **retval)
{
    *retval = nsnull;
    return NS_OK;
}

/* readonly attribute string contractID; */
NS_IMETHODIMP 
nsXPCConstructor::GetContractID(char * *aContractID)
{
    *aContractID = nsnull;
    return NS_ERROR_NOT_AVAILABLE;
}

/* readonly attribute string classDescription; */
NS_IMETHODIMP 
nsXPCConstructor::GetClassDescription(char * *aClassDescription)
{
    static const char classDescription[] = "XPCComponents_Interfaces";
    *aClassDescription = (char*)nsMemory::Clone(classDescription, sizeof(classDescription));
    return *aClassDescription ? NS_OK : NS_ERROR_OUT_OF_MEMORY;
}

/* readonly attribute nsCIDPtr classID; */
NS_IMETHODIMP 
nsXPCConstructor::GetClassID(nsCID * *aClassID)
{
    *aClassID = nsnull;
    return NS_OK;
}

/* readonly attribute PRUint32 implementationLanguage; */
NS_IMETHODIMP 
nsXPCConstructor::GetImplementationLanguage(
    PRUint32 *aImplementationLanguage)
{
    *aImplementationLanguage = nsIProgrammingLanguage::CPLUSPLUS;
    return NS_OK;
}

/* readonly attribute PRUint32 flags; */
NS_IMETHODIMP 
nsXPCConstructor::GetFlags(PRUint32 *aFlags)
{
    *aFlags = nsIClassInfo::THREADSAFE;
    return NS_OK;
}

/* [notxpcom] readonly attribute nsCID classIDNoAlloc; */
NS_IMETHODIMP 
nsXPCConstructor::GetClassIDNoAlloc(nsCID *aClassIDNoAlloc)
{
    return NS_ERROR_NOT_AVAILABLE;
}

nsXPCConstructor::nsXPCConstructor(nsIJSCID* aClassID,
                                   nsIJSIID* aInterfaceID,
                                   const char* aInitializer)
{
    NS_IF_ADDREF(mClassID = aClassID);
    NS_IF_ADDREF(mInterfaceID = aInterfaceID);
    mInitializer = aInitializer ?
        (char*) nsMemory::Clone(aInitializer, strlen(aInitializer)+1) :
        nsnull;
}

nsXPCConstructor::~nsXPCConstructor()
{
    NS_IF_RELEASE(mClassID);
    NS_IF_RELEASE(mInterfaceID);
    if(mInitializer)
        nsMemory::Free(mInitializer);
}

/* readonly attribute nsIJSCID classID; */
NS_IMETHODIMP
nsXPCConstructor::GetClassID(nsIJSCID * *aClassID)
{
    NS_IF_ADDREF(*aClassID = mClassID);
    return NS_OK;
}

/* readonly attribute nsIJSIID interfaceID; */
NS_IMETHODIMP
nsXPCConstructor::GetInterfaceID(nsIJSIID * *aInterfaceID)
{
    NS_IF_ADDREF(*aInterfaceID = mInterfaceID);
    return NS_OK;
}

/* readonly attribute string initializer; */
NS_IMETHODIMP
nsXPCConstructor::GetInitializer(char * *aInitializer)
{
    XPC_STRING_GETTER_BODY(aInitializer, mInitializer);
}

NS_INTERFACE_MAP_BEGIN(nsXPCConstructor)
  NS_INTERFACE_MAP_ENTRY(nsIXPCConstructor)
  NS_INTERFACE_MAP_ENTRY(nsIXPCScriptable)
  NS_INTERFACE_MAP_ENTRY(nsIClassInfo)
  NS_INTERFACE_MAP_ENTRY_AMBIGUOUS(nsISupports, nsIXPCConstructor)
NS_INTERFACE_MAP_END_THREADSAFE

NS_IMPL_THREADSAFE_ADDREF(nsXPCConstructor)
NS_IMPL_THREADSAFE_RELEASE(nsXPCConstructor)

// The nsIXPCScriptable map declaration that will generate stubs for us...
#define XPC_MAP_CLASSNAME           nsXPCConstructor
#define XPC_MAP_QUOTED_CLASSNAME   "nsXPCConstructor"
#define                             XPC_MAP_WANT_CALL
#define                             XPC_MAP_WANT_CONSTRUCT
#define XPC_MAP_FLAGS               0
#include "xpc_map_end.h" /* This will #undef the above */


/* PRBool call (in nsIXPConnectWrappedNative wrapper, in JSContextPtr cx, in JSObjectPtr obj, in PRUint32 argc, in JSValPtr argv, in JSValPtr vp); */
NS_IMETHODIMP
nsXPCConstructor::Call(nsIXPConnectWrappedNative *wrapper, JSContext * cx, JSObject * obj, PRUint32 argc, jsval * argv, jsval * vp, PRBool *_retval)
{
    return CallOrConstruct(wrapper, cx, obj, argc, argv, vp, _retval);

}

/* PRBool construct (in nsIXPConnectWrappedNative wrapper, in JSContextPtr cx, in JSObjectPtr obj, in PRUint32 argc, in JSValPtr argv, in JSValPtr vp); */
NS_IMETHODIMP
nsXPCConstructor::Construct(nsIXPConnectWrappedNative *wrapper, JSContext * cx, JSObject * obj, PRUint32 argc, jsval * argv, jsval * vp, PRBool *_retval)
{
    return CallOrConstruct(wrapper, cx, obj, argc, argv, vp, _retval);
}

// static
nsresult
nsXPCConstructor::CallOrConstruct(nsIXPConnectWrappedNative *wrapper,
                                  JSContext * cx, JSObject * obj,
                                  PRUint32 argc, jsval * argv,
                                  jsval * vp, PRBool *_retval)
{
    XPCCallContext ccx(JS_CALLER, cx);
    if(!ccx.IsValid())
        return ThrowAndFail(NS_ERROR_XPC_UNEXPECTED, cx, _retval);

    nsXPConnect* xpc = ccx.GetXPConnect();

    // security check not required because we are going to call through the
    // code which is reflected into JS which will do that for us later.

    nsCOMPtr<nsIXPConnectJSObjectHolder> cidHolder;
    nsCOMPtr<nsIXPConnectJSObjectHolder> iidHolder;
    JSObject* cidObj;
    JSObject* iidObj;

    if(NS_FAILED(xpc->WrapNative(cx, obj, mClassID, NS_GET_IID(nsIJSCID),
                 getter_AddRefs(cidHolder))) || !cidHolder ||
       NS_FAILED(cidHolder->GetJSObject(&cidObj)) || !cidObj ||
       NS_FAILED(xpc->WrapNative(cx, obj, mInterfaceID, NS_GET_IID(nsIJSIID),
                 getter_AddRefs(iidHolder))) || !iidHolder ||
       NS_FAILED(iidHolder->GetJSObject(&iidObj)) || !iidObj)
    {
        return ThrowAndFail(NS_ERROR_XPC_CANT_CREATE_WN, cx, _retval);
    }

    jsval ctorArgs[1] = {OBJECT_TO_JSVAL(iidObj)};
    jsval val;

    if(!JS_CallFunctionName(cx, cidObj, "createInstance", 1, ctorArgs, &val) ||
       JSVAL_IS_PRIMITIVE(val))
    {
        // createInstance will have thrown an exception
        *_retval = JS_FALSE;
        return NS_OK;
    }

    // root the result
    if(vp)
        *vp = val;

    // call initializer method if supplied
    if(mInitializer)
    {
        JSObject* newObj = JSVAL_TO_OBJECT(val);
        jsval fun;
        jsval ignored;

        // first check existence of function property for better error reporting
        if(!JS_GetProperty(cx, newObj, mInitializer, &fun) ||
           JSVAL_IS_PRIMITIVE(fun))
        {
            return ThrowAndFail(NS_ERROR_XPC_BAD_INITIALIZER_NAME, cx, _retval);
        }

        if(!JS_CallFunctionValue(cx, newObj, fun, argc, argv, &ignored))
        {
            // function should have thrown an exception
            *_retval = JS_FALSE;
            return NS_OK;
        }
    }

    return NS_OK;
}

/*******************************************************/
// JavaScript Constructor for nsIXPCConstructor objects (Components.Constructor)

class nsXPCComponents_Constructor :
  public nsIXPCComponents_Constructor,
  public nsIXPCScriptable,
  public nsIClassInfo
{
public:
    // all the interface method declarations...
    NS_DECL_ISUPPORTS
    NS_DECL_NSIXPCCOMPONENTS_CONSTRUCTOR
    NS_DECL_NSIXPCSCRIPTABLE
    NS_DECL_NSICLASSINFO

public:
    nsXPCComponents_Constructor();
    virtual ~nsXPCComponents_Constructor();

private:
    static nsresult CallOrConstruct(nsIXPConnectWrappedNative *wrapper,
                                    JSContext * cx, JSObject * obj,
                                    PRUint32 argc, jsval * argv,
                                    jsval * vp, PRBool *_retval);
};

/***************************************************************************/
/* void getInterfaces (out PRUint32 count, [array, size_is (count), retval] 
                       out nsIIDPtr array); */
NS_IMETHODIMP 
nsXPCComponents_Constructor::GetInterfaces(PRUint32 *aCount, nsIID * **aArray)
{
    PRUint32 count = 2;
    *aCount = count;
    nsIID **array;
    *aArray = array = static_cast<nsIID**>(nsMemory::Alloc(count * sizeof(nsIID*)));
    if(!array)
        return NS_ERROR_OUT_OF_MEMORY;

    PRUint32 index = 0;
    nsIID* clone;
#define PUSH_IID(id) \
    clone = static_cast<nsIID *>(nsMemory::Clone(&NS_GET_IID( id ),   \
                                                    sizeof(nsIID)));  \
    if (!clone)                                                       \
        goto oom;                                                     \
    array[index++] = clone;

    PUSH_IID(nsIXPCComponents_Constructor)
    PUSH_IID(nsIXPCScriptable)
#undef PUSH_IID

    return NS_OK;
oom:
    while (index)
        nsMemory::Free(array[--index]);
    nsMemory::Free(array);
    *aArray = nsnull;
    return NS_ERROR_OUT_OF_MEMORY;
}

/* nsISupports getHelperForLanguage (in PRUint32 language); */
NS_IMETHODIMP 
nsXPCComponents_Constructor::GetHelperForLanguage(PRUint32 language, 
                                      nsISupports **retval)
{
    *retval = nsnull;
    return NS_OK;
}

/* readonly attribute string contractID; */
NS_IMETHODIMP 
nsXPCComponents_Constructor::GetContractID(char * *aContractID)
{
    *aContractID = nsnull;
    return NS_ERROR_NOT_AVAILABLE;
}

/* readonly attribute string classDescription; */
NS_IMETHODIMP 
nsXPCComponents_Constructor::GetClassDescription(char * *aClassDescription)
{
    static const char classDescription[] = "XPCComponents_Interfaces";
    *aClassDescription = (char*)nsMemory::Clone(classDescription, sizeof(classDescription));
    return *aClassDescription ? NS_OK : NS_ERROR_OUT_OF_MEMORY;
}

/* readonly attribute nsCIDPtr classID; */
NS_IMETHODIMP 
nsXPCComponents_Constructor::GetClassID(nsCID * *aClassID)
{
    *aClassID = nsnull;
    return NS_OK;
}

/* readonly attribute PRUint32 implementationLanguage; */
NS_IMETHODIMP 
nsXPCComponents_Constructor::GetImplementationLanguage(
    PRUint32 *aImplementationLanguage)
{
    *aImplementationLanguage = nsIProgrammingLanguage::CPLUSPLUS;
    return NS_OK;
}

/* readonly attribute PRUint32 flags; */
NS_IMETHODIMP 
nsXPCComponents_Constructor::GetFlags(PRUint32 *aFlags)
{
    *aFlags = nsIClassInfo::THREADSAFE;
    return NS_OK;
}

/* [notxpcom] readonly attribute nsCID classIDNoAlloc; */
NS_IMETHODIMP 
nsXPCComponents_Constructor::GetClassIDNoAlloc(nsCID *aClassIDNoAlloc)
{
    return NS_ERROR_NOT_AVAILABLE;
}

nsXPCComponents_Constructor::nsXPCComponents_Constructor()
{
}

nsXPCComponents_Constructor::~nsXPCComponents_Constructor()
{
    // empty
}

NS_INTERFACE_MAP_BEGIN(nsXPCComponents_Constructor)
  NS_INTERFACE_MAP_ENTRY(nsIXPCComponents_Constructor)
  NS_INTERFACE_MAP_ENTRY(nsIXPCScriptable)
  NS_INTERFACE_MAP_ENTRY(nsIClassInfo)
  NS_INTERFACE_MAP_ENTRY_AMBIGUOUS(nsISupports, nsIXPCComponents_Constructor)
NS_INTERFACE_MAP_END_THREADSAFE

NS_IMPL_THREADSAFE_ADDREF(nsXPCComponents_Constructor)
NS_IMPL_THREADSAFE_RELEASE(nsXPCComponents_Constructor)

// The nsIXPCScriptable map declaration that will generate stubs for us...
#define XPC_MAP_CLASSNAME           nsXPCComponents_Constructor
#define XPC_MAP_QUOTED_CLASSNAME   "nsXPCComponents_Constructor"
#define                             XPC_MAP_WANT_CALL
#define                             XPC_MAP_WANT_CONSTRUCT
#define                             XPC_MAP_WANT_HASINSTANCE
#define XPC_MAP_FLAGS               0
#include "xpc_map_end.h" /* This will #undef the above */


/* PRBool call (in nsIXPConnectWrappedNative wrapper, in JSContextPtr cx, in JSObjectPtr obj, in PRUint32 argc, in JSValPtr argv, in JSValPtr vp); */
NS_IMETHODIMP
nsXPCComponents_Constructor::Call(nsIXPConnectWrappedNative *wrapper, JSContext * cx, JSObject * obj, PRUint32 argc, jsval * argv, jsval * vp, PRBool *_retval)
{
    return CallOrConstruct(wrapper, cx, obj, argc, argv, vp, _retval);
}

/* PRBool construct (in nsIXPConnectWrappedNative wrapper, in JSContextPtr cx, in JSObjectPtr obj, in PRUint32 argc, in JSValPtr argv, in JSValPtr vp); */
NS_IMETHODIMP
nsXPCComponents_Constructor::Construct(nsIXPConnectWrappedNative *wrapper, JSContext * cx, JSObject * obj, PRUint32 argc, jsval * argv, jsval * vp, PRBool *_retval)
{
    return CallOrConstruct(wrapper, cx, obj, argc, argv, vp, _retval);
}

// static
nsresult
nsXPCComponents_Constructor::CallOrConstruct(nsIXPConnectWrappedNative *wrapper,
                                             JSContext * cx, JSObject * obj,
                                             PRUint32 argc, jsval * argv,
                                             jsval * vp, PRBool *_retval)
{
    // make sure we have at least one arg

    if(!argc)
        return ThrowAndFail(NS_ERROR_XPC_NOT_ENOUGH_ARGS, cx, _retval);

    // get the various other object pointers we need

    XPCCallContext ccx(JS_CALLER, cx);
    if(!ccx.IsValid())
        return ThrowAndFail(NS_ERROR_XPC_UNEXPECTED, cx, _retval);

    nsXPConnect* xpc = ccx.GetXPConnect();
    XPCContext* xpcc = ccx.GetXPCContext();
    XPCWrappedNativeScope* scope =
        XPCWrappedNativeScope::FindInJSObjectScope(ccx, obj);
    nsXPCComponents* comp;

    if(!xpc || !xpcc || !scope || !(comp = scope->GetComponents()))
        return ThrowAndFail(NS_ERROR_XPC_UNEXPECTED, cx, _retval);

    // Do the security check if necessary

    nsIXPCSecurityManager* sm =
            xpcc->GetAppropriateSecurityManager(
                        nsIXPCSecurityManager::HOOK_CREATE_INSTANCE);
    if(sm && NS_FAILED(sm->CanCreateInstance(cx, nsXPCConstructor::GetCID())))
    {
        // the security manager vetoed. It should have set an exception.
        *_retval = JS_FALSE;
        return NS_OK;
    }

    // initialization params for the Constructor object we will create
    nsCOMPtr<nsIJSCID> cClassID;
    nsCOMPtr<nsIJSIID> cInterfaceID;
    const char*        cInitializer = nsnull;

    if(argc >= 3)
    {
        // argv[2] is an initializer function or property name
        JSString* str = JS_ValueToString(cx, argv[2]);
        if(!str || !(cInitializer = JS_GetStringBytes(str)))
            return ThrowAndFail(NS_ERROR_XPC_BAD_CONVERT_JS, cx, _retval);
    }

    if(argc >= 2)
    {
        // argv[1] is an iid name string
        // XXXjband support passing "Components.interfaces.foo"?

        nsCOMPtr<nsIScriptableInterfaces> ifaces;
        nsCOMPtr<nsIXPConnectJSObjectHolder> holder;
        JSObject* ifacesObj = nsnull;

        // we do the lookup by asking the Components.interfaces object
        // for the property with this name - i.e. we let its caching of these
        // nsIJSIID objects work for us.

        if(NS_FAILED(comp->GetInterfaces(getter_AddRefs(ifaces))) ||
           NS_FAILED(xpc->WrapNative(cx, obj, ifaces,
                                     NS_GET_IID(nsIScriptableInterfaces),
                                     getter_AddRefs(holder))) || !holder ||
           NS_FAILED(holder->GetJSObject(&ifacesObj)) || !ifacesObj)
        {
            return ThrowAndFail(NS_ERROR_XPC_UNEXPECTED, cx, _retval);
        }

        JSString* str = JS_ValueToString(cx, argv[1]);
        if(!str)
            return ThrowAndFail(NS_ERROR_XPC_BAD_CONVERT_JS, cx, _retval);

        jsval val;
        if(!JS_GetProperty(cx, ifacesObj, JS_GetStringBytes(str), &val) ||
           JSVAL_IS_PRIMITIVE(val))
        {
            return ThrowAndFail(NS_ERROR_XPC_BAD_IID, cx, _retval);
        }

        nsCOMPtr<nsIXPConnectWrappedNative> wn;
        if(NS_FAILED(xpc->GetWrappedNativeOfJSObject(cx, JSVAL_TO_OBJECT(val),
                                getter_AddRefs(wn))) || !wn ||
           !(cInterfaceID = do_QueryWrappedNative(wn)))
        {
            return ThrowAndFail(NS_ERROR_XPC_UNEXPECTED, cx, _retval);
        }
    }
    else
    {
        nsCOMPtr<nsIInterfaceInfo> info;
        xpc->GetInfoForIID(&NS_GET_IID(nsISupports), getter_AddRefs(info));

        if(info)
        {
            cInterfaceID =
                dont_AddRef(
                    static_cast<nsIJSIID*>(nsJSIID::NewID(info)));
        }
        if(!cInterfaceID)
            return ThrowAndFail(NS_ERROR_XPC_UNEXPECTED, cx, _retval);
    }

    // a new scope to avoid warnings about shadowed names
    {
        // argv[0] is a contractid name string
        // XXXjband support passing "Components.classes.foo"?

        // we do the lookup by asking the Components.classes object
        // for the property with this name - i.e. we let its caching of these
        // nsIJSCID objects work for us.

        nsCOMPtr<nsIXPCComponents_Classes> classes;
        nsCOMPtr<nsIXPConnectJSObjectHolder> holder;
        JSObject* classesObj = nsnull;

        if(NS_FAILED(comp->GetClasses(getter_AddRefs(classes))) ||
           NS_FAILED(xpc->WrapNative(cx, obj, classes,
                                     NS_GET_IID(nsIXPCComponents_Classes),
                                     getter_AddRefs(holder))) || !holder ||
           NS_FAILED(holder->GetJSObject(&classesObj)) || !classesObj)
        {
            return ThrowAndFail(NS_ERROR_XPC_UNEXPECTED, cx, _retval);
        }

        JSString* str = JS_ValueToString(cx, argv[0]);
        if(!str)
            return ThrowAndFail(NS_ERROR_XPC_BAD_CONVERT_JS, cx, _retval);

        jsval val;
        if(!JS_GetProperty(cx, classesObj, JS_GetStringBytes(str), &val) ||
           JSVAL_IS_PRIMITIVE(val))
        {
            return ThrowAndFail(NS_ERROR_XPC_BAD_CID, cx, _retval);
        }

        nsCOMPtr<nsIXPConnectWrappedNative> wn;
        if(NS_FAILED(xpc->GetWrappedNativeOfJSObject(cx, JSVAL_TO_OBJECT(val),
                                getter_AddRefs(wn))) || !wn ||
           !(cClassID = do_QueryWrappedNative(wn)))
        {
            return ThrowAndFail(NS_ERROR_XPC_UNEXPECTED, cx, _retval);
        }
    }

    nsCOMPtr<nsIXPCConstructor> ctor =
        static_cast<nsIXPCConstructor*>
                   (new nsXPCConstructor(cClassID, cInterfaceID, cInitializer));
    if(!ctor)
        return ThrowAndFail(NS_ERROR_XPC_UNEXPECTED, cx, _retval);

    nsCOMPtr<nsIXPConnectJSObjectHolder> holder2;
    JSObject* newObj = nsnull;

    if(NS_FAILED(xpc->WrapNative(cx, obj, ctor, NS_GET_IID(nsIXPCConstructor),
                 getter_AddRefs(holder2))) || !holder2 ||
       NS_FAILED(holder2->GetJSObject(&newObj)) || !newObj)
    {
        return ThrowAndFail(NS_ERROR_XPC_CANT_CREATE_WN, cx, _retval);
    }

    if(vp)
        *vp = OBJECT_TO_JSVAL(newObj);

    return NS_OK;
}

/* PRBool hasInstance (in nsIXPConnectWrappedNative wrapper, in JSContextPtr cx, in JSObjectPtr obj, in jsval val, out PRBool bp); */
NS_IMETHODIMP
nsXPCComponents_Constructor::HasInstance(nsIXPConnectWrappedNative *wrapper,
                                         JSContext * cx, JSObject * obj,
                                         const jsval &val, PRBool *bp,
                                         PRBool *_retval)
{
    if(bp)
        *bp = JSValIsInterfaceOfType(cx, val, NS_GET_IID(nsIXPCConstructor));
    return NS_OK;
}

/***************************************************************************/
// Javascript constructor for the sandbox object
class nsXPCComponents_utils_Sandbox : public nsIXPCComponents_utils_Sandbox,
                                      public nsIXPCScriptable
{
public:
    // Aren't macros nice?
    NS_DECL_ISUPPORTS
    NS_DECL_NSIXPCCOMPONENTS_UTILS_SANDBOX
    NS_DECL_NSIXPCSCRIPTABLE

public:
    nsXPCComponents_utils_Sandbox();
    virtual ~nsXPCComponents_utils_Sandbox();

private:
    static nsresult CallOrConstruct(nsIXPConnectWrappedNative *wrapper,
                                    JSContext * cx, JSObject * obj,
                                    PRUint32 argc, jsval * argv,
                                    jsval * vp, PRBool *_retval);
};

class nsXPCComponents_Utils :
            public nsIXPCComponents_Utils,
            public nsIXPCScriptable
#ifdef XPC_USE_SECURITY_CHECKED_COMPONENT
          , public nsISecurityCheckedComponent
#endif
{
public:
    // all the interface method declarations...
    NS_DECL_ISUPPORTS
    NS_DECL_NSIXPCSCRIPTABLE
#ifdef XPC_USE_SECURITY_CHECKED_COMPONENT
    NS_DECL_NSISECURITYCHECKEDCOMPONENT
#endif
    NS_DECL_NSIXPCCOMPONENTS_UTILS

public:
    nsXPCComponents_Utils() { }
    virtual ~nsXPCComponents_Utils() { }

private:
    nsCOMPtr<nsIXPCComponents_utils_Sandbox> mSandbox;
};

NS_INTERFACE_MAP_BEGIN(nsXPCComponents_Utils)
  NS_INTERFACE_MAP_ENTRY(nsIXPCComponents_Utils)
  NS_INTERFACE_MAP_ENTRY(nsIXPCScriptable)
#ifdef XPC_USE_SECURITY_CHECKED_COMPONENT
  NS_INTERFACE_MAP_ENTRY(nsISecurityCheckedComponent)
#endif
  NS_INTERFACE_MAP_ENTRY_AMBIGUOUS(nsISupports, nsIXPCComponents_Utils)
NS_INTERFACE_MAP_END_THREADSAFE

NS_IMPL_THREADSAFE_ADDREF(nsXPCComponents_Utils)
NS_IMPL_THREADSAFE_RELEASE(nsXPCComponents_Utils)

// The nsIXPCScriptable map declaration that will generate stubs for us...
#define XPC_MAP_CLASSNAME           nsXPCComponents_Utils
#define XPC_MAP_QUOTED_CLASSNAME   "nsXPCComponents_Utils"
#define XPC_MAP_FLAGS               nsIXPCScriptable::ALLOW_PROP_MODS_DURING_RESOLVE
#include "xpc_map_end.h" /* This will #undef the above */

NS_IMETHODIMP
nsXPCComponents_Utils::GetSandbox(nsIXPCComponents_utils_Sandbox **aSandbox)
{
    NS_ENSURE_ARG_POINTER(aSandbox);
    if (!mSandbox && !(mSandbox = new nsXPCComponents_utils_Sandbox())) {
        *aSandbox = nsnull;
        return NS_ERROR_OUT_OF_MEMORY;
    }
    NS_ADDREF(*aSandbox = mSandbox);
    return NS_OK;
}

/* void lookupMethod (); */
NS_IMETHODIMP
nsXPCComponents_Utils::LookupMethod()
{
    nsresult rv;

    nsCOMPtr<nsIXPConnect> xpc(do_GetService(nsIXPConnect::GetCID(), &rv));
    if(NS_FAILED(rv))
        return NS_ERROR_FAILURE;

    // get the xpconnect native call context
    nsAXPCNativeCallContext *cc = nsnull;
    xpc->GetCurrentNativeCallContext(&cc);
    if(!cc)
        return NS_ERROR_FAILURE;

// Check disabled until deprecated Components.lookupMethod removed.
#undef CHECK_FOR_INDIRECT_CALL
#ifdef CHECK_FOR_INDIRECT_CALL
    // verify that we are being called from JS (i.e. the current call is
    // to this object - though we don't verify that it is to this exact method)
    nsCOMPtr<nsISupports> callee;
    cc->GetCallee(getter_AddRefs(callee));
    if(!callee || callee.get() !=
                  static_cast<const nsISupports*>
                             (static_cast<const nsIXPCComponents_Utils*>(this)))
        return NS_ERROR_FAILURE;
#endif

    // Get JSContext of current call
    JSContext* cx;
    rv = cc->GetJSContext(&cx);
    if(NS_FAILED(rv) || !cx)
        return NS_ERROR_FAILURE;

    JSAutoRequest ar(cx);

    // get place for return value
    jsval *retval = nsnull;
    rv = cc->GetRetValPtr(&retval);
    if(NS_FAILED(rv) || !retval)
        return NS_ERROR_FAILURE;

    // get argc and argv and verify arg count
    PRUint32 argc;
    rv = cc->GetArgc(&argc);
    if(NS_FAILED(rv))
        return NS_ERROR_FAILURE;

    if(argc < 2)
        return NS_ERROR_XPC_NOT_ENOUGH_ARGS;

    jsval* argv;
    rv = cc->GetArgvPtr(&argv);
    if(NS_FAILED(rv) || !argv)
        return NS_ERROR_FAILURE;

    // first param must be a JSObject
    if(JSVAL_IS_PRIMITIVE(argv[0]))
        return NS_ERROR_XPC_BAD_CONVERT_JS;

    JSObject* obj = JSVAL_TO_OBJECT(argv[0]);
    rv = nsXPConnect::GetXPConnect()->GetJSObjectOfWrapper(cx, obj, &obj);
    if(NS_FAILED(rv))
        return rv;

    OBJ_TO_INNER_OBJECT(cx, obj);
    if(!obj)
        return NS_ERROR_XPC_BAD_CONVERT_JS;

    // second param must be a string
    if(!JSVAL_IS_STRING(argv[1]))
        return NS_ERROR_XPC_BAD_CONVERT_JS;

    // Make sure the name (argv[1]) that we use for looking up the
    // method/property is atomized.

    jsid name_id;
    if(!JS_ValueToId(cx, argv[1], &name_id) ||
       !JS_IdToValue(cx, name_id, &argv[1]))
        return NS_ERROR_XPC_BAD_CONVERT_JS;

    // this will do verification and the method lookup for us
    // Note that if |obj| is an XPCNativeWrapper this will all still work.
    // We'll hand back the same method that we'd hand back for the underlying
    // XPCWrappedNative.  This means no deep wrapping, unfortunately, but we
    // can't keep track of both the underlying function and the
    // XPCNativeWrapper at once in a single parent slot...
    XPCCallContext inner_cc(JS_CALLER, cx, obj, nsnull, name_id);

    // was our jsobject really a wrapped native at all?
    XPCWrappedNative* wrapper = inner_cc.GetWrapper();
    if(!wrapper || !wrapper->IsValid())
        return NS_ERROR_XPC_BAD_CONVERT_JS;

    // did we find a method/attribute by that name?
    XPCNativeMember* member = inner_cc.GetMember();
    if(!member || member->IsConstant())
        return NS_ERROR_XPC_BAD_CONVERT_JS;

    // it would a be a big surprise if there is a member without an interface :)
    XPCNativeInterface* iface = inner_cc.GetInterface();
    if(!iface)
        return NS_ERROR_XPC_BAD_CONVERT_JS;

    jsval funval;
    JSFunction *oldfunction;

    {
        JSAutoEnterCompartment ac;

        if (!ac.enter(inner_cc, wrapper->GetFlatJSObject())) {
            return NS_ERROR_UNEXPECTED;
        }

        // get (and perhaps lazily create) the member's cloned function
        if(!member->NewFunctionObject(inner_cc, iface, wrapper->GetFlatJSObject(),
                                      &funval))
            return NS_ERROR_XPC_BAD_CONVERT_JS;

        oldfunction = JS_ValueToFunction(inner_cc, funval);
        NS_ASSERTION(oldfunction, "Function is not a function");
    }

    // Stick the function in the return value. This roots it.
    *retval = funval;

    // Callers of this method are implicitly buying into
    // XPCNativeWrapper-like protection. The easiest way to enforce
    // this is to let the JS engine wrap the function.
    if (!JS_WrapValue(inner_cc, retval)) {
        return NS_ERROR_UNEXPECTED;
    }

    // Tell XPConnect that we returned the function through the call context.
    cc->SetReturnValueWasSet(PR_TRUE);
    return NS_OK;
}

/* void reportError (); */
NS_IMETHODIMP
nsXPCComponents_Utils::ReportError()
{
    // This function shall never fail! Silently eat any failure conditions.
    nsresult rv;

    nsCOMPtr<nsIConsoleService> console(
      do_GetService(NS_CONSOLESERVICE_CONTRACTID));

    nsCOMPtr<nsIScriptError> scripterr(new nsScriptError());

    nsCOMPtr<nsIXPConnect> xpc(do_GetService(nsIXPConnect::GetCID()));
    if(!scripterr || !console || !xpc)
        return NS_OK;

    // get the xpconnect native call context
    nsAXPCNativeCallContext *cc = nsnull;
    xpc->GetCurrentNativeCallContext(&cc);
    if(!cc)
        return NS_OK;

// Check disabled until deprecated Components.reportError removed.
#undef CHECK_FOR_INDIRECT_CALL
#ifdef CHECK_FOR_INDIRECT_CALL
    // verify that we are being called from JS (i.e. the current call is
    // to this object - though we don't verify that it is to this exact method)
    nsCOMPtr<nsISupports> callee;
    cc->GetCallee(getter_AddRefs(callee));
    if(!callee || callee.get() !=
                  static_cast<const nsISupports*>
                             (static_cast<const nsIXPCComponents_Utils*>(this))) {
        NS_ERROR("reportError() must only be called from JS!");
        return NS_ERROR_FAILURE;
    }
#endif

    // Get JSContext of current call
    JSContext* cx;
    rv = cc->GetJSContext(&cx);
    if(NS_FAILED(rv) || !cx)
        return NS_OK;

    JSAutoRequest ar(cx);

    // get argc and argv and verify arg count
    PRUint32 argc;
    rv = cc->GetArgc(&argc);
    if(NS_FAILED(rv))
        return NS_OK;

    if(argc < 1)
        return NS_ERROR_XPC_NOT_ENOUGH_ARGS;

    jsval* argv;
    rv = cc->GetArgvPtr(&argv);
    if(NS_FAILED(rv) || !argv)
        return NS_OK;

    JSErrorReport* err = JS_ErrorFromException(cx, argv[0]);
    if(err)
    {
        // It's a proper JS Error
        nsAutoString fileUni;
        CopyUTF8toUTF16(err->filename, fileUni);

        PRUint32 column = err->uctokenptr - err->uclinebuf;

        rv = scripterr->Init(reinterpret_cast<const PRUnichar*>
                                             (err->ucmessage),
                             fileUni.get(),
                             reinterpret_cast<const PRUnichar*>
                                             (err->uclinebuf),
                             err->lineno,
                             column,
                             err->flags,
                             "XPConnect JavaScript");
        if(NS_FAILED(rv))
            return NS_OK;

        console->LogMessage(scripterr);
        return NS_OK;
    }

    // It's not a JS Error object, so we synthesize as best we're able
    JSString* msgstr = JS_ValueToString(cx, argv[0]);
    if(msgstr)
    {
        // Root the string during scripterr->Init
        argv[0] = STRING_TO_JSVAL(msgstr);

        nsCOMPtr<nsIStackFrame> frame;
        nsXPConnect* xpc = nsXPConnect::GetXPConnect();
        if(xpc)
            xpc->GetCurrentJSStack(getter_AddRefs(frame));

        nsXPIDLCString fileName;
        PRInt32 lineNo = 0;
        if(frame)
        {
            frame->GetFilename(getter_Copies(fileName));
            frame->GetLineNumber(&lineNo);
        }

        rv = scripterr->Init(reinterpret_cast<const PRUnichar*>
                                             (JS_GetStringChars(msgstr)),
                             NS_ConvertUTF8toUTF16(fileName).get(),
                             nsnull,
                             lineNo, 0,
                             0, "XPConnect JavaScript");
        if(NS_SUCCEEDED(rv))
            console->LogMessage(scripterr);
    }

    return NS_OK;
}

#ifndef XPCONNECT_STANDALONE
#include "nsIScriptSecurityManager.h"
#include "nsIURI.h"
#include "nsNetUtil.h"
const char kScriptSecurityManagerContractID[] = NS_SCRIPTSECURITYMANAGER_CONTRACTID;

NS_IMPL_THREADSAFE_ISUPPORTS1(PrincipalHolder, nsIScriptObjectPrincipal)

nsIPrincipal *
PrincipalHolder::GetPrincipal()
{
    return mHoldee;
}

static JSBool
SandboxDump(JSContext *cx, uintN argc, jsval *vp)
{
    JSString *str;
    if (!argc)
        return JS_TRUE;

    str = JS_ValueToString(cx, JS_ARGV(cx, vp)[0]);
    if (!str)
        return JS_FALSE;

    jschar *chars = JS_GetStringChars(str);
    if (!chars)
        return JS_FALSE;

    nsDependentString wstr(reinterpret_cast<PRUnichar *>(chars),
                           JS_GetStringLength(str));
    char *cstr = ToNewUTF8String(wstr);
    if (!cstr)
        return JS_FALSE;

#if defined(XP_MAC) || defined(XP_MACOSX)
    // Be nice and convert all \r to \n.
    char *c = cstr, *cEnd = cstr + strlen(cstr);
    while (c < cEnd) {
        if (*c == '\r')
            *c = '\n';
        c++;
    }
#endif

    fputs(cstr, stderr);
    NS_Free(cstr);
    JS_SET_RVAL(cx, vp, JSVAL_TRUE);
    return JS_TRUE;
}

static JSBool
SandboxDebug(JSContext *cx, uintN argc, jsval *vp)
{
#ifdef DEBUG
    return SandboxDump(cx, argc, vp);
#else
    return JS_TRUE;
#endif
}

static JSBool
SandboxImport(JSContext *cx, uintN argc, jsval *vp)
{
    JSObject *thisobj = JS_THIS_OBJECT(cx, vp);
    if (!thisobj)
        return JS_FALSE;

    jsval *argv = JS_ARGV(cx, vp);
    if (argc < 1 || JSVAL_IS_PRIMITIVE(argv[0])) {
        XPCThrower::Throw(NS_ERROR_INVALID_ARG, cx);
        return JS_FALSE;
    }

    JSString *funname;
    if (argc > 1) {
        // Use the second parameter as the function name.
        funname = JS_ValueToString(cx, argv[1]);
        if (!funname)
            return JS_FALSE;
        argv[1] = STRING_TO_JSVAL(funname);
    } else {
        // NB: funobj must only be used to get the JSFunction out.
        JSObject *funobj = JSVAL_TO_OBJECT(argv[0]);
        if (funobj->isProxy()) {
            funobj = XPCWrapper::UnsafeUnwrapSecurityWrapper(cx, funobj);
        }

        JSAutoEnterCompartment ac;
        if (!ac.enter(cx, funobj)) {
            return JS_FALSE;
        }

        JSFunction *fun = JS_ValueToFunction(cx, OBJECT_TO_JSVAL(funobj));
        if (!fun) {
            XPCThrower::Throw(NS_ERROR_INVALID_ARG, cx);
            return JS_FALSE;
        }

        // Use the actual function name as the name.
        funname = JS_GetFunctionId(fun);
        if (!funname) {
            XPCThrower::Throw(NS_ERROR_INVALID_ARG, cx);
            return JS_FALSE;
        }
    }

    jsid id;
    if (!JS_ValueToId(cx, STRING_TO_JSVAL(funname), &id))
        return JS_FALSE;

    JS_SET_RVAL(cx, vp, JSVAL_VOID);
    return JS_SetPropertyById(cx, thisobj, id, &argv[0]);
}

static JSBool
sandbox_enumerate(JSContext *cx, JSObject *obj)
{
    return JS_EnumerateStandardClasses(cx, obj);
}

static JSBool
sandbox_resolve(JSContext *cx, JSObject *obj, jsid id)
{
    JSBool resolved;
    return JS_ResolveStandardClass(cx, obj, id, &resolved);
}

static void
sandbox_finalize(JSContext *cx, JSObject *obj)
{
    nsIScriptObjectPrincipal *sop =
        (nsIScriptObjectPrincipal *)xpc_GetJSPrivate(obj);
    NS_IF_RELEASE(sop);
}

static JSBool
sandbox_convert(JSContext *cx, JSObject *obj, JSType type, jsval *vp)
{
    if (type == JSTYPE_OBJECT) {
        *vp = OBJECT_TO_JSVAL(obj);
        return JS_TRUE;
    }

    return JS_ConvertStub(cx, obj, type, vp);
}

static JSClass SandboxClass = {
    "Sandbox",
    JSCLASS_HAS_PRIVATE | JSCLASS_PRIVATE_IS_NSISUPPORTS | JSCLASS_GLOBAL_FLAGS,
    JS_PropertyStub,   JS_PropertyStub, JS_PropertyStub, JS_PropertyStub,
    sandbox_enumerate, sandbox_resolve, sandbox_convert,  sandbox_finalize,
    JSCLASS_NO_OPTIONAL_MEMBERS
};

static JSFunctionSpec SandboxFunctions[] = {
    {"dump",    SandboxDump,    1,0},
    {"debug",   SandboxDebug,   1,0},
    {"importFunction", SandboxImport, 1,0},
    {nsnull,nsnull,0,0}
};

#endif /* !XPCONNECT_STANDALONE */

/***************************************************************************/
nsXPCComponents_utils_Sandbox::nsXPCComponents_utils_Sandbox()
{
}

nsXPCComponents_utils_Sandbox::~nsXPCComponents_utils_Sandbox()
{
}

NS_INTERFACE_MAP_BEGIN(nsXPCComponents_utils_Sandbox)
  NS_INTERFACE_MAP_ENTRY(nsIXPCComponents_utils_Sandbox)
  NS_INTERFACE_MAP_ENTRY(nsIXPCScriptable)
  NS_INTERFACE_MAP_ENTRY_AMBIGUOUS(nsISupports, nsIXPCComponents_utils_Sandbox)
NS_INTERFACE_MAP_END_THREADSAFE

NS_IMPL_THREADSAFE_ADDREF(nsXPCComponents_utils_Sandbox)
NS_IMPL_THREADSAFE_RELEASE(nsXPCComponents_utils_Sandbox)

// We use the nsIXPScriptable macros to generate lots of stuff for us.
#define XPC_MAP_CLASSNAME           nsXPCComponents_utils_Sandbox
#define XPC_MAP_QUOTED_CLASSNAME   "nsXPCComponents_utils_Sandbox"
#define                             XPC_MAP_WANT_CALL
#define                             XPC_MAP_WANT_CONSTRUCT
#define XPC_MAP_FLAGS               0
#include "xpc_map_end.h" /* This #undef's the above. */

#ifndef XPCONNECT_STANDALONE

static bool
WrapForSandbox(JSContext *cx, bool wantXrays, jsval *vp)
{
    return wantXrays
           ? JS_WrapValue(cx, vp)
           : xpc::WrapperFactory::WaiveXrayAndWrap(cx, vp);
}

// Needed to distinguish multiple compartments with the same origin from each
// other. The only thing we need out of identity objects are unique addresses.
class Identity : public nsISupports
{
    NS_DECL_ISUPPORTS
};

NS_IMPL_ISUPPORTS0(Identity)

nsresult
xpc_CreateSandboxObject(JSContext * cx, jsval * vp, nsISupports *prinOrSop, JSObject *proto,
                        bool wantXrays)
{
    // Create the sandbox global object
    nsresult rv;
    nsCOMPtr<nsIXPConnect> xpc(do_GetService(nsIXPConnect::GetCID(), &rv));
    if(NS_FAILED(rv))
        return NS_ERROR_XPC_UNEXPECTED;

    nsCOMPtr<nsIScriptObjectPrincipal> sop(do_QueryInterface(prinOrSop));

    if (!sop) {
        nsCOMPtr<nsIPrincipal> principal(do_QueryInterface(prinOrSop));

        if (!principal) {
            principal = do_CreateInstance("@mozilla.org/nullprincipal;1", &rv);
            NS_ASSERTION(NS_FAILED(rv) || principal,
                         "Bad return from do_CreateInstance");

            if (!principal || NS_FAILED(rv)) {
                if (NS_SUCCEEDED(rv)) {
                    rv = NS_ERROR_FAILURE;
                }

                return rv;
            }
        }

        sop = new PrincipalHolder(principal);
        if (!sop)
            return NS_ERROR_OUT_OF_MEMORY;
    }

    nsIPrincipal *principal = sop->GetPrincipal();

    JSCompartment *compartment;
    JSObject *sandbox;

    nsRefPtr<Identity> identity = new Identity();
    rv = xpc_CreateGlobalObject(cx, &SandboxClass, principal, identity,
                                wantXrays, &sandbox, &compartment);
    NS_ENSURE_SUCCESS(rv, rv);

    js::AutoObjectRooter tvr(cx, sandbox);

    {
        JSAutoEnterCompartment ac;
        if (!ac.enter(cx, sandbox))
            return NS_ERROR_XPC_UNEXPECTED;

        if (proto) {
            bool ok = JS_WrapObject(cx, &proto);
            if (!ok)
                return NS_ERROR_XPC_UNEXPECTED;

            if (xpc::WrapperFactory::IsXrayWrapper(proto) && !wantXrays) {
                jsval v;
                if (!JS_GetProperty(cx, proto, "wrappedJSObject", &v))
                    return NS_ERROR_XPC_UNEXPECTED;

                proto = JSVAL_TO_OBJECT(v);
            }

            ok = JS_SetPrototype(cx, sandbox, proto);
            if (!ok)
                return NS_ERROR_XPC_UNEXPECTED;
        }

        // Pass on ownership of sop to |sandbox|.
        if (!JS_SetPrivate(cx, sandbox, sop.forget().get())) {
            return NS_ERROR_XPC_UNEXPECTED;
        }

        rv = xpc->InitClasses(cx, sandbox);
        if (NS_SUCCEEDED(rv) &&
            !JS_DefineFunctions(cx, sandbox, SandboxFunctions)) {
            rv = NS_ERROR_FAILURE;
        }
        if (NS_FAILED(rv))
            return NS_ERROR_XPC_UNEXPECTED;
    }

    if (vp) {
        *vp = OBJECT_TO_JSVAL(sandbox);
        if (!WrapForSandbox(cx, wantXrays, vp)) {
            return NS_ERROR_UNEXPECTED;
        }
    }

    return NS_OK;
}
#endif /* !XPCONNECT_STANDALONE */

/* PRBool call(in nsIXPConnectWrappedNative wrapper,
               in JSContextPtr cx,
               in JSObjectPtr obj,
               in PRUint32 argc,
               in JSValPtr argv,
               in JSValPtr vp);
*/
NS_IMETHODIMP
nsXPCComponents_utils_Sandbox::Call(nsIXPConnectWrappedNative *wrapper,
                                    JSContext * cx,
                                    JSObject * obj,
                                    PRUint32 argc,
                                    jsval * argv,
                                    jsval * vp,
                                    PRBool *_retval)
{
    return CallOrConstruct(wrapper, cx, obj, argc, argv, vp, _retval);
}

/* PRBool construct(in nsIXPConnectWrappedNative wrapper,
                    in JSContextPtr cx,
                    in JSObjectPtr obj,
                    in PRUint32 argc,
                    in JSValPtr argv,
                    in JSValPtr vp);
*/
NS_IMETHODIMP
nsXPCComponents_utils_Sandbox::Construct(nsIXPConnectWrappedNative *wrapper,
                                         JSContext * cx,
                                         JSObject * obj,
                                         PRUint32 argc,
                                         jsval * argv,
                                         jsval * vp,
                                         PRBool *_retval)
{
    return CallOrConstruct(wrapper, cx, obj, argc, argv, vp, _retval);
}

// static
nsresult
nsXPCComponents_utils_Sandbox::CallOrConstruct(nsIXPConnectWrappedNative *wrapper,
                                               JSContext * cx, JSObject * obj,
                                               PRUint32 argc, jsval * argv,
                                               jsval * vp, PRBool *_retval)
{
#ifdef XPCONNECT_STANDALONE
    return NS_ERROR_NOT_AVAILABLE;
#else /* XPCONNECT_STANDALONE */
    if (argc < 1)
        return ThrowAndFail(NS_ERROR_XPC_NOT_ENOUGH_ARGS, cx, _retval);

    nsresult rv;

    // Make sure to set up principals on the sandbox before initing classes
    nsCOMPtr<nsIScriptObjectPrincipal> sop;
    nsCOMPtr<nsIPrincipal> principal;
    nsISupports *prinOrSop = nsnull;
    if (JSVAL_IS_STRING(argv[0])) {
        JSString *codebasestr = JSVAL_TO_STRING(argv[0]);
        nsAutoString codebase(reinterpret_cast<PRUnichar*>
                                              (JS_GetStringChars(codebasestr)),
                              JS_GetStringLength(codebasestr));
        nsCOMPtr<nsIURI> uri;
        rv = NS_NewURI(getter_AddRefs(uri), codebase);
        if (NS_FAILED(rv)) {
            return ThrowAndFail(rv, cx, _retval);
        }

        nsCOMPtr<nsIScriptSecurityManager> secman =
            do_GetService(kScriptSecurityManagerContractID);
        if (!secman ||
            NS_FAILED(rv = secman->GetCodebasePrincipal(uri,
                                                 getter_AddRefs(principal))) ||
            !principal) {
            if (NS_SUCCEEDED(rv))
                rv = NS_ERROR_FAILURE;
            return ThrowAndFail(rv, cx, _retval);
        }

        prinOrSop = principal;
    } else {
        if (!JSVAL_IS_PRIMITIVE(argv[0])) {
            nsCOMPtr<nsIXPConnect> xpc(do_GetService(nsIXPConnect::GetCID()));
            if(!xpc)
                return NS_ERROR_XPC_UNEXPECTED;
            nsCOMPtr<nsIXPConnectWrappedNative> wrapper;
            xpc->GetWrappedNativeOfJSObject(cx, JSVAL_TO_OBJECT(argv[0]),
                                            getter_AddRefs(wrapper));

            if (wrapper) {
                sop = do_QueryWrappedNative(wrapper);
                if (sop) {
                    prinOrSop = sop;
                } else {
                    principal = do_QueryWrappedNative(wrapper);
                    prinOrSop = principal;
                }
            }
        }

        if (!prinOrSop)
            return ThrowAndFail(NS_ERROR_INVALID_ARG, cx, _retval);
    }

    JSObject *proto = nsnull;
    bool wantXrays = true;
    if (argc > 1) {
        if (!JSVAL_IS_OBJECT(argv[1]))
            return ThrowAndFail(NS_ERROR_INVALID_ARG, cx, _retval);

        JSObject *optionsObject = JSVAL_TO_OBJECT(argv[1]);
        jsval option;

        JSBool found;
        if (!JS_HasProperty(cx, optionsObject, "sandboxPrototype", &found))
            return NS_ERROR_INVALID_ARG;

        if (found) {
            if (!JS_GetProperty(cx, optionsObject, "sandboxPrototype", &option) ||
                !JSVAL_IS_OBJECT(option)) {
                return ThrowAndFail(NS_ERROR_INVALID_ARG, cx, _retval);
            }

            proto = JSVAL_TO_OBJECT(option);
        }

        if (!JS_HasProperty(cx, optionsObject, "wantXrays", &found))
            return NS_ERROR_INVALID_ARG;

        if (found) {
            if (!JS_GetProperty(cx, optionsObject, "wantXrays", &option) ||
                !JSVAL_IS_BOOLEAN(option)) {
                return ThrowAndFail(NS_ERROR_INVALID_ARG, cx, _retval);
            }

            wantXrays = JSVAL_TO_BOOLEAN(option);
        }
    }

    rv = xpc_CreateSandboxObject(cx, vp, prinOrSop, proto, wantXrays);

    if (NS_FAILED(rv)) {
        return ThrowAndFail(rv, cx, _retval);
    }

    *_retval = PR_TRUE;

    return rv;
#endif /* XPCONNECT_STANDALONE */
}

class ContextHolder : public nsISupports
{
public:
    ContextHolder(JSContext *aOuterCx, JSObject *aSandbox);
    virtual ~ContextHolder();

    JSContext * GetJSContext()
    {
        return mJSContext;
    }

    NS_DECL_ISUPPORTS

private:
    static JSBool ContextHolderOperationCallback(JSContext *cx);

    JSContext* mJSContext;
    JSContext* mOrigCx;
};

NS_IMPL_ISUPPORTS0(ContextHolder)

ContextHolder::ContextHolder(JSContext *aOuterCx, JSObject *aSandbox)
    : mJSContext(JS_NewContext(JS_GetRuntime(aOuterCx), 1024)),
      mOrigCx(aOuterCx)
{
    if(mJSContext)
    {
        JSAutoRequest ar(mJSContext);
        JS_SetOptions(mJSContext,
                      JSOPTION_DONT_REPORT_UNCAUGHT |
                      JSOPTION_PRIVATE_IS_NSISUPPORTS);
        JS_SetGlobalObject(mJSContext, aSandbox);
        JS_SetContextPrivate(mJSContext, this);
        JS_SetOperationCallback(mJSContext, ContextHolderOperationCallback);
    }
}

ContextHolder::~ContextHolder()
{
    if(mJSContext)
        JS_DestroyContextNoGC(mJSContext);
}

JSBool
ContextHolder::ContextHolderOperationCallback(JSContext *cx)
{
    ContextHolder* thisObject =
        static_cast<ContextHolder*>(JS_GetContextPrivate(cx));
    NS_ASSERTION(thisObject, "How did that happen?");

    JSContext *origCx = thisObject->mOrigCx;
    JSOperationCallback callback = JS_GetOperationCallback(origCx);
    JSBool ok = JS_TRUE;
    if(callback)
        ok = callback(origCx);
    return ok;
}

/***************************************************************************/

/* void evalInSandbox(in AString source, in nativeobj sandbox); */
NS_IMETHODIMP
nsXPCComponents_Utils::EvalInSandbox(const nsAString &source)
{
#ifdef XPCONNECT_STANDALONE
    return NS_ERROR_NOT_AVAILABLE;
#else /* XPCONNECT_STANDALONE */
    nsresult rv;

    nsCOMPtr<nsIXPConnect> xpc(do_GetService(nsIXPConnect::GetCID(), &rv));
    if(NS_FAILED(rv))
        return rv;

    // get the xpconnect native call context
    nsAXPCNativeCallContext *cc = nsnull;
    xpc->GetCurrentNativeCallContext(&cc);
    if(!cc)
        return NS_ERROR_FAILURE;

    // Get JSContext of current call
    JSContext* cx;
    rv = cc->GetJSContext(&cx);
    if(NS_FAILED(rv) || !cx)
        return NS_ERROR_FAILURE;

    // get place for return value
    jsval *rval = nsnull;
    rv = cc->GetRetValPtr(&rval);
    if(NS_FAILED(rv) || !rval)
        return NS_ERROR_FAILURE;

    // get argc and argv and verify arg count
    PRUint32 argc;
    rv = cc->GetArgc(&argc);
    if(NS_FAILED(rv))
        return rv;

    // The second argument is the sandbox object. It is required.
    if (argc < 2)
        return NS_ERROR_XPC_NOT_ENOUGH_ARGS;

    jsval *argv;
    rv = cc->GetArgvPtr(&argv);
    if (NS_FAILED(rv))
        return rv;

    JSObject *sandbox;
    char *jsVersionStr = NULL;
    char *filenameStr = NULL;
    PRInt32 lineNo = 0;

    JSBool ok = JS_ConvertArguments(cx, argc, argv, "*o/ssi",
                                    &sandbox, &jsVersionStr,
                                    &filenameStr, &lineNo);

    if (!ok || !sandbox)
        return NS_ERROR_INVALID_ARG;

    JSVersion jsVersion = JSVERSION_DEFAULT;

    // Optional third argument: JS version, as a string.
    if (jsVersionStr) {
        jsVersion = JS_StringToVersion(jsVersionStr);
        if (jsVersion == JSVERSION_UNKNOWN)
            return NS_ERROR_INVALID_ARG;
    }

    nsXPIDLCString filename;

    // Optional fourth and fifth arguments: filename and line number.
    if (filenameStr) {
        filename = filenameStr;
    } else {
        // Get the current source info from xpc.
        nsCOMPtr<nsIStackFrame> frame;
        xpc->GetCurrentJSStack(getter_AddRefs(frame));
        if (frame) {
            frame->GetFilename(getter_Copies(filename));
            frame->GetLineNumber(&lineNo);
        }
    }

    rv = xpc_EvalInSandbox(cx, sandbox, source, filename.get(), lineNo,
                           jsVersion, PR_FALSE, rval);

    if (NS_SUCCEEDED(rv) && !JS_IsExceptionPending(cx))
        cc->SetReturnValueWasSet(PR_TRUE);

    return rv;
#endif /* XPCONNECT_STANDALONE */
}

#ifndef XPCONNECT_STANDALONE
nsresult
xpc_EvalInSandbox(JSContext *cx, JSObject *sandbox, const nsAString& source,
                  const char *filename, PRInt32 lineNo,
                  JSVersion jsVersion, PRBool returnStringOnly, jsval *rval)
{
#ifdef DEBUG
    // NB: The "unsafe" unwrap here is OK because we must be called from chrome.
    {
        nsIScriptSecurityManager *ssm = XPCWrapper::GetSecurityManager();
        if (ssm) {
            JSStackFrame *fp;
            nsIPrincipal *subjectPrincipal =
                ssm->GetCxSubjectPrincipalAndFrame(cx, &fp);
            PRBool system;
            ssm->IsSystemPrincipal(subjectPrincipal, &system);
            if (fp && !system) {
                ssm->IsCapabilityEnabled("UniversalXPConnect", &system);
                NS_ASSERTION(system, "Bad caller!");
            }
        }
    }
#endif

    sandbox = XPCWrapper::UnsafeUnwrapSecurityWrapper(cx, sandbox);
    if (!sandbox || sandbox->getJSClass() != &SandboxClass) {
        return NS_ERROR_INVALID_ARG;
    }

    nsIScriptObjectPrincipal *sop =
        (nsIScriptObjectPrincipal*)xpc_GetJSPrivate(sandbox);
    NS_ASSERTION(sop, "Invalid sandbox passed");
    nsCOMPtr<nsIPrincipal> prin = sop->GetPrincipal();

    JSPrincipals *jsPrincipals;

    if (!prin ||
        NS_FAILED(prin->GetJSPrincipals(cx, &jsPrincipals)) ||
        !jsPrincipals) {
        return NS_ERROR_FAILURE;
    }

    JSObject *callingScope;
    {
        JSAutoRequest req(cx);

        callingScope = JS_GetScopeChain(cx);
        if (!callingScope) {
            return NS_ERROR_FAILURE;
        }
        callingScope = JS_GetGlobalForObject(cx, callingScope);
    }

    nsRefPtr<ContextHolder> sandcx = new ContextHolder(cx, sandbox);
    if(!sandcx || !sandcx->GetJSContext()) {
        JS_ReportError(cx, "Can't prepare context for evalInSandbox");
        JSPRINCIPALS_DROP(cx, jsPrincipals);
        return NS_ERROR_OUT_OF_MEMORY;
    }

    if (jsVersion != JSVERSION_DEFAULT)
        JS_SetVersion(sandcx->GetJSContext(), jsVersion);

    XPCPerThreadData *data = XPCPerThreadData::GetData(cx);
    XPCJSContextStack *stack = nsnull;
    if (data && (stack = data->GetJSContextStack())) {
        if (NS_FAILED(stack->Push(sandcx->GetJSContext()))) {
            JS_ReportError(cx,
                    "Unable to initialize XPConnect with the sandbox context");
            JSPRINCIPALS_DROP(cx, jsPrincipals);
            return NS_ERROR_FAILURE;
        }
    }

    if (!filename) {
        // Default the filename to the codebase.
        filename = jsPrincipals->codebase;
        lineNo = 1;
    }

    nsresult rv = NS_OK;

    {
        JSAutoRequest req(sandcx->GetJSContext());
        JSAutoEnterCompartment ac;
        jsval v;
        JSString *str = nsnull;

        if (!ac.enter(sandcx->GetJSContext(), sandbox)) {
            if (stack) {
                stack->Pop(nsnull);
            }
            return NS_ERROR_FAILURE;
        }

        JSBool ok =
            JS_EvaluateUCScriptForPrincipals(sandcx->GetJSContext(), sandbox,
                                             jsPrincipals,
                                             reinterpret_cast<const jschar *>
                                                             (PromiseFlatString(source).get()),
                                             source.Length(), filename, lineNo,
                                             &v);
        if (ok && returnStringOnly && !(JSVAL_IS_VOID(v))) {
            ok = !!(str = JS_ValueToString(sandcx->GetJSContext(), v));
        }

        if (!ok) {
            // The sandbox threw an exception, convert it to a string (if
            // asked) or convert it to a SJOW.

            jsval exn;
            if (JS_GetPendingException(sandcx->GetJSContext(), &exn)) {
                JS_ClearPendingException(sandcx->GetJSContext());

                if (returnStringOnly) {
                    // The caller asked for strings only, convert the
                    // exception into a string.
                    str = JS_ValueToString(sandcx->GetJSContext(), exn);

                    JSAutoRequest req(cx);
                    if (str) {
                        // We converted the exception to a string. Use that
                        // as the value exception.
                        exn = STRING_TO_JSVAL(str);
                        if (JS_WrapValue(cx, &exn)) {
                            JS_SetPendingException(cx, STRING_TO_JSVAL(str));
                        } else {
                            JS_ClearPendingException(cx);
                            rv = NS_ERROR_FAILURE;
                        }
                    } else {
                        JS_ClearPendingException(cx);
                        rv = NS_ERROR_FAILURE;
                    }
                } else {
                    JSAutoRequest req(cx);

                    if (JS_WrapValue(cx, &exn)) {
                        JS_SetPendingException(cx, exn);
                    }
                }


                // Clear str so we don't confuse callers.
                str = nsnull;
            } else {
                rv = NS_ERROR_OUT_OF_MEMORY;
            }
        } else {
            // Convert the result into something safe for our caller.
            JSAutoRequest req(cx);
            JSAutoEnterCompartment ac;
            if (str) {
                v = STRING_TO_JSVAL(str);
            }

            xpc::CompartmentPrivate *sandboxdata =
                static_cast<xpc::CompartmentPrivate *>
                           (JS_GetCompartmentPrivate(cx, sandbox->getCompartment()));
            if (!ac.enter(cx, callingScope) ||
                !WrapForSandbox(cx, sandboxdata->wantXrays, &v)) {
                rv = NS_ERROR_FAILURE;
            }

            if (NS_SUCCEEDED(rv)) {
                *rval = v;
            }
        }
    }

    if (stack) {
        stack->Pop(nsnull);
    }

    JSPRINCIPALS_DROP(cx, jsPrincipals);

    return rv;
}
#endif /* !XPCONNECT_STANDALONE */

/* JSObject import (in AUTF8String registryLocation,
 *                  [optional] in JSObject targetObj);
 */
NS_IMETHODIMP
nsXPCComponents_Utils::Import(const nsACString & registryLocation)
{
#ifdef MOZ_JSLOADER
    nsCOMPtr<xpcIJSModuleLoader> moduleloader =
        do_GetService(MOZJSCOMPONENTLOADER_CONTRACTID);
    if (!moduleloader)
        return NS_ERROR_FAILURE;
    return moduleloader->Import(registryLocation);
#else
    return NS_ERROR_NOT_AVAILABLE;
#endif
}

/* xpcIJSWeakReference getWeakReference (); */
NS_IMETHODIMP
nsXPCComponents_Utils::GetWeakReference(xpcIJSWeakReference **_retval)
{
    nsRefPtr<xpcJSWeakReference> ref(new xpcJSWeakReference());
    if (!ref)
        return NS_ERROR_OUT_OF_MEMORY;
    ref->Init();
    *_retval = ref;
    NS_ADDREF(*_retval);
    return NS_OK;
}

/* void forceGC (); */
NS_IMETHODIMP
nsXPCComponents_Utils::ForceGC()
{
    nsXPConnect* xpc = nsXPConnect::GetXPConnect();
    if (!xpc)
        return NS_ERROR_FAILURE;

    // get the xpconnect native call context
    nsAXPCNativeCallContext *cc = nsnull;
    nsresult rv = xpc->GetCurrentNativeCallContext(&cc);
    if (!cc)
        return rv;

    // Get JSContext of current call
    JSContext* cx;
    cc->GetJSContext(&cx);
    if (!cx)
        return NS_ERROR_FAILURE;

    JS_GC(cx);

    return NS_OK;
}

/* void getGlobalForObject(); */
NS_IMETHODIMP
nsXPCComponents_Utils::GetGlobalForObject()
{
  nsresult rv;
  nsCOMPtr<nsIXPConnect> xpc(do_GetService(nsIXPConnect::GetCID(), &rv));
  if(NS_FAILED(rv))
    return NS_ERROR_FAILURE;

  // get the xpconnect native call context
  nsAXPCNativeCallContext *cc = nsnull;
  xpc->GetCurrentNativeCallContext(&cc);
  if(!cc)
    return NS_ERROR_FAILURE;

  // Get JSContext of current call
  JSContext* cx;
  rv = cc->GetJSContext(&cx);
  if(NS_FAILED(rv) || !cx)
    return NS_ERROR_FAILURE;

  // get place for return value
  jsval *rval = nsnull;
  rv = cc->GetRetValPtr(&rval);
  if(NS_FAILED(rv) || !rval)
    return NS_ERROR_FAILURE;

  // get argc and argv and verify arg count
  PRUint32 argc;
  rv = cc->GetArgc(&argc);
  if(NS_FAILED(rv))
    return NS_ERROR_FAILURE;

  if(argc != 1)
    return NS_ERROR_XPC_NOT_ENOUGH_ARGS;

  jsval* argv;
  rv = cc->GetArgvPtr(&argv);
  if(NS_FAILED(rv) || !argv)
    return NS_ERROR_FAILURE;

  // first argument must be an object
  if(JSVAL_IS_PRIMITIVE(argv[0]))
    return NS_ERROR_XPC_BAD_CONVERT_JS;

  JSObject *obj = JS_GetGlobalForObject(cx, JSVAL_TO_OBJECT(argv[0]));
  *rval = OBJECT_TO_JSVAL(obj);

  // Outerize if necessary.
  if (JSObjectOp outerize = obj->getClass()->ext.outerObject)
      *rval = OBJECT_TO_JSVAL(outerize(cx, obj));

  cc->SetReturnValueWasSet(PR_TRUE);
  return NS_OK;
}

#ifdef XPC_USE_SECURITY_CHECKED_COMPONENT
/* string canCreateWrapper (in nsIIDPtr iid); */
NS_IMETHODIMP
nsXPCComponents_Utils::CanCreateWrapper(const nsIID * iid, char **_retval)
{
    // We let anyone do this...
    *_retval = xpc_CloneAllAccess();
    return NS_OK;
}

/* string canCallMethod (in nsIIDPtr iid, in wstring methodName); */
NS_IMETHODIMP
nsXPCComponents_Utils::CanCallMethod(const nsIID * iid, const PRUnichar *methodName, char **_retval)
{
    static const char* allowed[] = { "lookupMethod", "evalInSandbox", nsnull };
    *_retval = xpc_CheckAccessList(methodName, allowed);
    return NS_OK;
}

/* string canGetProperty (in nsIIDPtr iid, in wstring propertyName); */
NS_IMETHODIMP
nsXPCComponents_Utils::CanGetProperty(const nsIID * iid, const PRUnichar *propertyName, char **_retval)
{
    *_retval = nsnull;
    return NS_OK;
}

/* string canSetProperty (in nsIIDPtr iid, in wstring propertyName); */
NS_IMETHODIMP
nsXPCComponents_Utils::CanSetProperty(const nsIID * iid, const PRUnichar *propertyName, char **_retval)
{
    // If you have to ask, then the answer is NO
    *_retval = nsnull;
    return NS_OK;
}
#endif

/***************************************************************************/
/***************************************************************************/
/***************************************************************************/

// XXXjband We ought to cache the wrapper in the object's slots rather than
// re-wrapping on demand

NS_INTERFACE_MAP_BEGIN(nsXPCComponents)
  NS_INTERFACE_MAP_ENTRY(nsIXPCComponents)
  NS_INTERFACE_MAP_ENTRY(nsIXPCScriptable)
  NS_INTERFACE_MAP_ENTRY(nsIClassInfo)
#ifdef XPC_USE_SECURITY_CHECKED_COMPONENT
  NS_INTERFACE_MAP_ENTRY(nsISecurityCheckedComponent)
#endif
  NS_INTERFACE_MAP_ENTRY_AMBIGUOUS(nsISupports, nsIXPCComponents)
NS_INTERFACE_MAP_END_THREADSAFE

NS_IMPL_THREADSAFE_ADDREF(nsXPCComponents)
NS_IMPL_THREADSAFE_RELEASE(nsXPCComponents)

/* void getInterfaces (out PRUint32 count, [array, size_is (count), retval] 
                       out nsIIDPtr array); */
NS_IMETHODIMP 
nsXPCComponents::GetInterfaces(PRUint32 *aCount, nsIID * **aArray)
{
    PRUint32 count = 2;
#ifdef XPC_USE_SECURITY_CHECKED_COMPONENT
    ++count;
#endif
    *aCount = count;
    nsIID **array;
    *aArray = array = static_cast<nsIID**>(nsMemory::Alloc(count * sizeof(nsIID*)));
    if(!array)
        return NS_ERROR_OUT_OF_MEMORY;

    PRUint32 index = 0;
    nsIID* clone;
#define PUSH_IID(id) \
    clone = static_cast<nsIID *>(nsMemory::Clone(&NS_GET_IID( id ),   \
                                                    sizeof(nsIID)));  \
    if (!clone)                                                       \
        goto oom;                                                     \
    array[index++] = clone;

    PUSH_IID(nsIXPCComponents)
    PUSH_IID(nsIXPCScriptable)
#ifdef XPC_USE_SECURITY_CHECKED_COMPONENT
    PUSH_IID(nsISecurityCheckedComponent)
#endif
#undef PUSH_IID

    return NS_OK;
oom:
    while (index)
        nsMemory::Free(array[--index]);
    nsMemory::Free(array);
    *aArray = nsnull;
    return NS_ERROR_OUT_OF_MEMORY;
}

/* nsISupports getHelperForLanguage (in PRUint32 language); */
NS_IMETHODIMP 
nsXPCComponents::GetHelperForLanguage(PRUint32 language, 
                                      nsISupports **retval)
{
    *retval = nsnull;
    return NS_OK;
}

/* readonly attribute string contractID; */
NS_IMETHODIMP 
nsXPCComponents::GetContractID(char * *aContractID)
{
    *aContractID = nsnull;
    return NS_ERROR_NOT_AVAILABLE;
}

/* readonly attribute string classDescription; */
NS_IMETHODIMP 
nsXPCComponents::GetClassDescription(char * *aClassDescription)
{
    static const char classDescription[] = "XPCComponents";
    *aClassDescription = (char*)nsMemory::Clone(classDescription, sizeof(classDescription));
    return *aClassDescription ? NS_OK : NS_ERROR_OUT_OF_MEMORY;
}

/* readonly attribute nsCIDPtr classID; */
NS_IMETHODIMP 
nsXPCComponents::GetClassID(nsCID * *aClassID)
{
    *aClassID = nsnull;
    return NS_OK;
}

/* readonly attribute PRUint32 implementationLanguage; */
NS_IMETHODIMP 
nsXPCComponents::GetImplementationLanguage(
    PRUint32 *aImplementationLanguage)
{
    *aImplementationLanguage = nsIProgrammingLanguage::CPLUSPLUS;
    return NS_OK;
}

/* readonly attribute PRUint32 flags; */
NS_IMETHODIMP 
nsXPCComponents::GetFlags(PRUint32 *aFlags)
{
    *aFlags = nsIClassInfo::THREADSAFE;
    return NS_OK;
}

/* [notxpcom] readonly attribute nsCID classIDNoAlloc; */
NS_IMETHODIMP 
nsXPCComponents::GetClassIDNoAlloc(nsCID *aClassIDNoAlloc)
{
    return NS_ERROR_NOT_AVAILABLE;
}

nsXPCComponents::nsXPCComponents()
    :   mInterfaces(nsnull),
        mInterfacesByID(nsnull),
        mClasses(nsnull),
        mClassesByID(nsnull),
        mResults(nsnull),
        mID(nsnull),
        mException(nsnull),
        mConstructor(nsnull),
        mUtils(nsnull)
{
}

nsXPCComponents::~nsXPCComponents()
{
    ClearMembers();
}

void
nsXPCComponents::ClearMembers()
{
    NS_IF_RELEASE(mInterfaces);
    NS_IF_RELEASE(mInterfacesByID);
    NS_IF_RELEASE(mClasses);
    NS_IF_RELEASE(mClassesByID);
    NS_IF_RELEASE(mResults);
    NS_IF_RELEASE(mID);
    NS_IF_RELEASE(mException);
    NS_IF_RELEASE(mConstructor);
    NS_IF_RELEASE(mUtils);
}

/*******************************************/
#define XPC_IMPL_GET_OBJ_METHOD(_b, _n) \
NS_IMETHODIMP nsXPCComponents::Get##_n(_b##_n * *a##_n) { \
    NS_ENSURE_ARG_POINTER(a##_n); \
    if(!m##_n) { \
        if(!(m##_n = new nsXPCComponents_##_n())) { \
            *a##_n = nsnull; \
            return NS_ERROR_OUT_OF_MEMORY; \
        } \
        NS_ADDREF(m##_n); \
    } \
    NS_ADDREF(m##_n); \
    *a##_n = m##_n; \
    return NS_OK; \
}

XPC_IMPL_GET_OBJ_METHOD(nsIScriptable,     Interfaces)
XPC_IMPL_GET_OBJ_METHOD(nsIScriptable,     InterfacesByID)
XPC_IMPL_GET_OBJ_METHOD(nsIXPCComponents_, Classes)
XPC_IMPL_GET_OBJ_METHOD(nsIXPCComponents_, ClassesByID)
XPC_IMPL_GET_OBJ_METHOD(nsIXPCComponents_, Results)
XPC_IMPL_GET_OBJ_METHOD(nsIXPCComponents_, ID)
XPC_IMPL_GET_OBJ_METHOD(nsIXPCComponents_, Exception)
XPC_IMPL_GET_OBJ_METHOD(nsIXPCComponents_, Constructor)
XPC_IMPL_GET_OBJ_METHOD(nsIXPCComponents_, Utils)

#undef XPC_IMPL_GET_OBJ_METHOD
/*******************************************/

NS_IMETHODIMP
nsXPCComponents::IsSuccessCode(nsresult result, PRBool *out)
{
    *out = NS_SUCCEEDED(result);
    return NS_OK;
}

NS_IMETHODIMP
nsXPCComponents::GetStack(nsIStackFrame * *aStack)
{
    nsresult rv;
    nsXPConnect* xpc = nsXPConnect::GetXPConnect();
    if(!xpc)
        return NS_ERROR_FAILURE;
    rv = xpc->GetCurrentJSStack(aStack);
    return rv;
}

NS_IMETHODIMP
nsXPCComponents::GetManager(nsIComponentManager * *aManager)
{
    NS_ASSERTION(aManager, "bad param");
    return NS_GetComponentManager(aManager);
}

/**********************************************/

// The nsIXPCScriptable map declaration that will generate stubs for us...
#define XPC_MAP_CLASSNAME           nsXPCComponents
#define XPC_MAP_QUOTED_CLASSNAME   "nsXPCComponents"
#define                             XPC_MAP_WANT_NEWRESOLVE
#define                             XPC_MAP_WANT_GETPROPERTY
#define                             XPC_MAP_WANT_SETPROPERTY
#define XPC_MAP_FLAGS               nsIXPCScriptable::ALLOW_PROP_MODS_DURING_RESOLVE
#include "xpc_map_end.h" /* This will #undef the above */

/* PRBool newResolve (in nsIXPConnectWrappedNative wrapper, in JSContextPtr cx, in JSObjectPtr obj, in jsval id, in PRUint32 flags, out JSObjectPtr objp); */
NS_IMETHODIMP
nsXPCComponents::NewResolve(nsIXPConnectWrappedNative *wrapper,
                            JSContext * cx, JSObject * obj,
                            jsid id, PRUint32 flags,
                            JSObject * *objp, PRBool *_retval)
{
    XPCJSRuntime* rt = nsXPConnect::GetRuntimeInstance();
    if(!rt)
        return NS_ERROR_FAILURE;

    uintN attrs = 0;

    if(id == rt->GetStringID(XPCJSRuntime::IDX_LAST_RESULT))
        attrs = JSPROP_READONLY;
    else if(id != rt->GetStringID(XPCJSRuntime::IDX_RETURN_CODE))
        return NS_OK;

    *objp = obj;
    *_retval = JS_DefinePropertyById(cx, obj, id, JSVAL_VOID, nsnull, nsnull,
                                     JSPROP_ENUMERATE | JSPROP_PERMANENT |
                                     attrs);
    return NS_OK;
}

/* PRBool getProperty (in nsIXPConnectWrappedNative wrapper, in JSContextPtr cx, in JSObjectPtr obj, in jsval id, in JSValPtr vp); */
NS_IMETHODIMP
nsXPCComponents::GetProperty(nsIXPConnectWrappedNative *wrapper,
                             JSContext * cx, JSObject * obj,
                             jsid id, jsval * vp, PRBool *_retval)
{
    XPCContext* xpcc = XPCContext::GetXPCContext(cx);
    if(!xpcc)
        return NS_ERROR_FAILURE;

    PRBool doResult = JS_FALSE;
    nsresult res;
    XPCJSRuntime* rt = xpcc->GetRuntime();
    if(id == rt->GetStringID(XPCJSRuntime::IDX_LAST_RESULT))
    {
        res = xpcc->GetLastResult();
        doResult = JS_TRUE;
    }
    else if(id == rt->GetStringID(XPCJSRuntime::IDX_RETURN_CODE))
    {
        res = xpcc->GetPendingResult();
        doResult = JS_TRUE;
    }

    nsresult rv = NS_OK;
    if(doResult)
    {
        if(!JS_NewNumberValue(cx, (jsdouble) res, vp))
            return NS_ERROR_OUT_OF_MEMORY;
        rv = NS_SUCCESS_I_DID_SOMETHING;
    }

    return rv;
}

/* PRBool setProperty (in nsIXPConnectWrappedNative wrapper, in JSContextPtr cx, in JSObjectPtr obj, in jsid id, in JSValPtr vp); */
NS_IMETHODIMP
nsXPCComponents::SetProperty(nsIXPConnectWrappedNative *wrapper,
                             JSContext * cx, JSObject * obj, jsid id,
                             jsval * vp, PRBool *_retval)
{
    XPCContext* xpcc = XPCContext::GetXPCContext(cx);
    if(!xpcc)
        return NS_ERROR_FAILURE;

    XPCJSRuntime* rt = xpcc->GetRuntime();
    if(!rt)
        return NS_ERROR_FAILURE;

    if(id == rt->GetStringID(XPCJSRuntime::IDX_RETURN_CODE))
    {
        nsresult rv;
        if(JS_ValueToECMAUint32(cx, *vp, (uint32*)&rv))
        {
            xpcc->SetPendingResult(rv);
            xpcc->SetLastResult(rv);
            return NS_SUCCESS_I_DID_SOMETHING;
        }
        return NS_ERROR_FAILURE;
    }

    return NS_ERROR_XPC_CANT_MODIFY_PROP_ON_WN;
}

// static
JSBool
nsXPCComponents::AttachNewComponentsObject(XPCCallContext& ccx,
                                           XPCWrappedNativeScope* aScope,
                                           JSObject* aGlobal)
{
    if(!aGlobal)
        return JS_FALSE;

    nsXPCComponents* components = new nsXPCComponents();
    if(!components)
        return JS_FALSE;

    nsCOMPtr<nsIXPCComponents> cholder(components);

    AutoMarkingNativeInterfacePtr iface(ccx);
    iface = XPCNativeInterface::GetNewOrUsed(ccx, &NS_GET_IID(nsIXPCComponents));

    if(!iface)
        return JS_FALSE;

    nsCOMPtr<XPCWrappedNative> wrapper;
    xpcObjectHelper helper(cholder);
    XPCWrappedNative::GetNewOrUsed(ccx, helper, aScope, iface,
                                   OBJ_IS_NOT_GLOBAL, getter_AddRefs(wrapper));
    if(!wrapper)
        return JS_FALSE;

    aScope->SetComponents(components);

    jsid id = ccx.GetRuntime()->GetStringID(XPCJSRuntime::IDX_COMPONENTS);
    JSObject* obj;

    return NS_SUCCEEDED(wrapper->GetJSObject(&obj)) &&
           obj && JS_DefinePropertyById(ccx, aGlobal, id, OBJECT_TO_JSVAL(obj),
                                        nsnull, nsnull,
                                        JSPROP_PERMANENT | JSPROP_READONLY);
}

/* void lookupMethod (); */
NS_IMETHODIMP nsXPCComponents::LookupMethod()
{
    nsresult rv;
    nsCOMPtr<nsIXPCComponents_Utils> utils;

    NS_WARNING("Components.lookupMethod deprecated, use Components.utils.lookupMethod");
    rv = GetUtils(getter_AddRefs(utils));
    if (NS_FAILED(rv))
        return rv;

    return utils->LookupMethod();
}

/* void reportError (); */
NS_IMETHODIMP nsXPCComponents::ReportError()
{
    nsresult rv;
    nsCOMPtr<nsIXPCComponents_Utils> utils;

    NS_WARNING("Components.reportError deprecated, use Components.utils.reportError");
    rv = GetUtils(getter_AddRefs(utils));
    if (NS_FAILED(rv))
        return rv;

    return utils->ReportError();
}

#ifdef XPC_USE_SECURITY_CHECKED_COMPONENT
/* string canCreateWrapper (in nsIIDPtr iid); */
NS_IMETHODIMP
nsXPCComponents::CanCreateWrapper(const nsIID * iid, char **_retval)
{
    // We let anyone do this...
    *_retval = xpc_CloneAllAccess();
    return NS_OK;
}

/* string canCallMethod (in nsIIDPtr iid, in wstring methodName); */
NS_IMETHODIMP
nsXPCComponents::CanCallMethod(const nsIID * iid, const PRUnichar *methodName, char **_retval)
{
    static const char* allowed[] = { "isSuccessCode", "lookupMethod", nsnull };
    *_retval = xpc_CheckAccessList(methodName, allowed);
    return NS_OK;
}

/* string canGetProperty (in nsIIDPtr iid, in wstring propertyName); */
NS_IMETHODIMP
nsXPCComponents::CanGetProperty(const nsIID * iid, const PRUnichar *propertyName, char **_retval)
{
    static const char* allowed[] = { "interfaces", "interfacesByID", "results", nsnull};
    *_retval = xpc_CheckAccessList(propertyName, allowed);
    return NS_OK;
}

/* string canSetProperty (in nsIIDPtr iid, in wstring propertyName); */
NS_IMETHODIMP
nsXPCComponents::CanSetProperty(const nsIID * iid, const PRUnichar *propertyName, char **_retval)
{
    // If you have to ask, then the answer is NO
    *_retval = nsnull;
    return NS_OK;
}
#endif
