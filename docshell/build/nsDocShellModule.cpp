/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
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
#include "nsWebShell.h"

NS_GENERIC_FACTORY_CONSTRUCTOR(nsWebShell);

// Currently no-one is instanciating docshell's directly because
// nsWebShell is still our main "shell" class. nsWebShell is a subclass
// of nsDocShell. Once migration is complete, docshells will be the main
// "shell" class and this module will need to register the docshell as
// a component
//NS_GENERIC_FACTORY_CONSTRUCTOR(nsDocShell);

static nsModuleComponentInfo gDocShellModuleInfo[] = {
    { "WebShell", 
      NS_WEB_SHELL_CID,
      "@mozilla.org/webshell;1",
      nsWebShellConstructor }
};

// "docshell provider" to illustrate that this thing really *should*
// be dispensing docshells rather than webshells.
NS_IMPL_NSGETMODULE("docshell provider", gDocShellModuleInfo)

