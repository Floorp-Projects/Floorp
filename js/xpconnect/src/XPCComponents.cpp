/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* vim: set ts=8 sts=4 et sw=4 tw=99: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/* The "Components" xpcom objects for JavaScript. */

#include "xpcprivate.h"
#include "xpc_make_class.h"
#include "XPCJSWeakReference.h"
#include "WrapperFactory.h"
#include "nsJSUtils.h"
#include "mozJSComponentLoader.h"
#include "nsContentUtils.h"
#include "nsCycleCollector.h"
#include "jsfriendapi.h"
#include "js/StructuredClone.h"
#include "mozilla/Attributes.h"
#include "mozilla/jsipc/CrossProcessObjectWrappers.h"
#include "mozilla/Preferences.h"
#include "nsJSEnvironment.h"
#include "mozilla/TimeStamp.h"
#include "mozilla/ResultExtensions.h"
#include "mozilla/URLPreloader.h"
#include "mozilla/dom/DOMException.h"
#include "mozilla/dom/DOMExceptionBinding.h"
#include "mozilla/dom/BindingUtils.h"
#include "mozilla/dom/StructuredCloneTags.h"
#include "mozilla/dom/WindowBinding.h"
#include "mozilla/Scheduler.h"
#include "nsZipArchive.h"
#include "nsWindowMemoryReporter.h"
#include "nsIException.h"
#include "nsIScriptError.h"
#include "nsISimpleEnumerator.h"
#include "nsPIDOMWindow.h"
#include "nsGlobalWindow.h"
#include "nsScriptError.h"
#include "GeckoProfiler.h"

using namespace mozilla;
using namespace JS;
using namespace js;
using namespace xpc;
using mozilla::dom::Exception;

/***************************************************************************/
// stuff used by all

nsresult
xpc::ThrowAndFail(nsresult errNum, JSContext* cx, bool* retval)
{
    XPCThrower::Throw(errNum, cx);
    *retval = false;
    return NS_OK;
}

static bool
JSValIsInterfaceOfType(JSContext* cx, HandleValue v, REFNSIID iid)
{

    nsCOMPtr<nsIXPConnectWrappedNative> wn;
    nsCOMPtr<nsISupports> iface;

    if (v.isPrimitive())
        return false;

    nsXPConnect* xpc = nsXPConnect::XPConnect();
    RootedObject obj(cx, &v.toObject());
    return NS_SUCCEEDED(xpc->GetWrappedNativeOfJSObject(cx, obj, getter_AddRefs(wn))) && wn &&
        NS_SUCCEEDED(wn->Native()->QueryInterface(iid, getter_AddRefs(iface))) && iface;
}

char*
xpc::CloneAllAccess()
{
    return moz_xstrdup("AllAccess");
}

char*
xpc::CheckAccessList(const char16_t* wideName, const char* const list[])
{
    nsAutoCString asciiName;
    CopyUTF16toUTF8(nsDependentString(wideName), asciiName);

    for (const char* const* p = list; *p; p++)
        if (!strcmp(*p, asciiName.get()))
            return CloneAllAccess();

    return nullptr;
}

/***************************************************************************/
/***************************************************************************/
/***************************************************************************/


class nsXPCComponents_Interfaces final :
            public nsIXPCComponents_Interfaces,
            public nsIXPCScriptable,
            public nsIClassInfo
{
public:
    // all the interface method declarations...
    NS_DECL_ISUPPORTS
    NS_DECL_NSIXPCCOMPONENTS_INTERFACES
    NS_DECL_NSIXPCSCRIPTABLE
    NS_DECL_NSICLASSINFO

public:
    nsXPCComponents_Interfaces();

private:
    virtual ~nsXPCComponents_Interfaces();
};

NS_IMETHODIMP
nsXPCComponents_Interfaces::GetInterfaces(uint32_t* aCount, nsIID * **aArray)
{
    *aCount = 2;
    nsIID** array = static_cast<nsIID**>(moz_xmalloc(2 * sizeof(nsIID*)));
    *aArray = array;

    array[0] = NS_GET_IID(nsIXPCComponents_Interfaces).Clone();
    array[1] = NS_GET_IID(nsIXPCScriptable).Clone();
    return NS_OK;
}

NS_IMETHODIMP
nsXPCComponents_Interfaces::GetScriptableHelper(nsIXPCScriptable** retval)
{
    *retval = nullptr;
    return NS_OK;
}

NS_IMETHODIMP
nsXPCComponents_Interfaces::GetContractID(nsACString& aContractID)
{
    aContractID.SetIsVoid(true);
    return NS_ERROR_NOT_AVAILABLE;
}

NS_IMETHODIMP
nsXPCComponents_Interfaces::GetClassDescription(nsACString& aClassDescription)
{
    aClassDescription.AssignLiteral("XPCComponents_Interfaces");
    return NS_OK;
}

NS_IMETHODIMP
nsXPCComponents_Interfaces::GetClassID(nsCID * *aClassID)
{
    *aClassID = nullptr;
    return NS_OK;
}

NS_IMETHODIMP
nsXPCComponents_Interfaces::GetFlags(uint32_t* aFlags)
{
    *aFlags = 0;
    return NS_OK;
}

NS_IMETHODIMP
nsXPCComponents_Interfaces::GetClassIDNoAlloc(nsCID* aClassIDNoAlloc)
{
    return NS_ERROR_NOT_AVAILABLE;
}

nsXPCComponents_Interfaces::nsXPCComponents_Interfaces()
{
}

nsXPCComponents_Interfaces::~nsXPCComponents_Interfaces()
{
    // empty
}


NS_IMPL_ISUPPORTS(nsXPCComponents_Interfaces,
                  nsIXPCComponents_Interfaces,
                  nsIXPCScriptable,
                  nsIClassInfo);

// The nsIXPCScriptable map declaration that will generate stubs for us...
#define XPC_MAP_CLASSNAME         nsXPCComponents_Interfaces
#define XPC_MAP_QUOTED_CLASSNAME "nsXPCComponents_Interfaces"
#define XPC_MAP_FLAGS (XPC_SCRIPTABLE_WANT_RESOLVE | \
                       XPC_SCRIPTABLE_WANT_NEWENUMERATE | \
                       XPC_SCRIPTABLE_ALLOW_PROP_MODS_DURING_RESOLVE)
#include "xpc_map_end.h" /* This will #undef the above */


NS_IMETHODIMP
nsXPCComponents_Interfaces::NewEnumerate(nsIXPConnectWrappedNative* wrapper,
                                         JSContext* cx, JSObject* obj,
                                         JS::AutoIdVector& properties,
                                         bool* _retval)
{

    if (!properties.reserve(nsXPTInterfaceInfo::InterfaceCount())) {
        *_retval = false;
        return NS_OK;
    }

    for (uint32_t index = 0; index < nsXPTInterfaceInfo::InterfaceCount(); index++) {
        const nsXPTInterfaceInfo* interface = nsXPTInterfaceInfo::ByIndex(index);
        if (!interface)
            continue;

        const char* name = interface->Name();
        if (!name)
            continue;

        RootedString idstr(cx, JS_NewStringCopyZ(cx, name));
        if (!idstr) {
            *_retval = false;
            return NS_OK;
        }

        RootedId id(cx);
        if (!JS_StringToId(cx, idstr, &id)) {
            *_retval = false;
            return NS_OK;
        }

        properties.infallibleAppend(id);
    }

    return NS_OK;
}

NS_IMETHODIMP
nsXPCComponents_Interfaces::Resolve(nsIXPConnectWrappedNative* wrapper,
                                    JSContext* cx, JSObject* objArg,
                                    jsid idArg, bool* resolvedp,
                                    bool* _retval)
{
    RootedObject obj(cx, objArg);
    RootedId id(cx, idArg);

    if (!JSID_IS_STRING(id))
        return NS_OK;

    JSAutoByteString name;
    RootedString str(cx, JSID_TO_STRING(id));

    // we only allow interfaces by name here
    if (name.encodeLatin1(cx, str) && name.ptr()[0] != '{') {
        const nsXPTInterfaceInfo* info = nsXPTInterfaceInfo::ByName(name.ptr());
        if (!info)
            return NS_OK;

        nsCOMPtr<nsIJSIID> nsid = nsJSIID::NewID(info);

        if (nsid) {
            nsXPConnect* xpc = nsXPConnect::XPConnect();
            RootedObject idobj(cx);
            if (NS_SUCCEEDED(xpc->WrapNative(cx, obj,
                                             static_cast<nsIJSIID*>(nsid),
                                             NS_GET_IID(nsIJSIID),
                                             idobj.address()))) {
                if (idobj) {
                    *resolvedp = true;
                    *_retval = JS_DefinePropertyById(cx, obj, id, idobj,
                                                     JSPROP_ENUMERATE |
                                                     JSPROP_READONLY |
                                                     JSPROP_PERMANENT |
                                                     JSPROP_RESOLVING);
                }
            }
        }
    }
    return NS_OK;
}

/***************************************************************************/
/***************************************************************************/
/***************************************************************************/

class nsXPCComponents_InterfacesByID final :
            public nsIXPCComponents_InterfacesByID,
            public nsIXPCScriptable,
            public nsIClassInfo
{
public:
    // all the interface method declarations...
    NS_DECL_ISUPPORTS
    NS_DECL_NSIXPCCOMPONENTS_INTERFACESBYID
    NS_DECL_NSIXPCSCRIPTABLE
    NS_DECL_NSICLASSINFO

public:
    nsXPCComponents_InterfacesByID();

private:
    virtual ~nsXPCComponents_InterfacesByID();
};

/***************************************************************************/
NS_IMETHODIMP
nsXPCComponents_InterfacesByID::GetInterfaces(uint32_t* aCount, nsIID * **aArray)
{
    *aCount = 2;
    nsIID** array = static_cast<nsIID**>(moz_xmalloc(2 * sizeof(nsIID*)));
    *aArray = array;

    array[0] = NS_GET_IID(nsIXPCComponents_InterfacesByID).Clone();
    array[1] = NS_GET_IID(nsIXPCScriptable).Clone();
    return NS_OK;
}

NS_IMETHODIMP
nsXPCComponents_InterfacesByID::GetScriptableHelper(nsIXPCScriptable** retval)
{
    *retval = nullptr;
    return NS_OK;
}

NS_IMETHODIMP
nsXPCComponents_InterfacesByID::GetContractID(nsACString& aContractID)
{
    aContractID.SetIsVoid(true);
    return NS_ERROR_NOT_AVAILABLE;
}

NS_IMETHODIMP
nsXPCComponents_InterfacesByID::GetClassDescription(
    nsACString& aClassDescription)
{
    aClassDescription.AssignLiteral("XPCComponents_InterfacesByID");
    return NS_OK;
}

NS_IMETHODIMP
nsXPCComponents_InterfacesByID::GetClassID(nsCID * *aClassID)
{
    *aClassID = nullptr;
    return NS_OK;
}

NS_IMETHODIMP
nsXPCComponents_InterfacesByID::GetFlags(uint32_t* aFlags)
{
    *aFlags = 0;
    return NS_OK;
}

NS_IMETHODIMP
nsXPCComponents_InterfacesByID::GetClassIDNoAlloc(nsCID* aClassIDNoAlloc)
{
    return NS_ERROR_NOT_AVAILABLE;
}

nsXPCComponents_InterfacesByID::nsXPCComponents_InterfacesByID()
{
}

nsXPCComponents_InterfacesByID::~nsXPCComponents_InterfacesByID()
{
    // empty
}

NS_IMPL_ISUPPORTS(nsXPCComponents_InterfacesByID,
                  nsIXPCComponents_InterfacesByID,
                  nsIXPCScriptable,
                  nsIClassInfo)

// The nsIXPCScriptable map declaration that will generate stubs for us...
#define XPC_MAP_CLASSNAME         nsXPCComponents_InterfacesByID
#define XPC_MAP_QUOTED_CLASSNAME "nsXPCComponents_InterfacesByID"
#define XPC_MAP_FLAGS (XPC_SCRIPTABLE_WANT_RESOLVE | \
                       XPC_SCRIPTABLE_WANT_NEWENUMERATE | \
                       XPC_SCRIPTABLE_ALLOW_PROP_MODS_DURING_RESOLVE)
#include "xpc_map_end.h" /* This will #undef the above */

NS_IMETHODIMP
nsXPCComponents_InterfacesByID::NewEnumerate(nsIXPConnectWrappedNative* wrapper,
                                             JSContext* cx, JSObject* obj,
                                             JS::AutoIdVector& properties,
                                             bool* _retval)
{

    if (!properties.reserve(nsXPTInterfaceInfo::InterfaceCount())) {
        *_retval = false;
        return NS_OK;
    }

    for (uint32_t index = 0; index < nsXPTInterfaceInfo::InterfaceCount(); index++) {
        const nsXPTInterfaceInfo* interface = nsXPTInterfaceInfo::ByIndex(index);
        if (!interface)
            continue;

        char idstr[NSID_LENGTH];
        interface->IID().ToProvidedString(idstr);
        RootedString jsstr(cx, JS_NewStringCopyZ(cx, idstr));
        if (!jsstr) {
            *_retval = false;
            return NS_OK;
        }

        RootedId id(cx);
        if (!JS_StringToId(cx, jsstr, &id)) {
            *_retval = false;
            return NS_OK;
        }

        properties.infallibleAppend(id);
    }

    return NS_OK;
}

