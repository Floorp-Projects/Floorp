/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
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

/* An xpcom implementation of the JavaScript nsIID and nsCID objects. */

#include "xpcprivate.h"

/***************************************************************************/
// nsJSID

NS_IMPL_THREADSAFE_ISUPPORTS1(nsJSID, nsIJSID)

char nsJSID::gNoString[] = "";

nsJSID::nsJSID()
    : mID(GetInvalidIID()), mNumber(gNoString), mName(gNoString)
{
}

nsJSID::~nsJSID()
{
    if(mNumber && mNumber != gNoString)
        NS_Free(mNumber);
    if(mName && mName != gNoString)
        NS_Free(mName);
}

void nsJSID::Reset()
{
    mID = GetInvalidIID();

    if(mNumber && mNumber != gNoString)
        NS_Free(mNumber);
    if(mName && mName != gNoString)
        NS_Free(mName);

    mNumber = mName = nsnull;
}

bool
nsJSID::SetName(const char* name)
{
    NS_ASSERTION(!mName || mName == gNoString ,"name already set");
    NS_ASSERTION(name,"null name");
    mName = NS_strdup(name);
    return mName ? PR_TRUE : PR_FALSE;
}

NS_IMETHODIMP
nsJSID::GetName(char * *aName)
{
    if(!aName)
        return NS_ERROR_NULL_POINTER;

    if(!NameIsSet())
        SetNameToNoString();
    NS_ASSERTION(mName, "name not set");
    *aName = NS_strdup(mName);
    return *aName ? NS_OK : NS_ERROR_OUT_OF_MEMORY;
}

NS_IMETHODIMP
nsJSID::GetNumber(char * *aNumber)
{
    if(!aNumber)
        return NS_ERROR_NULL_POINTER;

    if(!mNumber)
    {
        if(!(mNumber = mID.ToString()))
            mNumber = gNoString;
    }

    *aNumber = NS_strdup(mNumber);
    return *aNumber ? NS_OK : NS_ERROR_OUT_OF_MEMORY;
}

NS_IMETHODIMP_(const nsID*)
nsJSID::GetID()
{
    return &mID;
}

NS_IMETHODIMP
nsJSID::GetValid(bool *aValid)
{
    if(!aValid)
        return NS_ERROR_NULL_POINTER;

    *aValid = IsValid();
    return NS_OK;
}

NS_IMETHODIMP
nsJSID::Equals(nsIJSID *other, bool *_retval)
{
    if(!_retval)
        return NS_ERROR_NULL_POINTER;

    if(!other || mID.Equals(GetInvalidIID()))
    {
        *_retval = PR_FALSE;
        return NS_OK;
    }

    *_retval = other->GetID()->Equals(mID);
    return NS_OK;
}

NS_IMETHODIMP
nsJSID::Initialize(const char *idString)
{
    if(!idString)
        return NS_ERROR_NULL_POINTER;

    if(*idString != '\0' && mID.Equals(GetInvalidIID()))
    {
        Reset();

        if(idString[0] == '{')
        {
            if(mID.Parse(idString))
            {
                return NS_OK;
            }

            // error - reset to invalid state
            mID = GetInvalidIID();
        }
    }
    return NS_ERROR_FAILURE;
}

bool
nsJSID::InitWithName(const nsID& id, const char *nameString)
{
    NS_ASSERTION(nameString, "no name");
    Reset();
    mID = id;
    return SetName(nameString);
}

// try to use the name, if no name, then use the number
NS_IMETHODIMP
nsJSID::ToString(char **_retval)
{
    if(mName && mName != gNoString)
        return GetName(_retval);

    return GetNumber(_retval);
}

const nsID&
nsJSID::GetInvalidIID() const
{
    // {BB1F47B0-D137-11d2-9841-006008962422}
    static nsID invalid = {0xbb1f47b0, 0xd137, 0x11d2,
                            {0x98, 0x41, 0x0, 0x60, 0x8, 0x96, 0x24, 0x22}};
    return invalid;
}

