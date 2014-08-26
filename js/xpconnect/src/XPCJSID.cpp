/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* vim: set ts=8 sts=4 et sw=4 tw=99: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/* An xpcom implementation of the JavaScript nsIID and nsCID objects. */

#include "xpcprivate.h"
#include "mozilla/dom/BindingUtils.h"
#include "mozilla/Attributes.h"
#include "JavaScriptParent.h"
#include "mozilla/StaticPtr.h"

using namespace mozilla::dom;
using namespace JS;

/***************************************************************************/
// nsJSID

NS_IMPL_CLASSINFO(nsJSID, nullptr, 0, NS_JS_ID_CID)
NS_IMPL_ISUPPORTS_CI(nsJSID, nsIJSID)

const char nsJSID::gNoString[] = "";

nsJSID::nsJSID()
    : mID(GetInvalidIID()),
      mNumber(const_cast<char *>(gNoString)),
      mName(const_cast<char *>(gNoString))
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
    MOZ_ASSERT(!mName || mName == gNoString ,"name already set");
    MOZ_ASSERT(name,"null name");
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
    MOZ_ASSERT(mName, "name not set");
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
            mNumber = const_cast<char *>(gNoString);
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
    MOZ_ASSERT(nameString, "no name");
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
    static const nsID invalid = {0xbb1f47b0, 0xd137, 0x11d2,
                                  {0x98, 0x41, 0x0, 0x60, 0x8, 0x96, 0x24, 0x22}};
    return invalid;
}

//static
already_AddRefed<nsJSID>
nsJSID::NewID(const char* str)
{
    if (!str) {
        NS_ERROR("no string");
        return nullptr;
    }

    nsRefPtr<nsJSID> idObj = new nsJSID();
    NS_ENSURE_SUCCESS(idObj->Initialize(str), nullptr);
    return idObj.forget();
}

//static
already_AddRefed<nsJSID>
nsJSID::NewID(const nsID& id)
{
    nsRefPtr<nsJSID> idObj = new nsJSID();
    idObj->mID = id;
    idObj->mName = nullptr;
    idObj->mNumber = nullptr;
    return idObj.forget();
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
    ~SharedScriptableHelperForJSIID() {}
public:
    NS_DECL_ISUPPORTS
    NS_DECL_NSIXPCSCRIPTABLE
    SharedScriptableHelperForJSIID() {}
};

NS_INTERFACE_MAP_BEGIN(SharedScriptableHelperForJSIID)
  NS_INTERFACE_MAP_ENTRY(nsIXPCScriptable)
  NS_INTERFACE_MAP_ENTRY_AMBIGUOUS(nsISupports, nsIXPCScriptable)
NS_INTERFACE_MAP_END

NS_IMPL_ADDREF(SharedScriptableHelperForJSIID)
NS_IMPL_RELEASE(SharedScriptableHelperForJSIID)

// The nsIXPCScriptable map declaration that will generate stubs for us...
#define XPC_MAP_CLASSNAME           SharedScriptableHelperForJSIID
#define XPC_MAP_QUOTED_CLASSNAME   "JSIID"
#define XPC_MAP_FLAGS               nsIXPCScriptable::DONT_ENUM_STATIC_PROPS |\
                                    nsIXPCScriptable::ALLOW_PROP_MODS_DURING_RESOLVE
#include "xpc_map_end.h" /* This will #undef the above */

static mozilla::StaticRefPtr<nsIXPCScriptable> gSharedScriptableHelperForJSIID;
static bool gClassObjectsWereInited = false;

static void EnsureClassObjectsInitialized()
{
    if (!gClassObjectsWereInited) {
        gSharedScriptableHelperForJSIID = new SharedScriptableHelperForJSIID();

        gClassObjectsWereInited = true;
    }
}

NS_METHOD GetSharedScriptableHelperForJSIID(uint32_t language,
                                            nsISupports **helper)
{
    EnsureClassObjectsInitialized();
    if (language == nsIProgrammingLanguage::JAVASCRIPT) {
        nsCOMPtr<nsIXPCScriptable> temp = gSharedScriptableHelperForJSIID.get();
        temp.forget(helper);
    } else
        *helper = nullptr;
    return NS_OK;
}

