/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
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
#include "nsIServiceManager.h"
#include "nsIGenericFactory.h"
#include "nsICategoryManager.h"
#include "nsXPIDLString.h"

#include "nsWebBrowser.h"
#include "nsWebBrowserPersist.h"
#include "nsCommandHandler.h"
#include "nsWebBrowserContentPolicy.h"

// Factory Constructors

NS_GENERIC_FACTORY_CONSTRUCTOR(nsWebBrowser)
NS_GENERIC_FACTORY_CONSTRUCTOR(nsWebBrowserContentPolicy)
NS_GENERIC_FACTORY_CONSTRUCTOR(nsCommandHandler)
NS_GENERIC_FACTORY_CONSTRUCTOR(nsWebBrowserPersist)

static NS_METHOD
RegisterContentPolicy(nsIComponentManager *aCompMgr, nsIFile *aPath,
                      const char *registryLocation, const char *componentType,
                      const nsModuleComponentInfo *info)
{
    nsresult rv;
    nsCOMPtr<nsICategoryManager> catman =
        do_GetService(NS_CATEGORYMANAGER_CONTRACTID, &rv);
    if (NS_FAILED(rv)) return rv;
    nsXPIDLCString previous;
    return catman->AddCategoryEntry("content-policy",
                                    NS_WEBBROWSERCONTENTPOLICY_CONTRACTID,
                                    NS_WEBBROWSERCONTENTPOLICY_CONTRACTID,
                                    PR_TRUE, PR_TRUE, getter_Copies(previous));
}

static NS_METHOD
UnregisterContentPolicy(nsIComponentManager *aCompMgr, nsIFile *aPath,
                        const char *registryLocation,
                        const nsModuleComponentInfo *info)
{
    nsresult rv;
    nsCOMPtr<nsICategoryManager> catman =
        do_GetService(NS_CATEGORYMANAGER_CONTRACTID, &rv);
    if (NS_FAILED(rv)) return rv;

    return catman->DeleteCategoryEntry("content-policy",
                                       NS_WEBBROWSERCONTENTPOLICY_CONTRACTID,
                                       PR_TRUE);
}

// Component Table

static nsModuleComponentInfo components[] =
{
   { "WebBrowserPersist Component", NS_WEBBROWSERPERSIST_CID, 
     NS_WEBBROWSERPERSIST_CONTRACTID, nsWebBrowserPersistConstructor },
   { "WebBrowser Component", NS_WEBBROWSER_CID, 
     NS_WEBBROWSER_CONTRACTID, nsWebBrowserConstructor },
   { "CommandHandler Component", NS_COMMANDHANDLER_CID,
     NS_COMMANDHANDLER_CONTRACTID, nsCommandHandlerConstructor },
   { "nsIWebBrowserSetup content policy enforcer", 
     NS_WEBBROWSERCONTENTPOLICY_CID,
     NS_WEBBROWSERCONTENTPOLICY_CONTRACTID,
     nsWebBrowserContentPolicyConstructor,
     RegisterContentPolicy, UnregisterContentPolicy }
};


// NSGetModule implementation.

NS_IMPL_NSGETMODULE(Browser_Embedding_Module, components)



