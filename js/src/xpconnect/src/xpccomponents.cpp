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
 * Copyright (C) 1998 Netscape Communications Corporation. All
 * Rights Reserved.
 *
 * Contributor(s):
 *   John Bandhauer <jband@netscape.com> (original author)
 *   Pierre Phaneuf <pp@ludusdesign.com>
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

/* The "Components" xpcom objects for JavaScript. */

#include "xpcprivate.h"


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
       NS_SUCCEEDED(wn->GetNative(getter_AddRefs(sup))) && sup &&
       NS_SUCCEEDED(sup->QueryInterface(iid, (void**)&iface)) && iface)
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
    CopyUCS2toASCII(nsDependentString(wideName), asciiName);

    for(const char** p = list; *p; p++)
        if(!strcmp(*p, asciiName))
            return xpc_CloneAllAccess();
    
    return nsnull;
}
#endif

/***************************************************************************/

class nsXPCComponents_Interfaces :
            public nsIXPCComponents_Interfaces,
            public nsIXPCScriptable
#ifdef XPC_USE_SECURITY_CHECKED_COMPONENT
          , public nsISecurityCheckedComponent
#endif
{
public:
    // all the interface method declarations...
    NS_DECL_ISUPPORTS
    NS_DECL_NSIXPCCOMPONENTS_INTERFACES
    NS_DECL_NSIXPCSCRIPTABLE
#ifdef XPC_USE_SECURITY_CHECKED_COMPONENT
    NS_DECL_NSISECURITYCHECKEDCOMPONENT
#endif

public:
    nsXPCComponents_Interfaces();
    virtual ~nsXPCComponents_Interfaces();
};

nsXPCComponents_Interfaces::nsXPCComponents_Interfaces()
{
    NS_INIT_ISUPPORTS();
}

nsXPCComponents_Interfaces::~nsXPCComponents_Interfaces()
{
    // empty
}


NS_INTERFACE_MAP_BEGIN(nsXPCComponents_Interfaces)
  NS_INTERFACE_MAP_ENTRY(nsIXPCComponents_Interfaces)
  NS_INTERFACE_MAP_ENTRY(nsIXPCScriptable)
#ifdef XPC_USE_SECURITY_CHECKED_COMPONENT
  NS_INTERFACE_MAP_ENTRY(nsISecurityCheckedComponent)