NS_IMETHODIMP
nsXPCComponents_InterfacesByID::Resolve(nsIXPConnectWrappedNative* wrapper,
                                        JSContext* cx, JSObject* objArg,
                                        jsid idArg, bool* resolvedp,
                                        bool* _retval)
{
    RootedObject obj(cx, objArg);
    RootedId id(cx, idArg);

    if (!JSID_IS_STRING(id))
        return NS_OK;

    RootedString str(cx, JSID_TO_STRING(id));
    if (38 != JS_GetStringLength(str))
        return NS_OK;

    JSAutoByteString utf8str;
    if (utf8str.encodeUtf8(cx, str)) {
        nsID iid;
        if (!iid.Parse(utf8str.ptr()))
            return NS_OK;

        const nsXPTInterfaceInfo* info = nsXPTInterfaceInfo::ByIID(iid);
        if (!info)
            return NS_OK;

        nsCOMPtr<nsIJSIID> nsid = nsJSIID::NewID(info);

        if (!nsid)
            return NS_ERROR_OUT_OF_MEMORY;

        nsXPConnect* xpc = nsXPConnect::XPConnect();
        RootedObject idobj(cx);
        if (NS_SUCCEEDED(xpc->WrapNative(cx, obj,
                                         static_cast<nsIJSIID*>(nsid),
                                         NS_GET_IID(nsIJSIID),
                                         idobj.address()))) {
            if (idobj) {
                *resolvedp = true;
                *_retval =
                    JS_DefinePropertyById(cx, obj, id, idobj,
                                          JSPROP_ENUMERATE |
                                          JSPROP_READONLY |
                                          JSPROP_PERMANENT |
                                          JSPROP_RESOLVING);
            }
        }
    }
    return NS_OK;
}

/***************************************************************************/
/***************************************************************************/
/***************************************************************************/



class nsXPCComponents_Classes final :
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

private:
    virtual ~nsXPCComponents_Classes();
};

/***************************************************************************/
NS_IMETHODIMP
nsXPCComponents_Classes::GetInterfaces(uint32_t* aCount, nsIID * **aArray)
{
    *aCount = 2;
    nsIID** array = static_cast<nsIID**>(moz_xmalloc(2 * sizeof(nsIID*)));
    *aArray = array;

    array[0] = NS_GET_IID(nsIXPCComponents_Classes).Clone();
    array[1] = NS_GET_IID(nsIXPCScriptable).Clone();
    return NS_OK;
}

NS_IMETHODIMP
nsXPCComponents_Classes::GetScriptableHelper(nsIXPCScriptable** retval)
{
    *retval = nullptr;
    return NS_OK;
}

NS_IMETHODIMP
nsXPCComponents_Classes::GetContractID(nsACString& aContractID)
{
    aContractID.SetIsVoid(true);
    return NS_ERROR_NOT_AVAILABLE;
}

NS_IMETHODIMP
nsXPCComponents_Classes::GetClassDescription(nsACString& aClassDescription)
{
    aClassDescription.AssignLiteral("XPCComponents_Classes");
    return NS_OK;
}

NS_IMETHODIMP
nsXPCComponents_Classes::GetClassID(nsCID * *aClassID)
{
    *aClassID = nullptr;
    return NS_OK;
}

NS_IMETHODIMP
nsXPCComponents_Classes::GetFlags(uint32_t* aFlags)
{
    *aFlags = 0;
    return NS_OK;
}

NS_IMETHODIMP
nsXPCComponents_Classes::GetClassIDNoAlloc(nsCID* aClassIDNoAlloc)
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

NS_IMPL_ISUPPORTS(nsXPCComponents_Classes,
                  nsIXPCComponents_Classes,
                  nsIXPCScriptable,
                  nsIClassInfo)

// The nsIXPCScriptable map declaration that will generate stubs for us...
#define XPC_MAP_CLASSNAME         nsXPCComponents_Classes
#define XPC_MAP_QUOTED_CLASSNAME "nsXPCComponents_Classes"
#define XPC_MAP_FLAGS (XPC_SCRIPTABLE_WANT_RESOLVE | \
                       XPC_SCRIPTABLE_WANT_NEWENUMERATE | \
                       XPC_SCRIPTABLE_ALLOW_PROP_MODS_DURING_RESOLVE)
#include "xpc_map_end.h" /* This will #undef the above */

NS_IMETHODIMP
nsXPCComponents_Classes::NewEnumerate(nsIXPConnectWrappedNative* wrapper,
                                      JSContext* cx, JSObject* obj,
                                      JS::AutoIdVector& properties,
                                      bool* _retval)
{
    nsCOMPtr<nsIComponentRegistrar> compMgr;
    if (NS_FAILED(NS_GetComponentRegistrar(getter_AddRefs(compMgr))) || !compMgr)
        return NS_ERROR_UNEXPECTED;

    nsCOMPtr<nsISimpleEnumerator> e;
    if (NS_FAILED(compMgr->EnumerateContractIDs(getter_AddRefs(e))) || !e)
        return NS_ERROR_UNEXPECTED;

    bool hasMore;
    nsCOMPtr<nsISupports> isup;
    while(NS_SUCCEEDED(e->HasMoreElements(&hasMore)) && hasMore &&
          NS_SUCCEEDED(e->GetNext(getter_AddRefs(isup))) && isup) {
        nsCOMPtr<nsISupportsCString> holder(do_QueryInterface(isup));
        if (!holder)
            continue;

        nsAutoCString name;
        if (NS_SUCCEEDED(holder->GetData(name))) {
            RootedString idstr(cx, JS_NewStringCopyN(cx, name.get(), name.Length()));
            if (!idstr) {
                *_retval = false;
                return NS_OK;
            }

            RootedId id(cx);
            if (!JS_StringToId(cx, idstr, &id)) {
                *_retval = false;
                return NS_OK;
            }

            if (!properties.append(id)) {
                *_retval = false;
                return NS_OK;
            }
        }
    }

    return NS_OK;
}

NS_IMETHODIMP
nsXPCComponents_Classes::Resolve(nsIXPConnectWrappedNative* wrapper,
                                 JSContext* cx, JSObject* objArg,
                                 jsid idArg, bool* resolvedp,
                                 bool* _retval)

{
    RootedId id(cx, idArg);
    RootedObject obj(cx, objArg);

    JSAutoByteString name;
    if (JSID_IS_STRING(id) &&
        name.encodeLatin1(cx, JSID_TO_STRING(id)) &&
        name.ptr()[0] != '{') { // we only allow contractids here
        nsCOMPtr<nsIJSCID> nsid = nsJSCID::NewID(name.ptr());
        if (nsid) {
            nsXPConnect* xpc = nsXPConnect::XPConnect();
            RootedObject idobj(cx);
            if (NS_SUCCEEDED(xpc->WrapNative(cx, obj,
                                             static_cast<nsIJSCID*>(nsid),
                                             NS_GET_IID(nsIJSCID),
                                             idobj.address()))) {
                if (idobj) {
                    *resolvedp = true;
                    *_retval = JS_DefinePropertyById(cx, obj, id, idobj,
                                                     JSPROP_ENUMERATE |
                                                     JSPROP_READONLY |
                                                     JSPROP_PERMANENT |
                                                     JSPROP_RESOLVING);
                }
            }
        }
    }
    return NS_OK;
}

NS_IMETHODIMP
nsXPCComponents_Classes::Initialize(nsIJSCID* cid,
                                    const char* str)
{
    return cid->Initialize(str);
}


/***************************************************************************/
/***************************************************************************/
/***************************************************************************/

class nsXPCComponents_ClassesByID final :
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

private:
    virtual ~nsXPCComponents_ClassesByID();
};

/***************************************************************************/
NS_IMETHODIMP
nsXPCComponents_ClassesByID::GetInterfaces(uint32_t* aCount, nsIID * **aArray)
{
    *aCount = 2;
    nsIID** array = static_cast<nsIID**>(moz_xmalloc(2 * sizeof(nsIID*)));
    *aArray = array;

    array[0] = NS_GET_IID(nsIXPCComponents_ClassesByID).Clone();
    array[1] = NS_GET_IID(nsIXPCScriptable).Clone();
    return NS_OK;
}

NS_IMETHODIMP
nsXPCComponents_ClassesByID::GetScriptableHelper(nsIXPCScriptable** retval)
{
    *retval = nullptr;
    return NS_OK;
}

NS_IMETHODIMP
nsXPCComponents_ClassesByID::GetContractID(nsACString& aContractID)
{
    aContractID.SetIsVoid(true);
    return NS_ERROR_NOT_AVAILABLE;
}

NS_IMETHODIMP
nsXPCComponents_ClassesByID::GetClassDescription(nsACString& aClassDescription)
{
    aClassDescription.AssignLiteral("XPCComponents_ClassesByID");
    return NS_OK;
}

NS_IMETHODIMP
nsXPCComponents_ClassesByID::GetClassID(nsCID * *aClassID)
{
    *aClassID = nullptr;
    return NS_OK;
}

NS_IMETHODIMP
nsXPCComponents_ClassesByID::GetFlags(uint32_t* aFlags)
{
    *aFlags = 0;
    return NS_OK;
}

NS_IMETHODIMP
nsXPCComponents_ClassesByID::GetClassIDNoAlloc(nsCID* aClassIDNoAlloc)
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

NS_IMPL_ISUPPORTS(nsXPCComponents_ClassesByID,
                  nsIXPCComponents_ClassesByID,
                  nsIXPCScriptable,
                  nsIClassInfo)

// The nsIXPCScriptable map declaration that will generate stubs for us...
#define XPC_MAP_CLASSNAME         nsXPCComponents_ClassesByID
#define XPC_MAP_QUOTED_CLASSNAME "nsXPCComponents_ClassesByID"
#define XPC_MAP_FLAGS (XPC_SCRIPTABLE_WANT_RESOLVE | \
                       XPC_SCRIPTABLE_WANT_NEWENUMERATE | \
                       XPC_SCRIPTABLE_ALLOW_PROP_MODS_DURING_RESOLVE)
#include "xpc_map_end.h" /* This will #undef the above */

NS_IMETHODIMP
nsXPCComponents_ClassesByID::NewEnumerate(nsIXPConnectWrappedNative* wrapper,
                                          JSContext* cx, JSObject* obj,
                                          JS::AutoIdVector& properties,
                                          bool* _retval)
{

    nsCOMPtr<nsIComponentRegistrar> compMgr;
    if (NS_FAILED(NS_GetComponentRegistrar(getter_AddRefs(compMgr))) || !compMgr)
        return NS_ERROR_UNEXPECTED;

    nsISimpleEnumerator* e;
    if (NS_FAILED(compMgr->EnumerateCIDs(&e)) || !e)
        return NS_ERROR_UNEXPECTED;

    bool hasMore;
    nsCOMPtr<nsISupports> isup;
    while(NS_SUCCEEDED(e->HasMoreElements(&hasMore)) && hasMore &&
          NS_SUCCEEDED(e->GetNext(getter_AddRefs(isup))) && isup) {
        nsCOMPtr<nsISupportsID> holder(do_QueryInterface(isup));
        if (!holder)
            continue;

        char* name;
        if (NS_SUCCEEDED(holder->ToString(&name)) && name) {
            RootedString idstr(cx, JS_NewStringCopyZ(cx, name));
            if (!idstr) {
                *_retval = false;
                return NS_OK;
            }

            RootedId id(cx);
            if (!JS_StringToId(cx, idstr, &id)) {
                *_retval = false;
                return NS_OK;
            }

            if (!properties.append(id)) {
                *_retval = false;
                return NS_OK;
            }
        }
    }

    return NS_OK;
}

static bool
IsRegisteredCLSID(const char* str)
{
    bool registered;
    nsID id;

    if (!id.Parse(str))
        return false;

    nsCOMPtr<nsIComponentRegistrar> compMgr;
    if (NS_FAILED(NS_GetComponentRegistrar(getter_AddRefs(compMgr))) || !compMgr ||
        NS_FAILED(compMgr->IsCIDRegistered(id, &registered)))
        return false;

    return registered;
}

NS_IMETHODIMP
nsXPCComponents_ClassesByID::Resolve(nsIXPConnectWrappedNative* wrapper,
                                     JSContext* cx, JSObject* objArg,
                                     jsid idArg, bool* resolvedp,
                                     bool* _retval)
{
    RootedObject obj(cx, objArg);
    RootedId id(cx, idArg);

    if (!JSID_IS_STRING(id))
        return NS_OK;

    JSAutoByteString name;
    RootedString str(cx, JSID_TO_STRING(id));
    if (name.encodeLatin1(cx, str) && name.ptr()[0] == '{' &&
        IsRegisteredCLSID(name.ptr())) // we only allow canonical CLSIDs here
    {
        nsCOMPtr<nsIJSCID> nsid = nsJSCID::NewID(name.ptr());
        if (nsid) {
            nsXPConnect* xpc = nsXPConnect::XPConnect();
            RootedObject idobj(cx);
            if (NS_SUCCEEDED(xpc->WrapNative(cx, obj,
                                             static_cast<nsIJSCID*>(nsid),
                                             NS_GET_IID(nsIJSCID),
                                             idobj.address()))) {
                if (idobj) {
                    *resolvedp = true;
                    *_retval = JS_DefinePropertyById(cx, obj, id, idobj,
                                                     JSPROP_ENUMERATE |
                                                     JSPROP_READONLY |
                                                     JSPROP_PERMANENT |
                                                     JSPROP_RESOLVING);
                }
            }
        }
    }
    return NS_OK;
}


/***************************************************************************/

// Currently the possible results do not change at runtime, so they are only
// cached once (unlike ContractIDs, CLSIDs, and IIDs)

class nsXPCComponents_Results final :
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

private:
    virtual ~nsXPCComponents_Results();
};

/***************************************************************************/
NS_IMETHODIMP
nsXPCComponents_Results::GetInterfaces(uint32_t* aCount, nsIID * **aArray)
{
    *aCount = 2;
    nsIID** array = static_cast<nsIID**>(moz_xmalloc(2 * sizeof(nsIID*)));
    *aArray = array;

    array[0] = NS_GET_IID(nsIXPCComponents_Results).Clone();
    array[1] = NS_GET_IID(nsIXPCScriptable).Clone();
    return NS_OK;
}