//static
nsJSID*
nsJSID::NewID(const char* str)
{
    if(!str)
    {
        NS_ERROR("no string");
        return nsnull;
    }

    nsJSID* idObj = new nsJSID();
    if(idObj)
    {
        NS_ADDREF(idObj);
        if(NS_FAILED(idObj->Initialize(str)))
            NS_RELEASE(idObj);
    }
    return idObj;
}

//static
nsJSID*
nsJSID::NewID(const nsID& id)
{
    nsJSID* idObj = new nsJSID();
    if(idObj)
    {
        NS_ADDREF(idObj);
        idObj->mID = id;
        idObj->mName = nsnull;
        idObj->mNumber = nsnull;
    }
    return idObj;
}


/***************************************************************************/
// Class object support so that we can share prototypes of wrapper

// This class exists just so we can have a shared scriptable helper for
// the nsJSIID class. The instances implement their own helpers. But we
// needed to be able to indicate to the shared prototypes this single flag:
// nsIXPCScriptable::DONT_ENUM_STATIC_PROPS. And having a class to do it is
// the only means we have. Setting this flag on any given instance scriptable
// helper is not sufficient to convey the information that we don't want
// static properties enumerated on the shared proto.

class SharedScriptableHelperForJSIID : public nsIXPCScriptable
{
public:
    NS_DECL_ISUPPORTS
    NS_DECL_NSIXPCSCRIPTABLE
    SharedScriptableHelperForJSIID() {}
};

NS_INTERFACE_MAP_BEGIN(SharedScriptableHelperForJSIID)
  NS_INTERFACE_MAP_ENTRY(nsIXPCScriptable)
  NS_INTERFACE_MAP_ENTRY_AMBIGUOUS(nsISupports, nsIXPCScriptable)
NS_INTERFACE_MAP_END_THREADSAFE

NS_IMPL_THREADSAFE_ADDREF(SharedScriptableHelperForJSIID)
NS_IMPL_THREADSAFE_RELEASE(SharedScriptableHelperForJSIID)

// The nsIXPCScriptable map declaration that will generate stubs for us...
#define XPC_MAP_CLASSNAME           SharedScriptableHelperForJSIID
#define XPC_MAP_QUOTED_CLASSNAME   "JSIID"
#define XPC_MAP_FLAGS               nsIXPCScriptable::DONT_ENUM_STATIC_PROPS |\
                                    nsIXPCScriptable::ALLOW_PROP_MODS_DURING_RESOLVE
#include "xpc_map_end.h" /* This will #undef the above */

static nsIXPCScriptable* gSharedScriptableHelperForJSIID;

NS_METHOD GetSharedScriptableHelperForJSIID(PRUint32 language,
                                            nsISupports **helper)
{
    if(language == nsIProgrammingLanguage::JAVASCRIPT)
    {
        NS_IF_ADDREF(gSharedScriptableHelperForJSIID);
        *helper = gSharedScriptableHelperForJSIID;
    }
    else
        *helper = nsnull;
    return NS_OK;
}

/******************************************************/

static JSBool gClassObjectsWereInited = JS_FALSE;

#define NULL_CID \
{ 0x00000000, 0x0000, 0x0000, \
  { 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 } }

NS_DECL_CI_INTERFACE_GETTER(nsJSIID)
NS_IMPL_CLASSINFO(nsJSIID, GetSharedScriptableHelperForJSIID,
                  nsIClassInfo::THREADSAFE, NULL_CID)

NS_DECL_CI_INTERFACE_GETTER(nsJSCID)
NS_IMPL_CLASSINFO(nsJSCID, NULL, nsIClassInfo::THREADSAFE, NULL_CID)

void xpc_InitJSxIDClassObjects()
{
    if(!gClassObjectsWereInited) {
        gSharedScriptableHelperForJSIID = new SharedScriptableHelperForJSIID();
        NS_ADDREF(gSharedScriptableHelperForJSIID);
    }
    gClassObjectsWereInited = JS_TRUE;
}

void xpc_DestroyJSxIDClassObjects()
{
    NS_IF_RELEASE(NS_CLASSINFO_NAME(nsJSIID));
    NS_IF_RELEASE(NS_CLASSINFO_NAME(nsJSCID));
    NS_IF_RELEASE(gSharedScriptableHelperForJSIID);

    gClassObjectsWereInited = JS_FALSE;
}

/***************************************************************************/

