/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
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
 * The Original Code is Mozilla Communicator client code.
 *
 * The Initial Developer of the Original Code is Netscape Communications
 * Corporation.  Portions created by Netscape are
 * Copyright (C) 1998 Netscape Communications Corporation. All
 * Rights Reserved.
 */

#define NS_IMPL_IDS
#include "nsICharsetAlias.h"
#undef NS_IMPL_IDS
#include "nsCOMPtr.h"
#include "nsIModule.h"

#include "nspr.h"
#include "nsString.h"
#include "pratom.h"
#include "nsUniversalCharDetDll.h"
#include "nsISupports.h"
#include "nsIRegistry.h"
#include "nsIComponentManager.h"
#include "nsIFactory.h"
#include "nsIServiceManager.h"
#include "nsICharsetDetector.h"
#include "nsIStringCharsetDetector.h"
#include "nsIGenericFactory.h"

#include "nsUniversalDetector.h"

static NS_DEFINE_CID(kUniversalDetectorCID,  NS_UNIVERSAL_DETECTOR_CID);
static NS_DEFINE_CID(kUniversalStringDetectorCID,  NS_UNIVERSAL_STRING_DETECTOR_CID);

NS_GENERIC_FACTORY_CONSTRUCTOR(nsUniversalXPCOMDetector);
NS_GENERIC_FACTORY_CONSTRUCTOR(nsUniversalXPCOMStringDetector);

//----------------------------------------
static NS_METHOD nsUniversalCharDetectorRegistrationProc(nsIComponentManager *aCompMgr,
                                          nsIFile *aPath,
                                          const char *registryLocation,
                                          const char *componentType,
                                          const nsModuleComponentInfo *info)
{
  nsRegistryKey key;
  nsresult rv = NS_OK;
  nsCOMPtr<nsIRegistry> registry = do_GetService(NS_REGISTRY_CONTRACTID, &rv);
  if (NS_FAILED(rv)) {
    return rv;
  }

  // open the registry
  rv = registry->OpenWellKnownRegistry(
    nsIRegistry::ApplicationComponentRegistry);
  if (NS_FAILED(rv)) {
    return rv;
  }

  rv = registry -> AddSubtree(nsIRegistry::Common, 
                              NS_CHARSET_DETECTOR_REG_BASE "universal_charset_detector" ,&key);
  if (NS_SUCCEEDED(rv)) {
    rv = registry-> SetStringUTF8(key, "type", "universal_charset_detector");
    rv = registry-> SetStringUTF8(key, "defaultEnglishText", "UniversalDetector");
  }

  return rv;
}

// Component Table
static nsModuleComponentInfo components[] = 
{
   { "Universal Charset Detector", NS_UNIVERSAL_DETECTOR_CID, 
    NS_CHARSET_DETECTOR_CONTRACTID_BASE "universal_charset_detector", nsUniversalXPCOMDetectorConstructor, 
    nsUniversalCharDetectorRegistrationProc, NULL},
   { "Universal String Charset Detector", NS_UNIVERSAL_STRING_DETECTOR_CID, 
    NS_STRCDETECTOR_CONTRACTID_BASE "universal_charset_detector", nsUniversalXPCOMStringDetectorConstructor, 
    NULL, NULL}
};

NS_IMPL_NSGETMODULE(nsUniversalCharDetModule, components)