NS_IMETHODIMP
nsXPCComponents_Results::GetScriptableHelper(nsIXPCScriptable** retval)
{
    *retval = nullptr;
    return NS_OK;
}

NS_IMETHODIMP
nsXPCComponents_Results::GetContractID(nsACString& aContractID)
{
    aContractID.SetIsVoid(true);
    return NS_ERROR_NOT_AVAILABLE;
}

NS_IMETHODIMP
nsXPCComponents_Results::GetClassDescription(nsACString& aClassDescription)
{
    aClassDescription.AssignLiteral("XPCComponents_Results");
    return NS_OK;
}

NS_IMETHODIMP
nsXPCComponents_Results::GetClassID(nsCID * *aClassID)
{
    *aClassID = nullptr;
    return NS_OK;
}

NS_IMETHODIMP
nsXPCComponents_Results::GetFlags(uint32_t* aFlags)
{
    *aFlags = 0;
    return NS_OK;
}

NS_IMETHODIMP
nsXPCComponents_Results::GetClassIDNoAlloc(nsCID* aClassIDNoAlloc)
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

NS_IMPL_ISUPPORTS(nsXPCComponents_Results,
                  nsIXPCComponents_Results,
                  nsIXPCScriptable,
                  nsIClassInfo)

// The nsIXPCScriptable map declaration that will generate stubs for us...
#define XPC_MAP_CLASSNAME         nsXPCComponents_Results
#define XPC_MAP_QUOTED_CLASSNAME "nsXPCComponents_Results"
#define XPC_MAP_FLAGS (XPC_SCRIPTABLE_WANT_RESOLVE | \
                       XPC_SCRIPTABLE_WANT_NEWENUMERATE | \
                       XPC_SCRIPTABLE_ALLOW_PROP_MODS_DURING_RESOLVE)
#include "xpc_map_end.h" /* This will #undef the above */

NS_IMETHODIMP
nsXPCComponents_Results::NewEnumerate(nsIXPConnectWrappedNative* wrapper,
                                      JSContext* cx, JSObject* obj,
                                      JS::AutoIdVector& properties,
                                      bool* _retval)
{
    const char* name;
    const void* iter = nullptr;
    while (nsXPCException::IterateNSResults(nullptr, &name, nullptr, &iter)) {
        RootedString idstr(cx, JS_NewStringCopyZ(cx, name));
        if (!idstr) {
            *_retval = false;
            return NS_OK;
        }

        RootedId id(cx);
        if (!JS_StringToId(cx, idstr, &id)) {
            *_retval = false;
            return NS_OK;
        }

        if (!properties.append(id)) {
            *_retval = false;
            return NS_OK;
        }
    }

    return NS_OK;
}

NS_IMETHODIMP
nsXPCComponents_Results::Resolve(nsIXPConnectWrappedNative* wrapper,
                                 JSContext* cx, JSObject* objArg,
                                 jsid idArg, bool* resolvedp,
                                 bool* _retval)
{
    RootedObject obj(cx, objArg);
    RootedId id(cx, idArg);
    JSAutoByteString name;

    if (JSID_IS_STRING(id) && name.encodeLatin1(cx, JSID_TO_STRING(id))) {
        const char* rv_name;
        const void* iter = nullptr;
        nsresult rv;
        while (nsXPCException::IterateNSResults(&rv, &rv_name, nullptr, &iter)) {
            if (!strcmp(name.ptr(), rv_name)) {
                *resolvedp = true;
                if (!JS_DefinePropertyById(cx, obj, id, (uint32_t)rv,
                                           JSPROP_ENUMERATE |
                                           JSPROP_READONLY |
                                           JSPROP_PERMANENT |
                                           JSPROP_RESOLVING)) {
                    return NS_ERROR_UNEXPECTED;
                }
            }
        }
    }
    return NS_OK;
}

/***************************************************************************/
// JavaScript Constructor for nsIJSID objects (Components.ID)

class nsXPCComponents_ID final :
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

private:
    virtual ~nsXPCComponents_ID();
    static nsresult CallOrConstruct(nsIXPConnectWrappedNative* wrapper,
                                    JSContext* cx, HandleObject obj,
                                    const CallArgs& args, bool* _retval);
};

/***************************************************************************/
NS_IMETHODIMP
nsXPCComponents_ID::GetInterfaces(uint32_t* aCount, nsIID * **aArray)
{
    *aCount = 2;
    nsIID** array = static_cast<nsIID**>(moz_xmalloc(2 * sizeof(nsIID*)));
    *aArray = array;

    array[0] = NS_GET_IID(nsIXPCComponents_ID).Clone();
    array[1] = NS_GET_IID(nsIXPCScriptable).Clone();
    return NS_OK;
}

NS_IMETHODIMP
nsXPCComponents_ID::GetScriptableHelper(nsIXPCScriptable** retval)
{
    *retval = nullptr;
    return NS_OK;
}

NS_IMETHODIMP
nsXPCComponents_ID::GetContractID(nsACString& aContractID)
{
    aContractID.SetIsVoid(true);
    return NS_ERROR_NOT_AVAILABLE;
}

NS_IMETHODIMP
nsXPCComponents_ID::GetClassDescription(nsACString& aClassDescription)
{
    aClassDescription.AssignLiteral("XPCComponents_ID");
    return NS_OK;
}

NS_IMETHODIMP
nsXPCComponents_ID::GetClassID(nsCID * *aClassID)
{
    *aClassID = nullptr;
    return NS_OK;
}

NS_IMETHODIMP
nsXPCComponents_ID::GetFlags(uint32_t* aFlags)
{
    *aFlags = 0;
    return NS_OK;
}

NS_IMETHODIMP
nsXPCComponents_ID::GetClassIDNoAlloc(nsCID* aClassIDNoAlloc)
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

NS_IMPL_ISUPPORTS(nsXPCComponents_ID,
                  nsIXPCComponents_ID,
                  nsIXPCScriptable,
                  nsIClassInfo)

// The nsIXPCScriptable map declaration that will generate stubs for us...
#define XPC_MAP_CLASSNAME         nsXPCComponents_ID
#define XPC_MAP_QUOTED_CLASSNAME "nsXPCComponents_ID"
#define XPC_MAP_FLAGS (XPC_SCRIPTABLE_WANT_CALL | \
                       XPC_SCRIPTABLE_WANT_CONSTRUCT | \
                       XPC_SCRIPTABLE_WANT_HASINSTANCE | \
                       XPC_SCRIPTABLE_ALLOW_PROP_MODS_DURING_RESOLVE)
#include "xpc_map_end.h" /* This will #undef the above */


NS_IMETHODIMP
nsXPCComponents_ID::Call(nsIXPConnectWrappedNative* wrapper, JSContext* cx, JSObject* objArg,
                         const CallArgs& args, bool* _retval)
{
    RootedObject obj(cx, objArg);
    return CallOrConstruct(wrapper, cx, obj, args, _retval);
}

NS_IMETHODIMP
nsXPCComponents_ID::Construct(nsIXPConnectWrappedNative* wrapper, JSContext* cx, JSObject* objArg,
                              const CallArgs& args, bool* _retval)
{
    RootedObject obj(cx, objArg);
    return CallOrConstruct(wrapper, cx, obj, args, _retval);
}

// static
nsresult
nsXPCComponents_ID::CallOrConstruct(nsIXPConnectWrappedNative* wrapper,
                                    JSContext* cx, HandleObject obj,
                                    const CallArgs& args, bool* _retval)
{
    // make sure we have at least one arg

    if (args.length() < 1)
        return ThrowAndFail(NS_ERROR_XPC_NOT_ENOUGH_ARGS, cx, _retval);

    // Do the security check if necessary

    if (NS_FAILED(nsXPConnect::SecurityManager()->CanCreateInstance(cx, nsJSID::GetCID()))) {
        // the security manager vetoed. It should have set an exception.
        *_retval = false;
        return NS_OK;
    }

    // convert the first argument into a string and see if it looks like an id

    JSString* jsstr;
    JSAutoByteString bytes;
    nsID id;

    if (!(jsstr = ToString(cx, args[0])) ||
        !bytes.encodeLatin1(cx, jsstr) ||
        !id.Parse(bytes.ptr())) {
        return ThrowAndFail(NS_ERROR_XPC_BAD_ID_STRING, cx, _retval);
    }

    // make the new object and return it.

    JSObject* newobj = xpc_NewIDObject(cx, obj, id);
    if (!newobj)
        return NS_ERROR_UNEXPECTED;

    args.rval().setObject(*newobj);
    return NS_OK;
}

NS_IMETHODIMP
nsXPCComponents_ID::HasInstance(nsIXPConnectWrappedNative* wrapper,
                                JSContext* cx, JSObject* obj,
                                HandleValue val, bool* bp, bool* _retval)
{
    if (bp)
        *bp = JSValIsInterfaceOfType(cx, val, NS_GET_IID(nsIJSID));
    return NS_OK;
}

/***************************************************************************/
// JavaScript Constructor for Exception objects (Components.Exception)

class nsXPCComponents_Exception final :
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

private:
    virtual ~nsXPCComponents_Exception();
    static nsresult CallOrConstruct(nsIXPConnectWrappedNative* wrapper,
                                    JSContext* cx, HandleObject obj,
                                    const CallArgs& args, bool* _retval);
};

/***************************************************************************/
NS_IMETHODIMP
nsXPCComponents_Exception::GetInterfaces(uint32_t* aCount, nsIID * **aArray)
{
    *aCount = 2;
    nsIID** array = static_cast<nsIID**>(moz_xmalloc(2 * sizeof(nsIID*)));
    *aArray = array;

    array[0] = NS_GET_IID(nsIXPCComponents_Exception).Clone();
    array[1] = NS_GET_IID(nsIXPCScriptable).Clone();
    return NS_OK;
}

NS_IMETHODIMP
nsXPCComponents_Exception::GetScriptableHelper(nsIXPCScriptable** retval)
{
    *retval = nullptr;
    return NS_OK;
}

NS_IMETHODIMP
nsXPCComponents_Exception::GetContractID(nsACString& aContractID)
{
    aContractID.SetIsVoid(true);
    return NS_ERROR_NOT_AVAILABLE;
}

NS_IMETHODIMP
nsXPCComponents_Exception::GetClassDescription(nsACString& aClassDescription)
{
    aClassDescription.AssignLiteral("XPCComponents_Exception");
    return NS_OK;
}

NS_IMETHODIMP
nsXPCComponents_Exception::GetClassID(nsCID * *aClassID)
{
    *aClassID = nullptr;
    return NS_OK;
}

NS_IMETHODIMP
nsXPCComponents_Exception::GetFlags(uint32_t* aFlags)
{
    *aFlags = 0;
    return NS_OK;
}

NS_IMETHODIMP
nsXPCComponents_Exception::GetClassIDNoAlloc(nsCID* aClassIDNoAlloc)
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

NS_IMPL_ISUPPORTS(nsXPCComponents_Exception,
                  nsIXPCComponents_Exception,
                  nsIXPCScriptable,
                  nsIClassInfo)

// The nsIXPCScriptable map declaration that will generate stubs for us...
#define XPC_MAP_CLASSNAME         nsXPCComponents_Exception
#define XPC_MAP_QUOTED_CLASSNAME "nsXPCComponents_Exception"
#define XPC_MAP_FLAGS (XPC_SCRIPTABLE_WANT_CALL | \
                       XPC_SCRIPTABLE_WANT_CONSTRUCT | \
                       XPC_SCRIPTABLE_WANT_HASINSTANCE | \
                       XPC_SCRIPTABLE_ALLOW_PROP_MODS_DURING_RESOLVE)
#include "xpc_map_end.h" /* This will #undef the above */


NS_IMETHODIMP
nsXPCComponents_Exception::Call(nsIXPConnectWrappedNative* wrapper, JSContext* cx, JSObject* objArg,
                                const CallArgs& args, bool* _retval)
{
    RootedObject obj(cx, objArg);
    return CallOrConstruct(wrapper, cx, obj, args, _retval);
}

NS_IMETHODIMP
nsXPCComponents_Exception::Construct(nsIXPConnectWrappedNative* wrapper, JSContext* cx,
                                     JSObject* objArg, const CallArgs& args, bool* _retval)
{
    RootedObject obj(cx, objArg);
    return CallOrConstruct(wrapper, cx, obj, args, _retval);
}

struct MOZ_STACK_CLASS ExceptionArgParser
{
    ExceptionArgParser(JSContext* context,
                       nsXPConnect* xpconnect)
        : eMsg("exception")
        , eResult(NS_ERROR_FAILURE)
        , cx(context)
        , xpc(xpconnect)
    {}

    // Public exception parameter values. During construction, these are
    // initialized to the appropriate defaults.
    const char*             eMsg;
    nsresult                eResult;
    nsCOMPtr<nsIStackFrame> eStack;
    nsCOMPtr<nsISupports>   eData;

    // Parse the constructor arguments into the above |eFoo| parameter values.
    bool parse(const CallArgs& args) {
        /*
         * The Components.Exception takes a series of arguments, all of them
         * optional:
         *
         * Argument 0: Exception message (defaults to 'exception').
         * Argument 1: Result code (defaults to NS_ERROR_FAILURE) _or_ options
         *             object (see below).
         * Argument 2: Stack (defaults to the current stack, which we trigger
         *                    by leaving this nullptr in the parser).
         * Argument 3: Optional user data (defaults to nullptr).
         *
         * To dig our way out of this clunky API, we now support passing an
         * options object as the second parameter (as opposed to a result code).
         * If this is the case, all subsequent arguments are ignored, and the
         * following properties are parsed out of the object (using the
         * associated default if the property does not exist):
         *
         *   result:    Result code (see argument 1).
         *   stack:     Call stack (see argument 2).
         *   data:      User data (see argument 3).
         */
        if (args.length() > 0 && !parseMessage(args[0]))
            return false;
        if (args.length() > 1) {
            if (args[1].isObject()) {
                RootedObject obj(cx, &args[1].toObject());
                return parseOptionsObject(obj);
            }
            if (!parseResult(args[1]))
                return false;
        }
        if (args.length() > 2) {
            if (!parseStack(args[2]))
                return false;
        }
        if (args.length() > 3) {
            if (!parseData(args[3]))
                return false;
        }
        return true;
    }

