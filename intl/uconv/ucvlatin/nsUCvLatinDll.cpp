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
#include "nsIUnicodeEncoder.h"
#include "nsIUnicodeDecoder.h"
#include "nsLatin1ToUnicode.h"
#include "nsISO88597ToUnicode.h"
#include "nsCP1253ToUnicode.h"
#include "nsConverterCID.h"
#include "nsUCvLatinCID.h"
// just for NS_IMPL_IDS
#include "nsIUnicodeDecodeUtil.h"
#include "nsICharsetConverterManager.h"

//----------------------------------------------------------------------
// Global functions and data [declaration]

extern "C" PRInt32 g_InstanceCount = 0;
extern "C" PRInt32 g_LockCount = 0;

//----------------------------------------------------------------------
// Global functions and data [implementation]

NS_DEFINE_IID(kIFactoryIID, NS_IFACTORY_IID);

extern "C" NS_EXPORT PRBool NSCanUnload()
{
  return PRBool(g_InstanceCount == 0 && g_LockCount == 0);
}

extern "C" NS_EXPORT nsresult NSGetFactory(const nsCID &aCID, nsISupports* serviceMgr,
                                           nsIFactory **aFactory)
{
  if (aFactory == NULL) return NS_ERROR_NULL_POINTER;

  // the Latin1ToUnicode converter
  if (aCID.Equals(kLatin1ToUnicodeCID)) {
    nsLatin1ToUnicodeFactory *factory = new nsLatin1ToUnicodeFactory();
    nsresult res = factory->QueryInterface(kIFactoryIID, (void **) aFactory);

    if (NS_FAILED(res)) {
      *aFactory = NULL;
      delete factory;
    }

    return res;
  }
  // the ISO88597ToUnicode converter
  if (aCID.Equals(kISO88597ToUnicodeCID)) {
    nsISO88597ToUnicodeFactory *factory = new nsISO88597ToUnicodeFactory();
    nsresult res = factory->QueryInterface(kIFactoryIID, (void **) aFactory);

    if (NS_FAILED(res)) {
      *aFactory = NULL;
      delete factory;
    }

    return res;
  }

  // the CP1253ToUnicode converter
  if (aCID.Equals(kCP1253ToUnicodeCID)) {
    nsCP1253ToUnicodeFactory *factory = new nsCP1253ToUnicodeFactory();
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

  res = nsRepository::RegisterFactory(kLatin1ToUnicodeCID, path, 
      PR_TRUE, PR_TRUE);
  if(NS_FAILED(res) && (NS_ERROR_FACTORY_EXISTS != res)) return res;

  res = nsRepository::RegisterFactory(kCP1253ToUnicodeCID, path, 
      PR_TRUE, PR_TRUE);

  if(NS_FAILED(res) && (NS_ERROR_FACTORY_EXISTS != res)) return res;
  res = nsRepository::RegisterFactory(kISO88597ToUnicodeCID, path, 
      PR_TRUE, PR_TRUE);

  return res;
}

extern "C" NS_EXPORT nsresult NSUnregisterSelf(const char * path)
{
  nsresult res;

  res = nsRepository::UnregisterFactory(kLatin1ToUnicodeCID, path);
  if(NS_FAILED(res)) return res;

  res = nsRepository::UnregisterFactory(kCP1253ToUnicodeCID, path);
  if(NS_FAILED(res)) return res;

  res = nsRepository::UnregisterFactory(kISO88597ToUnicodeCID, path);
  return res;
}
