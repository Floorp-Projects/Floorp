/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 *
 * The contents of this file are subject to the Netscape Public
 * License Version 1.1 (the "License"); you may not use this file
 * except in compliance with the License. You may obtain a copy of
 * the License at http://www.mozilla.org/NPL/
 *
 * Software distributed under the License is distributed on an "AS
 * IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
 * implied. See the License for the specific language governing
 * rights and limitations under the License.
 *
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is Netscape
 * Communications Corporation.  Portions created by Netscape are
 * Copyright (C) 2001 Netscape Communications Corporation. All
 * Rights Reserved.
 *
 * Contributor(s): 
 */

#include "nsIGenericFactory.h"
#include "nsDialogParamBlock.h"
#include "nsPromptService.h"
#include "nsWindowWatcher.h"
#include "nsAppStartupNotifier.h"
#include "nsJSConsoleService.h"
#include "nsWebBrowserFind.h"

NS_GENERIC_FACTORY_CONSTRUCTOR(nsDialogParamBlock)
NS_GENERIC_FACTORY_CONSTRUCTOR_INIT(nsPromptService, Init)
NS_GENERIC_FACTORY_CONSTRUCTOR_INIT(nsWindowWatcher, Init)
NS_GENERIC_FACTORY_CONSTRUCTOR(nsAppStartupNotifier)
NS_GENERIC_FACTORY_CONSTRUCTOR(nsJSConsoleService)
NS_GENERIC_FACTORY_CONSTRUCTOR(nsWebBrowserFind)

static nsModuleComponentInfo gComponents[] = {

  { "Dialog ParamBlock", NS_DIALOGPARAMBLOCK_CID, NS_DIALOGPARAMBLOCK_CONTRACTID, nsDialogParamBlockConstructor },
  { "Prompt Service", NS_PROMPTSERVICE_CID, NS_PROMPTSERVICE_CONTRACTID, nsPromptServiceConstructor },
  { "JS Console Service", NS_JSCONSOLESERVICE_CID, NS_JSCONSOLESERVICE_CONTRACTID, nsJSConsoleServiceConstructor },
  { "Window Watcher", NS_WINDOWWATCHER_CID, NS_WINDOWWATCHER_CONTRACTID, nsWindowWatcherConstructor },
  { "Find",           NS_WEB_BROWSER_FIND_CID, NS_WEB_BROWSER_FIND_CONTRACTID, nsWebBrowserFindConstructor },
  { NS_APPSTARTUPNOTIFIER_CLASSNAME, NS_APPSTARTUPNOTIFIER_CID, NS_APPSTARTUPNOTIFIER_CONTRACTID, nsAppStartupNotifierConstructor }
};

NS_IMPL_NSGETMODULE(embedcomponents, gComponents)