  protected:

    /*
     * Parsing helpers.
     */

    bool parseMessage(HandleValue v) {
        JSString* str = ToString(cx, v);
        if (!str)
           return false;
        eMsg = messageBytes.encodeLatin1(cx, str);
        return !!eMsg;
    }

    bool parseResult(HandleValue v) {
        return JS::ToUint32(cx, v, (uint32_t*) &eResult);
    }

    bool parseStack(HandleValue v) {
        if (!v.isObject()) {
            // eStack has already been initialized to null, which is what we want
            // for any non-object values (including null).
            return true;
        }

        return NS_SUCCEEDED(xpc->WrapJS(cx, &v.toObject(),
                                        NS_GET_IID(nsIStackFrame),
                                        getter_AddRefs(eStack)));
    }

    bool parseData(HandleValue v) {
        if (!v.isObject()) {
            // eData has already been initialized to null, which is what we want
            // for any non-object values (including null).
            return true;
        }

        return NS_SUCCEEDED(xpc->WrapJS(cx, &v.toObject(),
                                        NS_GET_IID(nsISupports),
                                        getter_AddRefs(eData)));
    }

    bool parseOptionsObject(HandleObject obj) {
        RootedValue v(cx);

        if (!getOption(obj, "result", &v) ||
            (!v.isUndefined() && !parseResult(v)))
            return false;

        if (!getOption(obj, "stack", &v) ||
            (!v.isUndefined() && !parseStack(v)))
            return false;

        if (!getOption(obj, "data", &v) ||
            (!v.isUndefined() && !parseData(v)))
            return false;

        return true;
    }

    bool getOption(HandleObject obj, const char* name, MutableHandleValue rv) {
        // Look for the property.
        bool found;
        if (!JS_HasProperty(cx, obj, name, &found))
            return false;

        // If it wasn't found, indicate with undefined.
        if (!found) {
            rv.setUndefined();
            return true;
        }

        // Get the property.
        return JS_GetProperty(cx, obj, name, rv);
    }

    /*
     * Internal data members.
     */

    // If there's a non-default exception string, hold onto the allocated bytes.
    JSAutoByteString messageBytes;

    // Various bits and pieces that are helpful to have around.
    JSContext* cx;
    nsXPConnect* xpc;
};

// static
nsresult
nsXPCComponents_Exception::CallOrConstruct(nsIXPConnectWrappedNative* wrapper,
                                           JSContext* cx, HandleObject obj,
                                           const CallArgs& args, bool* _retval)
{
    nsXPConnect* xpc = nsXPConnect::XPConnect();

    MOZ_DIAGNOSTIC_ASSERT(nsContentUtils::IsCallerChrome());

    // Parse the arguments to the Exception constructor.
    ExceptionArgParser parser(cx, xpc);
    if (!parser.parse(args))
        return ThrowAndFail(NS_ERROR_XPC_BAD_CONVERT_JS, cx, _retval);

    RefPtr<Exception> e = new Exception(nsCString(parser.eMsg),
                                        parser.eResult,
                                        EmptyCString(),
                                        parser.eStack,
                                        parser.eData);

    RootedObject newObj(cx);
    if (NS_FAILED(xpc->WrapNative(cx, obj, e, NS_GET_IID(nsIException), newObj.address())) || !newObj) {
        return ThrowAndFail(NS_ERROR_XPC_CANT_CREATE_WN, cx, _retval);
    }

    args.rval().setObject(*newObj);
    return NS_OK;
}

NS_IMETHODIMP
nsXPCComponents_Exception::HasInstance(nsIXPConnectWrappedNative* wrapper,
                                       JSContext * cx, JSObject * obj,
                                       HandleValue val, bool* bp,
                                       bool* _retval)
{
    using namespace mozilla::dom;

    if (bp) {
        *bp = (val.isObject() &&
               IS_INSTANCE_OF(Exception, &val.toObject())) ||
              JSValIsInterfaceOfType(cx, val, NS_GET_IID(nsIException));
    }
    return NS_OK;
}

/***************************************************************************/
// This class is for the thing returned by "new Component.Constructor".

// XXXjband we use this CID for security check, but security system can't see
// it since it has no registed factory. Security really kicks in when we try
// to build a wrapper around an instance.

// {B4A95150-E25A-11d3-8F61-0010A4E73D9A}
#define NS_XPCCONSTRUCTOR_CID                                                 \
{ 0xb4a95150, 0xe25a, 0x11d3,                                                 \
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
    nsXPCConstructor() = delete;
    nsXPCConstructor(nsIJSCID* aClassID,
                     nsIJSIID* aInterfaceID,
                     const char* aInitializer);

private:
    virtual ~nsXPCConstructor();
    nsresult CallOrConstruct(nsIXPConnectWrappedNative* wrapper,
                             JSContext* cx, HandleObject obj,
                             const CallArgs& args, bool* _retval);
private:
    RefPtr<nsIJSCID> mClassID;
    RefPtr<nsIJSIID> mInterfaceID;
    char*              mInitializer;
};

/***************************************************************************/
NS_IMETHODIMP
nsXPCConstructor::GetInterfaces(uint32_t* aCount, nsIID * **aArray)
{
    *aCount = 2;
    nsIID** array = static_cast<nsIID**>(moz_xmalloc(2 * sizeof(nsIID*)));
    *aArray = array;

    array[0] = NS_GET_IID(nsIXPCConstructor).Clone();
    array[1] = NS_GET_IID(nsIXPCScriptable).Clone();
    return NS_OK;
}

NS_IMETHODIMP
nsXPCConstructor::GetScriptableHelper(nsIXPCScriptable** retval)
{
    *retval = nullptr;
    return NS_OK;
}

NS_IMETHODIMP
nsXPCConstructor::GetContractID(nsACString& aContractID)
{
    aContractID.SetIsVoid(true);
    return NS_ERROR_NOT_AVAILABLE;
}

NS_IMETHODIMP
nsXPCConstructor::GetClassDescription(nsACString& aClassDescription)
{
    aClassDescription.AssignLiteral("XPCConstructor");
    return NS_OK;
}

NS_IMETHODIMP
nsXPCConstructor::GetClassID(nsCID * *aClassID)
{
    *aClassID = nullptr;
    return NS_OK;
}

NS_IMETHODIMP
nsXPCConstructor::GetFlags(uint32_t* aFlags)
{
    *aFlags = 0;
    return NS_OK;
}

NS_IMETHODIMP
nsXPCConstructor::GetClassIDNoAlloc(nsCID* aClassIDNoAlloc)
{
    return NS_ERROR_NOT_AVAILABLE;
}

nsXPCConstructor::nsXPCConstructor(nsIJSCID* aClassID,
                                   nsIJSIID* aInterfaceID,
                                   const char* aInitializer)
    : mClassID(aClassID),
      mInterfaceID(aInterfaceID)
{
    mInitializer = aInitializer ? moz_xstrdup(aInitializer) : nullptr;
}

nsXPCConstructor::~nsXPCConstructor()
{
    if (mInitializer)
        free(mInitializer);
}

NS_IMETHODIMP
nsXPCConstructor::GetClassID(nsIJSCID * *aClassID)
{
    RefPtr<nsIJSCID> rval = mClassID;
    rval.forget(aClassID);
    return NS_OK;
}

NS_IMETHODIMP
nsXPCConstructor::GetInterfaceID(nsIJSIID * *aInterfaceID)
{
    RefPtr<nsIJSIID> rval = mInterfaceID;
    rval.forget(aInterfaceID);
    return NS_OK;
}

NS_IMETHODIMP
nsXPCConstructor::GetInitializer(char * *aInitializer)
{
    XPC_STRING_GETTER_BODY(aInitializer, mInitializer);
}

NS_IMPL_ISUPPORTS(nsXPCConstructor,
                  nsIXPCConstructor,
                  nsIXPCScriptable,
                  nsIClassInfo)

// The nsIXPCScriptable map declaration that will generate stubs for us...
#define XPC_MAP_CLASSNAME         nsXPCConstructor
#define XPC_MAP_QUOTED_CLASSNAME "nsXPCConstructor"
#define XPC_MAP_FLAGS (XPC_SCRIPTABLE_WANT_CALL | \
                       XPC_SCRIPTABLE_WANT_CONSTRUCT)
#include "xpc_map_end.h" /* This will #undef the above */


NS_IMETHODIMP
nsXPCConstructor::Call(nsIXPConnectWrappedNative* wrapper, JSContext* cx, JSObject* objArg,
                       const CallArgs& args, bool* _retval)
{
    RootedObject obj(cx, objArg);
    return CallOrConstruct(wrapper, cx, obj, args, _retval);

}

NS_IMETHODIMP
nsXPCConstructor::Construct(nsIXPConnectWrappedNative* wrapper, JSContext* cx, JSObject* objArg,
                            const CallArgs& args, bool* _retval)
{
    RootedObject obj(cx, objArg);
    return CallOrConstruct(wrapper, cx, obj, args, _retval);
}

// static
nsresult
nsXPCConstructor::CallOrConstruct(nsIXPConnectWrappedNative* wrapper,JSContext* cx,
                                  HandleObject obj, const CallArgs& args, bool* _retval)
{
    nsXPConnect* xpc = nsXPConnect::XPConnect();

    // security check not required because we are going to call through the
    // code which is reflected into JS which will do that for us later.

    RootedObject cidObj(cx);
    RootedObject iidObj(cx);

    if (NS_FAILED(xpc->WrapNative(cx, obj, mClassID, NS_GET_IID(nsIJSCID), cidObj.address())) || !cidObj ||
        NS_FAILED(xpc->WrapNative(cx, obj, mInterfaceID, NS_GET_IID(nsIJSIID), iidObj.address())) || !iidObj) {
        return ThrowAndFail(NS_ERROR_XPC_CANT_CREATE_WN, cx, _retval);
    }

    JS::Rooted<JS::Value> arg(cx, ObjectValue(*iidObj));
    RootedValue rval(cx);
    if (!JS_CallFunctionName(cx, cidObj, "createInstance", JS::HandleValueArray(arg), &rval) ||
        rval.isPrimitive()) {
        // createInstance will have thrown an exception
        *_retval = false;
        return NS_OK;
    }

    args.rval().set(rval);

    // call initializer method if supplied
    if (mInitializer) {
        RootedObject newObj(cx, &rval.toObject());
        // first check existence of function property for better error reporting
        RootedValue fun(cx);
        if (!JS_GetProperty(cx, newObj, mInitializer, &fun) ||
            fun.isPrimitive()) {
            return ThrowAndFail(NS_ERROR_XPC_BAD_INITIALIZER_NAME, cx, _retval);
        }

        RootedValue dummy(cx);
        if (!JS_CallFunctionValue(cx, newObj, fun, args, &dummy)) {
            // function should have thrown an exception
            *_retval = false;
            return NS_OK;
        }
    }

    return NS_OK;
}

/*******************************************************/
// JavaScript Constructor for nsIXPCConstructor objects (Components.Constructor)

class nsXPCComponents_Constructor final :
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

private:
    virtual ~nsXPCComponents_Constructor();
    static nsresult CallOrConstruct(nsIXPConnectWrappedNative* wrapper,
                                    JSContext* cx, HandleObject obj,
                                    const CallArgs& args, bool* _retval);
};

/***************************************************************************/
NS_IMETHODIMP
nsXPCComponents_Constructor::GetInterfaces(uint32_t* aCount, nsIID * **aArray)
{
    *aCount = 2;
    nsIID** array = static_cast<nsIID**>(moz_xmalloc(2 * sizeof(nsIID*)));
    *aArray = array;

    array[0] = NS_GET_IID(nsIXPCComponents_Constructor).Clone();
    array[1] = NS_GET_IID(nsIXPCScriptable).Clone();
    return NS_OK;
}

NS_IMETHODIMP
nsXPCComponents_Constructor::GetScriptableHelper(nsIXPCScriptable** retval)
{
    *retval = nullptr;
    return NS_OK;
}

NS_IMETHODIMP
nsXPCComponents_Constructor::GetContractID(nsACString& aContractID)
{
    aContractID.SetIsVoid(true);
    return NS_ERROR_NOT_AVAILABLE;
}

NS_IMETHODIMP
nsXPCComponents_Constructor::GetClassDescription(nsACString& aClassDescription)
{
    aClassDescription.AssignLiteral("XPCComponents_Constructor");
    return NS_OK;
}

NS_IMETHODIMP
nsXPCComponents_Constructor::GetClassID(nsCID * *aClassID)
{
    *aClassID = nullptr;
    return NS_OK;
}

NS_IMETHODIMP
nsXPCComponents_Constructor::GetFlags(uint32_t* aFlags)
{
    *aFlags = 0;
    return NS_OK;
}

NS_IMETHODIMP
nsXPCComponents_Constructor::GetClassIDNoAlloc(nsCID* aClassIDNoAlloc)
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

NS_IMPL_ISUPPORTS(nsXPCComponents_Constructor,
                  nsIXPCComponents_Constructor,
                  nsIXPCScriptable,
                  nsIClassInfo)

