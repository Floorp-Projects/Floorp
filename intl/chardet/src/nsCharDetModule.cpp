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
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is Netscape
 * Communications Corporation.  Portions created by Netscape are
 * Copyright (C) 1998 Netscape Communications Corporation. All
 * Rights Reserved.
 *
 * Contributor(s): 
 *   Pierre Phaneuf <pp@ludusdesign.com>
 */
#define NS_IMPL_IDS
#include "nsICharsetAlias.h"
#undef NS_IMPL_IDS
#include "nsCOMPtr.h"
#include "nsIModule.h"

#include "nspr.h"
#include "nsString.h"
#include "pratom.h"
#include "nsCharDetDll.h"
#include "nsISupports.h"
#include "nsIRegistry.h"
#include "nsIComponentManager.h"
#include "nsIFactory.h"
#include "nsIServiceManager.h"
#include "nsICharsetDetector.h"
#include "nsIWebShellServices.h"
#include "nsIStringCharsetDetector.h"
#include "nsIGenericFactory.h"
#include "nsIAppStartupNotifier.h"
#include "nsICategoryManager.h"
#include "nsMetaCharsetObserver.h"
#include "nsXMLEncodingObserver.h"
#include "nsDetectionAdaptor.h"
#include "nsDocumentCharsetInfo.h"
#include "nsCyrillicDetector.h"
#include "nsPSMDetectors.h"
#include "nsMetaCharsetCID.h"
#include "nsXMLEncodingCID.h"
#include "nsCharsetDetectionAdaptorCID.h"
#include "nsDocumentCharsetInfoCID.h"

//#define INCLUDE_DBGDETECTOR
#ifdef INCLUDE_DBGDETECTOR
// for debuging only
#include "nsDebugDetector.h"
NS_DEFINE_CID(k1stBlkDbgDetectorCID,  NS_1STBLKDBG_DETECTOR_CID);
NS_DEFINE_CID(k2ndBlkDbgDetectorCID,  NS_2NDBLKDBG_DETECTOR_CID);
NS_DEFINE_CID(klastBlkDbgDetectorCID, NS_LASTBLKDBG_DETECTOR_CID);
#endif /* INCLUDE_DBGDETECTOR  */

NS_DEFINE_CID(kMetaCharsetCID, NS_META_CHARSET_CID);
NS_DEFINE_CID(kXMLEncodingCID, NS_XML_ENCODING_CID);
NS_DEFINE_CID(kCharsetDetectionAdaptorCID, NS_CHARSET_DETECTION_ADAPTOR_CID);
NS_DEFINE_CID(kDocumentCharsetInfoCID, NS_DOCUMENTCHARSETINFO_CID);
NS_DEFINE_CID(kJAPSMDetectorCID,  NS_JA_PSMDETECTOR_CID);
NS_DEFINE_CID(kJAStringPSMDetectorCID,  NS_JA_STRING_PSMDETECTOR_CID);
NS_DEFINE_CID(kKOPSMDetectorCID,  NS_KO_PSMDETECTOR_CID);
NS_DEFINE_CID(kKOStringPSMDetectorCID,  NS_KO_STRING_PSMDETECTOR_CID);
NS_DEFINE_CID(kZHCNPSMDetectorCID,  NS_ZHCN_PSMDETECTOR_CID);
NS_DEFINE_CID(kZHCNStringPSMDetectorCID,  NS_ZHCN_STRING_PSMDETECTOR_CID);
NS_DEFINE_CID(kZHTWPSMDetectorCID,  NS_ZHTW_PSMDETECTOR_CID);
NS_DEFINE_CID(kZHTWStringPSMDetectorCID,  NS_ZHTW_STRING_PSMDETECTOR_CID);
NS_DEFINE_CID(kZHPSMDetectorCID,  NS_ZH_PSMDETECTOR_CID);
NS_DEFINE_CID(kZHStringPSMDetectorCID,  NS_ZH_STRING_PSMDETECTOR_CID);
NS_DEFINE_CID(kCJKPSMDetectorCID,  NS_CJK_PSMDETECTOR_CID);
NS_DEFINE_CID(kCJKStringPSMDetectorCID,  NS_CJK_STRING_PSMDETECTOR_CID);
NS_DEFINE_CID(kRUProbDetectorCID, NS_RU_PROBDETECTOR_CID);
NS_DEFINE_CID(kUKProbDetectorCID, NS_UK_PROBDETECTOR_CID);
NS_DEFINE_CID(kRUStringProbDetectorCID, NS_RU_STRING_PROBDETECTOR_CID);
NS_DEFINE_CID(kUKStringProbDetectorCID, NS_UK_STRING_PROBDETECTOR_CID);

