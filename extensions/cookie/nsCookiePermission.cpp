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
// 1 == ask before accepting, no more supported, treated like ACCEPT_NORMALLY (Bug 606655).
// 2 == downgrade to session
// 3 == limit lifetime to N days
static const uint32_t ACCEPT_NORMALLY = 0;
static const uint32_t ASK_BEFORE_ACCEPT = 1;
static const uint32_t ACCEPT_SESSION = 2;
static const uint32_t ACCEPT_FOR_N_DAYS = 3;

static const bool kDefaultPolicy = true;
static const char kCookiesLifetimePolicy[] = "network.cookie.lifetimePolicy";
static const char kCookiesLifetimeDays[] = "network.cookie.lifetime.days";

static const char kCookiesPrefsMigrated[] = "network.cookie.prefsMigrated";
// obsolete pref names for migration
static const char kCookiesLifetimeEnabled[] = "network.cookie.lifetime.enabled";
static const char kCookiesLifetimeBehavior[] = "network.cookie.lifetime.behavior";

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
    PrefChanged(prefBranch, nullptr);

    // migration code for original cookie prefs
    bool migrated;
    rv = prefBranch->GetBoolPref(kCookiesPrefsMigrated, &migrated);
    if (NS_FAILED(rv) || !migrated) {
      bool lifetimeEnabled = false;
      prefBranch->GetBoolPref(kCookiesLifetimeEnabled, &lifetimeEnabled);

      // if they're limiting lifetime, use the appropriate limited lifetime pref
      if (lifetimeEnabled) {
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
      NS_SUCCEEDED(aPrefBranch->GetIntPref(kCookiesLifetimePolicy, &val))) {
    if (val != static_cast<int32_t>(ACCEPT_SESSION) && val != static_cast<int32_t>(ACCEPT_FOR_N_DAYS)) {
      val = ACCEPT_NORMALLY;
    }
    mCookiesLifetimePolicy = val;
  }

  if (PREF_CHANGED(kCookiesLifetimeDays) &&
      NS_SUCCEEDED(aPrefBranch->GetIntPref(kCookiesLifetimeDays, &val)))
    // save cookie lifetime in seconds instead of days
    mCookiesLifetimeSec = (int64_t)val * 24 * 60 * 60;
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
    MOZ_FALLTHROUGH;

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

    // We are accepting the cookie, but,
    // if it's not a session cookie, we may have to limit its lifetime.
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
