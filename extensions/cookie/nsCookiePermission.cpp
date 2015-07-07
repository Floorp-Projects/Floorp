/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=2 sw=2 et: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsCookiePermission.h"

#include "mozIThirdPartyUtil.h"
#include "nsICookie2.h"
#include "nsIServiceManager.h"
#include "nsICookiePromptService.h"
#include "nsICookieManager2.h"
#include "nsNetUtil.h"
#include "nsIInterfaceRequestorUtils.h"
#include "nsIProtocolHandler.h"
#include "nsIURI.h"
#include "nsIPrefService.h"
#include "nsIPrefBranch.h"
#include "nsIChannel.h"
#include "nsIHttpChannelInternal.h"
#include "nsIDOMWindow.h"
#include "nsIPrincipal.h"
#include "nsString.h"
#include "nsCRT.h"
#include "nsILoadContext.h"
#include "nsIScriptObjectPrincipal.h"
#include "nsNetCID.h"
#include "prtime.h"

/****************************************************************
 ************************ nsCookiePermission ********************
 ****************************************************************/

// values for mCookiesLifetimePolicy
// 0 == accept normally
// 1 == ask before accepting
// 2 == downgrade to session
// 3 == limit lifetime to N days
static const uint32_t ACCEPT_NORMALLY = 0;
static const uint32_t ASK_BEFORE_ACCEPT = 1;
static const uint32_t ACCEPT_SESSION = 2;
static const uint32_t ACCEPT_FOR_N_DAYS = 3;

static const bool kDefaultPolicy = true;
static const char kCookiesLifetimePolicy[] = "network.cookie.lifetimePolicy";
static const char kCookiesLifetimeDays[] = "network.cookie.lifetime.days";
static const char kCookiesAlwaysAcceptSession[] = "network.cookie.alwaysAcceptSessionCookies";

static const char kCookiesPrefsMigrated[] = "network.cookie.prefsMigrated";
// obsolete pref names for migration
static const char kCookiesLifetimeEnabled[] = "network.cookie.lifetime.enabled";
static const char kCookiesLifetimeBehavior[] = "network.cookie.lifetime.behavior";
static const char kCookiesAskPermission[] = "network.cookie.warnAboutCookies";

static const char kPermissionType[] = "cookie";

NS_IMPL_ISUPPORTS(nsCookiePermission,
                  nsICookiePermission,
                  nsIObserver)

bool
nsCookiePermission::Init()
{
  // Initialize nsIPermissionManager and fetch relevant prefs. This is only
  // required for some methods on nsICookiePermission, so it should be done
  // lazily.
  nsresult rv;
  mPermMgr = do_GetService(NS_PERMISSIONMANAGER_CONTRACTID, &rv);
  if (NS_FAILED(rv)) return false;
  mThirdPartyUtil = do_GetService(THIRDPARTYUTIL_CONTRACTID, &rv);
  if (NS_FAILED(rv)) return false;

  // failure to access the pref service is non-fatal...
  nsCOMPtr<nsIPrefBranch> prefBranch =
      do_GetService(NS_PREFSERVICE_CONTRACTID);
  if (prefBranch) {
    prefBranch->AddObserver(kCookiesLifetimePolicy, this, false);
    prefBranch->AddObserver(kCookiesLifetimeDays, this, false);
    prefBranch->AddObserver(kCookiesAlwaysAcceptSession, this, false);
    PrefChanged(prefBranch, nullptr);

    // migration code for original cookie prefs
    bool migrated;
    rv = prefBranch->GetBoolPref(kCookiesPrefsMigrated, &migrated);
    if (NS_FAILED(rv) || !migrated) {
      bool warnAboutCookies = false;
      prefBranch->GetBoolPref(kCookiesAskPermission, &warnAboutCookies);

      // if the user is using ask before accepting, we'll use that
      if (warnAboutCookies)
        prefBranch->SetIntPref(kCookiesLifetimePolicy, ASK_BEFORE_ACCEPT);
        
      bool lifetimeEnabled = false;
      prefBranch->GetBoolPref(kCookiesLifetimeEnabled, &lifetimeEnabled);
      
      // if they're limiting lifetime and not using the prompts, use the 
      // appropriate limited lifetime pref
      if (lifetimeEnabled && !warnAboutCookies) {
        int32_t lifetimeBehavior;
        prefBranch->GetIntPref(kCookiesLifetimeBehavior, &lifetimeBehavior);
        if (lifetimeBehavior)
          prefBranch->SetIntPref(kCookiesLifetimePolicy, ACCEPT_FOR_N_DAYS);
        else
          prefBranch->SetIntPref(kCookiesLifetimePolicy, ACCEPT_SESSION);
      }
      prefBranch->SetBoolPref(kCookiesPrefsMigrated, true);
    }
  }

  return true;
}

