/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/ModuleUtils.h"
#include "nsIServiceManager.h"
#include "nsXPIDLString.h"

#include "nsEmbedCID.h"

#include "nsWebBrowser.h"
#include "nsCommandHandler.h"
#include "nsWebBrowserContentPolicy.h"

// Factory Constructors

NS_GENERIC_FACTORY_CONSTRUCTOR(nsWebBrowser)
NS_GENERIC_FACTORY_CONSTRUCTOR(nsWebBrowserContentPolicy)
NS_GENERIC_FACTORY_CONSTRUCTOR(nsCommandHandler)

NS_DEFINE_NAMED_CID(NS_WEBBROWSER_CID);
NS_DEFINE_NAMED_CID(NS_COMMANDHANDLER_CID);
NS_DEFINE_NAMED_CID(NS_WEBBROWSERCONTENTPOLICY_CID);

static const mozilla::Module::CIDEntry kWebBrowserCIDs[] = {
  { &kNS_WEBBROWSER_CID, false, nullptr, nsWebBrowserConstructor },
  { &kNS_COMMANDHANDLER_CID, false, nullptr, nsCommandHandlerConstructor },
  { &kNS_WEBBROWSERCONTENTPOLICY_CID, false, nullptr, nsWebBrowserContentPolicyConstructor },
  { nullptr }
};

static const mozilla::Module::ContractIDEntry kWebBrowserContracts[] = {
  { NS_WEBBROWSER_CONTRACTID, &kNS_WEBBROWSER_CID },
  { NS_COMMANDHANDLER_CONTRACTID, &kNS_COMMANDHANDLER_CID },
  { NS_WEBBROWSERCONTENTPOLICY_CONTRACTID, &kNS_WEBBROWSERCONTENTPOLICY_CID },
  { nullptr }
};

static const mozilla::Module::CategoryEntry kWebBrowserCategories[] = {
  { "content-policy", NS_WEBBROWSERCONTENTPOLICY_CONTRACTID, NS_WEBBROWSERCONTENTPOLICY_CONTRACTID },
  { nullptr }
};

static const mozilla::Module kWebBrowserModule = {
  mozilla::Module::kVersion,
  kWebBrowserCIDs,
  kWebBrowserContracts,
  kWebBrowserCategories
};

NSMODULE_DEFN(Browser_Embedding_Module) = &kWebBrowserModule;
