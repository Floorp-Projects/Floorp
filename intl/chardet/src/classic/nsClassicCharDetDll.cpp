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

#include "pratom.h"
#include "nsClassicCharDetDll.h"
#include "nsISupports.h"
#include "nsIComponentManager.h"
#include "nsIFactory.h"
#include "nsIServiceManager.h"
#include "nsCOMPtr.h"
#include "nsICharsetDetector.h"
#include "nsIStringCharsetDetector.h"
#include "nsClassicDetectors.h"


static NS_DEFINE_CID(kJAClassicDetectorCID,         NS_JA_CLASSIC_DETECTOR_CID);
static NS_DEFINE_CID(kJAClassicStringDetectorCID,   NS_JA_CLASSIC_STRING_DETECTOR_CID);
static NS_DEFINE_CID(kKOClassicDetectorCID,         NS_KO_CLASSIC_DETECTOR_CID);
static NS_DEFINE_CID(kKOClassicStringDetectorCID,   NS_KO_CLASSIC_STRING_DETECTOR_CID);
static NS_DEFINE_CID(kComponentManagerCID, NS_COMPONENTMANAGER_CID);
static NS_DEFINE_IID(kFactoryIID, NS_IFACTORY_IID);

PRInt32 g_InstanceCount = 0;
PRInt32 g_LockCount = 0;


extern "C" NS_EXPORT nsresult NSGetFactory(nsISupports* aServMgr,
                                           const nsCID &aClass,
                                           const char *aClassName,
                                           const char *aContractID,
                                           nsIFactory **aFactory)
{
  if (aFactory == NULL) {
    return NS_ERROR_NULL_POINTER;
  }

  nsIFactory *factory = nsnull;
  if (aClass.Equals(kJAClassicDetectorCID)) {
    ;
    //bug#13844 disable this until find out the reason of the freeze
    //factory = NEW_JA_CLASSICDETECTOR_FACTORY();
  } else if (aClass.Equals(kJAClassicStringDetectorCID)) {
    factory = NEW_JA_STRING_CLASSICDETECTOR_FACTORY();
#if 0
  } else if (aClass.Equals(kKOClassicDetectorCID)) {
    factory = NEW_KO_CLASSICDETECTOR_FACTORY();
  } else if (aClass.Equals(kKOClassicStringDetectorCID)) {
    factory = NEW_KO_STRING_CLASSICDETECTOR_FACTORY();
#endif
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
                           NS_GET_IID(nsIComponentManager), 
                           (nsISupports**)&compMgr);
  if (NS_FAILED(rv)) return rv;

  rv = compMgr->RegisterComponent(kJAClassicDetectorCID, 
                                  "Classic JA Charset Detector", 
                                  NS_CHARSET_DETECTOR_CONTRACTID_BASE "jaclassic", 
                                  path,
                                  PR_TRUE, PR_TRUE);

  rv = compMgr->RegisterComponent(kJAClassicStringDetectorCID, 
                                  "Classic JA String Charset Detector", 
                                  NS_STRCDETECTOR_CONTRACTID_BASE "jaclassic", 
                                  path,
                                  PR_TRUE, PR_TRUE);

  rv = compMgr->RegisterComponent(kKOClassicDetectorCID, 
                                  "Classic KO Charset Detector", 
                                  NS_CHARSET_DETECTOR_CONTRACTID_BASE "koclassic", 
                                  path,
                                  PR_TRUE, PR_TRUE);

  rv = compMgr->RegisterComponent(kKOClassicStringDetectorCID, 
                                  "Classic KO String Charset Detector", 
                                  NS_STRCDETECTOR_CONTRACTID_BASE "koclassic", 
                                  path,
                                  PR_TRUE, PR_TRUE);

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
                           NS_GET_IID(nsIComponentManager), 
                           (nsISupports**)&compMgr);
  if (NS_FAILED(rv)) return rv;

  rv = compMgr->UnregisterComponent(kJAClassicDetectorCID, path);
  rv = compMgr->UnregisterComponent(kJAClassicStringDetectorCID, path);
  rv = compMgr->UnregisterComponent(kKOClassicDetectorCID, path);
  rv = compMgr->UnregisterComponent(kKOClassicStringDetectorCID, path);

  (void)servMgr->ReleaseService(kComponentManagerCID, compMgr);
  return rv;
}

