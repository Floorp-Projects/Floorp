/* -*- Mode: C; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
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
 *   Pierre Phaneuf <pp@ludusdesign.com>
 *   Henrik Gemal <mozilla@gemal.dk>
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

#include "nsCookies.h"
#include "nsCookieService.h"
#include "nsICookiePermission.h"
#include "nsIPermissionManager.h"
#include "nsIIOService.h"
#include "prprf.h"
#include "nsReadableUtils.h"
#include "nsIPrefService.h"
#include "nsIPrefBranch.h"
#include "nsIPrefBranchInternal.h"
#include "nsIObserver.h"
#include "nsIObserverService.h"
#include "nsICookieConsent.h"
#include "nsIURL.h"
#include "prnetdb.h"
#include "nsComObsolete.h"
#include "nsILineInputStream.h"
#include "nsNetUtil.h"
#include "nsAppDirectoryServiceDefs.h"
#include "nsCRT.h"

// we want to explore making the document own the load group
// so we can associate the document URI with the load group.
// until this point, we have an evil hack:
#include "nsIHttpChannelInternal.h"  

/******************************************************************************
 * gCookiePrefObserver
 * This is instanced as a global variable in nsCookieService.cpp.
 *****************************************************************************/

// gCookiePrefObserver class - defined as |extern| in nsCookieService.h
nsCookiePrefObserver *gCookiePrefObserver;

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

// P3P constants
/* mCookiesP3PString (below) consists of 8 characters having the following interpretation:
 *   [0]: behavior for first-party cookies when site has no privacy policy
 *   [1]: behavior for third-party cookies when site has no privacy policy
 *   [2]: behavior for first-party cookies when site uses PII with no user consent
 *   [3]: behavior for third-party cookies when site uses PII with no user consent
 *   [4]: behavior for first-party cookies when site uses PII with implicit consent only
 *   [5]: behavior for third-party cookies when site uses PII with implicit consent only
 *   [6]: behavior for first-party cookies when site uses PII with explicit consent
 *   [7]: behavior for third-party cookies when site uses PII with explicit consent
 *
 * note: PII = personally identifiable information
 *
 * each of the eight characters can be one of the following
 *   'a': accept the cookie
 *   'd': accept the cookie but downgrade it to a session cookie
 *   'r': reject the cookie
 *
 * The following defines are used to refer to these character positions and values
 */
// do we need P3P_UnknownPolicy? can't we default to P3P_NoPolicy?
#define P3P_UnknownPolicy   -1
#define P3P_NoPolicy         0
#define P3P_NoConsent        2
#define P3P_ImplicitConsent  4
#define P3P_ExplicitConsent  6
#define P3P_NoIdentInfo      8

#define P3P_Unknown    ' '
#define P3P_Accept     'a'
#define P3P_Downgrade  'd'
#define P3P_Reject     'r'
#define P3P_Flag       'f'

nsCookiePrefObserver::nsCookiePrefObserver()
{
  nsresult rv;

  nsCOMPtr<nsIPrefBranchInternal> prefInternal;

  // install and cache the preferences observer
  mPrefBranch = do_GetService(NS_PREFSERVICE_CONTRACTID, &rv);
  if (NS_SUCCEEDED(rv)) {
    prefInternal = do_QueryInterface(mPrefBranch, &rv);
  }

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

    // initialize prefs
    rv = ReadPrefs();
    if (NS_FAILED(rv)) {
      NS_WARNING("Error occured reading cookie preferences");
    }
  }
}

nsCookiePrefObserver::~nsCookiePrefObserver()
{
}

NS_IMPL_ISUPPORTS1(nsCookiePrefObserver, nsIObserver)

NS_IMETHODIMP
nsCookiePrefObserver::Observe(nsISupports *aSubject,
                              const char *aTopic,
                              const PRUnichar *aData)
{
  nsresult rv;

  // check the topic, and the cached prefservice
  if (!mPrefBranch ||
      nsCRT::strcmp(NS_PREFBRANCH_PREFCHANGE_TOPIC_ID, aTopic)) {
    return NS_ERROR_FAILURE;
  }

  // which pref changed?
  NS_ConvertUCS2toUTF8 pref(aData);
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
      tempPrefValue = PERMISSION_DontUse;
    }
    mCookiesPermissions = (PERMISSION_BehaviorEnum) tempPrefValue;

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
    rv = mPrefBranch->GetBoolPref(kCookiesStrictDomains, &mCookiesStrictDomains);
    if (NS_FAILED(rv)) {
      mCookiesStrictDomains = PR_FALSE;
    }
  }

#ifdef MOZ_PHOENIX
  // collapse two boolean prefs into enumerated permissions
  // note: PERMISSION_P3P is not used in Phoenix, so we won't reach any P3P code.
  if (computePermissions) {
    if (mCookiesEnabled_temp) {
      // check if user wants cookies only for site domain
      if (mCookiesForDomainOnly_temp) {
        mCookiesPermissions = PERMISSION_DontAcceptForeign;
      } else {
        mCookiesPermissions = PERMISSION_Accept;
      }
    } else {
      mCookiesPermissions = PERMISSION_DontUse;
    }
  }
#endif

  return NS_OK;
}

nsresult
nsCookiePrefObserver::ReadPrefs()
{
  nsresult rv, rv2 = NS_OK;

  // check the prefservice is cached
  if (!mPrefBranch) {
    return NS_ERROR_FAILURE;
  }

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
  // note: PERMISSION_P3P is not used in Phoenix
  if (mCookiesEnabled_temp) {
    // check if user wants cookies only for site domain
    if (mCookiesForDomainOnly_temp) {
      mCookiesPermissions = PERMISSION_DontAcceptForeign;
    } else {
      mCookiesPermissions = PERMISSION_Accept;
    }
  } else {
    mCookiesPermissions = PERMISSION_DontUse;
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
    tempPrefValue = PERMISSION_DontUse;
    rv2 = rv;
  }
  mCookiesPermissions = (PERMISSION_BehaviorEnum) tempPrefValue;

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

  rv = mPrefBranch->GetBoolPref(kCookiesStrictDomains, &mCookiesStrictDomains);
  if (NS_FAILED(rv)) {
    mCookiesStrictDomains = PR_FALSE;
    rv2 = rv;
  }

  return rv2;
}

/******************************************************************************
 * nsCookies impl: #defines and constants
 *****************************************************************************/

nsVoidArray *sCookieList;
static const char kCookieFileName[] = "cookies.txt";

// cookie constants
#define MAX_NUMBER_OF_COOKIES 300
#define MAX_COOKIES_PER_SERVER 20
#define MAX_BYTES_PER_COOKIE 4096  /* must be at least 1 */

#ifdef PR_LOGGING
PRIVATE PRLogModuleInfo *gCookieLog = PR_NewLogModule("cookie");

void
cookie_LogFailure(PRBool aSetCookie, nsIURI *aHostURI, const char *aCookieString, const char *aReason) {
  // if logging isn't enabled, return now to save cycles
  if (!PR_LOG_TEST(gCookieLog, PR_LOG_WARNING)) {
    return;
  }

  nsCAutoString spec;
  if (aHostURI)
    aHostURI->GetAsciiSpec(spec);

  PR_LOG(gCookieLog, PR_LOG_WARNING,
    ("%s%s%s\n", "===== ", aSetCookie ? "COOKIE NOT ACCEPTED" : "COOKIE NOT SENT", " ====="));
  PR_LOG(gCookieLog, PR_LOG_WARNING,("request URL: %s\n", spec.get()));
  if (aSetCookie) {
    PR_LOG(gCookieLog, PR_LOG_WARNING,("cookie string: %s\n", aCookieString));
  }

  PRExplodedTime explodedTime;
  PR_ExplodeTime(PR_Now(), PR_GMTParameters, &explodedTime);
  char timeString[40];
  PR_FormatTimeUSEnglish(timeString, 40, "%c GMT", &explodedTime);

  PR_LOG(gCookieLog, PR_LOG_WARNING,("current time: %s", timeString));
  PR_LOG(gCookieLog, PR_LOG_WARNING,("rejected because %s\n", aReason));
  PR_LOG(gCookieLog, PR_LOG_WARNING,("\n"));
}

