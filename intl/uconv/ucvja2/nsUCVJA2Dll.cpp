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

#include "nsRepository.h"
#include "nsEUCJPToUnicode.h"
#include "nsISO2022JPToUnicode.h"
#include "nsUCVJA2CID.h"

// just for NS_IMPL_IDS; this is a good, central place to implement GUIDs
#include "nsIUnicodeDecoder.h"
#include "nsIUnicodeDecodeUtil.h"
#include "nsICharsetConverterManager.h"

//----------------------------------------------------------------------
// Global functions and data [declaration]

extern "C" PRInt32 g_InstanceCount = 0;
extern "C" PRInt32 g_LockCount = 0;

extern "C" PRUint16 g_0201Mapping[] = {
#include "jis0201.ut"
};

extern "C" PRUint16 g_0208Mapping[] = {
#include "jis0208.ut"
};

extern "C" PRUint16 g_0212Mapping[] = {
#include "jis0212.ut"
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

  // the EUCJPToUnicode converter
  if (aCID.Equals(kEUCJPToUnicodeCID)) {
    nsEUCJPToUnicodeFactory *factory = new nsEUCJPToUnicodeFactory();
    nsresult res = factory->QueryInterface(kIFactoryIID, (void **) aFactory);

    if (NS_FAILED(res)) {
      *aFactory = NULL;
      delete factory;
    }

    return res;
  }

  // the ISO2022JPToUnicode converter
  if (aCID.Equals(kISO2022JPToUnicodeCID)) {
    nsISO2022JPToUnicodeFactory *factory = new nsISO2022JPToUnicodeFactory();
    nsresult res = factory->QueryInterface(kIFactoryIID, (void **) aFactory);

    if (NS_FAILED(res)) {
      *aFactory = NULL;
      delete factory;
    }

    return res;
  }

  return NS_NOINTERFACE;
}

extern "C" NS_EXPORT nsresult NSRegisterSelf(const char * path)
{
  nsresult res;

  res = nsRepository::RegisterFactory(kEUCJPToUnicodeCID, path, 
      PR_TRUE, PR_TRUE);
  if(NS_FAILED(res) && (NS_ERROR_FACTORY_EXISTS != res)) return res;

  res = nsRepository::RegisterFactory(kISO2022JPToUnicodeCID, path, 
      PR_TRUE, PR_TRUE);
  if(NS_FAILED(res) && (NS_ERROR_FACTORY_EXISTS != res)) return res;

  return res;
}

extern "C" NS_EXPORT nsresult NSUnregisterSelf(const char * path)
{
  nsresult res;

  res = nsRepository::UnregisterFactory(kEUCJPToUnicodeCID, path);
  if(NS_FAILED(res)) return res;

  res = nsRepository::UnregisterFactory(kISO2022JPToUnicodeCID, path);
  if(NS_FAILED(res)) return res;

  return res;
}
