/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
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
 * Portions created by the Initial Developer are Copyright (C) 1998
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s): Radha Kulkarni (radha@netscape.com)
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

#include "nsIModule.h"
#include "nsIGenericFactory.h"
#include "nsIComponentManager.h"
#include "nsIServiceManager.h"
#include "nsIComponentRegistrar.h"

#include "nsQABrowserCID.h"
#include "nsQABrowserView.h"
#include "nsQABrowserUIGlue.h"
#include "nsQABrowserChrome.h"

// Factory Constructors

NS_GENERIC_FACTORY_CONSTRUCTOR(nsQABrowserView)
NS_GENERIC_FACTORY_CONSTRUCTOR(nsQABrowserUIGlue)
NS_GENERIC_FACTORY_CONSTRUCTOR(nsQABrowserChrome)

// Component Table



static NS_DEFINE_CID(kBrowserViewCID, NS_QABROWSERVIEW_CID);
static NS_DEFINE_CID(kBrowserUIGlueCID, NS_QABROWSERUIGLUE_CID);
static NS_DEFINE_CID(kBrowserChromeCID, NS_QABROWSERCHROME_CID);
static NS_DEFINE_CID(kComponentManagerCID, NS_COMPONENTMANAGER_CID);

static const nsModuleComponentInfo gQAEmbeddingModuleViewInfo[] =
{
   { "QA_BrowserView Component", NS_QABROWSERVIEW_CID, 
     NS_QABROWSERVIEW_CONTRACTID, nsQABrowserViewConstructor }
};

static const nsModuleComponentInfo gQAEmbeddingModuleUIInfo[] =
{
   { "QA_BrowserUIGlue Component", NS_QABROWSERUIGLUE_CID,
     NS_QABROWSERUIGLUE_CONTRACTID, nsQABrowserUIGlueConstructor }
};

static const nsModuleComponentInfo gQAEmbeddingModuleChromeInfo[] =
{
   { "QA_BrowserChrome Component", NS_QABROWSERCHROME_CID,
     NS_QABROWSERCHROME_CONTRACTID, nsQABrowserChromeConstructor }
};

nsresult
RegisterComponents()
{

  nsresult rv=NS_OK;
  nsIGenericFactory* fact;

  // Register the factory for all supporting interfaces. 
  nsCOMPtr<nsIComponentManager> compMgr = do_GetService(kComponentManagerCID, &rv);
  if (NS_FAILED(rv)) return rv;
  nsCOMPtr<nsIComponentRegistrar> registrar(do_QueryInterface(compMgr));

  // Register nsQABrowserView
  rv = NS_NewGenericFactory(&fact, gQAEmbeddingModuleViewInfo);
  rv = registrar->RegisterFactory(kBrowserViewCID,
                                  "QA Embedding BrowserView",
                                  NS_QABROWSERVIEW_CONTRACTID,
                                  fact);
  NS_RELEASE(fact);

  //Register nsQABrowserUIGlue
  rv = NS_NewGenericFactory(&fact, gQAEmbeddingModuleUIInfo);

  rv = registrar->RegisterFactory(kBrowserUIGlueCID,
                                  "QA Embedding BrowserUIGlue",
                                  NS_QABROWSERUIGLUE_CONTRACTID,
                                  fact);
  NS_RELEASE(fact);

  //Register nsQABrowserChrome
  rv = NS_NewGenericFactory(&fact, gQAEmbeddingModuleChromeInfo);

  rv = registrar->RegisterFactory(kBrowserChromeCID,
                                  "QA Embedding BrowserChrome",
                                  NS_QABROWSERCHROME_CONTRACTID,
                                  fact);
  NS_RELEASE(fact);
  return rv;

}




#if 0
nsresult
NSRegisterSelf(nsISupports* aServMgr, const char* aPath)
{
  nsresult rv;
  nsCOMPtr<nsIComponentManagerObsolete> compMgr =
           do_GetService(kComponentManagerCID, aServMgr, &rv);
  if (NS_FAILED(rv)) return rv;

  rv = compMgr->RegisterComponent(kBrowserViewCID,
                                  "QA Embedding BrowserView",
                                  NS_QABROWSERVIEW_CONTRACTID,
                                  aPath, PR_TRUE, PR_TRUE);
  rv = compMgr->RegisterComponent(kBrowserUIGlueCID,
                                  "QA Embedding BrowserUIGlue",
                                  NS_QABROWSERUIGLUE_CONTRACTID,
                                  aPath, PR_TRUE, PR_TRUE);

  return rv;

}

nsresult
NSUnregisterSelf(nsISupports* aServMgr, const char* aPath)
{
  nsresult rv;
  nsCOMPtr<nsIComponentManagerObsolete> compMgr =
         do_GetService(kComponentManagerCID, aServMgr, &rv);
  if (NS_FAILED(rv)) return rv;

  //rv = compMgr->UnregisterComponent(kRDFDataSourceCID, aPath);

  rv = compMgr->UnregisterComponent(kBrowserViewCID, aPath);
  rv = compMgr->UnregisterComponent(kBrowserUIGlueCID, aPath);
                            
  return rv;
}
#endif  /* 0 */



// NSGetModule implementation.


#if 0

NS_IMPL_NSGETMODULE(Mozilla_Embedding_Component, gQAEmbeddingModuleInfo)

nsresult
NSGetFactory(nsISupports* aServMgr,
             const nsCID &aClass,
             const char* aClassName,
             const char* aContractID,
             nsIFactory **aFactory)
{
  nsresult rv=NS_OK;
  nsIGenericFactory* fact;
 // if (aClass.Equals(kBrowserViewCID))
    rv = NS_NewGenericFactory(&fact, gQAEmbeddingModuleInfo);
 // else if (aClass.Equals(kBrowserUIGlueCID))
 //   rv = NS_NewGenericFactory(&fact, NS_NewQABrowserUIGlueFactory);
 //   rv = NS_ERROR_FAILURE;

  if (NS_SUCCEEDED(rv))
    *aFactory = fact;

#ifdef DEBUG_radha
  printf("nsQABrowserComponent NSGetFactory!\n");
#endif
  return rv;
}

#endif  