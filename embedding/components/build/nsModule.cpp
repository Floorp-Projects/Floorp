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

#include "nsIGenericFactory.h"
#include "nsDialogParamBlock.h"
#include "nsPromptService.h"
#include "nsWindowWatcher.h"
#include "nsAppStartupNotifier.h"
#include "nsJSConsoleService.h"
#include "nsFind.h"
#include "nsWebBrowserFind.h"
#include "nsWebBrowserPersist.h"
#include "nsCommandManager.h"
#include "nsControllerCommandTable.h"
#include "nsCommandParams.h"
#include "nsCommandGroup.h"
#include "nsPrintingPromptService.h"
#include "nsBaseCommandController.h"

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
NS_GENERIC_FACTORY_CONSTRUCTOR_INIT(nsPromptService, Init)
NS_GENERIC_FACTORY_CONSTRUCTOR(nsJSConsoleService)
NS_GENERIC_FACTORY_CONSTRUCTOR_INIT(nsPrintingPromptService, Init)
#endif

#ifdef MOZ_PROFILESHARING
#include "nsProfileSharingSetup.h"
NS_GENERIC_FACTORY_CONSTRUCTOR(nsProfileSharingSetup)
#endif

static const nsModuleComponentInfo gComponents[] = {

#ifdef MOZ_XUL
  { "Dialog ParamBlock", NS_DIALOGPARAMBLOCK_CID, NS_DIALOGPARAMBLOCK_CONTRACTID, nsDialogParamBlockConstructor },
  { "Prompt Service", NS_PROMPTSERVICE_CID, NS_PROMPTSERVICE_CONTRACTID, nsPromptServiceConstructor },
  { "JS Console Service", NS_JSCONSOLESERVICE_CID, NS_JSCONSOLESERVICE_CONTRACTID, nsJSConsoleServiceConstructor },
  { "Printing Prompt Service", NS_PRINTINGPROMPTSERVICE_CID, NS_PRINTINGPROMPTSERVICE_CONTRACTID, nsPrintingPromptServiceConstructor },
#endif
  { "Window Watcher", NS_WINDOWWATCHER_CID, NS_WINDOWWATCHER_CONTRACTID, nsWindowWatcherConstructor },
  { "Find",           NS_FIND_CID, NS_FIND_CONTRACTID, nsFindConstructor },
  { "WebBrowserFind",           NS_WEB_BROWSER_FIND_CID, NS_WEB_BROWSER_FIND_CONTRACTID, nsWebBrowserFindConstructor },
  { NS_APPSTARTUPNOTIFIER_CLASSNAME, NS_APPSTARTUPNOTIFIER_CID, NS_APPSTARTUPNOTIFIER_CONTRACTID, nsAppStartupNotifierConstructor },
  { "WebBrowserPersist Component", NS_WEBBROWSERPERSIST_CID, NS_WEBBROWSERPERSIST_CONTRACTID, nsWebBrowserPersistConstructor },
  { "Controller Command Table", NS_CONTROLLERCOMMANDTABLE_CID, NS_CONTROLLERCOMMANDTABLE_CONTRACTID, nsControllerCommandTableConstructor },
  { "Command Manager", NS_COMMAND_MANAGER_CID, NS_COMMAND_MANAGER_CONTRACTID, nsCommandManagerConstructor },
  { "Command Params", NS_COMMAND_PARAMS_CID, NS_COMMAND_PARAMS_CONTRACTID, nsCommandParamsConstructor },
  { "Command Group", NS_CONTROLLER_COMMAND_GROUP_CID, NS_CONTROLLER_COMMAND_GROUP_CONTRACTID, nsControllerCommandGroupConstructor },
  { "Base Command Controller", NS_BASECOMMANDCONTROLLER_CID, NS_BASECOMMANDCONTROLLER_CONTRACTID, nsBaseCommandControllerConstructor }
#ifdef MOZ_PROFILESHARING
  ,{ "Profile Sharing Setup", NS_PROFILESHARINGSETUP_CID, NS_PROFILESHARINGSETUP_CONTRACTID, nsProfileSharingSetupConstructor }
#endif
};

NS_IMPL_NSGETMODULE(embedcomponents, gComponents)
