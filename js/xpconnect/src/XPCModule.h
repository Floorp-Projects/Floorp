/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*-
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

#include "xpcprivate.h"
#include "mozilla/ModuleUtils.h"
#include "mozJSComponentLoader.h"
#include "mozJSSubScriptLoader.h"

/* Module implementation for the xpconnect library. */

#define XPCVARIANT_CONTRACTID "@mozilla.org/xpcvariant;1"
#define XPC_JSCONTEXT_STACK_ITERATOR_CONTRACTID                               \
    "@mozilla.org/js/xpc/ContextStackIterator;1"

// {FE4F7592-C1FC-4662-AC83-538841318803}
#define SCRIPTABLE_INTERFACES_CID                                             \
    {0xfe4f7592, 0xc1fc, 0x4662,                                              \
      { 0xac, 0x83, 0x53, 0x88, 0x41, 0x31, 0x88, 0x3 } }

#define MOZJSSUBSCRIPTLOADER_CONTRACTID "@mozilla.org/moz/jssubscript-loader;1"

NS_GENERIC_FACTORY_CONSTRUCTOR(nsJSID)
NS_GENERIC_FACTORY_CONSTRUCTOR(nsXPCException)
NS_GENERIC_FACTORY_CONSTRUCTOR(nsXPCJSContextStackIterator)
NS_GENERIC_FACTORY_SINGLETON_CONSTRUCTOR(nsIXPConnect,
                                         nsXPConnect::GetSingleton)
NS_GENERIC_FACTORY_CONSTRUCTOR(nsScriptError)

NS_GENERIC_FACTORY_CONSTRUCTOR(mozJSComponentLoader)
NS_GENERIC_FACTORY_CONSTRUCTOR(mozJSSubScriptLoader)

NS_DEFINE_NAMED_CID(NS_JS_ID_CID);
NS_DEFINE_NAMED_CID(NS_XPCONNECT_CID);
NS_DEFINE_NAMED_CID(NS_XPCEXCEPTION_CID);
NS_DEFINE_NAMED_CID(NS_SCRIPTERROR_CID);
NS_DEFINE_NAMED_CID(NS_XPC_JSCONTEXT_STACK_ITERATOR_CID);
NS_DEFINE_NAMED_CID(MOZJSCOMPONENTLOADER_CID);
NS_DEFINE_NAMED_CID(MOZ_JSSUBSCRIPTLOADER_CID);

#define XPCONNECT_CIDENTRIES \
  { &kNS_JS_ID_CID, false, NULL,  nsJSIDConstructor }, \
  { &kNS_XPCONNECT_CID, false, NULL,  nsIXPConnectConstructor }, \
  { &kNS_XPCEXCEPTION_CID, false, NULL, nsXPCExceptionConstructor }, \
  { &kNS_SCRIPTERROR_CID, false, NULL, nsScriptErrorConstructor }, \
  { &kNS_XPC_JSCONTEXT_STACK_ITERATOR_CID, false, NULL, nsXPCJSContextStackIteratorConstructor }, \
  { &kMOZJSCOMPONENTLOADER_CID, false, NULL, mozJSComponentLoaderConstructor }, \
  { &kMOZ_JSSUBSCRIPTLOADER_CID, false, NULL, mozJSSubScriptLoaderConstructor },

#define XPCONNECT_CONTRACTS \
  { XPC_ID_CONTRACTID, &kNS_JS_ID_CID }, \
  { XPC_XPCONNECT_CONTRACTID, &kNS_XPCONNECT_CID }, \
  { XPC_CONTEXT_STACK_CONTRACTID, &kNS_XPCONNECT_CID }, \
  { XPC_RUNTIME_CONTRACTID, &kNS_XPCONNECT_CID }, \
  { XPC_EXCEPTION_CONTRACTID, &kNS_XPCEXCEPTION_CID }, \
  { NS_SCRIPTERROR_CONTRACTID, &kNS_SCRIPTERROR_CID }, \
  { XPC_JSCONTEXT_STACK_ITERATOR_CONTRACTID, &kNS_XPC_JSCONTEXT_STACK_ITERATOR_CID }, \
  { MOZJSCOMPONENTLOADER_CONTRACTID, &kMOZJSCOMPONENTLOADER_CID }, \
  { MOZJSSUBSCRIPTLOADER_CONTRACTID, &kMOZ_JSSUBSCRIPTLOADER_CID },

#define XPCONNECT_CATEGORIES \
  { "module-loader", "js", MOZJSCOMPONENTLOADER_CONTRACTID },

nsresult xpcModuleCtor();
void xpcModuleDtor();
