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

#include "nsIServiceManager.h"
#include "nsCookieService.h"
#include "nsCookieHTTPNotify.h"
#include "nsCRT.h"
#include "nsCookie.h"
#include "nsIGenericFactory.h"
#include "nsXPIDLString.h"
#include "nsIScriptGlobalObject.h"
#include "nsIDOMWindowInternal.h"
#include "nsIPrompt.h"

////////////////////////////////////////////////////////////////////////////////


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
    COOKIE_RemoveAllCookies();
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
    
  COOKIE_RegisterCookiePrefCallbacks();
  COOKIE_ReadCookies();
  mInitted = PR_TRUE;
  return NS_OK;
}


NS_IMETHODIMP
nsCookieService::GetCookieString(nsIURI *aURL, nsString& aCookie) {
  nsXPIDLCString spec;
  nsresult rv = aURL->GetSpec(getter_Copies(spec));
  if (NS_FAILED(rv)) return rv;
  char *cookie = COOKIE_GetCookie((char *)(const char *)spec);
  if (nsnull != cookie) {
    aCookie.AssignWithConversion(cookie);
    nsCRT::free(cookie);
  } else {
    // No Cookie isn't an error condition.
    aCookie.Truncate();
  }
  return NS_OK;
}

NS_IMETHODIMP
nsCookieService::GetCookieStringFromHTTP(nsIURI *aURL, nsIURI *aFirstURL, nsString& aCookie) {
  nsXPIDLCString spec;
  nsresult rv = aURL->GetSpec(getter_Copies(spec));
  if (NS_FAILED(rv)) return rv;
  nsXPIDLCString firstSpec;
  rv = aFirstURL->GetSpec(getter_Copies(firstSpec));
  if (NS_FAILED(rv)) return rv;
  char *cookie = COOKIE_GetCookieFromHttp((char *)(const char *)spec, (char *)(const char *)firstSpec);
  if (nsnull != cookie) {
    aCookie.AssignWithConversion(cookie);
    nsCRT::free(cookie);
  } else {
    // No Cookie isn't an error condition.
    aCookie.Truncate();
  }
  return NS_OK;
}

NS_IMETHODIMP
nsCookieService::SetCookieString(nsIURI *aURL, nsIDocument* aDoc, const nsString& aCookie) {
  char *spec = NULL;
  nsresult result = aURL->GetSpec(&spec);
  NS_ASSERTION(result == NS_OK, "deal with this");
  char *cookie = aCookie.ToNewCString();

  nsCOMPtr<nsIScriptGlobalObject> globalObj;
  nsCOMPtr<nsIPrompt> prompt;
  if (aDoc) {
    aDoc->GetScriptGlobalObject(getter_AddRefs(globalObj));
    if (globalObj) {
      nsCOMPtr<nsIDOMWindowInternal> window (do_QueryInterface(globalObj));
      if (window) {
        window->GetPrompter(getter_AddRefs(prompt));
      }
    }
  }

  COOKIE_SetCookieString((char *)spec, prompt, cookie);
  nsCRT::free(spec);
  nsCRT::free(cookie);
  return NS_OK;
}

NS_IMETHODIMP
nsCookieService::SetCookieStringFromHttp(nsIURI *aURL, nsIURI *aFirstURL, nsIPrompt *aPrompter, const char *aCookie, const char *aExpires) {
  char *spec = NULL;
  nsresult rv = aURL->GetSpec(&spec);
  if (NS_FAILED(rv)) return rv;
  char *firstSpec = NULL;
  rv = aFirstURL->GetSpec(&firstSpec);  if (NS_FAILED(rv)) return rv;
  COOKIE_SetCookieStringFromHttp(spec, firstSpec, aPrompter, (char *)aCookie, (char *)aExpires);
  nsCRT::free(spec);
  nsCRT::free(firstSpec);
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

NS_IMETHODIMP nsCookieService::Cookie_GetPermissionListForViewer
    (nsString& aPermissionList, PRInt32 type) {
  ::COOKIE_GetPermissionListForViewer(aPermissionList, type);
  return NS_OK;
}

NS_IMETHODIMP nsCookieService::Image_Block(nsAutoString imageURL) {
  ::Image_Block(imageURL);
  return NS_OK;
}

NS_IMETHODIMP nsCookieService::Permission_Add
    (nsString imageURL, PRBool permission, PRInt32 type) {
  ::Permission_Add(imageURL, permission, type);
  return NS_OK;
}

NS_IMETHODIMP nsCookieService::Image_CheckForPermission
    (char * hostname, char * firstHostname, PRBool &permission) {
  return ::Image_CheckForPermission(hostname, firstHostname, permission);
}


NS_IMETHODIMP nsCookieService::CookieEnabled(PRBool* aEnabled)
{
  *aEnabled = (COOKIE_GetBehaviorPref() != COOKIE_DontUse);
  return NS_OK;
}


//----------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////
// Define the contructor function for the objects

NS_GENERIC_FACTORY_CONSTRUCTOR_INIT(nsCookieService, Init)
NS_GENERIC_FACTORY_CONSTRUCTOR_INIT(nsCookieHTTPNotify, Init)

////////////////////////////////////////////////////////////////////////
// Define a table of CIDs implemented by this module along with other
// information like the function to create an instance, contractid, and
// class name.
//
static nsModuleComponentInfo components[] = {
    { "CookieService", NS_COOKIESERVICE_CID,
      NS_COOKIESERVICE_CONTRACTID, nsCookieServiceConstructor, },	// XXX Singleton
    { NS_COOKIEHTTPNOTIFY_CLASSNAME,
      NS_COOKIEHTTPNOTIFY_CID,
      NS_COOKIEHTTPNOTIFY_CONTRACTID,
      nsCookieHTTPNotifyConstructor,
      nsCookieHTTPNotify::RegisterProc,
      nsCookieHTTPNotify::UnregisterProc
    },
};

////////////////////////////////////////////////////////////////////////
// Implement the NSGetModule() exported function for your module
// and the entire implementation of the module object.
//
NS_IMPL_NSGETMODULE("nsCookieModule", components)
