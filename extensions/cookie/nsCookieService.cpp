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

#include "nsCOMPtr.h"
#include "nsIFactory.h"
#include "nsIServiceManager.h"
#include "nsICookieService.h"
#include "nsCookieHTTPNotify.h"
#include "nsINetModuleMgr.h" 
#include "nsIEventQueueService.h"
#include "nsCRT.h"
#include "nsCookie.h"
#include "nsIModule.h"
#include "nsIGenericFactory.h"

static NS_DEFINE_IID(kISupportsIID, NS_ISUPPORTS_IID); 
static NS_DEFINE_IID(kICookieServiceIID, NS_ICOOKIESERVICE_IID);

static NS_DEFINE_CID(kNetModuleMgrCID, NS_NETMODULEMGR_CID); 
static NS_DEFINE_IID(kEventQueueServiceCID, NS_EVENTQUEUESERVICE_CID);
static NS_DEFINE_IID(kCookieServiceCID, NS_COOKIESERVICE_CID);
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
// Module implementation
class nsCookieServiceModule : public nsIModule
{
public:
    nsCookieServiceModule();
    virtual ~nsCookieServiceModule();

    NS_DECL_ISUPPORTS

    NS_DECL_NSIMODULE

protected:
    nsresult Initialize();

    void Shutdown();