// The nsIXPCScriptable map declaration that will generate stubs for us...
#define XPC_MAP_CLASSNAME         nsXPCComponents_Constructor
#define XPC_MAP_QUOTED_CLASSNAME "nsXPCComponents_Constructor"
#define XPC_MAP_FLAGS (XPC_SCRIPTABLE_WANT_CALL | \
                       XPC_SCRIPTABLE_WANT_CONSTRUCT | \
                       XPC_SCRIPTABLE_WANT_HASINSTANCE | \
                       XPC_SCRIPTABLE_ALLOW_PROP_MODS_DURING_RESOLVE)
#include "xpc_map_end.h" /* This will #undef the above */


NS_IMETHODIMP
nsXPCComponents_Constructor::Call(nsIXPConnectWrappedNative* wrapper, JSContext* cx,
                                  JSObject* objArg, const CallArgs& args, bool* _retval)
{
    RootedObject obj(cx, objArg);
    return CallOrConstruct(wrapper, cx, obj, args, _retval);
}

NS_IMETHODIMP
nsXPCComponents_Constructor::Construct(nsIXPConnectWrappedNative* wrapper, JSContext* cx,
                                       JSObject* objArg, const CallArgs& args, bool* _retval)
{
    RootedObject obj(cx, objArg);
    return CallOrConstruct(wrapper, cx, obj, args, _retval);
}

// static
nsresult
nsXPCComponents_Constructor::CallOrConstruct(nsIXPConnectWrappedNative* wrapper,
                                             JSContext* cx, HandleObject obj,
                                             const CallArgs& args, bool* _retval)
{
    // make sure we have at least one arg

    if (args.length() < 1)
        return ThrowAndFail(NS_ERROR_XPC_NOT_ENOUGH_ARGS, cx, _retval);

    // get the various other object pointers we need

    nsXPConnect* xpc = nsXPConnect::XPConnect();
    XPCWrappedNativeScope* scope = ObjectScope(obj);
    nsCOMPtr<nsIXPCComponents> comp;

    if (!xpc || !scope || !(comp = do_QueryInterface(scope->GetComponents())))
        return ThrowAndFail(NS_ERROR_XPC_UNEXPECTED, cx, _retval);

    // Do the security check if necessary

    if (NS_FAILED(nsXPConnect::SecurityManager()->CanCreateInstance(cx, nsXPCConstructor::GetCID()))) {
        // the security manager vetoed. It should have set an exception.
        *_retval = false;
        return NS_OK;
    }

    // initialization params for the Constructor object we will create
    nsCOMPtr<nsIJSCID> cClassID;
    nsCOMPtr<nsIJSIID> cInterfaceID;
    const char*        cInitializer = nullptr;
    JSAutoByteString  cInitializerBytes;

    if (args.length() >= 3) {
        // args[2] is an initializer function or property name
        RootedString str(cx, ToString(cx, args[2]));
        if (!str || !(cInitializer = cInitializerBytes.encodeLatin1(cx, str)))
            return ThrowAndFail(NS_ERROR_XPC_BAD_CONVERT_JS, cx, _retval);
    }

    if (args.length() >= 2) {
        // args[1] is an iid name string
        // XXXjband support passing "Components.interfaces.foo"?

        nsCOMPtr<nsIXPCComponents_Interfaces> ifaces;
        RootedObject ifacesObj(cx);

        // we do the lookup by asking the Components.interfaces object
        // for the property with this name - i.e. we let its caching of these
        // nsIJSIID objects work for us.

        if (NS_FAILED(comp->GetInterfaces(getter_AddRefs(ifaces))) ||
            NS_FAILED(xpc->WrapNative(cx, obj, ifaces,
                                      NS_GET_IID(nsIXPCComponents_Interfaces),
                                      ifacesObj.address())) || !ifacesObj) {
            return ThrowAndFail(NS_ERROR_XPC_UNEXPECTED, cx, _retval);
        }

        RootedString str(cx, ToString(cx, args[1]));
        RootedId id(cx);
        if (!str || !JS_StringToId(cx, str, &id))
            return ThrowAndFail(NS_ERROR_XPC_BAD_CONVERT_JS, cx, _retval);

        RootedValue val(cx);
        if (!JS_GetPropertyById(cx, ifacesObj, id, &val) || val.isPrimitive())
            return ThrowAndFail(NS_ERROR_XPC_BAD_IID, cx, _retval);

        nsCOMPtr<nsIXPConnectWrappedNative> wn;
        if (NS_FAILED(xpc->GetWrappedNativeOfJSObject(cx, &val.toObject(),
                                                      getter_AddRefs(wn))) || !wn ||
            !(cInterfaceID = do_QueryWrappedNative(wn))) {
            return ThrowAndFail(NS_ERROR_XPC_UNEXPECTED, cx, _retval);
        }
    } else {
        const nsXPTInterfaceInfo* info =
            nsXPTInterfaceInfo::ByIID(NS_GET_IID(nsISupports));

        if (info) {
            cInterfaceID = nsJSIID::NewID(info);
        }
        if (!cInterfaceID)
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
        RootedObject classesObj(cx);

        if (NS_FAILED(comp->GetClasses(getter_AddRefs(classes))) ||
            NS_FAILED(xpc->WrapNative(cx, obj, classes,
                                      NS_GET_IID(nsIXPCComponents_Classes),
                                      classesObj.address())) || !classesObj) {
            return ThrowAndFail(NS_ERROR_XPC_UNEXPECTED, cx, _retval);
        }

        RootedString str(cx, ToString(cx, args[0]));
        RootedId id(cx);
        if (!str || !JS_StringToId(cx, str, &id))
            return ThrowAndFail(NS_ERROR_XPC_BAD_CONVERT_JS, cx, _retval);

        RootedValue val(cx);
        if (!JS_GetPropertyById(cx, classesObj, id, &val) || val.isPrimitive())
            return ThrowAndFail(NS_ERROR_XPC_BAD_CID, cx, _retval);

        nsCOMPtr<nsIXPConnectWrappedNative> wn;
        if (NS_FAILED(xpc->GetWrappedNativeOfJSObject(cx, val.toObjectOrNull(),
                                                      getter_AddRefs(wn))) || !wn ||
            !(cClassID = do_QueryWrappedNative(wn))) {
            return ThrowAndFail(NS_ERROR_XPC_UNEXPECTED, cx, _retval);
        }
    }

    nsCOMPtr<nsIXPCConstructor> ctor = new nsXPCConstructor(cClassID, cInterfaceID, cInitializer);
    RootedObject newObj(cx);

    if (NS_FAILED(xpc->WrapNative(cx, obj, ctor, NS_GET_IID(nsIXPCConstructor), newObj.address())) || !newObj) {
        return ThrowAndFail(NS_ERROR_XPC_CANT_CREATE_WN, cx, _retval);
    }

    args.rval().setObject(*newObj);
    return NS_OK;
}

NS_IMETHODIMP
nsXPCComponents_Constructor::HasInstance(nsIXPConnectWrappedNative* wrapper,
                                         JSContext * cx, JSObject * obj,
                                         HandleValue val, bool* bp,
                                         bool* _retval)
{
    if (bp)
        *bp = JSValIsInterfaceOfType(cx, val, NS_GET_IID(nsIXPCConstructor));
    return NS_OK;
}

class nsXPCComponents_Utils final :
            public nsIXPCComponents_Utils,
            public nsIXPCScriptable
{
public:
    // all the interface method declarations...
    NS_DECL_ISUPPORTS
    NS_DECL_NSIXPCSCRIPTABLE
    NS_DECL_NSIXPCCOMPONENTS_UTILS

public:
    nsXPCComponents_Utils() { }

private:
    virtual ~nsXPCComponents_Utils() { }
    nsCOMPtr<nsIXPCComponents_utils_Sandbox> mSandbox;
};

NS_IMPL_ISUPPORTS(nsXPCComponents_Utils,
                  nsIXPCComponents_Utils,
                  nsIXPCScriptable)

// The nsIXPCScriptable map declaration that will generate stubs for us...
#define XPC_MAP_CLASSNAME         nsXPCComponents_Utils
#define XPC_MAP_QUOTED_CLASSNAME "nsXPCComponents_Utils"
#define XPC_MAP_FLAGS XPC_SCRIPTABLE_ALLOW_PROP_MODS_DURING_RESOLVE
#include "xpc_map_end.h" /* This will #undef the above */

NS_IMETHODIMP
nsXPCComponents_Utils::GetSandbox(nsIXPCComponents_utils_Sandbox** aSandbox)
{
    NS_ENSURE_ARG_POINTER(aSandbox);
    if (!mSandbox)
        mSandbox = NewSandboxConstructor();

    nsCOMPtr<nsIXPCComponents_utils_Sandbox> rval = mSandbox;
    rval.forget(aSandbox);
    return NS_OK;
}

NS_IMETHODIMP
nsXPCComponents_Utils::ReportError(HandleValue error, HandleValue stack, JSContext* cx)
{
    // This function shall never fail! Silently eat any failure conditions.

    nsCOMPtr<nsIConsoleService> console(do_GetService(NS_CONSOLESERVICE_CONTRACTID));
    if (!console)
        return NS_OK;

    nsGlobalWindowInner* globalWin = CurrentWindowOrNull(cx);
    nsPIDOMWindowInner* win = globalWin ? globalWin->AsInner() : nullptr;
    const uint64_t innerWindowID = win ? win->WindowID() : 0;

    RootedObject errorObj(cx, error.isObject() ? &error.toObject() : nullptr);
    JSErrorReport* err = errorObj ? JS_ErrorFromException(cx, errorObj) : nullptr;

    nsCOMPtr<nsIScriptError> scripterr;

    if (errorObj) {
        JS::RootedObject stackVal(cx,
          FindExceptionStackForConsoleReport(win, error));
        if (stackVal) {
            scripterr = new nsScriptErrorWithStack(stackVal);
        }
    }

    nsString fileName;
    uint32_t lineNo = 0;

    if (!scripterr) {
        RootedObject stackObj(cx);
        if (stack.isObject()) {
            if (!JS::IsSavedFrame(&stack.toObject())) {
                return NS_ERROR_INVALID_ARG;
            }

            stackObj = &stack.toObject();

            if (GetSavedFrameLine(cx, stackObj, &lineNo) != SavedFrameResult::Ok) {
                JS_ClearPendingException(cx);
            }

            RootedString source(cx);
            nsAutoJSString str;
            if (GetSavedFrameSource(cx, stackObj, &source) == SavedFrameResult::Ok &&
                str.init(cx, source)) {
                fileName = str;
            } else {
                JS_ClearPendingException(cx);
            }
        } else {
            nsCOMPtr<nsIStackFrame> frame = dom::GetCurrentJSStack();
            if (frame) {
                frame->GetFilename(cx, fileName);
                lineNo = frame->GetLineNumber(cx);
                JS::Rooted<JS::Value> stack(cx);
                nsresult rv = frame->GetNativeSavedFrame(&stack);
                if (NS_SUCCEEDED(rv) && stack.isObject()) {
                  stackObj = &stack.toObject();
                }
            }
        }

        if (stackObj) {
            scripterr = new nsScriptErrorWithStack(stackObj);
        }
    }

    if (!scripterr) {
        scripterr = new nsScriptError();
    }

    if (err) {
        // It's a proper JS Error
        nsAutoString fileUni;
        CopyUTF8toUTF16(err->filename, fileUni);

        uint32_t column = err->tokenOffset();

        const char16_t* linebuf = err->linebuf();

        nsresult rv = scripterr->InitWithWindowID(
                err->message() ? NS_ConvertUTF8toUTF16(err->message().c_str())
                : EmptyString(),
                fileUni,
                linebuf ? nsDependentString(linebuf, err->linebufLength()) : EmptyString(),
                err->lineno,
                column, err->flags, "XPConnect JavaScript", innerWindowID);
        NS_ENSURE_SUCCESS(rv, NS_OK);

        console->LogMessage(scripterr);
        return NS_OK;
    }

    // It's not a JS Error object, so we synthesize as best we're able.
    RootedString msgstr(cx, ToString(cx, error));
    if (!msgstr)
        return NS_OK;

    nsAutoJSString msg;
    if (!msg.init(cx, msgstr))
        return NS_OK;

    nsresult rv = scripterr->InitWithWindowID(
            msg, fileName, EmptyString(), lineNo, 0, 0,
            "XPConnect JavaScript", innerWindowID);
    NS_ENSURE_SUCCESS(rv, NS_OK);

    console->LogMessage(scripterr);
    return NS_OK;
}

NS_IMETHODIMP
nsXPCComponents_Utils::EvalInSandbox(const nsAString& source,
                                     HandleValue sandboxVal,
                                     HandleValue version,
                                     const nsACString& filenameArg,
                                     int32_t lineNumber,
                                     JSContext* cx,
                                     uint8_t optionalArgc,
                                     MutableHandleValue retval)
{
    RootedObject sandbox(cx);
    if (!JS_ValueToObject(cx, sandboxVal, &sandbox) || !sandbox)
        return NS_ERROR_INVALID_ARG;

    // Optional third argument: JS version, as a string, is unused.

    // Optional fourth and fifth arguments: filename and line number.
    int32_t lineNo = (optionalArgc >= 3) ? lineNumber : 1;
    nsCString filename;
    if (!filenameArg.IsVoid()) {
        filename.Assign(filenameArg);
    } else {
        // Get the current source info.
        nsCOMPtr<nsIStackFrame> frame = dom::GetCurrentJSStack();
        if (frame) {
            nsString frameFile;
            frame->GetFilename(cx, frameFile);
            CopyUTF16toUTF8(frameFile, filename);
            lineNo = frame->GetLineNumber(cx);
        }
    }

    return xpc::EvalInSandbox(cx, sandbox, source, filename, lineNo, retval);
}