NS_GENERIC_FACTORY_CONSTRUCTOR(nsMetaCharsetObserver);
NS_GENERIC_FACTORY_CONSTRUCTOR(nsDocumentCharsetInfo);
NS_GENERIC_FACTORY_CONSTRUCTOR(nsXMLEncodingObserver);
NS_GENERIC_FACTORY_CONSTRUCTOR(nsDetectionAdaptor);
NS_GENERIC_FACTORY_CONSTRUCTOR(nsJAPSMDetector);
NS_GENERIC_FACTORY_CONSTRUCTOR(nsJAStringPSMDetector);
NS_GENERIC_FACTORY_CONSTRUCTOR(nsKOPSMDetector);
NS_GENERIC_FACTORY_CONSTRUCTOR(nsKOStringPSMDetector);
NS_GENERIC_FACTORY_CONSTRUCTOR(nsZHTWPSMDetector);
NS_GENERIC_FACTORY_CONSTRUCTOR(nsZHTWStringPSMDetector);
NS_GENERIC_FACTORY_CONSTRUCTOR(nsZHCNPSMDetector);
NS_GENERIC_FACTORY_CONSTRUCTOR(nsZHCNStringPSMDetector);
NS_GENERIC_FACTORY_CONSTRUCTOR(nsZHPSMDetector);
NS_GENERIC_FACTORY_CONSTRUCTOR(nsZHStringPSMDetector);
NS_GENERIC_FACTORY_CONSTRUCTOR(nsCJKPSMDetector);
NS_GENERIC_FACTORY_CONSTRUCTOR(nsCJKStringPSMDetector);
NS_GENERIC_FACTORY_CONSTRUCTOR(nsRUProbDetector);
NS_GENERIC_FACTORY_CONSTRUCTOR(nsUKProbDetector);
NS_GENERIC_FACTORY_CONSTRUCTOR(nsRUStringProbDetector);
NS_GENERIC_FACTORY_CONSTRUCTOR(nsUKStringProbDetector);

#ifdef INCLUDE_DBGDETECTOR
NS_GENERIC_FACTORY_CONSTRUCTOR(ns1stBlkDbgDetector);
NS_GENERIC_FACTORY_CONSTRUCTOR(ns2ndBlkDbgDetector);
NS_GENERIC_FACTORY_CONSTRUCTOR(nsLastBlkDbgDetector);
#endif /* INCLUDE_DBGDETECTOR */

static NS_METHOD nsMetaCharsetObserverRegistrationProc(nsIComponentManager *aCompMgr,
                                          nsIFile *aPath,
                                          const char *registryLocation,
                                          const char *componentType,
                                          const nsModuleComponentInfo *info)
{
  nsresult rv;
  nsCOMPtr<nsICategoryManager> 
    categoryManager(do_GetService("@mozilla.org/categorymanager;1", &rv));
  if (NS_SUCCEEDED(rv)) {
    rv = categoryManager->AddCategoryEntry(APPSTARTUP_CATEGORY, "Meta Charset",
                        "service," NS_META_CHARSET_CONTRACTID,
                        PR_TRUE, PR_TRUE,
                        nsnull);
  }
  return rv;
}

static NS_METHOD nsDetectionAdaptorRegistrationProc(nsIComponentManager *aCompMgr,
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
                              NS_CHARSET_DETECTOR_REG_BASE "off" ,&key);
  if (NS_SUCCEEDED(rv)) {
    rv = registry-> SetStringUTF8(key, "type", "off");
    rv = registry-> SetStringUTF8(key, "defaultEnglishText", "Off");
  }

  return rv;
}

static NS_METHOD nsJAPSMDetectorRegistrationProc(nsIComponentManager *aCompMgr,
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
                              NS_CHARSET_DETECTOR_REG_BASE "ja_parallel_state_machine" ,&key);
  if (NS_SUCCEEDED(rv)) {
    rv = registry-> SetStringUTF8(key, "type", "ja_parallel_state_machine");
    rv = registry-> SetStringUTF8(key, "defaultEnglishText", "Japanese");
  }
  return rv;
}

static NS_METHOD nsKOPSMDetectorRegistrationProc(nsIComponentManager *aCompMgr,
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
                              NS_CHARSET_DETECTOR_REG_BASE "ko_parallel_state_machine" ,&key);
  if (NS_SUCCEEDED(rv)) {
    rv = registry-> SetStringUTF8(key, "type", "ko_parallel_state_machine");
    rv = registry-> SetStringUTF8(key, "defaultEnglishText", "Korean");
  }
  return rv;
}

