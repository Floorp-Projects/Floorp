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
 * Portions created by the Initial Developer are Copyright (C) 2003
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Daniel Witte (dwitte@stanford.edu)
 *   Michiel van Leeuwen (mvl@exedo.nl)
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

#include "nsCookieService.h"
#include "nsCookieHTTPNotify.h"
#include "nsIServiceManager.h"
#include "nsIGenericFactory.h"
#include "nsIScriptGlobalObject.h"
#include "nsIDocumentLoader.h"
#include "nsIWebProgress.h"

#include "nsIPermissionManager.h"
#include "nsIIOService.h"
#include "nsIPrefBranch.h"
#include "nsIPrefBranchInternal.h"
#include "nsIPrefService.h"
#include "nsIDocShell.h"
#include "nsIDocShellTreeItem.h"
#include "nsICookieConsent.h"
#include "nsICookiePermission.h"
#include "nsIURI.h"
#include "nsIURL.h"
#include "nsIChannel.h"
#include "nsIHttpChannel.h"
#include "nsIHttpChannelInternal.h" // evil hack!
#include "nsIPrompt.h"
#include "nsITimer.h"
#include "nsIFile.h"
#include "nsIObserverService.h"
#include "nsILineInputStream.h"

#include "nsAutoPtr.h"
#include "nsReadableUtils.h"
#include "nsCRT.h"
#include "nsInt64.h"
#include "prtime.h"
#include "prprf.h"
#include "prnetdb.h"
#include "nsNetUtil.h"
#include "nsNetCID.h"
#include "nsCURILoader.h"
#include "nsAppDirectoryServiceDefs.h"

/******************************************************************************
 * nsCookieService impl:
 * useful types & constants
 ******************************************************************************/

static NS_DEFINE_IID(kDocLoaderServiceCID, NS_DOCUMENTLOADER_SERVICE_CID);

static const char kCookieFileName[] = "cookies.txt";

static const PRUint32 kLazyWriteLoadingTimeout = 10000; //msec
static const PRUint32 kLazyWriteFinishedTimeout = 1000; //msec

static const PRInt32 kMaxNumberOfCookies = 300;
static const PRInt32 kMaxCookiesPerHost = 20;
static const PRUint32 kMaxBytesPerCookie = 4096;

// the following P3P constants are used to mangle one enumerated type into
// another. we may be able to clean up the p3pservice to use consistent
// types so these aren't required.
// do we need P3P_UnknownPolicy? can't we default to P3P_NoPolicy?
static const PRInt32 P3P_UnknownPolicy   = -1;
static const PRInt32 P3P_NoPolicy        = 0;
static const PRInt32 P3P_NoConsent       = 2;
static const PRInt32 P3P_ImplicitConsent = 4;
static const PRInt32 P3P_ExplicitConsent = 6;
static const PRInt32 P3P_NoIdentInfo     = 8;

static const char P3P_Unknown   = ' ';
static const char P3P_Accept    = 'a';
static const char P3P_Downgrade = 'd';
static const char P3P_Reject    = 'r';
static const char P3P_Flag      = 'f';

// XXX these casts and constructs are horrible, but our nsInt64/nsTime
// classes are lacking so we need them for now. see bug 198694.
#define USEC_PER_SEC   (nsInt64(1000000))
#define NOW_IN_SECONDS (nsInt64(PR_Now()) / USEC_PER_SEC)

// behavior pref constants 
static const PRUint32 BEHAVIOR_ACCEPT        = 0;
static const PRUint32 BEHAVIOR_REJECTFOREIGN = 1;
static const PRUint32 BEHAVIOR_REJECT        = 2;
static const PRUint32 BEHAVIOR_P3P           = 3;

// pref string constants
#ifdef MOZ_PHOENIX
static const char kCookiesEnabled[] = "network.cookie.enable";
static const char kCookiesForDomainOnly[] = "network.cookie.enableForOriginatingWebsiteOnly";
static const char kCookiesLifetimeCurrentSession[] = "network.cookie.enableForCurrentSessionOnly";
#else
static const char kCookiesPermissions[] = "network.cookie.cookieBehavior";
static const char kCookiesDisabledForMailNews[] = "network.cookie.disableCookieForMailNews";
static const char kCookiesLifetimeEnabled[] = "network.cookie.lifetime.enabled";
static const char kCookiesLifetimeDays[] = "network.cookie.lifetime.days";
static const char kCookiesLifetimeCurrentSession[] = "network.cookie.lifetime.behavior";
static const char kCookiesP3PString[] = "network.cookie.p3p";
static const char kCookiesP3PString_Default[] = "drdraaaa";
#endif
static const char kCookiesAskPermission[] = "network.cookie.warnAboutCookies";
static const char kCookiesStrictDomains[] = "network.cookie.strictDomains";

// struct for temporarily storing cookie attributes during header parsing
struct nsCookieAttributes
{
  nsCAutoString name;
  nsCAutoString value;
  nsCAutoString host;
  nsCAutoString path;
  nsCAutoString expires;
  nsCAutoString maxage;
  nsInt64 expiryTime;
  PRBool isSession;
  PRBool isSecure;
  PRBool isDomain;
};

/******************************************************************************
 * Cookie logging handlers
 * used for logging in nsCookieService
 ******************************************************************************/

// logging handlers
#ifdef MOZ_LOGGING
// in order to do logging, the following environment variables need to be set:
//
//    set NSPR_LOG_MODULES=cookie:3 -- shows rejected cookies
//    set NSPR_LOG_MODULES=cookie:4 -- shows accepted and rejected cookies
//    set NSPR_LOG_FILE=c:\cookie.log
//
// this next define has to appear before the include of prlog.h
#define FORCE_PR_LOG // Allow logging in the release build
#include "prlog.h"
#endif

// define logging macros for convenience
#define SET_COOKIE PR_TRUE
#define GET_COOKIE PR_FALSE

#ifdef PR_LOGGING
static PRLogModuleInfo *sCookieLog = PR_NewLogModule("cookie");

#define COOKIE_LOGFAILURE(a, b, c, d) LogFailure(a, b, c, d)
#define COOKIE_LOGSUCCESS(a, b, c, d) LogSuccess(a, b, c, d)

static void
LogFailure(PRBool aSetCookie, nsIURI *aHostURI, const char *aCookieString, const char *aReason)
{
  // if logging isn't enabled, return now to save cycles
  if (!PR_LOG_TEST(sCookieLog, PR_LOG_WARNING)) {
    return;
  }

  nsCAutoString spec;
  if (aHostURI)
    aHostURI->GetAsciiSpec(spec);

  PR_LOG(sCookieLog, PR_LOG_WARNING,
    ("%s%s%s\n", "===== ", aSetCookie ? "COOKIE NOT ACCEPTED" : "COOKIE NOT SENT", " ====="));
  PR_LOG(sCookieLog, PR_LOG_WARNING,("request URL: %s\n", spec.get()));
  if (aSetCookie) {
    PR_LOG(sCookieLog, PR_LOG_WARNING,("cookie string: %s\n", aCookieString));
  }

  PRExplodedTime explodedTime;
  PR_ExplodeTime(PR_Now(), PR_GMTParameters, &explodedTime);
  char timeString[40];
  PR_FormatTimeUSEnglish(timeString, 40, "%c GMT", &explodedTime);

  PR_LOG(sCookieLog, PR_LOG_WARNING,("current time: %s", timeString));
  PR_LOG(sCookieLog, PR_LOG_WARNING,("rejected because %s\n", aReason));
  PR_LOG(sCookieLog, PR_LOG_WARNING,("\n"));
}

static void
LogSuccess(PRBool aSetCookie, nsIURI *aHostURI, const char *aCookieString, nsCookie *aCookie)
{
  // if logging isn't enabled, return now to save cycles
  if (!PR_LOG_TEST(sCookieLog, PR_LOG_DEBUG)) {
    return;
  }

  nsCAutoString spec;
  aHostURI->GetAsciiSpec(spec);

  PR_LOG(sCookieLog, PR_LOG_DEBUG,
    ("%s%s%s\n", "===== ", aSetCookie ? "COOKIE ACCEPTED" : "COOKIE SENT", " ====="));
  PR_LOG(sCookieLog, PR_LOG_DEBUG,("request URL: %s\n", spec.get()));
  PR_LOG(sCookieLog, PR_LOG_DEBUG,("cookie string: %s\n", aCookieString));

  PRExplodedTime explodedTime;
  PR_ExplodeTime(PR_Now(), PR_GMTParameters, &explodedTime);
  char timeString[40];
  PR_FormatTimeUSEnglish(timeString, 40, "%c GMT", &explodedTime);

  PR_LOG(sCookieLog, PR_LOG_DEBUG,("current time: %s", timeString));

  if (aSetCookie) {
    PR_LOG(sCookieLog, PR_LOG_DEBUG,("----------------\n"));
    PR_LOG(sCookieLog, PR_LOG_DEBUG,("name: %s\n", aCookie->Name().get()));
    PR_LOG(sCookieLog, PR_LOG_DEBUG,("value: %s\n", aCookie->Value().get()));
    PR_LOG(sCookieLog, PR_LOG_DEBUG,("%s: %s\n", aCookie->IsDomain() ? "domain" : "host", aCookie->Host().get()));
    PR_LOG(sCookieLog, PR_LOG_DEBUG,("path: %s\n", aCookie->Path().get()));

    if (!aCookie->IsSession()) {
      PR_ExplodeTime(aCookie->Expiry() * USEC_PER_SEC, PR_GMTParameters, &explodedTime);
      PR_FormatTimeUSEnglish(timeString, 40, "%c GMT", &explodedTime);
    }

    PR_LOG(sCookieLog, PR_LOG_DEBUG,
      ("expires: %s", aCookie->IsSession() ? "at end of session" : timeString));
    PR_LOG(sCookieLog, PR_LOG_DEBUG,("is secure: %s\n", aCookie->IsSecure() ? "true" : "false"));
  }
  PR_LOG(sCookieLog, PR_LOG_DEBUG,("\n"));
}

// inline wrappers to make passing in nsAStrings easier
inline void
LogFailure(PRBool aSetCookie, nsIURI *aHostURI, const nsAFlatCString &aCookieString, const char *aReason)
{
  LogFailure(aSetCookie, aHostURI, aCookieString.get(), aReason);
}

inline void
LogSuccess(PRBool aSetCookie, nsIURI *aHostURI, const nsAFlatCString &aCookieString, nsCookie *aCookie)
{
  LogSuccess(aSetCookie, aHostURI, aCookieString.get(), aCookie);
}

#else
#define COOKIE_LOGFAILURE(a, b, c, d) /* nothing */
#define COOKIE_LOGSUCCESS(a, b, c, d) /* nothing */
#endif

/******************************************************************************
 * nsCookieService impl:
 * singleton instance ctor/dtor methods
 ******************************************************************************/

nsCookieService *nsCookieService::gCookieService = nsnull;

nsCookieService*
nsCookieService::GetSingleton()
{
  if (gCookieService) {
    NS_ADDREF(gCookieService);
    return gCookieService;
  }

  // Create a new singleton nsCookieService (note: the ctor AddRefs for us).
  // We AddRef only once since XPCOM has rules about the ordering of module
  // teardowns - by the time our module destructor is called, it's too late to
  // Release our members (e.g. nsIObserverService and nsIPrefBranch), since GC
  // cycles have already been completed and would result in serious leaks.
  // See bug 209571.
  gCookieService = new nsCookieService();

  return gCookieService;
}

/******************************************************************************
 * nsCookieService impl:
 * public methods
 ******************************************************************************/

NS_IMPL_ISUPPORTS6(nsCookieService,
                   nsICookieService,
                   nsICookieManager,
                   nsICookieManager2,
                   nsIObserver,
                   nsIWebProgressListener,
                   nsISupportsWeakReference)

nsCookieService::nsCookieService()
 : mLoadCount(0)
 , mWritePending(PR_FALSE)
 , mCookieChanged(PR_FALSE)
 , mCookieIconVisible(PR_FALSE)
{
  // AddRef now, so we don't cross XPCOM boundaries with a zero refcount
  NS_ADDREF_THIS();

  // cache and init the preferences observer
  InitPrefObservers();

  // cache mCookieFile
  NS_GetSpecialDirectory(NS_APP_USER_PROFILE_50_DIR, getter_AddRefs(mCookieFile));
  if (mCookieFile) {
    mCookieFile->AppendNative(NS_LITERAL_CSTRING(kCookieFileName));
  }

  Read();

  mObserverService = do_GetService("@mozilla.org/observer-service;1");
  if (mObserverService) {
    mObserverService->AddObserver(this, "profile-before-change", PR_TRUE);
    mObserverService->AddObserver(this, "profile-do-change", PR_TRUE);
    mObserverService->AddObserver(this, "cookieIcon", PR_TRUE);
  }

  mP3PService = do_GetService(NS_COOKIECONSENT_CONTRACTID);
  mPermissionService = do_GetService(NS_COOKIEPERMISSION_CONTRACTID);

  // Register as an observer for the document loader  
  nsCOMPtr<nsIDocumentLoader> docLoaderService = do_GetService(kDocLoaderServiceCID);
  nsCOMPtr<nsIWebProgress> progress = do_QueryInterface(docLoaderService);
  if (progress) {
    progress->AddProgressListener(this,
                                  nsIWebProgress::NOTIFY_STATE_DOCUMENT |
                                  nsIWebProgress::NOTIFY_STATE_NETWORK);
  } else {
    NS_ERROR("Couldn't get nsIDocumentLoader");
  }
}