void
cookie_LogSuccess(PRBool aSetCookie, nsIURI *aHostURI, const char *aCookieString, cookie_CookieStruct *aCookie) {
  // if logging isn't enabled, return now to save cycles
  if (!PR_LOG_TEST(gCookieLog, PR_LOG_DEBUG)) {
    return;
  }

  nsCAutoString spec;
  aHostURI->GetAsciiSpec(spec);

  PR_LOG(gCookieLog, PR_LOG_DEBUG,
    ("%s%s%s\n", "===== ", aSetCookie ? "COOKIE ACCEPTED" : "COOKIE SENT", " ====="));
  PR_LOG(gCookieLog, PR_LOG_DEBUG,("request URL: %s\n", spec.get()));
  PR_LOG(gCookieLog, PR_LOG_DEBUG,("cookie string: %s\n", aCookieString));

  PRExplodedTime explodedTime;
  PR_ExplodeTime(PR_Now(), PR_GMTParameters, &explodedTime);
  char timeString[40];
  PR_FormatTimeUSEnglish(timeString, 40, "%c GMT", &explodedTime);

  PR_LOG(gCookieLog, PR_LOG_DEBUG,("current time: %s", timeString));

  if (aSetCookie) {
    PR_LOG(gCookieLog, PR_LOG_DEBUG,("----------------\n"));
    PR_LOG(gCookieLog, PR_LOG_DEBUG,("name: %s\n", aCookie->name.get()));
    PR_LOG(gCookieLog, PR_LOG_DEBUG,("value: %s\n", aCookie->cookie.get()));
    PR_LOG(gCookieLog, PR_LOG_DEBUG,("%s: %s\n", aCookie->isDomain ? "domain" : "host", aCookie->host.get()));
    PR_LOG(gCookieLog, PR_LOG_DEBUG,("path: %s\n", aCookie->path.get()));

    PR_ExplodeTime(nsInt64(aCookie->expires) * USEC_PER_SEC, PR_GMTParameters, &explodedTime);
    PR_FormatTimeUSEnglish(timeString, 40, "%c GMT", &explodedTime);

    PR_LOG(gCookieLog, PR_LOG_DEBUG,("expires: %s",
           aCookie->isSession ? "at end of session" : timeString));
    PR_LOG(gCookieLog, PR_LOG_DEBUG,("is secure: %s\n", aCookie->isSecure ? "true" : "false"));
  }
  PR_LOG(gCookieLog, PR_LOG_DEBUG,("\n"));
}

// inline wrappers to make passing in nsAStrings easier
inline void
cookie_LogFailure(PRBool aSetCookie, nsIURI *aHostURI, const nsAFlatCString &aCookieString, const char *aReason) {
  cookie_LogFailure(aSetCookie, aHostURI, aCookieString.get(), aReason);
}

inline void
cookie_LogSuccess(PRBool aSetCookie, nsIURI *aHostURI, const nsAFlatCString &aCookieString, cookie_CookieStruct *aCookie) {
  cookie_LogSuccess(aSetCookie, aHostURI, aCookieString.get(), aCookie);
}
#endif

/******************************************************************************
 * nsCookies impl: some function prototypes
 *****************************************************************************/

PRIVATE PRBool cookie_IsInDomain(const nsACString &domain, const nsACString &host, PRBool aIsDomain = PR_TRUE);

/******************************************************************************
 * nsCookies impl: cookie list management functions
 *****************************************************************************/

// cookie list management
PRIVATE PRBool sCookieChanged = PR_FALSE;

PUBLIC void
COOKIE_RemoveAll()
{
  cookie_CookieStruct *cookieInList;
  for (PRInt32 i = sCookieList->Count() - 1; i >= 0; --i) {
    cookieInList = NS_STATIC_CAST(cookie_CookieStruct*, sCookieList->ElementAt(i));
    NS_ASSERTION(cookieInList, "corrupt cookie list");
    delete cookieInList;
  }
  sCookieList->SizeTo(0);
  sCookieChanged = PR_TRUE;
}

// removes any expired cookies from memory, and finds the oldest cookie in the list
PUBLIC void
COOKIE_RemoveExpiredCookies(nsInt64 aCurrentTime,
                            PRInt32 &aOldestPosition)
{
  aOldestPosition = -1;

  cookie_CookieStruct *cookieInList;
  nsInt64 oldestTime = LL_MININT;

  for (PRInt32 i = sCookieList->Count() - 1; i >= 0; --i) {
    cookieInList = NS_STATIC_CAST(cookie_CookieStruct*, sCookieList->ElementAt(i));
    NS_ASSERTION(cookieInList, "corrupt cookie list");

    if (cookieInList->isSession) {
      continue;
    }

    if (nsInt64(cookieInList->expires) < aCurrentTime) {
      sCookieList->RemoveElementAt(i);
      delete cookieInList;
      sCookieChanged = PR_TRUE;
      --aOldestPosition;
      continue;
    }

    if (oldestTime > nsInt64(cookieInList->lastAccessed)) {
      oldestTime = cookieInList->lastAccessed;
      aOldestPosition = i;
    }
  }
}

