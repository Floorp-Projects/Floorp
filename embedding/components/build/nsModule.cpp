/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* ***** BEGIN LICENSE BLOCK *****
 * Version: NPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Netscape Public License
 * Version 1.1 (the "License"); you may not use this file except in
 * compliance with the License. You may obtain a copy of the License at
 * http://www.mozilla.org/NPL/
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
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the NPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the NPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

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