NS_INTERFACE_MAP_BEGIN(nsJSIID)
  NS_INTERFACE_MAP_ENTRY(nsIJSID)
  NS_INTERFACE_MAP_ENTRY(nsIJSIID)
  NS_INTERFACE_MAP_ENTRY(nsIXPCScriptable)
  NS_INTERFACE_MAP_ENTRY(nsISecurityCheckedComponent)
  NS_INTERFACE_MAP_ENTRY_AMBIGUOUS(nsISupports, nsIJSID)
  NS_IMPL_QUERY_CLASSINFO(nsJSIID)
NS_INTERFACE_MAP_END_THREADSAFE

NS_IMPL_THREADSAFE_ADDREF(nsJSIID)
NS_IMPL_THREADSAFE_RELEASE(nsJSIID)
NS_IMPL_CI_INTERFACE_GETTER2(nsJSIID, nsIJSID, nsIJSIID)

// The nsIXPCScriptable map declaration that will generate stubs for us...
#define XPC_MAP_CLASSNAME           nsJSIID
#define XPC_MAP_QUOTED_CLASSNAME   "nsJSIID"
#define                             XPC_MAP_WANT_NEWRESOLVE
#define                             XPC_MAP_WANT_ENUMERATE
#define                             XPC_MAP_WANT_HASINSTANCE
#define XPC_MAP_FLAGS               nsIXPCScriptable::DONT_ENUM_STATIC_PROPS |\
                                    nsIXPCScriptable::ALLOW_PROP_MODS_DURING_RESOLVE
#include "xpc_map_end.h" /* This will #undef the above */


nsJSIID::nsJSIID(nsIInterfaceInfo* aInfo)
    : mInfo(aInfo)
{
}

nsJSIID::~nsJSIID() {}

// If mInfo is present we use it and ignore mDetails, else we use mDetails.

NS_IMETHODIMP nsJSIID::GetName(char * *aName)
{
    return mInfo->GetName(aName);    
}

NS_IMETHODIMP nsJSIID::GetNumber(char * *aNumber)
{
    char str[NSID_LENGTH];
    const nsIID* id;
    mInfo->GetIIDShared(&id);
    id->ToProvidedString(str);
    *aNumber = (char*) nsMemory::Clone(str, NSID_LENGTH);
    return *aNumber ? NS_OK : NS_ERROR_OUT_OF_MEMORY;
}        

NS_IMETHODIMP_(const nsID*) nsJSIID::GetID()
{
    const nsIID* id;
    mInfo->GetIIDShared(&id);
    return id;
}

NS_IMETHODIMP nsJSIID::GetValid(bool *aValid)
{
    *aValid = PR_TRUE;
    return NS_OK;
}

NS_IMETHODIMP nsJSIID::Equals(nsIJSID *other, bool *_retval)
{
    if(!_retval)
        return NS_ERROR_NULL_POINTER;

    if(!other)
    {
        *_retval = PR_FALSE;
        return NS_OK;
    }

    mInfo->IsIID(other->GetID(), _retval);
    return NS_OK;
}

NS_IMETHODIMP nsJSIID::Initialize(const char *idString)
{
    return NS_ERROR_FAILURE;
}

NS_IMETHODIMP nsJSIID::ToString(char **_retval)
{
    return mInfo->GetName(_retval);    
}

// static 
nsJSIID* 
nsJSIID::NewID(nsIInterfaceInfo* aInfo)
{
    if(!aInfo)
    {
        NS_ERROR("no info");
        return nsnull;
    }

    bool canScript;
    if(NS_FAILED(aInfo->IsScriptable(&canScript)) || !canScript)
        return nsnull;

    nsJSIID* idObj = new nsJSIID(aInfo);
    NS_IF_ADDREF(idObj);
    return idObj;
}


