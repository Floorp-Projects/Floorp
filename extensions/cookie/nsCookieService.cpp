/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* ***** BEGIN LICENSE BLOCK *****
 * Version: NPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Netscape Public License
 * Version 1.1 (the "License"); you may not use this file except in
 * compliance with the License. You may obtain a copy of the License at
 * http://www.mozilla.org/NPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is 
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 1998
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the NPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the NPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

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
#include "nsIObserver.h"
#include "nsIDocumentLoader.h"
#include "nsIWebProgress.h"
#include "nsCURILoader.h"
#include "nsNetCID.h"
#include "nsAppDirectoryServiceDefs.h"
#include "nsIHttpChannel.h"
#include "nsIHttpChannelInternal.h"

static NS_DEFINE_IID(kDocLoaderServiceCID, NS_DOCUMENTLOADER_SERVICE_CID);

static const PRUint32 kLazyWriteLoadingTimeout = 10000; //msec
static const PRUint32 kLazyWriteFinishedTimeout = 1000; //msec
// sadly, we need this multiple definition until everything is in one file
static const char kCookieFileName2[] = "cookies.txt";

////////////////////////////////////////////////////////////////////////////////
// nsCookieService Implementation

NS_IMPL_ISUPPORTS4(nsCookieService, nsICookieService,
                   nsIObserver, nsIWebProgressListener, nsISupportsWeakReference);

PRBool gCookieIconVisible = PR_FALSE;

nsCookieService::nsCookieService() :
  mCookieFile(nsnull),
  mObserverService(nsnull),
  mLoadCount(0),
  mWritePending(PR_FALSE)
{
  gCookiePrefObserver = nsnull;
  sCookieList = nsnull;
}

nsCookieService::~nsCookieService()
{
  if (mWriteTimer)
    mWriteTimer->Cancel();

  // clean up memory
  COOKIE_RemoveAll();

  NS_IF_RELEASE(gCookiePrefObserver);
  if (sCookieList) {
    delete sCookieList;
  }
}

nsresult nsCookieService::Init()
{
  nsresult rv;

  // install the main cookie pref observer (defined in nsCookieService.h)
  // this will be integrated into nsCookieService when nsCookies is removed.
  gCookiePrefObserver = new nsCookiePrefObserver();
  // create sCookieList
  sCookieList = new nsVoidArray();
  // if we can't create them, return an error so do_GetService() fails
  if (!gCookiePrefObserver || !sCookieList) {
    return NS_ERROR_OUT_OF_MEMORY;
  }

  // init nsCookiePrefObserver, and fail if it can't get the prefbranch
  NS_ADDREF(gCookiePrefObserver);
  rv = gCookiePrefObserver->Init();
  if (NS_FAILED(rv)) return rv;

  // cache mCookieFile
  rv = NS_GetSpecialDirectory(NS_APP_USER_PROFILE_50_DIR, getter_AddRefs(mCookieFile));
  if (NS_SUCCEEDED(rv)) {
    rv = mCookieFile->AppendNative(NS_LITERAL_CSTRING(kCookieFileName2));
  }

  COOKIE_Read();

  mObserverService = do_GetService("@mozilla.org/observer-service;1", &rv);
  if (mObserverService) {
    mObserverService->AddObserver(this, "profile-before-change", PR_TRUE);
    mObserverService->AddObserver(this, "profile-do-change", PR_TRUE);
    mObserverService->AddObserver(this, "cookieIcon", PR_TRUE);
  }

  // Register as an observer for the document loader  
  nsCOMPtr<nsIDocumentLoader> docLoaderService = do_GetService(kDocLoaderServiceCID, &rv);
  if (NS_SUCCEEDED(rv) && docLoaderService) {
    nsCOMPtr<nsIWebProgress> progress(do_QueryInterface(docLoaderService));
    if (progress)
        (void) progress->AddProgressListener((nsIWebProgressListener*)this,
                                             nsIWebProgress::NOTIFY_STATE_DOCUMENT |
                                             nsIWebProgress::NOTIFY_STATE_NETWORK);
  } else {
    NS_ERROR("Couldn't get nsIDocumentLoader");
  }

  return NS_OK;
}

void
nsCookieService::LazyWrite(PRBool aForce)
{
  // !aForce resets the timer at load end, but only if a write is pending
  if (!aForce && !mWritePending)
    return;

  PRUint32 timeout = mLoadCount > 0 ? kLazyWriteLoadingTimeout :
                                      kLazyWriteFinishedTimeout;
  if (mWriteTimer) {
    mWriteTimer->SetDelay(timeout);
    mWritePending = PR_TRUE;
  } else {
    mWriteTimer = do_CreateInstance("@mozilla.org/timer;1");
    if (mWriteTimer) {
      mWriteTimer->InitWithFuncCallback(DoLazyWrite, this, timeout,
                                        nsITimer::TYPE_ONE_SHOT);
      mWritePending = PR_TRUE;
    }
  }
}

void
nsCookieService::DoLazyWrite(nsITimer *aTimer, void *aClosure)
{
  nsCookieService *service = NS_REINTERPRET_CAST(nsCookieService *, aClosure);
  service->mWritePending = PR_FALSE;
  COOKIE_Write();
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
  NS_NOTREACHED("notification excluded in AddProgressListener(...)");
  return NS_OK;
}

