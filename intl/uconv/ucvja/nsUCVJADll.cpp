/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * The contents of this file are subject to the Netscape Public License
 * Version 1.0 (the "License"); you may not use this file except in
 * compliance with the License.  You may obtain a copy of the License at
 * http://www.mozilla.org/NPL/
 *
 * Software distributed under the License is distributed on an "AS IS"
 * basis, WITHOUT WARRANTY OF ANY KIND, either express or implied.  See
 * the License for the specific language governing rights and limitations
 * under the License.
 *
 * The Original Code is Mozilla Communicator client code.
 *
 * The Initial Developer of the Original Code is Netscape Communications
 * Corporation.  Portions created by Netscape are Copyright (C) 1998
 * Netscape Communications Corporation.  All Rights Reserved.
 */

#define NS_IMPL_IDS

#include "pratom.h"
#include "nsIComponentManager.h"
#include "nsIServiceManager.h"
#include "nsIFactory.h"
#include "nsIRegistry.h"
#include "nsCOMPtr.h"
#include "nsICharsetConverterInfo.h"
#include "nsUCVJACID.h"
#include "nsUCVJA2CID.h"

#include "nsUCVJADll.h"

#include "nsSJIS2Unicode.h"
#include "nsUnicodeToSJIS.h"
#include "nsEUCJPToUnicode.h"
#include "nsISO2022JPToUnicode.h"
#include "nsUnicodeToEUCJP.h"
#include "nsUnicodeToISO2022JP.h"
#include "nsUnicodeToJISx0201.h"
#include "nsUnicodeToJISx0208.h"
#include "nsUnicodeToJISx0212.h"

// just for NS_IMPL_IDS; this is a good, central place to implement GUIDs
#include "nsIUnicodeDecoder.h"
#include "nsIUnicodeDecodeUtil.h"
#include "nsIUnicodeEncoder.h"
#include "nsIUnicodeEncodeHelper.h"
#include "nsICharsetConverterManager.h"
#define DECODER_NAME_BASE "Unicode Decoder-"
#define ENCODER_NAME_BASE "Unicode Encoder-"

//----------------------------------------------------------------------
// Global functions and data [declaration]

static NS_DEFINE_IID(kISupportsIID, NS_ISUPPORTS_IID);
static NS_DEFINE_IID(kIFactoryIID, NS_IFACTORY_IID);
static NS_DEFINE_CID(kComponentManagerCID, NS_COMPONENTMANAGER_CID);

PRInt32 g_InstanceCount = 0;
PRInt32 g_LockCount = 0;
PRUint16 g_ut0201Mapping[] = {
#include "jis0201.ut"
};

PRUint16 g_ut0208Mapping[] = {
#include "jis0208.ut"
};

PRUint16 g_ut0212Mapping[] = {
#include "jis0212.ut"
};

PRUint16 g_uf0201Mapping[] = {
#include "jis0201.uf"
};

PRUint16 g_uf0208Mapping[] = {
#include "jis0208.uf"
};

PRUint16 g_uf0212Mapping[] = {
#include "jis0212.uf"
};


typedef nsresult (* fpCreateInstance) (nsISupports **);

struct FactoryData
{
  const nsCID   * mCID;
  fpCreateInstance  CreateInstance;
  char    * mCharsetSrc;
  char    * mCharsetDest;
};

FactoryData g_FactoryData[] =
{
  {
    &kSJIS2UnicodeCID,
    nsSJIS2Unicode::CreateInstance,
    "Shift_JIS",
    "Unicode"
  },
  {
    &kUnicodeToSJISCID,
    nsUnicodeToSJIS::CreateInstance,
    "Unicode",
    "Shift_JIS"
  },
  {
    &kISO2022JPToUnicodeCID,
    nsISO2022JPToUnicode::CreateInstance,
    "ISO-2022-JP",
    "Unicode"
  },
  {
    &kEUCJPToUnicodeCID,
    nsEUCJPToUnicode::CreateInstance,
    "EUC-JP",
    "Unicode"
  },
  {
    &kUnicodeToEUCJPCID,
    nsUnicodeToEUCJP::CreateInstance,
    "Unicode",
    "EUC-JP"
  },
  {
    &kUnicodeToJISx0201CID,
    nsUnicodeToJISx0201::CreateInstance,
    "Unicode",
    "jis_0201"
  },
  {
    &kUnicodeToJISx0208CID,
    nsUnicodeToJISx0208::CreateInstance,
    "Unicode",
    "jis_0208-1983"
  },
  {
    &kUnicodeToJISx0212CID,
    nsUnicodeToJISx0212::CreateInstance,
    "Unicode",
    "jis_0212-1990"
  },
  {
    &kUnicodeToISO2022JPCID,
    nsUnicodeToISO2022JP::CreateInstance,
    "Unicode",
    "ISO-2022-JP"
  }
};

