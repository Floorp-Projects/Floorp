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

#include "pratom.h"
#include "nsISupports.h"
#include "nsRepository.h"
#include "nsIFactory.h"
#include "nsUnicharUtilCIID.h"
#include "nsICaseConversion.h"
#include "nsCaseConversionImp2.h"


NS_DEFINE_IID(kFactoryIID, NS_IFACTORY_IID);
NS_DEFINE_CID(kUnicharUtilCID, NS_UNICHARUTIL_CID);
NS_DEFINE_IID(kICaseConversionIID, NS_ICASECONVERSION_IID);

static PRInt32 g_FactoryCount = 0;
static PRInt32 g_LockCount = 0;

class nsUnicharUtilFactory : public nsIFactory {
  NS_DECL_ISUPPORTS
  nsUnicharUtilFactory() {
    NS_INIT_REFCNT();
    PR_AtomicIncrement(&g_FactoryCount);
  };
  ~nsUnicharUtilFactory() {
    PR_AtomicDecrement(&g_FactoryCount);
  };

  NS_IMETHOD CreateInstance(nsISupports *aDelegate,
                            const nsIID &aIID,
                            void **aResult);

  NS_IMETHOD LockFactory(PRBool aLock) {
    if (aLock) {
      PR_AtomicIncrement(&g_LockCount);
    } else {
      PR_AtomicDecrement(&g_LockCount);
    }
    return NS_OK;
  };
};

NS_IMPL_ISUPPORTS(nsUnicharUtilFactory, kFactoryIID);

nsresult nsUnicharUtilFactory::CreateInstance(nsISupports *aDelegate,
                                            const nsIID &aIID,
                                            void **aResult)
{
  if(NULL == aResult ) {
    return NS_ERROR_NULL_POINTER;
  }
  
  *aResult = NULL;
   
  nsISupports *inst = new nsCaseConversionImp2();
  if(NULL == inst) {
    return NS_ERROR_OUT_OF_MEMORY;  
  }
  
  nsresult res = inst->QueryInterface(aIID, aResult);
  
  if(NS_FAILED(res)) {
    delete inst;
  }
  return res;
}

extern "C" NS_EXPORT nsresult NSGetFactory(const nsCID &aCID, nsISupports* serviceMgr,
                                           nsIFactory **aFactory)
{
  if (aFactory == NULL) {
    return NS_ERROR_NULL_POINTER;
  }
  if (aCID.Equals(kUnicharUtilCID)) {
    nsUnicharUtilFactory *factory = new nsUnicharUtilFactory();
    nsresult res = factory->QueryInterface(kFactoryIID, (void **) aFactory);
    if (NS_FAILED(res)) {
      *aFactory = NULL;
      delete factory;
    }
    return res;
  }
  return NS_NOINTERFACE;
}

extern "C" NS_EXPORT PRBool NSCanUnload() {
  return PRBool(g_FactoryCount == 0 && g_LockCount == 0);
}

// somehow  UNIX have problem to link against nsRepository::RegisterFactory
// temporary turn it off untill XPCOM folks fix it
#ifndef XP_UNIX 
extern "C" NS_EXPORT nsresult NSRegisterSelf(const char *path)
{
  return nsRepository::RegisterFactory(kUnicharUtilCID, path,
                                       PR_TRUE, PR_TRUE);
}

extern "C" NS_EXPORT nsresult NSUnregisterSelf(const char *path)
{
  return nsRepository::UnregisterFactory(kUnicharUtilCID, path);
}
#endif
