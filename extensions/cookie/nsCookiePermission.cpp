// vim:ts=2:sw=2:et:
/* ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is cookie manager code.
 *
 * The Initial Developer of the Original Code is
 * Michiel van Leeuwen (mvl@exedo.nl).
 * Portions created by the Initial Developer are Copyright (C) 2003
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Darin Fisher <darin@meer.net>
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */


#include "nsCookiePermission.h"
#include "nsICookie2.h"
#include "nsIServiceManager.h"
#include "nsICookiePromptService.h"
#include "nsICookieManager2.h"
#include "nsNetCID.h"
#include "nsIURI.h"
#include "nsIPrefService.h"
#include "nsIPrefBranch.h"
#include "nsIPrefBranchInternal.h"
#include "nsIDocShell.h"
#include "nsIDocShellTreeItem.h"
#include "nsIInterfaceRequestor.h"
#include "nsIInterfaceRequestorUtils.h"
#include "nsILoadGroup.h"
#include "nsIChannel.h"
#include "nsIDOMWindow.h"
#include "nsString.h"
#include "nsCRT.h"

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

static const PRBool kDefaultPolicy = PR_TRUE;
static const char kCookiesLifetimePolicy[] = "network.cookie.lifetimePolicy";
static const char kCookiesLifetimeDays[] = "network.cookie.lifetime.days";
static const char kCookiesAlwaysAcceptSession[] = "network.cookie.alwaysAcceptSessionCookies";
#ifdef MOZ_MAIL_NEWS
static const char kCookiesDisabledForMailNews[] = "network.cookie.disableCookieForMailNews";
#endif

static const char kCookiesPrefsMigrated[] = "network.cookie.prefsMigrated";
// obsolete pref names for migration
static const char kCookiesLifetimeEnabled[] = "network.cookie.lifetime.enabled";
static const char kCookiesLifetimeBehavior[] = "network.cookie.lifetime.behavior";
static const char kCookiesAskPermission[] = "network.cookie.warnAboutCookies";

static const char kPermissionType[] = "cookie";

// XXX these casts and constructs are horrible, but our nsInt64/nsTime
// classes are lacking so we need them for now. see bug 198694.
#define USEC_PER_SEC   (nsInt64(1000000))
#define NOW_IN_SECONDS (nsInt64(PR_Now()) / USEC_PER_SEC)

#ifdef MOZ_MAIL_NEWS
// returns PR_TRUE if URI appears to be the URI of a mailnews protocol
static PRBool
IsFromMailNews(nsIURI *aURI)
{
  static const char *kMailNewsProtocols[] =
      { "imap", "news", "snews", "mailbox", nsnull };
  PRBool result;
  for (const char **p = kMailNewsProtocols; *p; ++p) {
    if (NS_SUCCEEDED(aURI->SchemeIs(*p, &result)) && result)
      return PR_TRUE;
  }
  return PR_FALSE;
}
#endif

static void
GetInterfaceFromChannel(nsIChannel   *aChannel,
                        const nsIID  &aIID,
                        void        **aResult)
{
  if (!aChannel)
    return; // no context, no interface!
  NS_ASSERTION(!*aResult, "result not initialized to null");

  nsCOMPtr<nsIInterfaceRequestor> cbs;
  aChannel->GetNotificationCallbacks(getter_AddRefs(cbs));
  if (cbs)
    cbs->GetInterface(aIID, aResult);
  if (!*aResult) {
    // try load group's notification callbacks...
    nsCOMPtr<nsILoadGroup> loadGroup;
    aChannel->GetLoadGroup(getter_AddRefs(loadGroup));
    if (loadGroup) {
      loadGroup->GetNotificationCallbacks(getter_AddRefs(cbs));
      if (cbs)
        cbs->GetInterface(aIID, aResult);
    }
  }
}

NS_IMPL_ISUPPORTS2(nsCookiePermission,
                   nsICookiePermission,
                   nsIObserver)

nsresult
nsCookiePermission::Init()
{
  nsresult rv;
  mPermMgr = do_GetService(NS_PERMISSIONMANAGER_CONTRACTID, &rv);
  if (NS_FAILED(rv)) return rv;

  // failure to access the pref service is non-fatal...
  nsCOMPtr<nsIPrefBranchInternal> prefBranch =
      do_GetService(NS_PREFSERVICE_CONTRACTID);
  if (prefBranch) {
    prefBranch->AddObserver(kCookiesLifetimePolicy, this, PR_FALSE);
    prefBranch->AddObserver(kCookiesLifetimeDays, this, PR_FALSE);
    prefBranch->AddObserver(kCookiesAlwaysAcceptSession, this, PR_FALSE);
#ifdef MOZ_MAIL_NEWS
    prefBranch->AddObserver(kCookiesDisabledForMailNews, this, PR_FALSE);
#endif
    PrefChanged(prefBranch, nsnull);

    // migration code for original cookie prefs
    PRBool migrated;
    rv = prefBranch->GetBoolPref(kCookiesPrefsMigrated, &migrated);
    if (NS_FAILED(rv) || !migrated) {
      PRBool warnAboutCookies = PR_FALSE;
      prefBranch->GetBoolPref(kCookiesAskPermission, &warnAboutCookies);

      // if the user is using ask before accepting, we'll use that
      if (warnAboutCookies)
        prefBranch->SetIntPref(kCookiesLifetimePolicy, ASK_BEFORE_ACCEPT);
        
      PRBool lifetimeEnabled = PR_FALSE;
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
      prefBranch->SetBoolPref(kCookiesPrefsMigrated, PR_TRUE);
    }
  }

  return NS_OK;
}