/******************************************************/

#define NULL_CID                                                              \
{ 0x00000000, 0x0000, 0x0000,                                                 \
  { 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 } }

// We pass nsIClassInfo::DOM_OBJECT so that nsJSIID instances may be created
// in unprivileged scopes.
NS_DECL_CI_INTERFACE_GETTER(nsJSIID)
NS_IMPL_CLASSINFO(nsJSIID, GetSharedScriptableHelperForJSIID,
                  nsIClassInfo::DOM_OBJECT, NULL_CID)

NS_DECL_CI_INTERFACE_GETTER(nsJSCID)
NS_IMPL_CLASSINFO(nsJSCID, nullptr, 0, NULL_CID)

void xpc_DestroyJSxIDClassObjects()
{
    if (gClassObjectsWereInited) {
        NS_IF_RELEASE(NS_CLASSINFO_NAME(nsJSIID));
        NS_IF_RELEASE(NS_CLASSINFO_NAME(nsJSCID));
        gSharedScriptableHelperForJSIID = nullptr;

        gClassObjectsWereInited = false;
    }
}

/***************************************************************************/

NS_INTERFACE_MAP_BEGIN(nsJSIID)
  NS_INTERFACE_MAP_ENTRY(nsIJSID)
  NS_INTERFACE_MAP_ENTRY(nsIJSIID)
  NS_INTERFACE_MAP_ENTRY(nsIXPCScriptable)
  NS_INTERFACE_MAP_ENTRY_AMBIGUOUS(nsISupports, nsIJSID)
  NS_IMPL_QUERY_CLASSINFO(nsJSIID)
NS_INTERFACE_MAP_END

NS_IMPL_ADDREF(nsJSIID)
NS_IMPL_RELEASE(nsJSIID)
NS_IMPL_CI_INTERFACE_GETTER(nsJSIID, nsIJSID, nsIJSIID)

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
already_AddRefed<nsJSIID>
nsJSIID::NewID(nsIInterfaceInfo* aInfo)
{
    if (!aInfo) {
        NS_ERROR("no info");
        return nullptr;
    }

    bool canScript;
    if (NS_FAILED(aInfo->IsScriptable(&canScript)) || !canScript)
        return nullptr;

    nsRefPtr<nsJSIID> idObj = new nsJSIID(aInfo);
    return idObj.forget();
}


