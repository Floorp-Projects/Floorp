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
#include "nsRepository.h"
#include "nsIFactory.h"
#include "nsICharsetConverterInfo.h"
#include "nsUCVJA2CID.h"
#include "nsEUCJPToUnicode.h"
#include "nsISO2022JPToUnicode.h"

// just for NS_IMPL_IDS; this is a good, central place to implement GUIDs
#include "nsIUnicodeDecoder.h"
#include "nsIUnicodeDecodeUtil.h"
#include "nsICharsetConverterManager.h"

//----------------------------------------------------------------------
// Global functions and data [declaration]

extern "C" PRUint16 g_0201Mapping[] = {
#include "jis0201.ut"
};

extern "C" PRUint16 g_0208Mapping[] = {
#include "jis0208.ut"
};

extern "C" PRUint16 g_0212Mapping[] = {
#include "jis0212.ut"
};

extern "C" PRInt32 g_InstanceCount = 0;
extern "C" PRInt32 g_LockCount = 0;

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
    &kISO2022JPToUnicodeCID,
    nsISO2022JPToUnicode::CreateInstance,
    "ISO-2022-JP",
    "Unicode"
  },
  {
    &kEUCJPToUnicodeCID,
    nsEUCJPToUnicode::CreateInstance,
    "EUC-JP"
    "Unicode"
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

NS_DEFINE_IID(kIFactoryIID, NS_IFACTORY_IID);

extern "C" NS_EXPORT PRBool NSCanUnload()
{
  return PRBool(g_InstanceCount == 0 && g_LockCount == 0);
}

extern "C" NS_EXPORT nsresult NSGetFactory(const nsCID &aCID, 
                                           nsISupports* serviceMgr,
                                           nsIFactory **aFactory)
{
  if (aFactory == NULL) return NS_ERROR_NULL_POINTER;

  nsresult res;
  nsConverterFactory * fac;
  FactoryData * data;

  for (PRInt32 i=0; i<ARRAY_SIZE(g_FactoryData); i++) {
    data = &(g_FactoryData[i]);
    if (aCID.Equals(*(data->mCID))) {
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

extern "C" NS_EXPORT nsresult NSRegisterSelf(const char * path)
{
  nsresult res;

  for (PRInt32 i=0; i<ARRAY_SIZE(g_FactoryData); i++) {
    res = nsRepository::RegisterFactory(*(g_FactoryData[i].mCID), path, 
      PR_TRUE, PR_TRUE);
    if(NS_FAILED(res) && (NS_ERROR_FACTORY_EXISTS != res)) return res;
  }

  return res;
}

extern "C" NS_EXPORT nsresult NSUnregisterSelf(const char * path)
{
  nsresult res;

  for (PRInt32 i=0; i<ARRAY_SIZE(g_FactoryData); i++) {
    res = nsRepository::UnregisterFactory(*(g_FactoryData[i].mCID), path);
    if(NS_FAILED(res)) return res;
  }

  return res;
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
                                                                         
  static NS_DEFINE_IID(kISupportsIID, NS_ISUPPORTS_IID);                 
  static NS_DEFINE_IID(kClassIID, kICharsetConverterInfoIID);                         
  static NS_DEFINE_IID(kIFactoryIID, NS_IFACTORY_IID);

  if (aIID.Equals(kClassIID)) {                                          
    *aInstancePtr = (void*) ((nsICharsetConverterInfo*)this); 
    NS_ADDREF_THIS();                                                    
    return NS_OK;                                                        
  }                                                                      
  if (aIID.Equals(kIFactoryIID)) {                                          
    *aInstancePtr = (void*) ((nsIFactory*)this); 
    NS_ADDREF_THIS();                                                    
    return NS_OK;                                                        
  }                                                                      
  if (aIID.Equals(kISupportsIID)) {                                      
    *aInstancePtr = (void*) ((nsISupports*)(nsIFactory*)this);
    NS_ADDREF_THIS();                                                    
    return NS_OK;                                                        
  }                                                                      

  return NS_NOINTERFACE;                                                 
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