NS_IMETHODIMP
nsXPCComponents_Utils::GetSandboxMetadata(HandleValue sandboxVal,
                                          JSContext* cx, MutableHandleValue rval)
{
    if (!sandboxVal.isObject())
        return NS_ERROR_INVALID_ARG;

    RootedObject sandbox(cx, &sandboxVal.toObject());
    sandbox = js::CheckedUnwrap(sandbox);
    if (!sandbox || !xpc::IsSandbox(sandbox))
        return NS_ERROR_INVALID_ARG;

    return xpc::GetSandboxMetadata(cx, sandbox, rval);
}

NS_IMETHODIMP
nsXPCComponents_Utils::SetSandboxMetadata(HandleValue sandboxVal,
                                          HandleValue metadataVal,
                                          JSContext* cx)
{
    if (!sandboxVal.isObject())
        return NS_ERROR_INVALID_ARG;

    RootedObject sandbox(cx, &sandboxVal.toObject());
    sandbox = js::CheckedUnwrap(sandbox);
    if (!sandbox || !xpc::IsSandbox(sandbox))
        return NS_ERROR_INVALID_ARG;

    nsresult rv = xpc::SetSandboxMetadata(cx, sandbox, metadataVal);
    NS_ENSURE_SUCCESS(rv, rv);

    return NS_OK;
}

NS_IMETHODIMP
nsXPCComponents_Utils::Import(const nsACString& registryLocation,
                              HandleValue targetObj,
                              JSContext* cx,
                              uint8_t optionalArgc,
                              MutableHandleValue retval)
{
    RefPtr<mozJSComponentLoader> moduleloader = mozJSComponentLoader::Get();
    MOZ_ASSERT(moduleloader);

    AUTO_PROFILER_LABEL_DYNAMIC_NSCSTRING(
      "nsXPCComponents_Utils::Import", OTHER, registryLocation);

    return moduleloader->ImportInto(registryLocation, targetObj, cx, optionalArgc, retval);
}

NS_IMETHODIMP
nsXPCComponents_Utils::IsModuleLoaded(const nsACString& registryLocation, bool* retval)
{
    RefPtr<mozJSComponentLoader> moduleloader = mozJSComponentLoader::Get();
    MOZ_ASSERT(moduleloader);
    return moduleloader->IsModuleLoaded(registryLocation, retval);
}

NS_IMETHODIMP
nsXPCComponents_Utils::Unload(const nsACString & registryLocation)
{
    RefPtr<mozJSComponentLoader> moduleloader = mozJSComponentLoader::Get();
    MOZ_ASSERT(moduleloader);
    return moduleloader->Unload(registryLocation);
}

NS_IMETHODIMP
nsXPCComponents_Utils::ImportGlobalProperties(HandleValue aPropertyList,
                                              JSContext* cx)
{
    RootedObject global(cx, CurrentGlobalOrNull(cx));
    MOZ_ASSERT(global);

    // Don't allow doing this if the global is a Window
    nsGlobalWindowInner* win;
    if (NS_SUCCEEDED(UNWRAP_OBJECT(Window, &global, win))) {
        return NS_ERROR_NOT_AVAILABLE;
    }

    GlobalProperties options;
    NS_ENSURE_TRUE(aPropertyList.isObject(), NS_ERROR_INVALID_ARG);

    RootedObject propertyList(cx, &aPropertyList.toObject());
    bool isArray;
    if (NS_WARN_IF(!JS_IsArrayObject(cx, propertyList, &isArray))) {
        return NS_ERROR_FAILURE;
    }
    if (NS_WARN_IF(!isArray)) {
        return NS_ERROR_INVALID_ARG;
    }

    if (!options.Parse(cx, propertyList) ||
        !options.DefineInXPCComponents(cx, global))
    {
        return NS_ERROR_FAILURE;
    }

    return NS_OK;
}

NS_IMETHODIMP
nsXPCComponents_Utils::GetWeakReference(HandleValue object, JSContext* cx,
                                        xpcIJSWeakReference** _retval)
{
    RefPtr<xpcJSWeakReference> ref = new xpcJSWeakReference();
    nsresult rv = ref->Init(cx, object);
    NS_ENSURE_SUCCESS(rv, rv);
    ref.forget(_retval);
    return NS_OK;
}

NS_IMETHODIMP
nsXPCComponents_Utils::ForceGC()
{
    JSContext* cx = XPCJSContext::Get()->Context();
    PrepareForFullGC(cx);
    NonIncrementalGC(cx, GC_NORMAL, gcreason::COMPONENT_UTILS);
    return NS_OK;
}

NS_IMETHODIMP
nsXPCComponents_Utils::ForceCC(nsICycleCollectorListener* listener)
{
    nsJSContext::CycleCollectNow(listener);
    return NS_OK;
}

NS_IMETHODIMP
nsXPCComponents_Utils::FinishCC()
{
    nsCycleCollector_finishAnyCurrentCollection();
    return NS_OK;
}

NS_IMETHODIMP
nsXPCComponents_Utils::CcSlice(int64_t budget)
{
    nsJSContext::RunCycleCollectorWorkSlice(budget);
    return NS_OK;
}

NS_IMETHODIMP
nsXPCComponents_Utils::GetMaxCCSliceTimeSinceClear(int32_t* out)
{
    *out = nsJSContext::GetMaxCCSliceTimeSinceClear();
    return NS_OK;
}

NS_IMETHODIMP
nsXPCComponents_Utils::ClearMaxCCTime()
{
    nsJSContext::ClearMaxCCSliceTime();
    return NS_OK;
}

NS_IMETHODIMP
nsXPCComponents_Utils::ForceShrinkingGC()
{
    JSContext* cx = dom::danger::GetJSContext();
    PrepareForFullGC(cx);
    NonIncrementalGC(cx, GC_SHRINK, gcreason::COMPONENT_UTILS);
    return NS_OK;
}

class PreciseGCRunnable : public Runnable
{
  public:
    PreciseGCRunnable(ScheduledGCCallback* aCallback, bool aShrinking)
      : mozilla::Runnable("PreciseGCRunnable")
      , mCallback(aCallback)
      , mShrinking(aShrinking)
    {
    }

    NS_IMETHOD Run() override
    {
        nsJSContext::GarbageCollectNow(gcreason::COMPONENT_UTILS,
                                       nsJSContext::NonIncrementalGC,
                                       mShrinking ?
                                         nsJSContext::ShrinkingGC :
                                         nsJSContext::NonShrinkingGC);

        mCallback->Callback();
        return NS_OK;
    }

  private:
    RefPtr<ScheduledGCCallback> mCallback;
    bool mShrinking;
};

NS_IMETHODIMP
nsXPCComponents_Utils::SchedulePreciseGC(ScheduledGCCallback* aCallback)
{
    RefPtr<PreciseGCRunnable> event = new PreciseGCRunnable(aCallback, false);
    return NS_DispatchToMainThread(event);
}

NS_IMETHODIMP
nsXPCComponents_Utils::SchedulePreciseShrinkingGC(ScheduledGCCallback* aCallback)
{
    RefPtr<PreciseGCRunnable> event = new PreciseGCRunnable(aCallback, true);
    return NS_DispatchToMainThread(event);
}

NS_IMETHODIMP
nsXPCComponents_Utils::UnlinkGhostWindows()
{
#ifdef DEBUG
    nsWindowMemoryReporter::UnlinkGhostWindows();

    if (XRE_IsParentProcess()) {
        nsCOMPtr<nsIObserverService> obsvc = services::GetObserverService();
        if (obsvc) {
            obsvc->NotifyObservers(nullptr, "child-ghost-request", nullptr);
        }
    }

    return NS_OK;
#else
    return NS_ERROR_NOT_IMPLEMENTED;
#endif
}

NS_IMETHODIMP
nsXPCComponents_Utils::GetJSTestingFunctions(JSContext* cx,
                                             MutableHandleValue retval)
{
    JSObject* obj = js::GetTestingFunctions(cx);
    if (!obj)
        return NS_ERROR_XPC_JAVASCRIPT_ERROR;
    retval.setObject(*obj);
    return NS_OK;
}

NS_IMETHODIMP
nsXPCComponents_Utils::CallFunctionWithAsyncStack(HandleValue function,
                                                  nsIStackFrame* stack,
                                                  const nsAString& asyncCause,
                                                  JSContext* cx,
                                                  MutableHandleValue retval)
{
    nsresult rv;

    if (!stack || asyncCause.IsEmpty()) {
        return NS_ERROR_INVALID_ARG;
    }

    JS::Rooted<JS::Value> asyncStack(cx);
    rv = stack->GetNativeSavedFrame(&asyncStack);
    if (NS_FAILED(rv))
        return rv;
    if (!asyncStack.isObject()) {
        JS_ReportErrorASCII(cx, "Must use a native JavaScript stack frame");
        return NS_ERROR_INVALID_ARG;
    }

    JS::Rooted<JSObject*> asyncStackObj(cx, &asyncStack.toObject());

    NS_ConvertUTF16toUTF8 utf8Cause(asyncCause);
    JS::AutoSetAsyncStackForNewCalls sas(cx, asyncStackObj, utf8Cause.get(),
                                         JS::AutoSetAsyncStackForNewCalls::AsyncCallKind::EXPLICIT);

    if (!JS_CallFunctionValue(cx, nullptr, function,
                              JS::HandleValueArray::empty(), retval))
    {
        return NS_ERROR_XPC_JAVASCRIPT_ERROR;
    }

    return NS_OK;
}

NS_IMETHODIMP
nsXPCComponents_Utils::GetGlobalForObject(HandleValue object,
                                          JSContext* cx,
                                          MutableHandleValue retval)
{
    // First argument must be an object.
    if (object.isPrimitive())
        return NS_ERROR_XPC_BAD_CONVERT_JS;

    // Wrappers are parented to their the global in their home compartment. But
    // when getting the global for a cross-compartment wrapper, we really want
    // a wrapper for the foreign global. So we need to unwrap before getting the
    // parent, enter the compartment for the duration of the call, and wrap the
    // result.
    Rooted<JSObject*> obj(cx, &object.toObject());
    obj = js::UncheckedUnwrap(obj);
    {
        JSAutoRealm ar(cx, obj);
        obj = JS_GetGlobalForObject(cx, obj);
    }

    if (!JS_WrapObject(cx, &obj))
        return NS_ERROR_FAILURE;

    // Get the WindowProxy if necessary.
    obj = js::ToWindowProxyIfWindow(obj);

    retval.setObject(*obj);
    return NS_OK;
}

NS_IMETHODIMP
nsXPCComponents_Utils::IsProxy(HandleValue vobj, JSContext* cx, bool* rval)
{
    if (!vobj.isObject()) {
        *rval = false;
        return NS_OK;
    }

    RootedObject obj(cx, &vobj.toObject());
    obj = js::CheckedUnwrap(obj, /* stopAtWindowProxy = */ false);
    NS_ENSURE_TRUE(obj, NS_ERROR_FAILURE);

    *rval = js::IsScriptedProxy(obj);
    return NS_OK;
}

NS_IMETHODIMP
nsXPCComponents_Utils::ExportFunction(HandleValue vfunction, HandleValue vscope,
                                      HandleValue voptions, JSContext* cx,
                                      MutableHandleValue rval)
{
    if (!xpc::ExportFunction(cx, vfunction, vscope, voptions, rval))
        return NS_ERROR_FAILURE;
    return NS_OK;
}

NS_IMETHODIMP
nsXPCComponents_Utils::CreateObjectIn(HandleValue vobj, HandleValue voptions,
                                      JSContext* cx, MutableHandleValue rval)
{
    RootedObject optionsObject(cx, voptions.isObject() ? &voptions.toObject()
                                                       : nullptr);
    CreateObjectInOptions options(cx, optionsObject);
    if (voptions.isObject() &&
        !options.Parse())
    {
        return NS_ERROR_FAILURE;
    }

    if (!xpc::CreateObjectIn(cx, vobj, options, rval))
        return NS_ERROR_FAILURE;
    return NS_OK;
}

NS_IMETHODIMP
nsXPCComponents_Utils::MakeObjectPropsNormal(HandleValue vobj, JSContext* cx)
{
    if (!cx)
        return NS_ERROR_FAILURE;

    // first argument must be an object
    if (vobj.isPrimitive())
        return NS_ERROR_XPC_BAD_CONVERT_JS;

    RootedObject obj(cx, js::UncheckedUnwrap(&vobj.toObject()));
    JSAutoRealm ar(cx, obj);
    Rooted<IdVector> ida(cx, IdVector(cx));
    if (!JS_Enumerate(cx, obj, &ida))
        return NS_ERROR_FAILURE;

    RootedId id(cx);
    RootedValue v(cx);
    for (size_t i = 0; i < ida.length(); ++i) {
        id = ida[i];

        if (!JS_GetPropertyById(cx, obj, id, &v))
            return NS_ERROR_FAILURE;

        if (v.isPrimitive())
            continue;

        RootedObject propobj(cx, &v.toObject());
        // TODO Deal with non-functions.
        if (!js::IsWrapper(propobj) || !JS::IsCallable(propobj))
            continue;

        FunctionForwarderOptions forwarderOptions;
        if (!NewFunctionForwarder(cx, id, propobj, forwarderOptions, &v) ||
            !JS_SetPropertyById(cx, obj, id, v))
            return NS_ERROR_FAILURE;
    }

    return NS_OK;
}

NS_IMETHODIMP
nsXPCComponents_Utils::IsDeadWrapper(HandleValue obj, bool* out)
{
    *out = false;
    if (obj.isPrimitive())
        return NS_ERROR_INVALID_ARG;

    // We should never have cross-compartment wrappers for dead wrappers.
    MOZ_ASSERT_IF(js::IsCrossCompartmentWrapper(&obj.toObject()),
                  !JS_IsDeadWrapper(js::UncheckedUnwrap(&obj.toObject())));

    *out = JS_IsDeadWrapper(&obj.toObject());
    return NS_OK;
}

