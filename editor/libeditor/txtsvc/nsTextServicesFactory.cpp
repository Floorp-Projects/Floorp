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

#include "nscore.h"
#include "nsIFactory.h"
#include "nsISupports.h"
#include "nsIComponentManager.h"
#include "nsIServiceManager.h"

#include "nsTextServicesCID.h"
#include "nsTextServicesDocument.h"

static NS_DEFINE_IID(kISupportsIID, NS_ISUPPORTS_IID);
static NS_DEFINE_IID(kIFactoryIID, NS_IFACTORY_IID);
static NS_DEFINE_CID(kComponentManagerCID, NS_COMPONENTMANAGER_CID);

static NS_DEFINE_CID(kCTextServicesDocumentCID, NS_TEXTSERVICESDOCUMENT_CID);

class nsTextServicesDocumentFactory : public nsIFactory
{
  public:   

    nsTextServicesDocumentFactory();   
    virtual ~nsTextServicesDocumentFactory();   

    // nsISupports methods   
    NS_DECL_ISUPPORTS

    // nsIFactory methods   
    NS_IMETHOD CreateInstance(nsISupports *aOuter,   
                              REFNSIID aIID,   
                              void **aResult);   

    NS_IMETHOD LockFactory(PRBool aLock);   
};   

nsTextServicesDocumentFactory::nsTextServicesDocumentFactory()
{
  mRefCnt = 0;
}

nsTextServicesDocumentFactory::~nsTextServicesDocumentFactory()
{
  NS_ASSERTION(mRefCnt == 0, "non-zero refcnt at destruction");   
}

NS_IMPL_ADDREF(nsTextServicesDocumentFactory)
NS_IMPL_RELEASE(nsTextServicesDocumentFactory)

nsresult nsTextServicesDocumentFactory::QueryInterface(REFNSIID aIID,   
                                               void **aInstancePtr)
{
  if (nsnull == aInstancePtr)
    return NS_ERROR_NULL_POINTER;   

  *aInstancePtr = 0;   

  if (aIID.Equals(kISupportsIID)) {
    *aInstancePtr = (void *)(nsISupports*)this;   
  } else if (aIID.Equals(kIFactoryIID)) {
    *aInstancePtr = (void *)(nsIFactory*)this;   
  }

  if (nsnull == *aInstancePtr)
    return NS_NOINTERFACE;   

  NS_ADDREF_THIS();

  return NS_OK;   
}

nsresult nsTextServicesDocumentFactory::CreateInstance(nsISupports *aOuter,  
                                                     REFNSIID aIID,  
                                                     void **aResult)
{
  if (!aResult)
    return NS_ERROR_NULL_POINTER;  

  *aResult = 0;  
  
  nsISupports *inst = new nsTextServicesDocument();

  if (inst == NULL) {
    return NS_ERROR_OUT_OF_MEMORY;  
  }

  nsresult result = inst->QueryInterface(aIID, aResult);

  if (NS_FAILED(result)) {
    // We didn't get the right interface, so clean up  
    delete inst;  
  }

  return result;  
}

nsresult nsTextServicesDocumentFactory::LockFactory(PRBool aLock)
{
  // XXX: Not implemented yet.
  return NS_OK;
}

// return the proper factory to the caller
extern "C" NS_EXPORT nsresult NSGetFactory(nsISupports* serviceMgr,
                                           const nsCID &aClass,
                                           const char *aClassName,
                                           const char *aProgID,
                                           nsIFactory **aFactory)
{
  if (!aFactory)
    return NS_ERROR_NULL_POINTER;

  if (!aClass.Equals(kCTextServicesDocumentCID))
    return NS_NOINTERFACE;

  *aFactory = new nsTextServicesDocumentFactory();

  if (! *aFactory)
    return NS_ERROR_OUT_OF_MEMORY;

  return (*aFactory)->QueryInterface(kIFactoryIID, (void**)aFactory);
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

  rv = compMgr->RegisterComponent(kCTextServicesDocumentCID,
                                  NULL, NULL, path,
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

  rv = compMgr->UnregisterFactory(kCTextServicesDocumentCID, path);

  (void)servMgr->ReleaseService(kComponentManagerCID, compMgr);
  return rv;
}

