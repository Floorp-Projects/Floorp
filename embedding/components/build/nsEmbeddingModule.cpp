/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/ModuleUtils.h"
#include "nsDialogParamBlock.h"
#include "nsWindowWatcher.h"
#include "nsAppStartupNotifier.h"
#include "nsFind.h"
#include "nsWebBrowserFind.h"
#include "nsWebBrowserPersist.h"
#include "nsCommandManager.h"
#include "nsControllerCommandTable.h"
#include "nsCommandParams.h"
#include "nsCommandGroup.h"
#include "nsBaseCommandController.h"
#include "nsNetCID.h"
#include "nsEmbedCID.h"

#ifdef NS_PRINTING
#include "nsPrintingPromptService.h"
#endif


NS_GENERIC_FACTORY_CONSTRUCTOR_INIT(nsWindowWatcher, Init)
NS_GENERIC_FACTORY_CONSTRUCTOR(nsAppStartupNotifier)
NS_GENERIC_FACTORY_CONSTRUCTOR(nsFind)
NS_GENERIC_FACTORY_CONSTRUCTOR(nsWebBrowserFind)
NS_GENERIC_FACTORY_CONSTRUCTOR(nsWebBrowserPersist)
NS_GENERIC_FACTORY_CONSTRUCTOR(nsControllerCommandTable)
NS_GENERIC_FACTORY_CONSTRUCTOR(nsCommandManager)
NS_GENERIC_FACTORY_CONSTRUCTOR_INIT(nsCommandParams, Init)
NS_GENERIC_FACTORY_CONSTRUCTOR(nsControllerCommandGroup)
NS_GENERIC_FACTORY_CONSTRUCTOR(nsBaseCommandController)

#ifdef MOZ_XUL
NS_GENERIC_FACTORY_CONSTRUCTOR(nsDialogParamBlock)
#ifdef NS_PRINTING
NS_GENERIC_FACTORY_CONSTRUCTOR_INIT(nsPrintingPromptService, Init)
#endif
#endif

#ifdef MOZ_XUL
NS_DEFINE_NAMED_CID(NS_DIALOGPARAMBLOCK_CID);
#ifdef NS_PRINTING
NS_DEFINE_NAMED_CID(NS_PRINTINGPROMPTSERVICE_CID);
#endif
#endif
NS_DEFINE_NAMED_CID(NS_WINDOWWATCHER_CID);
NS_DEFINE_NAMED_CID(NS_FIND_CID);
NS_DEFINE_NAMED_CID(NS_WEB_BROWSER_FIND_CID);
NS_DEFINE_NAMED_CID(NS_APPSTARTUPNOTIFIER_CID);
NS_DEFINE_NAMED_CID(NS_WEBBROWSERPERSIST_CID);
NS_DEFINE_NAMED_CID(NS_CONTROLLERCOMMANDTABLE_CID);
NS_DEFINE_NAMED_CID(NS_COMMAND_MANAGER_CID);
NS_DEFINE_NAMED_CID(NS_COMMAND_PARAMS_CID);
NS_DEFINE_NAMED_CID(NS_CONTROLLER_COMMAND_GROUP_CID);
NS_DEFINE_NAMED_CID(NS_BASECOMMANDCONTROLLER_CID);

static const mozilla::Module::CIDEntry kEmbeddingCIDs[] = {
#ifdef MOZ_XUL
    { &kNS_DIALOGPARAMBLOCK_CID, false, nullptr, nsDialogParamBlockConstructor },
#ifdef NS_PRINTING
    { &kNS_PRINTINGPROMPTSERVICE_CID, false, nullptr, nsPrintingPromptServiceConstructor },
#endif
#endif
    { &kNS_WINDOWWATCHER_CID, false, nullptr, nsWindowWatcherConstructor },
    { &kNS_FIND_CID, false, nullptr, nsFindConstructor },
    { &kNS_WEB_BROWSER_FIND_CID, false, nullptr, nsWebBrowserFindConstructor },
    { &kNS_APPSTARTUPNOTIFIER_CID, false, nullptr, nsAppStartupNotifierConstructor },
    { &kNS_WEBBROWSERPERSIST_CID, false, nullptr, nsWebBrowserPersistConstructor },
    { &kNS_CONTROLLERCOMMANDTABLE_CID, false, nullptr, nsControllerCommandTableConstructor },
    { &kNS_COMMAND_MANAGER_CID, false, nullptr, nsCommandManagerConstructor },
    { &kNS_COMMAND_PARAMS_CID, false, nullptr, nsCommandParamsConstructor },
    { &kNS_CONTROLLER_COMMAND_GROUP_CID, false, nullptr, nsControllerCommandGroupConstructor },
    { &kNS_BASECOMMANDCONTROLLER_CID, false, nullptr, nsBaseCommandControllerConstructor },
    { nullptr }
};

static const mozilla::Module::ContractIDEntry kEmbeddingContracts[] = {
#ifdef MOZ_XUL
    { NS_DIALOGPARAMBLOCK_CONTRACTID, &kNS_DIALOGPARAMBLOCK_CID },
#ifdef NS_PRINTING
    { NS_PRINTINGPROMPTSERVICE_CONTRACTID, &kNS_PRINTINGPROMPTSERVICE_CID },
#endif
#endif
    { NS_WINDOWWATCHER_CONTRACTID, &kNS_WINDOWWATCHER_CID },
    { NS_FIND_CONTRACTID, &kNS_FIND_CID },
    { NS_WEB_BROWSER_FIND_CONTRACTID, &kNS_WEB_BROWSER_FIND_CID },
    { NS_APPSTARTUPNOTIFIER_CONTRACTID, &kNS_APPSTARTUPNOTIFIER_CID },
    { NS_WEBBROWSERPERSIST_CONTRACTID, &kNS_WEBBROWSERPERSIST_CID },
    { NS_CONTROLLERCOMMANDTABLE_CONTRACTID, &kNS_CONTROLLERCOMMANDTABLE_CID },
    { NS_COMMAND_MANAGER_CONTRACTID, &kNS_COMMAND_MANAGER_CID },
    { NS_COMMAND_PARAMS_CONTRACTID, &kNS_COMMAND_PARAMS_CID },
    { NS_CONTROLLER_COMMAND_GROUP_CONTRACTID, &kNS_CONTROLLER_COMMAND_GROUP_CID },
    { NS_BASECOMMANDCONTROLLER_CONTRACTID, &kNS_BASECOMMANDCONTROLLER_CID },
    { nullptr }
};

static const mozilla::Module kEmbeddingModule = {
    mozilla::Module::kVersion,
    kEmbeddingCIDs,
    kEmbeddingContracts
};

NSMODULE_DEFN(embedcomponents) = &kEmbeddingModule;
