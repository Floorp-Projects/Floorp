/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * The contents of this file are subject to the Netscape Public License
 * Version 1.0 (the "NPL"); you may not use this file except in
 * compliance with the NPL.  You may obtain a copy of the NPL at
 * http://www.mozilla.org/NPL/
 *
 * Software distributed under the NPL is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the NPL
 * for the specific language governing rights and limitations under the
 * NPL.
 *
 * The Initial Developer of this code under the NPL is Netscape
 * Communications Corporation.  Portions created by Netscape are
 * Copyright (C) 1998 Netscape Communications Corporation.  All Rights
 * Reserved.
 */

#define NS_IMPL_IDS
#include "nsICharsetAlias.h"
#undef NS_IMPL_IDS

#include "pratom.h"
#include "nsCharDetDll.h"
#include "nsISupports.h"
#include "nsIComponentManager.h"
#include "nsIFactory.h"
#include "nsIServiceManager.h"
#include "nsCOMPtr.h"
#include "nsMetaCharsetObserver.h"
#include "nsXMLEncodingObserver.h"
#include "nsDetectionAdaptor.h"
#include "nsICharsetDetector.h"
#include "nsIStringCharsetDetector.h"

#include "nsMetaCharsetCID.h"
#include "nsXMLEncodingCID.h"
#include "nsCharsetDetectionAdaptorCID.h"


#include "nsPSMDetectors.h"

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

#include "nsCyrillicDetector.h"
NS_DEFINE_CID(kRUProbDetectorCID,  NS_RU_PROBDETECTOR_CID);
NS_DEFINE_CID(kRUStringProbDetectorCID,  NS_RU_STRING_PROBDETECTOR_CID);
NS_DEFINE_CID(kUKProbDetectorCID,  NS_UK_PROBDETECTOR_CID);
NS_DEFINE_CID(kUKStringProbDetectorCID,  NS_UK_STRING_PROBDETECTOR_CID);


#define INCLUDE_DBGDETECTOR
#ifdef INCLUDE_DBGDETECTOR
// for debuging only
#include "nsDebugDetector.h"
NS_DEFINE_CID(k1stBlkDbgDetectorCID,  NS_1STBLKDBG_DETECTOR_CID);
NS_DEFINE_CID(k2ndBlkDbgDetectorCID,  NS_2NDBLKDBG_DETECTOR_CID);
NS_DEFINE_CID(klastBlkDbgDetectorCID, NS_LASTBLKDBG_DETECTOR_CID);
#endif /* INCLUDE_DBGDETECTOR  */

static NS_DEFINE_CID(kComponentManagerCID, NS_COMPONENTMANAGER_CID);

NS_DEFINE_IID(kFactoryIID, NS_IFACTORY_IID);
NS_DEFINE_CID(kMetaCharsetCID, NS_META_CHARSET_CID);
NS_DEFINE_CID(kXMLEncodingCID, NS_XML_ENCODING_CID);
NS_DEFINE_CID(kCharsetDetectionAdaptorCID, NS_CHARSET_DETECTION_ADAPTOR_CID);

PRInt32 g_InstanceCount = 0;
PRInt32 g_LockCount = 0;