static NS_METHOD nsZHTWPSMDetectorRegistrationProc(nsIComponentManager *aCompMgr,
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
                            NS_CHARSET_DETECTOR_REG_BASE "zhtw_parallel_state_machine" ,&key);
   if (NS_SUCCEEDED(rv)) {
     rv = registry-> SetStringUTF8(key, "type", "zhtw_parallel_state_machine");
     rv = registry-> SetStringUTF8(key, "defaultEnglishText", "Traditional Chinese");
   }
  return rv;
}

static NS_METHOD nsZHCNPSMDetectorRegistrationProc(nsIComponentManager *aCompMgr,
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
                            NS_CHARSET_DETECTOR_REG_BASE "zhcn_parallel_state_machine" ,&key);
   if (NS_SUCCEEDED(rv)) {
     rv = registry-> SetStringUTF8(key, "type", "zhtw_parallel_state_machine");
     rv = registry-> SetStringUTF8(key, "defaultEnglishText", "Simplified Chinese");
   }
  return rv;
}

static NS_METHOD nsZHPSMDetectorRegistrationProc(nsIComponentManager *aCompMgr,
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
                            NS_CHARSET_DETECTOR_REG_BASE "zh_parallel_state_machine" ,&key);
   if (NS_SUCCEEDED(rv)) {
     rv = registry-> SetStringUTF8(key, "type", "zh_parallel_state_machine");
     rv = registry-> SetStringUTF8(key, "defaultEnglishText", "Chinese");
   }
  return rv;
}

static NS_METHOD nsCJKPSMDetectorRegistrationProc(nsIComponentManager *aCompMgr,
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
                            NS_CHARSET_DETECTOR_REG_BASE "cjk_parallel_state_machine" ,&key);
   if (NS_SUCCEEDED(rv)) {
     rv = registry-> SetStringUTF8(key, "type", "cjk_parallel_state_machine");
     rv = registry-> SetStringUTF8(key, "defaultEnglishText", "East Asian");
   }
  return rv;
}

static NS_METHOD nsRUProbDetectorRegistrationProc(nsIComponentManager *aCompMgr,
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
                           NS_CHARSET_DETECTOR_REG_BASE "ruprob" ,&key);
  if (NS_SUCCEEDED(rv)) {
    rv = registry-> SetStringUTF8(key, "type", "ruprob");
    rv = registry-> SetStringUTF8(key, "defaultEnglishText", "Russian");
  }
  return rv;
}

static NS_METHOD nsUKProbDetectorRegistrationProc(nsIComponentManager *aCompMgr,
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
                           NS_CHARSET_DETECTOR_REG_BASE "ukprob" ,&key);
  if (NS_SUCCEEDED(rv)) {
    rv = registry-> SetStringUTF8(key, "type", "ukprob");
    rv = registry-> SetStringUTF8(key, "defaultEnglishText", "Ukrainian");
  }
  return rv;
}

