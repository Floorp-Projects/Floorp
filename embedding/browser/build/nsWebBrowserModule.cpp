/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * The contents of this file are subject to the Mozilla Public
 * License Version 1.1 (the "License"); you may not use this file
 * except in compliance with the License. You may obtain a copy of
 * the License at http://www.mozilla.org/MPL/
 * 
 * Software distributed under the License is distributed on an "AS
 * IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
 * implied. See the License for the specific language governing
 * rights and limitations under the License.
 * 
 * The Original Code is the Mozilla browser.
 * 
 * The Initial Developer of the Original Code is Netscape
 * Communications, Inc.  Portions created by Netscape are
 * Copyright (C) 1999, Mozilla.  All Rights Reserved.
 * 
 * Contributor(s):
 *   Travis Bogard <travis@netscape.com>
 */

#include "nsIModule.h"
#include "nsIGenericFactory.h"

#include "nsWebBrowser.h"
#include "nsCommandHandler.h"


// Factory Constructors

NS_GENERIC_FACTORY_CONSTRUCTOR(nsWebBrowser)
//NS_GENERIC_FACTORY_CONSTRUCTOR(nsWebBrowserSetup)
NS_GENERIC_FACTORY_CONSTRUCTOR(nsCommandHandler)


// Component Table

static nsModuleComponentInfo components[] =
{
   { "WebBrowser Component", NS_WEBBROWSER_CID, 
      NS_WEBBROWSER_CONTRACTID, nsWebBrowserConstructor },
   { "CommandHandler Component", NS_COMMANDHANDLER_CID,
      NS_COMMANDHANDLER_CONTRACTID, nsCommandHandlerConstructor }
//   { "WebBrowserSetup Component", NS_WEBBROWSER_SETUP_CID, 
//      NS_WEBBROWSER_SETUP_CONTRACTID, nsWebBrowserSetupConstructor }
};


// NSGetModule implementation.

NS_IMPL_NSGETMODULE("Browser Embedding Module", components)



#ifdef XP_WIN32
  //in addition to returning a version number for this module,
  //this also provides a convenient hook for the preloader
  //to keep (some if not all) of the module resident.
extern "C" __declspec(dllexport) float GetVersionNumber(void) {
  return 1.0;
}
#endif

