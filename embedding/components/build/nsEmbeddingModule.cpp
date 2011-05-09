/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* ***** BEGIN LICENSE BLOCK *****
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
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 2001
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
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
    { &kNS_DIALOGPARAMBLOCK_CID, false, NULL, nsDialogParamBlockConstructor },
#ifdef NS_PRINTING
    { &kNS_PRINTINGPROMPTSERVICE_CID, false, NULL, nsPrintingPromptServiceConstructor },
#endif
#endif
    { &kNS_WINDOWWATCHER_CID, false, NULL, nsWindowWatcherConstructor },
    { &kNS_FIND_CID, false, NULL, nsFindConstructor },
    { &kNS_WEB_BROWSER_FIND_CID, false, NULL, nsWebBrowserFindConstructor },
    { &kNS_APPSTARTUPNOTIFIER_CID, false, NULL, nsAppStartupNotifierConstructor },
    { &kNS_WEBBROWSERPERSIST_CID, false, NULL, nsWebBrowserPersistConstructor },
    { &kNS_CONTROLLERCOMMANDTABLE_CID, false, NULL, nsControllerCommandTableConstructor },
    { &kNS_COMMAND_MANAGER_CID, false, NULL, nsCommandManagerConstructor },
    { &kNS_COMMAND_PARAMS_CID, false, NULL, nsCommandParamsConstructor },
    { &kNS_CONTROLLER_COMMAND_GROUP_CID, false, NULL, nsControllerCommandGroupConstructor },
    { &kNS_BASECOMMANDCONTROLLER_CID, false, NULL, nsBaseCommandControllerConstructor },
    { NULL }
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
    { NULL }
};

static const mozilla::Module kEmbeddingModule = {
    mozilla::Module::kVersion,
    kEmbeddingCIDs,
    kEmbeddingContracts
};

NSMODULE_DEFN(embedcomponents) = &kEmbeddingModule;
