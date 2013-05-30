/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* vim: set ts=8 sts=4 et sw=4 tw=99: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/* An xpcom implementation of the JavaScript nsIID and nsCID objects. */

#include "xpcprivate.h"
#include "mozilla/dom/DOMJSClass.h"
#include "mozilla/dom/BindingUtils.h"
#include "mozilla/Attributes.h"
#include "XPCWrapper.h"

using namespace mozilla::dom;
using namespace JS;

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
    if (mNumber && mNumber != gNoString)
        NS_Free(mNumber);
    if (mName && mName != gNoString)
        NS_Free(mName);
}

void nsJSID::Reset()
{
    mID = GetInvalidIID();

    if (mNumber && mNumber != gNoString)
        NS_Free(mNumber);
    if (mName && mName != gNoString)
        NS_Free(mName);

    mNumber = mName = nullptr;
}

bool
nsJSID::SetName(const char* name)
{
    NS_ASSERTION(!mName || mName == gNoString ,"name already set");
    NS_ASSERTION(name,"null name");
    mName = NS_strdup(name);
    return mName ? true : false;
}

NS_IMETHODIMP
nsJSID::GetName(char * *aName)
{
    if (!aName)
        return NS_ERROR_NULL_POINTER;

    if (!NameIsSet())
        SetNameToNoString();
    NS_ASSERTION(mName, "name not set");
    *aName = NS_strdup(mName);
    return *aName ? NS_OK : NS_ERROR_OUT_OF_MEMORY;
}