    PRBool mInitialized;
    nsCOMPtr<nsIGenericFactory> mCookieServiceFactory;
    nsCOMPtr<nsIGenericFactory> mCookieHTTPNotifyFactory;
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


//----------------------------------------------------------------------

// Functions used to create new instances of a given object by the
// generic factory.

static NS_IMETHODIMP    
CreateNewCookieService(nsISupports* aOuter, REFNSIID aIID, void **aResult)
{                                                                    
    if (!aResult) {                                              
        return NS_ERROR_INVALID_POINTER; 
    }                            
    if (aOuter) {          
        *aResult = nsnull;
        return NS_ERROR_NO_AGGREGATION;
    }                                 
    nsICookieService* inst = nsnull;                   
    nsresult rv = NS_NewCookieService(&inst); 
    if (NS_FAILED(rv)) {       
        *aResult = nsnull;     
        return rv;          
    } 
    if(!inst) {
      return NS_ERROR_OUT_OF_MEMORY;
    }
    rv = inst->QueryInterface(aIID, aResult); 
    if (NS_FAILED(rv)) { 
        *aResult = nsnull; 
    }                      
    NS_RELEASE(inst);             /* get rid of extra refcnt */ 
    return rv;                
}


//----------------------------------------------------------------------


nsCookieServiceModule::nsCookieServiceModule()
    : mInitialized(PR_FALSE)
{
    NS_INIT_ISUPPORTS();
}

nsCookieServiceModule::~nsCookieServiceModule()
{
    Shutdown();
}

NS_IMPL_ISUPPORTS(nsCookieServiceModule, NS_GET_IID(nsIModule))

// Perform our one-time intialization for this module
nsresult
nsCookieServiceModule::Initialize()
{
    if (mInitialized) {
        return NS_OK;
    }
    mInitialized = PR_TRUE;
    return NS_OK;
}

// Shutdown this module, releasing all of the module resources
void
nsCookieServiceModule::Shutdown()
{
    // Release the factory objects
    mCookieServiceFactory = nsnull;
    mCookieHTTPNotifyFactory = nsnull;
}

// Create a factory object for creating instances of aClass.
NS_IMETHODIMP
nsCookieServiceModule::GetClassObject(nsIComponentManager *aCompMgr,
                               const nsCID& aClass,
                               const nsIID& aIID,
                               void** r_classObj)
{
    nsresult rv;

    // Defensive programming: Initialize *r_classObj in case of error below
    if (!r_classObj) {
        return NS_ERROR_INVALID_POINTER;
    }
    *r_classObj = NULL;

    // Do one-time-only initialization if necessary
    if (!mInitialized) {
        rv = Initialize();
        if (NS_FAILED(rv)) {
            // Initialization failed! yikes!
            return rv;
        }
    }

    // Choose the appropriate factory, based on the desired instance
    // class type (aClass).
    nsCOMPtr<nsIGenericFactory> fact;
    if (aClass.Equals(kCookieServiceCID)) {
        if (!mCookieServiceFactory) {
            // Create and save away the factory object for creating
            // new instances of CookieService. This way if we are called
            // again for the factory, we won't need to create a new
            // one.
            rv = NS_NewGenericFactory(getter_AddRefs(mCookieServiceFactory),
                                      CreateNewCookieService);
        }
        fact = mCookieServiceFactory;
    }
    else if (aClass.Equals(kCookieHTTPNotifyCID)) {
        if (!mCookieHTTPNotifyFactory) {
            // Create and save away the factory object for creating
            // new instances of CookieHTTPNotify. This way if we are called
            // again for the factory, we won't need to create a new
            // one.
            rv = NS_NewGenericFactory(getter_AddRefs(mCookieHTTPNotifyFactory),
                                      nsCookieHTTPNotify::Create);
        }
        fact = mCookieHTTPNotifyFactory;
    }
    else {
		    rv = NS_ERROR_FACTORY_NOT_REGISTERED;
#ifdef DEBUG
        char* cs = aClass.ToString();
        printf("+++ nsCookieServiceModule: unable to create factory for %s\n", cs);
        nsCRT::free(cs);
#endif
    }

    if (fact) {
        rv = fact->QueryInterface(aIID, r_classObj);
    }

    return rv;
}

//----------------------------------------

struct Components {
    const char* mDescription;
    const nsID* mCID;
    const char* mProgID;
};

// The list of components we register
static Components gComponents[] = {
    { "CookieService", &kCookieServiceCID,
      "component://netscape/cookie", },
    { "CookieHTTPNotifyService", &kCookieHTTPNotifyCID,
      "component://netscape/cookie-http-notify", },
};
#define NUM_COMPONENTS (sizeof(gComponents) / sizeof(gComponents[0]))


NS_IMETHODIMP
nsCookieServiceModule::RegisterSelf(nsIComponentManager *aCompMgr,
                                    nsIFileSpec* aPath,
                                    const char* registryLocation,
                                    const char* componentType)
{
    nsresult rv = NS_OK;

#ifdef DEBUG
    printf("*** Registering CookieService components\n");
#endif

    Components* cp = gComponents;
    Components* end = cp + NUM_COMPONENTS;
    while (cp < end) {
        rv = aCompMgr->RegisterComponentSpec(*cp->mCID, cp->mDescription,
                                             cp->mProgID, aPath, PR_TRUE,
                                             PR_TRUE);
        if (NS_FAILED(rv)) {
#ifdef DEBUG
            printf("nsCookieServiceModule: unable to register %s component => %x\n",
                   cp->mDescription, rv);
#endif
            break;
        }
        cp++;
    }

    return rv;
}

NS_IMETHODIMP
nsCookieServiceModule::UnregisterSelf(nsIComponentManager* aCompMgr,
                                      nsIFileSpec* aPath,
                                      const char* registryLocation)
{
#ifdef DEBUG
    printf("*** Unregistering CookieService components\n");
#endif
    Components* cp = gComponents;
    Components* end = cp + NUM_COMPONENTS;
    while (cp < end) {
        nsresult rv = aCompMgr->UnregisterComponentSpec(*cp->mCID, aPath);
        if (NS_FAILED(rv)) {
#ifdef DEBUG
            printf("nsCookieServiceModule: unable to unregister %s component => %x\n",
                   cp->mDescription, rv);
#endif
        }
        cp++;
    }

    return NS_OK;
}

NS_IMETHODIMP
nsCookieServiceModule::CanUnload(nsIComponentManager *aCompMgr, PRBool *okToUnload)
{
    if (!okToUnload) {
        return NS_ERROR_INVALID_POINTER;
    }
    *okToUnload = PR_FALSE;
    return NS_ERROR_FAILURE;
}

//----------------------------------------------------------------------

static nsCookieServiceModule *gModule = NULL;

extern "C" NS_EXPORT nsresult NSGetModule(nsIComponentManager *servMgr,
                                          nsIFileSpec* location,
                                          nsIModule** return_cobj)
{
    nsresult rv = NS_OK;

    NS_ENSURE_ARG_POINTER(return_cobj);
    NS_ENSURE_NOT(gModule, NS_ERROR_FAILURE);

    // Create and initialize the module instance
    nsCookieServiceModule *m = new nsCookieServiceModule();
    if (!m) {
        return NS_ERROR_OUT_OF_MEMORY;
    }

    // Increase refcnt and store away nsIModule interface to m in return_cobj
    rv = m->QueryInterface(NS_GET_IID(nsIModule), (void**)return_cobj);
    if (NS_FAILED(rv)) {
        delete m;
        m = nsnull;
    }
    gModule = m;                  // WARNING: Weak Reference
    return rv;
}


////////////////////////////////////////////////////////////////////////////////
