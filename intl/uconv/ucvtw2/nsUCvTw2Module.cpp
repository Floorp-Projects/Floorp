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
 *
 * Contributor(s): 
 *   Pierre Phaneuf <pp@ludusdesign.com>
 */

#define NS_IMPL_IDS

#include "nspr.h"
#include "nsString.h"
#include "pratom.h"
#include "nsCOMPtr.h"
#include "nsIFactory.h"
#include "nsIRegistry.h"
#include "nsIServiceManager.h"
#include "nsIModule.h"
#include "nsUCvTW2CID.h"
#include "nsUCvTW2Dll.h"

#include "nsEUCTWToUnicode.h"
#include "nsUnicodeToEUCTW.h"
#include "nsUnicodeToCNS11643p1.h"
#include "nsUnicodeToCNS11643p2.h"
#include "nsUnicodeToCNS11643p3.h"
#include "nsUnicodeToCNS11643p4.h"
#include "nsUnicodeToCNS11643p5.h"
#include "nsUnicodeToCNS11643p6.h"
#include "nsUnicodeToCNS11643p7.h"

//----------------------------------------------------------------------------
// Global functions and data [declaration]

static NS_DEFINE_CID(kComponentManagerCID, NS_COMPONENTMANAGER_CID);

#define DECODER_NAME_BASE "Unicode Decoder-"
#define ENCODER_NAME_BASE "Unicode Encoder-"

PRInt32 g_InstanceCount = 0;
PRInt32 g_LockCount = 0;

PRUint16 g_ASCIIMappingTable[] = {
  0x0001, 0x0004, 0x0005, 0x0008, 0x0000, 0x0000, 0x007F, 0x0000
};

PRUint16 g_ufCNS1MappingTable[] = {
#include "cns_1.uf"
};

PRUint16 g_ufCNS2MappingTable[] = {
#include "cns_2.uf"
};

PRUint16 g_ufCNS3MappingTable[] = {
#include "cns3.uf"
};

PRUint16 g_ufCNS4MappingTable[] = {
#include "cns4.uf"
};

PRUint16 g_ufCNS5MappingTable[] = {
#include "cns5.uf"
};

PRUint16 g_ufCNS6MappingTable[] = {
#include "cns6.uf"
};

PRUint16 g_ufCNS7MappingTable[] = {
#include "cns7.uf"
};

PRUint16 g_utCNS1MappingTable[] = {
#include "cns_1.ut"
};

PRUint16 g_utCNS2MappingTable[] = {
#include "cns_2.ut"
};

PRUint16 g_utCNS3MappingTable[] = {
#include "cns3.ut"
};

PRUint16 g_utCNS4MappingTable[] = {
#include "cns4.ut"
};

PRUint16 g_utCNS5MappingTable[] = {
#include "cns5.ut"
};

PRUint16 g_utCNS6MappingTable[] = {
#include "cns6.ut"
};

PRUint16 g_utCNS7MappingTable[] = {
#include "cns7.ut"
};

typedef nsresult (* fpCreateInstance) (nsISupports **);

struct FactoryData
{
  const nsCID   * mCID;
  fpCreateInstance  CreateInstance;
  char    * mCharsetSrc;
  char    * mCharsetDest;
};

static FactoryData g_FactoryData[] =
{
  {
    &kEUCTWToUnicodeCID,
    nsEUCTWToUnicode::CreateInstance,
    "x-euc-tw",
    "Unicode"
  },
  {
    &kUnicodeToEUCTWCID,
    nsUnicodeToEUCTW::CreateInstance,
    "Unicode",
    "x-euc-tw"
  },
  {
    &kUnicodeToCNS11643p1CID,
    nsUnicodeToCNS11643p1::CreateInstance,
    "Unicode",
    "x-cns-11643-1"
  },
  {
    &kUnicodeToCNS11643p2CID,
    nsUnicodeToCNS11643p2::CreateInstance,
    "Unicode",
    "x-cns-11643-2"
  },
  {
    &kUnicodeToCNS11643p3CID,
    nsUnicodeToCNS11643p3::CreateInstance,
    "Unicode",
    "x-cns-11643-3"
  },
  {
    &kUnicodeToCNS11643p4CID,
    nsUnicodeToCNS11643p4::CreateInstance,
    "Unicode",
    "x-cns-11643-4"
  },
  {
    &kUnicodeToCNS11643p5CID,
    nsUnicodeToCNS11643p5::CreateInstance,
    "Unicode",
    "x-cns-11643-5"
  },
  {
    &kUnicodeToCNS11643p6CID,
    nsUnicodeToCNS11643p6::CreateInstance,
    "Unicode",
    "x-cns-11643-6"
  },
  {
    &kUnicodeToCNS11643p7CID,
    nsUnicodeToCNS11643p7::CreateInstance,
    "Unicode",
    "x-cns-11643-7"
  }
};

