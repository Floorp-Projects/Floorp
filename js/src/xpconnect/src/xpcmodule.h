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

#ifndef xpcmodule_h___
#define xpcmodule_h___

#include "xpcprivate.h"
#ifdef MOZ_JSLOADER
#include "mozJSLoaderConstructors.h"
#endif

/* Module implementation for the xpconnect library. */

// {DC524540-487E-4501-9AC7-AAA784B17C1C}
#define XPCVARIANT_CID                                                        \
    {0xdc524540, 0x487e, 0x4501,                                              \
      { 0x9a, 0xc7, 0xaa, 0xa7, 0x84, 0xb1, 0x7c, 0x1c } }

#define XPCVARIANT_CONTRACTID "@mozilla.org/xpcvariant;1"
#define XPC_JSCONTEXT_STACK_ITERATOR_CONTRACTID                               \
    "@mozilla.org/js/xpc/ContextStackIterator;1"

// {FE4F7592-C1FC-4662-AC83-538841318803}
#define SCRIPTABLE_INTERFACES_CID                                             \
    {0xfe4f7592, 0xc1fc, 0x4662,                                              \
      { 0xac, 0x83, 0x53, 0x88, 0x41, 0x31, 0x88, 0x3 } }


#define XPCONNECT_GENERAL_FACTORIES                                           \
  NS_DECL_CLASSINFO(XPCVariant)                                               \
  NS_DECL_CLASSINFO(nsXPCException)                                           \
                                                                              \
  NS_GENERIC_FACTORY_CONSTRUCTOR(nsJSID)                                      \
  NS_GENERIC_FACTORY_CONSTRUCTOR(nsXPCException)                              \
  NS_GENERIC_FACTORY_CONSTRUCTOR(nsXPCJSContextStackIterator)                 \
  NS_GENERIC_FACTORY_SINGLETON_CONSTRUCTOR(nsIXPConnect,                      \
                                           nsXPConnect::GetSingleton)         \
  NS_GENERIC_FACTORY_CONSTRUCTOR(nsScriptError)                               \
  NS_GENERIC_FACTORY_CONSTRUCTOR(nsXPCComponents_Interfaces)


#ifdef XPC_IDISPATCH_SUPPORT

#define XPCONNECT_FACTORIES                                                   \
  NS_GENERIC_FACTORY_SINGLETON_CONSTRUCTOR(nsIDispatchSupport,                \
                                           nsDispatchSupport::GetSingleton)   \
  XPCONNECT_GENERAL_FACTORIES

#else

#define XPCONNECT_FACTORIES XPCONNECT_GENERAL_FACTORIES

#endif // XPC_IDISPATCH_SUPPORT


#ifdef XPCONNECT_STANDALONE
#define NO_SUBSCRIPT_LOADER
#endif


