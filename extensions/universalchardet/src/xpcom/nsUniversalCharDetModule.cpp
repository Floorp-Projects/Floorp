/* -*- Mode: C; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/*
 * CONFIDENTIAL AND PROPRIETARY SOURCE CODE
 * OF NETSCAPE COMMUNICATIONS CORPORATION
 *
 * Copyright (c) 2000 Netscape Communications Corporation.
 * All Rights Reserved.
 *
 * Use of this Source Code is subject to the terms of the applicable
 * license agreement from Netscape Communications Corporation.
 *
 * The copyright notice(s) in this Source Code does not indicate actual
 * or intended publication of this Source Code.
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
                              NS_CHARSET_DETECTOR_REG_BASE "Universal_charset_detector" ,&key);
  if (NS_SUCCEEDED(rv)) {
    rv = registry-> SetStringUTF8(key, "type", "Universal_charset_detector");
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
    NS_STRCDETECTOR_CONTRACTID_BASE "universal_string_charset_detector", nsUniversalXPCOMStringDetectorConstructor, 
    NULL, NULL}
};

NS_IMPL_NSGETMODULE(nsUniversalCharDetModule, components)