#define ARRAY_SIZE(_array)                                      \
     (sizeof(_array) / sizeof(_array[0]))

//----------------------------------------------------------------------------
// Class nsConverterFactory [declaration]

/**
 * General factory class for converter objects.
 * 
 * @created         24/Feb/1998
 * @author  Catalin Rotaru [CATA]
 */
class nsConverterFactory : public nsIFactory
{
  NS_DECL_ISUPPORTS

private:

  FactoryData * mData;

public:

  /**
   * Class constructor.
   */
  nsConverterFactory(FactoryData * aData);

  /**
   * Class destructor.
   */
  virtual ~nsConverterFactory();

  //--------------------------------------------------------------------------
  // Interface nsIFactory [declaration]

  NS_IMETHOD CreateInstance(nsISupports *aDelegate, const nsIID &aIID,
                            void **aResult);
  NS_IMETHOD LockFactory(PRBool aLock);
};

//----------------------------------------------------------------------------
// Class nsConverterModule [declaration]

class nsConverterModule : public nsIModule 
{
  NS_DECL_ISUPPORTS
  NS_DECL_NSIMODULE

private:

  PRBool mInitialized;

  void Shutdown();

public:

  nsConverterModule();

  virtual ~nsConverterModule();

  nsresult Initialize();

};

//----------------------------------------------------------------------------
// Global functions and data [implementation]

static nsConverterModule * gModule = NULL;