// Component Table
static nsModuleComponentInfo components[] = 
{
 { "Meta Charset", NS_META_CHARSET_CID, 
    NS_META_CHARSET_CONTRACTID, nsMetaCharsetObserverConstructor, 
    nsMetaCharsetObserverRegistrationProc, NULL},
 { "Document Charset Info", NS_DOCUMENTCHARSETINFO_CID, 
    NS_DOCUMENTCHARSETINFO_CONTRACTID, nsDocumentCharsetInfoConstructor, 
    NULL, NULL},
 { "XML Encoding", NS_XML_ENCODING_CID, 
    NS_XML_ENCODING_CONTRACTID, nsXMLEncodingObserverConstructor, 
    NULL, NULL},
 { "Charset Detection Adaptor", NS_CHARSET_DETECTION_ADAPTOR_CID, 
    NS_CHARSET_DETECTION_ADAPTOR_CONTRACTID, nsDetectionAdaptorConstructor, 
    nsDetectionAdaptorRegistrationProc, NULL},
 { "PSM based Japanese Charset Detector", NS_JA_PSMDETECTOR_CID, 
    NS_CHARSET_DETECTOR_CONTRACTID_BASE "ja_parallel_state_machine", nsJAPSMDetectorConstructor, 
    nsJAPSMDetectorRegistrationProc, NULL},
 { "PSM based Japanese String Charset Detector", NS_JA_STRING_PSMDETECTOR_CID, 
    NS_STRCDETECTOR_CONTRACTID_BASE "ja_parallel_state_machine", nsJAStringPSMDetectorConstructor, 
    NULL, NULL},
 { "PSM based Korean Charset Detector", NS_KO_PSMDETECTOR_CID, 
    NS_CHARSET_DETECTOR_CONTRACTID_BASE "ko_parallel_state_machine", nsKOPSMDetectorConstructor, 
    nsKOPSMDetectorRegistrationProc, NULL},
 { "PSM based Korean String Charset Detector", NS_KO_STRING_PSMDETECTOR_CID, 
    NS_STRCDETECTOR_CONTRACTID_BASE "ko_parallel_state_machine", nsKOStringPSMDetectorConstructor, 
    NULL, NULL},
 { "PSM based Traditional Chinese Charset Detector", NS_ZHTW_PSMDETECTOR_CID, 
    NS_CHARSET_DETECTOR_CONTRACTID_BASE "zhtw_parallel_state_machine", nsZHTWPSMDetectorConstructor, 
    nsZHTWPSMDetectorRegistrationProc, NULL},
 { "PSM based Traditional Chinese String Charset Detector", NS_ZHTW_STRING_PSMDETECTOR_CID, 
    NS_STRCDETECTOR_CONTRACTID_BASE "zhtw_parallel_state_machine", nsZHTWStringPSMDetectorConstructor, 
    NULL, NULL},
 { "PSM based Simplified Chinese Charset Detector", NS_ZHCN_PSMDETECTOR_CID, 
    NS_CHARSET_DETECTOR_CONTRACTID_BASE "zhcn_parallel_state_machine", nsZHCNPSMDetectorConstructor, 
    nsZHCNPSMDetectorRegistrationProc, NULL},
 { "PSM based Simplified Chinese String Charset Detector", NS_ZHCN_STRING_PSMDETECTOR_CID, 
    NS_STRCDETECTOR_CONTRACTID_BASE "zhcn_parallel_state_machine", nsZHCNStringPSMDetectorConstructor, 
    NULL, NULL},
 { "PSM based Chinese Charset Detector", NS_ZH_PSMDETECTOR_CID, 
    NS_CHARSET_DETECTOR_CONTRACTID_BASE "zh_parallel_state_machine", nsZHPSMDetectorConstructor, 
    nsZHPSMDetectorRegistrationProc, NULL},
 { "PSM based Chinese String Charset Detector", NS_ZH_STRING_PSMDETECTOR_CID, 
    NS_STRCDETECTOR_CONTRACTID_BASE "zh_parallel_state_machine", nsZHStringPSMDetectorConstructor, 
    NULL, NULL},
 { "PSM based CJK Charset Detector", NS_CJK_PSMDETECTOR_CID, 
    NS_CHARSET_DETECTOR_CONTRACTID_BASE "cjk_parallel_state_machine", nsCJKPSMDetectorConstructor, 
    nsCJKPSMDetectorRegistrationProc, NULL},
 { "PSM based CJK String Charset Detector", NS_CJK_STRING_PSMDETECTOR_CID, 
    NS_STRCDETECTOR_CONTRACTID_BASE "cjk_parallel_state_machine", nsCJKStringPSMDetectorConstructor, 
    NULL, NULL},
 { "Probability based Russian Charset Detector", NS_RU_PROBDETECTOR_CID, 
    NS_CHARSET_DETECTOR_CONTRACTID_BASE "ruprob", nsRUProbDetectorConstructor, 
    nsRUProbDetectorRegistrationProc, NULL},
 { "Probability based Ukrainian Charset Detector", NS_UK_PROBDETECTOR_CID, 
    NS_CHARSET_DETECTOR_CONTRACTID_BASE "ukprob", nsUKProbDetectorConstructor, 
    nsUKProbDetectorRegistrationProc, NULL},
 { "Probability based Russian String Charset Detector", NS_RU_STRING_PROBDETECTOR_CID, 
    NS_STRCDETECTOR_CONTRACTID_BASE "ruprob", nsRUStringProbDetectorConstructor, 
    NULL, NULL},
 { "Probability based Ukrainian String Charset Detector", NS_UK_STRING_PROBDETECTOR_CID, 
    NS_STRCDETECTOR_CONTRACTID_BASE "ukprob", nsUKStringProbDetectorConstructor, 
    NULL, NULL}
#ifdef INCLUDE_DBGDETECTOR
 ,
 { "Debuging Detector 1st block", NS_1STBLKDBG_DETECTOR_CID, 
    NS_CHARSET_DETECTOR_CONTRACTID_BASE "1stblkdbg", ns1stBlkDbgDetectorConstructor, 
    NULL, NULL},
 { "Debuging Detector 2nd block", NS_2NDBLKDBG_DETECTOR_CID, 
    NS_CHARSET_DETECTOR_CONTRACTID_BASE "2ndblkdbg", ns2ndBlkDbgDetectorConstructor, 
    NULL, NULL},
 { "Debuging Detector Last block", NS_LASTBLKDBG_DETECTOR_CID, 
    NS_CHARSET_DETECTOR_CONTRACTID_BASE "lastblkdbg", nsLastBlkDbgDetectorConstructor, 
    NULL, NULL}
#endif /* INCLUDE_DBGDETECTOR */
};

NS_IMPL_NSGETMODULE(nsCharDetModule, components)