NS_IMETHODIMP 
nsCookieService::OnStateChange(nsIWebProgress* aWebProgress, 
                               nsIRequest *aRequest, 
                               PRUint32 progressStateFlags, 
                               nsresult aStatus)
{
  if (progressStateFlags & STATE_IS_NETWORK) {
    if (progressStateFlags & STATE_START)
      ++mLoadCount;
    if (progressStateFlags & STATE_STOP) {
      if (mLoadCount > 0) // needed because at startup we may miss initial STATE_START
        --mLoadCount;
      if (mLoadCount == 0)
        LazyWrite(PR_FALSE);
    }
  }

  if (mObserverService &&
      (progressStateFlags & STATE_IS_DOCUMENT) &&
      (progressStateFlags & STATE_STOP)) {
    mObserverService->NotifyObservers(nsnull, "cookieChanged", NS_LITERAL_STRING("cookies").get());
  }

  return NS_OK;
}


/* void onLocationChange (in nsIURI location); */
NS_IMETHODIMP nsCookieService::OnLocationChange(nsIWebProgress* aWebProgress,
                                                     nsIRequest* aRequest,
                                                     nsIURI *location)
{
  NS_NOTREACHED("notification excluded in AddProgressListener(...)");
  return NS_OK;
}

NS_IMETHODIMP 
nsCookieService::OnStatusChange(nsIWebProgress* aWebProgress,
                                nsIRequest* aRequest,
                                nsresult aStatus,
                                const PRUnichar* aMessage)
{
  NS_NOTREACHED("notification excluded in AddProgressListener(...)");
  return NS_OK;
}

NS_IMETHODIMP 
nsCookieService::OnSecurityChange(nsIWebProgress *aWebProgress, 
                                  nsIRequest *aRequest, 
                                  PRUint32 state)
{
  NS_NOTREACHED("notification excluded in AddProgressListener(...)");
  return NS_OK;
}

NS_IMETHODIMP
nsCookieService::GetCookieString(nsIURI *aHostURI, char ** aCookie) {
  *aCookie = COOKIE_GetCookie(aHostURI, nsnull);
  return NS_OK;
}

NS_IMETHODIMP
nsCookieService::GetCookieStringFromHttp(nsIURI *aHostURI, nsIURI *aFirstURI, char ** aCookie) {
  *aCookie = COOKIE_GetCookie(aHostURI, aFirstURI);
  return NS_OK;
}

NS_IMETHODIMP
nsCookieService::SetCookieString(nsIURI *aHostURI, nsIPrompt *aPrompt, const char *aCookieHeader, nsIHttpChannel *aHttpChannel)
{
  nsCOMPtr<nsIURI> firstURI;

  if (aHttpChannel) {
    nsCOMPtr<nsIHttpChannelInternal> httpInternal = do_QueryInterface(aHttpChannel);
    if (!httpInternal ||
        NS_FAILED(httpInternal->GetDocumentURI(getter_AddRefs(firstURI)))) {
      COOKIE_LOGFAILURE(SET_COOKIE, aHostURI, aCookieHeader, "unable to determine first URI");
      return NS_OK;
    }
  }

  COOKIE_SetCookie(aHostURI, firstURI, aPrompt, aCookieHeader, nsnull, aHttpChannel);
  LazyWrite(PR_TRUE);
  return NS_OK;
}

NS_IMETHODIMP
nsCookieService::SetCookieStringFromHttp(nsIURI *aHostURI, nsIURI *aFirstURI, nsIPrompt *aPrompt, const char *aCookieHeader, const char *aServerTime, nsIHttpChannel* aHttpChannel) 
{
  nsCOMPtr<nsIURI> firstURI = aFirstURI;
  if (!firstURI) {
    firstURI = aHostURI;
  }

  COOKIE_SetCookie(aHostURI, firstURI, aPrompt, aCookieHeader, aServerTime, aHttpChannel);
  LazyWrite(PR_TRUE);
  return NS_OK;
}

NS_IMETHODIMP
nsCookieService::GetCookieIconIsVisible(PRBool *aIsVisible) {
  *aIsVisible = gCookieIconVisible;
  return NS_OK;
}

NS_IMETHODIMP nsCookieService::Observe(nsISupports *aSubject, const char *aTopic, const PRUnichar *aData)
{
  if (!nsCRT::strcmp(aTopic, "profile-before-change")) {
    // The profile is about to change,
    // or is going away because the application is shutting down.
    if (mWriteTimer)
      mWriteTimer->Cancel();

    if (!nsCRT::strcmp(aData, NS_LITERAL_STRING("shutdown-cleanse").get())) {
      COOKIE_RemoveAll();
      // delete the cookie file
      if (mCookieFile) {
        mCookieFile->Remove(PR_FALSE);
      }
    } else {
      COOKIE_Write();
      COOKIE_RemoveAll();
    }
  } else if (!nsCRT::strcmp(aTopic, "profile-do-change")) {
    // The profile has already changed.    
    // Now just read them from the new profile location.
    // we also need to update the cached cookie file location
    nsresult rv;
    rv = NS_GetSpecialDirectory(NS_APP_USER_PROFILE_50_DIR, getter_AddRefs(mCookieFile));
    if (NS_SUCCEEDED(rv)) {
      rv = mCookieFile->AppendNative(NS_LITERAL_CSTRING(kCookieFileName2));
    }
    COOKIE_Read();
  } else if (!nsCRT::strcmp(aTopic, "cookieIcon")) {
    // this is an evil trick to avoid the blatant inefficiency of
    // (!nsCRT::strcmp(aData, NS_LITERAL_STRING("on").get()))
    gCookieIconVisible = (aData[0] == 'o' && aData[1] == 'n' && aData[2] == '\0');
  }

  return NS_OK;
}