NS_IMETHODIMP
nsXPCComponents_Utils::IsCrossProcessWrapper(HandleValue obj, bool* out)
{
    *out = false;
    if (obj.isPrimitive())
        return NS_ERROR_INVALID_ARG;

    *out = jsipc::IsWrappedCPOW(&obj.toObject());
    return NS_OK;
}

NS_IMETHODIMP
nsXPCComponents_Utils::GetCrossProcessWrapperTag(HandleValue obj, nsACString& out)
{
    if (obj.isPrimitive() || !jsipc::IsWrappedCPOW(&obj.toObject()))
        return NS_ERROR_INVALID_ARG;

    jsipc::GetWrappedCPOWTag(&obj.toObject(), out);
    return NS_OK;
}

NS_IMETHODIMP
nsXPCComponents_Utils::PermitCPOWsInScope(HandleValue obj)
{
    if (!obj.isObject())
        return NS_ERROR_INVALID_ARG;

    JSObject* scopeObj = js::UncheckedUnwrap(&obj.toObject());
    MOZ_DIAGNOSTIC_ASSERT(!mozJSComponentLoader::Get()->IsLoaderGlobal(scopeObj),
                          "Don't call Cu.PermitCPOWsInScope() in a JSM that shares its global");
    CompartmentPrivate::Get(scopeObj)->allowCPOWs = true;
    return NS_OK;
}

NS_IMETHODIMP
nsXPCComponents_Utils::RecomputeWrappers(HandleValue vobj, JSContext* cx)
{
    // Determine the compartment of the given object, if any.
    JS::Compartment* c = vobj.isObject()
                         ? js::GetObjectCompartment(js::UncheckedUnwrap(&vobj.toObject()))
                         : nullptr;

    // If no compartment was given, recompute all.
    if (!c)
        js::RecomputeWrappers(cx, js::AllCompartments(), js::AllCompartments());
    // Otherwise, recompute wrappers for the given compartment.
    else
        js::RecomputeWrappers(cx, js::SingleCompartment(c), js::AllCompartments()) &&
        js::RecomputeWrappers(cx, js::AllCompartments(), js::SingleCompartment(c));

    return NS_OK;
}

NS_IMETHODIMP
nsXPCComponents_Utils::SetWantXrays(HandleValue vscope, JSContext* cx)
{
    if (!vscope.isObject())
        return NS_ERROR_INVALID_ARG;
    JSObject* scopeObj = js::UncheckedUnwrap(&vscope.toObject());
    MOZ_DIAGNOSTIC_ASSERT(!mozJSComponentLoader::Get()->IsLoaderGlobal(scopeObj),
                          "Don't call Cu.setWantXrays() in a JSM that shares its global");
    JS::Compartment* compartment = js::GetObjectCompartment(scopeObj);
    CompartmentPrivate::Get(scopeObj)->wantXrays = true;
    bool ok = js::RecomputeWrappers(cx, js::SingleCompartment(compartment),
                                    js::AllCompartments());
    NS_ENSURE_TRUE(ok, NS_ERROR_FAILURE);
    return NS_OK;
}

NS_IMETHODIMP
nsXPCComponents_Utils::ForcePermissiveCOWs(JSContext* cx)
{
    xpc::CrashIfNotInAutomation();
    JSObject* currentGlobal = CurrentGlobalOrNull(cx);
    MOZ_DIAGNOSTIC_ASSERT(!mozJSComponentLoader::Get()->IsLoaderGlobal(currentGlobal),
                          "Don't call Cu.forcePermissiveCOWs() in a JSM that shares its global");
    CompartmentPrivate::Get(currentGlobal)->forcePermissiveCOWs = true;
    return NS_OK;
}

NS_IMETHODIMP
nsXPCComponents_Utils::Dispatch(HandleValue runnableArg, HandleValue scope,
                                JSContext* cx)
{
    RootedValue runnable(cx, runnableArg);
    // Enter the given realm, if any, and rewrap runnable.
    Maybe<JSAutoRealm> ar;
    if (scope.isObject()) {
        JSObject* scopeObj = js::UncheckedUnwrap(&scope.toObject());
        if (!scopeObj)
            return NS_ERROR_FAILURE;
        ar.emplace(cx, scopeObj);
        if (!JS_WrapValue(cx, &runnable))
            return NS_ERROR_FAILURE;
    }

    // Get an XPCWrappedJS for |runnable|.
    if (!runnable.isObject())
        return NS_ERROR_INVALID_ARG;

    nsCOMPtr<nsIRunnable> run;
    nsresult rv = nsXPConnect::XPConnect()->WrapJS(cx, &runnable.toObject(),
                                                   NS_GET_IID(nsIRunnable),
                                                   getter_AddRefs(run));
    NS_ENSURE_SUCCESS(rv, rv);
    MOZ_ASSERT(run);

    // Dispatch.
    return NS_DispatchToMainThread(run);
}

#define GENERATE_JSCONTEXTOPTION_GETTER_SETTER(_attr, _getter, _setter) \
    NS_IMETHODIMP                                                       \
    nsXPCComponents_Utils::Get## _attr(JSContext* cx, bool* aValue)     \
    {                                                                   \
        *aValue = ContextOptionsRef(cx)._getter();                      \
        return NS_OK;                                                   \
    }                                                                   \
    NS_IMETHODIMP                                                       \
    nsXPCComponents_Utils::Set## _attr(JSContext* cx, bool aValue)      \
    {                                                                   \
        ContextOptionsRef(cx)._setter(aValue);                          \
        return NS_OK;                                                   \
    }

GENERATE_JSCONTEXTOPTION_GETTER_SETTER(Strict, extraWarnings, setExtraWarnings)
GENERATE_JSCONTEXTOPTION_GETTER_SETTER(Werror, werror, setWerror)
GENERATE_JSCONTEXTOPTION_GETTER_SETTER(Strict_mode, strictMode, setStrictMode)
GENERATE_JSCONTEXTOPTION_GETTER_SETTER(Ion, ion, setIon)

#undef GENERATE_JSCONTEXTOPTION_GETTER_SETTER

NS_IMETHODIMP
nsXPCComponents_Utils::SetGCZeal(int32_t aValue, JSContext* cx)
{
#ifdef JS_GC_ZEAL
    JS_SetGCZeal(cx, uint8_t(aValue), JS_DEFAULT_ZEAL_FREQ);
#endif
    return NS_OK;
}

NS_IMETHODIMP
nsXPCComponents_Utils::GetIsInAutomation(bool* aResult)
{
    NS_ENSURE_ARG_POINTER(aResult);

    *aResult = xpc::IsInAutomation();
    return NS_OK;
}

NS_IMETHODIMP
nsXPCComponents_Utils::CrashIfNotInAutomation()
{
    xpc::CrashIfNotInAutomation();
    return NS_OK;
}

NS_IMETHODIMP
nsXPCComponents_Utils::NukeSandbox(HandleValue obj, JSContext* cx)
{
    AUTO_PROFILER_LABEL("nsXPCComponents_Utils::NukeSandbox", OTHER);
    NS_ENSURE_TRUE(obj.isObject(), NS_ERROR_INVALID_ARG);
    JSObject* wrapper = &obj.toObject();
    NS_ENSURE_TRUE(IsWrapper(wrapper), NS_ERROR_INVALID_ARG);
    RootedObject sb(cx, UncheckedUnwrap(wrapper));
    NS_ENSURE_TRUE(IsSandbox(sb), NS_ERROR_INVALID_ARG);

    xpc::NukeAllWrappersForCompartment(cx, GetObjectCompartment(sb));

    return NS_OK;
}

NS_IMETHODIMP
nsXPCComponents_Utils::BlockScriptForGlobal(HandleValue globalArg,
                                            JSContext* cx)
{
    NS_ENSURE_TRUE(globalArg.isObject(), NS_ERROR_INVALID_ARG);
    RootedObject global(cx, UncheckedUnwrap(&globalArg.toObject(),
                                            /* stopAtWindowProxy = */ false));
    NS_ENSURE_TRUE(JS_IsGlobalObject(global), NS_ERROR_INVALID_ARG);
    if (nsContentUtils::IsSystemPrincipal(xpc::GetObjectPrincipal(global))) {
        JS_ReportErrorASCII(cx, "Script may not be disabled for system globals");
        return NS_ERROR_FAILURE;
    }
    Scriptability::Get(global).Block();
    return NS_OK;
}

NS_IMETHODIMP
nsXPCComponents_Utils::UnblockScriptForGlobal(HandleValue globalArg,
                                              JSContext* cx)
{
    NS_ENSURE_TRUE(globalArg.isObject(), NS_ERROR_INVALID_ARG);
    RootedObject global(cx, UncheckedUnwrap(&globalArg.toObject(),
                                            /* stopAtWindowProxy = */ false));
    NS_ENSURE_TRUE(JS_IsGlobalObject(global), NS_ERROR_INVALID_ARG);
    if (nsContentUtils::IsSystemPrincipal(xpc::GetObjectPrincipal(global))) {
        JS_ReportErrorASCII(cx, "Script may not be disabled for system globals");
        return NS_ERROR_FAILURE;
    }
    Scriptability::Get(global).Unblock();
    return NS_OK;
}

NS_IMETHODIMP
nsXPCComponents_Utils::IsXrayWrapper(HandleValue obj, bool* aRetval)
{
    *aRetval =
        obj.isObject() && xpc::WrapperFactory::IsXrayWrapper(&obj.toObject());
    return NS_OK;
}

NS_IMETHODIMP
nsXPCComponents_Utils::WaiveXrays(HandleValue aVal, JSContext* aCx, MutableHandleValue aRetval)
{
    RootedValue value(aCx, aVal);
    if (!xpc::WrapperFactory::WaiveXrayAndWrap(aCx, &value))
        return NS_ERROR_FAILURE;
    aRetval.set(value);
    return NS_OK;
}

NS_IMETHODIMP
nsXPCComponents_Utils::UnwaiveXrays(HandleValue aVal, JSContext* aCx, MutableHandleValue aRetval)
{
    if (!aVal.isObject()) {
        aRetval.set(aVal);
        return NS_OK;
    }

    RootedObject obj(aCx, js::UncheckedUnwrap(&aVal.toObject()));
    if (!JS_WrapObject(aCx, &obj))
        return NS_ERROR_FAILURE;
    aRetval.setObject(*obj);
    return NS_OK;
}

NS_IMETHODIMP
nsXPCComponents_Utils::GetClassName(HandleValue aObj, bool aUnwrap, JSContext* aCx, char** aRv)
{
    if (!aObj.isObject())
        return NS_ERROR_INVALID_ARG;
    RootedObject obj(aCx, &aObj.toObject());
    if (aUnwrap)
        obj = js::UncheckedUnwrap(obj, /* stopAtWindowProxy = */ false);
    *aRv = NS_strdup(js::GetObjectClass(obj)->name);
    NS_ENSURE_TRUE(*aRv, NS_ERROR_OUT_OF_MEMORY);
    return NS_OK;
}

NS_IMETHODIMP
nsXPCComponents_Utils::GetDOMClassInfo(const nsAString& aClassName,
                                       nsIClassInfo** aClassInfo)
{
    *aClassInfo = nullptr;
    return NS_ERROR_NOT_AVAILABLE;
}

NS_IMETHODIMP
nsXPCComponents_Utils::GetIncumbentGlobal(HandleValue aCallback,
                                          JSContext* aCx, MutableHandleValue aOut)
{
    nsCOMPtr<nsIGlobalObject> global = mozilla::dom::GetIncumbentGlobal();
    RootedValue globalVal(aCx);

    if (!global) {
        globalVal = NullValue();
    } else {
        // Note: We rely on the wrap call for outerization.
        globalVal = ObjectValue(*global->GetGlobalJSObject());
        if (!JS_WrapValue(aCx, &globalVal))
            return NS_ERROR_FAILURE;
    }

    // Invoke the callback, if passed.
    if (aCallback.isObject()) {
        RootedValue ignored(aCx);
        if (!JS_CallFunctionValue(aCx, nullptr, aCallback, JS::HandleValueArray(globalVal), &ignored))
            return NS_ERROR_FAILURE;
    }

    aOut.set(globalVal);
    return NS_OK;
}

/*
 * Below is a bunch of awkward junk to allow JS test code to trigger the
 * creation of an XPCWrappedJS, such that it ends up in the map. We need to
 * hand the caller some sort of reference to hold onto (to prevent the
 * refcount from dropping to zero as soon as the function returns), but trying
 * to return a bonafide XPCWrappedJS to script causes all sorts of trouble. So
 * we create a benign holder class instead, which acts as an opaque reference
 * that script can use to keep the XPCWrappedJS alive and in the map.
 */

class WrappedJSHolder : public nsISupports
{
    NS_DECL_ISUPPORTS
    WrappedJSHolder() {}

    RefPtr<nsXPCWrappedJS> mWrappedJS;

private:
    virtual ~WrappedJSHolder() {}
};

NS_IMPL_ADDREF(WrappedJSHolder)
NS_IMPL_RELEASE(WrappedJSHolder)

// nsINamed is always supported by nsXPCWrappedJSClass.
// We expose this interface only for the identity in telemetry analysis.
NS_INTERFACE_TABLE_HEAD(WrappedJSHolder)
  if (aIID.Equals(NS_GET_IID(nsINamed))) {
    return mWrappedJS->QueryInterface(aIID, aInstancePtr);
  }
NS_INTERFACE_TABLE0(WrappedJSHolder)
NS_INTERFACE_TABLE_TAIL

