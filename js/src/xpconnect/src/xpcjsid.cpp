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
        PR_Free(mNumber);
    if(mName && mName != gNoString)
        PR_Free(mName);
}

void nsJSID::Reset()
{
    mID = GetInvalidIID();

    if(mNumber && mNumber != gNoString)
        PR_Free(mNumber);
    if(mName && mName != gNoString)
        PR_Free(mName);

    mNumber = mName = nsnull;
}

PRBool
nsJSID::SetName(const char* name)
{
    NS_ASSERTION(!mName || mName == gNoString ,"name already set");
    NS_ASSERTION(name,"null name");
    int len = strlen(name)+1;
    mName = (char*)PR_Malloc(len);
    if(!mName)
        return PR_FALSE;
    memcpy(mName, name, len);
    return PR_TRUE;
}

NS_IMETHODIMP
nsJSID::GetName(char * *aName)
{
    if(!aName)
        return NS_ERROR_NULL_POINTER;

    if(!NameIsSet())
        SetNameToNoString();
    NS_ASSERTION(mName, "name not set");
    *aName = (char*) nsMemory::Clone(mName, strlen(mName)+1);
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

    *aNumber = (char*) nsMemory::Clone(mNumber, strlen(mNumber)+1);
    return *aNumber ? NS_OK : NS_ERROR_OUT_OF_MEMORY;
}

NS_IMETHODIMP
nsJSID::GetId(nsID* *aId)
{
    if(!aId)
        return NS_ERROR_NULL_POINTER;

    *aId = (nsID*) nsMemory::Clone(&mID, sizeof(nsID));
    return *aId ? NS_OK : NS_ERROR_OUT_OF_MEMORY;
}

NS_IMETHODIMP
nsJSID::GetValid(PRBool *aValid)
{
    if(!aValid)
        return NS_ERROR_NULL_POINTER;

    *aValid = IsValid();
    return NS_OK;
}

NS_IMETHODIMP
nsJSID::Equals(nsIJSID *other, PRBool *_retval)
{
    if(!_retval)
        return NS_ERROR_NULL_POINTER;

    *_retval = PR_FALSE;

    if(!other || mID.Equals(GetInvalidIID()))
        return NS_OK;

    nsID* otherID;
    if(NS_SUCCEEDED(other->GetId(&otherID)))
    {
        *_retval = mID.Equals(*otherID);
        nsMemory::Free(otherID);
    }
    return NS_OK;
}

NS_IMETHODIMP
nsJSID::Initialize(const char *idString)
{
    if(!idString)
        return NS_ERROR_NULL_POINTER;

    PRBool success = PR_FALSE;

    if(strlen(idString) && mID.Equals(GetInvalidIID()))
    {
        Reset();

        if(idString[0] == '{')
        {
            nsID id;
            if(id.Parse((char*)idString))
            {
                mID = id;
                success = PR_TRUE;
            }
        }
    }
    return success ? NS_OK : NS_ERROR_FAILURE;
}

PRBool
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
    if(mName != gNoString)
    {
        char* str;
        if(NS_SUCCEEDED(GetName(&str)))
        {
            if(mName != gNoString)
            {
                *_retval = str;
                return NS_OK;
            }
            else
                nsMemory::Free(str);
        }
    }
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
        NS_ASSERTION(0,"no string");
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

NS_DECL_CI_INTERFACE_GETTER(nsJSIID)
// Can't make this static. http://bugzilla.mozilla.org/show_bug.cgi?id=81436
nsIClassInfo* NS_CLASSINFO_NAME(nsJSIID);
static const nsModuleComponentInfo CI_nsJSIID =
    {"JSIID",
     {0x26ecb8d0, 0x35c9, 0x11d5, { 0x90, 0xb2, 0x0, 0x10, 0xa4, 0xe7, 0x3d, 0x9a }},
     nsnull, nsnull, nsnull,nsnull, nsnull,
     NS_CI_INTERFACE_GETTER_NAME(nsJSIID),
     GetSharedScriptableHelperForJSIID,
     &NS_CLASSINFO_NAME(nsJSIID), 0};

NS_DECL_CI_INTERFACE_GETTER(nsJSCID)
// Can't make this static. http://bugzilla.mozilla.org/show_bug.cgi?id=81436
nsIClassInfo* NS_CLASSINFO_NAME(nsJSCID);
static const nsModuleComponentInfo CI_nsJSCID =
    {"JSCID",
     {0x9255b5b0, 0x35cf, 0x11d5, { 0x90, 0xb2, 0x0, 0x10, 0xa4, 0xe7, 0x3d, 0x9a }},
     nsnull, nsnull, nsnull,nsnull, nsnull,
     NS_CI_INTERFACE_GETTER_NAME(nsJSCID), nsnull,
     &NS_CLASSINFO_NAME(nsJSCID), 0};