NS_IMETHODIMP
nsJSID::GetNumber(char * *aNumber)
{
    if (!aNumber)
        return NS_ERROR_NULL_POINTER;

    if (!mNumber) {
        if (!(mNumber = mID.ToString()))
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
    if (!aValid)
        return NS_ERROR_NULL_POINTER;

    *aValid = IsValid();
    return NS_OK;
}

NS_IMETHODIMP
nsJSID::Equals(nsIJSID *other, bool *_retval)
{
    if (!_retval)
        return NS_ERROR_NULL_POINTER;

    if (!other || mID.Equals(GetInvalidIID())) {
        *_retval = false;
        return NS_OK;
    }

    *_retval = other->GetID()->Equals(mID);
    return NS_OK;
}

NS_IMETHODIMP
nsJSID::Initialize(const char *idString)
{
    if (!idString)
        return NS_ERROR_NULL_POINTER;

    if (*idString != '\0' && mID.Equals(GetInvalidIID())) {
        Reset();

        if (idString[0] == '{') {
            if (mID.Parse(idString)) {
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
    if (mName && mName != gNoString)
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
    if (!str) {
        NS_ERROR("no string");
        return nullptr;
    }

    nsJSID* idObj = new nsJSID();
    if (idObj) {
        NS_ADDREF(idObj);
        if (NS_FAILED(idObj->Initialize(str)))
            NS_RELEASE(idObj);
    }
    return idObj;
}

//static
nsJSID*
nsJSID::NewID(const nsID& id)
{
    nsJSID* idObj = new nsJSID();
    if (idObj) {
        NS_ADDREF(idObj);
        idObj->mID = id;
        idObj->mName = nullptr;
        idObj->mNumber = nullptr;
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

class SharedScriptableHelperForJSIID MOZ_FINAL : public nsIXPCScriptable
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
static bool gClassObjectsWereInited = false;

static void EnsureClassObjectsInitialized()
{
    if (!gClassObjectsWereInited) {
        gSharedScriptableHelperForJSIID = new SharedScriptableHelperForJSIID();
        NS_ADDREF(gSharedScriptableHelperForJSIID);

        gClassObjectsWereInited = true;
    }
}

NS_METHOD GetSharedScriptableHelperForJSIID(uint32_t language,
                                            nsISupports **helper)
{
    EnsureClassObjectsInitialized();
    if (language == nsIProgrammingLanguage::JAVASCRIPT) {
        NS_IF_ADDREF(gSharedScriptableHelperForJSIID);
        *helper = gSharedScriptableHelperForJSIID;
    } else
        *helper = nullptr;
    return NS_OK;
}

/******************************************************/

#define NULL_CID                                                              \
{ 0x00000000, 0x0000, 0x0000,                                                 \
  { 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 } }

NS_DECL_CI_INTERFACE_GETTER(nsJSIID)
NS_IMPL_CLASSINFO(nsJSIID, GetSharedScriptableHelperForJSIID,
                  nsIClassInfo::THREADSAFE, NULL_CID)

NS_DECL_CI_INTERFACE_GETTER(nsJSCID)
NS_IMPL_CLASSINFO(nsJSCID, NULL, nsIClassInfo::THREADSAFE, NULL_CID)

void xpc_DestroyJSxIDClassObjects()
{
    if (gClassObjectsWereInited) {
        NS_IF_RELEASE(NS_CLASSINFO_NAME(nsJSIID));
        NS_IF_RELEASE(NS_CLASSINFO_NAME(nsJSCID));
        NS_IF_RELEASE(gSharedScriptableHelperForJSIID);

        gClassObjectsWereInited = false;
    }
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
    *aValid = true;
    return NS_OK;
}

NS_IMETHODIMP nsJSIID::Equals(nsIJSID *other, bool *_retval)
{
    if (!_retval)
        return NS_ERROR_NULL_POINTER;

    if (!other) {
        *_retval = false;
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
    if (!aInfo) {
        NS_ERROR("no info");
        return nullptr;
    }

    bool canScript;
    if (NS_FAILED(aInfo->IsScriptable(&canScript)) || !canScript)
        return nullptr;

    nsJSIID* idObj = new nsJSIID(aInfo);
    NS_IF_ADDREF(idObj);
    return idObj;
}


/* bool resolve (in nsIXPConnectWrappedNative wrapper, in JSContextPtr cx, in JSObjectPtr obj, in jsval id); */
NS_IMETHODIMP
nsJSIID::NewResolve(nsIXPConnectWrappedNative *wrapper,
                    JSContext * cx, JSObject * objArg,
                    jsid idArg, uint32_t flags,
                    JSObject * *objp, bool *_retval)
{
    RootedObject obj(cx, objArg);
    RootedId id(cx, idArg);
    XPCCallContext ccx(JS_CALLER, cx);

    AutoMarkingNativeInterfacePtr iface(ccx);

    const nsIID* iid;
    mInfo->GetIIDShared(&iid);

    iface = XPCNativeInterface::GetNewOrUsed(ccx, iid);

    if (!iface)
        return NS_OK;

    XPCNativeMember* member = iface->FindMember(id);
    if (member && member->IsConstant()) {
        RootedValue val(cx);
        if (!member->GetConstantValue(ccx, iface, val.address()))
            return NS_ERROR_OUT_OF_MEMORY;

        *objp = obj;
        *_retval = JS_DefinePropertyById(cx, obj, id, val, nullptr, nullptr,
                                         JSPROP_ENUMERATE | JSPROP_READONLY |
                                         JSPROP_PERMANENT);
    }

    return NS_OK;
}

/* bool enumerate (in nsIXPConnectWrappedNative wrapper, in JSContextPtr cx, in JSObjectPtr obj); */
NS_IMETHODIMP
nsJSIID::Enumerate(nsIXPConnectWrappedNative *wrapper,
                   JSContext * cx, JSObject * objArg, bool *_retval)
{
    // In this case, let's just eagerly resolve...

    RootedObject obj(cx, objArg);
    XPCCallContext ccx(JS_CALLER, cx);

    AutoMarkingNativeInterfacePtr iface(ccx);

    const nsIID* iid;
    mInfo->GetIIDShared(&iid);

    iface = XPCNativeInterface::GetNewOrUsed(ccx, iid);

    if (!iface)
        return NS_OK;

    uint16_t count = iface->GetMemberCount();
    for (uint16_t i = 0; i < count; i++) {
        XPCNativeMember* member = iface->GetMemberAt(i);
        if (member && member->IsConstant() &&
            !xpc_ForcePropertyResolve(cx, obj, member->GetName())) {
            return NS_ERROR_UNEXPECTED;
        }
    }
    return NS_OK;
}

/*
 * HasInstance hooks need to find an appropriate reflector in order to function
 * properly. There are two complexities that we need to handle:
 *
 * 1 - Cross-compartment wrappers. Chrome uses over 100 compartments, all with
 *     system principal. The success of an instanceof check should not depend
 *     on which compartment an object comes from. At the same time, we want to
 *     make sure we don't unwrap important security wrappers.
 *     CheckedUnwrap does the right thing here.
 *
 * 2 - Prototype chains. Suppose someone creates a vanilla JS object |a| and
 *     sets its __proto__ to some WN |b|. If |b instanceof nsIFoo| returns true,
 *     one would expect |a instanceof nsIFoo| to return true as well, since
 *     instanceof is transitive up the prototype chain in ECMAScript. Moreover,
 *     there's chrome code that relies on this.
 *
 * This static method handles both complexities, returning either an XPCWN, a
 * slim wrapper, a DOM object, or null. The object may well be cross-compartment
 * from |cx|.
 */
static JSObject *
FindObjectForHasInstance(JSContext *cx, HandleObject objArg)
{
    RootedObject obj(cx, objArg), proto(cx);
    while (obj && !IS_WRAPPER_CLASS(js::GetObjectClass(obj)) && !IsDOMObject(obj)) {
        if (js::IsWrapper(obj)) {
            obj = js::CheckedUnwrap(obj, /* stopAtOuter = */ false);
            continue;
        }
        if (!js::GetObjectProto(cx, obj, &proto))
            return nullptr;
        obj = proto;
    }
    return obj;
}


/* bool hasInstance (in nsIXPConnectWrappedNative wrapper, in JSContextPtr cx, in JSObjectPtr obj, in jsval val, out bool bp); */
NS_IMETHODIMP
nsJSIID::HasInstance(nsIXPConnectWrappedNative *wrapper,
                     JSContext * cx, JSObject * /* unused */,
                     const jsval &val, bool *bp, bool *_retval)
{
    *bp = false;
    nsresult rv = NS_OK;

    if (!JSVAL_IS_PRIMITIVE(val)) {
        // we have a JSObject
        RootedObject obj(cx, JSVAL_TO_OBJECT(val));

        NS_ASSERTION(obj, "when is an object not an object?");

        nsISupports *identity = nullptr;
        // is this really a native xpcom object with a wrapper?
        const nsIID* iid;
        mInfo->GetIIDShared(&iid);

        obj = FindObjectForHasInstance(cx, obj);
        if (!obj)
            return NS_OK;

        if (IS_SLIM_WRAPPER(obj)) {
            XPCWrappedNativeProto* proto = GetSlimWrapperProto(obj);
            if (proto->GetSet()->HasInterfaceWithAncestor(iid)) {
                *bp = true;
                return NS_OK;
            }

#ifdef DEBUG_slimwrappers
            char foo[NSID_LENGTH];
            iid->ToProvidedString(foo);
            SLIM_LOG_WILL_MORPH_FOR_PROP(cx, obj, foo);
#endif
            if (!MorphSlimWrapper(cx, obj))
                return NS_ERROR_FAILURE;
        } else if (IsDOMObject(obj)) {
              // Not all DOM objects implement nsISupports. But if they don't,
              // there's nothing to do in this HasInstance hook.
              if (!UnwrapDOMObjectToISupports(obj, identity))
                  return NS_OK;;
              nsCOMPtr<nsISupports> supp;
              identity->QueryInterface(*iid, getter_AddRefs(supp));
              *bp = supp;
              return NS_OK;
        }

        MOZ_ASSERT(IS_WN_WRAPPER(obj));
        XPCWrappedNative* other_wrapper = XPCWrappedNative::Get(obj);
        if (!other_wrapper)
            return NS_OK;

        // We'll trust the interface set of the wrapper if this is known
        // to be an interface that the objects *expects* to be able to
        // handle.
        if (other_wrapper->HasInterfaceNoQI(*iid)) {
            *bp = true;
            return NS_OK;
        }

        // Otherwise, we'll end up Querying the native object to be sure.
        XPCCallContext ccx(JS_CALLER, cx);

        AutoMarkingNativeInterfacePtr iface(ccx);
        iface = XPCNativeInterface::GetNewOrUsed(ccx, iid);

        nsresult findResult = NS_OK;
        if (iface && other_wrapper->FindTearOff(ccx, iface, false, &findResult))
            *bp = true;
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
    static const char* allowed[] = {"equals", "toString", nullptr};

    *_retval = xpc_CheckAccessList(methodName, allowed);
    return NS_OK;
}

/* string canGetProperty (in nsIIDPtr iid, in wstring propertyName); */
NS_IMETHODIMP
nsJSIID::CanGetProperty(const nsIID * iid, const PRUnichar *propertyName, char **_retval)
{
    static const char* allowed[] = {"name", "number", "valid", nullptr};
    *_retval = xpc_CheckAccessList(propertyName, allowed);
    return NS_OK;
}

/* string canSetProperty (in nsIIDPtr iid, in wstring propertyName); */
NS_IMETHODIMP
nsJSIID::CanSetProperty(const nsIID * iid, const PRUnichar *propertyName, char **_retval)
{
    // If you have to ask, then the answer is NO
    *_retval = nullptr;
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
    if (!mDetails.NameIsSet())
        mDetails.SetNameToNoString();
}

//static
nsJSCID*
nsJSCID::NewID(const char* str)
{
    if (!str) {
        NS_ERROR("no string");
        return nullptr;
    }

    nsJSCID* idObj = new nsJSCID();
    if (idObj) {
        bool success = false;
        NS_ADDREF(idObj);

        if (str[0] == '{') {
            if (NS_SUCCEEDED(idObj->Initialize(str)))
                success = true;
        } else {
            nsCOMPtr<nsIComponentRegistrar> registrar;
            NS_GetComponentRegistrar(getter_AddRefs(registrar));
            if (registrar) {
                nsCID *cid;
                if (NS_SUCCEEDED(registrar->ContractIDToCID(str, &cid))) {
                    success = idObj->mDetails.InitWithName(*cid, str);
                    nsMemory::Free(cid);
                }
            }
        }
        if (!success)
            NS_RELEASE(idObj);
    }
    return idObj;
}

static const nsID*
GetIIDArg(uint32_t argc, const JS::Value& val, JSContext* cx)
{
    const nsID* iid;

    // If an IID was passed in then use it
    if (argc) {
        JSObject* iidobj;
        if (JSVAL_IS_PRIMITIVE(val) ||
            !(iidobj = JSVAL_TO_OBJECT(val)) ||
            !(iid = xpc_JSObjectToID(cx, iidobj))) {
            return nullptr;
        }
    } else
        iid = &NS_GET_IID(nsISupports);

    return iid;
}

static void
GetWrapperObject(MutableHandleObject obj)
{
    obj.set(NULL);
    nsXPConnect* xpc = nsXPConnect::GetXPConnect();
    if (!xpc)
        return;

    nsAXPCNativeCallContext *ccxp = NULL;
    xpc->GetCurrentNativeCallContext(&ccxp);
    if (!ccxp)
        return;

    nsCOMPtr<nsIXPConnectWrappedNative> wrapper;
    ccxp->GetCalleeWrapper(getter_AddRefs(wrapper));
    obj.set(wrapper->GetJSObject());
}

/* nsISupports createInstance (); */
NS_IMETHODIMP
nsJSCID::CreateInstance(const JS::Value& iidval, JSContext* cx,
                        uint8_t optionalArgc, JS::Value* retval)
{
    if (!mDetails.IsValid())
        return NS_ERROR_XPC_BAD_CID;

    RootedObject obj(cx);
    GetWrapperObject(&obj);
    if (!obj) {
        return NS_ERROR_UNEXPECTED;
    }

    // Do the security check if necessary
    XPCContext* xpcc = XPCContext::GetXPCContext(cx);

    nsIXPCSecurityManager* sm;
    sm = xpcc->GetAppropriateSecurityManager(nsIXPCSecurityManager::HOOK_CREATE_INSTANCE);
    if (sm && NS_FAILED(sm->CanCreateInstance(cx, mDetails.ID()))) {
        NS_ERROR("how are we not being called from chrome here?");
        return NS_OK;
    }

    // If an IID was passed in then use it
    const nsID* iid = GetIIDArg(optionalArgc, iidval, cx);
    if (!iid)
        return NS_ERROR_XPC_BAD_IID;

    nsCOMPtr<nsIComponentManager> compMgr;
    nsresult rv = NS_GetComponentManager(getter_AddRefs(compMgr));
    if (NS_FAILED(rv))
        return NS_ERROR_UNEXPECTED;

    nsCOMPtr<nsISupports> inst;
    rv = compMgr->CreateInstance(mDetails.ID(), nullptr, *iid, getter_AddRefs(inst));
    NS_ASSERTION(NS_FAILED(rv) || inst, "component manager returned success, but instance is null!");

    if (NS_FAILED(rv) || !inst)
        return NS_ERROR_XPC_CI_RETURNED_FAILURE;

    rv = nsXPConnect::GetXPConnect()->WrapNativeToJSVal(cx, obj, inst, nullptr, iid, true, retval, nullptr);
    if (NS_FAILED(rv) || JSVAL_IS_PRIMITIVE(*retval))
        return NS_ERROR_XPC_CANT_CREATE_WN;
    return NS_OK;
}

/* nsISupports getService (); */
NS_IMETHODIMP
nsJSCID::GetService(const JS::Value& iidval, JSContext* cx,
                    uint8_t optionalArgc, JS::Value* retval)
{
    if (!mDetails.IsValid())
        return NS_ERROR_XPC_BAD_CID;

    RootedObject obj(cx);
    GetWrapperObject(&obj);
    if (!obj) {
        return NS_ERROR_UNEXPECTED;
    }

    // Do the security check if necessary
    XPCContext* xpcc = XPCContext::GetXPCContext(cx);

    nsIXPCSecurityManager* sm;
    sm = xpcc->GetAppropriateSecurityManager(nsIXPCSecurityManager::HOOK_GET_SERVICE);
    if (sm && NS_FAILED(sm->CanCreateInstance(cx, mDetails.ID()))) {
        NS_ASSERTION(JS_IsExceptionPending(cx),
                     "security manager vetoed GetService without setting exception");
        return NS_OK;
    }

    // If an IID was passed in then use it
    const nsID* iid = GetIIDArg(optionalArgc, iidval, cx);
    if (!iid)
        return NS_ERROR_XPC_BAD_IID;

    nsCOMPtr<nsIServiceManager> svcMgr;
    nsresult rv = NS_GetServiceManager(getter_AddRefs(svcMgr));
    if (NS_FAILED(rv))
        return rv;

    nsCOMPtr<nsISupports> srvc;
    rv = svcMgr->GetService(mDetails.ID(), *iid, getter_AddRefs(srvc));
    NS_ASSERTION(NS_FAILED(rv) || srvc, "service manager returned success, but service is null!");
    if (NS_FAILED(rv) || !srvc)
        return NS_ERROR_XPC_GS_RETURNED_FAILURE;

    RootedObject instJSObj(cx);
    nsCOMPtr<nsIXPConnectJSObjectHolder> holder;
    rv = nsXPConnect::GetXPConnect()->WrapNative(cx, obj, srvc, *iid, getter_AddRefs(holder));
    if (NS_FAILED(rv) || !holder ||
        // Assign, not compare
        !(instJSObj = holder->GetJSObject()))
        return NS_ERROR_XPC_CANT_CREATE_WN;

    *retval = OBJECT_TO_JSVAL(instJSObj);
    return NS_OK;
}

/* bool construct (in nsIXPConnectWrappedNative wrapper, in JSContextPtr cx, in JSObjectPtr obj, in uint32_t argc, in JSValPtr argv, in JSValPtr vp); */
NS_IMETHODIMP
nsJSCID::Construct(nsIXPConnectWrappedNative *wrapper,
                   JSContext * cx, JSObject * objArg,
                   const CallArgs &args, bool *_retval)
{
    RootedObject obj(cx, objArg);
    XPCJSRuntime* rt = nsXPConnect::GetRuntimeInstance();
    if (!rt)
        return NS_ERROR_FAILURE;

    // 'push' a call context and call on it
    RootedId name(cx, rt->GetStringID(XPCJSRuntime::IDX_CREATE_INSTANCE));
    XPCCallContext ccx(JS_CALLER, cx, obj, NullPtr(), name, args.length(), args.array(),
                       args.rval().address());

    *_retval = XPCWrappedNative::CallMethod(ccx);
    return NS_OK;
}

/* bool hasInstance (in nsIXPConnectWrappedNative wrapper, in JSContextPtr cx, in JSObjectPtr obj, in jsval val, out bool bp); */
NS_IMETHODIMP
nsJSCID::HasInstance(nsIXPConnectWrappedNative *wrapper,
                     JSContext * cx, JSObject * /* unused */,
                     const jsval &val, bool *bp, bool *_retval)
{
    *bp = false;
    nsresult rv = NS_OK;

    if (!JSVAL_IS_PRIMITIVE(val)) {
        // we have a JSObject
        RootedObject obj(cx, &val.toObject());

        NS_ASSERTION(obj, "when is an object not an object?");

        // is this really a native xpcom object with a wrapper?
        nsIClassInfo* ci = nullptr;
        obj = FindObjectForHasInstance(cx, obj);
        if (!obj || !IS_WRAPPER_CLASS(js::GetObjectClass(obj)))
            return rv;
        if (IS_SLIM_WRAPPER_OBJECT(obj))
            ci = GetSlimWrapperProto(obj)->GetClassInfo();
        else if (XPCWrappedNative* other_wrapper = XPCWrappedNative::Get(obj))
            ci = other_wrapper->GetClassInfo();

        // We consider CID equality to be the thing that matters here.
        // This is perhaps debatable.
        if (ci) {
            nsID cid;
            if (NS_SUCCEEDED(ci->GetClassIDNoAlloc(&cid)))
                *bp = cid.Equals(mDetails.ID());
        }
    }
    return rv;
}

/***************************************************************************/
// additional utilities...

JSObject *
xpc_NewIDObject(JSContext *cx, HandleObject jsobj, const nsID& aID)
{
    RootedObject obj(cx);

    nsCOMPtr<nsIJSID> iid =
            dont_AddRef(static_cast<nsIJSID*>(nsJSID::NewID(aID)));
    if (iid) {
        nsXPConnect* xpc = nsXPConnect::GetXPConnect();
        if (xpc) {
            nsCOMPtr<nsIXPConnectJSObjectHolder> holder;
            nsresult rv = xpc->WrapNative(cx, jsobj,
                                          static_cast<nsISupports*>(iid),
                                          NS_GET_IID(nsIJSID),
                                          getter_AddRefs(holder));
            if (NS_SUCCEEDED(rv) && holder) {
                obj = holder->GetJSObject();
            }
        }
    }
    return obj;
}

// note: returned pointer is only valid while |obj| remains alive!
const nsID*
xpc_JSObjectToID(JSContext *cx, JSObject* obj)
{
    if (!cx || !obj)
        return nullptr;

    // NOTE: this call does NOT addref
    XPCWrappedNative* wrapper = nullptr;
    obj = js::CheckedUnwrap(obj);
    if (obj && IS_WN_WRAPPER(obj))
        wrapper = XPCWrappedNative::Get(obj);
    if (wrapper &&
        (wrapper->HasInterfaceNoQI(NS_GET_IID(nsIJSID))  ||
         wrapper->HasInterfaceNoQI(NS_GET_IID(nsIJSIID)) ||
         wrapper->HasInterfaceNoQI(NS_GET_IID(nsIJSCID)))) {
        return ((nsIJSID*)wrapper->GetIdentityObject())->GetID();
    }
    return nullptr;
}

JSBool
xpc_JSObjectIsID(JSContext *cx, JSObject* obj)
{
    NS_ASSERTION(cx && obj, "bad param");
    // NOTE: this call does NOT addref
    XPCWrappedNative* wrapper = nullptr;
    obj = js::CheckedUnwrap(obj);
    if (obj && IS_WN_WRAPPER(obj))
        wrapper = XPCWrappedNative::Get(obj);
    return wrapper &&
           (wrapper->HasInterfaceNoQI(NS_GET_IID(nsIJSID))  ||
            wrapper->HasInterfaceNoQI(NS_GET_IID(nsIJSIID)) ||
            wrapper->HasInterfaceNoQI(NS_GET_IID(nsIJSCID)));
}