extern "C" NS_EXPORT nsresult NSGetFactory(nsISupports* aServMgr,
                                           const nsCID &aClass,
                                           const char *aClassName,
                                           const char *aProgID,
                                           nsIFactory **aFactory)
{
  if (aFactory == NULL) {
    return NS_ERROR_NULL_POINTER;
  }

  nsIFactory *factory = nsnull;
  if (aClass.Equals(kMetaCharsetCID)) {
    factory = NEW_META_CHARSET_OBSERVER_FACTORY();
  } else if (aClass.Equals(kXMLEncodingCID)) {
    factory = NEW_XML_ENCODING_OBSERVER_FACTORY();
  } else if (aClass.Equals(kCharsetDetectionAdaptorCID)) {
    factory = NEW_DETECTION_ADAPTOR_FACTORY();
  } else if (aClass.Equals(kJAPSMDetectorCID) ||
             aClass.Equals(kJAStringPSMDetectorCID) ||
             aClass.Equals(kKOPSMDetectorCID) ||
             aClass.Equals(kKOStringPSMDetectorCID) ||
             aClass.Equals(kZHCNPSMDetectorCID) ||
             aClass.Equals(kZHCNStringPSMDetectorCID) ||
             aClass.Equals(kZHTWPSMDetectorCID) ||
             aClass.Equals(kZHTWStringPSMDetectorCID) ||
             aClass.Equals(kZHPSMDetectorCID) ||
             aClass.Equals(kZHStringPSMDetectorCID) ||
             aClass.Equals(kCJKPSMDetectorCID) ||
             aClass.Equals(kCJKStringPSMDetectorCID)
            ) 
  {
    factory = NEW_PSMDETECTOR_FACTORY(aClass);
  } else if (aClass.Equals(kRUProbDetectorCID) ||
             aClass.Equals(kRUStringProbDetectorCID) ||
             aClass.Equals(kUKProbDetectorCID) ||
             aClass.Equals(kUKStringProbDetectorCID) 
            )
  {
    factory = NEW_PROBDETECTOR_FACTORY(aClass);
#ifdef INCLUDE_DBGDETECTOR
  } else if (aClass.Equals(k1stBlkDbgDetectorCID)) {
    factory = NEW_1STBLKDBG_DETECTOR_FACTORY();
  } else if (aClass.Equals(k2ndBlkDbgDetectorCID)) {
    factory = NEW_2NDBLKDBG_DETECTOR_FACTORY();
  } else if (aClass.Equals(klastBlkDbgDetectorCID)) {
    factory = NEW_LASTBLKDBG_DETECTOR_FACTORY();
#endif /* INCLUDE_DBGDETECTOR */
  }

  if(nsnull != factory) {
    nsresult res = factory->QueryInterface(kFactoryIID, (void **) aFactory);
    if (NS_FAILED(res)) {
      *aFactory = NULL;
      delete factory;
    }
    return res;
  }
  return NS_NOINTERFACE;
}