NS_IMETHODIMP
nsXPCComponents_Utils::GenerateXPCWrappedJS(HandleValue aObj, HandleValue aScope,
                                            JSContext* aCx, nsISupports** aOut)
{
    if (!aObj.isObject())
        return NS_ERROR_INVALID_ARG;
    RootedObject obj(aCx, &aObj.toObject());
    RootedObject scope(aCx, aScope.isObject() ? js::UncheckedUnwrap(&aScope.toObject())
                                              : CurrentGlobalOrNull(aCx));
    JSAutoRealm ar(aCx, scope);
    if (!JS_WrapObject(aCx, &obj))
        return NS_ERROR_FAILURE;

    RefPtr<WrappedJSHolder> holder = new WrappedJSHolder();
    nsresult rv = nsXPCWrappedJS::GetNewOrUsed(obj, NS_GET_IID(nsISupports),
                                               getter_AddRefs(holder->mWrappedJS));
    holder.forget(aOut);
    return rv;
}

NS_IMETHODIMP
nsXPCComponents_Utils::GetWatchdogTimestamp(const nsAString& aCategory, PRTime* aOut)
{
    WatchdogTimestampCategory category;
    if (aCategory.EqualsLiteral("ContextStateChange"))
        category = TimestampContextStateChange;
    else if (aCategory.EqualsLiteral("WatchdogWakeup"))
        category = TimestampWatchdogWakeup;
    else if (aCategory.EqualsLiteral("WatchdogHibernateStart"))
        category = TimestampWatchdogHibernateStart;
    else if (aCategory.EqualsLiteral("WatchdogHibernateStop"))
        category = TimestampWatchdogHibernateStop;
    else
        return NS_ERROR_INVALID_ARG;
    *aOut = XPCJSContext::Get()->GetWatchdogTimestamp(category);
    return NS_OK;
}

NS_IMETHODIMP
nsXPCComponents_Utils::GetJSEngineTelemetryValue(JSContext* cx, MutableHandleValue rval)
{
    RootedObject obj(cx, JS_NewPlainObject(cx));
    if (!obj)
        return NS_ERROR_OUT_OF_MEMORY;

    // No JS engine telemetry in use at the moment.

    rval.setObject(*obj);
    return NS_OK;
}

bool
xpc::CloneInto(JSContext* aCx, HandleValue aValue, HandleValue aScope,
               HandleValue aOptions, MutableHandleValue aCloned)
{
    if (!aScope.isObject())
        return false;

    RootedObject scope(aCx, &aScope.toObject());
    scope = js::CheckedUnwrap(scope);
    if(!scope) {
        JS_ReportErrorASCII(aCx, "Permission denied to clone object into scope");
        return false;
    }

    if (!aOptions.isUndefined() && !aOptions.isObject()) {
        JS_ReportErrorASCII(aCx, "Invalid argument");
        return false;
    }

    RootedObject optionsObject(aCx, aOptions.isObject() ? &aOptions.toObject()
                                                        : nullptr);
    StackScopedCloneOptions options(aCx, optionsObject);
    if (aOptions.isObject() && !options.Parse())
        return false;

    {
        JSAutoRealm ar(aCx, scope);
        aCloned.set(aValue);
        if (!StackScopedClone(aCx, options, aCloned))
            return false;
    }

    return JS_WrapValue(aCx, aCloned);
}

NS_IMETHODIMP
nsXPCComponents_Utils::CloneInto(HandleValue aValue, HandleValue aScope,
                                 HandleValue aOptions, JSContext* aCx,
                                 MutableHandleValue aCloned)
{
    return xpc::CloneInto(aCx, aValue, aScope, aOptions, aCloned) ?
           NS_OK : NS_ERROR_FAILURE;
}

NS_IMETHODIMP
nsXPCComponents_Utils::GetWebIDLCallerPrincipal(nsIPrincipal** aResult)
{
    // This API may only be when the Entry Settings Object corresponds to a
    // JS-implemented WebIDL call. In all other cases, the value will be null,
    // and we throw.
    nsCOMPtr<nsIPrincipal> callerPrin = mozilla::dom::GetWebIDLCallerPrincipal();
    if (!callerPrin)
        return NS_ERROR_NOT_AVAILABLE;
    callerPrin.forget(aResult);
    return NS_OK;
}

NS_IMETHODIMP
nsXPCComponents_Utils::GetObjectPrincipal(HandleValue val, JSContext* cx,
                                          nsIPrincipal** result)
{
    if (!val.isObject())
        return NS_ERROR_INVALID_ARG;
    RootedObject obj(cx, &val.toObject());
    obj = js::CheckedUnwrap(obj);
    MOZ_ASSERT(obj);

    nsCOMPtr<nsIPrincipal> prin = nsContentUtils::ObjectPrincipal(obj);
    prin.forget(result);
    return NS_OK;
}

NS_IMETHODIMP
nsXPCComponents_Utils::GetRealmLocation(HandleValue val,
                                        JSContext* cx,
                                        nsACString& result)
{
    if (!val.isObject())
        return NS_ERROR_INVALID_ARG;
    RootedObject obj(cx, &val.toObject());
    obj = js::CheckedUnwrap(obj);
    MOZ_ASSERT(obj);

    result = xpc::RealmPrivate::Get(obj)->GetLocation();
    return NS_OK;
}

NS_IMETHODIMP
nsXPCComponents_Utils::ReadUTF8File(nsIFile* aFile, nsACString& aResult)
{
    NS_ENSURE_TRUE(aFile, NS_ERROR_INVALID_ARG);

    MOZ_TRY_VAR(aResult, URLPreloader::ReadFile(aFile));
    return NS_OK;
}

NS_IMETHODIMP
nsXPCComponents_Utils::ReadUTF8URI(nsIURI* aURI, nsACString& aResult)
{
    NS_ENSURE_TRUE(aURI, NS_ERROR_INVALID_ARG);

    MOZ_TRY_VAR(aResult, URLPreloader::ReadURI(aURI));
    return NS_OK;
}

NS_IMETHODIMP
nsXPCComponents_Utils::Now(double* aRetval)
{
    TimeStamp start = TimeStamp::ProcessCreation();
    *aRetval = (TimeStamp::Now() - start).ToMilliseconds();
    return NS_OK;
}

NS_IMETHODIMP
nsXPCComponents_Utils::BlockThreadedExecution(nsIBlockThreadedExecutionCallback* aCallback)
{
    Scheduler::BlockThreadedExecution(aCallback);
    return NS_OK;
}

NS_IMETHODIMP
nsXPCComponents_Utils::UnblockThreadedExecution()
{
    Scheduler::UnblockThreadedExecution();
    return NS_OK;
}

/***************************************************************************/
/***************************************************************************/
/***************************************************************************/


nsXPCComponentsBase::nsXPCComponentsBase(XPCWrappedNativeScope* aScope)
    :   mScope(aScope)
{
    MOZ_ASSERT(aScope, "aScope must not be null");
}

nsXPCComponents::nsXPCComponents(XPCWrappedNativeScope* aScope)
    :   nsXPCComponentsBase(aScope)
{
}

nsXPCComponentsBase::~nsXPCComponentsBase()
{
}

nsXPCComponents::~nsXPCComponents()
{
}

void
nsXPCComponentsBase::ClearMembers()
{
    mInterfaces = nullptr;
    mInterfacesByID = nullptr;
    mResults = nullptr;
}

void
nsXPCComponents::ClearMembers()
{
    mClasses = nullptr;
    mClassesByID = nullptr;
    mID = nullptr;
    mException = nullptr;
    mConstructor = nullptr;
    mUtils = nullptr;

    nsXPCComponentsBase::ClearMembers();
}

/*******************************************/
#define XPC_IMPL_GET_OBJ_METHOD(_class, _n)                                   \
NS_IMETHODIMP _class::Get##_n(nsIXPCComponents_##_n * *a##_n) {               \
    NS_ENSURE_ARG_POINTER(a##_n);                                             \
    if (!m##_n)                                                               \
        m##_n = new nsXPCComponents_##_n();                                   \
    RefPtr<nsXPCComponents_##_n> ret = m##_n;                               \
    ret.forget(a##_n);                                                        \
    return NS_OK;                                                             \
}

XPC_IMPL_GET_OBJ_METHOD(nsXPCComponentsBase, Interfaces)
XPC_IMPL_GET_OBJ_METHOD(nsXPCComponentsBase, InterfacesByID)
XPC_IMPL_GET_OBJ_METHOD(nsXPCComponents, Classes)
XPC_IMPL_GET_OBJ_METHOD(nsXPCComponents, ClassesByID)
XPC_IMPL_GET_OBJ_METHOD(nsXPCComponentsBase, Results)
XPC_IMPL_GET_OBJ_METHOD(nsXPCComponents, ID)
XPC_IMPL_GET_OBJ_METHOD(nsXPCComponents, Exception)
XPC_IMPL_GET_OBJ_METHOD(nsXPCComponents, Constructor)
XPC_IMPL_GET_OBJ_METHOD(nsXPCComponents, Utils)

#undef XPC_IMPL_GET_OBJ_METHOD
/*******************************************/

NS_IMETHODIMP
nsXPCComponentsBase::IsSuccessCode(nsresult result, bool* out)
{
    *out = NS_SUCCEEDED(result);
    return NS_OK;
}

NS_IMETHODIMP
nsXPCComponents::GetStack(nsIStackFrame** aStack)
{
    nsCOMPtr<nsIStackFrame> frame = dom::GetCurrentJSStack();
    frame.forget(aStack);
    return NS_OK;
}

NS_IMETHODIMP
nsXPCComponents::GetManager(nsIComponentManager * *aManager)
{
    MOZ_ASSERT(aManager, "bad param");
    return NS_GetComponentManager(aManager);
}

NS_IMETHODIMP
nsXPCComponents::GetReturnCode(JSContext* aCx, MutableHandleValue aOut)
{
    nsresult res = XPCJSContext::Get()->GetPendingResult();
    aOut.setNumber(static_cast<uint32_t>(res));
    return NS_OK;
}

NS_IMETHODIMP
nsXPCComponents::SetReturnCode(JSContext* aCx, HandleValue aCode)
{
    nsresult rv;
    if (!ToUint32(aCx, aCode, (uint32_t*)&rv))
        return NS_ERROR_FAILURE;
    XPCJSContext::Get()->SetPendingResult(rv);
    return NS_OK;
}

/**********************************************/

class ComponentsSH : public nsIXPCScriptable
{
public:
    explicit constexpr ComponentsSH(unsigned dummy)
    {
    }

    // We don't actually inherit any ref counting infrastructure, but we don't
    // need an nsAutoRefCnt member, so the _INHERITED macro is a hack to avoid
    // having one.
    NS_DECL_ISUPPORTS_INHERITED
    NS_DECL_NSIXPCSCRIPTABLE
    static nsresult Get(nsIXPCScriptable** helper)
    {
        *helper = &singleton;
        return NS_OK;
    }

private:
    static ComponentsSH singleton;
};

ComponentsSH ComponentsSH::singleton(0);

// Singleton refcounting.
NS_IMETHODIMP_(MozExternalRefCountType) ComponentsSH::AddRef(void) { return 1; }
NS_IMETHODIMP_(MozExternalRefCountType) ComponentsSH::Release(void) { return 1; }

NS_IMPL_QUERY_INTERFACE(ComponentsSH, nsIXPCScriptable)

#define NSXPCCOMPONENTSBASE_CID \
{ 0xc62998e5, 0x95f1, 0x4058, \
  { 0xa5, 0x09, 0xec, 0x21, 0x66, 0x18, 0x92, 0xb9 } }

#define NSXPCCOMPONENTS_CID \
{ 0x3649f405, 0xf0ec, 0x4c28, \
    { 0xae, 0xb0, 0xaf, 0x9a, 0x51, 0xe4, 0x4c, 0x81 } }

NS_IMPL_CLASSINFO(nsXPCComponentsBase, &ComponentsSH::Get, 0, NSXPCCOMPONENTSBASE_CID)
NS_IMPL_ISUPPORTS_CI(nsXPCComponentsBase, nsIXPCComponentsBase)

NS_IMPL_CLASSINFO(nsXPCComponents, &ComponentsSH::Get, 0, NSXPCCOMPONENTS_CID)
// Below is more or less what NS_IMPL_ISUPPORTS_CI_INHERITED1 would look like
// if it existed.
NS_IMPL_ADDREF_INHERITED(nsXPCComponents, nsXPCComponentsBase)
NS_IMPL_RELEASE_INHERITED(nsXPCComponents, nsXPCComponentsBase)
NS_INTERFACE_MAP_BEGIN(nsXPCComponents)
    NS_INTERFACE_MAP_ENTRY(nsIXPCComponents)
    NS_IMPL_QUERY_CLASSINFO(nsXPCComponents)
NS_INTERFACE_MAP_END_INHERITING(nsXPCComponentsBase)
NS_IMPL_CI_INTERFACE_GETTER(nsXPCComponents, nsIXPCComponents)

// The nsIXPCScriptable map declaration that will generate stubs for us
#define XPC_MAP_CLASSNAME ComponentsSH
#define XPC_MAP_QUOTED_CLASSNAME "nsXPCComponents"
#define XPC_MAP_FLAGS XPC_SCRIPTABLE_WANT_PRECREATE
#include "xpc_map_end.h" /* This will #undef the above */

NS_IMETHODIMP
ComponentsSH::PreCreate(nsISupports* nativeObj, JSContext* cx, JSObject* globalObj, JSObject** parentObj)
{
  nsXPCComponentsBase* self = static_cast<nsXPCComponentsBase*>(nativeObj);
  // this should never happen
  if (!self->GetScope()) {
      NS_WARNING("mScope must not be null when nsXPCComponents::PreCreate is called");
      return NS_ERROR_FAILURE;
  }
  *parentObj = self->GetScope()->GetGlobalJSObject();
  return NS_OK;
}
