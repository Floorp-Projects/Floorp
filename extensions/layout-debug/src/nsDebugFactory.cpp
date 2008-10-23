/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
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
 * Portions created by the Initial Developer are Copyright (C) 1998
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

#include "nscore.h"
#include "nsLayoutDebugCIID.h"
#include "nsIFactory.h"
#include "nsISupports.h"
#include "nsRegressionTester.h"
#include "nsLayoutDebuggingTools.h"
#include "nsLayoutDebugCLH.h"
#include "nsIGenericFactory.h"
#include "nsICategoryManager.h"
#include "nsIServiceManager.h"

NS_GENERIC_FACTORY_CONSTRUCTOR(nsRegressionTester)
NS_GENERIC_FACTORY_CONSTRUCTOR(nsLayoutDebuggingTools)
NS_GENERIC_FACTORY_CONSTRUCTOR(nsLayoutDebugCLH)

static NS_IMETHODIMP
RegisterCommandLineHandlers(nsIComponentManager* compMgr, nsIFile* path,
                            const char *location, const char *type,
                            const nsModuleComponentInfo *info)
{
  nsresult rv;
  nsCOMPtr<nsICategoryManager> catMan (do_GetService(NS_CATEGORYMANAGER_CONTRACTID));
  NS_ENSURE_TRUE(catMan, NS_ERROR_FAILURE);

  rv = catMan->AddCategoryEntry("command-line-handler", "m-layoutdebug",
                                "@mozilla.org/commandlinehandler/general-startup;1?type=layoutdebug",
                                PR_TRUE, PR_TRUE, nsnull);
  if (NS_FAILED(rv))
    return NS_ERROR_FAILURE;

  return NS_OK;
}

static NS_IMETHODIMP
UnregisterCommandLineHandlers(nsIComponentManager* compMgr, nsIFile *path,
                              const char *location,
                              const nsModuleComponentInfo *info)
{
  nsCOMPtr<nsICategoryManager> catMan (do_GetService(NS_CATEGORYMANAGER_CONTRACTID));
  NS_ENSURE_TRUE(catMan, NS_ERROR_FAILURE);

  catMan->DeleteCategoryEntry("command-line-handler", "m-layoutdebug",
                              PR_TRUE);

  return NS_OK;
}

static const nsModuleComponentInfo components[] =
{
  { "nsRegressionTester",
    NS_REGRESSION_TESTER_CID,
    "@mozilla.org/layout-debug/regressiontester;1",
    nsRegressionTesterConstructor 
  },
  { "nsLayoutDebuggingTools",
    NS_LAYOUT_DEBUGGINGTOOLS_CID,
    NS_LAYOUT_DEBUGGINGTOOLS_CONTRACTID,
    nsLayoutDebuggingToolsConstructor
  },
  { "LayoutDebug Startup Handler",
    NS_LAYOUTDEBUGCLH_CID,
    "@mozilla.org/commandlinehandler/general-startup;1?type=layoutdebug",
    nsLayoutDebugCLHConstructor,
    RegisterCommandLineHandlers,
    UnregisterCommandLineHandlers
  }
};

NS_IMPL_NSGETMODULE(nsLayoutDebugModule, components)