#define ARRAY_SIZE(_array)                                      \
     (sizeof(_array) / sizeof(_array[0]))

//----------------------------------------------------------------------
// Class nsConverterFactory [declaration]

/**
 * General factory class for converter objects.
 * 
 * @created         24/Feb/1998
 * @author  Catalin Rotaru [CATA]
 */
class nsConverterFactory : public nsIFactory, 
public nsICharsetConverterInfo
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

  //--------------------------------------------------------------------
  // Interface nsIFactory [declaration]

  NS_IMETHOD CreateInstance(nsISupports *aDelegate, const nsIID &aIID,
                            void **aResult);

  NS_IMETHOD LockFactory(PRBool aLock);

  //--------------------------------------------------------------------
  // Interface nsICharsetConverterInfo [declaration]

  NS_IMETHOD GetCharsetSrc(char ** aCharset);
  NS_IMETHOD GetCharsetDest(char ** aCharset);
};

//----------------------------------------------------------------------
// Global functions and data [implementation]

extern "C" NS_EXPORT PRBool NSCanUnload(nsISupports* aServMgr)
{
  return PRBool(g_InstanceCount == 0 && g_LockCount == 0);
}

extern "C" NS_EXPORT nsresult NSGetFactory(nsISupports* aServMgr,
                                           const nsCID &aClass,
                                           const char *aClassName,
                                           const char *aProgID,
                                           nsIFactory **aFactory)
{
  if (aFactory == NULL) return NS_ERROR_NULL_POINTER;

  nsresult res;
  nsConverterFactory * fac;
  FactoryData * data;

  for (PRUint32 i=0; i<ARRAY_SIZE(g_FactoryData); i++) {
    data = &(g_FactoryData[i]);
    if (aClass.Equals(*(data->mCID))) {
      fac = new nsConverterFactory(data);
      res = fac->QueryInterface(kIFactoryIID, (void **) aFactory);
      if (NS_FAILED(res)) {
        *aFactory = NULL;
        delete fac;
      }

      return res;
    }
  }

  return NS_NOINTERFACE;
}

extern "C" NS_EXPORT nsresult NSRegisterSelf(nsISupports * aServMgr, 
                                             const char * path)
{
  nsresult res;
  PRUint32 i;
  nsIComponentManager * compMgr = NULL;
  nsIRegistry * registry = NULL;
  nsIRegistry::Key key;
  char buff[1024];

  // get the service manager
  nsCOMPtr<nsIServiceManager> servMgr(do_QueryInterface(aServMgr, &res));

  // get the component manager
  res = servMgr->GetService(kComponentManagerCID, 
                            nsIComponentManager::GetIID(), 
                            (nsISupports**)&compMgr);
  if (NS_FAILED(res)) goto done;

  // get the registry
  res = servMgr->GetService(NS_REGISTRY_PROGID, 
                            nsIRegistry::GetIID(), 
                            (nsISupports**)&registry);
  if (NS_FAILED(res)) goto done;

  // open the registry
  res = registry->OpenWellKnownRegistry(
      nsIRegistry::ApplicationComponentRegistry);
  if (NS_FAILED(res)) goto done;

  char name[128];
  char progid[128];
  for (i=0; i<ARRAY_SIZE(g_FactoryData); i++) {
    if(0==PL_strcmp(g_FactoryData[i].mCharsetSrc,"Unicode"))
    {
       PL_strcpy(name, DECODER_NAME_BASE);
       PL_strcat(name, g_FactoryData[i].mCharsetDest);
       PL_strcpy(progid, NS_UNICODEDECODER_PROGID_BASE);
       PL_strcat(progid, g_FactoryData[i].mCharsetDest);
    } else {
       PL_strcpy(name, ENCODER_NAME_BASE);
       PL_strcat(name, g_FactoryData[i].mCharsetSrc);
       PL_strcpy(progid, NS_UNICODEENCODER_PROGID_BASE);
       PL_strcat(progid, g_FactoryData[i].mCharsetSrc);
    }
    // register component
    res = compMgr->RegisterComponent(*(g_FactoryData[i].mCID), name, progid,
      path, PR_TRUE, PR_TRUE);
    if(NS_FAILED(res) && (NS_ERROR_FACTORY_EXISTS != res)) goto done;

    // register component info
    // XXX take these KONSTANTS out of here
    // XXX free the string from "ToString()"
    sprintf(buff, "%s/%s", "software/netscape/intl/uconv", (g_FactoryData[i].mCID -> ToString()));
    res = registry -> AddSubtree(nsIRegistry::Common, buff, &key);
    if (NS_FAILED(res)) goto done;
    res = registry -> SetString(key, "source", g_FactoryData[i].mCharsetSrc);
    if (NS_FAILED(res)) goto done;
    res = registry -> SetString(key, "destination", g_FactoryData[i].mCharsetDest);
    if (NS_FAILED(res)) goto done;
  }

done:
  if (compMgr != NULL) 
    (void)servMgr->ReleaseService(kComponentManagerCID, compMgr);
  if (registry != NULL) {
    registry -> Close();
    (void)servMgr->ReleaseService(NS_REGISTRY_PROGID, registry);
  }

  return res;
}

