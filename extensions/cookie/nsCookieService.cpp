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
#include "nsCookies.h"
#include "nsIGenericFactory.h"
#include "nsXPIDLString.h"
#include "nsIScriptGlobalObject.h"
#include "nsIDOMWindowInternal.h"
#include "nsIPrompt.h"
#include "nsIObserverService.h"
#include "nsIDocumentLoader.h"
#include "nsIWebProgress.h"
#include "nsCURILoader.h"

static NS_DEFINE_IID(kDocLoaderServiceCID, NS_DOCUMENTLOADER_SERVICE_CID);

////////////////////////////////////////////////////////////////////////////////


////////////////////////////////////////////////////////////////////////////////
// nsCookieService Implementation

NS_IMPL_ISUPPORTS4(nsCookieService, nsICookieService,
                   nsIObserver, nsIWebProgressListener, nsISupportsWeakReference);

nsCookieService::nsCookieService()
{
  NS_INIT_REFCNT();
}

nsCookieService::~nsCookieService(void)
{
  COOKIE_Write(); /* in case any deleted cookies didn't get removed from file yet */
  COOKIE_RemoveAll();
}

nsresult nsCookieService::Init()
{
  COOKIE_RegisterPrefCallbacks();
  COOKIE_Read();

  nsresult rv;
  nsCOMPtr<nsIObserverService> observerService = 
           do_GetService(NS_OBSERVERSERVICE_CONTRACTID, &rv);
  if (observerService) {
    observerService->AddObserver(this, NS_LITERAL_STRING("profile-before-change").get());
    observerService->AddObserver(this, NS_LITERAL_STRING("profile-do-change").get());
  }

  // Register as an observer for the document loader  
  nsCOMPtr<nsIDocumentLoader> docLoaderService = 
           do_GetService(kDocLoaderServiceCID, &rv);
  if (NS_SUCCEEDED(rv) && docLoaderService) {
    nsCOMPtr<nsIWebProgress> progress(do_QueryInterface(docLoaderService));
    if (progress)
        (void) progress->AddProgressListener((nsIWebProgressListener*)this);
  } else {
    NS_ASSERTION(PR_FALSE, "Could not get nsIDocumentLoader");
  }

  return NS_OK;
}

// nsIWebProgressListener implementation
NS_IMETHODIMP
nsCookieService::OnProgressChange(nsIWebProgress *aProgress,
                                  nsIRequest *aRequest, 
                                  PRInt32 curSelfProgress, 
                                  PRInt32 maxSelfProgress, 
                                  PRInt32 curTotalProgress, 
                                  PRInt32 maxTotalProgress)
{
    return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP 
nsCookieService::OnStateChange(nsIWebProgress* aWebProgress, 
                               nsIRequest *aRequest, 
                               PRInt32 progressStateFlags, 
                               nsresult aStatus)
{
    if (progressStateFlags & STATE_IS_DOCUMENT)
        if (progressStateFlags & STATE_STOP)
            COOKIE_Write();
    return NS_OK;
}


/* void onLocationChange (in nsIURI location); */
NS_IMETHODIMP nsCookieService::OnLocationChange(nsIWebProgress* aWebProgress,
                                                     nsIRequest* aRequest,
                                                     nsIURI *location)
{
    return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP 
nsCookieService::OnStatusChange(nsIWebProgress* aWebProgress,
                                     nsIRequest* aRequest,
                                     nsresult aStatus,
                                     const PRUnichar* aMessage)
{
    return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP 
nsCookieService::OnSecurityChange(nsIWebProgress *aWebProgress, 
                                       nsIRequest *aRequest, 
                                       PRInt32 state)
{
    return NS_ERROR_NOT_IMPLEMENTED;
}


NS_IMETHODIMP
nsCookieService::GetCookieString(nsIURI *aURL, char ** aCookie) {
  nsXPIDLCString spec;
  nsresult rv = aURL->GetSpec(getter_Copies(spec));
  if (NS_FAILED(rv)) return rv;
  *aCookie = COOKIE_GetCookie((char *)(const char *)spec);
  return NS_OK;
}

NS_IMETHODIMP
nsCookieService::GetCookieStringFromHttp(nsIURI *aURL, nsIURI *aFirstURL, char ** aCookie) {
  if (!aURL) {
    return NS_ERROR_FAILURE;
  }
  nsXPIDLCString spec;
  nsresult rv = aURL->GetSpec(getter_Copies(spec));
  if (NS_FAILED(rv)) return rv;
  if (aFirstURL) {
    nsXPIDLCString firstSpec;
    rv = aFirstURL->GetSpec(getter_Copies(firstSpec));
    if (NS_FAILED(rv)) return rv;
    *aCookie = COOKIE_GetCookieFromHttp((char *)(const char *)spec, (char *)(const char *)firstSpec);
  } else {
    *aCookie = COOKIE_GetCookieFromHttp((char *)(const char *)spec, nsnull);
  }
  return NS_OK;
}

NS_IMETHODIMP
nsCookieService::SetCookieString(nsIURI *aURL, nsIPrompt* aPrompt, const char * aCookie) {
  char *spec = NULL;
  nsresult result = aURL->GetSpec(&spec);
  NS_ASSERTION(result == NS_OK, "deal with this");

  COOKIE_SetCookieString(spec, aPrompt, aCookie);
  nsCRT::free(spec);
  return NS_OK;
}

NS_IMETHODIMP
nsCookieService::SetCookieStringFromHttp(nsIURI *aURL, nsIURI *aFirstURL, nsIPrompt *aPrompter, const char *aCookie, const char *aExpires) 
{
  char *spec = NULL;
  nsresult rv = aURL->GetSpec(&spec);
  if (NS_FAILED(rv)) return rv;
  NS_ASSERTION(aFirstURL,"aFirstURL is null");
  if (aFirstURL) {
    char *firstSpec = NULL;
    rv = aFirstURL->GetSpec(&firstSpec);
    if (NS_FAILED(rv)) return rv;
    COOKIE_SetCookieStringFromHttp(spec, firstSpec, aPrompter, (char *)aCookie, (char *)aExpires);
    nsCRT::free(firstSpec);
  }
  nsCRT::free(spec);
  return NS_OK;
}

NS_IMETHODIMP nsCookieService::Observe(nsISupports *aSubject, const PRUnichar *aTopic, const PRUnichar *someData)
{
  nsresult rv = NS_OK;

  if (!nsCRT::strcmp(aTopic, NS_LITERAL_STRING("profile-before-change").get())) {
    // The profile is about to change.
    
    // Dump current cookies.  This will be done by calling 
    // COOKIE_RemoveAll which clears the memory-resident
    // cookie table.  The reason the cookie file does not
    // need to be updated is because the file was updated every time
    // the memory-resident table changed (i.e., whenever a new cookie
    // was accepted).  If this condition ever changes, the cookie
    // file would need to be updated here.

    COOKIE_RemoveAll();
    if (!nsCRT::strcmp(someData, NS_LITERAL_STRING("shutdown-cleanse").get()))
      COOKIE_DeletePersistentUserData();
  }  
  else if (!nsCRT::strcmp(aTopic, NS_LITERAL_STRING("profile-do-change").get())) {
    // The profile has aleady changed.    
    // Now just read them from the new profile location.
    COOKIE_Read();
  }

  return rv;
}
