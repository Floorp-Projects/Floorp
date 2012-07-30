/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=2 sw=2 et: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */


#include "nsCookiePermission.h"
#include "nsICookie2.h"
#include "nsIServiceManager.h"
#include "nsICookiePromptService.h"
#include "nsICookieManager2.h"
#include "nsNetUtil.h"
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

/****************************************************************
 ************************ nsCookiePermission ********************
 ****************************************************************/

// values for mCookiesLifetimePolicy
// 0 == accept normally
// 1 == ask before accepting
// 2 == downgrade to session
// 3 == limit lifetime to N days
static const PRUint32 ACCEPT_NORMALLY = 0;
static const PRUint32 ASK_BEFORE_ACCEPT = 1;
static const PRUint32 ACCEPT_SESSION = 2;
static const PRUint32 ACCEPT_FOR_N_DAYS = 3;

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

NS_IMPL_ISUPPORTS2(nsCookiePermission,
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
        PRInt32 lifetimeBehavior;
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
  PRInt32 val;

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
  rv = mPermMgr->TestPermission(aURI, kPermissionType, (PRUint32 *) aResult);
  if (NS_SUCCEEDED(rv)) {
    switch (*aResult) {
    // if we have one of the publicly-available values, just return it
    case nsIPermissionManager::UNKNOWN_ACTION: // ACCESS_DEFAULT
    case nsIPermissionManager::ALLOW_ACTION:   // ACCESS_ALLOW
    case nsIPermissionManager::DENY_ACTION:    // ACCESS_DENY
      break;

    // ACCESS_SESSION means the cookie can be accepted; the session 
    // downgrade will occur in CanSetCookie().
    case nsICookiePermission::ACCESS_SESSION:
      *aResult = ACCESS_ALLOW;
      break;

    // ack, an unknown type! just use the defaults.
    default:
      *aResult = ACCESS_DEFAULT;
    }
  }

  return rv;
}