JSBool xpc_InitJSxIDClassObjects()
{
    if(gClassObjectsWereInited)
        return JS_TRUE;

    nsresult rv = NS_OK;

    if(!NS_CLASSINFO_NAME(nsJSIID))
    {
        nsCOMPtr<nsIGenericFactory> factory;
        rv = NS_NewGenericFactory(getter_AddRefs(factory), &CI_nsJSIID);
        if(NS_FAILED(rv))
            goto return_failure;
        rv = factory->QueryInterface(NS_GET_IID(nsIClassInfo),
                                     (void**)&NS_CLASSINFO_NAME(nsJSIID));
        if(NS_FAILED(rv))
            goto return_failure;
    }

    if(!NS_CLASSINFO_NAME(nsJSCID))
    {
        nsCOMPtr<nsIGenericFactory> factory;
        rv = NS_NewGenericFactory(getter_AddRefs(factory), &CI_nsJSCID);
        if(NS_FAILED(rv))
            goto return_failure;
        rv = factory->QueryInterface(NS_GET_IID(nsIClassInfo),
                                     (void**)&NS_CLASSINFO_NAME(nsJSCID));
        if(NS_FAILED(rv))
            goto return_failure;
    }

    gSharedScriptableHelperForJSIID = new SharedScriptableHelperForJSIID();
    if(!gSharedScriptableHelperForJSIID)
        goto return_failure;
    NS_ADDREF(gSharedScriptableHelperForJSIID);

    gClassObjectsWereInited = JS_TRUE;
    return JS_TRUE;
return_failure:
    return JS_FALSE;
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
#ifdef XPC_USE_SECURITY_CHECKED_COMPONENT
  NS_INTERFACE_MAP_ENTRY(nsISecurityCheckedComponent)
#endif
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
    const nsIID* id;
    mInfo->GetIIDShared(&id);
    char* str = id->ToString();
    if(!str)
        return NS_ERROR_OUT_OF_MEMORY;
    *aNumber = (char*) nsMemory::Clone(str, strlen(str)+1);
    PR_Free(str);
    return *aNumber ? NS_OK : NS_ERROR_OUT_OF_MEMORY;
}        

NS_IMETHODIMP nsJSIID::GetId(nsID* *aId)
{
    return mInfo->GetInterfaceIID((nsIID**)aId);
}

NS_IMETHODIMP nsJSIID::GetValid(PRBool *aValid)
{
    *aValid = PR_TRUE;
    return NS_OK;
}

NS_IMETHODIMP nsJSIID::Equals(nsIJSID *other, PRBool *_retval)
{
    nsID* otherID;
    if(NS_SUCCEEDED(other->GetId(&otherID)))
    {
        mInfo->IsIID((nsIID*)otherID, _retval);
        nsMemory::Free(otherID);
    }
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

    PRBool canScript;
    if(NS_FAILED(aInfo->IsScriptable(&canScript)) || !canScript)
        return nsnull;

    nsJSIID* idObj = new nsJSIID(aInfo);
    NS_IF_ADDREF(idObj);
    return idObj;
}


/* PRBool resolve (in nsIXPConnectWrappedNative wrapper, in JSContextPtr cx, in JSObjectPtr obj, in JSVal id); */
NS_IMETHODIMP
nsJSIID::NewResolve(nsIXPConnectWrappedNative *wrapper,
                    JSContext * cx, JSObject * obj,
                    jsval id, PRUint32 flags,
                    JSObject * *objp, PRBool *_retval)
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
        if(!member->GetValue(ccx, iface, &val))
            return NS_ERROR_OUT_OF_MEMORY;

        jsid idid;
        if(!JS_ValueToId(cx, id, &idid))
            return NS_ERROR_OUT_OF_MEMORY;

        *objp = obj;
        *_retval = OBJ_DEFINE_PROPERTY(cx, obj, idid, val,
                                       nsnull, nsnull,
                                       JSPROP_ENUMERATE |
                                       JSPROP_READONLY |
                                       JSPROP_PERMANENT,
                                       nsnull);
    }

    return NS_OK;
}

/* PRBool enumerate (in nsIXPConnectWrappedNative wrapper, in JSContextPtr cx, in JSObjectPtr obj); */
NS_IMETHODIMP
nsJSIID::Enumerate(nsIXPConnectWrappedNative *wrapper,
                   JSContext * cx, JSObject * obj, PRBool *_retval)
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

