/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * The contents of this file are subject to the Netscape Public License
 * Version 1.0 (the "NPL"); you may not use this file except in
 * compliance with the NPL. You may obtain a copy of the NPL at
 * http://www.mozilla.org/NPL/
 *
 * Software distributed under the NPL is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the NPL
 * for the specific language governing rights and limitations under the
 * NPL.
 *
 * The Initial Developer of this code under the NPL is Netscape
 * Communications Corporation. Portions created by Netscape are
 * Copyright (C) 1998 Netscape Communications Corporation. All Rights
 * Reserved.
 */

#include "nsBaseDLL.h"
#include "nscore.h"
#include "nsIProperties.h"
#include "nsProperties.h"
#include "nsIComponentManager.h"
#include "nsIServiceManager.h"
#include "nsCOMPtr.h"
#include "nsIObserverService.h"
#include "nsObserverService.h"
#include "nsIObserver.h"
#include "nsObserver.h"
#include "nsProperties.h"
#include "nsPageMgr.h"
#include "pratom.h"
#include "nsBuffer.h"

static NS_DEFINE_CID(kComponentManagerCID, NS_COMPONENTMANAGER_CID);
static NS_DEFINE_CID(kPageManagerCID, NS_PAGEMANAGER_CID);
static NS_DEFINE_CID(kObserverServiceCID, NS_OBSERVERSERVICE_CID);
static NS_DEFINE_CID(kObserverCID, NS_OBSERVER_CID);
static NS_DEFINE_CID(kBufferCID, NS_BUFFER_CID);

////////////////////////////////////////////////////////////////////////////////

class nsBaseFactory : public nsIFactory
{   
public:
  // nsISupports methods
  NS_DECL_ISUPPORTS 

  nsBaseFactory(const nsCID &aClass); 

  // nsIFactory methods   
  NS_IMETHOD CreateInstance(nsISupports *aOuter, const nsIID &aIID, void **aResult);   
  NS_IMETHOD LockFactory(PRBool aLock);   

protected:
  virtual ~nsBaseFactory();   

  static PRInt32 gLockCount;

  nsCID mClassID;
};

PRInt32 nsBaseFactory::gLockCount = 0;

nsBaseFactory::nsBaseFactory(const nsCID &aClass)
  : mClassID(aClass)
{   
  NS_INIT_REFCNT();
}   

nsBaseFactory::~nsBaseFactory()   
{
  NS_ASSERTION(mRefCnt == 0, "non-zero refcnt at destruction");   
}   

nsresult
nsBaseFactory::QueryInterface(const nsIID &aIID, void **aResult)   
{   
  if (aResult == nsnull)  
    return NS_ERROR_NULL_POINTER;  

  *aResult = nsnull;   

  if (aIID.Equals(nsIFactory::GetIID()) ||
      aIID.Equals(nsISupports::GetIID())) {
    *aResult = (void *)(nsIFactory*)this;   
  }
  if (*aResult == nsnull)
    return NS_NOINTERFACE;

  NS_ADDREF_THIS();
  return NS_OK;   
}   

NS_IMPL_ADDREF(nsBaseFactory)
NS_IMPL_RELEASE(nsBaseFactory)

nsresult
nsBaseFactory::CreateInstance(nsISupports *aOuter, const nsIID &aIID, void **aResult)  
{  
  nsresult rv = NS_OK;

  if (aOuter) {
    return NS_ERROR_NO_AGGREGATION;
  }
  if (aResult == nsnull) {
    return NS_ERROR_NULL_POINTER;
  }

  *aResult = nsnull;  
  
  if (mClassID.Equals(kPersistentPropertiesCID)) {
    nsPersistentProperties* props = new nsPersistentProperties();
    if (props == nsnull)
      return NS_ERROR_OUT_OF_MEMORY;
    rv = props->QueryInterface(aIID, aResult);
    if (NS_FAILED(rv)) {
      delete props;
    }
    return rv;
  }

  if (mClassID.Equals(kObserverServiceCID)) {
    return NS_NewObserverService((nsIObserverService**)aResult);
  }

  if (mClassID.Equals(kObserverCID)) {
    return NS_NewObserver((nsIObserver**)aResult);
  }
#ifdef XP_PC  // XXX for now until i fix the build
  if (mClassID.Equals(kPageManagerCID)) {
    nsPageMgr* pageMgr = new nsPageMgr();
    if (pageMgr == nsnull)
      return NS_ERROR_OUT_OF_MEMORY;
    rv = pageMgr->Init();
    if (NS_FAILED(rv)) {
      delete pageMgr;
      return rv;
    }
    rv = pageMgr->QueryInterface(aIID, aResult);
    if (NS_FAILED(rv)) {
      delete pageMgr;
      return rv;
    }
    return NS_OK;
  }

  if (mClassID.Equals(kBufferCID)) {
    nsBuffer* buffer = new nsBuffer();
    if (buffer == nsnull)
      return NS_ERROR_OUT_OF_MEMORY;
    rv = buffer->QueryInterface(aIID, aResult); 
    if (NS_FAILED(rv)) {
      delete buffer;
      return rv;
    }
    return NS_OK;
  }
#endif
  return NS_NOINTERFACE;
}  

