/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/*
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

#define NS_IMPL_IDS

#include "nsIFactory.h"
#include "nsIServiceManager.h"
#include "nsICookieService.h"
#include "nsCookieHTTPNotify.h"
#include "nsINetModuleMgr.h" 
#include "nsIEventQueueService.h"
#include "nsCRT.h"
#include "nsCookie.h"

static NS_DEFINE_IID(kISupportsIID, NS_ISUPPORTS_IID); 
static NS_DEFINE_IID(kICookieServiceIID, NS_ICOOKIESERVICE_IID);

static NS_DEFINE_CID(kNetModuleMgrCID, NS_NETMODULEMGR_CID); 
static NS_DEFINE_IID(kEventQueueServiceCID, NS_EVENTQUEUESERVICE_CID);
static NS_DEFINE_IID(kCookieHTTPNotifyCID, NS_COOKIEHTTPNOTIFY_CID);

////////////////////////////////////////////////////////////////////////////////

class nsCookieService : public nsICookieService {
public:

  // nsISupports
  NS_DECL_ISUPPORTS

  // nsICookieService
  static nsresult GetCookieService(nsICookieService** aCookieService);

  NS_IMETHOD GetCookieString(nsIURI *aURL, nsString& aCookie);
  NS_IMETHOD SetCookieString(nsIURI *aURL, const nsString& aCookie);
  NS_IMETHOD Cookie_RemoveAllCookies(void);
  NS_IMETHOD Cookie_CookieViewerReturn(nsAutoString results);
  NS_IMETHOD Cookie_GetCookieListForViewer(nsString& aCookieList);
  NS_IMETHOD Cookie_GetPermissionListForViewer(nsString& aPermissionList);

  nsCookieService();
  virtual ~nsCookieService(void);
  
protected:
    
private:
  nsIHTTPNotify *mCookieHTTPNotify;
  nsresult Init();
};

static nsCookieService* gCookieService = nsnull; // The one-and-only CookieService

////////////////////////////////////////////////////////////////////////////////

class nsCookieServiceFactory : public nsIFactory {
public:

  NS_DECL_ISUPPORTS

  // nsIFactory methods:

  NS_IMETHOD CreateInstance(nsISupports *aOuter, REFNSIID aIID, void **aResult);
  NS_IMETHOD LockFactory(PRBool aLock);

  // nsCookieService methods:

  nsCookieServiceFactory(void);
  virtual ~nsCookieServiceFactory(void);
};

////////////////////////////////////////////////////////////////////////////////
// nsCookieService Implementation

NS_IMPL_ISUPPORTS(nsCookieService, kICookieServiceIID);

NS_EXPORT nsresult NS_NewCookieService(nsICookieService** aCookieService) {
  return nsCookieService::GetCookieService(aCookieService);
}

nsCookieService::nsCookieService() {
  NS_INIT_REFCNT();
  mCookieHTTPNotify = nsnull;
  Init();
}

nsCookieService::~nsCookieService(void) {
  gCookieService = nsnull;
}

nsresult nsCookieService::GetCookieService(nsICookieService** aCookieService) {
  if (! gCookieService) {
    nsCookieService* it = new nsCookieService();
    if (! it) {
      return NS_ERROR_OUT_OF_MEMORY;
    }
    gCookieService = it;
  }
  NS_ADDREF(gCookieService);
  *aCookieService = gCookieService;
  return NS_OK;
}

nsresult nsCookieService::Init() {
  nsresult rv;
  NS_WITH_SERVICE(nsINetModuleMgr, pNetModuleMgr, kNetModuleMgrCID, &rv); 
  if (NS_FAILED(rv)) {
    return rv;
  }
  NS_WITH_SERVICE(nsIEventQueueService, eventQService, kEventQueueServiceCID, &rv); 
  if (NS_SUCCEEDED(rv)) 
  {
    rv = eventQService->CreateThreadEventQueue();
    if (NS_FAILED(rv)) {
      return rv;
    }
  } 
  if (NS_FAILED(rv))
    return rv;
  
  if (NS_FAILED(rv = NS_NewCookieHTTPNotify(&mCookieHTTPNotify))) {
    return rv;
  }
  rv = pNetModuleMgr->RegisterModule(NS_NETWORK_MODULE_MANAGER_HTTP_REQUEST_PROGID, mCookieHTTPNotify);
  if (NS_FAILED(rv)) {
    return rv;
  }
  rv = pNetModuleMgr->RegisterModule(NS_NETWORK_MODULE_MANAGER_HTTP_RESPONSE_PROGID, mCookieHTTPNotify);
  if (NS_FAILED(rv)) {
    return rv;
  }
  COOKIE_RegisterCookiePrefCallbacks();
  COOKIE_ReadCookies();     
  return rv;
}


NS_IMETHODIMP
nsCookieService::GetCookieString(nsIURI *aURL, nsString& aCookie) {
  char *spec = NULL;
  nsresult result = aURL->GetSpec(&spec);
  NS_ASSERTION(result == NS_OK, "deal with this");
  char *cookie = COOKIE_GetCookie((char *)spec);
  if (nsnull != cookie) {
    aCookie.SetString(cookie);
    nsCRT::free(cookie);
  } else {
    aCookie.SetString("");
  }
  nsCRT::free(spec);
  return NS_OK;
}

NS_IMETHODIMP
nsCookieService::SetCookieString(nsIURI *aURL, const nsString& aCookie) {
  char *spec = NULL;
  nsresult result = aURL->GetSpec(&spec);
  NS_ASSERTION(result == NS_OK, "deal with this");
  char *cookie = aCookie.ToNewCString();
  COOKIE_SetCookieString((char *)spec, cookie);
  nsCRT::free(spec);
  nsCRT::free(cookie);
  return NS_OK;
}

NS_IMETHODIMP nsCookieService::Cookie_RemoveAllCookies(void) {
  ::COOKIE_RemoveAllCookies();
  return NS_OK;
}

NS_IMETHODIMP nsCookieService::Cookie_CookieViewerReturn(nsAutoString results) {
  ::COOKIE_CookieViewerReturn(results);
  return NS_OK;
}

NS_IMETHODIMP nsCookieService::Cookie_GetCookieListForViewer(nsString& aCookieList) {
  ::COOKIE_GetCookieListForViewer(aCookieList);
  return NS_OK;
}

NS_IMETHODIMP nsCookieService::Cookie_GetPermissionListForViewer(nsString& aPermissionList) {
  ::COOKIE_GetPermissionListForViewer(aPermissionList);
  return NS_OK;
}

////////////////////////////////////////////////////////////////////////////////
// nsCookieServiceFactory Implementation

static NS_DEFINE_IID(kIFactoryIID, NS_IFACTORY_IID);
NS_IMPL_ISUPPORTS(nsCookieServiceFactory, kIFactoryIID);

nsCookieServiceFactory::nsCookieServiceFactory(void) {
  NS_INIT_REFCNT();
}

nsCookieServiceFactory::~nsCookieServiceFactory(void) {
}

nsresult
nsCookieServiceFactory::CreateInstance(nsISupports *aOuter, REFNSIID aIID, void **aResult) {
  if (! aResult) {
    return NS_ERROR_NULL_POINTER;
  }
  if (aOuter) {
    return NS_ERROR_NO_AGGREGATION;
  }
  *aResult = nsnull;
  nsresult rv;
  nsICookieService* inst = nsnull;
  if (NS_FAILED(rv = NS_NewCookieService(&inst))) {
    return rv;
  }
  if (!inst) {
    return NS_ERROR_OUT_OF_MEMORY;
  }
  rv = inst->QueryInterface(aIID, aResult);
  if (NS_FAILED(rv)) {
    *aResult = NULL;
  }
  return rv;
}

nsresult
nsCookieServiceFactory::LockFactory(PRBool aLock) {
  return NS_OK;
}

////////////////////////////////////////////////////////////////////////////////
// DLL Entry Points:

static NS_DEFINE_IID(kCookieServiceCID, NS_COOKIESERVICE_CID);

extern "C" NS_EXPORT nsresult
NSGetFactory(
    nsISupports* servMgr,
    const nsCID &aClass,
    const char *aClassName,
    const char *aProgID,
    nsIFactory **aFactory) {
  if (! aFactory) {
    return NS_ERROR_NULL_POINTER;
  }
  if (aClass.Equals(kCookieServiceCID)) {
    nsCookieServiceFactory *factory = new nsCookieServiceFactory();
    if (factory == nsnull) {
      return NS_ERROR_OUT_OF_MEMORY;
    }
    NS_ADDREF(factory);
    *aFactory = factory;
    return NS_OK;
  }
  if (aClass.Equals(kCookieHTTPNotifyCID)) {
    nsCookieHTTPNotifyFactory *factory = new nsCookieHTTPNotifyFactory();
    if (factory == nsnull) {
      return NS_ERROR_OUT_OF_MEMORY;
    }
    NS_ADDREF(factory);
    *aFactory = factory;
    return NS_OK;
  }
  return NS_NOINTERFACE;
}

extern "C" NS_EXPORT PRBool
NSCanUnload(nsISupports* serviceMgr) {
  return PR_FALSE;	
}

extern "C" PR_IMPLEMENT(nsresult)
NSRegisterSelf(nsISupports* serviceMgr, const char* aPath) {
  nsresult rv;
  rv = nsComponentManager::RegisterComponent(kCookieServiceCID, "CookieService", NS_COOKIESERVICE_PROGID, aPath,PR_TRUE, PR_TRUE);
  if (NS_FAILED(rv)) {
    return rv;
  }
  rv = nsComponentManager::RegisterComponent(kCookieHTTPNotifyCID, "CookieHTTPNotifyService", "component://netscape/cookie-http-notify", aPath, PR_TRUE, PR_TRUE);
  return rv;
}

extern "C" PR_IMPLEMENT(nsresult)
NSUnregisterSelf(nsISupports* serviceMgr, const char* aPath) {
  nsresult rv;
  rv = nsComponentManager::UnregisterComponent(kCookieServiceCID,  aPath);
  if (NS_FAILED(rv)) {
    return rv;
  }
  rv = nsComponentManager::UnregisterComponent(kCookieHTTPNotifyCID, aPath);
  return rv;
}

////////////////////////////////////////////////////////////////////////////////