/* PRBool hasInstance (in nsIXPConnectWrappedNative wrapper, in JSContextPtr cx, in JSObjectPtr obj, in JSVal val, out PRBool bp); */
NS_IMETHODIMP
nsJSIID::HasInstance(nsIXPConnectWrappedNative *wrapper,
                     JSContext * cx, JSObject * obj,
                     jsval val, PRBool *bp, PRBool *_retval)
{
    *bp = JS_FALSE;
    nsresult rv = NS_OK;

    if(!JSVAL_IS_PRIMITIVE(val))
    {
        // we have a JSObject
        JSObject* obj = JSVAL_TO_OBJECT(val);

        NS_ASSERTION(obj, "when is an object not an object?");

        // is this really a native xpcom object with a wrapper?
        XPCWrappedNative* other_wrapper =
           XPCWrappedNative::GetWrappedNativeOfJSObject(cx, obj);

        if(!other_wrapper)
            return NS_OK;

        const nsIID* iid;
        mInfo->GetIIDShared(&iid);

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

        if(iface && other_wrapper->FindTearOff(ccx, iface))
            *bp = JS_TRUE;
    }
    return rv;
}

#ifdef XPC_USE_SECURITY_CHECKED_COMPONENT
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
#endif

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

NS_IMETHODIMP nsJSCID::GetId(nsID* *aId)
    {return mDetails.GetId(aId);}

NS_IMETHODIMP nsJSCID::GetValid(PRBool *aValid)
    {return mDetails.GetValid(aValid);}

NS_IMETHODIMP nsJSCID::Equals(nsIJSID *other, PRBool *_retval)
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
        NS_ASSERTION(0,"no string");
        return nsnull;
    }

    nsJSCID* idObj = new nsJSCID();
    if(idObj)
    {
        PRBool success = PR_FALSE;
        NS_ADDREF(idObj);

        if(str[0] == '{')
        {
            if(NS_SUCCEEDED(idObj->Initialize(str)))
                success = PR_TRUE;
        }
        else
        {
            nsCID cid;
            if(NS_SUCCEEDED(nsComponentManager::ContractIDToClassID(str, &cid)))
            {
                success = idObj->mDetails.InitWithName(cid, str);
            }
        }
        if(!success)
            NS_RELEASE(idObj);
    }
    return idObj;
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

    nsCOMPtr<nsIXPCNativeCallContext> ccxp;
    xpc->GetCurrentNativeCallContext(getter_AddRefs(ccxp));
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

    XPCContext* xpcc = nsXPConnect::GetContext(cx);

    nsIXPCSecurityManager* sm;
    sm = xpcc->GetAppropriateSecurityManager(
                        nsIXPCSecurityManager::HOOK_CREATE_INSTANCE);
    if(sm && NS_FAILED(sm->CanCreateInstance(cx, *mDetails.GetID())))
    {
        // the security manager vetoed. It should have set an exception.
        ccxp->SetExceptionWasThrown(JS_TRUE);
        return NS_OK;
    }

    nsID iid;

    // If an IID was passed in then use it
    if(argc)
    {
        JSObject* iidobj;
        jsval val = *argv;
        nsID* piid = nsnull;
        if(JSVAL_IS_PRIMITIVE(val) ||
           !(iidobj = JSVAL_TO_OBJECT(val)) ||
           !(piid = xpc_JSObjectToID(cx, iidobj)))
        {
            return NS_ERROR_XPC_BAD_IID;
        }
        iid = *piid;
        nsMemory::Free(piid);
    }
    else
        iid = NS_GET_IID(nsISupports);

    nsCOMPtr<nsISupports> inst;
    nsresult rv;

    rv = nsComponentManager::CreateInstance(*mDetails.GetID(), nsnull, iid,
                                            (void**) getter_AddRefs(inst));
    NS_ASSERTION(NS_FAILED(rv) || inst, "component manager returned success, but instance is null!");

    if(NS_FAILED(rv) || !inst)
        return NS_ERROR_XPC_CI_RETURNED_FAILURE;

    JSObject* instJSObj;
    nsCOMPtr<nsIXPConnectJSObjectHolder> holder;
    rv = xpc->WrapNative(cx, obj, inst, iid, getter_AddRefs(holder));
    if(NS_FAILED(rv) || !holder || NS_FAILED(holder->GetJSObject(&instJSObj)))
        return NS_ERROR_XPC_CANT_CREATE_WN;

    *vp = OBJECT_TO_JSVAL(instJSObj);
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

    nsCOMPtr<nsIXPCNativeCallContext> ccxp;
    xpc->GetCurrentNativeCallContext(getter_AddRefs(ccxp));
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

    XPCContext* xpcc = nsXPConnect::GetContext(cx);

    nsIXPCSecurityManager* sm;
    sm = xpcc->GetAppropriateSecurityManager(
                        nsIXPCSecurityManager::HOOK_GET_SERVICE);
    if(sm && NS_FAILED(sm->CanCreateInstance(cx, *mDetails.GetID())))
    {
        // the security manager vetoed. It should have set an exception.
        ccxp->SetExceptionWasThrown(JS_TRUE);
        return NS_OK;
    }

    nsID iid;

    // If an IID was passed in then use it
    if(argc)
    {
        JSObject* iidobj;
        jsval val = *argv;
        nsID* piid = nsnull;
        if(JSVAL_IS_PRIMITIVE(val) ||
           !(iidobj = JSVAL_TO_OBJECT(val)) ||
           !(piid = xpc_JSObjectToID(cx, iidobj)))
        {
            return NS_ERROR_XPC_BAD_IID;
        }
        iid = *piid;
        nsMemory::Free(piid);
    }
    else
        iid = NS_GET_IID(nsISupports);

    nsCOMPtr<nsISupports> srvc;
    nsresult rv;

    rv = nsServiceManager::GetService(*mDetails.GetID(), iid,
                                      getter_AddRefs(srvc), nsnull);
    NS_ASSERTION(NS_FAILED(rv) || srvc, "service manager returned success, but service is null!");
    if(NS_FAILED(rv) || !srvc)
        return NS_ERROR_XPC_GS_RETURNED_FAILURE;

    JSObject* instJSObj;
    nsCOMPtr<nsIXPConnectJSObjectHolder> holder;
    rv = xpc->WrapNative(cx, obj, srvc, iid, getter_AddRefs(holder));
    if(NS_FAILED(rv) || !holder || NS_FAILED(holder->GetJSObject(&instJSObj)))
        return NS_ERROR_XPC_CANT_CREATE_WN;

    *vp = OBJECT_TO_JSVAL(instJSObj);
    ccxp->SetReturnValueWasSet(JS_TRUE);
    return NS_OK;
}

