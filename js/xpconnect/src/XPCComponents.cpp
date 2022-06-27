/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/* The "Components" xpcom objects for JavaScript. */

#include "xpcprivate.h"
#include "xpc_make_class.h"
#include "JSServices.h"
#include "XPCJSWeakReference.h"
#include "WrapperFactory.h"
#include "nsJSUtils.h"
#include "mozJSComponentLoader.h"
#include "nsContentUtils.h"
#include "nsCycleCollector.h"
#include "jsfriendapi.h"
#include "js/Array.h"  // JS::IsArrayObject
#include "js/CallAndConstruct.h"  // JS::IsCallable, JS_CallFunctionName, JS_CallFunctionValue
#include "js/CharacterEncoding.h"
#include "js/ContextOptions.h"
#include "js/friend/WindowProxy.h"  // js::ToWindowProxyIfWindow
#include "js/Object.h"              // JS::GetClass, JS::GetCompartment
#include "js/PropertyAndElement.h"  // JS_DefineProperty, JS_DefinePropertyById, JS_Enumerate, JS_GetProperty, JS_GetPropertyById, JS_HasProperty, JS_SetProperty, JS_SetPropertyById
#include "js/SavedFrameAPI.h"
#include "js/StructuredClone.h"
#include "mozilla/AppShutdown.h"
#include "mozilla/Attributes.h"
#include "mozilla/LoadContext.h"
#include "mozilla/Preferences.h"
#include "nsJSEnvironment.h"
#include "mozilla/BasePrincipal.h"
#include "mozilla/TimeStamp.h"
#include "mozilla/ResultExtensions.h"
#include "mozilla/URLPreloader.h"
#include "mozilla/dom/DOMException.h"
#include "mozilla/dom/DOMExceptionBinding.h"
#include "mozilla/dom/BindingUtils.h"
#include "mozilla/dom/RemoteObjectProxy.h"
#include "mozilla/dom/StructuredCloneTags.h"
#include "mozilla/dom/WindowBinding.h"
#include "nsZipArchive.h"
#include "nsWindowMemoryReporter.h"
#include "nsICycleCollectorListener.h"
#include "nsIException.h"
#include "nsIScriptError.h"
#include "nsPIDOMWindow.h"
#include "nsGlobalWindow.h"
#include "nsScriptError.h"
#include "GeckoProfiler.h"
#include "ProfilerControl.h"
#include "mozilla/EditorSpellCheck.h"
#include "nsCommandLine.h"
#include "nsCommandParams.h"
#include "nsPersistentProperties.h"
#include "nsIDocumentEncoder.h"

using namespace mozilla;
using namespace JS;
using namespace js;
using namespace xpc;
using mozilla::dom::Exception;

/***************************************************************************/
// stuff used by all

nsresult xpc::ThrowAndFail(nsresult errNum, JSContext* cx, bool* retval) {
  XPCThrower::Throw(errNum, cx);
  *retval = false;
  return NS_OK;
}

static bool JSValIsInterfaceOfType(JSContext* cx, HandleValue v, REFNSIID iid) {
  nsCOMPtr<nsIXPConnectWrappedNative> wn;
  nsCOMPtr<nsISupports> iface;

  if (v.isPrimitive()) {
    return false;
  }

  nsIXPConnect* xpc = nsIXPConnect::XPConnect();
  RootedObject obj(cx, &v.toObject());
  return NS_SUCCEEDED(
             xpc->GetWrappedNativeOfJSObject(cx, obj, getter_AddRefs(wn))) &&
         wn &&
         NS_SUCCEEDED(
             wn->Native()->QueryInterface(iid, getter_AddRefs(iface))) &&
         iface;
}

/***************************************************************************/
/***************************************************************************/
/***************************************************************************/