NS_IMETHODIMP 
nsCookiePermission::CanSetCookie(nsIURI     *aURI,
                                 nsIChannel *aChannel,
                                 nsICookie2 *aCookie,
                                 bool       *aIsSession,
                                 PRInt64    *aExpiry,
                                 bool       *aResult)
{
  NS_ASSERTION(aURI, "null uri");

  *aResult = kDefaultPolicy;

  // Lazily initialize ourselves
  if (!EnsureInitialized())
    return NS_ERROR_UNEXPECTED;

  PRUint32 perm;
  mPermMgr->TestPermission(aURI, kPermissionType, &perm);
  switch (perm) {
  case nsICookiePermission::ACCESS_SESSION:
    *aIsSession = true;

  case nsIPermissionManager::ALLOW_ACTION: // ACCESS_ALLOW
    *aResult = true;
    break;

  case nsIPermissionManager::DENY_ACTION:  // ACCESS_DENY
    *aResult = false;
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
    PRInt64 currentTime = PR_Now() / PR_USEC_PER_SEC;
    PRInt64 delta = *aExpiry - currentTime;
    
    // check whether the user wants to be prompted
    if (mCookiesLifetimePolicy == ASK_BEFORE_ACCEPT) {
      // if it's a session cookie and the user wants to accept these 
      // without asking, or if we are in private browsing mode, just
      // accept the cookie and return
      if ((*aIsSession && mCookiesAlwaysAcceptSession) ||
          InPrivateBrowsing()) {
        *aResult = true;
        return NS_OK;
      }
      
      // default to rejecting, in case the prompting process fails
      *aResult = false;

      nsCAutoString hostPort;
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

      // try to get a nsIDOMWindow from the channel...
      nsCOMPtr<nsIDOMWindow> parent;
      if (aChannel) {
        nsCOMPtr<nsILoadContext> ctx;
        NS_QueryNotificationCallbacks(aChannel, ctx);
        if (ctx) {
          ctx->GetAssociatedWindow(getter_AddRefs(parent));
        }
      }

      // get some useful information to present to the user:
      // whether a previous cookie already exists, and how many cookies this host
      // has set
      bool foundCookie = false;
      PRUint32 countFromHost;
      nsCOMPtr<nsICookieManager2> cookieManager = do_GetService(NS_COOKIEMANAGER_CONTRACTID, &rv);
      if (NS_SUCCEEDED(rv)) {
        nsCAutoString rawHost;
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
      PRInt32 dialogRes = nsICookiePromptService::DENY_COOKIE;
      rv = cookiePromptService->CookieDialog(parent, aCookie, hostPort, 
                                             countFromHost, foundCookie,
                                             &rememberDecision, &dialogRes);
      if (NS_FAILED(rv)) return rv;

      *aResult = !!dialogRes;
      if (dialogRes == nsICookiePromptService::ACCEPT_SESSION_COOKIE)
        *aIsSession = true;

      if (rememberDecision) {
        switch (dialogRes) {
          case nsICookiePromptService::DENY_COOKIE:
            mPermMgr->Add(aURI, kPermissionType, (PRUint32) nsIPermissionManager::DENY_ACTION,
                          nsIPermissionManager::EXPIRE_NEVER, 0);
            break;
          case nsICookiePromptService::ACCEPT_COOKIE:
            mPermMgr->Add(aURI, kPermissionType, (PRUint32) nsIPermissionManager::ALLOW_ACTION,
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
nsCookiePermission::GetOriginatingURI(nsIChannel  *aChannel,
                                      nsIURI     **aURI)
{
  /* to find the originating URI, we use the loadgroup of the channel to obtain
   * the window owning the load, and from there, we find the top same-type
   * window and its URI. there are several possible cases:
   *
   * 1) no channel.
   *
   * 2) a channel with the "force allow third party cookies" option set.
   *    since we may not have a window, we return the channel URI in this case.
   *
   * 3) a channel, but no window. this can occur when the consumer kicking
   *    off the load doesn't provide one to the channel, and should be limited
   *    to loads of certain types of resources.
   *
   * 4) a window equal to the top window of same type, with the channel its
   *    document channel. this covers the case of a freshly kicked-off load
   *    (e.g. the user typing something in the location bar, or clicking on a
   *    bookmark), where the window's URI hasn't yet been set, and will be
   *    bogus. we return the channel URI in this case.
   *
   * 5) Anything else. this covers most cases for an ordinary page load from
   *    the location bar, and will catch nested frames within a page, image
   *    loads, etc. we return the URI of the root window's document's principal
   *    in this case.
   */

  *aURI = nullptr;

  // case 1)
  if (!aChannel)
    return NS_ERROR_NULL_POINTER;

  // case 2)
  nsCOMPtr<nsIHttpChannelInternal> httpChannelInternal = do_QueryInterface(aChannel);
  if (httpChannelInternal)
  {
    bool doForce = false;
    if (NS_SUCCEEDED(httpChannelInternal->GetForceAllowThirdPartyCookie(&doForce)) && doForce)
    {
      // return the channel's URI (we may not have a window)
      aChannel->GetURI(aURI);
      if (!*aURI)
        return NS_ERROR_NULL_POINTER;

      return NS_OK;
    }
  }

  // find the associated window and its top window
  nsCOMPtr<nsILoadContext> ctx;
  NS_QueryNotificationCallbacks(aChannel, ctx);
  nsCOMPtr<nsIDOMWindow> topWin, ourWin;
  if (ctx) {
    ctx->GetTopWindow(getter_AddRefs(topWin));
    ctx->GetAssociatedWindow(getter_AddRefs(ourWin));
  }

  // case 3)
  if (!topWin)
    return NS_ERROR_INVALID_ARG;

  // case 4)
  if (ourWin == topWin) {
    // Check whether this is the document channel for this window (representing
    // a load of a new page).  This is a bit of a nasty hack, but we will
    // hopefully flag these channels better later.
    nsLoadFlags flags;
    aChannel->GetLoadFlags(&flags);

    if (flags & nsIChannel::LOAD_DOCUMENT_URI) {
      // get the channel URI - the window's will be bogus
      aChannel->GetURI(aURI);
      if (!*aURI)
        return NS_ERROR_NULL_POINTER;

      return NS_OK;
    }
  }

  // case 5) - get the originating URI from the top window's principal
  nsCOMPtr<nsIScriptObjectPrincipal> scriptObjPrin = do_QueryInterface(topWin);
  NS_ENSURE_TRUE(scriptObjPrin, NS_ERROR_UNEXPECTED);

  nsIPrincipal* prin = scriptObjPrin->GetPrincipal();
  NS_ENSURE_TRUE(prin, NS_ERROR_UNEXPECTED);
  
  prin->GetURI(aURI);

  if (!*aURI)
    return NS_ERROR_NULL_POINTER;

  // all done!
  return NS_OK;
}

NS_IMETHODIMP
nsCookiePermission::Observe(nsISupports     *aSubject,
                            const char      *aTopic,
                            const PRUnichar *aData)
{
  nsCOMPtr<nsIPrefBranch> prefBranch = do_QueryInterface(aSubject);
  NS_ASSERTION(!nsCRT::strcmp(NS_PREFBRANCH_PREFCHANGE_TOPIC_ID, aTopic),
               "unexpected topic - we only deal with pref changes!");

  if (prefBranch)
    PrefChanged(prefBranch, NS_LossyConvertUTF16toASCII(aData).get());
  return NS_OK;
}

bool
nsCookiePermission::InPrivateBrowsing()
{
  bool inPrivateBrowsingMode = false;
  if (!mPBService)
    mPBService = do_GetService(NS_PRIVATE_BROWSING_SERVICE_CONTRACTID);
  if (mPBService)
    mPBService->GetPrivateBrowsingEnabled(&inPrivateBrowsingMode);
  return inPrivateBrowsingMode;
}
