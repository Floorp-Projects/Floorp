/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "xpcprivate.h"
#include "mozilla/ModuleUtils.h"
#include "mozJSSubScriptLoader.h"

/* Module implementation for the xpconnect library. */

#define XPCVARIANT_CONTRACTID "@mozilla.org/xpcvariant;1"

// {FE4F7592-C1FC-4662-AC83-538841318803}
#define SCRIPTABLE_INTERFACES_CID                   \
  {                                                 \
    0xfe4f7592, 0xc1fc, 0x4662, {                   \
      0xac, 0x83, 0x53, 0x88, 0x41, 0x31, 0x88, 0x3 \
    }                                               \
  }

#define MOZJSSUBSCRIPTLOADER_CONTRACTID "@mozilla.org/moz/jssubscript-loader;1"

NS_GENERIC_FACTORY_CONSTRUCTOR(nsJSID)
NS_GENERIC_FACTORY_CONSTRUCTOR(mozJSSubScriptLoader)

NS_DEFINE_NAMED_CID(NS_JS_ID_CID);
NS_DEFINE_NAMED_CID(MOZ_JSSUBSCRIPTLOADER_CID);

#define XPCONNECT_CIDENTRIES                           \
  {&kNS_JS_ID_CID, false, nullptr, nsJSIDConstructor}, \
      {&kMOZ_JSSUBSCRIPTLOADER_CID, false, nullptr,    \
       mozJSSubScriptLoaderConstructor},

#define XPCONNECT_CONTRACTS \
  {MOZJSSUBSCRIPTLOADER_CONTRACTID, &kMOZ_JSSUBSCRIPTLOADER_CID},

nsresult xpcModuleCtor();
void xpcModuleDtor();
