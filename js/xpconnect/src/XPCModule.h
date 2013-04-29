/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "xpcprivate.h"
#include "mozilla/ModuleUtils.h"
#include "mozJSComponentLoader.h"
#include "mozJSSubScriptLoader.h"

/* Module implementation for the xpconnect library. */

#define XPCVARIANT_CONTRACTID "@mozilla.org/xpcvariant;1"

// {FE4F7592-C1FC-4662-AC83-538841318803}
#define SCRIPTABLE_INTERFACES_CID                                             \
    {0xfe4f7592, 0xc1fc, 0x4662,                                              \
      { 0xac, 0x83, 0x53, 0x88, 0x41, 0x31, 0x88, 0x3 } }

#define MOZJSSUBSCRIPTLOADER_CONTRACTID "@mozilla.org/moz/jssubscript-loader;1"

NS_GENERIC_FACTORY_CONSTRUCTOR(nsJSID)
NS_GENERIC_FACTORY_CONSTRUCTOR(nsXPCException)
NS_GENERIC_FACTORY_SINGLETON_CONSTRUCTOR(nsIXPConnect,
                                         nsXPConnect::GetSingleton)
NS_GENERIC_FACTORY_CONSTRUCTOR(nsScriptError)

NS_GENERIC_FACTORY_CONSTRUCTOR(mozJSComponentLoader)
NS_GENERIC_FACTORY_CONSTRUCTOR(mozJSSubScriptLoader)

NS_DEFINE_NAMED_CID(NS_JS_ID_CID);
NS_DEFINE_NAMED_CID(NS_XPCONNECT_CID);
NS_DEFINE_NAMED_CID(NS_XPCEXCEPTION_CID);
NS_DEFINE_NAMED_CID(NS_SCRIPTERROR_CID);
NS_DEFINE_NAMED_CID(MOZJSCOMPONENTLOADER_CID);
NS_DEFINE_NAMED_CID(MOZ_JSSUBSCRIPTLOADER_CID);

#define XPCONNECT_CIDENTRIES                                                  \
  { &kNS_JS_ID_CID, false, NULL,  nsJSIDConstructor },                        \
  { &kNS_XPCONNECT_CID, false, NULL,  nsIXPConnectConstructor },              \
  { &kNS_XPCEXCEPTION_CID, false, NULL, nsXPCExceptionConstructor },          \
  { &kNS_SCRIPTERROR_CID, false, NULL, nsScriptErrorConstructor },            \
  { &kMOZJSCOMPONENTLOADER_CID, false, NULL, mozJSComponentLoaderConstructor },\
  { &kMOZ_JSSUBSCRIPTLOADER_CID, false, NULL, mozJSSubScriptLoaderConstructor },

#define XPCONNECT_CONTRACTS                                                   \
  { XPC_ID_CONTRACTID, &kNS_JS_ID_CID },                                      \
  { XPC_XPCONNECT_CONTRACTID, &kNS_XPCONNECT_CID },                           \
  { XPC_CONTEXT_STACK_CONTRACTID, &kNS_XPCONNECT_CID },                       \
  { XPC_RUNTIME_CONTRACTID, &kNS_XPCONNECT_CID },                             \
  { XPC_EXCEPTION_CONTRACTID, &kNS_XPCEXCEPTION_CID },                        \
  { NS_SCRIPTERROR_CONTRACTID, &kNS_SCRIPTERROR_CID },                        \
  { MOZJSCOMPONENTLOADER_CONTRACTID, &kMOZJSCOMPONENTLOADER_CID },            \
  { MOZJSSUBSCRIPTLOADER_CONTRACTID, &kMOZ_JSSUBSCRIPTLOADER_CID },

#define XPCONNECT_CATEGORIES \
  { "module-loader", "js", MOZJSCOMPONENTLOADER_CONTRACTID },

nsresult xpcModuleCtor();
void xpcModuleDtor();