/* bool resolve (in nsIXPConnectWrappedNative wrapper, in JSContextPtr cx, in JSObjectPtr obj, in jsval id); */
NS_IMETHODIMP
nsJSIID::NewResolve(nsIXPConnectWrappedNative *wrapper,
                    JSContext * cx, JSObject * obj,
                    jsid id, PRUint32 flags,
                    JSObject * *objp, bool *_retval)
{
    XPCCallContext ccx(JS_CALLER, cx);

    AutoMarkingNativeInterfacePtr iface(ccx);

    const nsIID* iid;
    mInfo->GetIIDShared(&iid);

    iface = XPCNativeInterface::GetNewOrUsed(ccx, iid);

    if(!iface)
        return NS_OK;

    XPCNativeMember* member = iface->FindMember(id);
    if(member && member->IsConstant())
    {
        jsval val;
        if(!member->GetConstantValue(ccx, iface, &val))
            return NS_ERROR_OUT_OF_MEMORY;

        *objp = obj;
        *_retval = JS_DefinePropertyById(cx, obj, id, val, nsnull, nsnull,
                                         JSPROP_ENUMERATE | JSPROP_READONLY |
                                         JSPROP_PERMANENT);
    }

    return NS_OK;
}

/* bool enumerate (in nsIXPConnectWrappedNative wrapper, in JSContextPtr cx, in JSObjectPtr obj); */
NS_IMETHODIMP
nsJSIID::Enumerate(nsIXPConnectWrappedNative *wrapper,
                   JSContext * cx, JSObject * obj, bool *_retval)
{
    // In this case, let's just eagerly resolve...

    XPCCallContext ccx(JS_CALLER, cx);

    AutoMarkingNativeInterfacePtr iface(ccx);

    const nsIID* iid;
    mInfo->GetIIDShared(&iid);

    iface = XPCNativeInterface::GetNewOrUsed(ccx, iid);

    if(!iface)
        return NS_OK;

    PRUint16 count = iface->GetMemberCount();
    for(PRUint16 i = 0; i < count; i++)
    {
        XPCNativeMember* member = iface->GetMemberAt(i);
        if(member && member->IsConstant() &&
           !xpc_ForcePropertyResolve(cx, obj, member->GetName()))
        {
            return NS_ERROR_UNEXPECTED;
        }
    }
    return NS_OK;
}

/* bool hasInstance (in nsIXPConnectWrappedNative wrapper, in JSContextPtr cx, in JSObjectPtr obj, in jsval val, out bool bp); */
NS_IMETHODIMP
nsJSIID::HasInstance(nsIXPConnectWrappedNative *wrapper,
                     JSContext * cx, JSObject * obj,
                     const jsval &val, bool *bp, bool *_retval)
{
    *bp = JS_FALSE;
    nsresult rv = NS_OK;

    if(!JSVAL_IS_PRIMITIVE(val))
    {
        // we have a JSObject
        JSObject* obj = JSVAL_TO_OBJECT(val);

        NS_ASSERTION(obj, "when is an object not an object?");

        // is this really a native xpcom object with a wrapper?
        const nsIID* iid;
        mInfo->GetIIDShared(&iid);

        if(IS_SLIM_WRAPPER(obj))
        {
            XPCWrappedNativeProto* proto = GetSlimWrapperProto(obj);
            if(proto->GetSet()->HasInterfaceWithAncestor(iid))
            {
                *bp = JS_TRUE;
                return NS_OK;
            }

#ifdef DEBUG_slimwrappers
            char foo[NSID_LENGTH];
            iid->ToProvidedString(foo);
            SLIM_LOG_WILL_MORPH_FOR_PROP(cx, obj, foo);
#endif
            if(!MorphSlimWrapper(cx, obj))
                return NS_ERROR_FAILURE;
        }

        if (mozilla::dom::binding::instanceIsProxy(obj))
        {
            nsISupports *identity =
                static_cast<nsISupports*>(js::GetProxyPrivate(obj).toPrivate());
            nsCOMPtr<nsIClassInfo> ci = do_QueryInterface(identity);

            XPCCallContext ccx(JS_CALLER, cx);

            AutoMarkingNativeSetPtr set(ccx);
            set = XPCNativeSet::GetNewOrUsed(ccx, ci);
            if(!set)
                return NS_ERROR_FAILURE;
            *bp = set->HasInterfaceWithAncestor(iid);
            return NS_OK;
     }

        XPCWrappedNative* other_wrapper =
           XPCWrappedNative::GetWrappedNativeOfJSObject(cx, obj);

        if(!other_wrapper)
            return NS_OK;

        // We'll trust the interface set of the wrapper if this is known
        // to be an interface that the objects *expects* to be able to
        // handle.
        if(other_wrapper->HasInterfaceNoQI(*iid))
        {
            *bp = JS_TRUE;
            return NS_OK;
        }

        // Otherwise, we'll end up Querying the native object to be sure.
        XPCCallContext ccx(JS_CALLER, cx);

        AutoMarkingNativeInterfacePtr iface(ccx);
        iface = XPCNativeInterface::GetNewOrUsed(ccx, iid);

        nsresult findResult = NS_OK;
        if(iface && other_wrapper->FindTearOff(ccx, iface, JS_FALSE, &findResult))
            *bp = JS_TRUE;
        if (NS_FAILED(findResult) && findResult != NS_ERROR_NO_INTERFACE)
            rv = findResult;
    }
    return rv;
}