extern "C" NS_EXPORT nsresult NSUnregisterSelf(nsISupports* aServMgr, const char * path)
{
  nsresult rv;

  nsCOMPtr<nsIServiceManager> servMgr(do_QueryInterface(aServMgr, &rv));
  if (NS_FAILED(rv)) return rv;

  nsIComponentManager* compMgr;
  rv = servMgr->GetService(kComponentManagerCID, 
                           nsIComponentManager::GetIID(), 
                           (nsISupports**)&compMgr);
  if (NS_FAILED(rv)) return rv;

  for (PRUint32 i=0; i<ARRAY_SIZE(g_FactoryData); i++) {
    rv = compMgr->UnregisterComponent(*(g_FactoryData[i].mCID), path);
    if(NS_FAILED(rv)) goto done;
  }

  done:
  (void)servMgr->ReleaseService(kComponentManagerCID, compMgr);
  return rv;
}

//----------------------------------------------------------------------
// Class nsConverterFactory [implementation]

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

//----------------------------------------------------------------------
// Interface nsISupports [implementation]

NS_IMPL_ADDREF(nsConverterFactory);
NS_IMPL_RELEASE(nsConverterFactory);

nsresult nsConverterFactory::QueryInterface(REFNSIID aIID, 
                                            void** aInstancePtr)
{                                                                        
  if (NULL == aInstancePtr) {                                            
    return NS_ERROR_NULL_POINTER;                                        
  }                                                                      
                                                                         
  *aInstancePtr = NULL;                                                  

  if (aIID.Equals(kICharsetConverterInfoIID)) {
    *aInstancePtr = (void*) ((nsICharsetConverterInfo*)this);
  } else if (aIID.Equals(kIFactoryIID)) {
    *aInstancePtr = (void*) ((nsIFactory*)this);
  } else if (aIID.Equals(kISupportsIID)) {
    *aInstancePtr = (void*) ((nsISupports*)(nsIFactory*)this);
  } else {
    return NS_NOINTERFACE;
  }

  NS_ADDREF_THIS();                                                    
  return NS_OK;                                                        
}

//----------------------------------------------------------------------
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
  
  nsresult res = t->QueryInterface(aIID, aResult);
  if (NS_FAILED(res)) delete t;

  return res;
}

NS_IMETHODIMP nsConverterFactory::LockFactory(PRBool aLock)
{
  if (aLock) PR_AtomicIncrement(&g_LockCount);
  else PR_AtomicDecrement(&g_LockCount);

  return NS_OK;
}

//----------------------------------------------------------------------
// Interface nsICharsetConverterInfo [implementation]

NS_IMETHODIMP nsConverterFactory::GetCharsetSrc(char ** aCharset)
{
  (*aCharset) = mData->mCharsetSrc;
  return NS_OK;
}

NS_IMETHODIMP nsConverterFactory::GetCharsetDest(char ** aCharset)
{
  (*aCharset) = mData->mCharsetDest;
  return NS_OK;
}