#define XPCONNECT_GENERAL_COMPONENTS                                          \
  {                                                                           \
    nsnull,                                                                   \
    NS_JS_ID_CID,                                                             \
    XPC_ID_CONTRACTID,                                                        \
    nsJSIDConstructor                                                         \
  },                                                                          \
  {                                                                           \
    nsnull,                                                                   \
    NS_XPCONNECT_CID,                                                         \
    XPC_XPCONNECT_CONTRACTID,                                                 \
    nsIXPConnectConstructor                                                   \
  },                                                                          \
  {                                                                           \
    nsnull,                                                                   \
    NS_XPC_THREAD_JSCONTEXT_STACK_CID,                                        \
    XPC_CONTEXT_STACK_CONTRACTID,                                             \
    nsIXPConnectConstructor                                                   \
  },                                                                          \
  {                                                                           \
    nsnull,                                                                   \
    NS_XPCEXCEPTION_CID,                                                      \
    XPC_EXCEPTION_CONTRACTID,                                                 \
    nsXPCExceptionConstructor,                                                \
    nsnull,                                                                   \
    nsnull,                                                                   \
    nsnull,                                                                   \
    NS_CI_INTERFACE_GETTER_NAME(nsXPCException),                              \
    nsnull,                                                                   \
    &NS_CLASSINFO_NAME(nsXPCException),                                       \
    nsIClassInfo::DOM_OBJECT                                                  \
  },                                                                          \
  {                                                                           \
    nsnull,                                                                   \
    NS_JS_RUNTIME_SERVICE_CID,                                                \
    XPC_RUNTIME_CONTRACTID,                                                   \
    nsIXPConnectConstructor                                                   \
  },                                                                          \
  {                                                                           \
    NS_SCRIPTERROR_CLASSNAME,                                                 \
    NS_SCRIPTERROR_CID,                                                       \
    NS_SCRIPTERROR_CONTRACTID,                                                \
    nsScriptErrorConstructor                                                  \
  },                                                                          \
  {                                                                           \
    nsnull,                                                                   \
    SCRIPTABLE_INTERFACES_CID,                                                \
    NS_SCRIPTABLE_INTERFACES_CONTRACTID,                                      \
    nsXPCComponents_InterfacesConstructor,                                    \
    nsnull,                                                                   \
    nsnull,                                                                   \
    nsnull,                                                                   \
    nsnull,                                                                   \
    nsnull,                                                                   \
    nsnull,                                                                   \
    nsIClassInfo::THREADSAFE                                                  \
  },                                                                          \
  {                                                                           \
    nsnull,                                                                   \
    XPCVARIANT_CID,                                                           \
    XPCVARIANT_CONTRACTID,                                                    \
    nsnull,                                                                   \
    nsnull,                                                                   \
    nsnull,                                                                   \
    nsnull,                                                                   \
    NS_CI_INTERFACE_GETTER_NAME(XPCVariant),                                  \
    nsnull,                                                                   \
    &NS_CLASSINFO_NAME(XPCVariant)                                            \
  },                                                                          \
  {                                                                           \
    nsnull,                                                                   \
    NS_XPC_JSCONTEXT_STACK_ITERATOR_CID,                                      \
    XPC_JSCONTEXT_STACK_ITERATOR_CONTRACTID,                                  \
    nsXPCJSContextStackIteratorConstructor                                    \
  }

// jsloader stuff
#ifdef MOZ_JSLOADER

#define XPCONNECT_LOADER_COMPONENTS                                           \
  {                                                                           \
    "JS component loader",                                                    \
    MOZJSCOMPONENTLOADER_CID,                                                 \
    MOZJSCOMPONENTLOADER_CONTRACTID,                                          \
    mozJSComponentLoaderConstructor,                                          \
    RegisterJSLoader,                                                         \
    UnregisterJSLoader                                                        \
  },                                                                          \
  XPCONNECT_SUBSCRIPT_LOADER_COMPONENTS

#ifdef NO_SUBSCRIPT_LOADER

#define XPCONNECT_SUBSCRIPT_LOADER_COMPONENTS

#else

#define XPCONNECT_SUBSCRIPT_LOADER_COMPONENTS                                 \
  {                                                                           \
    "JS subscript loader",                                                    \
    MOZ_JSSUBSCRIPTLOADER_CID,                                                \
    mozJSSubScriptLoadContractID,                                             \
    mozJSSubScriptLoaderConstructor                                           \
  },

#endif // NO_SUBSCRIPT_LOADER

#else

#define XPCONNECT_LOADER_COMPONENTS

#endif // MOZ_JSLOADER


#ifdef XPC_IDISPATCH_SUPPORT

#define XPCONNECT_IDISPATCH_COMPONENTS                                        \
  {                                                                           \
    nsnull,                                                                   \
    NS_IDISPATCH_SUPPORT_CID,                                                 \
    NS_IDISPATCH_SUPPORT_CONTRACTID,                                          \
    nsIDispatchSupportConstructor                                             \
  },

#else

#define XPCONNECT_IDISPATCH_COMPONENTS

#endif // XPC_IDISPATCH_SUPPORT


#define XPCONNECT_COMPONENTS                                                  \
  XPCONNECT_LOADER_COMPONENTS                                                 \
  XPCONNECT_IDISPATCH_COMPONENTS                                              \
  XPCONNECT_GENERAL_COMPONENTS

extern nsresult xpcModuleCtor(nsIModule* self);
extern void xpcModuleDtor(nsIModule*);

#endif /* xpcmodule_h___ */