/* string canCreateWrapper (in nsIIDPtr iid); */
NS_IMETHODIMP
nsJSIID::CanCreateWrapper(const nsIID * iid, char **_retval)
{
    // We let anyone do this...
    *_retval = xpc_CloneAllAccess();
    return NS_OK;
}

/* string canCallMethod (in nsIIDPtr iid, in wstring methodName); */
NS_IMETHODIMP
nsJSIID::CanCallMethod(const nsIID * iid, const PRUnichar *methodName, char **_retval)
{
    static const char* allowed[] = {"equals", "toString", nsnull};

    *_retval = xpc_CheckAccessList(methodName, allowed);
    return NS_OK;
}

/* string canGetProperty (in nsIIDPtr iid, in wstring propertyName); */
NS_IMETHODIMP
nsJSIID::CanGetProperty(const nsIID * iid, const PRUnichar *propertyName, char **_retval)
{
    static const char* allowed[] = {"name", "number", "valid", nsnull};
    *_retval = xpc_CheckAccessList(propertyName, allowed);
    return NS_OK;
}

/* string canSetProperty (in nsIIDPtr iid, in wstring propertyName); */
NS_IMETHODIMP
nsJSIID::CanSetProperty(const nsIID * iid, const PRUnichar *propertyName, char **_retval)
{
    // If you have to ask, then the answer is NO
    *_retval = nsnull;
    return NS_OK;
}

/***************************************************************************/

NS_INTERFACE_MAP_BEGIN(nsJSCID)
  NS_INTERFACE_MAP_ENTRY(nsIJSID)
  NS_INTERFACE_MAP_ENTRY(nsIJSCID)
  NS_INTERFACE_MAP_ENTRY(nsIXPCScriptable)
  NS_INTERFACE_MAP_ENTRY_AMBIGUOUS(nsISupports, nsIJSID)
  NS_IMPL_QUERY_CLASSINFO(nsJSCID)
NS_INTERFACE_MAP_END_THREADSAFE

NS_IMPL_THREADSAFE_ADDREF(nsJSCID)
NS_IMPL_THREADSAFE_RELEASE(nsJSCID)
NS_IMPL_CI_INTERFACE_GETTER2(nsJSCID, nsIJSID, nsIJSCID)

// The nsIXPCScriptable map declaration that will generate stubs for us...
#define XPC_MAP_CLASSNAME           nsJSCID
#define XPC_MAP_QUOTED_CLASSNAME   "nsJSCID"
#define                             XPC_MAP_WANT_CONSTRUCT
#define                             XPC_MAP_WANT_HASINSTANCE
#define XPC_MAP_FLAGS               0
#include "xpc_map_end.h" /* This will #undef the above */

nsJSCID::nsJSCID()  {}
nsJSCID::~nsJSCID() {}

NS_IMETHODIMP nsJSCID::GetName(char * *aName)
    {ResolveName(); return mDetails.GetName(aName);}

NS_IMETHODIMP nsJSCID::GetNumber(char * *aNumber)
    {return mDetails.GetNumber(aNumber);}

NS_IMETHODIMP_(const nsID*) nsJSCID::GetID()
    {return &mDetails.ID();}

NS_IMETHODIMP nsJSCID::GetValid(bool *aValid)
    {return mDetails.GetValid(aValid);}

