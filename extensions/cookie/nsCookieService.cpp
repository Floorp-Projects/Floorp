/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/*
 * The contents of this file are subject to the Netscape Public
 * License Version 1.1 (the "License"); you may not use this file
 * except in compliance with the License. You may obtain a copy of
 * the License at http://www.mozilla.org/NPL/
 *
 * Software distributed under the License is distributed on an "AS
 * IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
 * implied. See the License for the specific language governing
 * rights and limitations under the License.
 *
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is Netscape
 * Communications Corporation.  Portions created by Netscape are
 * Copyright (C) 1998 Netscape Communications Corporation. All
 * Rights Reserved.
 *
 * Contributor(s): 
 */

#define NS_IMPL_IDS

#include "nsCOMPtr.h"
#include "nsIFactory.h"
#include "nsIServiceManager.h"
#include "nsICookieService.h"
#include "nsCookieHTTPNotify.h"
#include "nsIEventQueueService.h"
#include "nsCRT.h"
#include "nsCookie.h"
#include "nsIModule.h"
#include "nsIGenericFactory.h"
#include "nsXPIDLString.h"

static NS_DEFINE_IID(kEventQueueServiceCID, NS_EVENTQUEUESERVICE_CID);

////////////////////////////////////////////////////////////////////////////////

class nsCookieService : public nsICookieService {
public:

  // nsISupports
  NS_DECL_ISUPPORTS

  NS_IMETHOD GetCookieString(nsIURI *aURL, nsString& aCookie);
  NS_IMETHOD SetCookieString(nsIURI *aURL, const nsString& aCookie);
  NS_IMETHOD SetCookieStringFromHttp(nsIURI *aURL, const char *aCookie, const char *aExpires);
  NS_IMETHOD Cookie_RemoveAllCookies(void);
  NS_IMETHOD Cookie_CookieViewerReturn(nsAutoString results);
  NS_IMETHOD Cookie_GetCookieListForViewer(nsString& aCookieList);
  NS_IMETHOD Cookie_GetPermissionListForViewer(nsString& aPermissionList);

  nsCookieService();
  virtual ~nsCookieService(void);
  nsresult Init();
  
protected:
  PRBool   mInitted;
  
private:
  nsCOMPtr<nsIHTTPNotify> mCookieHTTPNotify;
};

////////////////////////////////////////////////////////////////////////////////
// nsCookieService Implementation

NS_IMPL_ISUPPORTS1(nsCookieService, nsICookieService);

nsCookieService::nsCookieService()
: mInitted(PR_FALSE)
{
  NS_INIT_REFCNT();
}

nsCookieService::~nsCookieService(void)
{
}

nsresult nsCookieService::Init()
{
  // make sure we're not initted twice, because this has the serious
  // consequence of reading the cookies file twice
  if (mInitted)
  {
    NS_ASSERTION(0, "Baking the cookies twice. Doesn't that make them biscuits?");
    return NS_ERROR_ALREADY_INITIALIZED;
  }
    
  nsresult rv;
  NS_WITH_SERVICE(nsIEventQueueService, eventQService, kEventQueueServiceCID, &rv); 
  if (NS_FAILED(rv)) return rv;
  rv = eventQService->CreateThreadEventQueue();
  if (NS_FAILED(rv)) return rv;
  
  if (NS_FAILED(rv = NS_NewCookieHTTPNotify(getter_AddRefs(mCookieHTTPNotify)))) {
    return rv;
  }

  COOKIE_RegisterCookiePrefCallbacks();
  COOKIE_ReadCookies();
  mInitted = PR_TRUE;
  return rv;
}


NS_IMETHODIMP
nsCookieService::GetCookieString(nsIURI *aURL, nsString& aCookie) {
  nsXPIDLCString spec;
  nsresult rv = aURL->GetSpec(getter_Copies(spec));
  if (NS_FAILED(rv)) return rv;
  char *cookie = COOKIE_GetCookie((char *)(const char *)spec);
  if (nsnull != cookie) {
    aCookie.SetString(cookie);
    nsCRT::free(cookie);
  } else {
    // No Cookie isn't an error condition.
    aCookie.SetString("");
  }
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

NS_IMETHODIMP
nsCookieService::SetCookieStringFromHttp(nsIURI *aURL, const char *aCookie, const char *aExpires) {
  char *spec = NULL;
  nsresult rv = aURL->GetSpec(&spec);
  if (NS_FAILED(rv)) return rv;
  COOKIE_SetCookieStringFromHttp(spec, (char *)aCookie, (char *)aExpires);
  nsCRT::free(spec);
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

////////////////////////////////////////////////////////////////////////
// Define the contructor function for the objects
//
// NOTE: This creates an instance of objects by using the default constructor
//
// NS_GENERIC_FACTORY_CONSTRUCTOR(nsCookieService)

NS_GENERIC_FACTORY_CONSTRUCTOR_INIT(nsCookieService, Init)

////////////////////////////////////////////////////////////////////////
// Define a table of CIDs implemented by this module along with other
// information like the function to create an instance, progid, and
// class name.
//
static nsModuleComponentInfo components[] = {
    { "CookieService", NS_COOKIESERVICE_CID,
      NS_COOKIESERVICE_PROGID, nsCookieServiceConstructor, },	// XXX Singleton
};

////////////////////////////////////////////////////////////////////////
// Implement the NSGetModule() exported function for your module
// and the entire implementation of the module object.
//
NS_IMPL_NSGETMODULE("nsCookieModule", components)
