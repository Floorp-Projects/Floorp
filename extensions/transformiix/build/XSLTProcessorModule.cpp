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
#include "nsICategoryManager.h"
#include "nsIServiceManager.h"
#include "nsIScriptNameSpaceManager.h"
#include "nsSyncLoader.h"
#include "nsXPIDLString.h"
#include "txAtoms.h"
#include "XSLTProcessor.h"
#include "XPathProcessor.h"

// Factory Constructor
NS_GENERIC_FACTORY_CONSTRUCTOR(XSLTProcessor)
NS_GENERIC_FACTORY_CONSTRUCTOR(XPathProcessor)
NS_GENERIC_FACTORY_CONSTRUCTOR(nsSyncLoader)

static NS_METHOD 
RegisterTransformiix(nsIComponentManager *aCompMgr,
                     nsIFile *aPath,
                     const char *registryLocation,
                     const char *componentType,
                     const nsModuleComponentInfo *info)
{
  nsresult rv = NS_OK;

  nsCOMPtr<nsICategoryManager> catman =
    do_GetService(NS_CATEGORYMANAGER_CONTRACTID, &rv);

  if (NS_FAILED(rv))
    return rv;

  nsXPIDLCString previous;
  rv = catman->AddCategoryEntry(JAVASCRIPT_GLOBAL_CONSTRUCTOR_CATEGORY,
                                "XSLTProcessor",
                                TRANSFORMIIX_XSLT_PROCESSOR_CONTRACTID,
                                PR_TRUE, PR_TRUE, getter_Copies(previous));
  NS_ENSURE_SUCCESS(rv, rv);

  rv = catman->AddCategoryEntry(JAVASCRIPT_GLOBAL_CONSTRUCTOR_CATEGORY,
                                "XPathProcessor",
                                TRANSFORMIIX_XPATH_PROCESSOR_CONTRACTID,
                                PR_TRUE, PR_TRUE, getter_Copies(previous));

  return rv;
}

static PRBool gInitialized = PR_FALSE;

// Perform our one-time intialization for this module
PR_STATIC_CALLBACK(nsresult)
Initialize(nsIModule* aSelf)
{
  NS_PRECONDITION(!gInitialized, "module already initialized");
  if (gInitialized)
    return NS_OK;

  gInitialized = PR_TRUE;

  if (!txXMLAtoms::init())
    return NS_ERROR_OUT_OF_MEMORY;
  if (!txXPathAtoms::init())
    return NS_ERROR_OUT_OF_MEMORY;
  if (!txXSLTAtoms::init())
    return NS_ERROR_OUT_OF_MEMORY;
  if (!txHTMLAtoms::init())
    return NS_ERROR_OUT_OF_MEMORY;
  return NS_OK;
}

// Shutdown this module, releasing all of the module resources
PR_STATIC_CALLBACK(void)
Shutdown(nsIModule* aSelf)
{
  NS_PRECONDITION(gInitialized, "module not initialized");
  if (! gInitialized)
    return;

  gInitialized = PR_FALSE;
  txXMLAtoms::shutdown();
  txXPathAtoms::shutdown();
  txXSLTAtoms::shutdown();
  txHTMLAtoms::shutdown();
}

// Component Table
static nsModuleComponentInfo gComponents[] = {
    { "Transformiix XSLT Processor",
      TRANSFORMIIX_XSLT_PROCESSOR_CID,
      TRANSFORMIIX_XSLT_PROCESSOR_CONTRACTID,
      XSLTProcessorConstructor,
      RegisterTransformiix },
    { "Transformiix XPath Processor",
      TRANSFORMIIX_XPATH_PROCESSOR_CID,
      TRANSFORMIIX_XPATH_PROCESSOR_CONTRACTID,
      XPathProcessorConstructor },
    { "Transformiix Synchronous Loader",
      TRANSFORMIIX_SYNCLOADER_CID,
      TRANSFORMIIX_SYNCLOADER_CONTRACTID,
      nsSyncLoaderConstructor }
};

NS_IMPL_NSGETMODULE_WITH_CTOR_DTOR(TransformiixModule, gComponents,
                                   Initialize, Shutdown)