NS_IMETHODIMP nsJSCID::Equals(nsIJSID *other, bool *_retval)
    {return mDetails.Equals(other, _retval);}

NS_IMETHODIMP nsJSCID::Initialize(const char *idString)
    {return mDetails.Initialize(idString);}

NS_IMETHODIMP nsJSCID::ToString(char **_retval)
    {ResolveName(); return mDetails.ToString(_retval);}

void
nsJSCID::ResolveName()
{
    if(!mDetails.NameIsSet())
        mDetails.SetNameToNoString();
}

//static
nsJSCID*
nsJSCID::NewID(const char* str)
{
    if(!str)
    {
        NS_ERROR("no string");
        return nsnull;
    }

    nsJSCID* idObj = new nsJSCID();
    if(idObj)
    {
        bool success = false;
        NS_ADDREF(idObj);

        if(str[0] == '{')
        {
            if(NS_SUCCEEDED(idObj->Initialize(str)))
                success = PR_TRUE;
        }
        else
        {
            nsCOMPtr<nsIComponentRegistrar> registrar;
            NS_GetComponentRegistrar(getter_AddRefs(registrar));
            if (registrar)
            {
                nsCID *cid;
                if(NS_SUCCEEDED(registrar->ContractIDToCID(str, &cid)))
                {
                    success = idObj->mDetails.InitWithName(*cid, str);
                    nsMemory::Free(cid);
                }
            }
        }
        if(!success)
            NS_RELEASE(idObj);
    }
    return idObj;
}

static const nsID*
GetIIDArg(PRUint32 argc, jsval* argv, JSContext* cx)
{
    const nsID* iid;

    // If an IID was passed in then use it
    if(argc)
    {
        JSObject* iidobj;
        jsval val = *argv;
        if(JSVAL_IS_PRIMITIVE(val) ||
           !(iidobj = JSVAL_TO_OBJECT(val)) ||
           !(iid = xpc_JSObjectToID(cx, iidobj)))
        {
            return nsnull;
        }
    }
    else
        iid = &NS_GET_IID(nsISupports);

    return iid;
}
 
/* nsISupports createInstance (); */
NS_IMETHODIMP
nsJSCID::CreateInstance(nsISupports **_retval)
{
    if(!mDetails.IsValid())
        return NS_ERROR_XPC_BAD_CID;

    nsXPConnect* xpc = nsXPConnect::GetXPConnect();
    if(!xpc)
        return NS_ERROR_UNEXPECTED;

    nsAXPCNativeCallContext *ccxp = nsnull;
    xpc->GetCurrentNativeCallContext(&ccxp);
    if(!ccxp)
        return NS_ERROR_UNEXPECTED;

    PRUint32 argc;
    jsval * argv;
    jsval * vp;
    JSContext* cx;
    JSObject* obj;

    ccxp->GetJSContext(&cx);
    ccxp->GetArgc(&argc);
    ccxp->GetArgvPtr(&argv);
    ccxp->GetRetValPtr(&vp);

    nsCOMPtr<nsIXPConnectWrappedNative> wrapper;
    ccxp->GetCalleeWrapper(getter_AddRefs(wrapper));
    wrapper->GetJSObject(&obj);

    // Do the security check if necessary

    XPCContext* xpcc = XPCContext::GetXPCContext(cx);

    nsIXPCSecurityManager* sm;
    sm = xpcc->GetAppropriateSecurityManager(
                        nsIXPCSecurityManager::HOOK_CREATE_INSTANCE);
    if(sm && NS_FAILED(sm->CanCreateInstance(cx, mDetails.ID())))
    {
        NS_ERROR("how are we not being called from chrome here?");
        return NS_OK;
    }

    // If an IID was passed in then use it
    const nsID* iid = GetIIDArg(argc, argv, cx);
    if (!iid)
        return NS_ERROR_XPC_BAD_IID;

    nsCOMPtr<nsIComponentManager> compMgr;
    nsresult rv = NS_GetComponentManager(getter_AddRefs(compMgr));
    if (NS_FAILED(rv))
        return NS_ERROR_UNEXPECTED;

    nsCOMPtr<nsISupports> inst;
    rv = compMgr->CreateInstance(mDetails.ID(), nsnull, *iid, getter_AddRefs(inst));
    NS_ASSERTION(NS_FAILED(rv) || inst, "component manager returned success, but instance is null!");

    if(NS_FAILED(rv) || !inst)
        return NS_ERROR_XPC_CI_RETURNED_FAILURE;

    rv = xpc->WrapNativeToJSVal(cx, obj, inst, nsnull, iid, PR_TRUE, vp, nsnull);
    if(NS_FAILED(rv) || JSVAL_IS_PRIMITIVE(*vp))
        return NS_ERROR_XPC_CANT_CREATE_WN;
    ccxp->SetReturnValueWasSet(JS_TRUE);
    return NS_OK;
}

