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
#include "nsUUDll.h"
#include "nsISupports.h"
#include "nsIComponentManager.h"
#include "nsIFactory.h"
#include "nsUnicharUtilCIID.h"
#include "nsHankakuToZenkakuCID.h"
#include "nsTextTransformFactory.h"
#include "nsICaseConversion.h"
#include "nsCaseConversionImp2.h"
#include "nsIServiceManager.h"
#include "nsCOMPtr.h"

static NS_DEFINE_CID(kComponentManagerCID, NS_COMPONENTMANAGER_CID);

NS_DEFINE_IID(kFactoryIID, NS_IFACTORY_IID);
NS_DEFINE_CID(kUnicharUtilCID, NS_UNICHARUTIL_CID);
NS_DEFINE_IID(kHankakuToZenkakuCID, NS_HANKAKUTOZENKAKU_CID);
NS_DEFINE_IID(kICaseConversionIID, NS_ICASECONVERSION_IID);

PRInt32 g_InstanceCount = 0;
PRInt32 g_LockCount = 0;

class nsUnicharUtilFactory : public nsIFactory {
  NS_DECL_ISUPPORTS
  nsUnicharUtilFactory() {
    NS_INIT_REFCNT();
    PR_AtomicIncrement(&g_InstanceCount);
  };
  virtual ~nsUnicharUtilFactory() {
    PR_AtomicDecrement(&g_InstanceCount);
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

extern "C" NS_EXPORT nsresult NSGetFactory(nsISupports* aServMgr,
                                           const nsCID &aClass,
                                           const char *aClassName,
                                           const char *aProgID,
                                           nsIFactory **aFactory)
{
  if (aFactory == NULL) {
    return NS_ERROR_NULL_POINTER;
  }
  *aFactory = NULL;
  if (aClass.Equals(kUnicharUtilCID)) {
    nsUnicharUtilFactory *factory = new nsUnicharUtilFactory();
    if(nsnull == factory)
      return NS_ERROR_OUT_OF_MEMORY;
    nsresult res = factory->QueryInterface(kFactoryIID, (void **) aFactory);
    if (NS_FAILED(res)) {
      *aFactory = NULL;
      delete factory;
    }
    return res;
  }
  if (aClass.Equals(kHankakuToZenkakuCID)) {
    nsIFactory *factory = NEW_HANKAKU_TO_ZENKAKU_FACTORY();
    if(nsnull == factory)
      return NS_ERROR_OUT_OF_MEMORY;
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

  rv = compMgr->RegisterComponent(kUnicharUtilCID, 
                                  "Unichar Utility", 
                                  NS_UNICHARUTIL_PROGID, 
                                  path,
                                  PR_TRUE, PR_TRUE);

  if(NS_FAILED(rv) && (NS_ERROR_FACTORY_EXISTS != rv))  goto done;

  rv = compMgr->RegisterComponent(kHankakuToZenkakuCID, 
                                  "Japanese Hakaku To Zenkaku", 
                                  NS_HANKAKUTOZENKAKU_PROGID, 
                                  path,
                                  PR_TRUE, PR_TRUE);

done:
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

  rv = compMgr->UnregisterComponent(kUnicharUtilCID, path);

  if (NS_FAILED(rv)) goto done;

  rv = compMgr->UnregisterComponent(kHankakuToZenkakuCID, path);

done:
  (void)servMgr->ReleaseService(kComponentManagerCID, compMgr);
  return rv;
}