/* bool resolve (in nsIXPConnectWrappedNative wrapper, in JSContextPtr cx, in JSObjectPtr obj, in jsval id); */
NS_IMETHODIMP
nsJSIID::NewResolve(nsIXPConnectWrappedNative *wrapper,
                    JSContext * cx, JSObject * objArg,
                    jsid idArg, JSObject * *objp,
                    bool *_retval)
{
    RootedObject obj(cx, objArg);
    RootedId id(cx, idArg);
    XPCCallContext ccx(JS_CALLER, cx);

    AutoMarkingNativeInterfacePtr iface(ccx);

    iface = XPCNativeInterface::GetNewOrUsed(mInfo);

    if (!iface)
        return NS_OK;

    XPCNativeMember* member = iface->FindMember(id);
    if (member && member->IsConstant()) {
        RootedValue val(cx);
        if (!member->GetConstantValue(ccx, iface, val.address()))
            return NS_ERROR_OUT_OF_MEMORY;

        *objp = obj;
        *_retval = JS_DefinePropertyById(cx, obj, id, val,
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

    iface = XPCNativeInterface::GetNewOrUsed(mInfo);

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
 * DOM object, or null. The object may well be cross-compartment from |cx|.
 */
static JSObject *
FindObjectForHasInstance(JSContext *cx, HandleObject objArg)
{
    RootedObject obj(cx, objArg), proto(cx);

    while (obj && !IS_WN_REFLECTOR(obj) &&
           !IsDOMObject(obj) && !mozilla::jsipc::IsCPOW(obj))
    {
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

nsresult
xpc::HasInstance(JSContext *cx, HandleObject objArg, const nsID *iid, bool *bp)
{
    *bp = false;

    RootedObject obj(cx, FindObjectForHasInstance(cx, objArg));
    if (!obj)
        return NS_OK;

    if (mozilla::jsipc::IsCPOW(obj))
        return mozilla::jsipc::InstanceOf(obj, iid, bp);

    nsISupports *identity = UnwrapReflectorToISupports(obj);
    if (!identity)
        return NS_OK;

    nsCOMPtr<nsISupports> supp;
    identity->QueryInterface(*iid, getter_AddRefs(supp));
    *bp = supp;

    // Our old HasInstance implementation operated by invoking FindTearOff on
    // XPCWrappedNatives, and various bits of chrome JS came to depend on
    // |instanceof| doing an implicit QI if it succeeds. Do a drive-by QI to
    // preserve that behavior. This is just a compatibility hack, so we don't
    // really care if it fails.
    if (IS_WN_REFLECTOR(obj))
        (void) XPCWrappedNative::Get(obj)->FindTearOff(*iid);

    return NS_OK;
}

/* bool hasInstance (in nsIXPConnectWrappedNative wrapper, in JSContextPtr cx, in JSObjectPtr obj, in jsval val, out bool bp); */
NS_IMETHODIMP
nsJSIID::HasInstance(nsIXPConnectWrappedNative *wrapper,
                     JSContext *cx, JSObject * /* unused */,
                     HandleValue val, bool *bp, bool *_retval)
{
    *bp = false;

    if (val.isPrimitive())
        return NS_OK;

    // we have a JSObject
    RootedObject obj(cx, &val.toObject());

    const nsIID* iid;
    mInfo->GetIIDShared(&iid);
    return xpc::HasInstance(cx, obj, iid, bp);
}

/***************************************************************************/

NS_INTERFACE_MAP_BEGIN(nsJSCID)
  NS_INTERFACE_MAP_ENTRY(nsIJSID)
  NS_INTERFACE_MAP_ENTRY(nsIJSCID)
  NS_INTERFACE_MAP_ENTRY(nsIXPCScriptable)
  NS_INTERFACE_MAP_ENTRY_AMBIGUOUS(nsISupports, nsIJSID)
  NS_IMPL_QUERY_CLASSINFO(nsJSCID)
NS_INTERFACE_MAP_END

NS_IMPL_ADDREF(nsJSCID)
NS_IMPL_RELEASE(nsJSCID)
NS_IMPL_CI_INTERFACE_GETTER(nsJSCID, nsIJSID, nsIJSCID)

// The nsIXPCScriptable map declaration that will generate stubs for us...
#define XPC_MAP_CLASSNAME           nsJSCID
#define XPC_MAP_QUOTED_CLASSNAME   "nsJSCID"
#define                             XPC_MAP_WANT_CONSTRUCT
#define                             XPC_MAP_WANT_HASINSTANCE
#define XPC_MAP_FLAGS               0
#include "xpc_map_end.h" /* This will #undef the above */

nsJSCID::nsJSCID()  { mDetails = new nsJSID(); }
nsJSCID::~nsJSCID() {}

NS_IMETHODIMP nsJSCID::GetName(char * *aName)
    {ResolveName(); return mDetails->GetName(aName);}

NS_IMETHODIMP nsJSCID::GetNumber(char * *aNumber)
    {return mDetails->GetNumber(aNumber);}

NS_IMETHODIMP_(const nsID*) nsJSCID::GetID()
    {return &mDetails->ID();}

NS_IMETHODIMP nsJSCID::GetValid(bool *aValid)
    {return mDetails->GetValid(aValid);}

NS_IMETHODIMP nsJSCID::Equals(nsIJSID *other, bool *_retval)
    {return mDetails->Equals(other, _retval);}

NS_IMETHODIMP nsJSCID::Initialize(const char *idString)
    {return mDetails->Initialize(idString);}

NS_IMETHODIMP nsJSCID::ToString(char **_retval)
    {ResolveName(); return mDetails->ToString(_retval);}

void
nsJSCID::ResolveName()
{
    if (!mDetails->NameIsSet())
        mDetails->SetNameToNoString();
}

//static
already_AddRefed<nsJSCID>
nsJSCID::NewID(const char* str)
{
    if (!str) {
        NS_ERROR("no string");
        return nullptr;
    }

    nsRefPtr<nsJSCID> idObj = new nsJSCID();
    if (str[0] == '{') {
        NS_ENSURE_SUCCESS(idObj->Initialize(str), nullptr);
    } else {
        nsCOMPtr<nsIComponentRegistrar> registrar;
        NS_GetComponentRegistrar(getter_AddRefs(registrar));
        NS_ENSURE_TRUE(registrar, nullptr);

        nsCID *cid;
        if (NS_FAILED(registrar->ContractIDToCID(str, &cid)))
            return nullptr;
        bool success = idObj->mDetails->InitWithName(*cid, str);
        nsMemory::Free(cid);
        if (!success)
            return nullptr;
    }
    return idObj.forget();
}

static const nsID*
GetIIDArg(uint32_t argc, const JS::Value& val, JSContext* cx)
{
    const nsID* iid;

    // If an IID was passed in then use it
    if (argc) {
        JSObject* iidobj;
        if (val.isPrimitive() ||
            !(iidobj = val.toObjectOrNull()) ||
            !(iid = xpc_JSObjectToID(cx, iidobj))) {
            return nullptr;
        }
    } else
        iid = &NS_GET_IID(nsISupports);

    return iid;
}

/* nsISupports createInstance (); */
NS_IMETHODIMP
nsJSCID::CreateInstance(HandleValue iidval, JSContext *cx,
                        uint8_t optionalArgc, MutableHandleValue retval)
{
    if (!mDetails->IsValid())
        return NS_ERROR_XPC_BAD_CID;

    if (NS_FAILED(nsXPConnect::SecurityManager()->CanCreateInstance(cx, mDetails->ID()))) {
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
    rv = compMgr->CreateInstance(mDetails->ID(), nullptr, *iid, getter_AddRefs(inst));
    MOZ_ASSERT(NS_FAILED(rv) || inst, "component manager returned success, but instance is null!");

    if (NS_FAILED(rv) || !inst)
        return NS_ERROR_XPC_CI_RETURNED_FAILURE;

    rv = nsContentUtils::WrapNative(cx, inst, iid, retval);
    if (NS_FAILED(rv) || retval.isPrimitive())
        return NS_ERROR_XPC_CANT_CREATE_WN;
    return NS_OK;
}

/* nsISupports getService (); */
NS_IMETHODIMP
nsJSCID::GetService(HandleValue iidval, JSContext *cx, uint8_t optionalArgc,
                    MutableHandleValue retval)
{
    if (!mDetails->IsValid())
        return NS_ERROR_XPC_BAD_CID;

    if (NS_FAILED(nsXPConnect::SecurityManager()->CanCreateInstance(cx, mDetails->ID()))) {
        MOZ_ASSERT(JS_IsExceptionPending(cx),
                   "security manager vetoed GetService without setting exception");
        return NS_OK;
    }

    // If an IID was passed in then use it
    const nsID *iid = GetIIDArg(optionalArgc, iidval, cx);
    if (!iid)
        return NS_ERROR_XPC_BAD_IID;

    nsCOMPtr<nsIServiceManager> svcMgr;
    nsresult rv = NS_GetServiceManager(getter_AddRefs(svcMgr));
    if (NS_FAILED(rv))
        return rv;

    nsCOMPtr<nsISupports> srvc;
    rv = svcMgr->GetService(mDetails->ID(), *iid, getter_AddRefs(srvc));
    MOZ_ASSERT(NS_FAILED(rv) || srvc, "service manager returned success, but service is null!");
    if (NS_FAILED(rv) || !srvc)
        return NS_ERROR_XPC_GS_RETURNED_FAILURE;

    RootedValue v(cx);
    rv = nsContentUtils::WrapNative(cx, srvc, iid, &v);
    if (NS_FAILED(rv) || !v.isObject())
        return NS_ERROR_XPC_CANT_CREATE_WN;

    retval.set(v);
    return NS_OK;
}

/* bool construct (in nsIXPConnectWrappedNative wrapper, in JSContextPtr cx, in JSObjectPtr obj, in uint32_t argc, in JSValPtr argv, in JSValPtr vp); */
NS_IMETHODIMP
nsJSCID::Construct(nsIXPConnectWrappedNative *wrapper,
                   JSContext *cx, JSObject *objArg,
                   const CallArgs &args, bool *_retval)
{
    RootedObject obj(cx, objArg);
    XPCJSRuntime* rt = nsXPConnect::GetRuntimeInstance();
    if (!rt)
        return NS_ERROR_FAILURE;

    // 'push' a call context and call on it
    RootedId name(cx, rt->GetStringID(XPCJSRuntime::IDX_CREATE_INSTANCE));
    XPCCallContext ccx(JS_CALLER, cx, obj, JS::NullPtr(), name, args.length(), args.array(),
                       args.rval().address());

    *_retval = XPCWrappedNative::CallMethod(ccx);
    return NS_OK;
}

/* bool hasInstance (in nsIXPConnectWrappedNative wrapper, in JSContextPtr cx, in JSObjectPtr obj, in jsval val, out bool bp); */
NS_IMETHODIMP
nsJSCID::HasInstance(nsIXPConnectWrappedNative *wrapper,
                     JSContext *cx, JSObject * /* unused */,
                     HandleValue val, bool *bp, bool *_retval)
{
    *bp = false;
    nsresult rv = NS_OK;

    if (val.isObject()) {
        // we have a JSObject
        RootedObject obj(cx, &val.toObject());

        MOZ_ASSERT(obj, "when is an object not an object?");

        // is this really a native xpcom object with a wrapper?
        nsIClassInfo *ci = nullptr;
        obj = FindObjectForHasInstance(cx, obj);
        if (!obj || !IS_WN_REFLECTOR(obj))
            return rv;
        if (XPCWrappedNative *other_wrapper = XPCWrappedNative::Get(obj))
            ci = other_wrapper->GetClassInfo();

        // We consider CID equality to be the thing that matters here.
        // This is perhaps debatable.
        if (ci) {
            nsID cid;
            if (NS_SUCCEEDED(ci->GetClassIDNoAlloc(&cid)))
                *bp = cid.Equals(mDetails->ID());
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

    nsCOMPtr<nsIJSID> iid = nsJSID::NewID(aID);
    if (iid) {
        nsXPConnect *xpc = nsXPConnect::XPConnect();
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
xpc_JSObjectToID(JSContext *cx, JSObject *obj)
{
    if (!cx || !obj)
        return nullptr;

    // NOTE: this call does NOT addref
    XPCWrappedNative *wrapper = nullptr;
    obj = js::CheckedUnwrap(obj);
    if (obj && IS_WN_REFLECTOR(obj))
        wrapper = XPCWrappedNative::Get(obj);
    if (wrapper &&
        (wrapper->HasInterfaceNoQI(NS_GET_IID(nsIJSID))  ||
         wrapper->HasInterfaceNoQI(NS_GET_IID(nsIJSIID)) ||
         wrapper->HasInterfaceNoQI(NS_GET_IID(nsIJSCID)))) {
        return ((nsIJSID*)wrapper->GetIdentityObject())->GetID();
    }
    return nullptr;
}

bool
xpc_JSObjectIsID(JSContext *cx, JSObject *obj)
{
    MOZ_ASSERT(cx && obj, "bad param");
    // NOTE: this call does NOT addref
    XPCWrappedNative *wrapper = nullptr;
    obj = js::CheckedUnwrap(obj);
    if (obj && IS_WN_REFLECTOR(obj))
        wrapper = XPCWrappedNative::Get(obj);
    return wrapper &&
           (wrapper->HasInterfaceNoQI(NS_GET_IID(nsIJSID))  ||
            wrapper->HasInterfaceNoQI(NS_GET_IID(nsIJSIID)) ||
            wrapper->HasInterfaceNoQI(NS_GET_IID(nsIJSCID)));
}


