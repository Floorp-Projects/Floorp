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
#include "nsMetaCharsetObserver.h"
#include "nsXMLEncodingObserver.h"
#include "nsDetectionAdaptor.h"
#include "nsICharsetDetector.h"
#include "nsIStringCharsetDetector.h"

#include "nsMetaCharsetCID.h"
#include "nsXMLEncodingCID.h"
#include "nsCharsetDetectionAdaptorCID.h"
#include "nsDocumentCharsetInfo.h"

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


//#define INCLUDE_DBGDETECTOR
#ifdef INCLUDE_DBGDETECTOR
// for debuging only
#include "nsDebugDetector.h"
NS_DEFINE_CID(k1stBlkDbgDetectorCID,  NS_1STBLKDBG_DETECTOR_CID);
NS_DEFINE_CID(k2ndBlkDbgDetectorCID,  NS_2NDBLKDBG_DETECTOR_CID);
NS_DEFINE_CID(klastBlkDbgDetectorCID, NS_LASTBLKDBG_DETECTOR_CID);
#endif /* INCLUDE_DBGDETECTOR  */

NS_DEFINE_IID(kFactoryIID, NS_IFACTORY_IID);
NS_DEFINE_CID(kMetaCharsetCID, NS_META_CHARSET_CID);
NS_DEFINE_CID(kXMLEncodingCID, NS_XML_ENCODING_CID);
NS_DEFINE_CID(kCharsetDetectionAdaptorCID, NS_CHARSET_DETECTION_ADAPTOR_CID);
NS_DEFINE_CID(kDocumentCharsetInfoCID, NS_DOCUMENTCHARSETINFO_CID);

PRInt32 g_InstanceCount = 0;
PRInt32 g_LockCount = 0;

//----------------------------------------------------------------------

// Module implementation for the CharDet library
class nsCharDetModule : public nsIModule
{
public:
  nsCharDetModule();
  virtual ~nsCharDetModule();

  NS_DECL_ISUPPORTS

  NS_DECL_NSIMODULE

protected:
  nsresult Initialize();

  void Shutdown();

  PRBool mInitialized;
};

//----------------------------------------------------------------------

static NS_DEFINE_IID(kIModuleIID, NS_IMODULE_IID);

nsCharDetModule::nsCharDetModule()
  : mInitialized(PR_FALSE)
{
  NS_INIT_ISUPPORTS();
}

nsCharDetModule::~nsCharDetModule()
{
  Shutdown();
}

NS_IMPL_ISUPPORTS(nsCharDetModule, kIModuleIID)

// Perform our one-time intialization for this module
nsresult
nsCharDetModule::Initialize()
{
  if (mInitialized) {
    return NS_OK;
  }
  mInitialized = PR_TRUE;
  return NS_OK;
}

// Shutdown this module, releasing all of the module resources
void
nsCharDetModule::Shutdown()
{
}