class nsXPCComponents_Interfaces final : public nsIXPCComponents_Interfaces,
                                         public nsIXPCScriptable,
                                         public nsIClassInfo {
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
nsXPCComponents_Interfaces::GetInterfaces(nsTArray<nsIID>& aArray) {
  aArray = nsTArray<nsIID>{NS_GET_IID(nsIXPCComponents_Interfaces),
                           NS_GET_IID(nsIXPCScriptable)};
  return NS_OK;
}

NS_IMETHODIMP
nsXPCComponents_Interfaces::GetScriptableHelper(nsIXPCScriptable** retval) {
  *retval = nullptr;
  return NS_OK;
}

NS_IMETHODIMP
nsXPCComponents_Interfaces::GetContractID(nsACString& aContractID) {
  aContractID.SetIsVoid(true);
  return NS_ERROR_NOT_AVAILABLE;
}

NS_IMETHODIMP
nsXPCComponents_Interfaces::GetClassDescription(nsACString& aClassDescription) {
  aClassDescription.AssignLiteral("XPCComponents_Interfaces");
  return NS_OK;
}

NS_IMETHODIMP
nsXPCComponents_Interfaces::GetClassID(nsCID** aClassID) {
  *aClassID = nullptr;
  return NS_OK;
}

NS_IMETHODIMP
nsXPCComponents_Interfaces::GetFlags(uint32_t* aFlags) {
  *aFlags = 0;
  return NS_OK;
}

NS_IMETHODIMP
nsXPCComponents_Interfaces::GetClassIDNoAlloc(nsCID* aClassIDNoAlloc) {
  return NS_ERROR_NOT_AVAILABLE;
}

nsXPCComponents_Interfaces::nsXPCComponents_Interfaces() = default;

nsXPCComponents_Interfaces::~nsXPCComponents_Interfaces() {
  // empty
}

NS_IMPL_ISUPPORTS(nsXPCComponents_Interfaces, nsIXPCComponents_Interfaces,
                  nsIXPCScriptable, nsIClassInfo);

// The nsIXPCScriptable map declaration that will generate stubs for us...
#define XPC_MAP_CLASSNAME nsXPCComponents_Interfaces
#define XPC_MAP_QUOTED_CLASSNAME "nsXPCComponents_Interfaces"
#define XPC_MAP_FLAGS                                               \
  (XPC_SCRIPTABLE_WANT_RESOLVE | XPC_SCRIPTABLE_WANT_NEWENUMERATE | \
   XPC_SCRIPTABLE_ALLOW_PROP_MODS_DURING_RESOLVE)
#include "xpc_map_end.h" /* This will #undef the above */

NS_IMETHODIMP
nsXPCComponents_Interfaces::NewEnumerate(nsIXPConnectWrappedNative* wrapper,
                                         JSContext* cx, JSObject* obj,
                                         JS::MutableHandleIdVector properties,
                                         bool enumerableOnly, bool* _retval) {
  if (!properties.reserve(nsXPTInterfaceInfo::InterfaceCount())) {
    *_retval = false;
    return NS_OK;
  }

  for (uint32_t index = 0; index < nsXPTInterfaceInfo::InterfaceCount();
       index++) {
    const nsXPTInterfaceInfo* interface = nsXPTInterfaceInfo::ByIndex(index);
    if (!interface) {
      continue;
    }

    const char* name = interface->Name();
    if (!name) {
      continue;
    }

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
                                    JSContext* cx, JSObject* objArg, jsid idArg,
                                    bool* resolvedp, bool* _retval) {
  RootedObject obj(cx, objArg);
  RootedId id(cx, idArg);

  if (!id.isString()) {
    return NS_OK;
  }

  RootedString str(cx, id.toString());
  JS::UniqueChars name = JS_EncodeStringToLatin1(cx, str);

  // we only allow interfaces by name here
  if (name && name[0] != '{') {
    const nsXPTInterfaceInfo* info = nsXPTInterfaceInfo::ByName(name.get());
    if (!info) {
      return NS_OK;
    }

    RootedValue iidv(cx);
    if (xpc::IfaceID2JSValue(cx, *info, &iidv)) {
      *resolvedp = true;
      *_retval = JS_DefinePropertyById(cx, obj, id, iidv,
                                       JSPROP_ENUMERATE | JSPROP_READONLY |
                                           JSPROP_PERMANENT | JSPROP_RESOLVING);
    }
  }
  return NS_OK;
}

/***************************************************************************/
/***************************************************************************/
/***************************************************************************/

class nsXPCComponents_Classes final : public nsIXPCComponents_Classes,
                                      public nsIXPCScriptable,
                                      public nsIClassInfo {
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
nsXPCComponents_Classes::GetInterfaces(nsTArray<nsIID>& aArray) {
  aArray = nsTArray<nsIID>{NS_GET_IID(nsIXPCComponents_Classes),
                           NS_GET_IID(nsIXPCScriptable)};
  return NS_OK;
}

NS_IMETHODIMP
nsXPCComponents_Classes::GetScriptableHelper(nsIXPCScriptable** retval) {
  *retval = nullptr;
  return NS_OK;
}

NS_IMETHODIMP
nsXPCComponents_Classes::GetContractID(nsACString& aContractID) {
  aContractID.SetIsVoid(true);
  return NS_ERROR_NOT_AVAILABLE;
}

NS_IMETHODIMP
nsXPCComponents_Classes::GetClassDescription(nsACString& aClassDescription) {
  aClassDescription.AssignLiteral("XPCComponents_Classes");
  return NS_OK;
}

NS_IMETHODIMP
nsXPCComponents_Classes::GetClassID(nsCID** aClassID) {
  *aClassID = nullptr;
  return NS_OK;
}

NS_IMETHODIMP
nsXPCComponents_Classes::GetFlags(uint32_t* aFlags) {
  *aFlags = 0;
  return NS_OK;
}

NS_IMETHODIMP
nsXPCComponents_Classes::GetClassIDNoAlloc(nsCID* aClassIDNoAlloc) {
  return NS_ERROR_NOT_AVAILABLE;
}

nsXPCComponents_Classes::nsXPCComponents_Classes() = default;

nsXPCComponents_Classes::~nsXPCComponents_Classes() {
  // empty
}

NS_IMPL_ISUPPORTS(nsXPCComponents_Classes, nsIXPCComponents_Classes,
                  nsIXPCScriptable, nsIClassInfo)

// The nsIXPCScriptable map declaration that will generate stubs for us...
#define XPC_MAP_CLASSNAME nsXPCComponents_Classes
#define XPC_MAP_QUOTED_CLASSNAME "nsXPCComponents_Classes"
#define XPC_MAP_FLAGS                                               \
  (XPC_SCRIPTABLE_WANT_RESOLVE | XPC_SCRIPTABLE_WANT_NEWENUMERATE | \
   XPC_SCRIPTABLE_ALLOW_PROP_MODS_DURING_RESOLVE)
#include "xpc_map_end.h" /* This will #undef the above */

NS_IMETHODIMP
nsXPCComponents_Classes::NewEnumerate(nsIXPConnectWrappedNative* wrapper,
                                      JSContext* cx, JSObject* obj,
                                      JS::MutableHandleIdVector properties,
                                      bool enumerableOnly, bool* _retval) {
  nsCOMPtr<nsIComponentRegistrar> compMgr;
  if (NS_FAILED(NS_GetComponentRegistrar(getter_AddRefs(compMgr))) ||
      !compMgr) {
    return NS_ERROR_UNEXPECTED;
  }

  nsTArray<nsCString> contractIDs;
  if (NS_FAILED(compMgr->GetContractIDs(contractIDs))) {
    return NS_ERROR_UNEXPECTED;
  }

  for (const auto& name : contractIDs) {
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

  return NS_OK;
}

NS_IMETHODIMP
nsXPCComponents_Classes::Resolve(nsIXPConnectWrappedNative* wrapper,
                                 JSContext* cx, JSObject* objArg, jsid idArg,
                                 bool* resolvedp, bool* _retval)

{
  RootedId id(cx, idArg);
  RootedObject obj(cx, objArg);

  RootedValue cidv(cx);
  if (id.isString() && xpc::ContractID2JSValue(cx, id.toString(), &cidv)) {
    *resolvedp = true;
    *_retval = JS_DefinePropertyById(cx, obj, id, cidv,
                                     JSPROP_ENUMERATE | JSPROP_READONLY |
                                         JSPROP_PERMANENT | JSPROP_RESOLVING);
  }
  return NS_OK;
}

/***************************************************************************/
/***************************************************************************/
/***************************************************************************/

// Currently the possible results do not change at runtime, so they are only
// cached once (unlike ContractIDs, CLSIDs, and IIDs)

class nsXPCComponents_Results final : public nsIXPCComponents_Results,
                                      public nsIXPCScriptable,
                                      public nsIClassInfo {
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
nsXPCComponents_Results::GetInterfaces(nsTArray<nsIID>& aArray) {
  aArray = nsTArray<nsIID>{NS_GET_IID(nsIXPCComponents_Results),
                           NS_GET_IID(nsIXPCScriptable)};
  return NS_OK;
}

NS_IMETHODIMP
nsXPCComponents_Results::GetScriptableHelper(nsIXPCScriptable** retval) {
  *retval = nullptr;
  return NS_OK;
}

NS_IMETHODIMP
nsXPCComponents_Results::GetContractID(nsACString& aContractID) {
  aContractID.SetIsVoid(true);
  return NS_ERROR_NOT_AVAILABLE;
}

NS_IMETHODIMP
nsXPCComponents_Results::GetClassDescription(nsACString& aClassDescription) {
  aClassDescription.AssignLiteral("XPCComponents_Results");
  return NS_OK;
}

NS_IMETHODIMP
nsXPCComponents_Results::GetClassID(nsCID** aClassID) {
  *aClassID = nullptr;
  return NS_OK;
}

NS_IMETHODIMP
nsXPCComponents_Results::GetFlags(uint32_t* aFlags) {
  *aFlags = 0;
  return NS_OK;
}

NS_IMETHODIMP
nsXPCComponents_Results::GetClassIDNoAlloc(nsCID* aClassIDNoAlloc) {
  return NS_ERROR_NOT_AVAILABLE;
}

nsXPCComponents_Results::nsXPCComponents_Results() = default;

nsXPCComponents_Results::~nsXPCComponents_Results() {
  // empty
}

NS_IMPL_ISUPPORTS(nsXPCComponents_Results, nsIXPCComponents_Results,
                  nsIXPCScriptable, nsIClassInfo)

// The nsIXPCScriptable map declaration that will generate stubs for us...
#define XPC_MAP_CLASSNAME nsXPCComponents_Results
#define XPC_MAP_QUOTED_CLASSNAME "nsXPCComponents_Results"
#define XPC_MAP_FLAGS                                               \
  (XPC_SCRIPTABLE_WANT_RESOLVE | XPC_SCRIPTABLE_WANT_NEWENUMERATE | \
   XPC_SCRIPTABLE_ALLOW_PROP_MODS_DURING_RESOLVE)
#include "xpc_map_end.h" /* This will #undef the above */

NS_IMETHODIMP
nsXPCComponents_Results::NewEnumerate(nsIXPConnectWrappedNative* wrapper,
                                      JSContext* cx, JSObject* obj,
                                      JS::MutableHandleIdVector properties,
                                      bool enumerableOnly, bool* _retval) {
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
                                 JSContext* cx, JSObject* objArg, jsid idArg,
                                 bool* resolvedp, bool* _retval) {
  RootedObject obj(cx, objArg);
  RootedId id(cx, idArg);
  if (!id.isString()) {
    return NS_OK;
  }

  JS::UniqueChars name = JS_EncodeStringToLatin1(cx, id.toString());
  if (name) {
    const char* rv_name;
    const void* iter = nullptr;
    nsresult rv;
    while (nsXPCException::IterateNSResults(&rv, &rv_name, nullptr, &iter)) {
      if (!strcmp(name.get(), rv_name)) {
        *resolvedp = true;
        if (!JS_DefinePropertyById(cx, obj, id, (uint32_t)rv,
                                   JSPROP_ENUMERATE | JSPROP_READONLY |
                                       JSPROP_PERMANENT | JSPROP_RESOLVING)) {
          return NS_ERROR_UNEXPECTED;
        }
      }
    }
  }
  return NS_OK;
}

/***************************************************************************/
// JavaScript Constructor for nsIJSID objects (Components.ID)

class nsXPCComponents_ID final : public nsIXPCComponents_ID,
                                 public nsIXPCScriptable,
                                 public nsIClassInfo {
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
nsXPCComponents_ID::GetInterfaces(nsTArray<nsIID>& aArray) {
  aArray = nsTArray<nsIID>{NS_GET_IID(nsIXPCComponents_ID),
                           NS_GET_IID(nsIXPCScriptable)};
  return NS_OK;
}

NS_IMETHODIMP
nsXPCComponents_ID::GetScriptableHelper(nsIXPCScriptable** retval) {
  *retval = nullptr;
  return NS_OK;
}

NS_IMETHODIMP
nsXPCComponents_ID::GetContractID(nsACString& aContractID) {
  aContractID.SetIsVoid(true);
  return NS_ERROR_NOT_AVAILABLE;
}

NS_IMETHODIMP
nsXPCComponents_ID::GetClassDescription(nsACString& aClassDescription) {
  aClassDescription.AssignLiteral("XPCComponents_ID");
  return NS_OK;
}

NS_IMETHODIMP
nsXPCComponents_ID::GetClassID(nsCID** aClassID) {
  *aClassID = nullptr;
  return NS_OK;
}

NS_IMETHODIMP
nsXPCComponents_ID::GetFlags(uint32_t* aFlags) {
  *aFlags = 0;
  return NS_OK;
}

NS_IMETHODIMP
nsXPCComponents_ID::GetClassIDNoAlloc(nsCID* aClassIDNoAlloc) {
  return NS_ERROR_NOT_AVAILABLE;
}

nsXPCComponents_ID::nsXPCComponents_ID() = default;

nsXPCComponents_ID::~nsXPCComponents_ID() {
  // empty
}

NS_IMPL_ISUPPORTS(nsXPCComponents_ID, nsIXPCComponents_ID, nsIXPCScriptable,
                  nsIClassInfo)

// The nsIXPCScriptable map declaration that will generate stubs for us...
#define XPC_MAP_CLASSNAME nsXPCComponents_ID
#define XPC_MAP_QUOTED_CLASSNAME "nsXPCComponents_ID"
#define XPC_MAP_FLAGS                                         \
  (XPC_SCRIPTABLE_WANT_CALL | XPC_SCRIPTABLE_WANT_CONSTRUCT | \
   XPC_SCRIPTABLE_WANT_HASINSTANCE |                          \
   XPC_SCRIPTABLE_ALLOW_PROP_MODS_DURING_RESOLVE)
#include "xpc_map_end.h" /* This will #undef the above */

NS_IMETHODIMP
nsXPCComponents_ID::Call(nsIXPConnectWrappedNative* wrapper, JSContext* cx,
                         JSObject* objArg, const CallArgs& args,
                         bool* _retval) {
  RootedObject obj(cx, objArg);
  return CallOrConstruct(wrapper, cx, obj, args, _retval);
}

NS_IMETHODIMP
nsXPCComponents_ID::Construct(nsIXPConnectWrappedNative* wrapper, JSContext* cx,
                              JSObject* objArg, const CallArgs& args,
                              bool* _retval) {
  RootedObject obj(cx, objArg);
  return CallOrConstruct(wrapper, cx, obj, args, _retval);
}

// static
nsresult nsXPCComponents_ID::CallOrConstruct(nsIXPConnectWrappedNative* wrapper,
                                             JSContext* cx, HandleObject obj,
                                             const CallArgs& args,
                                             bool* _retval) {
  // make sure we have at least one arg

  if (args.length() < 1) {
    return ThrowAndFail(NS_ERROR_XPC_NOT_ENOUGH_ARGS, cx, _retval);
  }

  // Prevent non-chrome code from creating ID objects.
  if (!nsContentUtils::IsCallerChrome()) {
    return ThrowAndFail(NS_ERROR_DOM_XPCONNECT_ACCESS_DENIED, cx, _retval);
  }

  // convert the first argument into a string and see if it looks like an id

  JSString* jsstr = ToString(cx, args[0]);
  if (!jsstr) {
    return ThrowAndFail(NS_ERROR_XPC_BAD_ID_STRING, cx, _retval);
  }

  JS::UniqueChars bytes = JS_EncodeStringToLatin1(cx, jsstr);
  if (!bytes) {
    return ThrowAndFail(NS_ERROR_XPC_BAD_ID_STRING, cx, _retval);
  }

  nsID id;
  if (!id.Parse(bytes.get())) {
    return ThrowAndFail(NS_ERROR_XPC_BAD_ID_STRING, cx, _retval);
  }

  // make the new object and return it.

  if (!xpc::ID2JSValue(cx, id, args.rval())) {
    return NS_ERROR_UNEXPECTED;
  }
  return NS_OK;
}

NS_IMETHODIMP
nsXPCComponents_ID::HasInstance(nsIXPConnectWrappedNative* wrapper,
                                JSContext* cx, JSObject* obj, HandleValue val,
                                bool* bp, bool* _retval) {
  if (bp) {
    *bp = xpc::JSValue2ID(cx, val).isSome();
  }
  return NS_OK;
}

/***************************************************************************/
// JavaScript Constructor for Exception objects (Components.Exception)

class nsXPCComponents_Exception final : public nsIXPCComponents_Exception,
                                        public nsIXPCScriptable,
                                        public nsIClassInfo {
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
nsXPCComponents_Exception::GetInterfaces(nsTArray<nsIID>& aArray) {
  aArray = nsTArray<nsIID>{NS_GET_IID(nsIXPCComponents_Exception),
                           NS_GET_IID(nsIXPCScriptable)};
  return NS_OK;
}

NS_IMETHODIMP
nsXPCComponents_Exception::GetScriptableHelper(nsIXPCScriptable** retval) {
  *retval = nullptr;
  return NS_OK;
}

NS_IMETHODIMP
nsXPCComponents_Exception::GetContractID(nsACString& aContractID) {
  aContractID.SetIsVoid(true);
  return NS_ERROR_NOT_AVAILABLE;
}

NS_IMETHODIMP
nsXPCComponents_Exception::GetClassDescription(nsACString& aClassDescription) {
  aClassDescription.AssignLiteral("XPCComponents_Exception");
  return NS_OK;
}

NS_IMETHODIMP
nsXPCComponents_Exception::GetClassID(nsCID** aClassID) {
  *aClassID = nullptr;
  return NS_OK;
}

NS_IMETHODIMP
nsXPCComponents_Exception::GetFlags(uint32_t* aFlags) {
  *aFlags = 0;
  return NS_OK;
}

NS_IMETHODIMP
nsXPCComponents_Exception::GetClassIDNoAlloc(nsCID* aClassIDNoAlloc) {
  return NS_ERROR_NOT_AVAILABLE;
}

nsXPCComponents_Exception::nsXPCComponents_Exception() = default;

nsXPCComponents_Exception::~nsXPCComponents_Exception() {
  // empty
}

NS_IMPL_ISUPPORTS(nsXPCComponents_Exception, nsIXPCComponents_Exception,
                  nsIXPCScriptable, nsIClassInfo)

// The nsIXPCScriptable map declaration that will generate stubs for us...
#define XPC_MAP_CLASSNAME nsXPCComponents_Exception
#define XPC_MAP_QUOTED_CLASSNAME "nsXPCComponents_Exception"
#define XPC_MAP_FLAGS                                         \
  (XPC_SCRIPTABLE_WANT_CALL | XPC_SCRIPTABLE_WANT_CONSTRUCT | \
   XPC_SCRIPTABLE_WANT_HASINSTANCE |                          \
   XPC_SCRIPTABLE_ALLOW_PROP_MODS_DURING_RESOLVE)
#include "xpc_map_end.h" /* This will #undef the above */

NS_IMETHODIMP
nsXPCComponents_Exception::Call(nsIXPConnectWrappedNative* wrapper,
                                JSContext* cx, JSObject* objArg,
                                const CallArgs& args, bool* _retval) {
  RootedObject obj(cx, objArg);
  return CallOrConstruct(wrapper, cx, obj, args, _retval);
}

NS_IMETHODIMP
nsXPCComponents_Exception::Construct(nsIXPConnectWrappedNative* wrapper,
                                     JSContext* cx, JSObject* objArg,
                                     const CallArgs& args, bool* _retval) {
  RootedObject obj(cx, objArg);
  return CallOrConstruct(wrapper, cx, obj, args, _retval);
}

struct MOZ_STACK_CLASS ExceptionArgParser {
  ExceptionArgParser(JSContext* context, nsIXPConnect* xpconnect)
      : eMsg("exception"),
        eResult(NS_ERROR_FAILURE),
        cx(context),
        xpc(xpconnect) {}

  // Public exception parameter values. During construction, these are
  // initialized to the appropriate defaults.
  const char* eMsg;
  nsresult eResult;
  nsCOMPtr<nsIStackFrame> eStack;
  nsCOMPtr<nsISupports> eData;

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
    if (args.length() > 0 && !parseMessage(args[0])) {
      return false;
    }
    if (args.length() > 1) {
      if (args[1].isObject()) {
        RootedObject obj(cx, &args[1].toObject());
        return parseOptionsObject(obj);
      }
      if (!parseResult(args[1])) {
        return false;
      }
    }
    if (args.length() > 2) {
      if (!parseStack(args[2])) {
        return false;
      }
    }
    if (args.length() > 3) {
      if (!parseData(args[3])) {
        return false;
      }
    }
    return true;
  }

 protected:
  /*
   * Parsing helpers.
   */

  bool parseMessage(HandleValue v) {
    JSString* str = ToString(cx, v);
    if (!str) {
      return false;
    }
    messageBytes = JS_EncodeStringToLatin1(cx, str);
    eMsg = messageBytes.get();
    return !!eMsg;
  }

  bool parseResult(HandleValue v) {
    return JS::ToUint32(cx, v, (uint32_t*)&eResult);
  }

  bool parseStack(HandleValue v) {
    if (!v.isObject()) {
      // eStack has already been initialized to null, which is what we want
      // for any non-object values (including null).
      return true;
    }

    RootedObject stackObj(cx, &v.toObject());
    return NS_SUCCEEDED(xpc->WrapJS(cx, stackObj, NS_GET_IID(nsIStackFrame),
                                    getter_AddRefs(eStack)));
  }

  bool parseData(HandleValue v) {
    if (!v.isObject()) {
      // eData has already been initialized to null, which is what we want
      // for any non-object values (including null).
      return true;
    }

    RootedObject obj(cx, &v.toObject());
    return NS_SUCCEEDED(
        xpc->WrapJS(cx, obj, NS_GET_IID(nsISupports), getter_AddRefs(eData)));
  }

  bool parseOptionsObject(HandleObject obj) {
    RootedValue v(cx);

    if (!getOption(obj, "result", &v) || (!v.isUndefined() && !parseResult(v)))
      return false;

    if (!getOption(obj, "stack", &v) || (!v.isUndefined() && !parseStack(v)))
      return false;

    if (!getOption(obj, "data", &v) || (!v.isUndefined() && !parseData(v)))
      return false;

    return true;
  }

  bool getOption(HandleObject obj, const char* name, MutableHandleValue rv) {
    // Look for the property.
    bool found;
    if (!JS_HasProperty(cx, obj, name, &found)) {
      return false;
    }

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
  JS::UniqueChars messageBytes;

  // Various bits and pieces that are helpful to have around.
  JSContext* cx;
  nsIXPConnect* xpc;
};

// static
nsresult nsXPCComponents_Exception::CallOrConstruct(
    nsIXPConnectWrappedNative* wrapper, JSContext* cx, HandleObject obj,
    const CallArgs& args, bool* _retval) {
  nsIXPConnect* xpc = nsIXPConnect::XPConnect();

  MOZ_DIAGNOSTIC_ASSERT(nsContentUtils::IsCallerChrome());

  // Parse the arguments to the Exception constructor.
  ExceptionArgParser parser(cx, xpc);
  if (!parser.parse(args)) {
    return ThrowAndFail(NS_ERROR_XPC_BAD_CONVERT_JS, cx, _retval);
  }

  RefPtr<Exception> e = new Exception(nsCString(parser.eMsg), parser.eResult,
                                      ""_ns, parser.eStack, parser.eData);

  RootedObject newObj(cx);
  if (NS_FAILED(xpc->WrapNative(cx, obj, e, NS_GET_IID(nsIException),
                                newObj.address())) ||
      !newObj) {
    return ThrowAndFail(NS_ERROR_XPC_CANT_CREATE_WN, cx, _retval);
  }

  args.rval().setObject(*newObj);
  return NS_OK;
}

NS_IMETHODIMP
nsXPCComponents_Exception::HasInstance(nsIXPConnectWrappedNative* wrapper,
                                       JSContext* cx, JSObject* obj,
                                       HandleValue val, bool* bp,
                                       bool* _retval) {
  using namespace mozilla::dom;

  if (bp) {
    *bp = (val.isObject() && IS_INSTANCE_OF(Exception, &val.toObject())) ||
          JSValIsInterfaceOfType(cx, val, NS_GET_IID(nsIException));
  }
  return NS_OK;
}

/*******************************************************/
// JavaScript Constructor for nsIXPCConstructor objects (Components.Constructor)

class nsXPCComponents_Constructor final : public nsIXPCComponents_Constructor,
                                          public nsIXPCScriptable,
                                          public nsIClassInfo {
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
  static bool InnerConstructor(JSContext* cx, unsigned argc, JS::Value* vp);
  static nsresult CallOrConstruct(nsIXPConnectWrappedNative* wrapper,
                                  JSContext* cx, HandleObject obj,
                                  const CallArgs& args, bool* _retval);
};

/***************************************************************************/
NS_IMETHODIMP
nsXPCComponents_Constructor::GetInterfaces(nsTArray<nsIID>& aArray) {
  aArray = nsTArray<nsIID>{NS_GET_IID(nsIXPCComponents_Constructor),
                           NS_GET_IID(nsIXPCScriptable)};
  return NS_OK;
}

NS_IMETHODIMP
nsXPCComponents_Constructor::GetScriptableHelper(nsIXPCScriptable** retval) {
  *retval = nullptr;
  return NS_OK;
}

NS_IMETHODIMP
nsXPCComponents_Constructor::GetContractID(nsACString& aContractID) {
  aContractID.SetIsVoid(true);
  return NS_ERROR_NOT_AVAILABLE;
}

NS_IMETHODIMP
nsXPCComponents_Constructor::GetClassDescription(
    nsACString& aClassDescription) {
  aClassDescription.AssignLiteral("XPCComponents_Constructor");
  return NS_OK;
}

NS_IMETHODIMP
nsXPCComponents_Constructor::GetClassID(nsCID** aClassID) {
  *aClassID = nullptr;
  return NS_OK;
}

NS_IMETHODIMP
nsXPCComponents_Constructor::GetFlags(uint32_t* aFlags) {
  *aFlags = 0;
  return NS_OK;
}

NS_IMETHODIMP
nsXPCComponents_Constructor::GetClassIDNoAlloc(nsCID* aClassIDNoAlloc) {
  return NS_ERROR_NOT_AVAILABLE;
}

nsXPCComponents_Constructor::nsXPCComponents_Constructor() = default;

nsXPCComponents_Constructor::~nsXPCComponents_Constructor() {
  // empty
}

NS_IMPL_ISUPPORTS(nsXPCComponents_Constructor, nsIXPCComponents_Constructor,
                  nsIXPCScriptable, nsIClassInfo)

// The nsIXPCScriptable map declaration that will generate stubs for us...
#define XPC_MAP_CLASSNAME nsXPCComponents_Constructor
#define XPC_MAP_QUOTED_CLASSNAME "nsXPCComponents_Constructor"
#define XPC_MAP_FLAGS                                         \
  (XPC_SCRIPTABLE_WANT_CALL | XPC_SCRIPTABLE_WANT_CONSTRUCT | \
   XPC_SCRIPTABLE_WANT_HASINSTANCE |                          \
   XPC_SCRIPTABLE_ALLOW_PROP_MODS_DURING_RESOLVE)
#include "xpc_map_end.h" /* This will #undef the above */

// static
bool nsXPCComponents_Constructor::InnerConstructor(JSContext* cx, unsigned argc,
                                                   JS::Value* vp) {
  CallArgs args = CallArgsFromVp(argc, vp);
  RootedObject callee(cx, &args.callee());

  // Fetch the property name ids, so we can look them up.
  XPCJSRuntime* runtime = XPCJSRuntime::Get();
  HandleId classIDProp = runtime->GetStringID(XPCJSContext::IDX_CLASS_ID);
  HandleId interfaceIDProp =
      runtime->GetStringID(XPCJSContext::IDX_INTERFACE_ID);
  HandleId initializerProp =
      runtime->GetStringID(XPCJSContext::IDX_INITIALIZER);

  // Get properties ('classID', 'interfaceID', and 'initializer') off the
  // constructor object.
  RootedValue classIDv(cx);
  RootedValue interfaceID(cx);
  RootedValue initializer(cx);
  if (!JS_GetPropertyById(cx, callee, classIDProp, &classIDv) ||
      !JS_GetPropertyById(cx, callee, interfaceIDProp, &interfaceID) ||
      !JS_GetPropertyById(cx, callee, initializerProp, &initializer)) {
    return false;
  }
  if (!classIDv.isObject() || !interfaceID.isObject()) {
    XPCThrower::Throw(NS_ERROR_UNEXPECTED, cx);
    return false;
  }

  // Call 'createInstance' on the 'classID' object to create the object.
  RootedValue instancev(cx);
  RootedObject classID(cx, &classIDv.toObject());
  if (!JS_CallFunctionName(cx, classID, "createInstance",
                           HandleValueArray(interfaceID), &instancev)) {
    return false;
  }
  if (!instancev.isObject()) {
    XPCThrower::Throw(NS_ERROR_FAILURE, cx);
    return false;
  }

  // Call the method 'initializer' on the instance, passing in our parameters.
  if (!initializer.isUndefined()) {
    RootedValue dummy(cx);
    RootedValue initfunc(cx);
    RootedId initid(cx);
    RootedObject instance(cx, &instancev.toObject());
    if (!JS_ValueToId(cx, initializer, &initid) ||
        !JS_GetPropertyById(cx, instance, initid, &initfunc) ||
        !JS_CallFunctionValue(cx, instance, initfunc, args, &dummy)) {
      return false;
    }
  }

  args.rval().set(instancev);
  return true;
}

NS_IMETHODIMP
nsXPCComponents_Constructor::Call(nsIXPConnectWrappedNative* wrapper,
                                  JSContext* cx, JSObject* objArg,
                                  const CallArgs& args, bool* _retval) {
  RootedObject obj(cx, objArg);
  return CallOrConstruct(wrapper, cx, obj, args, _retval);
}

NS_IMETHODIMP
nsXPCComponents_Constructor::Construct(nsIXPConnectWrappedNative* wrapper,
                                       JSContext* cx, JSObject* objArg,
                                       const CallArgs& args, bool* _retval) {
  RootedObject obj(cx, objArg);
  return CallOrConstruct(wrapper, cx, obj, args, _retval);
}

// static
nsresult nsXPCComponents_Constructor::CallOrConstruct(
    nsIXPConnectWrappedNative* wrapper, JSContext* cx, HandleObject obj,
    const CallArgs& args, bool* _retval) {
  // make sure we have at least one arg

  if (args.length() < 1) {
    return ThrowAndFail(NS_ERROR_XPC_NOT_ENOUGH_ARGS, cx, _retval);
  }

  // Fetch the property name ids, so we can look them up.
  XPCJSRuntime* runtime = XPCJSRuntime::Get();
  HandleId classIDProp = runtime->GetStringID(XPCJSContext::IDX_CLASS_ID);
  HandleId interfaceIDProp =
      runtime->GetStringID(XPCJSContext::IDX_INTERFACE_ID);
  HandleId initializerProp =
      runtime->GetStringID(XPCJSContext::IDX_INITIALIZER);

  // get the various other object pointers we need

  nsIXPConnect* xpc = nsIXPConnect::XPConnect();
  XPCWrappedNativeScope* scope = ObjectScope(obj);
  nsCOMPtr<nsIXPCComponents> comp;

  if (!xpc || !scope || !(comp = scope->GetComponents())) {
    return ThrowAndFail(NS_ERROR_XPC_UNEXPECTED, cx, _retval);
  }

  // Prevent non-chrome code from creating constructor objects.
  if (!nsContentUtils::IsCallerChrome()) {
    return ThrowAndFail(NS_ERROR_DOM_XPCONNECT_ACCESS_DENIED, cx, _retval);
  }

  JSFunction* ctorfn = JS_NewFunction(cx, InnerConstructor, 0,
                                      JSFUN_CONSTRUCTOR, "XPCOM_Constructor");
  if (!ctorfn) {
    return ThrowAndFail(NS_ERROR_OUT_OF_MEMORY, cx, _retval);
  }

  JS::RootedObject ctor(cx, JS_GetFunctionObject(ctorfn));

  if (args.length() >= 3) {
    // args[2] is an initializer function or property name
    RootedString str(cx, ToString(cx, args[2]));
    if (!JS_DefinePropertyById(
            cx, ctor, initializerProp, str,
            JSPROP_ENUMERATE | JSPROP_READONLY | JSPROP_PERMANENT)) {
      return ThrowAndFail(NS_ERROR_FAILURE, cx, _retval);
    }
  }

  RootedString ifaceName(cx);
  if (args.length() >= 2) {
    ifaceName = ToString(cx, args[1]);
  } else {
    ifaceName = JS_NewStringCopyZ(cx, "nsISupports");
  }

  if (!ifaceName) {
    return ThrowAndFail(NS_ERROR_XPC_BAD_CONVERT_JS, cx, _retval);
  }

  // a new scope to avoid warnings about shadowed names
  {
    nsCOMPtr<nsIXPCComponents_Interfaces> ifaces;
    RootedObject ifacesObj(cx);

    // we do the lookup by asking the Components.interfaces object
    // for the property with this name - i.e. we let its caching of these
    // nsIJSIID objects work for us.

    if (NS_FAILED(comp->GetInterfaces(getter_AddRefs(ifaces))) ||
        NS_FAILED(xpc->WrapNative(cx, obj, ifaces,
                                  NS_GET_IID(nsIXPCComponents_Interfaces),
                                  ifacesObj.address())) ||
        !ifacesObj) {
      return ThrowAndFail(NS_ERROR_XPC_UNEXPECTED, cx, _retval);
    }

    RootedId id(cx);
    if (!JS_StringToId(cx, ifaceName, &id)) {
      return ThrowAndFail(NS_ERROR_XPC_BAD_CONVERT_JS, cx, _retval);
    }

    RootedValue val(cx);
    if (!JS_GetPropertyById(cx, ifacesObj, id, &val) || val.isPrimitive()) {
      return ThrowAndFail(NS_ERROR_XPC_BAD_IID, cx, _retval);
    }

    if (!JS_DefinePropertyById(
            cx, ctor, interfaceIDProp, val,
            JSPROP_ENUMERATE | JSPROP_READONLY | JSPROP_PERMANENT)) {
      return ThrowAndFail(NS_ERROR_FAILURE, cx, _retval);
    }
  }

  // a new scope to avoid warnings about shadowed names
  {
    // argv[0] is a contractid name string

    // we do the lookup by asking the Components.classes object
    // for the property with this name - i.e. we let its caching of these
    // nsIJSCID objects work for us.

    nsCOMPtr<nsIXPCComponents_Classes> classes;
    RootedObject classesObj(cx);

    if (NS_FAILED(comp->GetClasses(getter_AddRefs(classes))) ||
        NS_FAILED(xpc->WrapNative(cx, obj, classes,
                                  NS_GET_IID(nsIXPCComponents_Classes),
                                  classesObj.address())) ||
        !classesObj) {
      return ThrowAndFail(NS_ERROR_XPC_UNEXPECTED, cx, _retval);
    }

    RootedString str(cx, ToString(cx, args[0]));
    RootedId id(cx);
    if (!str || !JS_StringToId(cx, str, &id)) {
      return ThrowAndFail(NS_ERROR_XPC_BAD_CONVERT_JS, cx, _retval);
    }

    RootedValue val(cx);
    if (!JS_GetPropertyById(cx, classesObj, id, &val) || val.isPrimitive()) {
      return ThrowAndFail(NS_ERROR_XPC_BAD_CID, cx, _retval);
    }

    if (!JS_DefinePropertyById(
            cx, ctor, classIDProp, val,
            JSPROP_ENUMERATE | JSPROP_READONLY | JSPROP_PERMANENT)) {
      return ThrowAndFail(NS_ERROR_FAILURE, cx, _retval);
    }
  }

  args.rval().setObject(*ctor);
  return NS_OK;
}

NS_IMETHODIMP
nsXPCComponents_Constructor::HasInstance(nsIXPConnectWrappedNative* wrapper,
                                         JSContext* cx, JSObject* obj,
                                         HandleValue val, bool* isa,
                                         bool* _retval) {
  *isa =
      val.isObject() && JS_IsNativeFunction(&val.toObject(), InnerConstructor);
  return NS_OK;
}

class nsXPCComponents_Utils final : public nsIXPCComponents_Utils,
                                    public nsIXPCScriptable {
 public:
  // all the interface method declarations...
  NS_DECL_ISUPPORTS
  NS_DECL_NSIXPCSCRIPTABLE
  NS_DECL_NSIXPCCOMPONENTS_UTILS

 public:
  nsXPCComponents_Utils() = default;

 private:
  virtual ~nsXPCComponents_Utils() = default;
  nsCOMPtr<nsIXPCComponents_utils_Sandbox> mSandbox;
};

NS_IMPL_ISUPPORTS(nsXPCComponents_Utils, nsIXPCComponents_Utils,
                  nsIXPCScriptable)

// The nsIXPCScriptable map declaration that will generate stubs for us...
#define XPC_MAP_CLASSNAME nsXPCComponents_Utils
#define XPC_MAP_QUOTED_CLASSNAME "nsXPCComponents_Utils"
#define XPC_MAP_FLAGS XPC_SCRIPTABLE_ALLOW_PROP_MODS_DURING_RESOLVE
#include "xpc_map_end.h" /* This will #undef the above */

NS_IMETHODIMP
nsXPCComponents_Utils::GetSandbox(nsIXPCComponents_utils_Sandbox** aSandbox) {
  NS_ENSURE_ARG_POINTER(aSandbox);
  if (!mSandbox) {
    mSandbox = NewSandboxConstructor();
  }

  nsCOMPtr<nsIXPCComponents_utils_Sandbox> rval = mSandbox;
  rval.forget(aSandbox);
  return NS_OK;
}

NS_IMETHODIMP
nsXPCComponents_Utils::CreateServicesCache(JSContext* aCx,
                                           MutableHandleValue aServices) {
  if (JSObject* services = NewJSServices(aCx)) {
    aServices.setObject(*services);
    return NS_OK;
  }
  return NS_ERROR_FAILURE;
}

NS_IMETHODIMP
nsXPCComponents_Utils::PrintStderr(const nsACString& message) {
  printf_stderr("%s", PromiseFlatUTF8String(message).get());
  return NS_OK;
}

NS_IMETHODIMP
nsXPCComponents_Utils::ReportError(HandleValue error, HandleValue stack,
                                   JSContext* cx) {
  // This function shall never fail! Silently eat any failure conditions.

  nsCOMPtr<nsIConsoleService> console(
      do_GetService(NS_CONSOLESERVICE_CONTRACTID));
  if (!console) {
    return NS_OK;
  }

  nsGlobalWindowInner* win = CurrentWindowOrNull(cx);
  const uint64_t innerWindowID = win ? win->WindowID() : 0;

  Rooted<Maybe<Value>> exception(cx, Some(error));
  if (!innerWindowID) {
    // Leak mitigation: nsConsoleService::ClearMessagesForWindowID needs
    // a WindowID for cleanup and exception values could hold arbitrary
    // objects alive.
    exception = Nothing();
  }

  nsCOMPtr<nsIScriptError> scripterr;
  RootedObject errorObj(cx, error.isObject() ? &error.toObject() : nullptr);
  if (errorObj) {
    JS::RootedObject stackVal(cx);
    JS::RootedObject stackGlobal(cx);
    FindExceptionStackForConsoleReport(win, error, nullptr, &stackVal,
                                       &stackGlobal);
    if (stackVal) {
      scripterr = CreateScriptError(win, exception, stackVal, stackGlobal);
    }
  }

  nsString fileName;
  uint32_t lineNo = 0;

  if (!scripterr) {
    RootedObject stackObj(cx);
    RootedObject stackGlobal(cx);
    if (stack.isObject()) {
      if (!JS::IsMaybeWrappedSavedFrame(&stack.toObject())) {
        return NS_ERROR_INVALID_ARG;
      }

      // |stack| might be a wrapper, but it must be same-compartment with
      // the current global.
      stackObj = &stack.toObject();
      stackGlobal = JS::CurrentGlobalOrNull(cx);
      js::AssertSameCompartment(stackObj, stackGlobal);

      JSPrincipals* principals =
          JS::GetRealmPrincipals(js::GetContextRealm(cx));

      if (GetSavedFrameLine(cx, principals, stackObj, &lineNo) !=
          SavedFrameResult::Ok) {
        JS_ClearPendingException(cx);
      }

      RootedString source(cx);
      nsAutoJSString str;
      if (GetSavedFrameSource(cx, principals, stackObj, &source) ==
              SavedFrameResult::Ok &&
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
          MOZ_ASSERT(JS::IsUnwrappedSavedFrame(stackObj));
          stackGlobal = JS::GetNonCCWObjectGlobal(stackObj);
        }
      }
    }

    if (stackObj) {
      scripterr = CreateScriptError(win, exception, stackObj, stackGlobal);
    }
  }

  if (!scripterr) {
    scripterr = CreateScriptError(win, exception, nullptr, nullptr);
  }

  JSErrorReport* err = errorObj ? JS_ErrorFromException(cx, errorObj) : nullptr;
  if (err) {
    // It's a proper JS Error
    nsAutoString fileUni;
    CopyUTF8toUTF16(mozilla::MakeStringSpan(err->filename), fileUni);

    uint32_t column = err->tokenOffset();

    const char16_t* linebuf = err->linebuf();
    uint32_t flags = err->isWarning() ? nsIScriptError::warningFlag
                                      : nsIScriptError::errorFlag;

    nsresult rv = scripterr->InitWithWindowID(
        err->message() ? NS_ConvertUTF8toUTF16(err->message().c_str())
                       : EmptyString(),
        fileUni,
        linebuf ? nsDependentString(linebuf, err->linebufLength())
                : EmptyString(),
        err->lineno, column, flags, "XPConnect JavaScript", innerWindowID,
        innerWindowID == 0 ? true : false);
    NS_ENSURE_SUCCESS(rv, NS_OK);

    console->LogMessage(scripterr);
    return NS_OK;
  }

  // It's not a JS Error object, so we synthesize as best we're able.
  RootedString msgstr(cx, ToString(cx, error));
  if (!msgstr) {
    return NS_OK;
  }

  nsAutoJSString msg;
  if (!msg.init(cx, msgstr)) {
    return NS_OK;
  }

  nsresult rv = scripterr->InitWithWindowID(
      msg, fileName, u""_ns, lineNo, 0, 0, "XPConnect JavaScript",
      innerWindowID, innerWindowID == 0 ? true : false);
  NS_ENSURE_SUCCESS(rv, NS_OK);

  console->LogMessage(scripterr);
  return NS_OK;
}

NS_IMETHODIMP
nsXPCComponents_Utils::EvalInSandbox(
    const nsAString& source, HandleValue sandboxVal, HandleValue version,
    const nsACString& filenameArg, int32_t lineNumber,
    bool enforceFilenameRestrictions, JSContext* cx, uint8_t optionalArgc,
    MutableHandleValue retval) {
  RootedObject sandbox(cx);
  if (!JS_ValueToObject(cx, sandboxVal, &sandbox) || !sandbox) {
    return NS_ERROR_INVALID_ARG;
  }

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
  enforceFilenameRestrictions =
      (optionalArgc >= 4) ? enforceFilenameRestrictions : true;

  return xpc::EvalInSandbox(cx, sandbox, source, filename, lineNo,
                            enforceFilenameRestrictions, retval);
}

NS_IMETHODIMP
nsXPCComponents_Utils::GetUAWidgetScope(nsIPrincipal* principal, JSContext* cx,
                                        MutableHandleValue rval) {
  rval.set(UndefinedValue());

  JSObject* scope = xpc::GetUAWidgetScope(cx, principal);

  rval.set(JS::ObjectValue(*scope));

  return NS_OK;
}

NS_IMETHODIMP
nsXPCComponents_Utils::GetSandboxMetadata(HandleValue sandboxVal, JSContext* cx,
                                          MutableHandleValue rval) {
  if (!sandboxVal.isObject()) {
    return NS_ERROR_INVALID_ARG;
  }

  RootedObject sandbox(cx, &sandboxVal.toObject());
  // We only care about sandboxes here, so CheckedUnwrapStatic is fine.
  sandbox = js::CheckedUnwrapStatic(sandbox);
  if (!sandbox || !xpc::IsSandbox(sandbox)) {
    return NS_ERROR_INVALID_ARG;
  }

  return xpc::GetSandboxMetadata(cx, sandbox, rval);
}

NS_IMETHODIMP
nsXPCComponents_Utils::SetSandboxMetadata(HandleValue sandboxVal,
                                          HandleValue metadataVal,
                                          JSContext* cx) {
  if (!sandboxVal.isObject()) {
    return NS_ERROR_INVALID_ARG;
  }

  RootedObject sandbox(cx, &sandboxVal.toObject());
  // We only care about sandboxes here, so CheckedUnwrapStatic is fine.
  sandbox = js::CheckedUnwrapStatic(sandbox);
  if (!sandbox || !xpc::IsSandbox(sandbox)) {
    return NS_ERROR_INVALID_ARG;
  }

  nsresult rv = xpc::SetSandboxMetadata(cx, sandbox, metadataVal);
  NS_ENSURE_SUCCESS(rv, rv);

  return NS_OK;
}

NS_IMETHODIMP
nsXPCComponents_Utils::Import(const nsACString& registryLocation,
                              HandleValue targetObj, JSContext* cx,
                              uint8_t optionalArgc, MutableHandleValue retval) {
  RefPtr<mozJSComponentLoader> moduleloader = mozJSComponentLoader::Get();
  MOZ_ASSERT(moduleloader);

  AUTO_PROFILER_LABEL_DYNAMIC_NSCSTRING("nsXPCComponents_Utils::Import", OTHER,
                                        registryLocation);

  return moduleloader->ImportInto(registryLocation, targetObj, cx, optionalArgc,
                                  retval);
}

NS_IMETHODIMP
nsXPCComponents_Utils::IsModuleLoaded(const nsACString& aResourceURI,
                                      bool* retval) {
  RefPtr<mozJSComponentLoader> moduleloader = mozJSComponentLoader::Get();
  MOZ_ASSERT(moduleloader);
  return moduleloader->IsModuleLoaded(aResourceURI, retval);
}

NS_IMETHODIMP
nsXPCComponents_Utils::IsJSModuleLoaded(const nsACString& aResourceURI,
                                        bool* retval) {
  RefPtr<mozJSComponentLoader> moduleloader = mozJSComponentLoader::Get();
  MOZ_ASSERT(moduleloader);
  return moduleloader->IsJSModuleLoaded(aResourceURI, retval);
}

NS_IMETHODIMP
nsXPCComponents_Utils::IsESModuleLoaded(const nsACString& aResourceURI,
                                        bool* retval) {
  RefPtr<mozJSComponentLoader> moduleloader = mozJSComponentLoader::Get();
  MOZ_ASSERT(moduleloader);
  return moduleloader->IsESModuleLoaded(aResourceURI, retval);
}

NS_IMETHODIMP
nsXPCComponents_Utils::Unload(const nsACString& registryLocation) {
  RefPtr<mozJSComponentLoader> moduleloader = mozJSComponentLoader::Get();
  MOZ_ASSERT(moduleloader);
  return moduleloader->Unload(registryLocation);
}

NS_IMETHODIMP
nsXPCComponents_Utils::ImportGlobalProperties(HandleValue aPropertyList,
                                              JSContext* cx) {
  // Ensure we're working in the scripted caller's realm. This is not guaranteed
  // to be the current realm because we switch realms when calling cross-realm
  // functions.
  RootedObject global(cx, JS::GetScriptedCallerGlobal(cx));
  MOZ_ASSERT(global);
  js::AssertSameCompartment(cx, global);
  JSAutoRealm ar(cx, global);

  // Don't allow doing this if the global is a Window.
  nsGlobalWindowInner* win;
  if (NS_SUCCEEDED(UNWRAP_NON_WRAPPER_OBJECT(Window, global, win))) {
    return NS_ERROR_NOT_AVAILABLE;
  }

  GlobalProperties options;
  NS_ENSURE_TRUE(aPropertyList.isObject(), NS_ERROR_INVALID_ARG);

  RootedObject propertyList(cx, &aPropertyList.toObject());
  bool isArray;
  if (NS_WARN_IF(!JS::IsArrayObject(cx, propertyList, &isArray))) {
    return NS_ERROR_FAILURE;
  }
  if (NS_WARN_IF(!isArray)) {
    return NS_ERROR_INVALID_ARG;
  }

  if (!options.Parse(cx, propertyList) ||
      !options.DefineInXPCComponents(cx, global)) {
    return NS_ERROR_FAILURE;
  }

  return NS_OK;
}

NS_IMETHODIMP
nsXPCComponents_Utils::GetWeakReference(HandleValue object, JSContext* cx,
                                        xpcIJSWeakReference** _retval) {
  RefPtr<xpcJSWeakReference> ref = new xpcJSWeakReference();
  nsresult rv = ref->Init(cx, object);
  NS_ENSURE_SUCCESS(rv, rv);
  ref.forget(_retval);
  return NS_OK;
}

NS_IMETHODIMP
nsXPCComponents_Utils::ForceGC(JSContext* aCx) {
  PrepareForFullGC(aCx);
  NonIncrementalGC(aCx, GCOptions::Normal, GCReason::COMPONENT_UTILS);
  return NS_OK;
}

NS_IMETHODIMP
nsXPCComponents_Utils::ForceCC(nsICycleCollectorListener* listener) {
  nsJSContext::CycleCollectNow(CCReason::API, listener);
  return NS_OK;
}

NS_IMETHODIMP
nsXPCComponents_Utils::CreateCCLogger(nsICycleCollectorListener** aListener) {
  NS_ENSURE_ARG_POINTER(aListener);
  nsCOMPtr<nsICycleCollectorListener> logger = nsCycleCollector_createLogger();
  logger.forget(aListener);
  return NS_OK;
}

NS_IMETHODIMP
nsXPCComponents_Utils::FinishCC() {
  nsCycleCollector_finishAnyCurrentCollection();
  return NS_OK;
}

NS_IMETHODIMP
nsXPCComponents_Utils::CcSlice(int64_t budget) {
  nsJSContext::RunCycleCollectorWorkSlice(budget);
  return NS_OK;
}

NS_IMETHODIMP
nsXPCComponents_Utils::GetMaxCCSliceTimeSinceClear(int32_t* out) {
  *out = nsJSContext::GetMaxCCSliceTimeSinceClear();
  return NS_OK;
}

NS_IMETHODIMP
nsXPCComponents_Utils::ClearMaxCCTime() {
  nsJSContext::ClearMaxCCSliceTime();
  return NS_OK;
}

NS_IMETHODIMP
nsXPCComponents_Utils::ForceShrinkingGC(JSContext* aCx) {
  PrepareForFullGC(aCx);
  NonIncrementalGC(aCx, GCOptions::Shrink, GCReason::COMPONENT_UTILS);
  return NS_OK;
}

class PreciseGCRunnable : public Runnable {
 public:
  PreciseGCRunnable(nsIScheduledGCCallback* aCallback, bool aShrinking)
      : mozilla::Runnable("PreciseGCRunnable"),
        mCallback(aCallback),
        mShrinking(aShrinking) {}

  NS_IMETHOD Run() override {
    nsJSContext::GarbageCollectNow(
        GCReason::COMPONENT_UTILS,
        mShrinking ? nsJSContext::ShrinkingGC : nsJSContext::NonShrinkingGC);

    mCallback->Callback();
    return NS_OK;
  }

 private:
  nsCOMPtr<nsIScheduledGCCallback> mCallback;
  bool mShrinking;
};

NS_IMETHODIMP
nsXPCComponents_Utils::SchedulePreciseGC(nsIScheduledGCCallback* aCallback) {
  RefPtr<PreciseGCRunnable> event = new PreciseGCRunnable(aCallback, false);
  return NS_DispatchToMainThread(event);
}

NS_IMETHODIMP
nsXPCComponents_Utils::SchedulePreciseShrinkingGC(
    nsIScheduledGCCallback* aCallback) {
  RefPtr<PreciseGCRunnable> event = new PreciseGCRunnable(aCallback, true);
  return NS_DispatchToMainThread(event);
}

NS_IMETHODIMP
nsXPCComponents_Utils::UnlinkGhostWindows() {
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

#ifdef NS_FREE_PERMANENT_DATA
struct IntentionallyLeakedObject {
  MOZ_COUNTED_DEFAULT_CTOR(IntentionallyLeakedObject)

  MOZ_COUNTED_DTOR(IntentionallyLeakedObject)
};
#endif

NS_IMETHODIMP
nsXPCComponents_Utils::IntentionallyLeak() {
#ifdef NS_FREE_PERMANENT_DATA
  Unused << new IntentionallyLeakedObject();
  return NS_OK;
#else
  return NS_ERROR_NOT_IMPLEMENTED;
#endif
}

NS_IMETHODIMP
nsXPCComponents_Utils::GetJSTestingFunctions(JSContext* cx,
                                             MutableHandleValue retval) {
  JSObject* obj = js::GetTestingFunctions(cx);
  if (!obj) {
    return NS_ERROR_XPC_JAVASCRIPT_ERROR;
  }
  retval.setObject(*obj);
  return NS_OK;
}

NS_IMETHODIMP
nsXPCComponents_Utils::GetFunctionSourceLocation(HandleValue funcValue,
                                                 JSContext* cx,
                                                 MutableHandleValue retval) {
  NS_ENSURE_TRUE(funcValue.isObject(), NS_ERROR_INVALID_ARG);

  nsAutoString filename;
  uint32_t lineNumber;
  {
    RootedObject funcObj(cx, UncheckedUnwrap(&funcValue.toObject()));
    JSAutoRealm ar(cx, funcObj);

    Rooted<JSFunction*> func(cx, JS_GetObjectFunction(funcObj));
    NS_ENSURE_TRUE(func, NS_ERROR_INVALID_ARG);

    RootedScript script(cx, JS_GetFunctionScript(cx, func));
    NS_ENSURE_TRUE(func, NS_ERROR_FAILURE);

    AppendUTF8toUTF16(nsDependentCString(JS_GetScriptFilename(script)),
                      filename);
    lineNumber = JS_GetScriptBaseLineNumber(cx, script) + 1;
  }

  RootedObject res(cx, JS_NewPlainObject(cx));
  NS_ENSURE_TRUE(res, NS_ERROR_OUT_OF_MEMORY);

  RootedValue filenameVal(cx);
  if (!xpc::NonVoidStringToJsval(cx, filename, &filenameVal) ||
      !JS_DefineProperty(cx, res, "filename", filenameVal, JSPROP_ENUMERATE)) {
    return NS_ERROR_OUT_OF_MEMORY;
  }

  if (!JS_DefineProperty(cx, res, "lineNumber", lineNumber, JSPROP_ENUMERATE)) {
    return NS_ERROR_OUT_OF_MEMORY;
  }

  retval.setObject(*res);
  return NS_OK;
}

NS_IMETHODIMP
nsXPCComponents_Utils::CallFunctionWithAsyncStack(HandleValue function,
                                                  nsIStackFrame* stack,
                                                  const nsAString& asyncCause,
                                                  JSContext* cx,
                                                  MutableHandleValue retval) {
  nsresult rv;

  if (!stack || asyncCause.IsEmpty()) {
    return NS_ERROR_INVALID_ARG;
  }

  JS::Rooted<JS::Value> asyncStack(cx);
  rv = stack->GetNativeSavedFrame(&asyncStack);
  if (NS_FAILED(rv)) {
    return rv;
  }
  if (!asyncStack.isObject()) {
    JS_ReportErrorASCII(cx, "Must use a native JavaScript stack frame");
    return NS_ERROR_INVALID_ARG;
  }

  JS::Rooted<JSObject*> asyncStackObj(cx, &asyncStack.toObject());

  NS_ConvertUTF16toUTF8 utf8Cause(asyncCause);
  JS::AutoSetAsyncStackForNewCalls sas(
      cx, asyncStackObj, utf8Cause.get(),
      JS::AutoSetAsyncStackForNewCalls::AsyncCallKind::EXPLICIT);

  if (!JS_CallFunctionValue(cx, nullptr, function,
                            JS::HandleValueArray::empty(), retval)) {
    return NS_ERROR_XPC_JAVASCRIPT_ERROR;
  }

  return NS_OK;
}

NS_IMETHODIMP
nsXPCComponents_Utils::GetGlobalForObject(HandleValue object, JSContext* cx,
                                          MutableHandleValue retval) {
  // First argument must be an object.
  if (object.isPrimitive()) {
    return NS_ERROR_XPC_BAD_CONVERT_JS;
  }

  // When getting the global for a cross-compartment wrapper, we really want
  // a wrapper for the foreign global. So we need to unwrap before getting the
  // global and then wrap the result.
  Rooted<JSObject*> obj(cx, &object.toObject());
  obj = JS::GetNonCCWObjectGlobal(js::UncheckedUnwrap(obj));

  if (!JS_WrapObject(cx, &obj)) {
    return NS_ERROR_FAILURE;
  }

  // Get the WindowProxy if necessary.
  obj = js::ToWindowProxyIfWindow(obj);

  retval.setObject(*obj);
  return NS_OK;
}

NS_IMETHODIMP
nsXPCComponents_Utils::IsProxy(HandleValue vobj, JSContext* cx, bool* rval) {
  if (!vobj.isObject()) {
    *rval = false;
    return NS_OK;
  }

  RootedObject obj(cx, &vobj.toObject());
  // We need to do a dynamic unwrap, because we apparently want to treat
  // "failure to unwrap" differently from "not a proxy" (throw for the former,
  // return false for the latter).
  obj = js::CheckedUnwrapDynamic(obj, cx, /* stopAtWindowProxy = */ false);
  NS_ENSURE_TRUE(obj, NS_ERROR_FAILURE);

  *rval = js::IsScriptedProxy(obj);
  return NS_OK;
}

NS_IMETHODIMP
nsXPCComponents_Utils::ExportFunction(HandleValue vfunction, HandleValue vscope,
                                      HandleValue voptions, JSContext* cx,
                                      MutableHandleValue rval) {
  if (!xpc::ExportFunction(cx, vfunction, vscope, voptions, rval)) {
    return NS_ERROR_FAILURE;
  }
  return NS_OK;
}

NS_IMETHODIMP
nsXPCComponents_Utils::CreateObjectIn(HandleValue vobj, HandleValue voptions,
                                      JSContext* cx, MutableHandleValue rval) {
  RootedObject optionsObject(
      cx, voptions.isObject() ? &voptions.toObject() : nullptr);
  CreateObjectInOptions options(cx, optionsObject);
  if (voptions.isObject() && !options.Parse()) {
    return NS_ERROR_FAILURE;
  }

  if (!xpc::CreateObjectIn(cx, vobj, options, rval)) {
    return NS_ERROR_FAILURE;
  }
  return NS_OK;
}

NS_IMETHODIMP
nsXPCComponents_Utils::MakeObjectPropsNormal(HandleValue vobj, JSContext* cx) {
  if (!cx) {
    return NS_ERROR_FAILURE;
  }

  // first argument must be an object
  if (vobj.isPrimitive()) {
    return NS_ERROR_XPC_BAD_CONVERT_JS;
  }

  RootedObject obj(cx, js::UncheckedUnwrap(&vobj.toObject()));
  JSAutoRealm ar(cx, obj);
  Rooted<IdVector> ida(cx, IdVector(cx));
  if (!JS_Enumerate(cx, obj, &ida)) {
    return NS_ERROR_FAILURE;
  }

  RootedId id(cx);
  RootedValue v(cx);
  for (size_t i = 0; i < ida.length(); ++i) {
    id = ida[i];

    if (!JS_GetPropertyById(cx, obj, id, &v)) {
      return NS_ERROR_FAILURE;
    }

    if (v.isPrimitive()) {
      continue;
    }

    RootedObject propobj(cx, &v.toObject());
    // TODO Deal with non-functions.
    if (!js::IsWrapper(propobj) || !JS::IsCallable(propobj)) {
      continue;
    }

    FunctionForwarderOptions forwarderOptions;
    if (!NewFunctionForwarder(cx, id, propobj, forwarderOptions, &v) ||
        !JS_SetPropertyById(cx, obj, id, v))
      return NS_ERROR_FAILURE;
  }

  return NS_OK;
}

NS_IMETHODIMP
nsXPCComponents_Utils::IsDeadWrapper(HandleValue obj, bool* out) {
  *out = false;
  if (obj.isPrimitive()) {
    return NS_ERROR_INVALID_ARG;
  }

  // We should never have cross-compartment wrappers for dead wrappers.
  MOZ_ASSERT_IF(js::IsCrossCompartmentWrapper(&obj.toObject()),
                !JS_IsDeadWrapper(js::UncheckedUnwrap(&obj.toObject())));

  *out = JS_IsDeadWrapper(&obj.toObject());
  return NS_OK;
}

NS_IMETHODIMP
nsXPCComponents_Utils::IsRemoteProxy(HandleValue val, bool* out) {
  if (val.isObject()) {
    *out = dom::IsRemoteObjectProxy(UncheckedUnwrap(&val.toObject()));
    ;
  } else {
    *out = false;
  }
  return NS_OK;
}

NS_IMETHODIMP
nsXPCComponents_Utils::RecomputeWrappers(HandleValue vobj, JSContext* cx) {
  // Determine the compartment of the given object, if any.
  JS::Compartment* c =
      vobj.isObject()
          ? JS::GetCompartment(js::UncheckedUnwrap(&vobj.toObject()))
          : nullptr;

  // If no compartment was given, recompute all.
  if (!c) {
    js::RecomputeWrappers(cx, js::AllCompartments(), js::AllCompartments());
    // Otherwise, recompute wrappers for the given compartment.
  } else {
    js::RecomputeWrappers(cx, js::SingleCompartment(c),
                          js::AllCompartments()) &&
        js::RecomputeWrappers(cx, js::AllCompartments(),
                              js::SingleCompartment(c));
  }

  return NS_OK;
}

NS_IMETHODIMP
nsXPCComponents_Utils::SetWantXrays(HandleValue vscope, JSContext* cx) {
  if (!vscope.isObject()) {
    return NS_ERROR_INVALID_ARG;
  }
  JSObject* scopeObj = js::UncheckedUnwrap(&vscope.toObject());
  MOZ_RELEASE_ASSERT(!AccessCheck::isChrome(scopeObj),
                     "Don't call setWantXrays on system-principal scopes");
  JS::Compartment* compartment = JS::GetCompartment(scopeObj);
  CompartmentPrivate::Get(scopeObj)->wantXrays = true;
  bool ok = js::RecomputeWrappers(cx, js::SingleCompartment(compartment),
                                  js::AllCompartments());
  NS_ENSURE_TRUE(ok, NS_ERROR_FAILURE);
  return NS_OK;
}

NS_IMETHODIMP
nsXPCComponents_Utils::Dispatch(HandleValue runnableArg, HandleValue scope,
                                JSContext* cx) {
  RootedValue runnable(cx, runnableArg);
  // Enter the given realm, if any, and rewrap runnable.
  Maybe<JSAutoRealm> ar;
  if (scope.isObject()) {
    JSObject* scopeObj = js::UncheckedUnwrap(&scope.toObject());
    if (!scopeObj) {
      return NS_ERROR_FAILURE;
    }
    ar.emplace(cx, scopeObj);
    if (!JS_WrapValue(cx, &runnable)) {
      return NS_ERROR_FAILURE;
    }
  }

  // Get an XPCWrappedJS for |runnable|.
  if (!runnable.isObject()) {
    return NS_ERROR_INVALID_ARG;
  }

  RootedObject runnableObj(cx, &runnable.toObject());
  nsCOMPtr<nsIRunnable> run;
  nsresult rv = nsXPConnect::XPConnect()->WrapJS(
      cx, runnableObj, NS_GET_IID(nsIRunnable), getter_AddRefs(run));
  NS_ENSURE_SUCCESS(rv, rv);
  MOZ_ASSERT(run);

  // Dispatch.
  return NS_DispatchToMainThread(run);
}

#define GENERATE_JSCONTEXTOPTION_GETTER_SETTER(_attr, _getter, _setter) \
  NS_IMETHODIMP                                                         \
  nsXPCComponents_Utils::Get##_attr(JSContext* cx, bool* aValue) {      \
    *aValue = ContextOptionsRef(cx)._getter();                          \
    return NS_OK;                                                       \
  }                                                                     \
  NS_IMETHODIMP                                                         \
  nsXPCComponents_Utils::Set##_attr(JSContext* cx, bool aValue) {       \
    ContextOptionsRef(cx)._setter(aValue);                              \
    return NS_OK;                                                       \
  }

GENERATE_JSCONTEXTOPTION_GETTER_SETTER(Strict_mode, strictMode, setStrictMode)

#undef GENERATE_JSCONTEXTOPTION_GETTER_SETTER

NS_IMETHODIMP
nsXPCComponents_Utils::SetGCZeal(int32_t aValue, JSContext* cx) {
#ifdef JS_GC_ZEAL
  JS_SetGCZeal(cx, uint8_t(aValue), JS_DEFAULT_ZEAL_FREQ);
#endif
  return NS_OK;
}

NS_IMETHODIMP
nsXPCComponents_Utils::GetIsInAutomation(bool* aResult) {
  NS_ENSURE_ARG_POINTER(aResult);

  *aResult = xpc::IsInAutomation();
  return NS_OK;
}

NS_IMETHODIMP
nsXPCComponents_Utils::ExitIfInAutomation() {
  NS_ENSURE_TRUE(xpc::IsInAutomation(), NS_ERROR_FAILURE);

  profiler_shutdown(IsFastShutdown::Yes);

  mozilla::AppShutdown::DoImmediateExit();
  return NS_OK;
}

NS_IMETHODIMP
nsXPCComponents_Utils::CrashIfNotInAutomation() {
  xpc::CrashIfNotInAutomation();
  return NS_OK;
}

NS_IMETHODIMP
nsXPCComponents_Utils::NukeSandbox(HandleValue obj, JSContext* cx) {
  AUTO_PROFILER_LABEL("nsXPCComponents_Utils::NukeSandbox", OTHER);
  NS_ENSURE_TRUE(obj.isObject(), NS_ERROR_INVALID_ARG);
  JSObject* wrapper = &obj.toObject();
  NS_ENSURE_TRUE(IsWrapper(wrapper), NS_ERROR_INVALID_ARG);
  RootedObject sb(cx, UncheckedUnwrap(wrapper));
  NS_ENSURE_TRUE(IsSandbox(sb), NS_ERROR_INVALID_ARG);

  xpc::NukeAllWrappersForRealm(cx, GetNonCCWObjectRealm(sb));

  return NS_OK;
}

NS_IMETHODIMP
nsXPCComponents_Utils::BlockScriptForGlobal(HandleValue globalArg,
                                            JSContext* cx) {
  NS_ENSURE_TRUE(globalArg.isObject(), NS_ERROR_INVALID_ARG);
  RootedObject global(cx, UncheckedUnwrap(&globalArg.toObject(),
                                          /* stopAtWindowProxy = */ false));
  NS_ENSURE_TRUE(JS_IsGlobalObject(global), NS_ERROR_INVALID_ARG);
  if (xpc::GetObjectPrincipal(global)->IsSystemPrincipal()) {
    JS_ReportErrorASCII(cx, "Script may not be disabled for system globals");
    return NS_ERROR_FAILURE;
  }
  Scriptability::Get(global).Block();
  return NS_OK;
}

NS_IMETHODIMP
nsXPCComponents_Utils::UnblockScriptForGlobal(HandleValue globalArg,
                                              JSContext* cx) {
  NS_ENSURE_TRUE(globalArg.isObject(), NS_ERROR_INVALID_ARG);
  RootedObject global(cx, UncheckedUnwrap(&globalArg.toObject(),
                                          /* stopAtWindowProxy = */ false));
  NS_ENSURE_TRUE(JS_IsGlobalObject(global), NS_ERROR_INVALID_ARG);
  if (xpc::GetObjectPrincipal(global)->IsSystemPrincipal()) {
    JS_ReportErrorASCII(cx, "Script may not be disabled for system globals");
    return NS_ERROR_FAILURE;
  }
  Scriptability::Get(global).Unblock();
  return NS_OK;
}

NS_IMETHODIMP
nsXPCComponents_Utils::IsOpaqueWrapper(HandleValue obj, bool* aRetval) {
  *aRetval =
      obj.isObject() && xpc::WrapperFactory::IsOpaqueWrapper(&obj.toObject());
  return NS_OK;
}

NS_IMETHODIMP
nsXPCComponents_Utils::IsXrayWrapper(HandleValue obj, bool* aRetval) {
  *aRetval =
      obj.isObject() && xpc::WrapperFactory::IsXrayWrapper(&obj.toObject());
  return NS_OK;
}

NS_IMETHODIMP
nsXPCComponents_Utils::WaiveXrays(HandleValue aVal, JSContext* aCx,
                                  MutableHandleValue aRetval) {
  RootedValue value(aCx, aVal);
  if (!xpc::WrapperFactory::WaiveXrayAndWrap(aCx, &value)) {
    return NS_ERROR_FAILURE;
  }
  aRetval.set(value);
  return NS_OK;
}

NS_IMETHODIMP
nsXPCComponents_Utils::UnwaiveXrays(HandleValue aVal, JSContext* aCx,
                                    MutableHandleValue aRetval) {
  if (!aVal.isObject()) {
    aRetval.set(aVal);
    return NS_OK;
  }

  RootedObject obj(aCx, js::UncheckedUnwrap(&aVal.toObject()));
  if (!JS_WrapObject(aCx, &obj)) {
    return NS_ERROR_FAILURE;
  }
  aRetval.setObject(*obj);
  return NS_OK;
}

NS_IMETHODIMP
nsXPCComponents_Utils::GetClassName(HandleValue aObj, bool aUnwrap,
                                    JSContext* aCx, char** aRv) {
  if (!aObj.isObject()) {
    return NS_ERROR_INVALID_ARG;
  }
  RootedObject obj(aCx, &aObj.toObject());
  if (aUnwrap) {
    obj = js::UncheckedUnwrap(obj, /* stopAtWindowProxy = */ false);
  }
  *aRv = NS_xstrdup(JS::GetClass(obj)->name);
  return NS_OK;
}

NS_IMETHODIMP
nsXPCComponents_Utils::GetDOMClassInfo(const nsAString& aClassName,
                                       nsIClassInfo** aClassInfo) {
  *aClassInfo = nullptr;
  return NS_ERROR_NOT_AVAILABLE;
}

NS_IMETHODIMP
nsXPCComponents_Utils::GetIncumbentGlobal(HandleValue aCallback, JSContext* aCx,
                                          MutableHandleValue aOut) {
  nsCOMPtr<nsIGlobalObject> global = mozilla::dom::GetIncumbentGlobal();
  RootedValue globalVal(aCx);

  if (!global) {
    globalVal = NullValue();
  } else {
    // Note: We rely on the wrap call for outerization.
    globalVal = ObjectValue(*global->GetGlobalJSObject());
    if (!JS_WrapValue(aCx, &globalVal)) {
      return NS_ERROR_FAILURE;
    }
  }

  // Invoke the callback, if passed.
  if (aCallback.isObject()) {
    RootedValue ignored(aCx);
    if (!JS_CallFunctionValue(aCx, nullptr, aCallback,
                              JS::HandleValueArray(globalVal), &ignored)) {
      return NS_ERROR_FAILURE;
    }
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

class WrappedJSHolder : public nsISupports {
  NS_DECL_ISUPPORTS
  WrappedJSHolder() = default;

  RefPtr<nsXPCWrappedJS> mWrappedJS;

 private:
  virtual ~WrappedJSHolder() = default;
};

NS_IMPL_ADDREF(WrappedJSHolder)
NS_IMPL_RELEASE(WrappedJSHolder)

// nsINamed is always supported by nsXPCWrappedJS::DelegatedQueryInterface().
// We expose this interface only for the identity in telemetry analysis.
NS_INTERFACE_TABLE_HEAD(WrappedJSHolder)
  if (aIID.Equals(NS_GET_IID(nsINamed))) {
    return mWrappedJS->QueryInterface(aIID, aInstancePtr);
  }
  NS_INTERFACE_TABLE0(WrappedJSHolder)
NS_INTERFACE_TABLE_TAIL

NS_IMETHODIMP
nsXPCComponents_Utils::GenerateXPCWrappedJS(HandleValue aObj,
                                            HandleValue aScope, JSContext* aCx,
                                            nsISupports** aOut) {
  if (!aObj.isObject()) {
    return NS_ERROR_INVALID_ARG;
  }
  RootedObject obj(aCx, &aObj.toObject());
  RootedObject scope(aCx, aScope.isObject()
                              ? js::UncheckedUnwrap(&aScope.toObject())
                              : CurrentGlobalOrNull(aCx));
  JSAutoRealm ar(aCx, scope);
  if (!JS_WrapObject(aCx, &obj)) {
    return NS_ERROR_FAILURE;
  }

  RefPtr<WrappedJSHolder> holder = new WrappedJSHolder();
  nsresult rv = nsXPCWrappedJS::GetNewOrUsed(
      aCx, obj, NS_GET_IID(nsISupports), getter_AddRefs(holder->mWrappedJS));
  holder.forget(aOut);
  return rv;
}

NS_IMETHODIMP
nsXPCComponents_Utils::GetWatchdogTimestamp(const nsAString& aCategory,
                                            PRTime* aOut) {
  WatchdogTimestampCategory category;
  if (aCategory.EqualsLiteral("ContextStateChange")) {
    category = TimestampContextStateChange;
  } else if (aCategory.EqualsLiteral("WatchdogWakeup")) {
    category = TimestampWatchdogWakeup;
  } else if (aCategory.EqualsLiteral("WatchdogHibernateStart")) {
    category = TimestampWatchdogHibernateStart;
  } else if (aCategory.EqualsLiteral("WatchdogHibernateStop")) {
    category = TimestampWatchdogHibernateStop;
  } else {
    return NS_ERROR_INVALID_ARG;
  }
  *aOut = XPCJSContext::Get()->GetWatchdogTimestamp(category);
  return NS_OK;
}

NS_IMETHODIMP
nsXPCComponents_Utils::GetJSEngineTelemetryValue(JSContext* cx,
                                                 MutableHandleValue rval) {
  RootedObject obj(cx, JS_NewPlainObject(cx));
  if (!obj) {
    return NS_ERROR_OUT_OF_MEMORY;
  }

  // No JS engine telemetry in use at the moment.

  rval.setObject(*obj);
  return NS_OK;
}

bool xpc::CloneInto(JSContext* aCx, HandleValue aValue, HandleValue aScope,
                    HandleValue aOptions, MutableHandleValue aCloned) {
  if (!aScope.isObject()) {
    return false;
  }

  RootedObject scope(aCx, &aScope.toObject());
  // The scope could be a Window, so we need to CheckedUnwrapDynamic.
  scope = js::CheckedUnwrapDynamic(scope, aCx);
  if (!scope) {
    JS_ReportErrorASCII(aCx, "Permission denied to clone object into scope");
    return false;
  }

  if (!aOptions.isUndefined() && !aOptions.isObject()) {
    JS_ReportErrorASCII(aCx, "Invalid argument");
    return false;
  }

  RootedObject optionsObject(
      aCx, aOptions.isObject() ? &aOptions.toObject() : nullptr);
  StackScopedCloneOptions options(aCx, optionsObject);
  if (aOptions.isObject() && !options.Parse()) {
    return false;
  }

  js::AssertSameCompartment(aCx, aValue);
  RootedObject sourceScope(aCx, JS::CurrentGlobalOrNull(aCx));

  {
    JSAutoRealm ar(aCx, scope);
    aCloned.set(aValue);
    if (!StackScopedClone(aCx, options, sourceScope, aCloned)) {
      return false;
    }
  }

  return JS_WrapValue(aCx, aCloned);
}

NS_IMETHODIMP
nsXPCComponents_Utils::CloneInto(HandleValue aValue, HandleValue aScope,
                                 HandleValue aOptions, JSContext* aCx,
                                 MutableHandleValue aCloned) {
  return xpc::CloneInto(aCx, aValue, aScope, aOptions, aCloned)
             ? NS_OK
             : NS_ERROR_FAILURE;
}

NS_IMETHODIMP
nsXPCComponents_Utils::GetWebIDLCallerPrincipal(nsIPrincipal** aResult) {
  // This API may only be when the Entry Settings Object corresponds to a
  // JS-implemented WebIDL call. In all other cases, the value will be null,
  // and we throw.
  nsCOMPtr<nsIPrincipal> callerPrin = mozilla::dom::GetWebIDLCallerPrincipal();
  if (!callerPrin) {
    return NS_ERROR_NOT_AVAILABLE;
  }
  callerPrin.forget(aResult);
  return NS_OK;
}

NS_IMETHODIMP
nsXPCComponents_Utils::GetObjectPrincipal(HandleValue val, JSContext* cx,
                                          nsIPrincipal** result) {
  if (!val.isObject()) {
    return NS_ERROR_INVALID_ARG;
  }
  RootedObject obj(cx, &val.toObject());
  // We need to be able to unwrap to WindowProxy or Location here, so
  // use CheckedUnwrapDynamic.
  obj = js::CheckedUnwrapDynamic(obj, cx);
  MOZ_ASSERT(obj);

  nsCOMPtr<nsIPrincipal> prin = nsContentUtils::ObjectPrincipal(obj);
  prin.forget(result);
  return NS_OK;
}

NS_IMETHODIMP
nsXPCComponents_Utils::GetRealmLocation(HandleValue val, JSContext* cx,
                                        nsACString& result) {
  if (!val.isObject()) {
    return NS_ERROR_INVALID_ARG;
  }
  RootedObject obj(cx, &val.toObject());
  // We need to be able to unwrap to WindowProxy or Location here, so
  // use CheckedUnwrapDynamic.
  obj = js::CheckedUnwrapDynamic(obj, cx);
  MOZ_ASSERT(obj);

  result = xpc::RealmPrivate::Get(obj)->GetLocation();
  return NS_OK;
}

NS_IMETHODIMP
nsXPCComponents_Utils::ReadUTF8File(nsIFile* aFile, nsACString& aResult) {
  NS_ENSURE_TRUE(aFile, NS_ERROR_INVALID_ARG);

  MOZ_TRY_VAR(aResult, URLPreloader::ReadFile(aFile));
  return NS_OK;
}

NS_IMETHODIMP
nsXPCComponents_Utils::ReadUTF8URI(nsIURI* aURI, nsACString& aResult) {
  NS_ENSURE_TRUE(aURI, NS_ERROR_INVALID_ARG);

  MOZ_TRY_VAR(aResult, URLPreloader::ReadURI(aURI));
  return NS_OK;
}

NS_IMETHODIMP
nsXPCComponents_Utils::Now(double* aRetval) {
  TimeStamp start = TimeStamp::ProcessCreation();
  *aRetval = (TimeStamp::Now() - start).ToMilliseconds();
  return NS_OK;
}

NS_IMETHODIMP
nsXPCComponents_Utils::CreateSpellChecker(nsIEditorSpellCheck** aSpellChecker) {
  NS_ENSURE_ARG_POINTER(aSpellChecker);
  nsCOMPtr<nsIEditorSpellCheck> spellChecker = new mozilla::EditorSpellCheck();
  spellChecker.forget(aSpellChecker);
  return NS_OK;
}

NS_IMETHODIMP
nsXPCComponents_Utils::CreateCommandLine(const nsTArray<nsCString>& aArgs,
                                         nsIFile* aWorkingDir, uint32_t aState,
                                         nsISupports** aCommandLine) {
  NS_ENSURE_ARG_MAX(aState, nsICommandLine::STATE_REMOTE_EXPLICIT);
  NS_ENSURE_ARG_POINTER(aCommandLine);

  nsCOMPtr<nsISupports> commandLine = new nsCommandLine();
  nsCOMPtr<nsICommandLineRunner> runner = do_QueryInterface(commandLine);

  nsTArray<const char*> fakeArgv(aArgs.Length() + 2);

  // Prepend a dummy argument for the program name, which will be ignored.
  fakeArgv.AppendElement(nullptr);
  for (const nsCString& arg : aArgs) {
    fakeArgv.AppendElement(arg.get());
  }
  // Append a null terminator.
  fakeArgv.AppendElement(nullptr);

  nsresult rv = runner->Init(fakeArgv.Length() - 1, fakeArgv.Elements(),
                             aWorkingDir, aState);
  NS_ENSURE_SUCCESS(rv, rv);

  commandLine.forget(aCommandLine);
  return NS_OK;
}

NS_IMETHODIMP
nsXPCComponents_Utils::CreateCommandParams(nsICommandParams** aCommandParams) {
  NS_ENSURE_ARG_POINTER(aCommandParams);
  nsCOMPtr<nsICommandParams> commandParams = new nsCommandParams();
  commandParams.forget(aCommandParams);
  return NS_OK;
}

NS_IMETHODIMP
nsXPCComponents_Utils::CreateLoadContext(nsILoadContext** aLoadContext) {
  NS_ENSURE_ARG_POINTER(aLoadContext);
  nsCOMPtr<nsILoadContext> loadContext = ::CreateLoadContext();
  loadContext.forget(aLoadContext);
  return NS_OK;
}

NS_IMETHODIMP
nsXPCComponents_Utils::CreatePrivateLoadContext(nsILoadContext** aLoadContext) {
  NS_ENSURE_ARG_POINTER(aLoadContext);
  nsCOMPtr<nsILoadContext> loadContext = ::CreatePrivateLoadContext();
  loadContext.forget(aLoadContext);
  return NS_OK;
}

NS_IMETHODIMP
nsXPCComponents_Utils::CreatePersistentProperties(
    nsIPersistentProperties** aPersistentProperties) {
  NS_ENSURE_ARG_POINTER(aPersistentProperties);
  nsCOMPtr<nsIPersistentProperties> props = new nsPersistentProperties();
  props.forget(aPersistentProperties);
  return NS_OK;
}

NS_IMETHODIMP
nsXPCComponents_Utils::CreateDocumentEncoder(
    const char* aContentType, nsIDocumentEncoder** aDocumentEncoder) {
  NS_ENSURE_ARG_POINTER(aDocumentEncoder);
  nsCOMPtr<nsIDocumentEncoder> encoder = do_createDocumentEncoder(aContentType);
  encoder.forget(aDocumentEncoder);
  return NS_OK;
}

NS_IMETHODIMP
nsXPCComponents_Utils::CreateHTMLCopyEncoder(
    nsIDocumentEncoder** aDocumentEncoder) {
  NS_ENSURE_ARG_POINTER(aDocumentEncoder);
  nsCOMPtr<nsIDocumentEncoder> encoder = do_createHTMLCopyEncoder();
  encoder.forget(aDocumentEncoder);
  return NS_OK;
}

NS_IMETHODIMP
nsXPCComponents_Utils::GetLoadedModules(nsTArray<nsCString>& aLoadedModules) {
  return mozJSComponentLoader::Get()->GetLoadedJSAndESModules(aLoadedModules);
}

NS_IMETHODIMP
nsXPCComponents_Utils::GetLoadedComponents(
    nsTArray<nsCString>& aLoadedComponents) {
  mozJSComponentLoader::Get()->GetLoadedComponents(aLoadedComponents);
  return NS_OK;
}

NS_IMETHODIMP
nsXPCComponents_Utils::GetLoadedJSModules(
    nsTArray<nsCString>& aLoadedJSModules) {
  mozJSComponentLoader::Get()->GetLoadedModules(aLoadedJSModules);
  return NS_OK;
}

NS_IMETHODIMP
nsXPCComponents_Utils::GetLoadedESModules(
    nsTArray<nsCString>& aLoadedESModules) {
  return mozJSComponentLoader::Get()->GetLoadedESModules(aLoadedESModules);
}

NS_IMETHODIMP
nsXPCComponents_Utils::GetModuleImportStack(const nsACString& aLocation,
                                            nsACString& aRetval) {
  return mozJSComponentLoader::Get()->GetModuleImportStack(aLocation, aRetval);
}

NS_IMETHODIMP
nsXPCComponents_Utils::GetComponentLoadStack(const nsACString& aLocation,
                                             nsACString& aRetval) {
  return mozJSComponentLoader::Get()->GetComponentLoadStack(aLocation, aRetval);
}

/***************************************************************************/
/***************************************************************************/
/***************************************************************************/

nsXPCComponents::nsXPCComponents(XPCWrappedNativeScope* aScope)
    : mScope(aScope) {
  MOZ_ASSERT(aScope, "aScope must not be null");
}

nsXPCComponents::~nsXPCComponents() = default;

void nsXPCComponents::ClearMembers() {
  mInterfaces = nullptr;
  mResults = nullptr;
  mClasses = nullptr;
  mID = nullptr;
  mException = nullptr;
  mConstructor = nullptr;
  mUtils = nullptr;
}

/*******************************************/
#define XPC_IMPL_GET_OBJ_METHOD(_class, _n)                      \
  NS_IMETHODIMP _class::Get##_n(nsIXPCComponents_##_n** a##_n) { \
    NS_ENSURE_ARG_POINTER(a##_n);                                \
    if (!m##_n) m##_n = new nsXPCComponents_##_n();              \
    RefPtr<nsXPCComponents_##_n> ret = m##_n;                    \
    ret.forget(a##_n);                                           \
    return NS_OK;                                                \
  }

XPC_IMPL_GET_OBJ_METHOD(nsXPCComponents, Interfaces)
XPC_IMPL_GET_OBJ_METHOD(nsXPCComponents, Classes)
XPC_IMPL_GET_OBJ_METHOD(nsXPCComponents, Results)
XPC_IMPL_GET_OBJ_METHOD(nsXPCComponents, ID)
XPC_IMPL_GET_OBJ_METHOD(nsXPCComponents, Exception)
XPC_IMPL_GET_OBJ_METHOD(nsXPCComponents, Constructor)
XPC_IMPL_GET_OBJ_METHOD(nsXPCComponents, Utils)

#undef XPC_IMPL_GET_OBJ_METHOD
/*******************************************/

NS_IMETHODIMP
nsXPCComponents::IsSuccessCode(nsresult result, bool* out) {
  *out = NS_SUCCEEDED(result);
  return NS_OK;
}

NS_IMETHODIMP
nsXPCComponents::GetStack(nsIStackFrame** aStack) {
  nsCOMPtr<nsIStackFrame> frame = dom::GetCurrentJSStack();
  frame.forget(aStack);
  return NS_OK;
}

NS_IMETHODIMP
nsXPCComponents::GetManager(nsIComponentManager** aManager) {
  MOZ_ASSERT(aManager, "bad param");
  return NS_GetComponentManager(aManager);
}

NS_IMETHODIMP
nsXPCComponents::GetReturnCode(JSContext* aCx, MutableHandleValue aOut) {
  nsresult res = XPCJSContext::Get()->GetPendingResult();
  aOut.setNumber(static_cast<uint32_t>(res));
  return NS_OK;
}

NS_IMETHODIMP
nsXPCComponents::SetReturnCode(JSContext* aCx, HandleValue aCode) {
  nsresult rv;
  if (!ToUint32(aCx, aCode, (uint32_t*)&rv)) {
    return NS_ERROR_FAILURE;
  }
  XPCJSContext::Get()->SetPendingResult(rv);
  return NS_OK;
}

/**********************************************/

class ComponentsSH : public nsIXPCScriptable {
 public:
  explicit constexpr ComponentsSH(unsigned dummy) {}

  // We don't actually inherit any ref counting infrastructure, but we don't
  // need an nsAutoRefCnt member, so the _INHERITED macro is a hack to avoid
  // having one.
  NS_DECL_ISUPPORTS_INHERITED
  NS_DECL_NSIXPCSCRIPTABLE
  static nsresult Get(nsIXPCScriptable** helper) {
    *helper = &singleton;
    return NS_OK;
  }

 private:
  static ComponentsSH singleton;
};

ComponentsSH ComponentsSH::singleton(0);

// Singleton refcounting.
NS_IMETHODIMP_(MozExternalRefCountType) ComponentsSH::AddRef(void) { return 1; }
NS_IMETHODIMP_(MozExternalRefCountType) ComponentsSH::Release(void) {
  return 1;
}

NS_IMPL_QUERY_INTERFACE(ComponentsSH, nsIXPCScriptable)

#define NSXPCCOMPONENTS_CID                          \
  {                                                  \
    0x3649f405, 0xf0ec, 0x4c28, {                    \
      0xae, 0xb0, 0xaf, 0x9a, 0x51, 0xe4, 0x4c, 0x81 \
    }                                                \
  }

NS_IMPL_CLASSINFO(nsXPCComponents, &ComponentsSH::Get, 0, NSXPCCOMPONENTS_CID)
NS_IMPL_ISUPPORTS_CI(nsXPCComponents, nsIXPCComponents)

// The nsIXPCScriptable map declaration that will generate stubs for us
#define XPC_MAP_CLASSNAME ComponentsSH
#define XPC_MAP_QUOTED_CLASSNAME "nsXPCComponents"
#define XPC_MAP_FLAGS XPC_SCRIPTABLE_WANT_PRECREATE
#include "xpc_map_end.h" /* This will #undef the above */

NS_IMETHODIMP
ComponentsSH::PreCreate(nsISupports* nativeObj, JSContext* cx,
                        JSObject* globalObj, JSObject** parentObj) {
  nsXPCComponents* self = static_cast<nsXPCComponents*>(nativeObj);
  // this should never happen
  if (!self->GetScope()) {
    NS_WARNING(
        "mScope must not be null when nsXPCComponents::PreCreate is called");
    return NS_ERROR_FAILURE;
  }
  *parentObj = self->GetScope()->GetGlobalForWrappedNatives();
  return NS_OK;
}