/* nsISupports getService (); */
NS_IMETHODIMP
nsJSCID::GetService(nsISupports **_retval)
{
    if(!mDetails.IsValid())
        return NS_ERROR_XPC_BAD_CID;

    nsXPConnect* xpc = nsXPConnect::GetXPConnect();
    if(!xpc)
        return NS_ERROR_UNEXPECTED;

    nsAXPCNativeCallContext *ccxp = nsnull;
    xpc->GetCurrentNativeCallContext(&ccxp);
    if(!ccxp)
        return NS_ERROR_UNEXPECTED;

    PRUint32 argc;
    jsval * argv;
    jsval * vp;
    JSContext* cx;
    JSObject* obj;

    ccxp->GetJSContext(&cx);
    ccxp->GetArgc(&argc);
    ccxp->GetArgvPtr(&argv);
    ccxp->GetRetValPtr(&vp);

    nsCOMPtr<nsIXPConnectWrappedNative> wrapper;
    ccxp->GetCalleeWrapper(getter_AddRefs(wrapper));
    wrapper->GetJSObject(&obj);

    // Do the security check if necessary

    XPCContext* xpcc = XPCContext::GetXPCContext(cx);

    nsIXPCSecurityManager* sm;
    sm = xpcc->GetAppropriateSecurityManager(
                        nsIXPCSecurityManager::HOOK_GET_SERVICE);
    if(sm && NS_FAILED(sm->CanCreateInstance(cx, mDetails.ID())))
    {
        NS_ASSERTION(JS_IsExceptionPending(cx),
                     "security manager vetoed GetService without setting exception");
        return NS_OK;
    }

    // If an IID was passed in then use it
    const nsID* iid = GetIIDArg(argc, argv, cx);
    if (!iid)
        return NS_ERROR_XPC_BAD_IID;

    nsCOMPtr<nsIServiceManager> svcMgr;
    nsresult rv = NS_GetServiceManager(getter_AddRefs(svcMgr));
    if (NS_FAILED(rv))
        return rv;

    nsCOMPtr<nsISupports> srvc;
    rv = svcMgr->GetService(mDetails.ID(), *iid, getter_AddRefs(srvc));
    NS_ASSERTION(NS_FAILED(rv) || srvc, "service manager returned success, but service is null!");
    if(NS_FAILED(rv) || !srvc)
        return NS_ERROR_XPC_GS_RETURNED_FAILURE;

    JSObject* instJSObj;
    nsCOMPtr<nsIXPConnectJSObjectHolder> holder;
    rv = xpc->WrapNative(cx, obj, srvc, *iid, getter_AddRefs(holder));
    if(NS_FAILED(rv) || !holder || NS_FAILED(holder->GetJSObject(&instJSObj)))
        return NS_ERROR_XPC_CANT_CREATE_WN;

    *vp = OBJECT_TO_JSVAL(instJSObj);
    ccxp->SetReturnValueWasSet(JS_TRUE);
    return NS_OK;
}

/* bool construct (in nsIXPConnectWrappedNative wrapper, in JSContextPtr cx, in JSObjectPtr obj, in PRUint32 argc, in JSValPtr argv, in JSValPtr vp); */
NS_IMETHODIMP
nsJSCID::Construct(nsIXPConnectWrappedNative *wrapper,
                   JSContext * cx, JSObject * obj,
                   PRUint32 argc, jsval * argv, jsval * vp,
                   bool *_retval)
{
    XPCJSRuntime* rt = nsXPConnect::GetRuntimeInstance();
    if(!rt)
        return NS_ERROR_FAILURE;

    // 'push' a call context and call on it
    XPCCallContext ccx(JS_CALLER, cx, obj, nsnull,
                       rt->GetStringID(XPCJSRuntime::IDX_CREATE_INSTANCE),
                       argc, argv, vp);

    *_retval = XPCWrappedNative::CallMethod(ccx);
    return NS_OK;
}

