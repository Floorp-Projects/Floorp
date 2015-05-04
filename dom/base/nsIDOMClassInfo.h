/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef nsIDOMClassInfo_h___
#define nsIDOMClassInfo_h___

#include "nsIClassInfoImpl.h"
#include "nsDOMClassInfoID.h"
#include "nsIXPCScriptable.h"
#include "nsIServiceManager.h"
#include "nsIDOMScriptObjectFactory.h"
#include "nsDOMCID.h"

#define DOM_BASE_SCRIPTABLE_FLAGS                                          \
  (nsIXPCScriptable::USE_JSSTUB_FOR_ADDPROPERTY |                          \
   nsIXPCScriptable::USE_JSSTUB_FOR_DELPROPERTY |                          \
   nsIXPCScriptable::USE_JSSTUB_FOR_SETPROPERTY |                          \
   nsIXPCScriptable::ALLOW_PROP_MODS_TO_PROTOTYPE |                        \
   nsIXPCScriptable::DONT_ASK_INSTANCE_FOR_SCRIPTABLE |                    \
   nsIXPCScriptable::DONT_REFLECT_INTERFACE_NAMES)

#define DEFAULT_SCRIPTABLE_FLAGS                                           \
  (DOM_BASE_SCRIPTABLE_FLAGS |                                             \
   nsIXPCScriptable::WANT_RESOLVE |                                        \
   nsIXPCScriptable::WANT_PRECREATE)

#define DOM_DEFAULT_SCRIPTABLE_FLAGS                                       \
  (DEFAULT_SCRIPTABLE_FLAGS |                                              \
   nsIXPCScriptable::DONT_ENUM_QUERY_INTERFACE |                           \
   nsIXPCScriptable::CLASSINFO_INTERFACES_ONLY)


#ifdef MOZILLA_INTERNAL_API

// See nsDOMClassInfoID.h

#else