void
nsCookiePermission::PrefChanged(nsIPrefBranch *aPrefBranch,
                                const char    *aPref)
{
  int32_t val;

#define PREF_CHANGED(_P) (!aPref || !strcmp(aPref, _P))

  if (PREF_CHANGED(kCookiesLifetimePolicy) &&
      NS_SUCCEEDED(aPrefBranch->GetIntPref(kCookiesLifetimePolicy, &val)))
    mCookiesLifetimePolicy = val;

  if (PREF_CHANGED(kCookiesLifetimeDays) &&
      NS_SUCCEEDED(aPrefBranch->GetIntPref(kCookiesLifetimeDays, &val)))
    // save cookie lifetime in seconds instead of days
    mCookiesLifetimeSec = val * 24 * 60 * 60;

  bool bval;
  if (PREF_CHANGED(kCookiesAlwaysAcceptSession) &&
      NS_SUCCEEDED(aPrefBranch->GetBoolPref(kCookiesAlwaysAcceptSession, &bval)))
    mCookiesAlwaysAcceptSession = bval;
}

NS_IMETHODIMP
nsCookiePermission::SetAccess(nsIURI         *aURI,
                              nsCookieAccess  aAccess)
{
  // Lazily initialize ourselves
  if (!EnsureInitialized())
    return NS_ERROR_UNEXPECTED;

  //
  // NOTE: nsCookieAccess values conveniently match up with
  //       the permission codes used by nsIPermissionManager.
  //       this is nice because it avoids conversion code.
  //
  return mPermMgr->Add(aURI, kPermissionType, aAccess,
                       nsIPermissionManager::EXPIRE_NEVER, 0);
}

NS_IMETHODIMP
nsCookiePermission::CanAccess(nsIURI         *aURI,
                              nsIChannel     *aChannel,
                              nsCookieAccess *aResult)
{
  // Check this protocol doesn't allow cookies
  bool hasFlags;
  nsresult rv =
    NS_URIChainHasFlags(aURI, nsIProtocolHandler::URI_FORBIDS_COOKIE_ACCESS,
                        &hasFlags);
  if (NS_FAILED(rv) || hasFlags) {
    *aResult = ACCESS_DENY;
    return NS_OK;
  }

  // Lazily initialize ourselves
  if (!EnsureInitialized())
    return NS_ERROR_UNEXPECTED;

  // finally, check with permission manager...
  rv = mPermMgr->TestPermission(aURI, kPermissionType, (uint32_t *) aResult);
  if (NS_SUCCEEDED(rv)) {
    if (*aResult == nsICookiePermission::ACCESS_SESSION) {
      *aResult = nsICookiePermission::ACCESS_ALLOW;
    }
  }

  return rv;
}