/* PRBool construct (in nsIXPConnectWrappedNative wrapper, in JSContextPtr cx, in JSObjectPtr obj, in PRUint32 argc, in JSValPtr argv, in JSValPtr vp); */
NS_IMETHODIMP
nsJSCID::Construct(nsIXPConnectWrappedNative *wrapper,
                   JSContext * cx, JSObject * obj,
                   PRUint32 argc, jsval * argv, jsval * vp,
                   PRBool *_retval)
{
    XPCJSRuntime* rt = nsXPConnect::GetRuntime();
    if(!rt)
        return NS_ERROR_FAILURE;

    // 'push' a call context and call on it
    XPCCallContext ccx(JS_CALLER, cx, obj, nsnull,
                       rt->GetStringJSVal(XPCJSRuntime::IDX_CREATE_INSTANCE),
                       argc, argv, vp);

    *_retval = XPCWrappedNative::CallMethod(ccx);
    return NS_OK;
}

/* PRBool hasInstance (in nsIXPConnectWrappedNative wrapper, in JSContextPtr cx, in JSObjectPtr obj, in JSVal val, out PRBool bp); */
NS_IMETHODIMP
nsJSCID::HasInstance(nsIXPConnectWrappedNative *wrapper,
                     JSContext * cx, JSObject * obj,
                     jsval val, PRBool *bp, PRBool *_retval)
{
    *bp = JS_FALSE;
    nsresult rv = NS_OK;

    if(!JSVAL_IS_PRIMITIVE(val))
    {
        // we have a JSObject
        JSObject* obj = JSVAL_TO_OBJECT(val);

        NS_ASSERTION(obj, "when is an object not an object?");

        // is this really a native xpcom object with a wrapper?
        XPCWrappedNative* other_wrapper =
           XPCWrappedNative::GetWrappedNativeOfJSObject(cx, obj);

        if(!other_wrapper)
            return NS_OK;

        // We consider CID equality to be the thing that matters here.
        // This is perhaps debatable.
        nsIClassInfo* ci = other_wrapper->GetClassInfo();
        if(ci)
        {
            nsID cid;
            if(NS_SUCCEEDED(ci->GetClassIDNoAlloc(&cid)))
                *bp = cid.Equals(*mDetails.GetID());
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

    char* idString = aID.ToString();
    if(idString)
    {
        nsCOMPtr<nsIJSID> iid =
            dont_AddRef(NS_STATIC_CAST(nsIJSID*, nsJSID::NewID(idString)));
        PR_Free(idString);
        if(iid)
        {
            nsXPConnect* xpc = nsXPConnect::GetXPConnect();
            if(xpc)
            {
                nsCOMPtr<nsIXPConnectJSObjectHolder> holder;
                nsresult rv = xpc->WrapNative(cx, jsobj,
                                              NS_STATIC_CAST(nsISupports*,iid),
                                              NS_GET_IID(nsIJSID),
                                              getter_AddRefs(holder));
                if(NS_SUCCEEDED(rv) && holder)
                {
                    holder->GetJSObject(&obj);
                }
            }
        }
    }
    return obj;
}

nsID*
xpc_JSObjectToID(JSContext *cx, JSObject* obj)
{
    nsID* id = nsnull;
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
        ((nsIJSID*)wrapper->GetIdentityObject())->GetId(&id);
    }
    return id;
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