#define NS_DOM_INTERFACE_MAP_ENTRY_CLASSINFO(_class)                       \
  if (aIID.Equals(NS_GET_IID(nsIClassInfo)) ||                             \
      aIID.Equals(NS_GET_IID(nsXPCClassInfo))) {                           \
    static NS_DEFINE_CID(kDOMSOF_CID, NS_DOM_SCRIPT_OBJECT_FACTORY_CID);   \
                                                                           \
    nsresult rv;                                                           \
    nsCOMPtr<nsIDOMScriptObjectFactory> sof(do_GetService(kDOMSOF_CID,     \
                                                          &rv));           \
    if (NS_FAILED(rv)) {                                                   \
      *aInstancePtr = nullptr;                                              \
      return rv;                                                           \
    }                                                                      \
                                                                           \
    foundInterface =                                                       \
      sof->GetClassInfoInstance(eDOMClassInfo_##_class##_id);              \
  } else

#endif /* MOZILLA_INTERNAL_API */

// Looks up the nsIClassInfo for a class name registered with the 
// nsScriptNamespaceManager. Remember to release NS_CLASSINFO_NAME(_class)
// (eg. when your module unloads).
#define NS_INTERFACE_MAP_ENTRY_EXTERNAL_DOM_CLASSINFO(_class)              \
  if (aIID.Equals(NS_GET_IID(nsIClassInfo)) ||                             \
      aIID.Equals(NS_GET_IID(nsXPCClassInfo))) {                           \
    extern nsISupports *NS_CLASSINFO_NAME(_class);                         \
    if (NS_CLASSINFO_NAME(_class)) {                                       \
      foundInterface = NS_CLASSINFO_NAME(_class);                          \
    } else {                                                               \
      static NS_DEFINE_CID(kDOMSOF_CID, NS_DOM_SCRIPT_OBJECT_FACTORY_CID); \
                                                                           \
      nsresult rv;                                                         \
      nsCOMPtr<nsIDOMScriptObjectFactory> sof(do_GetService(kDOMSOF_CID,   \
                                                            &rv));         \
      if (NS_FAILED(rv)) {                                                 \
        *aInstancePtr = nullptr;                                            \
        return rv;                                                         \
      }                                                                    \
                                                                           \
      foundInterface =                                                     \
        sof->GetExternalClassInfoInstance(NS_LITERAL_STRING(#_class));     \
                                                                           \
      if (foundInterface) {                                                \
        NS_CLASSINFO_NAME(_class) = foundInterface;                        \
        NS_CLASSINFO_NAME(_class)->AddRef();                               \
      }                                                                    \
    }                                                                      \
  } else


#define NS_DECL_DOM_CLASSINFO(_class) \
  nsISupports *NS_CLASSINFO_NAME(_class) = nullptr;

// {891a7b01-1b61-11d6-a7f2-f690b638899c}
#define NS_IDOMCI_EXTENSION_IID  \
{ 0x891a7b01, 0x1b61, 0x11d6, \
{ 0xa7, 0xf2, 0xf6, 0x90, 0xb6, 0x38, 0x89, 0x9c } }

class nsIDOMScriptObjectFactory;

class nsIDOMCIExtension : public nsISupports {
public:  
  NS_DECLARE_STATIC_IID_ACCESSOR(NS_IDOMCI_EXTENSION_IID)

  NS_IMETHOD RegisterDOMCI(const char* aName,
                           nsIDOMScriptObjectFactory* aDOMSOFactory) = 0;
};

NS_DEFINE_STATIC_IID_ACCESSOR(nsIDOMCIExtension, NS_IDOMCI_EXTENSION_IID)

#define NS_DOMCI_EXTENSION_NAME(_module) ns##_module##DOMCIExtension
#define NS_DOMCI_EXTENSION_CONSTRUCTOR(_module) \
  ns##_module##DOMCIExtensionConstructor
#define NS_DOMCI_EXTENSION_CONSTRUCTOR_IMP(_extension) \
  NS_GENERIC_FACTORY_CONSTRUCTOR(_extension)

#define NS_DOMCI_EXTENSION(_module)                                       \
class NS_DOMCI_EXTENSION_NAME(_module) : public nsIDOMCIExtension         \
{                                                                         \
public:                                                                   \
  NS_DOMCI_EXTENSION_NAME(_module)();                                     \
  virtual ~NS_DOMCI_EXTENSION_NAME(_module)();                            \
                                                                          \
  NS_DECL_ISUPPORTS                                                       \
                                                                          \
  NS_IMETHOD RegisterDOMCI(const char* aName,                             \
                           nsIDOMScriptObjectFactory* aDOMSOFactory);     \
};                                                                        \
                                                                          \
NS_DOMCI_EXTENSION_CONSTRUCTOR_IMP(NS_DOMCI_EXTENSION_NAME(_module))      \
                                                                          \
NS_DOMCI_EXTENSION_NAME(_module)::NS_DOMCI_EXTENSION_NAME(_module)()      \
{                                                                         \
}                                                                         \
                                                                          \
NS_DOMCI_EXTENSION_NAME(_module)::~NS_DOMCI_EXTENSION_NAME(_module)()     \
{                                                                         \
}                                                                         \
                                                                          \
NS_IMPL_ISUPPORTS(NS_DOMCI_EXTENSION_NAME(_module), nsIDOMCIExtension)    \
                                                                          \
NS_IMETHODIMP                                                             \
NS_DOMCI_EXTENSION_NAME(_module)::RegisterDOMCI(const char* aName,        \
                                                nsIDOMScriptObjectFactory* aDOMSOFactory) \
{

#define NS_DOMCI_EXTENSION_ENTRY_BEGIN(_class)                            \
  if (nsCRT::strcmp(aName, #_class) == 0) {                               \
    static const nsIID* interfaces[] = {

#define NS_DOMCI_EXTENSION_ENTRY_INTERFACE(_interface)                    \
      &NS_GET_IID(_interface),

// Don't forget to register the primary interface (_proto) in the 
// JAVASCRIPT_DOM_INTERFACE category, or prototypes for this class
// won't work (except if the interface name starts with nsIDOM).
#define NS_DOMCI_EXTENSION_ENTRY_END_HELPER(_class, _proto, _hasclassif,  \
                                            _constructorcid)              \
      nullptr                                                              \
    };                                                                    \
    aDOMSOFactory->RegisterDOMClassInfo(#_class, nullptr, _proto,          \
                                        interfaces,                       \
                                        DOM_DEFAULT_SCRIPTABLE_FLAGS,     \
                                        _hasclassif, _constructorcid);    \
    return NS_OK;                                                         \
  }

#define NS_DOMCI_EXTENSION_ENTRY_END(_class, _proto, _hasclassif,         \
                                     _constructorcid)                     \
  NS_DOMCI_EXTENSION_ENTRY_END_HELPER(_class, &NS_GET_IID(_proto),        \
                                      _hasclassif, _constructorcid)

#define NS_DOMCI_EXTENSION_ENTRY_END_NO_PRIMARY_IF(_class, _hasclassif,   \
                                                   _constructorcid)       \
  NS_DOMCI_EXTENSION_ENTRY_END_HELPER(_class, nullptr, _hasclassif,        \
                                      _constructorcid)

#define NS_DOMCI_EXTENSION_END                                            \
  return NS_ERROR_FAILURE;                                                \
}


#endif /* nsIDOMClassInfo_h___ */