nsresult nsBaseFactory::LockFactory(PRBool aLock)  
{  
  if (aLock) { 
    PR_AtomicIncrement(&gLockCount); 
  } else { 
    PR_AtomicDecrement(&gLockCount); 
  } 

  return NS_OK;
}  

////////////////////////////////////////////////////////////////////////////////

extern "C" NS_EXPORT nsresult
NSRegisterSelf(nsISupports* aServMgr, const char* path)
{
  nsresult rv;

  NS_WITH_SERVICE1(nsIComponentManager, compMgr, 
                   aServMgr, kComponentManagerCID, &rv);
  if (NS_FAILED(rv)) return rv;

  rv = compMgr->RegisterComponent(kPersistentPropertiesCID, 
                                  "Persistent Properties", NULL,
                                  path, PR_TRUE, PR_TRUE);
  if (NS_FAILED(rv)) return rv;

  rv = compMgr->RegisterComponent(kObserverServiceCID,
                                  "ObserverService", 
                                  NS_OBSERVERSERVICE_PROGID,
                                  path, PR_TRUE, PR_TRUE);
  if (NS_FAILED(rv)) return rv;

  rv = compMgr->RegisterComponent(kObserverCID, 
                                  "Observer", 
                                  NS_OBSERVER_PROGID,
                                  path, PR_TRUE, PR_TRUE);
  if (NS_FAILED(rv)) return rv;

  rv = compMgr->RegisterComponent(kPageManagerCID, 
                                  "Page Manager", NULL,
                                  path, PR_TRUE, PR_TRUE);
  if (NS_FAILED(rv)) return rv;

  rv = compMgr->RegisterComponent(kBufferCID, 
                                  "Buffer", NULL,
                                  path, PR_TRUE, PR_TRUE);
  if (NS_FAILED(rv)) return rv;

  return rv;
}

extern "C" NS_EXPORT nsresult
NSUnregisterSelf(nsISupports* aServMgr, const char* path)
{
  nsresult rv;

  NS_WITH_SERVICE1(nsIComponentManager, compMgr, 
                   aServMgr, kComponentManagerCID, &rv);
  if (NS_FAILED(rv)) return rv;

  rv = compMgr->UnregisterComponent(kPersistentPropertiesCID, path);
  if (NS_FAILED(rv)) return rv;

  rv = compMgr->UnregisterComponent(kObserverServiceCID, path);
  if (NS_FAILED(rv)) return rv;

  rv = compMgr->UnregisterComponent(kObserverCID, path);
  if (NS_FAILED(rv)) return rv;

  rv = compMgr->UnregisterComponent(kPageManagerCID, path);
  if (NS_FAILED(rv)) return rv;

  rv = compMgr->UnregisterComponent(kBufferCID, path);
  if (NS_FAILED(rv)) return rv;

  return rv;
}

extern "C" NS_EXPORT nsresult
NSGetFactory(nsISupports* aServMgr, 
             const nsCID& aClass,
             const char *aClassName,
             const char *aProgID,
             nsIFactory** aFactory)
{
  if (!aFactory) {
    return NS_ERROR_NULL_POINTER;
  }
  nsBaseFactory* factory = new nsBaseFactory(aClass);
  if (factory == nsnull)
    return NS_ERROR_OUT_OF_MEMORY;

  NS_ADDREF(factory);
  *aFactory = factory;
  return NS_OK;
}

extern "C" NS_EXPORT PRBool
NSCanUnload(nsISupports* serviceMgr)
{
  return PR_FALSE;      // XXX can we unload this?
}

////////////////////////////////////////////////////////////////////////////////