extern "C" NS_EXPORT nsresult NSGetModule(nsIComponentManager * compMgr,
                                          nsIFile* location,
                                          nsIModule** return_cobj)
{
  nsresult rv = NS_OK;

  NS_ENSURE_ARG_POINTER(return_cobj);
  NS_ENSURE_FALSE(gModule, NS_ERROR_FAILURE);

  // Create an initialize the module instance
  nsConverterModule * m = new nsConverterModule();
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

//----------------------------------------------------------------------------
// Class nsConverterFactory [implementation]

NS_IMPL_ISUPPORTS(nsConverterFactory, NS_GET_IID(nsIFactory));

nsConverterFactory::nsConverterFactory(FactoryData * aData) 
{
  mData = aData;

  NS_INIT_REFCNT();
  PR_AtomicIncrement(&g_InstanceCount);
}

nsConverterFactory::~nsConverterFactory() 
{
  PR_AtomicDecrement(&g_InstanceCount);
}

//----------------------------------------------------------------------------
// Interface nsIFactory [implementation]

NS_IMETHODIMP nsConverterFactory::CreateInstance(nsISupports *aDelegate,
                                                 const nsIID &aIID,
                                                 void **aResult)
{
  if (aResult == NULL) return NS_ERROR_NULL_POINTER;
  if (aDelegate != NULL) return NS_ERROR_NO_AGGREGATION;

  nsISupports * t;
  mData->CreateInstance(&t);
  if (t == NULL) return NS_ERROR_OUT_OF_MEMORY;
  
  NS_ADDREF(t);  // Stabilize
  
  nsresult res = t->QueryInterface(aIID, aResult);

  NS_RELEASE(t); // Destabilize and avoid leaks. Avoid calling delete <interface pointer>  

  return res;
}

NS_IMETHODIMP nsConverterFactory::LockFactory(PRBool aLock)
{
  if (aLock) PR_AtomicIncrement(&g_LockCount);
  else PR_AtomicDecrement(&g_LockCount);

  return NS_OK;
}

//----------------------------------------------------------------------------
// Class nsConverterModule [implementation]

NS_IMPL_ISUPPORTS(nsConverterModule, NS_GET_IID(nsIModule))

nsConverterModule::nsConverterModule()
: mInitialized(PR_FALSE)
{
  NS_INIT_ISUPPORTS();
}

nsConverterModule::~nsConverterModule()
{
  Shutdown();
}

nsresult nsConverterModule::Initialize()
{
  return NS_OK;
}

void nsConverterModule::Shutdown()
{
}

//----------------------------------------------------------------------------
// Interface nsIModule [implementation]

NS_IMETHODIMP nsConverterModule::GetClassObject(nsIComponentManager *aCompMgr,
                                                const nsCID& aClass,
                                                const nsIID& aIID,
                                                void ** r_classObj)
{
  nsresult rv;

  // Defensive programming: Initialize *r_classObj in case of error below
  if (!r_classObj) {
    return NS_ERROR_INVALID_POINTER;
  }
  *r_classObj = NULL;

  if (!mInitialized) {
    rv = Initialize();
    if (NS_FAILED(rv)) {
      return rv;
    }
    mInitialized = PR_TRUE;
  }

  FactoryData * data;
  nsConverterFactory * fact;

  // XXX cache these factories
  for (PRUint32 i=0; i<ARRAY_SIZE(g_FactoryData); i++) {
    data = &(g_FactoryData[i]);
    if (aClass.Equals(*(data->mCID))) {
      fact = new nsConverterFactory(data);
      if (fact == NULL) {
        return NS_ERROR_OUT_OF_MEMORY;
      }
      rv = fact->QueryInterface(aIID, (void **) r_classObj);
      if (NS_FAILED(rv)) delete fact;

      return rv;
    }
  }

  return NS_ERROR_FACTORY_NOT_REGISTERED;
}

NS_IMETHODIMP nsConverterModule::RegisterSelf(nsIComponentManager *aCompMgr,
                                              nsIFile* aPath,
                                              const char* registryLocation,
                                              const char* componentType)
{
  nsresult res;
  PRUint32 i;
  nsIRegistry * registry = NULL;
  nsRegistryKey key;
  char buff[1024];

  // get the registry
  res = nsServiceManager::GetService(NS_REGISTRY_CONTRACTID, 
      NS_GET_IID(nsIRegistry), (nsISupports**)&registry);
  if (NS_FAILED(res)) goto done;

  // open the registry
  res = registry->OpenWellKnownRegistry(
      nsIRegistry::ApplicationComponentRegistry);
  if (NS_FAILED(res)) goto done;

  char name[128];
  char contractid[128];
  char * cid_string;
  for (i=0; i<ARRAY_SIZE(g_FactoryData); i++) {
    if(0==PL_strcmp(g_FactoryData[i].mCharsetSrc,"Unicode"))
    {
       PL_strcpy(name, ENCODER_NAME_BASE);
       PL_strcat(name, g_FactoryData[i].mCharsetDest);
       PL_strcpy(contractid, NS_UNICODEENCODER_CONTRACTID_BASE);
       PL_strcat(contractid, g_FactoryData[i].mCharsetDest);
    } else {
       PL_strcpy(name, DECODER_NAME_BASE);
       PL_strcat(name, g_FactoryData[i].mCharsetSrc);
       PL_strcpy(contractid, NS_UNICODEDECODER_CONTRACTID_BASE);
       PL_strcat(contractid, g_FactoryData[i].mCharsetSrc);
    }
    // register component
    res = aCompMgr->RegisterComponentSpec(*(g_FactoryData[i].mCID), name, 
      contractid, aPath, PR_TRUE, PR_TRUE);
    if(NS_FAILED(res) && (NS_ERROR_FACTORY_EXISTS != res)) goto done;

    // register component info
    // XXX take these KONSTANTS out of here; refine this code
    cid_string = g_FactoryData[i].mCID->ToString();
    sprintf(buff, "%s/%s", "software/netscape/intl/uconv", cid_string);
    nsCRT::free(cid_string);
    res = registry -> AddSubtree(nsIRegistry::Common, buff, &key);
    if (NS_FAILED(res)) goto done;
    res = registry -> SetStringUTF8(key, "source", g_FactoryData[i].mCharsetSrc);
    if (NS_FAILED(res)) goto done;
    res = registry -> SetStringUTF8(key, "destination", g_FactoryData[i].mCharsetDest);
    if (NS_FAILED(res)) goto done;
  }

done:
  if (registry != NULL) {
    nsServiceManager::ReleaseService(NS_REGISTRY_CONTRACTID, registry);
  }

  return res;
}

NS_IMETHODIMP nsConverterModule::UnregisterSelf(nsIComponentManager *aCompMgr,
                                                nsIFile* aPath,
                                                const char* registryLocation)
{
  // XXX also delete the stuff I added to the registry
  nsresult rv;

  for (PRUint32 i=0; i<ARRAY_SIZE(g_FactoryData); i++) {
    rv = aCompMgr->UnregisterComponentSpec(*(g_FactoryData[i].mCID), aPath);
  }

  return NS_OK;
}

NS_IMETHODIMP nsConverterModule::CanUnload(nsIComponentManager *aCompMgr, 
                                           PRBool *okToUnload)
{
  if (!okToUnload) {
    return NS_ERROR_INVALID_POINTER;
  }
  *okToUnload = (g_InstanceCount == 0 && g_LockCount == 0);
  return NS_OK;
}

