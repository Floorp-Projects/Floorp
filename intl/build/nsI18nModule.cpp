/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
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
 * Contributor(s):
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

#include "nsIGenericFactory.h"
#include "nsCOMPtr.h"
#include "nsICategoryManager.h"
#include "nsIServiceManager.h"

// chardet
#include "nsMetaCharsetCID.h"
#include "nsICharsetDetector.h"
#include "nsICharsetAlias.h"
#include "nsMetaCharsetObserver.h"
#include "nsDocumentCharsetInfo.h"
#include "nsXMLEncodingObserver.h"
#include "nsICharsetDetectionAdaptor.h"
#include "nsICharsetDetectionObserver.h"
#include "nsDetectionAdaptor.h"
#include "nsIStringCharsetDetector.h"
#include "nsPSMDetectors.h"
#include "nsCyrillicDetector.h"
#include "nsDocumentCharsetInfoCID.h"
#include "nsXMLEncodingCID.h"
#include "nsCharsetDetectionAdaptorCID.h"

// chardet
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



static NS_METHOD
AddCategoryEntry(const char* category,
                 const char* key,
                 const char* value)
{
  nsresult rv;
  nsCOMPtr<nsICategoryManager> 
    categoryManager(do_GetService("@mozilla.org/categorymanager;1", &rv));
  if (NS_FAILED(rv)) return rv;
  
  return categoryManager->AddCategoryEntry(category, key, value, 
                                           PR_TRUE, PR_TRUE,
                                           nsnull);
}

static NS_METHOD
DeleteCategoryEntry(const char* category,
                    const char* key)
{
  nsresult rv;
  nsCOMPtr<nsICategoryManager> 
    categoryManager(do_GetService("@mozilla.org/categorymanager;1", &rv));
  if (NS_FAILED(rv)) return rv;
  
  return categoryManager->DeleteCategoryEntry(category, key, PR_TRUE);
}

static NS_METHOD
nsMetaCharsetObserverRegistrationProc(nsIComponentManager *aCompMgr,
                                      nsIFile *aPath,
                                      const char *registryLocation,
                                      const char *componentType,
                                      const nsModuleComponentInfo *info)
{
  return AddCategoryEntry("parser-service-category", 
                          "Meta Charset Service",
                          NS_META_CHARSET_CONTRACTID);
}

static NS_METHOD
nsMetaCharsetObserverUnegistrationProc(nsIComponentManager *aCompMgr,
                                       nsIFile *aPath,
                                       const char *registryLocation,
                                       const nsModuleComponentInfo *info)
{
  return DeleteCategoryEntry("parser-service-category",
                             "Meta Charset Service");
}

static NS_METHOD
nsDetectionAdaptorRegistrationProc(nsIComponentManager *aCompMgr,
                                   nsIFile *aPath,
                                   const char *registryLocation,
                                   const char *componentType,
                                   const nsModuleComponentInfo *info)
{
  return AddCategoryEntry(NS_CHARSET_DETECTOR_CATEGORY, "off", "off");
}

static NS_METHOD
nsJAPSMDetectorRegistrationProc(nsIComponentManager *aCompMgr,
                                nsIFile *aPath,
                                const char *registryLocation,
                                const char *componentType,
                                const nsModuleComponentInfo *info)
{
  return AddCategoryEntry(NS_CHARSET_DETECTOR_CATEGORY,
                          "ja_parallel_state_machine",
                          info->mContractID);
}

static NS_METHOD
nsKOPSMDetectorRegistrationProc(nsIComponentManager *aCompMgr,
                                nsIFile *aPath,
                                const char *registryLocation,
                                const char *componentType,
                                const nsModuleComponentInfo *info)
{
  return AddCategoryEntry(NS_CHARSET_DETECTOR_CATEGORY,
                          "ko_parallel_state_machine",
                          info->mContractID);
}

static NS_METHOD
nsZHTWPSMDetectorRegistrationProc(nsIComponentManager *aCompMgr,
                                  nsIFile *aPath,
                                  const char *registryLocation,
                                  const char *componentType,
                                  const nsModuleComponentInfo *info)
{
  return AddCategoryEntry(NS_CHARSET_DETECTOR_CATEGORY,
                          "zhtw_parallel_state_machine",
                          info->mContractID);
}

static NS_METHOD
nsZHCNPSMDetectorRegistrationProc(nsIComponentManager *aCompMgr,
                                  nsIFile *aPath,
                                  const char *registryLocation,
                                  const char *componentType,
                                  const nsModuleComponentInfo *info)
{
  return AddCategoryEntry(NS_CHARSET_DETECTOR_CATEGORY,
                          "zhcn_parallel_state_machine",
                          info->mContractID);
}

static NS_METHOD
nsZHPSMDetectorRegistrationProc(nsIComponentManager *aCompMgr,
                                nsIFile *aPath,
                                const char *registryLocation,
                                const char *componentType,
                                const nsModuleComponentInfo *info)
{
  return AddCategoryEntry(NS_CHARSET_DETECTOR_CATEGORY,
                          "zh_parallel_state_machine",
                          info->mContractID);
}

static NS_METHOD
nsCJKPSMDetectorRegistrationProc(nsIComponentManager *aCompMgr,
                                 nsIFile *aPath,
                                 const char *registryLocation,
                                 const char *componentType,
                                 const nsModuleComponentInfo *info)
{
  return AddCategoryEntry(NS_CHARSET_DETECTOR_CATEGORY,
                          "cjk_parallel_state_machine",
                          info->mContractID);
}

static NS_METHOD
nsRUProbDetectorRegistrationProc(nsIComponentManager *aCompMgr,
                                 nsIFile *aPath,
                                 const char *registryLocation,
                                 const char *componentType,
                                 const nsModuleComponentInfo *info)
{
  return AddCategoryEntry(NS_CHARSET_DETECTOR_CATEGORY,
                          "ruprob",
                          info->mContractID);
}

static NS_METHOD
nsUKProbDetectorRegistrationProc(nsIComponentManager *aCompMgr,
                                 nsIFile *aPath,
                                 const char *registryLocation,
                                 const char *componentType,
                                 const nsModuleComponentInfo *info)
{
  return AddCategoryEntry(NS_CHARSET_DETECTOR_CATEGORY,
                          "ukprob",
                          info->mContractID);
}


static nsModuleComponentInfo components[] =
{
    // chardet
 { "Meta Charset", NS_META_CHARSET_CID, 
    NS_META_CHARSET_CONTRACTID, nsMetaCharsetObserverConstructor, 
    nsMetaCharsetObserverRegistrationProc, nsMetaCharsetObserverUnegistrationProc,
    NULL},
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
    NULL, NULL},
#endif /* INCLUDE_DBGDETECTOR */
    
};


NS_IMPL_NSGETMODULE(nsI18nModule, components);