nsCookieService::~nsCookieService()
{
  gCookieService = nsnull;

  if (mWriteTimer)
    mWriteTimer->Cancel();

  // clean up memory
  RemoveAllFromMemory();
}

NS_IMETHODIMP
nsCookieService::Observe(nsISupports     *aSubject,
                         const char      *aTopic,
                         const PRUnichar *aData)
{
  nsresult rv;

  // check the topic
  if (!nsCRT::strcmp(aTopic, "profile-before-change")) {
    // The profile is about to change,
    // or is going away because the application is shutting down.
    if (mWriteTimer)
      mWriteTimer->Cancel();

    if (!nsCRT::strcmp(aData, NS_LITERAL_STRING("shutdown-cleanse").get())) {
      RemoveAllFromMemory();
      // delete the cookie file
      if (mCookieFile) {
        mCookieFile->Remove(PR_FALSE);
      }
    } else {
      Write();
      RemoveAllFromMemory();
    }

  } else if (!nsCRT::strcmp(aTopic, "profile-do-change")) {
    // The profile has already changed.    
    // Now just read them from the new profile location.
    // we also need to update the cached cookie file location
    nsresult rv;
    rv = NS_GetSpecialDirectory(NS_APP_USER_PROFILE_50_DIR, getter_AddRefs(mCookieFile));
    if (NS_SUCCEEDED(rv)) {
      rv = mCookieFile->AppendNative(NS_LITERAL_CSTRING(kCookieFileName));
    }
    Read();

  } else if (!nsCRT::strcmp(aTopic, "cookieIcon")) {
    // this is an evil trick to avoid the blatant inefficiency of
    // (!nsCRT::strcmp(aData, NS_LITERAL_STRING("on").get()))
    mCookieIconVisible = (aData[0] == 'o' && aData[1] == 'n' && aData[2] == '\0');

  } else if (!nsCRT::strcmp(aTopic, NS_PREFBRANCH_PREFCHANGE_TOPIC_ID)) {
    // which pref changed?
    NS_LossyConvertUCS2toASCII pref(aData);
    PRInt32 tempPrefValue;

#ifdef MOZ_PHOENIX
    PRBool computePermissions = PR_FALSE;

    if (pref.Equals(kCookiesEnabled)) {
      rv = mPrefBranch->GetBoolPref(kCookiesEnabled, &tempPrefValue);
      if (NS_FAILED(rv)) {
        tempPrefValue = PR_FALSE;
      }
      mCookiesEnabled_temp = tempPrefValue;
      // set flag so we know to update the enumerated permissions
      computePermissions = PR_TRUE;

    } else if (pref.Equals(kCookiesForDomainOnly)) {
      rv = mPrefBranch->GetBoolPref(kCookiesForDomainOnly, &tempPrefValue);
      if (NS_FAILED(rv)) {
        tempPrefValue = PR_FALSE;
      }
      mCookiesForDomainOnly_temp = tempPrefValue;
      // set flag so we know to update the enumerated permissions
      computePermissions = PR_TRUE;

    } else if (pref.Equals(kCookiesLifetimeCurrentSession)) {
      rv = mPrefBranch->GetBoolPref(kCookiesLifetimeCurrentSession, &tempPrefValue);
      if (NS_FAILED(rv)) {
        tempPrefValue = PR_FALSE;
      }
      mCookiesLifetimeCurrentSession = tempPrefValue;
      // Phoenix hack to reduce ifdefs in code
      mCookiesLifetimeEnabled = mCookiesLifetimeCurrentSession;

#else
    if (pref.Equals(kCookiesPermissions)) {
      rv = mPrefBranch->GetIntPref(kCookiesPermissions, &tempPrefValue);
      if (NS_FAILED(rv) || tempPrefValue < 0 || tempPrefValue > 3) {
        tempPrefValue = BEHAVIOR_REJECT;
      }
      mCookiesPermissions = tempPrefValue;

    } else if (pref.Equals(kCookiesDisabledForMailNews)) {
      rv = mPrefBranch->GetBoolPref(kCookiesDisabledForMailNews, &tempPrefValue);
      if (NS_FAILED(rv)) {
        tempPrefValue = PR_TRUE;
      }
      mCookiesDisabledForMailNews = tempPrefValue;

    } else if (pref.Equals(kCookiesLifetimeEnabled)) {
      rv = mPrefBranch->GetBoolPref(kCookiesLifetimeEnabled, &tempPrefValue);
      if (NS_FAILED(rv)) {
        tempPrefValue = PR_FALSE;
      }
      mCookiesLifetimeEnabled = tempPrefValue;

    } else if (pref.Equals(kCookiesLifetimeDays)) {
      rv = mPrefBranch->GetIntPref(kCookiesLifetimeDays, &mCookiesLifetimeSec);
      if (NS_FAILED(rv)) {
        mCookiesLifetimeEnabled = PR_FALSE; // disable lifetime limit...
        mCookiesLifetimeSec = 0;
      }
      // save cookie lifetime in seconds instead of days
      mCookiesLifetimeSec *= 24*60*60;

    } else if (pref.Equals(kCookiesLifetimeCurrentSession)) {
      rv = mPrefBranch->GetIntPref(kCookiesLifetimeCurrentSession, &tempPrefValue);
      if (NS_FAILED(rv)) {
        tempPrefValue = 1; // disable currentSession limit
      }
      mCookiesLifetimeCurrentSession = (tempPrefValue == 0);

    // P3P prefs
    } else if (pref.Equals(kCookiesP3PString)) {
      rv = mPrefBranch->GetCharPref(kCookiesP3PString, getter_Copies(mCookiesP3PString));
      // check for a malformed string
      if (NS_FAILED(rv) || mCookiesP3PString.Length() != 8) {
        // reassign to default string
        mCookiesP3PString = NS_LITERAL_CSTRING(kCookiesP3PString_Default);
      }

#endif
    // common prefs between Phoenix & Mozilla
    } else if (pref.Equals(kCookiesAskPermission)) {
      rv = mPrefBranch->GetBoolPref(kCookiesAskPermission, &tempPrefValue);
      if (NS_FAILED(rv)) {
        tempPrefValue = PR_FALSE;
      }
      mCookiesAskPermission = tempPrefValue;

    } else if (pref.Equals(kCookiesStrictDomains)) {
      rv = mPrefBranch->GetBoolPref(kCookiesStrictDomains, &tempPrefValue);
      if (NS_FAILED(rv)) {
        tempPrefValue = PR_FALSE;
      }
      mCookiesStrictDomains = tempPrefValue;
    }

#ifdef MOZ_PHOENIX
    // collapse two boolean prefs into enumerated permissions
    // note: BEHAVIOR_P3P is not used in Phoenix, so we won't reach any P3P code.
    if (computePermissions) {
      if (mCookiesEnabled_temp) {
        // check if user wants cookies only for site domain
        if (mCookiesForDomainOnly_temp) {
          mCookiesPermissions = BEHAVIOR_REJECTFOREIGN;
        } else {
          mCookiesPermissions = BEHAVIOR_ACCEPT;
        }
      } else {
        mCookiesPermissions = BEHAVIOR_REJECT;
      }
    }
#endif

  }

  return NS_OK;
}

NS_IMETHODIMP
nsCookieService::GetCookieString(nsIURI     *aHostURI,
                                 nsIChannel *aChannel,
                                 char       **aCookie)
{
  // try to determine first party URI
  nsCOMPtr<nsIURI> firstURI;
  if (aChannel) {
    nsCOMPtr<nsIHttpChannelInternal> httpInternal = do_QueryInterface(aChannel);
    if (httpInternal)
      httpInternal->GetDocumentURI(getter_AddRefs(firstURI));
  }

  return GetCookieStringFromHttp(aHostURI, firstURI, aChannel, aCookie);
}

// helper function for GetCookieStringFromHttp
static inline PRBool ispathdelimiter(char c) { return c == '/' || c == '?' || c == '#' || c == ';'; }

NS_IMETHODIMP
nsCookieService::GetCookieStringFromHttp(nsIURI     *aHostURI,
                                         nsIURI     *aFirstURI,
                                         nsIChannel *aChannel,
                                         char       **aCookie)
{
  *aCookie = nsnull;

  if (!aHostURI) {
    COOKIE_LOGFAILURE(GET_COOKIE, nsnull, nsnull, "host URI is null");
    return NS_OK;
  }

  // check default prefs
  PRUint32 permission;
  nsCookieStatus cookieStatus = CheckPrefs(aHostURI, aFirstURI, aChannel, nsnull, permission);
  // for GetCookie(), we don't update the UI icon if cookie was rejected.
  if (cookieStatus == nsICookie::STATUS_REJECTED) {
    return NS_OK;
  }

  // get host and path from the nsIURI
  // note: there was a "check if host has embedded whitespace" here.
  // it was removed since this check was added into the nsIURI impl (bug 146094).
  nsCAutoString hostFromURI, pathFromURI;
  if (NS_FAILED(aHostURI->GetAsciiHost(hostFromURI)) ||
      NS_FAILED(aHostURI->GetPath(pathFromURI))) {
    COOKIE_LOGFAILURE(GET_COOKIE, aHostURI, nsnull, "couldn't get host/path from URI");
    return NS_OK;
  }
  // trim trailing dots
  hostFromURI.Trim(".");
  ToLowerCase(hostFromURI);

  // initialize variables used in the list traversal
  nsInt64 currentTime = NOW_IN_SECONDS;
  nsCookie *cookieInList;
  // initialize string for return data
  nsCAutoString cookieData;

  // check if aHostURI is using an https secure protocol.
  // if it isn't, then we can't send a secure cookie over the connection.
  // if SchemeIs fails, assume an insecure connection, to be on the safe side
  PRBool isSecure;
  if NS_FAILED(aHostURI->SchemeIs("https", &isSecure)) {
    isSecure = PR_FALSE;
  }

  // begin mCookieList traversal
  PRInt32 count = mCookieList.Count();
  for (PRInt32 i = 0; i < count; ++i) {
    cookieInList = NS_STATIC_CAST(nsCookie*, mCookieList.ElementAt(i));
    NS_ASSERTION(cookieInList, "corrupt cookie list");

    // if the cookie is secure and the host scheme isn't, we can't send it
    if (cookieInList->IsSecure() & !isSecure) {
      continue;
    }

    // check if the host is in the cookie's domain
    // (taking into account whether it's a domain cookie)
    if (!IsInDomain(cookieInList->Host(), hostFromURI, cookieInList->IsDomain())) {
      continue;
    }

    // calculate cookie path length, excluding trailing '/'
    PRUint32 cookiePathLen = cookieInList->Path().Length();
    if (cookiePathLen > 0 && cookieInList->Path().Last() == '/') {
      --cookiePathLen;
    }

    // the cookie list is in order of path length; longest to shortest.
    // if the nsIURI path is shorter than the cookie path, then we know the path
    // isn't on the cookie path.
    if (!StringBeginsWith(pathFromURI, Substring(cookieInList->Path(), 0, cookiePathLen))) {
      continue;
    }

    if (pathFromURI.Length() > cookiePathLen &&
        !ispathdelimiter(pathFromURI.CharAt(cookiePathLen))) {
      /*
       * |ispathdelimiter| tests four cases: '/', '?', '#', and ';'.
       * '/' is the "standard" case; the '?' test allows a site at host/abc?def
       * to receive a cookie that has a path attribute of abc.  this seems
       * strange but at least one major site (citibank, bug 156725) depends
       * on it.  The test for # and ; are put in to proactively avoid problems
       * with other sites - these are the only other chars allowed in the path.
       */
      continue;
    }

    // check if the cookie has expired, and remove if so.
    // note that we do this *after* previous tests passed - so we're only removing
    // the ones that are relevant to this particular search.
    if (!cookieInList->IsSession() && cookieInList->Expiry() <= currentTime) {
      mCookieList.RemoveElementAt(i--); // decrement i so next cookie isn't skipped
      NS_RELEASE(cookieInList);
      --count; // update the count
      mCookieChanged = PR_TRUE;
      continue;
    }

    // all checks passed - update lastAccessed stamp of cookie
    cookieInList->SetLastAccessed(currentTime);

    // check if we have anything to write
    if (!cookieInList->Name().IsEmpty() || !cookieInList->Value().IsEmpty()) {
      // if we've already added a cookie to the return list, append a "; " so
      // that subsequent cookies are delimited in the final list.
      if (!cookieData.IsEmpty()) {
        cookieData += NS_LITERAL_CSTRING("; ");
      }

      // NOTE: we used to have an #ifdef PREVENT_DUPLICATE_NAMES here,
      // which would prevent multiple cookies with the same name being sent (i.e.
      // only the first instance is sent). This wasn't invoked in our previous code,
      // and RFC2109 implicitly allows duplicate names, so I've removed it.

      if (!cookieInList->Name().IsEmpty()) {
        // we have a cookie->name and cookie->cookie - write both
        cookieData += cookieInList->Name() + NS_LITERAL_CSTRING("=") + cookieInList->Value();
      } else {
        // just write cookie->cookie
        cookieData += cookieInList->Value();
      }
    }
  } // for()

  // it's wasteful to alloc a new string; but we have no other choice, until we
  // fix the callers to use nsACStrings.
  if (!cookieData.IsEmpty()) {
    COOKIE_LOGSUCCESS(GET_COOKIE, aHostURI, cookieData, nsnull);
    *aCookie = ToNewCString(cookieData);
  }

  return NS_OK;
}

NS_IMETHODIMP
nsCookieService::SetCookieString(nsIURI     *aHostURI,
                                 nsIPrompt  *aPrompt,
                                 const char *aCookieHeader,
                                 nsIChannel *aChannel)
{
  // try to determine first party URI
  nsCOMPtr<nsIURI> firstURI;

  if (aChannel) {
    nsCOMPtr<nsIHttpChannelInternal> httpInternal = do_QueryInterface(aChannel);
    if (httpInternal)
      httpInternal->GetDocumentURI(getter_AddRefs(firstURI));
  }

  return SetCookieStringFromHttp(aHostURI, firstURI, aPrompt, aCookieHeader, nsnull, aChannel);
}

NS_IMETHODIMP
nsCookieService::SetCookieStringFromHttp(nsIURI     *aHostURI,
                                         nsIURI     *aFirstURI,
                                         nsIPrompt  *aPrompt,
                                         const char *aCookieHeader,
                                         const char *aServerTime,
                                         nsIChannel *aChannel) 
{
  if (!aHostURI) {
    COOKIE_LOGFAILURE(SET_COOKIE, nsnull, aCookieHeader, "host URI is null");
    return NS_OK;
  }

  // check default prefs
  PRUint32 listPermission;
  nsCookieStatus cookieStatus = CheckPrefs(aHostURI, aFirstURI, aChannel, aCookieHeader, listPermission);
  // update UI icon, and return, if cookie was rejected.
  // should we be doing this just for p3p?
  if (cookieStatus == nsICookie::STATUS_REJECTED) {
    UpdateCookieIcon();
    return NS_OK;
  }
  // get the site's p3p policy now (common to all cookies)
  nsCookiePolicy cookiePolicy = GetP3PPolicy(SiteP3PPolicy(aHostURI, aChannel));

  // parse server local time. this is not just done here for efficiency
  // reasons - if there's an error parsing it, and we need to default it
  // to the current time, we must do it here since the current time in
  // SetCookieInternal() will change for each cookie processed (e.g. if the
  // user is prompted).
  nsInt64 serverTime;
  PRTime tempServerTime;
  if (aServerTime && PR_ParseTimeString(aServerTime, PR_TRUE, &tempServerTime) == PR_SUCCESS) {
    serverTime = nsInt64(tempServerTime) / USEC_PER_SEC;
  } else {
    serverTime = NOW_IN_SECONDS;
  }

  // switch to a nice string type now, and process each cookie in the header
  nsDependentCString cookieHeader(aCookieHeader);
  while (SetCookieInternal(aHostURI,
                           cookieHeader, serverTime,
                           cookieStatus, cookiePolicy, listPermission));

  // write out the cookie file
  LazyWrite(PR_TRUE);
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
nsCookieService::DoLazyWrite(nsITimer *aTimer,
                             void     *aClosure)
{
  nsCookieService *service = NS_REINTERPRET_CAST(nsCookieService*, aClosure);
  service->mWritePending = PR_FALSE;
  service->Write();
}

NS_IMETHODIMP 
nsCookieService::OnStateChange(nsIWebProgress *aWebProgress, 
                               nsIRequest     *aRequest, 
                               PRUint32       aProgressStateFlags, 
                               nsresult       aStatus)
{
  if (aProgressStateFlags & STATE_IS_NETWORK) {
    if (aProgressStateFlags & STATE_START)
      ++mLoadCount;
    if (aProgressStateFlags & STATE_STOP) {
      if (mLoadCount > 0) // needed because at startup we may miss initial STATE_START
        --mLoadCount;
      if (mLoadCount == 0)
        LazyWrite(PR_FALSE);
    }
  }

  if (mObserverService &&
      (aProgressStateFlags & STATE_IS_DOCUMENT) &&
      (aProgressStateFlags & STATE_STOP)) {
    mObserverService->NotifyObservers(nsnull, "cookieChanged", NS_LITERAL_STRING("cookies").get());
  }

  return NS_OK;
}

void
nsCookieService::UpdateCookieIcon()
{
  mCookieIconVisible = PR_TRUE;
  if (mObserverService) {
    mObserverService->NotifyObservers(nsnull, "cookieIcon", NS_LITERAL_STRING("on").get());
  }
}

NS_IMETHODIMP
nsCookieService::GetCookieIconIsVisible(PRBool *aIsVisible)
{
  *aIsVisible = mCookieIconVisible;
  return NS_OK;
}

// nsIWebProgressListener implementation
NS_IMETHODIMP
nsCookieService::OnProgressChange(nsIWebProgress *aProgress,
                                  nsIRequest     *aRequest,
                                  PRInt32        aCurSelfProgress,
                                  PRInt32        aMaxSelfProgress,
                                  PRInt32        aCurTotalProgress,
                                  PRInt32        aMaxTotalProgress)
{
  NS_NOTREACHED("notification excluded in AddProgressListener(...)");
  return NS_OK;
}

NS_IMETHODIMP
nsCookieService::OnLocationChange(nsIWebProgress *aWebProgress,
                                  nsIRequest     *aRequest,
                                  nsIURI         *aLocation)
{
  NS_NOTREACHED("notification excluded in AddProgressListener(...)");
  return NS_OK;
}

NS_IMETHODIMP 
nsCookieService::OnStatusChange(nsIWebProgress  *aWebProgress,
                                nsIRequest      *aRequest,
                                nsresult        aStatus,
                                const PRUnichar *aMessage)
{
  NS_NOTREACHED("notification excluded in AddProgressListener(...)");
  return NS_OK;
}

NS_IMETHODIMP 
nsCookieService::OnSecurityChange(nsIWebProgress *aWebProgress, 
                                  nsIRequest     *aRequest, 
                                  PRUint32       aState)
{
  NS_NOTREACHED("notification excluded in AddProgressListener(...)");
  return NS_OK;
}

/******************************************************************************
 * nsCookieService:
 * pref observer impl
 ******************************************************************************/

void
nsCookieService::InitPrefObservers()
{
  nsresult rv;

  // install and cache the preferences observer
  mPrefBranch = do_GetService(NS_PREFSERVICE_CONTRACTID, &rv);
  if (NS_SUCCEEDED(rv)) {
    nsCOMPtr<nsIPrefBranchInternal> prefInternal = do_QueryInterface(mPrefBranch, &rv);

    // add observers
    if (NS_SUCCEEDED(rv)) {
#ifdef MOZ_PHOENIX
      prefInternal->AddObserver(kCookiesEnabled, this, PR_TRUE);
      prefInternal->AddObserver(kCookiesForDomainOnly, this, PR_TRUE);
      prefInternal->AddObserver(kCookiesLifetimeCurrentSession, this, PR_TRUE);
#else
      prefInternal->AddObserver(kCookiesPermissions, this, PR_TRUE);
      prefInternal->AddObserver(kCookiesDisabledForMailNews, this, PR_TRUE);
      prefInternal->AddObserver(kCookiesLifetimeEnabled, this, PR_TRUE);
      prefInternal->AddObserver(kCookiesLifetimeDays, this, PR_TRUE);
      prefInternal->AddObserver(kCookiesLifetimeCurrentSession, this, PR_TRUE);
      prefInternal->AddObserver(kCookiesP3PString, this, PR_TRUE);
#endif
      prefInternal->AddObserver(kCookiesAskPermission, this, PR_TRUE);
      prefInternal->AddObserver(kCookiesStrictDomains, this, PR_TRUE);
    }

    // initialize prefs
    rv = ReadPrefs();
    if (NS_FAILED(rv)) {
      NS_WARNING("Error occured reading cookie preferences");
    }

  } else {
    // only called if getting the prefbranch failed.
#ifdef MOZ_PHOENIX
    mCookiesDisabledForMailNews = PR_FALSE; // for efficiency
#else
    mCookiesDisabledForMailNews = PR_TRUE;
    mCookiesP3PString = NS_LITERAL_CSTRING(kCookiesP3PString_Default);
#endif
    mCookiesPermissions = BEHAVIOR_REJECT;
    mCookiesLifetimeEnabled = PR_FALSE;
    mCookiesAskPermission = PR_FALSE;
    mCookiesStrictDomains = PR_FALSE;
  }
}

nsresult
nsCookieService::ReadPrefs()
{
  nsresult rv, rv2 = NS_OK;

  PRInt32 tempPrefValue;
#ifdef MOZ_PHOENIX
  rv = mPrefBranch->GetBoolPref(kCookiesEnabled, &tempPrefValue);
  if (NS_FAILED(rv)) {
    tempPrefValue = PR_FALSE;
    rv2 = rv;
  }
  mCookiesEnabled_temp = tempPrefValue;

  rv = mPrefBranch->GetBoolPref(kCookiesForDomainOnly, &tempPrefValue);
  if (NS_FAILED(rv)) {
    tempPrefValue = PR_FALSE;
    rv2 = rv;
  }
  mCookiesForDomainOnly_temp = tempPrefValue;

  // collapse two boolean prefs into enumerated permissions
  // note: BEHAVIOR_P3P is not used in Phoenix
  if (mCookiesEnabled_temp) {
    // check if user wants cookies only for site domain
    if (mCookiesForDomainOnly_temp) {
      mCookiesPermissions = BEHAVIOR_REJECTFOREIGN;
    } else {
      mCookiesPermissions = BEHAVIOR_ACCEPT;
    }
  } else {
    mCookiesPermissions = BEHAVIOR_REJECT;
  }

  rv = mPrefBranch->GetBoolPref(kCookiesLifetimeCurrentSession, &tempPrefValue);
  if (NS_FAILED(rv)) {
    tempPrefValue = PR_FALSE;
    rv2 = rv;
  }
  mCookiesLifetimeCurrentSession = tempPrefValue;
  // Phoenix hacks to reduce ifdefs in code
  mCookiesLifetimeEnabled = mCookiesLifetimeCurrentSession;
  mCookiesDisabledForMailNews = PR_FALSE;
  mCookiesLifetimeSec = 0;

#else
  rv = mPrefBranch->GetIntPref(kCookiesPermissions, &tempPrefValue);
  if (NS_FAILED(rv)) {
    tempPrefValue = BEHAVIOR_REJECT;
    rv2 = rv;
  }
  mCookiesPermissions = tempPrefValue;

  rv = mPrefBranch->GetBoolPref(kCookiesDisabledForMailNews, &tempPrefValue);
  if (NS_FAILED(rv)) {
    tempPrefValue = PR_TRUE;
    rv2 = rv;
  }
  mCookiesDisabledForMailNews = tempPrefValue;

  rv = mPrefBranch->GetBoolPref(kCookiesLifetimeEnabled, &tempPrefValue);
  if (NS_FAILED(rv)) {
    tempPrefValue = PR_FALSE;
    rv2 = rv;
  }
  mCookiesLifetimeEnabled = tempPrefValue;

  rv = mPrefBranch->GetIntPref(kCookiesLifetimeDays, &mCookiesLifetimeSec);
  if (NS_FAILED(rv)) {
    mCookiesLifetimeEnabled = PR_FALSE; // disable lifetime limit...
    mCookiesLifetimeSec = 0;
    rv2 = rv;
  }
  // save cookie lifetime in seconds instead of days
  mCookiesLifetimeSec *= 24*60*60;

  rv = mPrefBranch->GetIntPref(kCookiesLifetimeCurrentSession, &tempPrefValue);
  if (NS_FAILED(rv)) {
    tempPrefValue = 1; // disable currentSession limit
    rv2 = rv;
  }
  mCookiesLifetimeCurrentSession = (tempPrefValue == 0);

  // P3P prefs
  rv = mPrefBranch->GetCharPref(kCookiesP3PString, getter_Copies(mCookiesP3PString));
  // check for a malformed string
  if (NS_FAILED(rv) || mCookiesP3PString.Length() != 8) {
    // reassign to default string
    mCookiesP3PString = NS_LITERAL_CSTRING(kCookiesP3PString_Default);
    rv2 = rv;
  }

#endif
  // common prefs between Phoenix & Mozilla
  rv = mPrefBranch->GetBoolPref(kCookiesAskPermission, &tempPrefValue);
  if (NS_FAILED(rv)) {
    tempPrefValue = PR_FALSE;
    rv2 = rv;
  }
  mCookiesAskPermission = tempPrefValue;

  rv = mPrefBranch->GetBoolPref(kCookiesStrictDomains, &tempPrefValue);
  if (NS_FAILED(rv)) {
    tempPrefValue = PR_FALSE;
    // we don't update rv2 here like we do for other prefs, since this pref
    // is optional (most profiles won't have it set), and ReadPrefs' caller
    // will NS_WARNING on NS_FAILED(rv2). so this is a little bit quieter...
  }
  mCookiesStrictDomains = tempPrefValue;

  return rv2;
}

/******************************************************************************
 * nsICookieManager impl:
 * nsCookieEnumerator
 ******************************************************************************/

class nsCookieEnumerator : public nsISimpleEnumerator
{
  public:
    NS_DECL_ISUPPORTS

    // note: mCookieCount is initialized just once in the ctor. While it might
    // appear that the cookie list can change while the cookiemanager is running,
    // the cookieservice is actually on the same thread, so it can't. Note that
    // a new nsCookieEnumerator is created each time the cookiemanager is loaded.
    // So we only need to get the count once. If we ever change the cookieservice to
    // run on a different thread, then something to the effect of a lock will be
    // required. see bug 191682 for details.
    nsCookieEnumerator(const nsVoidArray &aCookieList)
     : mCookieList(aCookieList)
     , mCookieCount(aCookieList.Count())
     , mCookieIndex(0)
    {
    }

    NS_IMETHOD
    HasMoreElements(PRBool *aResult) 
    {
      *aResult = mCookieIndex < mCookieCount;
      return NS_OK;
    }

    NS_IMETHOD
    GetNext(nsISupports **aResult) 
    {
      if (mCookieIndex >= mCookieCount) {
        *aResult = nsnull;
        NS_ERROR("bad cookie index");
        return NS_ERROR_UNEXPECTED;
      }

      // cast the nsCookie to an nsICookie
      nsICookie *cookieInList = NS_STATIC_CAST(nsICookie*, NS_STATIC_CAST(nsCookie*, mCookieList.ElementAt(mCookieIndex++)));
      NS_ASSERTION(cookieInList, "corrupt cookie list");

      *aResult = cookieInList;
      NS_ADDREF(cookieInList);

      return NS_OK;
    }

    virtual ~nsCookieEnumerator() 
    {
    }

  protected:
    const nsVoidArray &mCookieList;
    PRInt32           mCookieCount;
    PRInt32           mCookieIndex;
};

NS_IMPL_ISUPPORTS1(nsCookieEnumerator, nsISimpleEnumerator)

/******************************************************************************
 * nsICookieManager impl:
 * nsICookieManager
 ******************************************************************************/

NS_IMETHODIMP
nsCookieService::RemoveAll()
{
  RemoveAllFromMemory();
  Write();
  return NS_OK;
}

NS_IMETHODIMP
nsCookieService::GetEnumerator(nsISimpleEnumerator **aEnumerator)
{
  PRInt32 temp;
  RemoveExpiredCookies(NOW_IN_SECONDS, temp);

  nsCookieEnumerator* enumerator = new nsCookieEnumerator(mCookieList);
  if (!enumerator) {
    *aEnumerator = nsnull;
    return NS_ERROR_OUT_OF_MEMORY;
  }

  NS_ADDREF(enumerator);
  *aEnumerator = enumerator;
  return NS_OK;
}

NS_IMETHODIMP
nsCookieService::Add(const nsACString &aDomain,
                     const nsACString &aPath,
                     const nsACString &aName,
                     const nsACString &aValue,
                     PRBool           aIsSecure,
                     PRInt32          aExpires)
{
  nsInt64 currentTime = NOW_IN_SECONDS;

  nsRefPtr<nsCookie> cookie =
    new nsCookie(aName, aValue, aDomain, aPath,
                 nsInt64(aExpires), currentTime,
                 nsInt64(aExpires) == nsInt64(0), PR_TRUE, aIsSecure,
                 nsICookie::STATUS_UNKNOWN,
                 nsICookie::POLICY_UNKNOWN);
  if (!cookie) {
    return NS_ERROR_OUT_OF_MEMORY;
  }

  AddInternal(cookie, currentTime, nsnull, nsnull);
  return NS_OK;
}

NS_IMETHODIMP
nsCookieService::Remove(const nsACString &aHost,
                        const nsACString &aName,
                        const nsACString &aPath,
                        PRBool           aBlocked)
{
  nsCookie *cookieInList;

  // step through all cookies, searching for indicated one
  PRInt32 count = mCookieList.Count();
  for (PRInt32 i = 0; i < count; ++i) {
    cookieInList = NS_STATIC_CAST(nsCookie*, mCookieList.ElementAt(i));
    NS_ASSERTION(cookieInList, "corrupt cookie list");

    if (cookieInList->Path().Equals(aPath) &&
        cookieInList->Host().Equals(aHost) &&
        cookieInList->Name().Equals(aName)) {
      // check if we need to add the host to the permissions blacklist.
      // we should push this portion into the UI, it shouldn't live here in the backend.
      if (aBlocked) {
        nsresult rv;
        nsCOMPtr<nsIPermissionManager> permissionManager = do_GetService(NS_PERMISSIONMANAGER_CONTRACTID, &rv);

        if (NS_SUCCEEDED(rv)) {
          nsCOMPtr<nsIURI> uri;
          static NS_NAMED_LITERAL_CSTRING(httpPrefix, "http://");

          // remove leading dot from host
          if (cookieInList->IsDomain()) {
            rv = NS_NewURI(getter_AddRefs(uri), PromiseFlatCString(httpPrefix + Substring(cookieInList->Host(), 1, cookieInList->Host().Length() - 1)));
          } else {
            rv = NS_NewURI(getter_AddRefs(uri), PromiseFlatCString(httpPrefix + cookieInList->Host()));
          }

          if (NS_SUCCEEDED(rv))
            permissionManager->Add(uri, "cookie", nsIPermissionManager::DENY_ACTION);
        }
      }

      mCookieList.RemoveElementAt(i);
      NS_RELEASE(cookieInList);
      mCookieChanged = PR_TRUE;
      // we might want to eventually push this Write() call into the UI,
      // to just write once on cookiemanager close.
      Write();
      break;
    }
  }

  return NS_OK;
}

/******************************************************************************
 * nsCookieService impl:
 * private file I/O functions
 ******************************************************************************/

// comparison function for sorting cookies by path length:
// returns < 0 if the first element has a greater path length than the second element,
//           0 if they both have the same path length,
//         > 0 if the second element has a greater path length than the first element.
PR_STATIC_CALLBACK(int)
compareCookiesByPath(const void *aElement1,
                     const void *aElement2,
                     void       *aData)
{
  const nsCookie *cookie1 = NS_STATIC_CAST(const nsCookie*, aElement1);
  const nsCookie *cookie2 = NS_STATIC_CAST(const nsCookie*, aElement2);
  NS_ASSERTION(cookie1 && cookie2, "corrupt cookie list");

  return cookie2->Path().Length() - cookie1->Path().Length();
}

// comparison function for sorting cookies by lastAccessed time:
// returns < 0 if the first element was used more recently than the second element,
//           0 if they both have the same last-use time,
//         > 0 if the second element was used more recently than the first element.
PR_STATIC_CALLBACK(int)
compareCookiesByLRU(const void *aElement1,
                    const void *aElement2,
                    void       *aData)
{
  const nsCookie *cookie1 = NS_STATIC_CAST(const nsCookie*, aElement1);
  const nsCookie *cookie2 = NS_STATIC_CAST(const nsCookie*, aElement2);
  NS_ASSERTION(cookie1 && cookie2, "corrupt cookie list");

  // we may have overflow problems returning the result directly, so we need branches
  nsInt64 difference = cookie2->LastAccessed() - cookie1->LastAccessed();
  return (difference > nsInt64(0)) ? 1 : (difference < nsInt64(0)) ? -1 : 0;
}

nsresult
nsCookieService::Read()
{
  nsresult rv;
  nsCOMPtr<nsIInputStream> fileInputStream;
  rv = NS_NewLocalFileInputStream(getter_AddRefs(fileInputStream), mCookieFile);
  if (NS_FAILED(rv)) {
    return rv;
  }

  nsCOMPtr<nsILineInputStream> lineInputStream = do_QueryInterface(fileInputStream, &rv);
  if (NS_FAILED(rv)) {
    return rv;
  }

  // presize mCookieList to the maximum size, to avoid excessive malloc's.
  // we'll compact it when we're done reading
  mCookieList.SizeTo(kMaxNumberOfCookies);

  static NS_NAMED_LITERAL_CSTRING(kTrue, "TRUE");

  nsAutoString bufferUnicode;
  nsCAutoString buffer;
  PRBool isMore = PR_TRUE;
  PRInt32 hostIndex = 0, isDomainIndex, pathIndex, secureIndex, expiresIndex, nameIndex, cookieIndex;
  nsASingleFragmentCString::char_iterator iter;
  PRInt32 numInts;
  PRInt64 expires;
  PRBool isDomain;
  nsInt64 currentTime = NOW_IN_SECONDS;
  // we use lastAccessedCounter to keep cookies in recently-used order,
  // so we start by initializing to currentTime (somewhat arbitrary)
  nsInt64 lastAccessedCounter = currentTime;
  nsCookie *newCookie;

  /* file format is:
   *
   * host \t isDomain \t path \t secure \t expires \t name \t cookie
   *
   * if this format isn't respected we move onto the next line in the file.
   * isDomain is "TRUE" or "FALSE" (default to "FALSE")
   * isSecure is "TRUE" or "FALSE" (default to "TRUE")
   * expires is a PRInt64 integer
   * note 1: cookie can contain tabs.
   * note 2: cookies are written in order of lastAccessed time:
   *         most-recently used come first; least-recently-used come last.
   */

  while (isMore && NS_SUCCEEDED(lineInputStream->ReadLine(bufferUnicode, &isMore))) {
    // downconvert to ASCII. eventually, we want to fix nsILineInputStream
    // to operate on a CString buffer...
    CopyUCS2toASCII(bufferUnicode, buffer);

    if (buffer.IsEmpty() || buffer.First() == '#') {
      continue;
    }

    // this is a cheap, cheesy way of parsing a tab-delimited line into
    // string indexes, which can be lopped off into substrings. just for
    // purposes of obfuscation, it also checks that each token was found.
    // todo: use iterators?
    if ((isDomainIndex = buffer.FindChar('\t', hostIndex)     + 1) == 0 ||
        (pathIndex     = buffer.FindChar('\t', isDomainIndex) + 1) == 0 ||
        (secureIndex   = buffer.FindChar('\t', pathIndex)     + 1) == 0 ||
        (expiresIndex  = buffer.FindChar('\t', secureIndex)   + 1) == 0 ||
        (nameIndex     = buffer.FindChar('\t', expiresIndex)  + 1) == 0 ||
        (cookieIndex   = buffer.FindChar('\t', nameIndex)     + 1) == 0) {
      continue;
    }

    // check the expirytime first - if it's expired, ignore
    // nullstomp the trailing tab, to avoid copying the string
    buffer.BeginWriting(iter);
    *(iter += nameIndex - 1) = char(0);
    numInts = PR_sscanf(buffer.get() + expiresIndex, "%lld", &expires);
    if (numInts != 1 || nsInt64(expires) < currentTime) {
      continue;
    }

    isDomain = Substring(buffer, isDomainIndex, pathIndex - isDomainIndex - 1).Equals(kTrue);
    const nsASingleFragmentCString &host = Substring(buffer, hostIndex, isDomainIndex - hostIndex - 1);
    // check for bad legacy cookies (domain not starting with a dot, or containing a port),
    // and discard
    if (isDomain && !host.IsEmpty() && host.First() != '.' ||
        host.FindChar(':') != kNotFound) {
      continue;
    }

    // create a new nsCookie and assign the data
    newCookie =
      new nsCookie(Substring(buffer, nameIndex, cookieIndex - nameIndex - 1),
                   Substring(buffer, cookieIndex, buffer.Length() - cookieIndex),
                   host,
                   Substring(buffer, pathIndex, secureIndex - pathIndex - 1),
                   nsInt64(expires),
                   lastAccessedCounter,
                   PR_FALSE,
                   isDomain,
                   Substring(buffer, secureIndex, expiresIndex - secureIndex - 1).Equals(kTrue),
                   nsICookie::STATUS_UNKNOWN,
                   nsICookie::POLICY_UNKNOWN);
    if (!newCookie) {
      return NS_ERROR_OUT_OF_MEMORY;
    }

    // trick: keep the cookies in most-recently-used order,
    // by successively decrementing the lastAccessed time
    lastAccessedCounter -= nsInt64(1);

    // add new cookie to the list
    mCookieList.AppendElement(newCookie);
    NS_ADDREF(newCookie);
  }

  // compact the array, now that we're done reading data
  mCookieList.Compact();
  // sort the list in order of descending path length
  mCookieList.Sort(compareCookiesByPath, nsnull);

  mCookieChanged = PR_FALSE;
  return NS_OK;
}

nsresult
nsCookieService::Write()
{
  if (!mCookieChanged) {
    return NS_OK;
  }

  nsresult rv;
  nsCOMPtr<nsIOutputStream> fileOutputStream;
  rv = NS_NewLocalFileOutputStream(getter_AddRefs(fileOutputStream), mCookieFile);
  if (NS_FAILED(rv)) {
    NS_ERROR("failed to open cookies.txt for writing");
    return rv;
  }

  // get a buffered output stream 4096 bytes big, to optimize writes
  nsCOMPtr<nsIOutputStream> bufferedOutputStream;
  rv = NS_NewBufferedOutputStream(getter_AddRefs(bufferedOutputStream), fileOutputStream, 4096);
  if (NS_FAILED(rv)) {
    return rv;
  }

  static const char kHeader[] =
      "# HTTP Cookie File\n"
      "# http://www.netscape.com/newsref/std/cookie_spec.html\n"
      "# This is a generated file!  Do not edit.\n"
      "# To delete cookies, use the Cookie Manager.\n\n";
  // note: kTrue and kFalse have leading/trailing tabs already added
  static const char kTrue[] = "\tTRUE\t";
  static const char kFalse[] = "\tFALSE\t";
  static const char kTab[] = "\t";
  static const char kNew[] = "\n";

  // create a new nsVoidArray to hold the cookie list, and sort it
  // such that least-recently-used cookies come last
  nsVoidArray sortedCookieList;
  sortedCookieList = mCookieList;
  sortedCookieList.Sort(compareCookiesByLRU, nsnull);

  bufferedOutputStream->Write(kHeader, sizeof(kHeader) - 1, &rv);

  /* file format is:
   *
   * host \t isDomain \t path \t secure \t expires \t name \t cookie
   *
   * isDomain is "TRUE" or "FALSE"
   * isSecure is "TRUE" or "FALSE"
   * expires is a PRInt64 integer
   * note 1: cookie can contain tabs.
   * note 2: cookies are written in order of lastAccessed time:
   *         most-recently used come first; least-recently-used come last.
   */
  nsCookie *cookieInList;
  nsInt64 currentTime = NOW_IN_SECONDS;
  char dateString[22];
  PRUint32 dateLen;
  PRInt32 count = sortedCookieList.Count();
  for (PRInt32 i = 0; i < count; ++i) {
    cookieInList = NS_STATIC_CAST(nsCookie*, sortedCookieList.ElementAt(i));
    NS_ASSERTION(cookieInList, "corrupt cookie list");

    // don't write entry if cookie has expired, or is a session cookie
    if (cookieInList->IsSession() || cookieInList->Expiry() <= currentTime) {
      continue;
    }

    bufferedOutputStream->Write(cookieInList->Host().get(), cookieInList->Host().Length(), &rv);
    if (cookieInList->IsDomain()) {
      bufferedOutputStream->Write(kTrue, sizeof(kTrue) - 1, &rv);
    } else {
      bufferedOutputStream->Write(kFalse, sizeof(kFalse) - 1, &rv);
    }
    bufferedOutputStream->Write(cookieInList->Path().get(), cookieInList->Path().Length(), &rv);
    if (cookieInList->IsSecure()) {
      bufferedOutputStream->Write(kTrue, sizeof(kTrue) - 1, &rv);
    } else {
      bufferedOutputStream->Write(kFalse, sizeof(kFalse) - 1, &rv);
    }
    dateLen = PR_snprintf(dateString, sizeof(dateString), "%lld", PRInt64(cookieInList->Expiry()));
    bufferedOutputStream->Write(dateString, dateLen, &rv);
    bufferedOutputStream->Write(kTab, sizeof(kTab) - 1, &rv);
    bufferedOutputStream->Write(cookieInList->Name().get(), cookieInList->Name().Length(), &rv);
    bufferedOutputStream->Write(kTab, sizeof(kTab) - 1, &rv);
    bufferedOutputStream->Write(cookieInList->Value().get(), cookieInList->Value().Length(), &rv);
    bufferedOutputStream->Write(kNew, sizeof(kNew) - 1, &rv);
  }

  mCookieChanged = PR_FALSE;
  return NS_OK;
}

/******************************************************************************
 * nsCookieService impl:
 * private GetCookie/SetCookie helpers
 ******************************************************************************/

// processes a single cookie, and returns PR_TRUE if there are more cookies
// to be processed
PRBool
nsCookieService::SetCookieInternal(nsIURI             *aHostURI,
                                   nsDependentCString &aCookieHeader,
                                   nsInt64            aServerTime,
                                   nsCookieStatus     aStatus,
                                   nsCookiePolicy     aPolicy,
                                   PRUint32           aListPermission)
{
  nsresult rv;

  // keep a |const char*| version of the unmodified aCookieHeader,
  // for logging purposes
  const char *cookieHeader = aCookieHeader.get();

  // create a stack-based nsCookieAttributes, to store all the
  // attributes parsed from the cookie
  nsCookieAttributes cookieAttributes;

  // newCookie says whether there are multiple cookies in the header; so we can handle them separately.
  // after this function, we don't need the cookieHeader string for processing this cookie anymore;
  // so this function uses it as an outparam to point to the next cookie, if there is one.
  const PRBool newCookie = ParseAttributes(aCookieHeader, cookieAttributes);

  // reject cookie if it's over the size limit, per RFC2109
  if ((cookieAttributes.name.Length() + cookieAttributes.value.Length()) > kMaxBytesPerCookie) {
    COOKIE_LOGFAILURE(SET_COOKIE, aHostURI, cookieHeader, "cookie too big (> 4kb)");
    return newCookie;
  }

  // calculate expiry time of cookie. we need to pass in cookieStatus, since
  // the cookie may have been downgraded to a session cookie by p3p.
  const nsInt64 currentTime = NOW_IN_SECONDS;
  cookieAttributes.isSession = GetExpiry(cookieAttributes, aServerTime,
                                         currentTime, aStatus);

  // domain & path checks
  if (!CheckDomain(cookieAttributes, aHostURI)) {
    COOKIE_LOGFAILURE(SET_COOKIE, aHostURI, cookieHeader, "failed the domain tests");
    return newCookie;
  }
  if (!CheckPath(cookieAttributes, aHostURI)) {
    COOKIE_LOGFAILURE(SET_COOKIE, aHostURI, cookieHeader, "failed the path tests");
    return newCookie;
  }

  // create a new nsCookie and copy attributes
  nsRefPtr<nsCookie> cookie =
    new nsCookie(cookieAttributes.name,
                 cookieAttributes.value,
                 cookieAttributes.host,
                 cookieAttributes.path,
                 cookieAttributes.expiryTime,
                 currentTime,
                 cookieAttributes.isSession,
                 cookieAttributes.isDomain,
                 cookieAttributes.isSecure,
                 aStatus,
                 aPolicy);
  if (!cookie) {
    COOKIE_LOGFAILURE(SET_COOKIE, aHostURI, cookieHeader, "unable to allocate memory for new cookie");
    return newCookie;
  }

  // count the number of cookies from this host, and find whether a previous cookie
  // has been set, for prompting purposes
  PRUint32 countFromHost;
  const PRBool foundCookie = FindCookiesFromHost(cookie, countFromHost, currentTime);

  // check if the cookie we're trying to set is already expired, and return.
  // but we need to check there's no previous cookie first, because servers use
  // this as a trick for deleting previous cookies.
  if (!foundCookie && !cookie->IsSession() && cookie->Expiry() <= currentTime) {
    COOKIE_LOGFAILURE(SET_COOKIE, aHostURI, cookieHeader, "cookie has already expired");
    return newCookie;
  }

  // check permissions from site permission list, or ask the user,
  // to determine if we can set the cookie
  if (mPermissionService) {
    PRBool permission;
    // we need to think about prompters/parent windows here - TestPermission
    // needs one to prompt, so right now it has to fend for itself to get one
    mPermissionService->TestPermission(aHostURI,
                                       NS_STATIC_CAST(nsICookie*, NS_STATIC_CAST(nsCookie*, cookie)),
                                       nsnull,
                                       countFromHost, foundCookie,
                                       mCookiesAskPermission,
                                       aListPermission,
                                       &permission);
    if (!permission) {
      COOKIE_LOGFAILURE(SET_COOKIE, aHostURI, cookieHeader, "cookie rejected by permission manager");
      return newCookie;
    }
  }

  // add the cookie to the list
  rv = AddInternal(cookie, NOW_IN_SECONDS, aHostURI, cookieHeader);
  if (NS_FAILED(rv)) {
    // no need to log a failure here, AddInternal() does it for us
    return newCookie;
  }

  // notify observers if the cookie was downgraded or flagged (only for p3p
  // at this point). this occurs only if the cookie was set successfully.
  if (aStatus == nsICookie::STATUS_DOWNGRADED ||
      aStatus == nsICookie::STATUS_FLAGGED) {
    UpdateCookieIcon();
  }

  COOKIE_LOGSUCCESS(SET_COOKIE, aHostURI, cookieHeader, cookie);
  return newCookie;
}

// this is a backend function for adding a cookie to the list, via SetCookie.
// also used in the cookie manager, for profile migration from IE.
// returns NS_OK if aCookie was added to the list; NS_ERROR otherwise.
nsresult
nsCookieService::AddInternal(nsCookie   *aCookie,
                             nsInt64    aCurrentTime,
                             nsIURI     *aHostURI,
                             const char *aCookieHeader)
{
  // find a position to insert the cookie at (and delete a cookie from, if necessary).
  // also removes expired cookies from the list, for maintenance purposes.
  PRInt32 insertPosition, deletePosition;
  PRBool foundCookie = FindPosition(aCookie, insertPosition, deletePosition, aCurrentTime);

  // store the cookie
  if (foundCookie) {
    nsCookie *prevCookie = NS_STATIC_CAST(nsCookie*, mCookieList.ElementAt(insertPosition));
    NS_ASSERTION(prevCookie, "corrupt cookie list");
    NS_RELEASE(prevCookie);

    // check if the server wants to delete the cookie
    if (!aCookie->IsSession() && aCookie->Expiry() <= aCurrentTime) {
      // delete previous cookie
      mCookieList.RemoveElementAt(insertPosition);

      COOKIE_LOGFAILURE(SET_COOKIE, aHostURI, aCookieHeader, "previously stored cookie was deleted");
      mCookieChanged = PR_TRUE;
      return NS_ERROR_FAILURE;

    } else {
      // replace previous cookie
      mCookieList.ReplaceElementAt(aCookie, insertPosition);
      NS_ADDREF(aCookie);
    }

  } else {
    // check if cookie has already expired
    if (!aCookie->IsSession() && aCookie->Expiry() <= aCurrentTime) {
      COOKIE_LOGFAILURE(SET_COOKIE, aHostURI, aCookieHeader, "cookie has already expired");
      return NS_ERROR_FAILURE;
    }

    // add the cookie, which means we might have to delete an old cookie
    if (deletePosition != -1) {
      nsCookie *deleteCookie = NS_STATIC_CAST(nsCookie*, mCookieList.ElementAt(deletePosition));
      NS_ASSERTION(deleteCookie, "corrupt cookie list");

      mCookieList.RemoveElementAt(deletePosition);
      NS_RELEASE(deleteCookie);

      // adjust insertPosition if we removed a cookie before it
      if (insertPosition > deletePosition) {
        --insertPosition;
      }
    }

    mCookieList.InsertElementAt(aCookie, insertPosition);
    NS_ADDREF(aCookie);
  }

  mCookieChanged = PR_TRUE;
  return NS_OK;
}

/******************************************************************************
 * nsCookieService impl:
 * private cookie header parsing functions
 ******************************************************************************/

// The following comment block elucidates the function of ParseAttributes.
/******************************************************************************
 ** Augmented BNF, modified from RFC2109 Section 4.2.2 and RFC2616 Section 2.1
 ** please note: this BNF deviates from both specifications, and reflects this
 ** implementation. <bnf> indicates a reference to the defined grammer "bnf".

 ** Differences from RFC2109/2616 and explanations:
    1. implied *LWS
         The grammar described by this specification is word-based. Except
         where noted otherwise, linear white space (<LWS>) can be included
         between any two adjacent words (token or quoted-string), and
         between adjacent words and separators, without changing the
         interpretation of a field.
       <LWS> according to spec is SP|HT|CR|LF, but here, we allow only SP | HT.

    2. We use CR | LF as cookie separators, not ',' per spec, since ',' is in
       common use inside values.

    3. tokens and values have looser restrictions on allowed characters than
       spec. This is also due to certain characters being in common use inside
       values. We allow only '=' to separate token/value pairs, and ';' to
       terminate tokens or values. <LWS> is allowed within tokens and values
       (see bug 206022).

    4. where appropriate, full <OCTET>s are allowed, where the spec dictates to
       reject control chars or non-ASCII chars. This is erring on the loose
       side, since there's probably no good reason to enforce this strictness.

    5. cookie <NAME> is optional, where spec requires it. This is a fairly
       trivial case, but allows the flexibility of setting only a cookie <VALUE>
       with a blank <NAME> and is required by some sites (see bug 169091).

 ** Begin BNF:
    token         = 1*<any allowed-chars except separators>
    value         = token-value | quoted-string
    token-value   = 1*<any allowed-chars except value-sep>
    quoted-string = ( <"> *( qdtext | quoted-pair ) <"> )
    qdtext        = <any allowed-chars except <">>             ; CR | LF removed by necko
    quoted-pair   = "\" <any OCTET except NUL or cookie-sep>   ; CR | LF removed by necko
    separators    = ";" | "="
    value-sep     = ";"
    cookie-sep    = CR | LF
    allowed-chars = <any OCTET except NUL or cookie-sep>
    OCTET         = <any 8-bit sequence of data>
    LWS           = SP | HT
    NUL           = <US-ASCII NUL, null control character (0)>
    CR            = <US-ASCII CR, carriage return (13)>
    LF            = <US-ASCII LF, linefeed (10)>
    SP            = <US-ASCII SP, space (32)>
    HT            = <US-ASCII HT, horizontal-tab (9)>

    set-cookie    = "Set-Cookie:" cookies
    cookies       = cookie *( cookie-sep cookie )
    cookie        = [NAME "="] VALUE *(";" cookie-av)    ; cookie NAME/VALUE must come first
    NAME          = token                                ; cookie name
    VALUE         = value                                ; cookie value
    cookie-av     = token ["=" value]

    valid values for cookie-av (checked post-parsing) are:
    cookie-av     = "Path"    "=" value
                  | "Domain"  "=" value
                  | "Expires" "=" value
                  | "Max-Age" "=" value
                  | "Comment" "=" value
                  | "Version" "=" value
                  | "Secure"

******************************************************************************/

// helper functions for GetTokenValue
static inline PRBool iswhitespace     (char c) { return c == ' '  || c == '\t'; }
static inline PRBool isterminator     (char c) { return c == '\n' || c == '\r'; }
static inline PRBool isquoteterminator(char c) { return isterminator(c) || c == '"'; }
static inline PRBool isvalueseparator (char c) { return isterminator(c) || c == ';'; }
static inline PRBool istokenseparator (char c) { return isvalueseparator(c) || c == '='; }

// Parse a single token/value pair.
// Returns PR_TRUE if a cookie terminator is found, so caller can parse new cookie.
PRBool
nsCookieService::GetTokenValue(nsASingleFragmentCString::const_char_iterator &aIter,
                               nsASingleFragmentCString::const_char_iterator &aEndIter,
                               nsDependentSingleFragmentCSubstring           &aTokenString,
                               nsDependentSingleFragmentCSubstring           &aTokenValue,
                               PRBool                                        &aEqualsFound)
{
  nsASingleFragmentCString::const_char_iterator start, lastSpace;
  // initialize value string to clear garbage
  aTokenValue.Rebind(aIter, aIter);

  // find <token>, including any <LWS> between the end-of-token and the
  // token separator. we'll remove trailing <LWS> next
  while (aIter != aEndIter && iswhitespace(*aIter))
    ++aIter;
  start = aIter;
  while (aIter != aEndIter && !istokenseparator(*aIter))
    ++aIter;

  // remove trailing <LWS>; first check we're not at the beginning
  lastSpace = aIter;
  if (lastSpace != start) {
    while (--lastSpace != start && iswhitespace(*lastSpace));
    ++lastSpace;
  }
  aTokenString.Rebind(start, lastSpace);

  aEqualsFound = (*aIter == '=');
  if (aEqualsFound) {
    // find <value>
    while (++aIter != aEndIter && iswhitespace(*aIter));

    start = aIter;

    if (*aIter == '"') {
      // process <quoted-string>
      // (note: cookie terminators, CR | LF, can't happen:
      // they're removed by necko before the header gets here)
      // assume value mangled if no terminating '"', return
      while (++aIter != aEndIter && !isquoteterminator(*aIter)) {
        // if <qdtext> (backwhacked char), skip over it. this allows '\"' in <quoted-string>.
        // we increment once over the backwhack, nullcheck, then continue to the 'while',
        // which increments over the backwhacked char. one exception - we don't allow
        // CR | LF here either (see above about necko)
        if (*aIter == '\\' && (++aIter == aEndIter || isterminator(*aIter)))
          break;
      }

      if (aIter != aEndIter && !isterminator(*aIter)) {
        // include terminating quote in attribute string
        aTokenValue.Rebind(start, ++aIter);
        // skip to next ';'
        while (aIter != aEndIter && !isvalueseparator(*aIter))
          ++aIter;
      }
    } else {
      // process <token-value>
      // just look for ';' to terminate ('=' allowed)
      while (aIter != aEndIter && !isvalueseparator(*aIter))
        ++aIter;

      // remove trailing <LWS>; first check we're not at the beginning
      if (aIter != start) {
        lastSpace = aIter;
        while (--lastSpace != start && iswhitespace(*lastSpace));
        aTokenValue.Rebind(start, ++lastSpace);
      }
    }
  }

  // aIter is on ';', or terminator, or EOS
  if (aIter != aEndIter) {
    // if on terminator, increment past & return PR_TRUE to process new cookie
    if (isterminator(*aIter)) {
      ++aIter;
      return PR_TRUE;
    }
    // fall-through: aIter is on ';', increment and return PR_FALSE
    ++aIter;
  }
  return PR_FALSE;
}

// Parses attributes from cookie header. expires/max-age attributes aren't folded into the
// cookie struct here, because we don't know which one to use until we've parsed the header.
PRBool
nsCookieService::ParseAttributes(nsDependentCString &aCookieHeader,
                                 nsCookieAttributes &aCookieAttributes)
{
  static NS_NAMED_LITERAL_CSTRING(kPath,    "path"   );
  static NS_NAMED_LITERAL_CSTRING(kDomain,  "domain" );
  static NS_NAMED_LITERAL_CSTRING(kExpires, "expires");
  static NS_NAMED_LITERAL_CSTRING(kMaxage,  "max-age");
  static NS_NAMED_LITERAL_CSTRING(kSecure,  "secure" );

  nsASingleFragmentCString::const_char_iterator tempBegin, tempEnd;
  nsASingleFragmentCString::const_char_iterator cookieStart, cookieEnd;
  aCookieHeader.BeginReading(cookieStart);
  aCookieHeader.EndReading(cookieEnd);

  aCookieAttributes.isSecure = PR_FALSE;

  nsDependentSingleFragmentCSubstring tokenString(cookieStart, cookieStart);
  nsDependentSingleFragmentCSubstring tokenValue (cookieStart, cookieStart);
  PRBool newCookie, equalsFound;

  // extract cookie <NAME> & <VALUE> (first attribute), and copy the strings.
  // if we find multiple cookies, return for processing
  // note: if there's no '=', we assume token is NAME, not VALUE.
  //       the old code assumed VALUE instead.
  // note: if there's no '=', we assume token is <VALUE>. this is required by
  //       some sites (see bug 169091).
  // XXX fix the parser to parse according to <VALUE> grammar for this case
  newCookie = GetTokenValue(cookieStart, cookieEnd, tokenString, tokenValue, equalsFound);
  if (equalsFound) {
    aCookieAttributes.name = tokenString;
    aCookieAttributes.value = tokenValue;
  } else {
    aCookieAttributes.value = tokenString;
  }

  // extract remaining attributes
  while (cookieStart != cookieEnd && !newCookie) {
    newCookie = GetTokenValue(cookieStart, cookieEnd, tokenString, tokenValue, equalsFound);

    if (!tokenValue.IsEmpty() && *tokenValue.BeginReading(tempBegin) == '"'
                              && *tokenValue.EndReading(tempEnd) == '"') {
      // our parameter is a quoted-string; remove quotes for later parsing
      tokenValue.Rebind(++tempBegin, --tempEnd);
    }

    // decide which attribute we have, and copy the string
    if (tokenString.Equals(kPath, nsCaseInsensitiveCStringComparator()))
      aCookieAttributes.path = tokenValue;

    else if (tokenString.Equals(kDomain, nsCaseInsensitiveCStringComparator()))
      aCookieAttributes.host = tokenValue;

    else if (tokenString.Equals(kExpires, nsCaseInsensitiveCStringComparator()))
      aCookieAttributes.expires = tokenValue;

    else if (tokenString.Equals(kMaxage, nsCaseInsensitiveCStringComparator()))
      aCookieAttributes.maxage = tokenValue;

    // ignore any tokenValue for isSecure; just set the boolean
    else if (tokenString.Equals(kSecure, nsCaseInsensitiveCStringComparator()))
      aCookieAttributes.isSecure = PR_TRUE;
  }

  // rebind aCookieHeader, in case we need to process another cookie
  aCookieHeader.Rebind(cookieStart, cookieEnd);
  return newCookie;
}

/******************************************************************************
 * nsCookieService impl:
 * private domain & permission compliance enforcement functions
 ******************************************************************************/

// returns PR_TRUE if aHost is an IP address
PRBool
nsCookieService::IsIPAddress(const nsAFlatCString &aHost)
{
  PRNetAddr addr;
  return (PR_StringToNetAddr(aHost.get(), &addr) == PR_SUCCESS);
}

// returns PR_TRUE if URI scheme is from mailnews
PRBool
nsCookieService::IsFromMailNews(const nsAFlatCString &aScheme)
{
  return (aScheme.Equals(NS_LITERAL_CSTRING("imap"))  || 
          aScheme.Equals(NS_LITERAL_CSTRING("news"))  ||
          aScheme.Equals(NS_LITERAL_CSTRING("snews")) ||
          aScheme.Equals(NS_LITERAL_CSTRING("mailbox")));
}

PRBool
nsCookieService::IsInDomain(const nsACString &aDomain,
                            const nsACString &aHost,
                            PRBool           aIsDomain)
{
  // if we have a non-domain cookie, require an exact match between domain and host.
  // RFC2109 specifies this behavior; it allows a site to prevent its subdomains
  // from accessing a cookie, for whatever reason.
  if (!aIsDomain) {
    return aDomain.Equals(aHost);
  }

  // we have a domain cookie; test the following two cases:
  /*
   * normal case for hostName = x<domainName>
   *    e.g., hostName = home.netscape.com
   *          domainName = .netscape.com
   *
   * special case for domainName = .hostName
   *    e.g., hostName = netscape.com
   *          domainName = .netscape.com
   */
  // the lengthDifference tests are for efficiency, so we do only one .Equals()
  PRUint32 domainLength = aDomain.Length();
  PRInt32 lengthDifference = aHost.Length() - domainLength;
  // case for host & domain equal
  // (e.g. .netscape.com & .netscape.com)
  // this gives us slightly more efficiency, since we don't have
  // to call up Substring().
  if (lengthDifference == 0) {
    return aDomain.Equals(aHost);
  }
  // normal case
  if (lengthDifference > 0) {
    return aDomain.Equals(Substring(aHost, lengthDifference, domainLength));
  }
  // special case
  if (lengthDifference == -1) {
    return Substring(aDomain, 1, domainLength - 1).Equals(aHost);
  }
  // no match
  return PR_FALSE;
}

PRBool
nsCookieService::IsForeign(nsIURI *aHostURI,
                           nsIURI *aFirstURI)
{
  // if aFirstURI is null, default to not foreign
  if (!aFirstURI) {
    return PR_FALSE;
  }

  // chrome URLs are never foreign (otherwise sidebar cookies won't work).
  // eventually we want to have a protocol whitelist here,
  // _or_ do something smart with nsIProtocolHandler::protocolFlags.
  PRBool isChrome = PR_FALSE;
  nsresult rv = aFirstURI->SchemeIs("chrome", &isChrome);
  if (NS_SUCCEEDED(rv) && isChrome) {
    return PR_FALSE;
  }

  // Get hosts
  nsCAutoString currentHost, firstHost;
  if (NS_FAILED(aHostURI->GetAsciiHost(currentHost)) ||
      NS_FAILED(aFirstURI->GetAsciiHost(firstHost))) {
    return PR_TRUE;
  }
  // trim trailing dots
  currentHost.Trim(".");
  firstHost.Trim(".");
  ToLowerCase(currentHost);
  ToLowerCase(firstHost);

  // determine if it's foreign. we have a new algorithm for doing this,
  // since the old behavior was broken:

  // first ensure we're not dealing with IP addresses; if we are, require an
  // exact match. we can't avoid this, otherwise the algo below will allow two
  // IP's such as 128.12.96.5 and 213.12.96.5 to match.
  if (IsIPAddress(firstHost)) {
    return !IsInDomain(firstHost, currentHost, PR_FALSE);
  }

  // next, allow a one-subdomain-level "fuzz" in the comparison. first, we need
  // to find how many subdomain levels each host has; we only do the looser
  // comparison if they have the same number of levels. e.g.
  //  firstHost = weather.yahoo.com, currentHost = cookies.yahoo.com -> match
  //  firstHost =     a.b.yahoo.com, currentHost =       b.yahoo.com -> no match
  //  firstHost =         yahoo.com, currentHost = weather.yahoo.com -> no match
  //  (since the normal test (next) will catch this case and give a match.)
  // also, we can only do this if they have >=2 subdomain levels, to avoid
  // matching yahoo.com with netscape.com (yes, this breaks for .co.nz etc...)
  PRUint32 dotsInFirstHost = firstHost.CountChar('.');
  if (dotsInFirstHost == currentHost.CountChar('.') &&
      dotsInFirstHost >= 2) {
    // we have enough dots - check IsInDomain(choppedFirstHost, currentHost)
    PRInt32 dot1 = firstHost.FindChar('.');
    return !IsInDomain(Substring(firstHost, dot1, firstHost.Length() - dot1), currentHost);
  }

  // don't have enough dots to chop firstHost, or the subdomain levels differ;
  // so we just do the plain old check, IsInDomain(firstHost, currentHost).
  return !IsInDomain(NS_LITERAL_CSTRING(".") + firstHost, currentHost);
}

nsCookiePolicy
nsCookieService::GetP3PPolicy(PRInt32 aPolicy)
{
  switch (aPolicy) {
    case P3P_NoPolicy:
      return nsICookie::POLICY_NONE;
    case P3P_NoConsent:
      return nsICookie::POLICY_NO_CONSENT;
    case P3P_ImplicitConsent:
      return nsICookie::POLICY_IMPLICIT_CONSENT;
    case P3P_ExplicitConsent:
      return nsICookie::POLICY_EXPLICIT_CONSENT;
    case P3P_NoIdentInfo:
      return nsICookie::POLICY_NO_II;
    default:
      return nsICookie::POLICY_UNKNOWN;
  }
}

/*
 * returns P3P_NoPolicy, P3P_NoConsent, P3P_ImplicitConsent,
 * P3P_ExplicitConsent, or P3P_NoIdentInfo based on site
 */
PRInt32
nsCookieService::SiteP3PPolicy(nsIURI     *aCurrentURI,
                               nsIChannel *aChannel)
{
  // default to P3P_NoPolicy if anything fails
  PRInt32 consent = P3P_NoPolicy;

  nsCOMPtr<nsIHttpChannel> httpChannel = do_QueryInterface(aChannel);
  if (mP3PService && httpChannel) {
    nsCAutoString currentURISpec;
    if (NS_SUCCEEDED(aCurrentURI->GetAsciiSpec(currentURISpec))) {
      mP3PService->GetConsent(currentURISpec.get(), httpChannel, &consent);
    }
  }
  return consent;
}

nsCookieStatus
nsCookieService::P3PDecision(nsIURI     *aHostURI,
                             nsIURI     *aFirstURI,
                             nsIChannel *aChannel)
{
  // get the site policy from aHttpChannel
  PRInt32 policy = SiteP3PPolicy(aHostURI, aChannel);
  // check if the cookie is foreign; if aFirstURI is null, default to foreign
  PRInt32 isForeign = IsForeign(aHostURI, aFirstURI) == PR_TRUE;
  
  // if site does not collect identifiable info, then treat it as if it did and
  // asked for explicit consent. this check is required, since there is no entry
  // in mCookiesP3PString for it.
  if (policy == P3P_NoIdentInfo) {
    policy = P3P_ExplicitConsent;
  }

  // decide P3P_Accept, P3P_Downgrade, P3P_Flag, or P3P_Reject based on user's
  // preferences.
    // note: mCookiesP3PString can't be empty here, since we only execute this
    // path if BEHAVIOR_P3P is set; this in turn can only occur
    // if the p3p pref has been read (which is set to a default if the read
    // fails). if cookie is foreign, [policy + 1] points to the appropriate
    // pref; if cookie isn't foreign, [policy] points.
  PRInt32 decision = mCookiesP3PString.CharAt(policy + isForeign);

  switch (decision) {
    case P3P_Unknown:
      return nsICookie::STATUS_UNKNOWN;
    case P3P_Accept:
      return nsICookie::STATUS_ACCEPTED;
    case P3P_Downgrade:
      return nsICookie::STATUS_DOWNGRADED;
    case P3P_Flag:
      return nsICookie::STATUS_FLAGGED;
    case P3P_Reject:
      return nsICookie::STATUS_REJECTED;
  }
  return nsICookie::STATUS_UNKNOWN;
}

nsCookieStatus
nsCookieService::CheckPrefs(nsIURI     *aHostURI,
                            nsIURI     *aFirstURI,
                            nsIChannel *aChannel,
                            const char *aCookieHeader,
                            PRUint32   &aListPermission)
{
  // pref tree:
  // 0) get the scheme strings from the two URI's
  // 1) disallow ftp
  // 2) disallow mailnews, if pref set
  // 3) perform a permissionlist lookup to see if an entry exists for this host
  //    (a match here will override defaults in 4)
  // 4) go through enumerated permissions to see which one we have:
  // -> cookies disabled: return
  // -> dontacceptforeign: check if cookie is foreign
  // -> p3p: check p3p cookie data

  // we've extended the "nsCookieStatus" type to be used for all cases now
  // (used to be only for p3p), so beware that its interpretation is not p3p-
  // specific anymore.

  // first, get the URI scheme for further use
  // if GetScheme fails on aHostURI, reject; aFirstURI is optional, so failing is ok
  nsCAutoString currentURIScheme, firstURIScheme;
  nsresult rv, rv2 = NS_OK;
  rv = aHostURI->GetScheme(currentURIScheme);
  if (aFirstURI) {
    rv2 = aFirstURI->GetScheme(firstURIScheme);
  }
  if (NS_FAILED(rv) || NS_FAILED(rv2)) {
    COOKIE_LOGFAILURE(aCookieHeader ? SET_COOKIE : GET_COOKIE, aHostURI, aCookieHeader, "couldn't get scheme of host URI");
    return nsICookie::STATUS_REJECTED;
  }

  // don't let ftp sites get/set cookies (could be a security issue)
  if (currentURIScheme.Equals(NS_LITERAL_CSTRING("ftp"))) {
    COOKIE_LOGFAILURE(aCookieHeader ? SET_COOKIE : GET_COOKIE, aHostURI, aCookieHeader, "ftp sites cannot read cookies");
    return nsICookie::STATUS_REJECTED;
  }

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
      nsCOMPtr<nsIInterfaceRequestor> req;
      aChannel->GetNotificationCallbacks(getter_AddRefs(req));
      if (!req) {
        // check the load group's notification callbacks...
        nsCOMPtr<nsILoadGroup> group;
        aChannel->GetLoadGroup(getter_AddRefs(group));
        if (group)
          group->GetNotificationCallbacks(getter_AddRefs(req));
      }
      if (req) {
        nsCOMPtr<nsIDocShellTreeItem> item, parent = do_GetInterface(req);
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
    }
    if ((appType == nsIDocShell::APP_TYPE_MAIL) ||
        (aFirstURI && IsFromMailNews(firstURIScheme)) ||
        IsFromMailNews(currentURIScheme)) {
      COOKIE_LOGFAILURE(aCookieHeader ? SET_COOKIE : GET_COOKIE, aHostURI, aCookieHeader, "cookies disabled for mailnews");
      return nsICookie::STATUS_REJECTED;
    }
  }

  // check the permission list first; if we find an entry, it overrides
  // default prefs. see bug 184059.
  nsCOMPtr<nsIPermissionManager> permissionManager = do_GetService(NS_PERMISSIONMANAGER_CONTRACTID, &rv);
  if (NS_SUCCEEDED(rv)) {
    // we need to pass the lookup result, aListPermission, as an outparam since
    // we'll need it later on to decide whether to prompt the user or not.
    // (if an entry exists in the list, we don't want to prompt the user.)
    permissionManager->TestPermission(aHostURI, "cookie", &aListPermission);

    // if we found an entry, use it
    switch (aListPermission) {
      case nsIPermissionManager::DENY_ACTION:
        COOKIE_LOGFAILURE(aCookieHeader ? SET_COOKIE : GET_COOKIE, aHostURI, aCookieHeader, "cookies are blocked for this site");
        return nsICookie::STATUS_REJECTED;

      case nsIPermissionManager::ALLOW_ACTION:
        return nsICookie::STATUS_ACCEPTED;
    }
  }

  // check default prefs - go thru enumerated permissions
  if (mCookiesPermissions == BEHAVIOR_REJECT) {
    COOKIE_LOGFAILURE(aCookieHeader ? SET_COOKIE : GET_COOKIE, aHostURI, aCookieHeader, "cookies are disabled");
    return nsICookie::STATUS_REJECTED;

  } else if (mCookiesPermissions == BEHAVIOR_REJECTFOREIGN) {
    // check if cookie is foreign.
    // if aFirstURI is null, allow by default

    // note: this can be circumvented if we have http redirects within html,
    // since the documentURI attribute isn't always correctly
    // passed to the redirected channels. (or isn't correctly set in the first place)
    if (IsForeign(aHostURI, aFirstURI)) {
      COOKIE_LOGFAILURE(aCookieHeader ? SET_COOKIE : GET_COOKIE, aHostURI, aCookieHeader, "originating server test failed");
      return nsICookie::STATUS_REJECTED;
    }

  } else if (mCookiesPermissions == BEHAVIOR_P3P) {
    // check to see if P3P conditions are satisfied.
    // nsCookieStatus is an enumerated type, defined in nsCookie.idl (frozen interface):
    // STATUS_UNKNOWN -- cookie collected in a previous session and this info no longer available
    // STATUS_ACCEPTED -- cookie was accepted
    // STATUS_DOWNGRADED -- cookie was accepted but downgraded to a session cookie
    // STATUS_FLAGGED -- cookie was accepted with a warning being issued to the user
    // STATUS_REJECTED

    // to do this, at the moment, we need a channel, but we can live without
    // the two URI's (as long as no foreign checks are required).
    // if the channel is null, we can fall back on "no p3p policy" prefs.
    nsCookieStatus p3pStatus = P3PDecision(aHostURI, aFirstURI, aChannel);
    if (p3pStatus == nsICookie::STATUS_REJECTED) {
      COOKIE_LOGFAILURE(aCookieHeader ? SET_COOKIE : GET_COOKIE, aHostURI, aCookieHeader, "P3P test failed");
    }
    return p3pStatus;
  }

  // if nothing has complained, accept cookie
  return nsICookie::STATUS_ACCEPTED;
}

// processes domain attribute, and returns PR_TRUE if host has permission to set for this domain.
PRBool
nsCookieService::CheckDomain(nsCookieAttributes &aCookieAttributes,
                             nsIURI             *aHostURI)
{
  // get host from aHostURI
  nsCAutoString hostFromURI;
  if (NS_FAILED(aHostURI->GetAsciiHost(hostFromURI))) {
    return PR_FALSE;
  }
  // trim trailing dots
  hostFromURI.Trim(".");
  ToLowerCase(hostFromURI);

  // if a domain is given, check the host has permission
  if (!aCookieAttributes.host.IsEmpty()) {
    // switch to lowercase now, to avoid case-insensitive compares everywhere
    ToLowerCase(aCookieAttributes.host);

    // check whether the host is an IP address, and override isDomain to
    // make the cookie a non-domain one. this will require an exact host
    // match for the cookie, so we eliminate any chance of IP address
    // funkiness (e.g. the alias 127.1 domain-matching 99.54.127.1).
    // bug 105917 originally noted the requirement to deal with IP addresses.
    if (IsIPAddress(aCookieAttributes.host)) {
      aCookieAttributes.isDomain = PR_FALSE;
      return IsInDomain(aCookieAttributes.host, hostFromURI, PR_FALSE);
    }

    /*
     * verify that this host has the authority to set for this domain.   We do
     * this by making sure that the host is in the domain.  We also require
     * that a domain have at least one embedded period to prevent domains of the form
     * ".com" and ".edu"
     */
    aCookieAttributes.host.Trim(".");
    PRInt32 dot = aCookieAttributes.host.FindChar('.');
    if (dot == kNotFound) {
      // fail dot test
      return PR_FALSE;
    }

    // prepend a dot, and check if the host is in the domain
    aCookieAttributes.isDomain = PR_TRUE;
    aCookieAttributes.host.Insert(NS_LITERAL_CSTRING("."), 0);
    if (!IsInDomain(aCookieAttributes.host, hostFromURI)) {
      return PR_FALSE;
    }

    /*
     * check that portion of host not in domain does not contain a dot
     *    This satisfies the fourth requirement in section 4.3.2 of the cookie
     *    spec rfc 2109 (see www.cis.ohio-state.edu/htbin/rfc/rfc2109.html).
     *    It prevents host of the form x.y.co.nz from setting cookies in the
     *    entire .co.nz domain.  Note that this doesn't really solve the problem,
     *    it justs makes it more unlikely.  Sites such as y.co.nz can still set
     *    cookies for the entire .co.nz domain.
     *
     *  Although this is the right thing to do(tm), it breaks too many sites.  
     *  So only do it if the "network.cookie.strictDomains" pref is PR_TRUE.
     *
     */
    if (mCookiesStrictDomains) {
      dot = hostFromURI.FindChar('.', 0, hostFromURI.Length() - aCookieAttributes.host.Length());
      if (dot != kNotFound) {
        return PR_FALSE;
      }
    }

  // no domain specified, use hostFromURI
  } else {
    aCookieAttributes.isDomain = PR_FALSE;
    aCookieAttributes.host = hostFromURI;
  }

  return PR_TRUE;
}

PRBool
nsCookieService::CheckPath(nsCookieAttributes &aCookieAttributes,
                           nsIURI             *aHostURI)
{
  // if a path is given, check the host has permission
  if (aCookieAttributes.path.IsEmpty()) {
    // strip down everything after the last slash to get the path,
    // ignoring slashes in the query string part.
    // if we can QI to nsIURL, that'll take care of the query string portion.
    // otherwise, it's not an nsIURL and can't have a query string, so just find the last slash.
    nsCOMPtr<nsIURL> hostURL = do_QueryInterface(aHostURI);
    if (hostURL) {
      hostURL->GetDirectory(aCookieAttributes.path);
    } else {
      aHostURI->GetPath(aCookieAttributes.path);
      PRInt32 slash = aCookieAttributes.path.RFindChar('/');
      if (slash != kNotFound) {
        aCookieAttributes.path.Truncate(slash + 1);
      }
    }

#if 0
  } else {
    /**
     * The following test is part of the RFC2109 spec.  Loosely speaking, it says that a site
     * cannot set a cookie for a path that it is not on.  See bug 155083.  However this patch
     * broke several sites -- nordea (bug 155768) and citibank (bug 156725).  So this test has
     * been disabled, unless we can evangelize these sites.
     */
    // get path from aHostURI
    nsCAutoString pathFromURI;
    if (NS_FAILED(aHostURI->GetPath(pathFromURI)) ||
        !StringBeginsWith(pathFromURI, aCookieAttributes.path)) {
      return PR_FALSE;
    }
#endif
  }

  return PR_TRUE;
}

PRBool
nsCookieService::GetExpiry(nsCookieAttributes &aCookieAttributes,
                           nsInt64            aServerTime,
                           nsInt64            aCurrentTime,
                           nsCookieStatus     aStatus)
{
  /* Determine when the cookie should expire. This is done by taking the difference between 
   * the server time and the time the server wants the cookie to expire, and adding that 
   * difference to the client time. This localizes the client time regardless of whether or
   * not the TZ environment variable was set on the client.
   *
   * Note: We need to consider accounting for network lag here, per RFC.
   */
  nsInt64 delta;

  // check for max-age attribute first; this overrides expires attribute
  if (!aCookieAttributes.maxage.IsEmpty()) {
    // obtain numeric value of maxageAttribute
    PRInt64 maxage;
    PRInt32 numInts = PR_sscanf(aCookieAttributes.maxage.get(), "%lld", &maxage);

    // default to session cookie if the conversion failed
    if (numInts != 1) {
      return PR_TRUE;
    }

    delta = nsInt64(maxage);

  // check for expires attribute
  } else if (!aCookieAttributes.expires.IsEmpty()) {
    nsInt64 expires;
    PRTime tempExpires;

    // parse expiry time
    if (PR_ParseTimeString(aCookieAttributes.expires.get(), PR_TRUE, &tempExpires) == PR_SUCCESS) {
      expires = nsInt64(tempExpires) / USEC_PER_SEC;
    } else {
      return PR_TRUE;
    }

    delta = expires - aServerTime;

  // default to session cookie if no attributes found
  } else {
    return PR_TRUE;
  }

  if (delta > nsInt64(0)) {
    // check cookie lifetime pref, and limit lifetime if required.
    // we only want to do this if the cookie isn't going to be expired anyway.
    if (mCookiesLifetimeEnabled) {
      if (mCookiesLifetimeCurrentSession) {
        // limit lifetime to session
        return PR_TRUE;
      } else if (delta > nsInt64(mCookiesLifetimeSec)) {
        // limit lifetime to specified time
        delta = mCookiesLifetimeSec;
      }
    }
  }

  // if this addition overflows, expiryTime will be less than currentTime
  // and the cookie will be expired - that's okay.
  aCookieAttributes.expiryTime = aCurrentTime + delta;

  // we need to return whether the cookie is a session cookie or not:
  // the cookie may have been previously downgraded by p3p prefs,
  // so we take that into account here. only applies to non-expired cookies.
  return aStatus == nsICookie::STATUS_DOWNGRADED &&
         aCookieAttributes.expiryTime > aCurrentTime;
}

/******************************************************************************
 * nsCookieService impl:
 * private cookielist management functions
 ******************************************************************************/

void
nsCookieService::RemoveAllFromMemory()
{
  nsCookie *cookieInList;
  for (PRInt32 i = mCookieList.Count(); i--;) {
    cookieInList = NS_STATIC_CAST(nsCookie*, mCookieList.ElementAt(i));
    NS_ASSERTION(cookieInList, "corrupt cookie list");
    NS_RELEASE(cookieInList);
  }
  mCookieList.SizeTo(0);
  mCookieChanged = PR_TRUE;
}

// removes any expired cookies from memory, and finds the oldest cookie in the list
void
nsCookieService::RemoveExpiredCookies(nsInt64 aCurrentTime,
                                      PRInt32 &aOldestPosition)
{
  aOldestPosition = -1;

  nsCookie *cookieInList;
  nsInt64 oldestTime = LL_MAXINT;

  for (PRInt32 i = mCookieList.Count(); i--;) {
    cookieInList = NS_STATIC_CAST(nsCookie*, mCookieList.ElementAt(i));
    NS_ASSERTION(cookieInList, "corrupt cookie list");

    if (!cookieInList->IsSession() && cookieInList->Expiry() <= aCurrentTime) {
      mCookieList.RemoveElementAt(i);
      NS_RELEASE(cookieInList);
      mCookieChanged = PR_TRUE;
      --aOldestPosition;
      continue;
    }

    if (oldestTime > cookieInList->LastAccessed()) {
      oldestTime = cookieInList->LastAccessed();
      aOldestPosition = i;
    }
  }
}

// count the number of cookies from this host, and find whether a previous cookie
// has been set, for prompting purposes. 
PRBool
nsCookieService::FindCookiesFromHost(nsCookie *aCookie,
                                     PRUint32 &aCountFromHost,
                                     nsInt64  aCurrentTime)
{
  aCountFromHost = 0;
  PRBool foundCookie = PR_FALSE;

  nsCookie *cookieInList;
  const nsAFlatCString &host = aCookie->Host();
  const nsAFlatCString &path = aCookie->Path();
  const nsAFlatCString &name = aCookie->Name();

  PRInt32 count = mCookieList.Count();
  for (PRInt32 i = 0; i < count; ++i) {
    cookieInList = NS_STATIC_CAST(nsCookie*, mCookieList.ElementAt(i));
    NS_ASSERTION(cookieInList, "corrupt cookie list");

    // only count session or non-expired cookies
    if (IsInDomain(cookieInList->Host(), host, cookieInList->IsDomain()) &&
        (cookieInList->IsSession() || cookieInList->Expiry() > aCurrentTime)) {
      ++aCountFromHost;

      // check if we've found the previous cookie
      if (path.Equals(cookieInList->Path()) &&
          host.Equals(cookieInList->Host()) &&
          name.Equals(cookieInList->Name())) {
        foundCookie = PR_TRUE;
      }
    }
  }

  return foundCookie;
}

// find a position to store a cookie (either replacing an existing cookie, or adding
// to end of list), and a cookie to delete (if maximum number of cookies has been
// exceeded). also performs list maintenance by removing expired cookies.
// returns whether a previous cookie already exists,
// aInsertPosition is the position to insert the new cookie;
// aDeletePosition is the position to delete (-1 if not required).
PRBool
nsCookieService::FindPosition(nsCookie *aCookie,
                              PRInt32  &aInsertPosition,
                              PRInt32  &aDeletePosition,
                              nsInt64  aCurrentTime)
{
  aDeletePosition = -1;
  aInsertPosition = -1;
  PRBool foundCookie = PR_FALSE;

  // list maintenance: remove expired cookies, and find the position of the
  // oldest cookie while we're at it - required if we have kMaxNumberOfCookies
  // and need to remove one.
  PRInt32 oldestPosition;
  RemoveExpiredCookies(aCurrentTime, oldestPosition);

  nsCookie *cookieInList;
  nsInt64 oldestTimeFromHost = LL_MAXINT;
  PRInt32 oldestPositionFromHost;
  PRInt32 countFromHost = 0;
  const nsAFlatCString &host = aCookie->Host();
  const nsAFlatCString &path = aCookie->Path();
  const nsAFlatCString &name = aCookie->Name();

  PRInt32 count = mCookieList.Count();
  for (PRInt32 i = 0; i < count; ++i) {
    cookieInList = NS_STATIC_CAST(nsCookie*, mCookieList.ElementAt(i));
    NS_ASSERTION(cookieInList, "corrupt cookie list");

    // check if we've passed the location where we might find a previous cookie.
    // mCookieList is sorted in order of descending path length,
    // so since we're enumerating forwards, we look for aCookie path length
    // to become greater than cookieInList path length.
    // if we've found a position to insert the cookie at (either replacing a 
    // previous cookie, or inserting at a new position), we don't need to keep looking.
    if (aInsertPosition == -1 &&
        path.Length() > cookieInList->Path().Length()) {
      aInsertPosition = i;
    }

    if (IsInDomain(cookieInList->Host(), host, cookieInList->IsDomain())) {
      ++countFromHost;

      if (oldestTimeFromHost > cookieInList->LastAccessed()) {
        oldestTimeFromHost = cookieInList->LastAccessed();
        oldestPositionFromHost = i;
      }

      if (aInsertPosition == -1 &&
          path.Equals(cookieInList->Path()) &&
          host.Equals(cookieInList->Host()) &&
          name.Equals(cookieInList->Name())) {
        aInsertPosition = i;
        foundCookie = PR_TRUE;
      }
    }
  }

  // if we didn't find a position to insert at, put it at the end of the list
  if (aInsertPosition == -1) {
    aInsertPosition = count;
  }

  // choose which cookie to delete (oldest cookie, or oldest cookie from this host),
  // if we have to.
  if (countFromHost >= kMaxCookiesPerHost) {
    aDeletePosition = oldestPositionFromHost;
  } else if (count >= kMaxNumberOfCookies) {
    aDeletePosition = oldestPosition;
  }

  return foundCookie;
}
