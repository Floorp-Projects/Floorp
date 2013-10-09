/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nscore.h"
#include "nsLayoutDebugCIID.h"
#include "mozilla/ModuleUtils.h"
#include "nsIFactory.h"
#include "nsISupports.h"
#include "nsRegressionTester.h"
#include "nsLayoutDebuggingTools.h"
#include "nsLayoutDebugCLH.h"
#include "nsIServiceManager.h"

NS_GENERIC_FACTORY_CONSTRUCTOR(nsRegressionTester)
NS_GENERIC_FACTORY_CONSTRUCTOR(nsLayoutDebuggingTools)
NS_GENERIC_FACTORY_CONSTRUCTOR(nsLayoutDebugCLH)

NS_DEFINE_NAMED_CID(NS_REGRESSION_TESTER_CID);
NS_DEFINE_NAMED_CID(NS_LAYOUT_DEBUGGINGTOOLS_CID);
NS_DEFINE_NAMED_CID(NS_LAYOUTDEBUGCLH_CID);

static const mozilla::Module::CIDEntry kLayoutDebugCIDs[] = {
  { &kNS_REGRESSION_TESTER_CID, false, nullptr, nsRegressionTesterConstructor },
  { &kNS_LAYOUT_DEBUGGINGTOOLS_CID, false, nullptr, nsLayoutDebuggingToolsConstructor },
  { &kNS_LAYOUTDEBUGCLH_CID, false, nullptr, nsLayoutDebugCLHConstructor },
  { nullptr }
};

static const mozilla::Module::ContractIDEntry kLayoutDebugContracts[] = {
  { "@mozilla.org/layout-debug/regressiontester;1", &kNS_REGRESSION_TESTER_CID },
  { NS_LAYOUT_DEBUGGINGTOOLS_CONTRACTID, &kNS_LAYOUT_DEBUGGINGTOOLS_CID },
  { "@mozilla.org/commandlinehandler/general-startup;1?type=layoutdebug", &kNS_LAYOUTDEBUGCLH_CID },
  { nullptr }
};

static const mozilla::Module::CategoryEntry kLayoutDebugCategories[] = {
  { "command-line-handler", "m-layoutdebug", "@mozilla.org/commandlinehandler/general-startup;1?type=layoutdebug" },
  { nullptr }
};

static const mozilla::Module kLayoutDebugModule = {
  mozilla::Module::kVersion,
  kLayoutDebugCIDs,
  kLayoutDebugContracts,
  kLayoutDebugCategories
};

NSMODULE_DEFN(nsLayoutDebugModule) = &kLayoutDebugModule;