void
nsCookiePermission::PrefChanged(nsIPrefBranch *aPrefBranch,
                                const char    *aPref)
{
  PRBool val;

#define PREF_CHANGED(_P) (!aPref || !strcmp(aPref, _P))

  if (PREF_CHANGED(kCookiesLifetimePolicy) &&
      NS_SUCCEEDED(aPrefBranch->GetIntPref(kCookiesLifetimePolicy, &val)))
    mCookiesLifetimePolicy = val;

  if (PREF_CHANGED(kCookiesLifetimeDays) &&
      NS_SUCCEEDED(aPrefBranch->GetIntPref(kCookiesLifetimeDays, &val)))
    // save cookie lifetime in seconds instead of days
    mCookiesLifetimeSec = val * 24 * 60 * 60;

  if (PREF_CHANGED(kCookiesAlwaysAcceptSession) &&
      NS_SUCCEEDED(aPrefBranch->GetBoolPref(kCookiesAlwaysAcceptSession, &val)))
    mCookiesAlwaysAcceptSession = val;

#ifdef MOZ_MAIL_NEWS
  if (PREF_CHANGED(kCookiesDisabledForMailNews) &&
      NS_SUCCEEDED(aPrefBranch->GetBoolPref(kCookiesDisabledForMailNews, &val)))
    mCookiesDisabledForMailNews = val;
#endif
}

NS_IMETHODIMP
nsCookiePermission::SetAccess(nsIURI         *aURI,
                              nsCookieAccess  aAccess)
{
  //
  // NOTE: nsCookieAccess values conveniently match up with
  //       the permission codes used by nsIPermissionManager.
  //       this is nice because it avoids conversion code.
  //
  return mPermMgr->Add(aURI, kPermissionType, aAccess);
}

