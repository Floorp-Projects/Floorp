/*
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
 * The Original Code is TransforMiiX XSLT processor.
 * 
 * The Initial Developer of the Original Code is The MITRE Corporation.
 * Portions created by MITRE are Copyright (C) 1999 The MITRE Corporation.
 *
 * Portions created by Peter Van der Beken are Copyright (C) 2000
 * Peter Van der Beken. All Rights Reserved.
 *
 * Contributor(s):
 * Peter Van der Beken, peter.vanderbeken@pandora.be
 *    -- original author.
 *
 */

#include "nsIGenericFactory.h"
#include "nsIAppStartupNotifier.h"
#include "nsICategoryManager.h"
#include "nsIObserver.h"
#include "nsIScriptExternalNameSet.h"
#include "nsIScriptNameSetRegistry.h"
#include "nsIScriptNameSpaceManager.h"
#include "nsIScriptContext.h"
#include "nsIRegistry.h"
#include "nsDOMCID.h"
#include "prprf.h"

#include "XSLTProcessor.h"
#include "XPathProcessor.h"
#include "nsSyncLoader.h"

static NS_DEFINE_CID(kCScriptNameSetRegistryCID, NS_SCRIPT_NAMESET_REGISTRY_CID);

// Factory Constructor
NS_GENERIC_FACTORY_CONSTRUCTOR(XSLTProcessor)
NS_GENERIC_FACTORY_CONSTRUCTOR(XPathProcessor)
NS_GENERIC_FACTORY_CONSTRUCTOR(nsSyncLoader)

class TransformiixNameset : public nsIScriptExternalNameSet {
public:
  TransformiixNameset();
  virtual ~TransformiixNameset();

  // nsISupports
  NS_DECL_ISUPPORTS

  // nsIScriptExternalNameSet
  NS_IMETHOD InitializeClasses(nsIScriptContext* aScriptContext);
  NS_IMETHOD AddNameSet(nsIScriptContext* aScriptContext);
};

TransformiixNameset::TransformiixNameset()
{
  NS_INIT_ISUPPORTS();
}

TransformiixNameset::~TransformiixNameset()
{
}

NS_IMPL_ISUPPORTS1(TransformiixNameset, nsIScriptExternalNameSet)

NS_IMETHODIMP
TransformiixNameset::InitializeClasses(nsIScriptContext* aScriptContext)
{
  return NS_OK;
}

NS_IMETHODIMP
TransformiixNameset::AddNameSet(nsIScriptContext* aScriptContext)
{
  static NS_DEFINE_CID(kXSLTProcessor_CID, TRANSFORMIIX_XSLT_PROCESSOR_CID);
  static NS_DEFINE_CID(kXPathProcessor_CID, TRANSFORMIIX_XPATH_PROCESSOR_CID);
  nsresult result;
  nsCOMPtr<nsIScriptNameSpaceManager> manager;
  
  result = aScriptContext->GetNameSpaceManager(getter_AddRefs(manager));
  if (NS_SUCCEEDED(result)) {
    result = manager->RegisterGlobalName(NS_ConvertASCIItoUCS2("XSLTProcessor"),
                                         NS_GET_IID(nsIDocumentTransformer),
                                         kXSLTProcessor_CID,
                                         PR_TRUE);
    NS_ENSURE_SUCCESS(result, result);

    result = manager->RegisterGlobalName(NS_ConvertASCIItoUCS2("XPathProcessor"),
                                         NS_GET_IID(nsIXPathNodeSelector),
                                         kXPathProcessor_CID,
                                         PR_TRUE);
    NS_ENSURE_SUCCESS(result, result);
  }

  return result;
}

/* 878f99ae-1dd2-11b2-add2-edf7a72f957b */
#define TRANSFORMIIX_CID   \
{ 0x878f99ae, 0x1dd2, 0x11b2, {0xad, 0xd2, 0xed, 0xf7, 0xa7, 0x2f, 0x95, 0x7b} }

#define TRANSFORMIIX_CONTRACTID "@mozilla.org/Transformiix;1"

class TransformiixComponent : public nsIObserver {
public:
  TransformiixComponent();
  virtual ~TransformiixComponent();

  NS_DEFINE_STATIC_CID_ACCESSOR(TRANSFORMIIX_CID);

  // nsISupports
  NS_DECL_ISUPPORTS

  // nsIObserver
  NS_DECL_NSIOBSERVER
};

TransformiixComponent::TransformiixComponent()
{
  NS_INIT_ISUPPORTS();
}

TransformiixComponent::~TransformiixComponent()
{
}

NS_IMPL_ISUPPORTS1(TransformiixComponent, nsIObserver)

NS_IMETHODIMP
TransformiixComponent::Observe(nsISupports *aSubject,
                               const PRUnichar *aTopic,
                               const PRUnichar *aData) 
{
  nsresult rv;
  nsCOMPtr<nsIScriptNameSetRegistry>
    namesetService(do_GetService(kCScriptNameSetRegistryCID, &rv));
  
  if (NS_SUCCEEDED(rv)) {
    TransformiixNameset* nameset = new TransformiixNameset();
    if (!nameset)
      return NS_ERROR_OUT_OF_MEMORY;
    // the NameSet service will AddRef this one
    rv = namesetService->AddExternalNameSet(nameset);
  }
  
  return rv;
}

NS_GENERIC_FACTORY_CONSTRUCTOR(TransformiixComponent);

static NS_METHOD 
RegisterTransformiix(nsIComponentManager *aCompMgr,
                     nsIFile *aPath,
                     const char *registryLocation,
                     const char *componentType,
                     const nsModuleComponentInfo *info)
{
  nsresult rv;
  nsCOMPtr<nsICategoryManager> 
    categoryManager(do_GetService("@mozilla.org/categorymanager;1", &rv));
  if (NS_SUCCEEDED(rv)) {
    rv = categoryManager->AddCategoryEntry(APPSTARTUP_CATEGORY, "Transformiix Module",
                        "service," TRANSFORMIIX_CONTRACTID,
                        PR_TRUE, PR_TRUE,
                        nsnull);
  }
  return rv;
}

// Component Table
static nsModuleComponentInfo components[] = {
    { "Transformiix component",
      TRANSFORMIIX_CID,
      TRANSFORMIIX_CONTRACTID,
      TransformiixComponentConstructor,
      RegisterTransformiix },
    { "Transformiix XSLT Processor",
      TRANSFORMIIX_XSLT_PROCESSOR_CID,
      TRANSFORMIIX_XSLT_PROCESSOR_CONTRACTID,
      XSLTProcessorConstructor },
    { "Transformiix XPath Processor",
      TRANSFORMIIX_XPATH_PROCESSOR_CID,
      TRANSFORMIIX_XPATH_PROCESSOR_CONTRACTID,
      XPathProcessorConstructor },
    { "Transformiix Synchronous Loader",
      TRANSFORMIIX_SYNCLOADER_CID,
      TRANSFORMIIX_SYNCLOADER_CONTRACTID,
      nsSyncLoaderConstructor }
};

NS_IMPL_NSGETMODULE(TransformiixModule, components)