extern "C" NS_EXPORT PRBool NSCanUnload(nsISupports* aServMgr) {
  return PRBool(g_InstanceCount == 0 && g_LockCount == 0);
}
extern "C" NS_EXPORT nsresult NSRegisterSelf(nsISupports* aServMgr, const char *path)
{
  nsresult rv;

  nsCOMPtr<nsIServiceManager> servMgr(do_QueryInterface(aServMgr, &rv));
  if (NS_FAILED(rv)) return rv;

  nsIComponentManager* compMgr;
  rv = servMgr->GetService(kComponentManagerCID, 
                           nsIComponentManager::GetIID(), 
                           (nsISupports**)&compMgr);
  if (NS_FAILED(rv)) return rv;

  rv = compMgr->RegisterComponent(kMetaCharsetCID, 
                                  "Meta Charset", 
                                  NS_META_CHARSET_PROGID, 
                                  path,
                                  PR_TRUE, PR_TRUE);
  rv = compMgr->RegisterComponent(kXMLEncodingCID, 
                                  "XML Encoding", 
                                  NS_XML_ENCODING_PROGID, 
                                  path,
                                  PR_TRUE, PR_TRUE);
  rv = compMgr->RegisterComponent(kCharsetDetectionAdaptorCID, 
                                  "Charset Detection Adaptor", 
                                  NS_CHARSET_DETECTION_ADAPTOR_PROGID, 
                                  path,
                                  PR_TRUE, PR_TRUE);
  rv = compMgr->RegisterComponent(kJAPSMDetectorCID, 
                                  "PSM based Japanese Charset Detector", 
                                  NS_CHARSET_DETECTOR_PROGID_BASE "japsm", 
                                  path,
                                  PR_TRUE, PR_TRUE);
  rv = compMgr->RegisterComponent(kJAStringPSMDetectorCID, 
                                  "PSM based Japanese String Charset Detector", 
                                  NS_STRCDETECTOR_PROGID_BASE "japsm", 
                                  path,
                                  PR_TRUE, PR_TRUE);
  rv = compMgr->RegisterComponent(kKOPSMDetectorCID, 
                                  "PSM based Korean Charset Detector", 
                                  NS_CHARSET_DETECTOR_PROGID_BASE "kopsm", 
                                  path,
                                  PR_TRUE, PR_TRUE);
  rv = compMgr->RegisterComponent(kKOStringPSMDetectorCID, 
                                  "PSM based Korean String Charset Detector", 
                                  NS_STRCDETECTOR_PROGID_BASE "kopsm", 
                                  path,
                                  PR_TRUE, PR_TRUE);
  rv = compMgr->RegisterComponent(kZHTWPSMDetectorCID, 
                                  "PSM based Traditional Chinese Charset Detector", 
                                  NS_CHARSET_DETECTOR_PROGID_BASE "zhtwpsm", 
                                  path,
                                  PR_TRUE, PR_TRUE);
  rv = compMgr->RegisterComponent(kZHTWStringPSMDetectorCID, 
                                  "PSM based Traditional Chinese String Charset Detector", 
                                  NS_STRCDETECTOR_PROGID_BASE "zhtwpsm", 
                                  path,
                                  PR_TRUE, PR_TRUE);
  rv = compMgr->RegisterComponent(kZHCNPSMDetectorCID, 
                                  "PSM based Simplified Chinese Charset Detector", 
                                  NS_CHARSET_DETECTOR_PROGID_BASE "zhcnpsm", 
                                  path,
                                  PR_TRUE, PR_TRUE);
  rv = compMgr->RegisterComponent(kZHCNStringPSMDetectorCID, 
                                  "PSM based Simplified Chinese String Charset Detector", 
                                  NS_STRCDETECTOR_PROGID_BASE "zhcnpsm", 
                                  path,
                                  PR_TRUE, PR_TRUE);
  rv = compMgr->RegisterComponent(kZHPSMDetectorCID, 
                                  "PSM based Chinese Charset Detector", 
                                  NS_CHARSET_DETECTOR_PROGID_BASE "zhpsm", 
                                  path,
                                  PR_TRUE, PR_TRUE);
  rv = compMgr->RegisterComponent(kZHStringPSMDetectorCID, 
                                  "PSM based Chinese String Charset Detector", 
                                  NS_STRCDETECTOR_PROGID_BASE "zhpsm", 
                                  path,
                                  PR_TRUE, PR_TRUE);
  rv = compMgr->RegisterComponent(kCJKPSMDetectorCID, 
                                  "PSM based CJK Charset Detector", 
                                  NS_CHARSET_DETECTOR_PROGID_BASE "cjkpsm", 
                                  path,
                                  PR_TRUE, PR_TRUE);
  rv = compMgr->RegisterComponent(kCJKStringPSMDetectorCID, 
                                  "PSM based CJK String Charset Detector", 
                                  NS_STRCDETECTOR_PROGID_BASE "cjkpsm", 
                                  path,
                                  PR_TRUE, PR_TRUE);
  rv = compMgr->RegisterComponent(kRUProbDetectorCID, 
                                  "Probability based Russian Charset Detector", 
                                  NS_CHARSET_DETECTOR_PROGID_BASE "ruprob", 
                                  path,
                                  PR_TRUE, PR_TRUE);
  rv = compMgr->RegisterComponent(kUKProbDetectorCID, 
                                  "Probability based Ukrainian Charset Detector", 
                                  NS_CHARSET_DETECTOR_PROGID_BASE "ukprob", 
                                  path,
                                  PR_TRUE, PR_TRUE);
  rv = compMgr->RegisterComponent(kRUStringProbDetectorCID, 
                                  "Probability based Russian String Charset Detector", 
                                  NS_STRCDETECTOR_PROGID_BASE "ruprob", 
                                  path,
                                  PR_TRUE, PR_TRUE);
  rv = compMgr->RegisterComponent(kUKStringProbDetectorCID, 
                                  "Probability based Ukrainian String Charset Detector", 
                                  NS_STRCDETECTOR_PROGID_BASE "ukprob", 
                                  path,
                                  PR_TRUE, PR_TRUE);
#ifdef INCLUDE_DBGDETECTOR
  rv = compMgr->RegisterComponent(k1stBlkDbgDetectorCID,
                                  "Debuging Detector 1st block", 
                                  NS_CHARSET_DETECTOR_PROGID_BASE "1stblkdbg", 
                                  path,
                                  PR_TRUE, PR_TRUE);
  rv = compMgr->RegisterComponent(k2ndBlkDbgDetectorCID,
                                  "Debuging Detector 2nd block", 
                                  NS_CHARSET_DETECTOR_PROGID_BASE "2ndblkdbg", 
                                  path,
                                  PR_TRUE, PR_TRUE);
  rv = compMgr->RegisterComponent(klastBlkDbgDetectorCID,
                                  "Debuging Detector last block", 
                                  NS_CHARSET_DETECTOR_PROGID_BASE "lastblkdbg", 
                                  path,
                                  PR_TRUE, PR_TRUE);
#endif /* INCLUDE_DBGDETECTOR */

  (void)servMgr->ReleaseService(kComponentManagerCID, compMgr);
  return rv;
}

