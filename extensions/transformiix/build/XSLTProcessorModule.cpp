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
#include "nsIAppShellComponent.h"
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
  nsresult result = NS_OK;
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

#define TRANSFORMIIX_CONTRACTID   \
NS_IAPPSHELLCOMPONENT_CONTRACTID "/Transformiix;1"

class TransformiixComponent : public nsIAppShellComponent {
public:
  TransformiixComponent();
  virtual ~TransformiixComponent();

  NS_DEFINE_STATIC_CID_ACCESSOR(TRANSFORMIIX_CID);

  // nsISupports
  NS_DECL_ISUPPORTS

  // nsIAppShellComponent
  NS_DECL_NSIAPPSHELLCOMPONENT

  static TransformiixComponent *GetInstance();
  static TransformiixComponent* mInstance;
};

TransformiixComponent* TransformiixComponent::mInstance = nsnull;

TransformiixComponent *
TransformiixComponent::GetInstance()
{
  if (mInstance == nsnull) {
    mInstance = new TransformiixComponent();
    // Will be released in the module destructor
    NS_IF_ADDREF(mInstance);
  }

  NS_IF_ADDREF(mInstance);
  return mInstance;
}

TransformiixComponent::TransformiixComponent()
{
  NS_INIT_ISUPPORTS();
}

TransformiixComponent::~TransformiixComponent()
{
}

NS_IMPL_ISUPPORTS1(TransformiixComponent, nsIAppShellComponent)

NS_IMETHODIMP
TransformiixComponent::Initialize(nsIAppShellService *anAppShell, 
                         nsICmdLineService  *aCmdLineService) 
{
  nsresult rv;
  nsCOMPtr<nsIScriptNameSetRegistry> namesetService = 
    do_GetService(kCScriptNameSetRegistryCID, &rv);
  
  if (NS_SUCCEEDED(rv)) {
    TransformiixNameset* nameset = new TransformiixNameset();
    // the NameSet service will AddRef this one
    namesetService->AddExternalNameSet(nameset);
  }
  
  return rv;

}

NS_IMETHODIMP
TransformiixComponent::Shutdown()
{
  return NS_OK;
}

NS_GENERIC_FACTORY_SINGLETON_CONSTRUCTOR(TransformiixComponent, TransformiixComponent::GetInstance);

static NS_METHOD 
RegisterTransformiix(nsIComponentManager *aCompMgr,
                     nsIFile *aPath,
                     const char *registryLocation,
                     const char *componentType)
{
  // get the registry
  nsIRegistry* registry;
  nsresult rv = nsServiceManager::GetService(NS_REGISTRY_CONTRACTID,
                                             NS_GET_IID(nsIRegistry),
                                             (nsISupports**)&registry);
  if (NS_SUCCEEDED(rv)) {
    registry->OpenWellKnownRegistry(nsIRegistry::ApplicationComponentRegistry);
    char buffer[256];
    char *cid = TransformiixComponent::GetCID().ToString();
    PR_snprintf(buffer,
                sizeof buffer,
                "%s/%s",
                NS_IAPPSHELLCOMPONENT_KEY,
                cid ? cid : "unknown" );
    nsCRT::free(cid);

    nsRegistryKey key;
    rv = registry->AddSubtree(nsIRegistry::Common,
                              buffer,
                              &key );
    nsServiceManager::ReleaseService(NS_REGISTRY_CONTRACTID, registry);
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

static void PR_CALLBACK
TransformiixModuleDtor(nsIModule* self)
{
  NS_IF_RELEASE(TransformiixComponent::mInstance);
}

NS_IMPL_NSGETMODULE_WITH_DTOR("TransformiixModule", components, TransformiixModuleDtor)
