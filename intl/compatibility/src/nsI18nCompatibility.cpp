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
#include "nsIComponentManager.h"
#include "nsCOMPtr.h"
#include "nsIFactory.h"
#include "nsIServiceManager.h"
#include "nsII18nCompatibility.h"
#include "nsI18nCompatibility.h"

static NS_DEFINE_CID(kComponentManagerCID, NS_COMPONENTMANAGER_CID);
static NS_DEFINE_CID(kI18nCompatibilityCID, NS_I18NCOMPATIBILITY_CID);
static NS_DEFINE_IID(kIFactoryIID,  NS_IFACTORY_IID);


///////////////////////////////////////////////////////////////////////////////////////////

class nsI18nCompatibility : public nsII18nCompatibility {
 public: 
  NS_DECL_ISUPPORTS 

  /* wstring CSIDtoCharsetName (in unsigned short csid); */
  NS_IMETHOD  CSIDtoCharsetName(PRUint16 csid, PRUnichar **_retval);

  nsI18nCompatibility() {NS_INIT_REFCNT();}
  virtual ~nsI18nCompatibility() {}
};

NS_IMPL_ISUPPORTS(nsI18nCompatibility, nsII18nCompatibility::GetIID());

NS_IMETHODIMP nsI18nCompatibility::CSIDtoCharsetName(PRUint16 csid, PRUnichar **_retval)
{
  nsString charsetname(I18N_CSIDtoCharsetName(csid));

  *_retval = charsetname.ToNewUnicode();

  return NS_OK;
}

///////////////////////////////////////////////////////////////////////////////////////////

PRInt32 g_InstanceCount = 0;
PRInt32 g_LockCount = 0;

class nsI18nCompatibilityFactory : public nsIFactory {
  NS_DECL_ISUPPORTS
  nsI18nCompatibilityFactory() {
    NS_INIT_REFCNT();
    PR_AtomicIncrement(&g_InstanceCount);
  };
  virtual ~nsI18nCompatibilityFactory() {
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

NS_IMPL_ISUPPORTS(nsI18nCompatibilityFactory, kIFactoryIID);

nsresult nsI18nCompatibilityFactory::CreateInstance(nsISupports *aDelegate,
                                                    const nsIID &aIID,
                                                    void **aResult)
{
  if(NULL == aResult) {
    return NS_ERROR_NULL_POINTER;
  }
  
  *aResult = NULL;

  nsI18nCompatibility *imp = new nsI18nCompatibility();
  
  if(NULL == imp) 
     return NS_ERROR_OUT_OF_MEMORY;

  nsresult rv = imp->QueryInterface(aIID, aResult);
  
  if(NS_FAILED(rv)) {
    delete imp;
  }

  return rv;
}

///////////////////////////////////////////////////////////////////////////////////////////

extern "C" NS_EXPORT nsresult NSGetFactory(nsISupports* aServMgr,
                                           const nsCID &aClass,
                                           const char *aClassName,
                                           const char *aProgID,
                                           nsIFactory **aFactory)
{
  if (aFactory == NULL) {
    return NS_ERROR_NULL_POINTER;
  }
  if (aClass.Equals(kI18nCompatibilityCID)) {
    *aFactory = NULL;
    nsI18nCompatibilityFactory *factory = new nsI18nCompatibilityFactory();
    if(nsnull == factory) 
      return NS_ERROR_OUT_OF_MEMORY;
    nsresult res = factory->QueryInterface(kIFactoryIID, (void **) aFactory);
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

  rv = compMgr->RegisterComponent(kI18nCompatibilityCID, 
                                  "I18n compatibility", 
                                  NS_I18NCOMPATIBILITY_PROGID, 
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
                           nsIComponentManager::GetIID(), 
                           (nsISupports**)&compMgr);
  if (NS_FAILED(rv)) return rv;

  rv = compMgr->UnregisterComponent(kI18nCompatibilityCID, path);

  (void)servMgr->ReleaseService(kComponentManagerCID, compMgr);
  return rv;
}
