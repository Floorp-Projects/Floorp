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

NS_GENERIC_FACTORY_CONSTRUCTOR(nsDialogParamBlock)
NS_GENERIC_FACTORY_CONSTRUCTOR_INIT(nsPromptService, Init)
NS_GENERIC_FACTORY_CONSTRUCTOR_INIT(nsWindowWatcher, Init)
NS_GENERIC_FACTORY_CONSTRUCTOR(nsAppStartupNotifier)

static nsModuleComponentInfo components[] = {

  //{ "Dialog ParamBlock", NS_DIALOGPARAMBLOCK_CID, NS_DIALOGPARAMBLOCK_CONTRACTID, nsDialogParamBlockConstructor },
  { "Prompt Service", NS_PROMPTSERVICE_CID, NS_PROMPTSERVICE_CONTRACTID, nsPromptServiceConstructor },
  { "Window Watcher", NS_WINDOWWATCHER_CID, NS_WINDOWWATCHER_CONTRACTID, nsWindowWatcherConstructor },
  { NS_APPSTARTUPNOTIFIER_CLASSNAME, NS_APPSTARTUPNOTIFIER_CID, NS_APPSTARTUPNOTIFIER_CONTRACTID, nsAppStartupNotifierConstructor }
};

NS_IMPL_NSGETMODULE("embedcomponents", components)

#ifdef XP_WIN32
  //in addition to returning a version number for this module,
  //this also provides a convenient hook for the preloader
  //to keep (some if not all) of the module resident.
extern "C" __declspec(dllexport) float GetVersionNumber(void) {
  return 1.0;
}
#endif