// count the number of cookies from this host, and find whether a previous cookie
// has been set, for prompting purposes. 
PRIVATE PRBool
cookie_FindCookiesFromHost(cookie_CookieStruct *aCookie,
                           PRUint32            &aCountFromHost,
                           nsInt64             aCurrentTime)
{
  aCountFromHost = 0;
  PRBool foundCookie = PR_FALSE;

  cookie_CookieStruct *cookieInList;
  const nsAFlatCString &host = aCookie->host;
  const nsAFlatCString &path = aCookie->path;
  const nsAFlatCString &name = aCookie->name;

  PRInt32 count = sCookieList->Count();
  for (PRInt32 i = 0; i < count; ++i) {
    cookieInList = NS_STATIC_CAST(cookie_CookieStruct*, sCookieList->ElementAt(i));
    NS_ASSERTION(cookieInList, "corrupt cookie list");

    // only count non-expired cookies
    if (cookie_IsInDomain(cookieInList->host, host, cookieInList->isDomain) &&
        !cookieInList->isSession && (nsInt64(cookieInList->expires) > aCurrentTime)) {
      ++aCountFromHost;

      // check if we've found the previous cookie
      if (path.Equals(cookieInList->path) &&
          host.Equals(cookieInList->host) &&
          name.Equals(cookieInList->name)) {
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
PRIVATE PRBool
cookie_FindPosition(cookie_CookieStruct *aCookie,
                    PRInt32             &aInsertPosition,
                    PRInt32             &aDeletePosition,
                    nsInt64             aCurrentTime)
{
  aDeletePosition = -1;
  aInsertPosition = -1;
  PRBool foundCookie = PR_FALSE;

  // list maintenance: remove expired cookies, and find the position of the
  // oldest cookie while we're at it - required if we have MAX_NUMBER_OF_COOKIES
  // and need to remove one.
  PRInt32 oldestPosition;
  COOKIE_RemoveExpiredCookies(aCurrentTime, oldestPosition);

  cookie_CookieStruct *cookieInList;
  nsInt64 oldestTimeFromHost = LL_MININT;
  PRInt32 oldestPositionFromHost;
  PRInt32 countFromHost = 0;
  const nsAFlatCString &host = aCookie->host;
  const nsAFlatCString &path = aCookie->path;
  const nsAFlatCString &name = aCookie->name;

  PRInt32 count = sCookieList->Count();
  for (PRInt32 i = 0; i < count; ++i) {
    cookieInList = NS_STATIC_CAST(cookie_CookieStruct*, sCookieList->ElementAt(i));
    NS_ASSERTION(cookieInList, "corrupt cookie list");

    if (cookie_IsInDomain(cookieInList->host, host, cookieInList->isDomain)) {
      ++countFromHost;

      if (oldestTimeFromHost > nsInt64(cookieInList->lastAccessed)) {
        oldestTimeFromHost = cookieInList->lastAccessed;
        oldestPositionFromHost = i;
      }

      // if we've found a position to insert the cookie at (either replacing a 
      // previous cookie, or inserting at a new position), we don't need to keep looking.
      if (aInsertPosition != -1) {
        continue;
      }

      // check if we've passed the location where we might find a previous cookie.
      // sCookieList is sorted in order of descending path length,
      // so since we're enumerating forwards, we look for aCookie path length
      // to become greater than cookieInList path length.
      if (aCookie->path.Length() > cookieInList->path.Length()) {
        aInsertPosition = i;
      
      // check if we've found the previous cookie
      } else if (path.Equals(cookieInList->path) &&
                 host.Equals(cookieInList->host) &&
                 name.Equals(cookieInList->name)) {
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
  if (countFromHost >= MAX_COOKIES_PER_SERVER) {
    aDeletePosition = oldestPositionFromHost;
  } else if (count >= MAX_NUMBER_OF_COOKIES) {
    aDeletePosition = oldestPosition;
  }

  return foundCookie;
}

PUBLIC void
COOKIE_Remove(const nsACString &host,
              const nsACString &name,
              const nsACString &path,
              PRBool           blocked)
{
  cookie_CookieStruct *cookieInList;

  // step through all cookies, searching for indicated one
  PRInt32 count = sCookieList->Count();
  for (PRInt32 i = 0; i < count; ++i) {
    cookieInList = NS_STATIC_CAST(cookie_CookieStruct*, sCookieList->ElementAt(i));
    NS_ASSERTION(cookieInList, "corrupt cookie list");

    if (cookieInList->path.Equals(path) &&
        cookieInList->host.Equals(host) &&
        cookieInList->name.Equals(name)) {
      // check if we need to add the host to the permissions blacklist.
      // we should push this portion into the UI, it shouldn't live here in the backend.
      if (blocked) {
        nsresult rv;
        nsCOMPtr<nsIPermissionManager> permissionManager = do_GetService(NS_PERMISSIONMANAGER_CONTRACTID, &rv);

        if (NS_SUCCEEDED(rv)) {
          nsCOMPtr<nsIURI> uri;
          NS_NAMED_LITERAL_CSTRING(httpPrefix, "http://");
          // remove leading dot from host
          if (!cookieInList->host.IsEmpty() && cookieInList->host.First() == '.') {
            rv = NS_NewURI(getter_AddRefs(uri), PromiseFlatCString(httpPrefix + Substring(cookieInList->host, 1, cookieInList->host.Length() - 1)));
          } else {
            rv = NS_NewURI(getter_AddRefs(uri), PromiseFlatCString(httpPrefix + cookieInList->host));
          }

          if (NS_SUCCEEDED(rv))
            permissionManager->Add(uri,
                                   nsIPermissionManager::COOKIE_TYPE,
                                   nsIPermissionManager::DENY_ACTION);
        }
      }

      sCookieList->RemoveElementAt(i);
      delete cookieInList;
      sCookieChanged = PR_TRUE;
      COOKIE_Write(); // might want to eventually push this into the UI, to write once on cookiemanager close
      break;
    }
  }
}

// this is a backend function for adding a cookie to the list, via COOKIE_SetCookie.
// also used in the cookie manager, for profile migration from IE.
// returns NS_OK if aCookie was added to the list; NS_FAILED if it can be free'd by
// the caller.
PUBLIC nsresult
COOKIE_Add(cookie_CookieStruct *aCookie,
           nsInt64             aCurrentTime,
           nsIURI              *aHostURI,
           const char          *aCookieHeader)
{
  // find a position to insert the cookie at (and delete a cookie from, if necessary).
  // also removes expired cookies from the list, for maintenance purposes.
  PRInt32 insertPosition, deletePosition;
  PRBool foundCookie = cookie_FindPosition(aCookie, insertPosition, deletePosition, aCurrentTime);

  // store the cookie
  if (foundCookie) {
    cookie_CookieStruct *prevCookie;
    prevCookie = NS_STATIC_CAST(cookie_CookieStruct*, sCookieList->ElementAt(insertPosition));
    NS_ASSERTION(prevCookie, "corrupt cookie list");
    delete prevCookie;

    // check if the server wants to delete the cookie
    if (!aCookie->isSession && nsInt64(aCookie->expires) < aCurrentTime) {
      // delete previous cookie
      sCookieList->RemoveElementAt(insertPosition);
      COOKIE_LOGFAILURE(SET_COOKIE, aHostURI, aCookieHeader, "previously stored cookie was deleted");
      sCookieChanged = PR_TRUE;
      return NS_ERROR_FAILURE;

    } else {
      // replace previous cookie
      sCookieList->ReplaceElementAt(aCookie, insertPosition);
    }

  } else {
    // check if cookie has already expired
    if (!aCookie->isSession && nsInt64(aCookie->expires) < aCurrentTime) {
      COOKIE_LOGFAILURE(SET_COOKIE, aHostURI, aCookieHeader, "cookie has already expired");
      return NS_ERROR_FAILURE;
    }

    // add the cookie, which means we might have to delete an old cookie
    if (deletePosition != -1) {
      cookie_CookieStruct *deleteCookie;
      deleteCookie = NS_STATIC_CAST(cookie_CookieStruct*, sCookieList->ElementAt(deletePosition));
      NS_ASSERTION(deleteCookie, "corrupt cookie list");

      sCookieList->RemoveElementAt(deletePosition);
      delete deleteCookie;
      // adjust insertPosition if we removed a cookie before it
      if (insertPosition > deletePosition) {
        --insertPosition;
      }
    }

    sCookieList->InsertElementAt(aCookie, insertPosition);
  }

  sCookieChanged = PR_TRUE;
  return NS_OK;
}

/******************************************************************************
 * nsCookies impl: domain & permission related functions
 *****************************************************************************/

// returns PR_TRUE if hostname is an IP address
PRIVATE inline PRBool
cookie_IsIPAddress(const nsAFlatCString &aHost)
{
  PRNetAddr addr;
  return (PR_StringToNetAddr(aHost.get(), &addr) == PR_SUCCESS);
}

// returns PR_TRUE if URI scheme is from mailnews
PRIVATE inline PRBool
cookie_IsFromMailNews(const nsAFlatCString &aScheme)
{
  return (aScheme.Equals(NS_LITERAL_CSTRING("imap"))  || 
          aScheme.Equals(NS_LITERAL_CSTRING("news"))  ||
          aScheme.Equals(NS_LITERAL_CSTRING("snews")) ||
          aScheme.Equals(NS_LITERAL_CSTRING("mailbox")));
}

PRIVATE PRBool
cookie_IsInDomain(const nsACString &domain,
                  const nsACString &host,
                  PRBool           aIsDomain)
{
  // if we have a non-domain cookie, require an exact match between domain and host.
  // RFC2109 specifies this behavior; it allows a site to prevent its subdomains
  // from accessing a cookie, for whatever reason.
  if (!aIsDomain) {
    return domain.Equals(host);
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
  PRUint32 domainLength = domain.Length();
  PRInt32 lengthDifference = host.Length() - domainLength;
  // normal case
  if (lengthDifference > 0) {
    return domain.Equals(Substring(host, lengthDifference, domainLength));
  }
  // special case
  if (lengthDifference == -1) {
    return Substring(domain, 1, domainLength - 1).Equals(host);
  }
  // no match
  return PR_FALSE;
}

PRIVATE PRBool
cookie_IsForeign(nsIURI *aHostURI, nsIURI *aFirstURI) {
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
  // IP's such as 128.12.96.5 and 213.12.96.5 to match. if we decide this is
  // unlikely or doesn't matter, this can be removed (which will remove the
  // dependency on prnetdb.h -> codesize).
  if (cookie_IsIPAddress(firstHost)) {
    return cookie_IsInDomain(firstHost, currentHost, PR_FALSE);
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
    return !cookie_IsInDomain(Substring(firstHost, dot1, firstHost.Length() - dot1), currentHost);
  }

  // don't have enough dots to chop firstHost, or the subdomain levels differ;
  // so we just do the plain old check, IsInDomain(firstHost, currentHost).
  return !cookie_IsInDomain(NS_LITERAL_CSTRING(".") + firstHost, currentHost);
}

PRIVATE inline nsCookiePolicy
cookie_GetPolicy(PRInt32 policy) {
  switch (policy) {
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
P3P_SitePolicy(nsIURI *aCurrentURI, nsIHttpChannel *aHttpChannel)
{
  // default to P3P_NoPolicy if anything fails
  PRInt32 consent = P3P_NoPolicy;

  nsCOMPtr<nsICookieConsent> p3p(do_GetService(NS_COOKIECONSENT_CONTRACTID));
  if (p3p && aHttpChannel) {
    nsCAutoString currentURISpec;
    if (NS_FAILED(aCurrentURI->GetAsciiSpec(currentURISpec))) {
      return consent;
    }
    p3p->GetConsent(currentURISpec.get(), aHttpChannel, &consent);
  }
  return consent;
}

PRIVATE nsCookieStatus
cookie_P3PDecision(nsIURI *aHostURI, nsIURI *aFirstURI, nsIHttpChannel *aHttpChannel)
{
  // get the site policy from aHttpChannel
  PRInt32 policy = P3P_SitePolicy(aHostURI, aHttpChannel);
  // check if the cookie is foreign; if aFirstURI is null, default to foreign
  PRInt32 isForeign = cookie_IsForeign(aHostURI, aFirstURI) == PR_TRUE;
  
  // if site does not collect identifiable info, then treat it as if it did and
  // asked for explicit consent. this check is required, since there is no entry
  // in mCookiesP3PString for it.
  if (policy == P3P_NoIdentInfo) {
    policy = P3P_ExplicitConsent;
  }

  // decide P3P_Accept, P3P_Downgrade, P3P_Flag, or P3P_Reject based on user's
  // preferences.
    // note: mCookiesP3PString can't be empty here, since we only execute this
    // path if PERMISSION_P3P is set; this in turn can only occur
    // if the p3p pref has been read (which is set to a default if the read
    // fails). if cookie is foreign, [policy + 1] points to the appropriate
    // pref; if cookie isn't foreign, [policy] points.
  PRInt32 decision = gCookiePrefObserver->mCookiesP3PString.CharAt(policy + isForeign);

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

/******************************************************************************
 * nsCookies impl: cookie header parsing functions
 *****************************************************************************/

// The following comment block elucidates the function of cookie_ParseAttributes.
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
       terminate tokens or values.

    4. where appropriate, full <OCTET>s are allowed, where the spec dictates to
       reject control chars or non-ASCII chars. This is erring on the loose
       side, since there's probably no good reason to enforce this strictness.

    5. cookie <VALUE> is optional, where spec requires it. This is a fairly
       trivial case, but allows the flexibility of setting only a cookie <NAME>.

 ** Begin BNF:
    token         = 1*<any allowed-chars except separators>
    value         = token-value | quoted-string
    token-value   = 1*<any allowed-chars except value-sep>
    quoted-string = ( <"> *( qdtext | quoted-pair ) <"> )
    qdtext        = <any allowed-chars except <">>             ; CR | LF removed by necko
    quoted-pair   = "\" <any OCTET except NUL or cookie-sep>   ; CR | LF removed by necko
    separators    = ";" | "=" | LWS
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
    cookie        = NAME ["=" VALUE] *(";" cookie-av)    ; cookie NAME/VALUE must come first
    NAME          = token                                ; cookie name
    VALUE         = value                                ; cookie value
    cookie-av     = token ["=" value]

    valid values for cookie-av (checked post-parsing) are:
    cookie-av     = "Path" "=" value
                  | "Domain"  "=" value
                  | "Expires" "=" value
                  | "Max-Age" "=" value
                  | "Comment" "=" value
                  | "Version" "=" value
                  | "Secure"

******************************************************************************/

// helper functions for cookie_GetTokenValue
PRIVATE inline PRBool iswhitespace     (char c) { return c == ' '  || c == '\t'; }
PRIVATE inline PRBool isterminator     (char c) { return c == '\n' || c == '\r'; }
PRIVATE inline PRBool isquoteterminator(char c) { return isterminator(c) || c == '"'; }
PRIVATE inline PRBool isvalueseparator (char c) { return isterminator(c) || c == ';'; }
PRIVATE inline PRBool istokenseparator (char c) { return isvalueseparator(c) || iswhitespace(c) || c == '='; }

// Parse a single token/value pair.
// Returns PR_TRUE if a cookie terminator is found, so caller can parse new cookie.
PRIVATE PRBool
cookie_GetTokenValue(nsASingleFragmentCString::const_char_iterator &aIter,
                     nsASingleFragmentCString::const_char_iterator &aEndIter,
                     nsDependentSingleFragmentCSubstring &aTokenString,
                     nsDependentSingleFragmentCSubstring &aTokenValue)
{
  nsASingleFragmentCString::const_char_iterator start;
  // initialize value string to clear garbage
  aTokenValue.Rebind(aIter, aIter);

  // find <token>
  while (aIter != aEndIter && iswhitespace(*aIter))
    ++aIter;
  start = aIter;
  while (aIter != aEndIter && !istokenseparator(*aIter))
    ++aIter;
  aTokenString.Rebind(start, aIter);

  // now expire whitespace to see if '=' awaits us
  while (aIter != aEndIter && iswhitespace(*aIter)) // skip over spaces at end of cookie name
    ++aIter;

  if (*aIter == '=') {
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
        nsASingleFragmentCString::const_char_iterator lastSpace = aIter;
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
PRIVATE PRBool
cookie_ParseAttributes(nsDependentCString  &aCookieHeader,
                       cookie_CookieStruct *aCookie,
                       nsACString          &aExpiresAttribute,
                       nsACString          &aMaxageAttribute)
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

  aCookie->isSecure = PR_FALSE;

  nsDependentSingleFragmentCSubstring tokenString(cookieStart, cookieStart);
  nsDependentSingleFragmentCSubstring tokenValue (cookieStart, cookieStart);
  PRBool newCookie;

  // extract cookie NAME & VALUE (first attribute), and copy the strings
  // if we find multiple cookies, return for processing
  // note: if there's no '=', we assume token is NAME, not VALUE.
  //       the old code assumed VALUE instead.
  newCookie = cookie_GetTokenValue(cookieStart, cookieEnd, tokenString, tokenValue);
  aCookie->name = tokenString;
  aCookie->cookie = tokenValue;

  // extract remaining attributes
  while (cookieStart != cookieEnd && !newCookie) {
    newCookie = cookie_GetTokenValue(cookieStart, cookieEnd, tokenString, tokenValue);

    if (!tokenValue.IsEmpty() && *tokenValue.BeginReading(tempBegin) == '"'
                              && *tokenValue.EndReading(tempEnd) == '"') {
      // our parameter is a quoted-string; remove quotes for later parsing
      tokenValue.Rebind(++tempBegin, --tempEnd);
    }

    // decide which attribute we have, and copy the string
    if (tokenString.Equals(kPath, nsCaseInsensitiveCStringComparator()))
      aCookie->path = tokenValue;

    else if (tokenString.Equals(kDomain, nsCaseInsensitiveCStringComparator()))
      aCookie->host = tokenValue;

    else if (tokenString.Equals(kExpires, nsCaseInsensitiveCStringComparator()))
      aExpiresAttribute = tokenValue;

    else if (tokenString.Equals(kMaxage, nsCaseInsensitiveCStringComparator()))
      aMaxageAttribute = tokenValue;

    // ignore any tokenValue for isSecure; just set the boolean
    else if (tokenString.Equals(kSecure, nsCaseInsensitiveCStringComparator()))
      aCookie->isSecure = PR_TRUE;
  }

  // rebind aCookieHeader, in case we need to process another cookie
  aCookieHeader.Rebind(cookieStart, cookieEnd);
  return newCookie;
}

/******************************************************************************
 * nsCookies impl: main Get() and Set() functions
 *****************************************************************************/

PRIVATE nsCookieStatus
cookie_CheckPrefs(nsIURI         *aHostURI,
                  nsIURI         *aFirstURI,
                  nsIHttpChannel *aHttpChannel,
                  const char     *aCookieHeader)
{
  // pref tree:
  // 0) get the scheme strings from the two URI's
  // 1) disallow ftp
  // 2) disallow mailnews, if pref set
  // 3) go thru enumerated permissions to see which one we have:
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
    COOKIE_LOGFAILURE(aCookieHeader ? SET_COOKIE : GET_COOKIE, aHostURI, aCookieHeader ? "" : aCookieHeader, "couldn't get scheme of host URI");
    return nsICookie::STATUS_REJECTED;
  }

  // don't let ftp sites get/set cookies (could be a security issue)
  if (currentURIScheme.Equals(NS_LITERAL_CSTRING("ftp"))) {
    COOKIE_LOGFAILURE(aCookieHeader ? SET_COOKIE : GET_COOKIE, aHostURI, aCookieHeader ? "" : aCookieHeader, "ftp sites cannot read cookies");
    return nsICookie::STATUS_REJECTED;
  }

  // disable cookies in mailnews if user's prefs say so.
  // we perform a fairly strict test here:
  // if firstURI is null, we can't tell if it's from mailnews - reject
  // otherwise, we test both firstURI and currentURI.
  // this needs to be revisited when we get firstURI behaving correctly
  // in all cases.
  if (gCookiePrefObserver->mCookiesDisabledForMailNews &&
      (!aFirstURI ||
       cookie_IsFromMailNews(firstURIScheme) ||
       cookie_IsFromMailNews(currentURIScheme))) {
    COOKIE_LOGFAILURE(aCookieHeader ? SET_COOKIE : GET_COOKIE, aHostURI, aCookieHeader ? "" : aCookieHeader, "cookies disabled for mailnews");
    return nsICookie::STATUS_REJECTED;
  }

  // check default prefs - go thru enumerated permissions
  if (gCookiePrefObserver->mCookiesPermissions == PERMISSION_DontUse) {
    COOKIE_LOGFAILURE(aCookieHeader ? SET_COOKIE : GET_COOKIE, aHostURI, aCookieHeader ? "" : aCookieHeader, "cookies are disabled");
    return nsICookie::STATUS_REJECTED;

  } else if (gCookiePrefObserver->mCookiesPermissions == PERMISSION_DontAcceptForeign) {
    // check if cookie is foreign.
    // if aFirstURI is null, deny by default

    // note: this can be circumvented if we have http redirects within html,
    // since the documentURI attribute isn't always correctly
    // passed to the redirected channels. (or isn't correctly set in the first place)
    if (cookie_IsForeign(aHostURI, aFirstURI)) {
      COOKIE_LOGFAILURE(aCookieHeader ? SET_COOKIE : GET_COOKIE, aHostURI, aCookieHeader ? "" : aCookieHeader, "originating server test failed");
      return nsICookie::STATUS_REJECTED;
    }

  } else if (gCookiePrefObserver->mCookiesPermissions == PERMISSION_P3P) {
    // check to see if P3P conditions are satisfied.
    // nsCookieStatus is an enumerated type, defined in nsCookie.idl (frozen interface):
    // STATUS_UNKNOWN -- cookie collected in a previous session and this info no longer available
    // STATUS_ACCEPTED -- cookie was accepted
    // STATUS_DOWNGRADED -- cookie was accepted but downgraded to a session cookie
    // STATUS_FLAGGED -- cookie was accepted with a warning being issued to the user
    // STATUS_REJECTED

    // to do this, at the moment, we need an httpChannel, but we can live without
    // the two URI's (as long as no foreign checks are required).
    // if the channel is null, we can fall back on "no p3p policy" prefs.
    nsCookieStatus p3pStatus = cookie_P3PDecision(aHostURI, aFirstURI, aHttpChannel);
    if (p3pStatus == nsICookie::STATUS_REJECTED) {
      COOKIE_LOGFAILURE(aCookieHeader ? SET_COOKIE : GET_COOKIE, aHostURI, aCookieHeader ? "" : aCookieHeader, "P3P test failed");
    }
    return p3pStatus;
  }

  // if nothing has complained, accept cookie
  return nsICookie::STATUS_ACCEPTED;
}

PUBLIC char *
COOKIE_GetCookie(nsIURI *aHostURI,
                 nsIURI *aFirstURI)
{
  if (!aHostURI) {
    COOKIE_LOGFAILURE(GET_COOKIE, nsnull, "", "host URI is null");
    return nsnull;
  }

  // check default prefs
  nsCookieStatus cookieStatus = cookie_CheckPrefs(aHostURI, aFirstURI, nsnull, nsnull);
  // for GetCookie(), we don't update the UI icon if cookie was rejected.
  if (cookieStatus == nsICookie::STATUS_REJECTED) {
    return nsnull;
  }

  // get host and path from the nsIURI
  // note: there was a "check if host has embedded whitespace" here.
  // it was removed since this check was added into the nsIURI impl (bug 146094).
  nsCAutoString hostFromURI, pathFromURI;
  if (NS_FAILED(aHostURI->GetAsciiHost(hostFromURI)) ||
      NS_FAILED(aHostURI->GetPath(pathFromURI))) {
    COOKIE_LOGFAILURE(GET_COOKIE, aHostURI, "", "couldn't get host/path from URI");
    return nsnull;
  }
  // trim trailing dots
  hostFromURI.Trim(".");
  ToLowerCase(hostFromURI);

  // initialize variables used in the list traversal
  nsInt64 currentTime = NOW_IN_SECONDS;
  cookie_CookieStruct *cookieInList;
  // initialize string for return data
  nsCAutoString cookieData;

  // check if aHostURI is using an https secure protocol.
  // if it isn't, then we can't send a secure cookie over the connection.
  // if SchemeIs fails, assume an insecure connection, to be on the safe side
  PRBool isSecure;
  if NS_FAILED(aHostURI->SchemeIs("https", &isSecure)) {
    isSecure = PR_FALSE;
  }

  // begin sCookieList traversal
  PRInt32 count = sCookieList->Count();
  for (PRInt32 i = 0; i < count; ++i) {
    cookieInList = NS_STATIC_CAST(cookie_CookieStruct*, sCookieList->ElementAt(i));
    NS_ASSERTION(cookieInList, "corrupt cookie list");

    // if the cookie is secure and the host scheme isn't, we can't send it
    if (cookieInList->isSecure & !isSecure) {
      continue;
    }

    // check if the host is in the cookie's domain
    // (taking into account whether it's a domain cookie)
    if (!cookie_IsInDomain(cookieInList->host, hostFromURI, cookieInList->isDomain)) {
      continue;
    }

    // calculate cookie path length, excluding trailing '/'
    PRUint32 cookiePathLen = cookieInList->path.Length();
    if (cookiePathLen > 0 && cookieInList->path.Last() == '/') {
      --cookiePathLen;
    }

    // the cookie list is in order of path length; longest to shortest.
    // if the nsIURI path is shorter than the cookie path, then we know the path
    // isn't on the cookie path.
    if (!Substring(pathFromURI, 0, cookiePathLen).Equals(Substring(cookieInList->path, 0, cookiePathLen))) {
      continue;
    }

    char pathLastChar = pathFromURI.CharAt(cookiePathLen);
    if (pathFromURI.Length() > cookiePathLen &&
        pathLastChar != '/' && pathLastChar != '?' && pathLastChar != '#' && pathLastChar != ';') {
      /*
       * note that the '?' test above allows a site at host/abc?def to receive a cookie that
       * has a path attribute of abc.  This seems strange but at least one major site
       * (citibank, bug 156725) depends on it.  The test for # and ; are put in to proactively
       * avoid problems with other sites - these are the only other chars allowed in the path.
       */
      continue;
    }

    // check if the cookie has expired, and remove if so.
    // note that we do this *after* previous tests passed - so we're only removing
    // the ones that are relevant to this particular search.
    if (!cookieInList->isSession && nsInt64(cookieInList->expires) < currentTime) {
      sCookieList->RemoveElementAt(i--); // decrement i so next cookie isn't skipped
      --count; // update the count
      delete cookieInList;
      sCookieChanged = PR_TRUE;
      continue;
    }

    // all checks passed - update lastAccessed stamp of cookie
    cookieInList->lastAccessed = currentTime;

    if (!cookieInList->name.IsEmpty()) {
      // if we've already added a cookie to the return list, append a "; " so
      // that subsequent cookies are delimited in the final list.
      if (!cookieData.IsEmpty()) {
        cookieData += NS_LITERAL_CSTRING("; ");
      }

      // NOTE: we used to have an #ifdef PREVENT_DUPLICATE_NAMES here,
      // which would prevent multiple cookies with the same name being sent (i.e.
      // only the first instance is sent). This wasn't invoked in our previous code,
      // and RFC2109 implicitly allows duplicate names, so I've removed it.

      // if we have just a cookie->name, don't write the last portion.
      if (cookieInList->cookie.IsEmpty()) {
        cookieData += cookieInList->name;
      } else {
        cookieData += cookieInList->name + NS_LITERAL_CSTRING("=") + cookieInList->cookie;
      }
    }
  } // for()

  // it's wasteful to alloc a new string; but we have no other choice, until we
  // fix the callers to use nsACStrings.
  if (!cookieData.IsEmpty()) {
    COOKIE_LOGSUCCESS(GET_COOKIE, aHostURI, cookieData, nsnull);
    return ToNewCString(cookieData);
  }

  return nsnull;
}

// processes domain attribute, and returns PR_TRUE if host has permission to set for this domain.
PRIVATE inline PRBool
cookie_CheckDomain(cookie_CookieStruct *aCookie,
                   nsIURI              *aHostURI)
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
  if (!aCookie->host.IsEmpty()) {
    // switch to lowercase now, to avoid case-insensitive compares everywhere
    ToLowerCase(aCookie->host);

    // check whether the host is an IP address, and override isDomain to
    // make the cookie a non-domain one. this will require an exact host
    // match for the cookie, so we eliminate any chance of IP address
    // funkiness (e.g. the alias 127.1 domain-matching 99.54.127.1).
    // bug 105917 originally noted the requirement to deal with IP addresses.
    if (cookie_IsIPAddress(aCookie->host)) {
      aCookie->isDomain = PR_FALSE;
      return cookie_IsInDomain(aCookie->host, hostFromURI, PR_FALSE);
    }

    /*
     * verify that this host has the authority to set for this domain.   We do
     * this by making sure that the host is in the domain.  We also require
     * that a domain have at least one embedded period to prevent domains of the form
     * ".com" and ".edu"
     */
    aCookie->host.Trim(".");
    PRInt32 dot = aCookie->host.FindChar('.');
    if (dot == kNotFound) {
      // fail dot test
      return PR_FALSE;
    }

    // prepend a dot, and check if the host is in the domain
    aCookie->isDomain = PR_TRUE;
    aCookie->host.Insert(NS_LITERAL_CSTRING("."), 0);
    if (!cookie_IsInDomain(aCookie->host, hostFromURI)) {
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
    if (gCookiePrefObserver->mCookiesStrictDomains) {
      dot = hostFromURI.FindChar('.', 0, hostFromURI.Length() - aCookie->host.Length());
      if (dot != kNotFound) {
        return PR_FALSE;
      }
    }

  // no domain specified, use hostFromURI
  } else {
    aCookie->isDomain = PR_FALSE;
    aCookie->host = hostFromURI;
  }

  return PR_TRUE;
}

PRIVATE inline PRBool
cookie_CheckPath(cookie_CookieStruct *aCookie,
                 nsIURI              *aHostURI)
{
  // if a path is given, check the host has permission
  if (aCookie->path.IsEmpty()) {
    // strip down everything after the last slash to get the path,
    // ignoring slashes in the query string part.
    // if we can QI to nsIURL, that'll take care of the query string portion.
    // otherwise, it's not an nsIURL and can't have a query string, so just find the last slash.
    nsCOMPtr<nsIURL> hostURL = do_QueryInterface(aHostURI);
    if (hostURL) {
      hostURL->GetDirectory(aCookie->path);
    } else {
      PRInt32 slash = aCookie->path.RFindChar('/');
      if (slash != kNotFound) {
        aCookie->path.Truncate(slash + 1);
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
        !Substring(pathFromURI, 0, aCookie->path.Length()).Equals(aCookie->path)) {
      return PR_FALSE;
    }
#endif
  }

  return PR_TRUE;
}

PRIVATE inline PRBool
cookie_GetExpiry(const nsAFlatCString &maxageAttribute,
                 const nsAFlatCString &expiresAttribute,
                 nsInt64              serverTime,
                 nsInt64              &expiryTime,
                 nsInt64              currentTime,
                 nsCookieStatus       aStatus)
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
  if (!maxageAttribute.IsEmpty()) {
    // obtain numeric value of maxageAttribute
    PRInt64 maxage;
    PRInt32 numInts = PR_sscanf(maxageAttribute.get(), "%lld", &maxage);

    // default to session cookie if the conversion failed
    if (numInts != 1) {
      goto session_cookie;
    }

    delta = nsInt64(maxage);

  // check for expires attribute
  } else if (!expiresAttribute.IsEmpty()) {
    nsInt64 expires;
    PRTime tempExpires;

    // parse expiry time
    if (PR_ParseTimeString(expiresAttribute.get(), PR_TRUE, &tempExpires) == PR_SUCCESS) {
      expires = nsInt64(tempExpires) / USEC_PER_SEC;
    } else {
      goto session_cookie;
    }

    delta = expires - serverTime;

  // default to session cookie if no attributes found
  } else {
    goto session_cookie;
  }

  if (delta <= nsInt64(0)) {
    goto expire_cookie;
  }

  // check cookie lifetime pref, and limit lifetime if required
  if (gCookiePrefObserver->mCookiesLifetimeEnabled) {
    if (gCookiePrefObserver->mCookiesLifetimeCurrentSession) {
      // limit lifetime to session
      goto session_cookie;
    } else if (delta > nsInt64(gCookiePrefObserver->mCookiesLifetimeSec)) {
      // limit lifetime to specified time
      delta = gCookiePrefObserver->mCookiesLifetimeSec;
    }
  }

  // if overflow, set to session cookie... not ideal, but it'll work.
  expiryTime = currentTime + delta;
  if (expiryTime < currentTime) {
    goto session_cookie;
  }

  // default case (proper expiry time).
  // the cookie may have been previously downgraded by p3p prefs,
  // so we take that into account here.
  return (aStatus == nsICookie::STATUS_DOWNGRADED);

  session_cookie:
    return PR_TRUE;

  expire_cookie:
    // make sure the cookie is marked appropriately, for expiry.
    // we can't just return now, because we might need to delete a previous cookie
    expiryTime = currentTime - nsInt64(1);
    return PR_FALSE;
}

// helper function to copy data from a cookie_CookieStruct into an nsICookie.
// also takes care of altering the expiry time, which has different interpretation
// between the two formats.
PUBLIC already_AddRefed<nsICookie>
COOKIE_ChangeFormat(cookie_CookieStruct *aCookie)
{
  // note that we must provide compatibility with the "old" method of
  // denoting session cookies, where expiryTime = 0.
  // note that we need to cast from PRInt64 to PRUint64...
  PRUint64 expiryTimeCompat;
  if (aCookie->isSession) {
    expiryTimeCompat = 0;
  } else {
    expiryTimeCompat = nsInt64(aCookie->expires) > nsInt64(0) ? aCookie->expires : 1;
  }

  // copy the information into the nsICookie
  nsICookie *cookie = new nsCookie(aCookie->name,
                                   aCookie->cookie,
                                   aCookie->isDomain,
                                   aCookie->host,
                                   aCookie->path,
                                   aCookie->isSecure,
                                   expiryTimeCompat,
                                   aCookie->status,
                                   aCookie->policy);

  NS_IF_ADDREF(cookie);
  return cookie;
}

// processes a single cookie, and returns PR_TRUE if there are more cookies
// to be processed
PRIVATE PRBool
cookie_SetCookieInternal(nsIURI             *aHostURI,
                         nsDependentCString &aCookieHeader,
                         nsInt64            aServerTime,
                         nsCookieStatus     aStatus,
                         nsCookiePolicy     aPolicy)
{
  nsresult rv;
  // keep a |const char*| version of the unmodified aCookieHeader,
  // for logging purposes
  const char *cookieHeader = aCookieHeader.get();

  nsInt64 expiryTime, currentTime = NOW_IN_SECONDS;
  PRBool isSession, foundCookie;

  nsCOMPtr<nsICookie> thisCookie;
  nsCOMPtr<nsICookiePermission> cookiePermission;

  // cookie stores all the attributes parsed from the cookie;
  // expires and maxage are separate, because we have to process them to find the expiry.
  // create a new cookie to store the attributes
  cookie_CookieStruct *cookie = new cookie_CookieStruct;
  if (!cookie) {
    COOKIE_LOGFAILURE(SET_COOKIE, aHostURI, cookieHeader, "unable to allocate memory for new cookie");
    // if we fail here, abort all processing of subsequent cookies by SetCookie()
    return PR_FALSE;
  }
  nsCAutoString expiresAttribute, maxageAttribute;

  // newCookie says whether there are multiple cookies in the header; so we can handle them separately.
  // after this function, we don't need the cookieHeader string for processing this cookie anymore;
  // so this function uses it as an outparam to point to the next cookie, if there is one.
  PRBool newCookie = cookie_ParseAttributes(aCookieHeader, cookie, expiresAttribute, maxageAttribute);

  // reject cookie if it's over the size limit, per RFC2109
  if ((cookie->name.Length() + cookie->cookie.Length()) > MAX_BYTES_PER_COOKIE) {
    COOKIE_LOGFAILURE(SET_COOKIE, aHostURI, cookieHeader, "cookie too big (> 4kb)");
    goto failure;
  }

  // calculate expiry time of cookie. we need to pass in cookieStatus, since
  // the cookie may have been downgraded to a session cookie by p3p.
  isSession = cookie_GetExpiry(maxageAttribute, expiresAttribute, aServerTime,
                               expiryTime, currentTime, aStatus);

  // put remaining information into cookieStruct
  cookie->expires = expiryTime;
  cookie->lastAccessed = currentTime;
  cookie->isSession = isSession;
  cookie->status = aStatus;
  cookie->policy = aPolicy;

  // domain & path checks
  if (!cookie_CheckDomain(cookie, aHostURI)) {
    COOKIE_LOGFAILURE(SET_COOKIE, aHostURI, cookieHeader, "failed the domain tests");
    goto failure;
  }
  if (!cookie_CheckPath(cookie, aHostURI)) {
    COOKIE_LOGFAILURE(SET_COOKIE, aHostURI, cookieHeader, "failed the path tests");
    goto failure;
  }

  // count the number of cookies from this host, and find whether a previous cookie
  // has been set, for prompting purposes
  PRUint32 countFromHost;
  foundCookie = cookie_FindCookiesFromHost(cookie, countFromHost, currentTime);

  // check if the cookie we're trying to set is already expired, and return.
  // but we need to check there's no previous cookie first, because servers use
  // this as a trick for deleting previous cookies.
  if (!foundCookie && !cookie->isSession && nsInt64(cookie->expires) < currentTime) {
    COOKIE_LOGFAILURE(SET_COOKIE, aHostURI, cookieHeader, "cookie has already expired");
    goto failure;
  }

  // create a new nsICookie and copy the cookie data,
  // for passing to the permission manager
  thisCookie = COOKIE_ChangeFormat(cookie);

  // we want to cache this ptr when we merge everything into nsCookieService
  cookiePermission = do_GetService(NS_COOKIEPERMISSION_CONTRACTID, &rv);
  // check permissions from site permission list, or ask the user,
  // to determine if we can set the cookie
  if (NS_SUCCEEDED(rv)) {
    PRBool permission;
    // we need to think about prompters/parent windows here - TestPermission
    // needs one to prompt, so right now it has to fend for itself to get one
    rv = cookiePermission->TestPermission(aHostURI, thisCookie, nsnull,
                                          countFromHost, foundCookie,
                                          gCookiePrefObserver->mCookiesAskPermission,
                                          &permission);
    if (!permission) {
      COOKIE_LOGFAILURE(SET_COOKIE, aHostURI, cookieHeader, "cookies are blocked for this site");
      goto failure;
    }
  }

  // add the cookie to the list
  rv = COOKIE_Add(cookie, NOW_IN_SECONDS, aHostURI, cookieHeader);
  if (NS_FAILED(rv)) {
    // no need to log a failure here, Add() does it for us
    goto failure;
  }

  // notify observers if the cookie was downgraded or flagged (only for p3p
  // at this point). this occurs only if the cookie was set successfully.
  if (aStatus == nsICookie::STATUS_DOWNGRADED ||
      aStatus == nsICookie::STATUS_FLAGGED) {
    nsCOMPtr<nsIObserverService> os(do_GetService("@mozilla.org/observer-service;1"));
    if (os) {
      os->NotifyObservers(nsnull, "cookieIcon", NS_LITERAL_STRING("on").get());
    }
  }

  COOKIE_LOGSUCCESS(SET_COOKIE, aHostURI, cookieHeader, cookie);
  return newCookie;

  // something failed - free the cookie
  failure:
    delete cookie;
    return newCookie;
}

// performs functions common to all cookies (checking user prefs and processing
// the time string from the server), and processes each cookie in the header.
PUBLIC void
COOKIE_SetCookie(nsIURI         *aHostURI,
                 nsIURI         *aFirstURI,
                 nsIPrompt      *aPrompt,
                 const char     *aCookieHeader,
                 const char     *aServerTime,
                 nsIHttpChannel *aHttpChannel)
{
  if (!aHostURI) {
    COOKIE_LOGFAILURE(SET_COOKIE, nsnull, aCookieHeader, "host URI is null");
    return;
  }

  // check default prefs
  nsCookieStatus cookieStatus = cookie_CheckPrefs(aHostURI, aFirstURI, aHttpChannel, aCookieHeader);
  // update UI icon, and return, if cookie was rejected.
  // should we be doing this just for p3p?
  if (cookieStatus == nsICookie::STATUS_REJECTED) {
    nsCOMPtr<nsIObserverService> os(do_GetService("@mozilla.org/observer-service;1"));
    if (os) {
      os->NotifyObservers(nsnull, "cookieIcon", NS_LITERAL_STRING("on").get());
    }
    return;
  }
  // get the site's p3p policy now (common to all cookies)
  nsCookiePolicy cookiePolicy = cookie_GetPolicy(P3P_SitePolicy(aHostURI, aHttpChannel));

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
  while (cookie_SetCookieInternal(aHostURI,
                                  cookieHeader, serverTime,
                                  cookieStatus, cookiePolicy));
}

/******************************************************************************
 * nsCookies impl: file I/O functions
 *****************************************************************************/

// comparison function for sorting cookies by lastAccessed time:
// returns <0 if the first element was used more recently than the second element,
//          0 if they both have the same last-use time,
//         >0 if the second element was used more recently than the first element.
PR_STATIC_CALLBACK(int) compareCookiesByLRU(const void *aElement1, const void *aElement2, void *aData)
{
  const cookie_CookieStruct *cookie1 = NS_STATIC_CAST(const cookie_CookieStruct*, aElement1);
  const cookie_CookieStruct *cookie2 = NS_STATIC_CAST(const cookie_CookieStruct*, aElement2);
  NS_ASSERTION(cookie1 && cookie2, "corrupt cookie list");

  // we may have overflow problems returning the result directly, so we need branches
  nsInt64 difference = nsInt64(cookie2->lastAccessed) - nsInt64(cookie1->lastAccessed);
  return (difference > nsInt64(0)) ? 1 : (difference < nsInt64(0)) ? -1 : 0;
}

// comparison function for sorting cookies by path length:
// returns <0 if the first element has a greater path length than the second element,
//          0 if they both have the same path length,
//         >0 if the second element has a greater path length than the first element.
PR_STATIC_CALLBACK(int) compareCookiesByPath(const void *aElement1, const void *aElement2, void *aData)
{
  const cookie_CookieStruct *cookie1 = NS_STATIC_CAST(const cookie_CookieStruct*, aElement1);
  const cookie_CookieStruct *cookie2 = NS_STATIC_CAST(const cookie_CookieStruct*, aElement2);
  NS_ASSERTION(cookie1 && cookie2, "corrupt cookie list");

  return cookie2->path.Length() - cookie1->path.Length();
}

// writes cookies to the cookie file
PUBLIC nsresult
COOKIE_Write()
{
  if (!sCookieChanged) {
    return NS_OK;
  }

  nsresult rv;
  nsCOMPtr<nsIFile> file;
  rv = NS_GetSpecialDirectory(NS_APP_USER_PROFILE_50_DIR, getter_AddRefs(file));
  if (NS_SUCCEEDED(rv)) {
    rv = file->AppendNative(NS_LITERAL_CSTRING(kCookieFileName));
  }
  if (NS_FAILED(rv)) {
    return rv;
  }

  nsCOMPtr<nsIOutputStream> fileOutputStream;
  rv = NS_NewLocalFileOutputStream(getter_AddRefs(fileOutputStream), file);
  if (NS_FAILED(rv)) {
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
  sortedCookieList = *sCookieList;
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
  cookie_CookieStruct *cookieInList;
  nsInt64 currentTime = NOW_IN_SECONDS;
  PRInt32 count = sortedCookieList.Count();
  for (PRInt32 i = 0; i < count; ++i) {
    cookieInList = NS_STATIC_CAST(cookie_CookieStruct*, sortedCookieList.ElementAt(i));
    NS_ASSERTION(cookieInList, "corrupt cookie list");

    // don't write entry if cookie has expired, or is a session cookie
    if (cookieInList->isSession || nsInt64(cookieInList->expires) < currentTime) {
      continue;
    }

    bufferedOutputStream->Write(cookieInList->host.get(), cookieInList->host.Length(), &rv);
    if (cookieInList->isDomain) {
      bufferedOutputStream->Write(kTrue, sizeof(kTrue) - 1, &rv);
    } else {
      bufferedOutputStream->Write(kFalse, sizeof(kFalse) - 1, &rv);
    }
    bufferedOutputStream->Write(cookieInList->path.get(), cookieInList->path.Length(), &rv);
    if (cookieInList->isSecure) {
      bufferedOutputStream->Write(kTrue, sizeof(kTrue) - 1, &rv);
    } else {
      bufferedOutputStream->Write(kFalse, sizeof(kFalse) - 1, &rv);
    }
    char dateString[22];
    PRUint32 dateLen = PR_snprintf(dateString, sizeof(dateString), "%lld", cookieInList->expires);
    bufferedOutputStream->Write(dateString, dateLen, &rv);
    bufferedOutputStream->Write(kTab, sizeof(kTab) - 1, &rv);
    bufferedOutputStream->Write(cookieInList->name.get(), cookieInList->name.Length(), &rv);
    bufferedOutputStream->Write(kTab, sizeof(kTab) - 1, &rv);
    bufferedOutputStream->Write(cookieInList->cookie.get(), cookieInList->cookie.Length(), &rv);
    bufferedOutputStream->Write(kNew, sizeof(kNew) - 1, &rv);
  }

  sCookieChanged = PR_FALSE;
  return NS_OK;
}

// reads cookies from the cookie file
PUBLIC nsresult
COOKIE_Read()
{
  // if sCookieList is non-empty, we've already read. we can remove this check
  // once we're sure this function gets called in only the appropriate places
  if (sCookieList->Count() > 0) {
    return NS_OK;
  }

  nsresult rv;
  nsCOMPtr<nsIFile> file;
  rv = NS_GetSpecialDirectory(NS_APP_USER_PROFILE_50_DIR, getter_AddRefs(file));
  if (NS_SUCCEEDED(rv)) {
    rv = file->AppendNative(NS_LITERAL_CSTRING(kCookieFileName));
  }
  if (NS_FAILED(rv)) {
    return rv;
  }

  nsCOMPtr<nsIInputStream> fileInputStream;
  rv = NS_NewLocalFileInputStream(getter_AddRefs(fileInputStream), file);
  if (NS_FAILED(rv)) {
    return rv;
  }

  nsCOMPtr<nsILineInputStream> lineInputStream = do_QueryInterface(fileInputStream, &rv);
  if (NS_FAILED(rv)) {
    return rv;
  }

  static NS_NAMED_LITERAL_CSTRING(kTrue, "TRUE");

  nsInt64 currentTime = NOW_IN_SECONDS;
  // we use lastAccessedCounter to keep cookies in recently-used order,
  // so we start by initializing to currentTime (somewhat arbitrary)
  nsInt64 lastAccessedCounter = currentTime;
  PRInt64 expires;
  cookie_CookieStruct *newCookie;
  nsAutoString bufferUnicode;
  nsCAutoString buffer, expiresString;
  PRBool isMore = PR_TRUE;
  PRInt32 hostIndex = 0, isDomainIndex, pathIndex, secureIndex, expiresIndex, nameIndex, cookieIndex;

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
    expiresString = Substring(buffer, expiresIndex, nameIndex - expiresIndex - 1);
    PRInt32 numInts = PR_sscanf(expiresString.get(), "%lld", &expires);
    if (numInts != 1 || nsInt64(expires) < currentTime) {
      continue;
    }

    // create a new cookieStruct and assign the data
    newCookie = new cookie_CookieStruct;
    if (!newCookie) {
      return NS_ERROR_OUT_OF_MEMORY;
    }

    newCookie->host     = Substring(buffer, hostIndex, isDomainIndex - hostIndex - 1);
    newCookie->isDomain = Substring(buffer, isDomainIndex, pathIndex - isDomainIndex - 1).Equals(kTrue);
    newCookie->path     = Substring(buffer, pathIndex, secureIndex - pathIndex - 1);
    newCookie->isSecure = Substring(buffer, secureIndex, expiresIndex - secureIndex - 1).Equals(kTrue);
    newCookie->name     = Substring(buffer, nameIndex, cookieIndex - nameIndex - 1);
    newCookie->cookie   = Substring(buffer, cookieIndex, buffer.Length() - cookieIndex);
    newCookie->isSession = PR_FALSE;
    newCookie->status = nsICookie::STATUS_UNKNOWN;
    newCookie->policy = nsICookie::POLICY_UNKNOWN;
    newCookie->expires = expires;
    // trick: keep the cookies in most-recently-used order,
    // by successively decrementing the lastAccessed time
    newCookie->lastAccessed = lastAccessedCounter;
    lastAccessedCounter -= nsInt64(1);

    // check for bad legacy cookies (domain not starting with a dot, or containing a port),
    // and discard
    if (newCookie->isDomain && !newCookie->host.IsEmpty() && newCookie->host.First() != '.' ||
        newCookie->host.FindChar(':') != kNotFound) {
      delete newCookie;
      continue;
    }

    // add new cookie to the list
    sCookieList->AppendElement(newCookie);
  }

  // sort sCookieList in order of descending path length
  sCookieList->Sort(compareCookiesByPath, nsnull);

  sCookieChanged = PR_FALSE;
  return NS_OK;
}