NS_IMETHODIMP
nsCookiePermission::CanSetCookie(nsIURI     *aURI,
                                 nsIChannel *aChannel,
                                 nsICookie2 *aCookie,
                                 bool       *aIsSession,
                                 int64_t    *aExpiry,
                                 bool       *aResult)
{
  NS_ASSERTION(aURI, "null uri");

  *aResult = kDefaultPolicy;

  // Lazily initialize ourselves
  if (!EnsureInitialized())
    return NS_ERROR_UNEXPECTED;

  uint32_t perm;
  mPermMgr->TestPermission(aURI, kPermissionType, &perm);
  bool isThirdParty = false;
  switch (perm) {
  case nsICookiePermission::ACCESS_SESSION:
    *aIsSession = true;

  case nsICookiePermission::ACCESS_ALLOW:
    *aResult = true;
    break;

  case nsICookiePermission::ACCESS_DENY:
    *aResult = false;
    break;

  case nsICookiePermission::ACCESS_ALLOW_FIRST_PARTY_ONLY:
    mThirdPartyUtil->IsThirdPartyChannel(aChannel, aURI, &isThirdParty);
    // If it's third party, we can't set the cookie
    if (isThirdParty)
      *aResult = false;
    break;

  case nsICookiePermission::ACCESS_LIMIT_THIRD_PARTY:
    mThirdPartyUtil->IsThirdPartyChannel(aChannel, aURI, &isThirdParty);
    // If it's third party, check whether cookies are already set
    if (isThirdParty) {
      nsresult rv;
      nsCOMPtr<nsICookieManager2> cookieManager = do_GetService(NS_COOKIEMANAGER_CONTRACTID, &rv);
      if (NS_FAILED(rv)) {
        *aResult = false;
        break;
      }
      uint32_t priorCookieCount = 0;
      nsAutoCString hostFromURI;
      aURI->GetHost(hostFromURI);
      cookieManager->CountCookiesFromHost(hostFromURI, &priorCookieCount);
      *aResult = priorCookieCount != 0;
    }
    break;

  default:
    // the permission manager has nothing to say about this cookie -
    // so, we apply the default prefs to it.
    NS_ASSERTION(perm == nsIPermissionManager::UNKNOWN_ACTION, "unknown permission");

    // now we need to figure out what type of accept policy we're dealing with
    // if we accept cookies normally, just bail and return
    if (mCookiesLifetimePolicy == ACCEPT_NORMALLY) {
      *aResult = true;
      return NS_OK;
    }

    // declare this here since it'll be used in all of the remaining cases
    int64_t currentTime = PR_Now() / PR_USEC_PER_SEC;
    int64_t delta = *aExpiry - currentTime;

    // check whether the user wants to be prompted
    if (mCookiesLifetimePolicy == ASK_BEFORE_ACCEPT) {
      // if it's a session cookie and the user wants to accept these 
      // without asking, or if we are in private browsing mode, just
      // accept the cookie and return
      if ((*aIsSession && mCookiesAlwaysAcceptSession) ||
          (aChannel && NS_UsePrivateBrowsing(aChannel))) {
        *aResult = true;
        return NS_OK;
      }

      // default to rejecting, in case the prompting process fails
      *aResult = false;

      nsAutoCString hostPort;
      aURI->GetHostPort(hostPort);

      if (!aCookie) {
         return NS_ERROR_UNEXPECTED;
      }
      // If there is no host, use the scheme, and append "://",
      // to make sure it isn't a host or something.
      // This is done to make the dialog appear for javascript cookies from
      // file:// urls, and make the text on it not too weird. (bug 209689)
      if (hostPort.IsEmpty()) {
        aURI->GetScheme(hostPort);
        if (hostPort.IsEmpty()) {
          // still empty. Just return the default.
          return NS_OK;
        }
        hostPort = hostPort + NS_LITERAL_CSTRING("://");
      }

      // we don't cache the cookiePromptService - it's not used often, so not
      // worth the memory.
      nsresult rv;
      nsCOMPtr<nsICookiePromptService> cookiePromptService =
          do_GetService(NS_COOKIEPROMPTSERVICE_CONTRACTID, &rv);
      if (NS_FAILED(rv)) return rv;

      // get some useful information to present to the user:
      // whether a previous cookie already exists, and how many cookies this host
      // has set
      bool foundCookie = false;
      uint32_t countFromHost;
      nsCOMPtr<nsICookieManager2> cookieManager = do_GetService(NS_COOKIEMANAGER_CONTRACTID, &rv);
      if (NS_SUCCEEDED(rv)) {
        nsAutoCString rawHost;
        aCookie->GetRawHost(rawHost);
        rv = cookieManager->CountCookiesFromHost(rawHost, &countFromHost);

        if (NS_SUCCEEDED(rv) && countFromHost > 0)
          rv = cookieManager->CookieExists(aCookie, &foundCookie);
      }
      if (NS_FAILED(rv)) return rv;

      // check if the cookie we're trying to set is already expired, and return;
      // but only if there's no previous cookie, because then we need to delete the previous
      // cookie. we need this check to avoid prompting the user for already-expired cookies.
      if (!foundCookie && !*aIsSession && delta <= 0) {
        // the cookie has already expired. accept it, and let the backend figure
        // out it's expired, so that we get correct logging & notifications.
        *aResult = true;
        return rv;
      }

      bool rememberDecision = false;
      int32_t dialogRes = nsICookiePromptService::DENY_COOKIE;
      rv = cookiePromptService->CookieDialog(nullptr, aCookie, hostPort, 
                                             countFromHost, foundCookie,
                                             &rememberDecision, &dialogRes);
      if (NS_FAILED(rv)) return rv;

      *aResult = !!dialogRes;
      if (dialogRes == nsICookiePromptService::ACCEPT_SESSION_COOKIE)
        *aIsSession = true;

      if (rememberDecision) {
        switch (dialogRes) {
          case nsICookiePromptService::DENY_COOKIE:
            mPermMgr->Add(aURI, kPermissionType, (uint32_t) nsIPermissionManager::DENY_ACTION,
                          nsIPermissionManager::EXPIRE_NEVER, 0);
            break;
          case nsICookiePromptService::ACCEPT_COOKIE:
            mPermMgr->Add(aURI, kPermissionType, (uint32_t) nsIPermissionManager::ALLOW_ACTION,
                          nsIPermissionManager::EXPIRE_NEVER, 0);
            break;
          case nsICookiePromptService::ACCEPT_SESSION_COOKIE:
            mPermMgr->Add(aURI, kPermissionType, nsICookiePermission::ACCESS_SESSION,
                          nsIPermissionManager::EXPIRE_NEVER, 0);
            break;
          default:
            break;
        }
      }
    } else {
      // we're not prompting, so we must be limiting the lifetime somehow
      // if it's a session cookie, we do nothing
      if (!*aIsSession && delta > 0) {
        if (mCookiesLifetimePolicy == ACCEPT_SESSION) {
          // limit lifetime to session
          *aIsSession = true;
        } else if (delta > mCookiesLifetimeSec) {
          // limit lifetime to specified time
          *aExpiry = currentTime + mCookiesLifetimeSec;
        }
      }
    }
  }

  return NS_OK;
}

NS_IMETHODIMP
nsCookiePermission::Observe(nsISupports     *aSubject,
                            const char      *aTopic,
                            const char16_t *aData)
{
  nsCOMPtr<nsIPrefBranch> prefBranch = do_QueryInterface(aSubject);
  NS_ASSERTION(!nsCRT::strcmp(NS_PREFBRANCH_PREFCHANGE_TOPIC_ID, aTopic),
               "unexpected topic - we only deal with pref changes!");

  if (prefBranch)
    PrefChanged(prefBranch, NS_LossyConvertUTF16toASCII(aData).get());
  return NS_OK;
}