#endif
  NS_INTERFACE_MAP_ENTRY_AMBIGUOUS(nsISupports, nsIXPCComponents_Interfaces)
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
        {
            nsCOMPtr<nsIInterfaceInfoManager>
                iim(dont_AddRef(XPTI_GetInterfaceInfoManager()));

            if(!iim || NS_FAILED(iim->EnumerateInterfaces(&e)) || !e ||
               NS_FAILED(e->First()))

            {
                *statep = JSVAL_NULL;
                return NS_ERROR_UNEXPECTED;
            }

            *statep = PRIVATE_TO_JSVAL(e);
            if(idp)
                *idp = JSVAL_ZERO; // indicate that we don't know the count
            return NS_OK;
        }
        case JSENUMERATE_NEXT:
        {
            nsCOMPtr<nsISupports> isup;

            e = (nsIEnumerator*) JSVAL_TO_PRIVATE(*statep);
            
            while(1)
            {
                if(NS_COMFALSE == e->IsDone() &&
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

/* PRBool newResolve (in nsIXPConnectWrappedNative wrapper, in JSContextPtr cx, in JSObjectPtr obj, in JSVal id, in PRUint32 flags, out JSObjectPtr objp); */
NS_IMETHODIMP
nsXPCComponents_Interfaces::NewResolve(nsIXPConnectWrappedNative *wrapper,
                                       JSContext * cx, JSObject * obj,
                                       jsval id, PRUint32 flags,
                                       JSObject * *objp, PRBool *_retval)
{
    const char* name = nsnull;

    if(JSVAL_IS_STRING(id) &&
       nsnull != (name = JS_GetStringBytes(JSVAL_TO_STRING(id))) &&
       name[0] != '{') // we only allow interfaces by name here
    {
        nsCOMPtr<nsIJSIID> nsid =
            dont_AddRef(NS_STATIC_CAST(nsIJSIID*,nsJSIID::NewID(name)));
        if(nsid)
        {
            nsCOMPtr<nsIXPConnect> xpc;
            wrapper->GetXPConnect(getter_AddRefs(xpc));
            if(xpc)
            {
                nsCOMPtr<nsIXPConnectJSObjectHolder> holder;
                if(NS_SUCCEEDED(xpc->WrapNative(cx, obj,
                                                NS_STATIC_CAST(nsIJSIID*,nsid),
                                                NS_GET_IID(nsIJSIID),
                                                getter_AddRefs(holder))))
                {
                    JSObject* idobj;
                    if(holder && NS_SUCCEEDED(holder->GetJSObject(&idobj)))
                    {
                        jsid idid;

                        *objp = obj;
                        *_retval = JS_ValueToId(cx, id, &idid) &&
                                   OBJ_DEFINE_PROPERTY(cx, obj, idid,
                                                       OBJECT_TO_JSVAL(idobj),
                                                       nsnull, nsnull,
                                                       JSPROP_ENUMERATE |
                                                       JSPROP_READONLY |
                                                       JSPROP_PERMANENT,
                                                       nsnull);
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



class nsXPCComponents_Classes : public nsIXPCComponents_Classes, public nsIXPCScriptable
{
public:
    // all the interface method declarations...
    NS_DECL_ISUPPORTS
    NS_DECL_NSIXPCCOMPONENTS_CLASSES
    NS_DECL_NSIXPCSCRIPTABLE

public:
    nsXPCComponents_Classes();
    virtual ~nsXPCComponents_Classes();
};

nsXPCComponents_Classes::nsXPCComponents_Classes()
{
    NS_INIT_ISUPPORTS();
}

nsXPCComponents_Classes::~nsXPCComponents_Classes()
{
    // empty
}

NS_INTERFACE_MAP_BEGIN(nsXPCComponents_Classes)
  NS_INTERFACE_MAP_ENTRY(nsIXPCComponents_Classes)
  NS_INTERFACE_MAP_ENTRY(nsIXPCScriptable)
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
    nsIEnumerator* e;

    switch(enum_op)
    {
        case JSENUMERATE_INIT:
        {
            nsIComponentManager* compMgr;
            if(NS_FAILED(NS_GetGlobalComponentManager(&compMgr)) ||
               !compMgr || NS_FAILED(compMgr->EnumerateContractIDs(&e)) || !e ||
               NS_FAILED(e->First()))

            {
                *statep = JSVAL_NULL;
                return NS_ERROR_UNEXPECTED;
            }

            *statep = PRIVATE_TO_JSVAL(e);
            if(idp)
                *idp = JSVAL_ZERO; // indicate that we don't know the count
            return NS_OK;
        }
        case JSENUMERATE_NEXT:
        {
            nsCOMPtr<nsISupports> isup;

            e = (nsIEnumerator*) JSVAL_TO_PRIVATE(*statep);
            if(NS_COMFALSE == e->IsDone() &&
               NS_SUCCEEDED(e->CurrentItem(getter_AddRefs(isup))) && isup)
            {
                e->Next();
                nsCOMPtr<nsISupportsString> holder(do_QueryInterface(isup));
                if(holder)
                {
                    char* name;
                    if(NS_SUCCEEDED(holder->GetData(&name)) && name)
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
            e = (nsIEnumerator*) JSVAL_TO_PRIVATE(*statep);
            NS_IF_RELEASE(e);
            *statep = JSVAL_NULL;
            return NS_OK;
    }
}

/* PRBool newResolve (in nsIXPConnectWrappedNative wrapper, in JSContextPtr cx, in JSObjectPtr obj, in JSVal id, in PRUint32 flags, out JSObjectPtr objp); */
NS_IMETHODIMP
nsXPCComponents_Classes::NewResolve(nsIXPConnectWrappedNative *wrapper,
                                    JSContext * cx, JSObject * obj,
                                    jsval id, PRUint32 flags,
                                    JSObject * *objp, PRBool *_retval)

{
    const char* name = nsnull;

    if(JSVAL_IS_STRING(id) &&
       nsnull != (name = JS_GetStringBytes(JSVAL_TO_STRING(id))) &&
       name[0] != '{') // we only allow contractids here
    {
        nsCOMPtr<nsIJSCID> nsid =
            dont_AddRef(NS_STATIC_CAST(nsIJSCID*,nsJSCID::NewID(name)));
        if(nsid)
        {
            nsCOMPtr<nsIXPConnect> xpc;
            wrapper->GetXPConnect(getter_AddRefs(xpc));
            if(xpc)
            {
                nsCOMPtr<nsIXPConnectJSObjectHolder> holder;
                if(NS_SUCCEEDED(xpc->WrapNative(cx, obj,
                                                NS_STATIC_CAST(nsIJSCID*,nsid),
                                                NS_GET_IID(nsIJSCID),
                                                getter_AddRefs(holder))))
                {
                    JSObject* idobj;
                    if(holder && NS_SUCCEEDED(holder->GetJSObject(&idobj)))
                    {
                        jsid idid;

                        *objp = obj;
                        *_retval = JS_ValueToId(cx, id, &idid) &&
                                   OBJ_DEFINE_PROPERTY(cx, obj, idid,
                                                       OBJECT_TO_JSVAL(idobj),
                                                       nsnull, nsnull,
                                                       JSPROP_ENUMERATE |
                                                       JSPROP_READONLY |
                                                       JSPROP_PERMANENT,
                                                       nsnull);
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

class nsXPCComponents_ClassesByID : public nsIXPCComponents_ClassesByID, public nsIXPCScriptable
{
public:
    // all the interface method declarations...
    NS_DECL_ISUPPORTS
    NS_DECL_NSIXPCCOMPONENTS_CLASSESBYID
    NS_DECL_NSIXPCSCRIPTABLE

public:
    nsXPCComponents_ClassesByID();
    virtual ~nsXPCComponents_ClassesByID();
};

nsXPCComponents_ClassesByID::nsXPCComponents_ClassesByID()
{
    NS_INIT_ISUPPORTS();
}

nsXPCComponents_ClassesByID::~nsXPCComponents_ClassesByID()
{
    // empty
}

NS_INTERFACE_MAP_BEGIN(nsXPCComponents_ClassesByID)
  NS_INTERFACE_MAP_ENTRY(nsIXPCComponents_ClassesByID)
  NS_INTERFACE_MAP_ENTRY(nsIXPCScriptable)
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
    nsIEnumerator* e;

    switch(enum_op)
    {
        case JSENUMERATE_INIT:
        {
            nsIComponentManager* compMgr;
            if(NS_FAILED(NS_GetGlobalComponentManager(&compMgr)) ||
               !compMgr || NS_FAILED(compMgr->EnumerateCLSIDs(&e)) || !e ||
               NS_FAILED(e->First()))

            {
                *statep = JSVAL_NULL;
                return NS_ERROR_UNEXPECTED;
            }

            *statep = PRIVATE_TO_JSVAL(e);
            if(idp)
                *idp = JSVAL_ZERO; // indicate that we don't know the count
            return NS_OK;
        }
        case JSENUMERATE_NEXT:
        {
            nsCOMPtr<nsISupports> isup;

            e = (nsIEnumerator*) JSVAL_TO_PRIVATE(*statep);
            if(NS_COMFALSE == e->IsDone() &&
               NS_SUCCEEDED(e->CurrentItem(getter_AddRefs(isup))) && isup)
            {
                e->Next();
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
            e = (nsIEnumerator*) JSVAL_TO_PRIVATE(*statep);
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

    nsIComponentManager* compMgr;
    if(NS_FAILED(NS_GetGlobalComponentManager(&compMgr)) ||
       !compMgr || NS_FAILED(compMgr->IsRegistered(id, &registered)))
        return PR_FALSE;

    return registered;
}

/* PRBool newResolve (in nsIXPConnectWrappedNative wrapper, in JSContextPtr cx, in JSObjectPtr obj, in JSVal id, in PRUint32 flags, out JSObjectPtr objp); */
NS_IMETHODIMP
nsXPCComponents_ClassesByID::NewResolve(nsIXPConnectWrappedNative *wrapper,
                                        JSContext * cx, JSObject * obj,
                                        jsval id, PRUint32 flags,
                                        JSObject * *objp, PRBool *_retval)
{
    const char* name = nsnull;

    if(JSVAL_IS_STRING(id) &&
       nsnull != (name = JS_GetStringBytes(JSVAL_TO_STRING(id))) &&
       name[0] == '{' &&
       IsRegisteredCLSID(name)) // we only allow canonical CLSIDs here
    {
        nsCOMPtr<nsIJSCID> nsid =
            dont_AddRef(NS_STATIC_CAST(nsIJSCID*,nsJSCID::NewID(name)));
        if(nsid)
        {
            nsCOMPtr<nsIXPConnect> xpc;
            wrapper->GetXPConnect(getter_AddRefs(xpc));
            if(xpc)
            {
                nsCOMPtr<nsIXPConnectJSObjectHolder> holder;
                if(NS_SUCCEEDED(xpc->WrapNative(cx, obj,
                                                NS_STATIC_CAST(nsIJSCID*,nsid),
                                                NS_GET_IID(nsIJSCID),
                                                getter_AddRefs(holder))))
                {
                    JSObject* idobj;
                    if(holder && NS_SUCCEEDED(holder->GetJSObject(&idobj)))
                    {
                        jsid idid;

                        *objp = obj;
                        *_retval = JS_ValueToId(cx, id, &idid) &&
                                   OBJ_DEFINE_PROPERTY(cx, obj, idid,
                                                       OBJECT_TO_JSVAL(idobj),
                                                       nsnull, nsnull,
                                                       JSPROP_ENUMERATE |
                                                       JSPROP_READONLY |
                                                       JSPROP_PERMANENT,
                                                       nsnull);
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

class nsXPCComponents_Results : public nsIXPCComponents_Results, public nsIXPCScriptable
{
public:
    // all the interface method declarations...
    NS_DECL_ISUPPORTS
    NS_DECL_NSIXPCCOMPONENTS_RESULTS
    NS_DECL_NSIXPCSCRIPTABLE

public:
    nsXPCComponents_Results();
    virtual ~nsXPCComponents_Results();
};

nsXPCComponents_Results::nsXPCComponents_Results()
{
    NS_INIT_ISUPPORTS();
}

nsXPCComponents_Results::~nsXPCComponents_Results()
{
    // empty
}

NS_INTERFACE_MAP_BEGIN(nsXPCComponents_Results)
  NS_INTERFACE_MAP_ENTRY(nsIXPCComponents_Results)
  NS_INTERFACE_MAP_ENTRY(nsIXPCScriptable)
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
        {
            if(idp)
                *idp = INT_TO_JSVAL(nsXPCException::GetNSResultCount());

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


/* PRBool newResolve (in nsIXPConnectWrappedNative wrapper, in JSContextPtr cx, in JSObjectPtr obj, in JSVal id, in PRUint32 flags, out JSObjectPtr objp); */
NS_IMETHODIMP
nsXPCComponents_Results::NewResolve(nsIXPConnectWrappedNative *wrapper,
                                    JSContext * cx, JSObject * obj,
                                    jsval id, PRUint32 flags,
                                    JSObject * *objp, PRBool *_retval)
{
    const char* name = nsnull;

    if(JSVAL_IS_STRING(id) &&
       nsnull != (name = JS_GetStringBytes(JSVAL_TO_STRING(id))))
    {
        const char* rv_name;
        void* iter = nsnull;
        nsresult rv;
        while(nsXPCException::IterateNSResults(&rv, &rv_name, nsnull, &iter))
        {
            if(!strcmp(name, rv_name))
            {
                jsid idid;
                jsval val;

                *objp = obj;
                if(!JS_NewNumberValue(cx, (jsdouble)rv, &val) ||
                   !JS_ValueToId(cx, id, &idid) ||
                   !OBJ_DEFINE_PROPERTY(cx, obj, idid, val,
                                        nsnull, nsnull,
                                        JSPROP_ENUMERATE |
                                        JSPROP_READONLY |
                                        JSPROP_PERMANENT,
                                        nsnull))
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

class nsXPCComponents_ID : public nsIXPCComponents_ID, public nsIXPCScriptable
{
public:
    // all the interface method declarations...
    NS_DECL_ISUPPORTS
    NS_DECL_NSIXPCCOMPONENTS_ID
    NS_DECL_NSIXPCSCRIPTABLE


public:
    nsXPCComponents_ID();
    virtual ~nsXPCComponents_ID();

private:
    NS_METHOD CallOrConstruct(nsIXPConnectWrappedNative *wrapper,
                              JSContext * cx, JSObject * obj,
                              PRUint32 argc, jsval * argv,
                              jsval * vp, PRBool *_retval);
};

nsXPCComponents_ID::nsXPCComponents_ID()
{
    NS_INIT_ISUPPORTS();
}

nsXPCComponents_ID::~nsXPCComponents_ID()
{
    // empty
}

NS_INTERFACE_MAP_BEGIN(nsXPCComponents_ID)
  NS_INTERFACE_MAP_ENTRY(nsIXPCComponents_ID)
  NS_INTERFACE_MAP_ENTRY(nsIXPCScriptable)
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

NS_METHOD
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

/* PRBool hasInstance (in nsIXPConnectWrappedNative wrapper, in JSContextPtr cx, in JSObjectPtr obj, in JSVal val, out PRBool bp); */
NS_IMETHODIMP
nsXPCComponents_ID::HasInstance(nsIXPConnectWrappedNative *wrapper,
                                JSContext * cx, JSObject * obj,
                                jsval val, PRBool *bp, PRBool *_retval)
{
    if(bp)
        *bp = JSValIsInterfaceOfType(cx, val, NS_GET_IID(nsIJSID));
    return NS_OK;
}

/***************************************************************************/
// JavaScript Constructor for nsIXPCException objects (Components.Exception)

class nsXPCComponents_Exception : public nsIXPCComponents_Exception, public nsIXPCScriptable
{
public:
    // all the interface method declarations...
    NS_DECL_ISUPPORTS
    NS_DECL_NSIXPCCOMPONENTS_EXCEPTION
    NS_DECL_NSIXPCSCRIPTABLE


public:
    nsXPCComponents_Exception();
    virtual ~nsXPCComponents_Exception();

private:
    NS_METHOD CallOrConstruct(nsIXPConnectWrappedNative *wrapper,
                              JSContext * cx, JSObject * obj,
                              PRUint32 argc, jsval * argv,
                              jsval * vp, PRBool *_retval);
};

nsXPCComponents_Exception::nsXPCComponents_Exception()
{
    NS_INIT_ISUPPORTS();
}

nsXPCComponents_Exception::~nsXPCComponents_Exception()
{
    // empty
}

NS_INTERFACE_MAP_BEGIN(nsXPCComponents_Exception)
  NS_INTERFACE_MAP_ENTRY(nsIXPCComponents_Exception)
  NS_INTERFACE_MAP_ENTRY(nsIXPCScriptable)
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

NS_METHOD
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

/* PRBool hasInstance (in nsIXPConnectWrappedNative wrapper, in JSContextPtr cx, in JSObjectPtr obj, in JSVal val, out PRBool bp); */
NS_IMETHODIMP
nsXPCComponents_Exception::HasInstance(nsIXPConnectWrappedNative *wrapper,
                                       JSContext * cx, JSObject * obj,
                                       jsval val, PRBool *bp,
                                       PRBool *_retval)
{
    if(bp)
        *bp = JSValIsInterfaceOfType(cx, val, NS_GET_IID(nsIXPCException));
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

class nsXPCConstructor : public nsIXPCConstructor, public nsIXPCScriptable
{
public:
    NS_DEFINE_STATIC_CID_ACCESSOR(NS_XPCCONSTRUCTOR_CID)
public:
    // all the interface method declarations...
    NS_DECL_ISUPPORTS
    NS_DECL_NSIXPCCONSTRUCTOR
    NS_DECL_NSIXPCSCRIPTABLE

public:
    nsXPCConstructor(); // not implemented
    nsXPCConstructor(nsIJSCID* aClassID,
                     nsIJSIID* aInterfaceID,
                     const char* aInitializer);
    virtual ~nsXPCConstructor();

private:
    NS_METHOD CallOrConstruct(nsIXPConnectWrappedNative *wrapper,
                              JSContext * cx, JSObject * obj,
                              PRUint32 argc, jsval * argv,
                              jsval * vp, PRBool *_retval);
private:
    nsIJSCID* mClassID;
    nsIJSIID* mInterfaceID;
    char*     mInitializer;
};

nsXPCConstructor::nsXPCConstructor(nsIJSCID* aClassID,
                                   nsIJSIID* aInterfaceID,
                                   const char* aInitializer)
{
    NS_INIT_ISUPPORTS();
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

NS_METHOD
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

class nsXPCComponents_Constructor : public nsIXPCComponents_Constructor, public nsIXPCScriptable
{
public:
    // all the interface method declarations...
    NS_DECL_ISUPPORTS
    NS_DECL_NSIXPCCOMPONENTS_CONSTRUCTOR
    NS_DECL_NSIXPCSCRIPTABLE

public:
    nsXPCComponents_Constructor();
    virtual ~nsXPCComponents_Constructor();

private:
    NS_METHOD CallOrConstruct(nsIXPConnectWrappedNative *wrapper,
                              JSContext * cx, JSObject * obj,
                              PRUint32 argc, jsval * argv,
                              jsval * vp, PRBool *_retval);
};

nsXPCComponents_Constructor::nsXPCComponents_Constructor()
{
    NS_INIT_ISUPPORTS();
}

nsXPCComponents_Constructor::~nsXPCComponents_Constructor()
{
    // empty
}

NS_INTERFACE_MAP_BEGIN(nsXPCComponents_Constructor)
  NS_INTERFACE_MAP_ENTRY(nsIXPCComponents_Constructor)
  NS_INTERFACE_MAP_ENTRY(nsIXPCScriptable)
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

NS_METHOD
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

        nsCOMPtr<nsIXPCComponents_Interfaces> ifaces;
        nsCOMPtr<nsIXPConnectJSObjectHolder> holder;
        JSObject* ifacesObj = nsnull;

        // we do the lookup by asking the Components.interfaces object
        // for the property with this name - i.e. we let its caching of these
        // nsIJSIID objects work for us.

        if(NS_FAILED(comp->GetInterfaces(getter_AddRefs(ifaces))) ||
           NS_FAILED(xpc->WrapNative(cx, obj, ifaces,
                                     NS_GET_IID(nsIXPCComponents_Interfaces),
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
        nsCOMPtr<nsISupports> sup;
        if(NS_FAILED(xpc->GetWrappedNativeOfJSObject(cx, JSVAL_TO_OBJECT(val),
                                getter_AddRefs(wn))) || !wn ||
           NS_FAILED(wn->GetNative(getter_AddRefs(sup))) || !sup ||
           !(cInterfaceID = do_QueryInterface(sup)))
        {
            return ThrowAndFail(NS_ERROR_XPC_UNEXPECTED, cx, _retval);
        }
    }
    else
    {
        cInterfaceID =
            dont_AddRef(
                NS_STATIC_CAST(nsIJSIID*, nsJSIID::NewID("nsISupports")));
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
        nsCOMPtr<nsISupports> sup;
        if(NS_FAILED(xpc->GetWrappedNativeOfJSObject(cx, JSVAL_TO_OBJECT(val),
                                getter_AddRefs(wn))) || !wn ||
           NS_FAILED(wn->GetNative(getter_AddRefs(sup))) || !sup ||
           !(cClassID = do_QueryInterface(sup)))
        {
            return ThrowAndFail(NS_ERROR_XPC_UNEXPECTED, cx, _retval);
        }
    }

    nsCOMPtr<nsIXPCConstructor> ctor =
        NS_STATIC_CAST(nsIXPCConstructor*,
            new nsXPCConstructor(cClassID, cInterfaceID, cInitializer));
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

/* PRBool hasInstance (in nsIXPConnectWrappedNative wrapper, in JSContextPtr cx, in JSObjectPtr obj, in JSVal val, out PRBool bp); */
NS_IMETHODIMP
nsXPCComponents_Constructor::HasInstance(nsIXPConnectWrappedNative *wrapper,
                                         JSContext * cx, JSObject * obj,
                                         jsval val, PRBool *bp,
                                         PRBool *_retval)
{
    if(bp)
        *bp = JSValIsInterfaceOfType(cx, val, NS_GET_IID(nsIXPCConstructor));
    return NS_OK;
}

/***************************************************************************/
/***************************************************************************/
/***************************************************************************/

// XXXjband We ought to cache the wrapper in the object's slots rather than
// re-wrapping on demand

NS_INTERFACE_MAP_BEGIN(nsXPCComponents)
  NS_INTERFACE_MAP_ENTRY(nsIXPCComponents)
  NS_INTERFACE_MAP_ENTRY(nsIXPCScriptable)
#ifdef XPC_USE_SECURITY_CHECKED_COMPONENT
  NS_INTERFACE_MAP_ENTRY(nsISecurityCheckedComponent)
#endif
  NS_INTERFACE_MAP_ENTRY_AMBIGUOUS(nsISupports, nsIXPCComponents)
NS_INTERFACE_MAP_END_THREADSAFE

NS_IMPL_THREADSAFE_ADDREF(nsXPCComponents)
NS_IMPL_THREADSAFE_RELEASE(nsXPCComponents)

nsXPCComponents::nsXPCComponents()
    :   mInterfaces(nsnull),
        mClasses(nsnull),
        mClassesByID(nsnull),
        mResults(nsnull),
        mID(nsnull),
        mException(nsnull),
        mConstructor(nsnull)
{
    NS_INIT_ISUPPORTS();
}

nsXPCComponents::~nsXPCComponents()
{
    NS_IF_RELEASE(mInterfaces);
    NS_IF_RELEASE(mClasses);
    NS_IF_RELEASE(mClassesByID);
    NS_IF_RELEASE(mResults);
    NS_IF_RELEASE(mID);
    NS_IF_RELEASE(mException);
    NS_IF_RELEASE(mConstructor);
}

/*******************************************/
#define XPC_IMPL_GET_OBJ_METHOD(_n) \
NS_IMETHODIMP nsXPCComponents::Get##_n(nsIXPCComponents_##_n * *a##_n) { \
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

XPC_IMPL_GET_OBJ_METHOD(Interfaces);
XPC_IMPL_GET_OBJ_METHOD(Classes);
XPC_IMPL_GET_OBJ_METHOD(ClassesByID);
XPC_IMPL_GET_OBJ_METHOD(Results);
XPC_IMPL_GET_OBJ_METHOD(ID);
XPC_IMPL_GET_OBJ_METHOD(Exception);
XPC_IMPL_GET_OBJ_METHOD(Constructor);

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

    nsIComponentManager* cm;
    if(NS_FAILED(NS_GetGlobalComponentManager(&cm)))
    {
        *aManager = nsnull;
        return NS_ERROR_FAILURE;
    }
    NS_IF_ADDREF(cm);
    *aManager = cm;

    return NS_OK;
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

/* PRBool newResolve (in nsIXPConnectWrappedNative wrapper, in JSContextPtr cx, in JSObjectPtr obj, in JSVal id, in PRUint32 flags, out JSObjectPtr objp); */
NS_IMETHODIMP
nsXPCComponents::NewResolve(nsIXPConnectWrappedNative *wrapper,
                            JSContext * cx, JSObject * obj,
                            jsval id, PRUint32 flags,
                            JSObject * *objp, PRBool *_retval)
{
    XPCJSRuntime* rt = nsXPConnect::GetRuntime();
    if(!rt)
        return NS_ERROR_FAILURE;

    jsid idid;

    if(id == rt->GetStringJSVal(XPCJSRuntime::IDX_LAST_RESULT))
        idid = rt->GetStringID(XPCJSRuntime::IDX_LAST_RESULT);
    else if(id == rt->GetStringJSVal(XPCJSRuntime::IDX_RETURN_CODE))
        idid = rt->GetStringID(XPCJSRuntime::IDX_RETURN_CODE);
    else
        return NS_OK;

    *objp = obj;
    *_retval = OBJ_DEFINE_PROPERTY(cx, obj, idid, JSVAL_VOID,
                                   nsnull, nsnull,
                                   JSPROP_ENUMERATE |
                                   JSPROP_READONLY |
                                   JSPROP_PERMANENT,
                                   nsnull);
    return NS_OK;
}

/* PRBool getProperty (in nsIXPConnectWrappedNative wrapper, in JSContextPtr cx, in JSObjectPtr obj, in JSVal id, in JSValPtr vp); */
NS_IMETHODIMP
nsXPCComponents::GetProperty(nsIXPConnectWrappedNative *wrapper,
                             JSContext * cx, JSObject * obj,
                             jsval id, jsval * vp, PRBool *_retval)
{
    XPCContext* xpcc = nsXPConnect::GetContext(cx);
    if(!xpcc)
        return NS_ERROR_FAILURE;

    PRBool doResult = JS_FALSE;
    nsresult res;
    XPCJSRuntime* rt = xpcc->GetRuntime();
    if(id == rt->GetStringJSVal(XPCJSRuntime::IDX_LAST_RESULT))
    {
        res = xpcc->GetLastResult();
        doResult = JS_TRUE;
    }
    else if(id == rt->GetStringJSVal(XPCJSRuntime::IDX_RETURN_CODE))
    {
        res = xpcc->GetPendingResult();
        doResult = JS_TRUE;
    }

    if(doResult)
    {
        if(!JS_NewNumberValue(cx, (jsdouble) res, vp))
            return NS_ERROR_OUT_OF_MEMORY;
    }

    return NS_OK;
}

/* PRBool setProperty (in nsIXPConnectWrappedNative wrapper, in JSContextPtr cx, in JSObjectPtr obj, in JSVal id, in JSValPtr vp); */
NS_IMETHODIMP
nsXPCComponents::SetProperty(nsIXPConnectWrappedNative *wrapper,
                             JSContext * cx, JSObject * obj, jsval id,
                             jsval * vp, PRBool *_retval)
{
    XPCContext* xpcc = nsXPConnect::GetContext(cx);
    if(!xpcc)
        return NS_ERROR_FAILURE;

    XPCJSRuntime* rt = xpcc->GetRuntime();
    if(!rt)
        return NS_ERROR_FAILURE;

    if(id == rt->GetStringJSVal(XPCJSRuntime::IDX_RETURN_CODE))
    {
        nsresult rv;
        if(JS_ValueToECMAUint32(cx, *vp, (uint32*)&rv))
        {
            xpcc->SetPendingResult(rv);
            xpcc->SetLastResult(rv);
            return NS_OK;
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
    XPCWrappedNative::GetNewOrUsed(ccx, cholder, aScope, iface,
                                   getter_AddRefs(wrapper));
    if(!wrapper)
        return JS_FALSE;

    aScope->SetComponents(components);

    jsid id = ccx.GetRuntime()->GetStringID(XPCJSRuntime::IDX_COMPONENTS);
    JSObject* obj;

    return NS_SUCCEEDED(wrapper->GetJSObject(&obj)) &&
           obj && OBJ_DEFINE_PROPERTY(ccx,
                                      aGlobal, id, OBJECT_TO_JSVAL(obj),
                                      nsnull, nsnull,
                                      JSPROP_PERMANENT | JSPROP_READONLY,
                                      nsnull);
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
    // If you have to ask, then the answer is NO
    *_retval = nsnull;
    return NS_OK;
}

/* string canGetProperty (in nsIIDPtr iid, in wstring propertyName); */
NS_IMETHODIMP
nsXPCComponents::CanGetProperty(const nsIID * iid, const PRUnichar *propertyName, char **_retval)
{
    static const char* allowed[] = { "interfaces", "results", nsnull};
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