/* bool hasInstance (in nsIXPConnectWrappedNative wrapper, in JSContextPtr cx, in JSObjectPtr obj, in jsval val, out bool bp); */
NS_IMETHODIMP
nsJSCID::HasInstance(nsIXPConnectWrappedNative *wrapper,
                     JSContext * cx, JSObject * obj,
                     const jsval &val, bool *bp, bool *_retval)
{
    *bp = JS_FALSE;
    nsresult rv = NS_OK;

    if(!JSVAL_IS_PRIMITIVE(val))
    {
        // we have a JSObject
        JSObject* obj = JSVAL_TO_OBJECT(val);

        NS_ASSERTION(obj, "when is an object not an object?");

        // is this really a native xpcom object with a wrapper?
        JSObject* obj2;
        XPCWrappedNative* other_wrapper =
           XPCWrappedNative::GetWrappedNativeOfJSObject(cx, obj, nsnull, &obj2);

        if(!other_wrapper && !obj2)
            return NS_OK;

        nsIClassInfo* ci = other_wrapper ?
                           other_wrapper->GetClassInfo() :
                           GetSlimWrapperProto(obj2)->GetClassInfo();

        // We consider CID equality to be the thing that matters here.
        // This is perhaps debatable.
        if(ci)
        {
            nsID cid;
            if(NS_SUCCEEDED(ci->GetClassIDNoAlloc(&cid)))
                *bp = cid.Equals(mDetails.ID());
        }
    }
    return rv;
}

/***************************************************************************/
// additional utilities...

JSObject *
xpc_NewIDObject(JSContext *cx, JSObject* jsobj, const nsID& aID)
{
    JSObject *obj = nsnull;

    nsCOMPtr<nsIJSID> iid =
            dont_AddRef(static_cast<nsIJSID*>(nsJSID::NewID(aID)));
    if(iid)
    {
        nsXPConnect* xpc = nsXPConnect::GetXPConnect();
        if(xpc)
        {
            nsCOMPtr<nsIXPConnectJSObjectHolder> holder;
            nsresult rv = xpc->WrapNative(cx, jsobj,
                                          static_cast<nsISupports*>(iid),
                                          NS_GET_IID(nsIJSID),
                                          getter_AddRefs(holder));
            if(NS_SUCCEEDED(rv) && holder)
            {
                holder->GetJSObject(&obj);
            }
        }
    }
    return obj;
}

// note: returned pointer is only valid while |obj| remains alive!
const nsID*
xpc_JSObjectToID(JSContext *cx, JSObject* obj)
{
    if(!cx || !obj)
        return nsnull;

    // NOTE: this call does NOT addref
    XPCWrappedNative* wrapper =
        XPCWrappedNative::GetWrappedNativeOfJSObject(cx, obj);
    if(wrapper &&
       (wrapper->HasInterfaceNoQI(NS_GET_IID(nsIJSID))  ||
        wrapper->HasInterfaceNoQI(NS_GET_IID(nsIJSIID)) ||
        wrapper->HasInterfaceNoQI(NS_GET_IID(nsIJSCID))))
    {
        return ((nsIJSID*)wrapper->GetIdentityObject())->GetID();
    }
    return nsnull;
}

JSBool
xpc_JSObjectIsID(JSContext *cx, JSObject* obj)
{
    NS_ASSERTION(cx && obj, "bad param");
    // NOTE: this call does NOT addref
    XPCWrappedNative* wrapper =
        XPCWrappedNative::GetWrappedNativeOfJSObject(cx, obj);
    return wrapper &&
           (wrapper->HasInterfaceNoQI(NS_GET_IID(nsIJSID))  ||
            wrapper->HasInterfaceNoQI(NS_GET_IID(nsIJSIID)) ||
            wrapper->HasInterfaceNoQI(NS_GET_IID(nsIJSCID)));
}