// Create a factory object for creating instances of aClass.
NS_IMETHODIMP
nsCharDetModule::GetClassObject(nsIComponentManager *aCompMgr,
                               const nsCID& aClass,
                               const nsIID& aIID,
                               void** r_classObj)
{
  nsresult rv;

  // Defensive programming: Initialize *r_classObj in case of error below
  if (!r_classObj) {
    return NS_ERROR_INVALID_POINTER;
  }
  *r_classObj = NULL;

  // Do one-time-only initialization if necessary
  if (!mInitialized) {
    rv = Initialize();
    if (NS_FAILED(rv)) {
      // Initialization failed! yikes!
      return rv;
    }
  }

  // Choose the appropriate factory, based on the desired instance
  // class type (aClass).
  nsIFactory* factory = nsnull;
  if (aClass.Equals(kMetaCharsetCID)) {
    factory = NEW_META_CHARSET_OBSERVER_FACTORY();
  } else if (aClass.Equals(kDocumentCharsetInfoCID)) {
    factory = NEW_DOCUMENT_CHARSET_INFO_FACTORY();
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
  else {
		rv = NS_ERROR_FACTORY_NOT_REGISTERED;
#ifdef DEBUG
    char* cs = aClass.ToString();
    printf("+++ nsCharDetModule: unable to create factory for %s\n", cs);
    nsCRT::free(cs);
#endif
  }

  if (factory) {
    rv = factory->QueryInterface(aIID, r_classObj);
    if (NS_FAILED(rv)) {
      delete factory;           // XXX only works if virtual dtors were used!
    }
  }

  return rv;
}

//----------------------------------------

struct Components {
  const char* mDescription;
  const nsID* mCID;
  const char* mContractID;
};

// The list of components we register
static Components gComponents[] = {
  { "Meta Charset", &kMetaCharsetCID,
    NS_META_CHARSET_CONTRACTID, },
  { "Document Charset Info", &kDocumentCharsetInfoCID,
    NS_DOCUMENTCHARSETINFO_PID, },
  { "XML Encoding", &kXMLEncodingCID,
    NS_XML_ENCODING_CONTRACTID, },
  { "Charset Detection Adaptor", &kCharsetDetectionAdaptorCID,
    NS_CHARSET_DETECTION_ADAPTOR_CONTRACTID, },
  { "PSM based Japanese Charset Detector", &kJAPSMDetectorCID,
    NS_CHARSET_DETECTOR_CONTRACTID_BASE "ja_parallel_state_machine", },
  { "PSM based Japanese String Charset Detector", &kJAStringPSMDetectorCID,
    NS_STRCDETECTOR_CONTRACTID_BASE "ja_parallel_state_machine", },
  { "PSM based Korean Charset Detector", &kKOPSMDetectorCID,
    NS_CHARSET_DETECTOR_CONTRACTID_BASE "ko_parallel_state_machine", },
  { "PSM based Korean String Charset Detector", &kKOStringPSMDetectorCID,
    NS_STRCDETECTOR_CONTRACTID_BASE "ko_parallel_state_machine", },
  { "PSM based Traditional Chinese Charset Detector", &kZHTWPSMDetectorCID,
    NS_CHARSET_DETECTOR_CONTRACTID_BASE "zhtw_parallel_state_machine", },
  { "PSM based Traditional Chinese String Charset Detector",
    &kZHTWStringPSMDetectorCID, 
    NS_STRCDETECTOR_CONTRACTID_BASE "zhtw_parallel_state_machine", },
  { "PSM based Simplified Chinese Charset Detector", &kZHCNPSMDetectorCID,
    NS_CHARSET_DETECTOR_CONTRACTID_BASE "zhcn_parallel_state_machine", },
  { "PSM based Simplified Chinese String Charset Detector",
    &kZHCNStringPSMDetectorCID, 
    NS_STRCDETECTOR_CONTRACTID_BASE "zhcn_parallel_state_machine", },
  { "PSM based Chinese Charset Detector", &kZHPSMDetectorCID,
    NS_CHARSET_DETECTOR_CONTRACTID_BASE "zh_parallel_state_machine", },
  { "PSM based Chinese String Charset Detector", &kZHStringPSMDetectorCID,
    NS_STRCDETECTOR_CONTRACTID_BASE "zh_parallel_state_machine", },
  { "PSM based CJK Charset Detector", &kCJKPSMDetectorCID,
    NS_CHARSET_DETECTOR_CONTRACTID_BASE "cjk_parallel_state_machine", },
  { "PSM based CJK String Charset Detector", &kCJKStringPSMDetectorCID,
    NS_STRCDETECTOR_CONTRACTID_BASE "cjk_parallel_state_machine", },
  { "Probability based Russian Charset Detector", &kRUProbDetectorCID,
    NS_CHARSET_DETECTOR_CONTRACTID_BASE "ruprob", },
  { "Probability based Ukrainian Charset Detector", &kUKProbDetectorCID,
    NS_CHARSET_DETECTOR_CONTRACTID_BASE "ukprob", },
  { "Probability based Russian String Charset Detector",
    &kRUStringProbDetectorCID, 
    NS_STRCDETECTOR_CONTRACTID_BASE "ruprob", },
  { "Probability based Ukrainian String Charset Detector",
    &kUKStringProbDetectorCID, 
    NS_STRCDETECTOR_CONTRACTID_BASE "ukprob", },
#ifdef INCLUDE_DBGDETECTOR
  { "Debuging Detector 1st block", &k1stBlkDbgDetectorCID,
    NS_CHARSET_DETECTOR_CONTRACTID_BASE "1stblkdbg", },
  { "Debuging Detector 2nd block", &k2ndBlkDbgDetectorCID,
    NS_CHARSET_DETECTOR_CONTRACTID_BASE "2ndblkdbg", },
  { "Debuging Detector last block", &klastBlkDbgDetectorCID,
    NS_CHARSET_DETECTOR_CONTRACTID_BASE "lastblkdbg", },
#endif /* INCLUDE_DBGDETECTOR */
};
#define NUM_COMPONENTS (sizeof(gComponents) / sizeof(gComponents[0]))

NS_IMETHODIMP
nsCharDetModule::RegisterSelf(nsIComponentManager *aCompMgr,
                              nsIFile* aPath,
                              const char* registryLocation,
                              const char* componentType)
{
  nsresult rv = NS_OK;

#ifdef DEBUG
  printf("*** Registering CharDet components\n");
#endif

  Components* cp = gComponents;
  Components* end = cp + NUM_COMPONENTS;
  while (cp < end) {
    rv = aCompMgr->RegisterComponentSpec(*cp->mCID, cp->mDescription,
                                         cp->mContractID, aPath, PR_TRUE,
                                         PR_TRUE);
    if (NS_FAILED(rv)) {
#ifdef DEBUG
      printf("nsCharDetModule: unable to register %s component => %x\n",
             cp->mDescription, rv);
#endif
      break;
    }
    cp++;
  }

  // get the registry
  nsRegistryKey key;
  nsIRegistry* registry;
  rv = nsServiceManager::GetService(NS_REGISTRY_CONTRACTID,
                                    NS_GET_IID(nsIRegistry),
                                    (nsISupports**)&registry);
  if (NS_FAILED(rv)) {
    return rv;
  }

  // open the registry
  rv = registry->OpenWellKnownRegistry(
    nsIRegistry::ApplicationComponentRegistry);
  if (NS_FAILED(rv)) {
    goto done;
  }

  rv = registry -> AddSubtree(nsIRegistry::Common, 
                              NS_CHARSET_DETECTOR_REG_BASE "off" ,&key);
  if (NS_SUCCEEDED(rv)) {
    rv = registry-> SetStringUTF8(key, "type", "off");
    rv = registry-> SetStringUTF8(key, "defaultEnglishText", "Off");
  }
  rv = registry -> AddSubtree(nsIRegistry::Common, 
                              NS_CHARSET_DETECTOR_REG_BASE "ja_parallel_state_machine" ,&key);
  if (NS_SUCCEEDED(rv)) {
    rv = registry-> SetStringUTF8(key, "type", "ja_parallel_state_machine");
    rv = registry-> SetStringUTF8(key, "defaultEnglishText", "Japanese");
  }
  rv = registry -> AddSubtree(nsIRegistry::Common, 
                              NS_CHARSET_DETECTOR_REG_BASE "ko_parallel_state_machine" ,&key);
  if (NS_SUCCEEDED(rv)) {
    rv = registry-> SetStringUTF8(key, "type", "ko_parallel_state_machine");
    rv = registry-> SetStringUTF8(key, "defaultEnglishText", "Korean");
  }
  rv = registry -> AddSubtree(nsIRegistry::Common, 
                           NS_CHARSET_DETECTOR_REG_BASE "zhtw_parallel_state_machine" ,&key);
  if (NS_SUCCEEDED(rv)) {
    rv = registry-> SetStringUTF8(key, "type", "zhtw_parallel_state_machine");
    rv = registry-> SetStringUTF8(key, "defaultEnglishText", "Traditional Chinese");
  }
  rv = registry -> AddSubtree(nsIRegistry::Common, 
                           NS_CHARSET_DETECTOR_REG_BASE "zhcn_parallel_state_machine" ,&key);
  if (NS_SUCCEEDED(rv)) {
    rv = registry-> SetStringUTF8(key, "type", "zhtw_parallel_state_machine");
    rv = registry-> SetStringUTF8(key, "defaultEnglishText", "Simplified Chinese");
  }
  rv = registry -> AddSubtree(nsIRegistry::Common, 
                           NS_CHARSET_DETECTOR_REG_BASE "zh_parallel_state_machine" ,&key);
  if (NS_SUCCEEDED(rv)) {
    rv = registry-> SetStringUTF8(key, "type", "zh_parallel_state_machine");
    rv = registry-> SetStringUTF8(key, "defaultEnglishText", "Chinese");
  }
  rv = registry -> AddSubtree(nsIRegistry::Common, 
                           NS_CHARSET_DETECTOR_REG_BASE "cjk_parallel_state_machine" ,&key);
  if (NS_SUCCEEDED(rv)) {
    rv = registry-> SetStringUTF8(key, "type", "cjk_parallel_state_machine");
    rv = registry-> SetStringUTF8(key, "defaultEnglishText", "East Asian");
  }
  rv = registry -> AddSubtree(nsIRegistry::Common, 
                           NS_CHARSET_DETECTOR_REG_BASE "ruprob" ,&key);
  if (NS_SUCCEEDED(rv)) {
    rv = registry-> SetStringUTF8(key, "type", "ruprob");
    rv = registry-> SetStringUTF8(key, "defaultEnglishText", "Russian");
  }
  rv = registry -> AddSubtree(nsIRegistry::Common, 
                           NS_CHARSET_DETECTOR_REG_BASE "ukprob" ,&key);
  if (NS_SUCCEEDED(rv)) {
    rv = registry-> SetStringUTF8(key, "type", "ukprob");
    rv = registry-> SetStringUTF8(key, "defaultEnglishText", "Ukrainian");
  }

 done:
  if(nsnull != registry) {
      nsServiceManager::ReleaseService(NS_REGISTRY_CONTRACTID, registry);
  }

  return rv;
}

NS_IMETHODIMP
nsCharDetModule::UnregisterSelf(nsIComponentManager* aCompMgr,
                                nsIFile* aPath,
                                const char* registryLocation)
{
#ifdef DEBUG
  printf("*** Unregistering CharDet components\n");
#endif
  Components* cp = gComponents;
  Components* end = cp + NUM_COMPONENTS;
  while (cp < end) {
    nsresult rv = aCompMgr->UnregisterComponentSpec(*cp->mCID, aPath);
    if (NS_FAILED(rv)) {
#ifdef DEBUG
      printf("nsCharDetModule: unable to unregister %s component => %x\n",
             cp->mDescription, rv);
#endif
    }
    cp++;
  }

  return NS_OK;
}

NS_IMETHODIMP
nsCharDetModule::CanUnload(nsIComponentManager *aCompMgr, PRBool *okToUnload)
{
  if (!okToUnload) {
    return NS_ERROR_INVALID_POINTER;
  }
  *okToUnload = (g_InstanceCount == 0) && (g_LockCount == 0);
  return NS_OK;
}

//----------------------------------------------------------------------

static nsCharDetModule *gModule = NULL;

extern "C" NS_EXPORT nsresult NSGetModule(nsIComponentManager *servMgr,
                                          nsIFile* location,
                                          nsIModule** return_cobj)
{
    nsresult rv = NS_OK;

    NS_ASSERTION(return_cobj, "Null argument");
    NS_ASSERTION(gModule == NULL, "nsCharDetModule: Module already created.");

    // Create an initialize the layout module instance
    nsCharDetModule *m = new nsCharDetModule();
    if (!m) {
        return NS_ERROR_OUT_OF_MEMORY;
    }

    // Increase refcnt and store away nsIModule interface to m in return_cobj
    rv = m->QueryInterface(NS_GET_IID(nsIModule), (void**)return_cobj);
    if (NS_FAILED(rv)) {
        delete m;
        m = nsnull;
    }
    gModule = m;                  // WARNING: Weak Reference
    return rv;
}