NS_IMETHODIMP
nsCookiePermission::CanAccess(nsIURI         *aURI,
                              nsIURI         *aFirstURI,
                              nsIChannel     *aChannel,
                              nsCookieAccess *aResult)
{
#ifdef MOZ_MAIL_NEWS
  // disable cookies in mailnews if user's prefs say so
  if (mCookiesDisabledForMailNews) {
    //
    // try to examine the "app type" of the docshell owning this request.  if
    // we find a docshell in the heirarchy of type APP_TYPE_MAIL, then assume
    // this URI is being loaded from within mailnews.
    //
    // XXX this is a pretty ugly hack at the moment since cookies really
    // shouldn't have to talk to the docshell directly.  ultimately, we want
    // to talk to some more generic interface, which the docshell would also
    // implement.  but, the basic mechanism here of leveraging the channel's
    // (or loadgroup's) notification callbacks attribute seems ideal as it
    // avoids the problem of having to modify all places in the code which
    // kick off network requests.
    //
    PRUint32 appType = nsIDocShell::APP_TYPE_UNKNOWN;
    if (aChannel) {
      nsCOMPtr<nsIDocShellTreeItem> item, parent;
      GetInterfaceFromChannel(aChannel, NS_GET_IID(nsIDocShellTreeItem),
                              getter_AddRefs(parent));
      if (parent) {
        do {
            item = parent;
            nsCOMPtr<nsIDocShell> docshell = do_QueryInterface(item);
            if (docshell)
              docshell->GetAppType(&appType);
        } while (appType != nsIDocShell::APP_TYPE_MAIL &&
                 NS_SUCCEEDED(item->GetParent(getter_AddRefs(parent))) &&
                 parent);
      }
    }
    if ((appType == nsIDocShell::APP_TYPE_MAIL) ||
        (aFirstURI && IsFromMailNews(aFirstURI)) ||
        IsFromMailNews(aURI)) {
      *aResult = ACCESS_DENY;
      return NS_OK;
    }
  }
#endif // MOZ_MAIL_NEWS
  
  // finally, check with permission manager...
  nsresult rv = mPermMgr->TestPermission(aURI, kPermissionType, (PRUint32 *) aResult);
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
                                 PRBool     *aIsSession,
                                 PRInt64    *aExpiry,
                                 PRBool     *aResult)
{
  NS_ASSERTION(aURI, "null uri");

  *aResult = kDefaultPolicy;

  PRUint32 perm;
  mPermMgr->TestPermission(aURI, kPermissionType, &perm);
  switch (perm) {
  case nsICookiePermission::ACCESS_SESSION:
    *aIsSession = PR_TRUE;

  case nsIPermissionManager::ALLOW_ACTION: // ACCESS_ALLOW
    *aResult = PR_TRUE;
    break;

  case nsIPermissionManager::DENY_ACTION:  // ACCESS_DENY
    *aResult = PR_FALSE;
    break;

  default:
    // the permission manager has nothing to say about this cookie -
    // so, we apply the default prefs to it.
    NS_ASSERTION(perm == nsIPermissionManager::UNKNOWN_ACTION, "unknown permission");
    
    // now we need to figure out what type of accept policy we're dealing with
    // if we accept cookies normally, just bail and return
    if (mCookiesLifetimePolicy == ACCEPT_NORMALLY) {
      *aResult = PR_TRUE;
      return NS_OK;
    }
    
    // declare this here since it'll be used in all of the remaining cases
    nsInt64 currentTime = NOW_IN_SECONDS;
    nsInt64 delta = nsInt64(*aExpiry) - currentTime;
    
    // check whether the user wants to be prompted
    if (mCookiesLifetimePolicy == ASK_BEFORE_ACCEPT) {
      // if it's a session cookie and the user wants to accept these 
      // without asking, just accept the cookie and return
      if (*aIsSession && mCookiesAlwaysAcceptSession) {
        *aResult = PR_TRUE;
        return NS_OK;
      }
      
      // default to rejecting, in case the prompting process fails
      *aResult = PR_FALSE;

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
      GetInterfaceFromChannel(aChannel, NS_GET_IID(nsIDOMWindow),
                              getter_AddRefs(parent));

      // get some useful information to present to the user:
      // whether a previous cookie already exists, and how many cookies this host
      // has set
      PRBool foundCookie;
      PRUint32 countFromHost;
      nsCOMPtr<nsICookieManager2> cookieManager = do_GetService(NS_COOKIEMANAGER_CONTRACTID, &rv);
      if (NS_SUCCEEDED(rv))
        rv = cookieManager->FindMatchingCookie(aCookie, &countFromHost, &foundCookie);
      if (NS_FAILED(rv)) return rv;

      // check if the cookie we're trying to set is already expired, and return;
      // but only if there's no previous cookie, because then we need to delete the previous
      // cookie. we need this check to avoid prompting the user for already-expired cookies.
      if (!foundCookie && !*aIsSession && delta <= nsInt64(0)) {
        // the cookie has already expired. accept it, and let the backend figure
        // out it's expired, so that we get correct logging & notifications.
        *aResult = PR_TRUE;
        return rv;
      }

      PRBool rememberDecision = PR_FALSE;
      rv = cookiePromptService->CookieDialog(parent, aCookie, hostPort, 
                                             countFromHost, foundCookie,
                                             &rememberDecision, aResult);
      if (NS_FAILED(rv)) return rv;
      
      if (*aResult == nsICookiePromptService::ACCEPT_SESSION_COOKIE)
        *aIsSession = PR_TRUE;

      if (rememberDecision) {
        switch (*aResult) {
          case nsICookiePromptService::DENY_COOKIE:
            mPermMgr->Add(aURI, kPermissionType, (PRUint32) nsIPermissionManager::DENY_ACTION);
            break;
          case nsICookiePromptService::ACCEPT_COOKIE:
            mPermMgr->Add(aURI, kPermissionType, (PRUint32) nsIPermissionManager::ALLOW_ACTION);
            break;
          case nsICookiePromptService::ACCEPT_SESSION_COOKIE:
            mPermMgr->Add(aURI, kPermissionType, nsICookiePermission::ACCESS_SESSION);
            break;
          default:
            break;
        }
      }
    } else {
      // we're not prompting, so we must be limiting the lifetime somehow
      // if it's a session cookie, we do nothing
      if (!*aIsSession && delta > nsInt64(0)) {
        if (mCookiesLifetimePolicy == ACCEPT_SESSION) {
          // limit lifetime to session
          *aIsSession = PR_TRUE;
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
                            const PRUnichar *aData)
{
  nsCOMPtr<nsIPrefBranch> prefBranch = do_QueryInterface(aSubject);
  NS_ASSERTION(!nsCRT::strcmp(NS_PREFBRANCH_PREFCHANGE_TOPIC_ID, aTopic),
               "unexpected topic - we only deal with pref changes!");

  if (prefBranch)
    PrefChanged(prefBranch, NS_LossyConvertUTF16toASCII(aData).get());
  return NS_OK;
}