extern "C" NS_EXPORT nsresult NSUnregisterSelf(nsISupports* aServMgr, const char *path)
{
  nsresult rv;

  nsCOMPtr<nsIServiceManager> servMgr(do_QueryInterface(aServMgr, &rv));
  if (NS_FAILED(rv)) return rv;

  nsIComponentManager* compMgr;
  rv = servMgr->GetService(kComponentManagerCID, 
                           nsIComponentManager::GetIID(), 
                           (nsISupports**)&compMgr);
  if (NS_FAILED(rv)) return rv;

  rv = compMgr->UnregisterComponent(kMetaCharsetCID, path);
  rv = compMgr->UnregisterComponent(kXMLEncodingCID, path);
  rv = compMgr->UnregisterComponent(kCharsetDetectionAdaptorCID, path);
  rv = compMgr->UnregisterComponent(kJAPSMDetectorCID, path);
  rv = compMgr->UnregisterComponent(kJAStringPSMDetectorCID, path);
  rv = compMgr->UnregisterComponent(kKOPSMDetectorCID, path);
  rv = compMgr->UnregisterComponent(kKOStringPSMDetectorCID, path);
  rv = compMgr->UnregisterComponent(kZHTWPSMDetectorCID, path);
  rv = compMgr->UnregisterComponent(kZHTWStringPSMDetectorCID, path);
  rv = compMgr->UnregisterComponent(kZHCNPSMDetectorCID, path);
  rv = compMgr->UnregisterComponent(kZHCNStringPSMDetectorCID, path);
  rv = compMgr->UnregisterComponent(kZHPSMDetectorCID, path);
  rv = compMgr->UnregisterComponent(kZHStringPSMDetectorCID, path);
  rv = compMgr->UnregisterComponent(kCJKPSMDetectorCID, path);
  rv = compMgr->UnregisterComponent(kCJKStringPSMDetectorCID, path);
  rv = compMgr->UnregisterComponent(kRUProbDetectorCID, path);
  rv = compMgr->UnregisterComponent(kUKProbDetectorCID, path);
  rv = compMgr->UnregisterComponent(kRUStringProbDetectorCID, path);
  rv = compMgr->UnregisterComponent(kUKStringProbDetectorCID, path);
#ifdef INCLUDE_DBGDETECTOR
  rv = compMgr->UnregisterComponent(k1stBlkDbgDetectorCID, path);
  rv = compMgr->UnregisterComponent(k2ndBlkDbgDetectorCID, path);
  rv = compMgr->UnregisterComponent(klastBlkDbgDetectorCID, path);
#endif /* INCLUDE_DBGDETECTOR */

  (void)servMgr->ReleaseService(kComponentManagerCID, compMgr);
  return rv;
}

