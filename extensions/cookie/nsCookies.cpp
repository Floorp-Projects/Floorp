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
 *   Henrik Gemal <gemal@gemal.dk>
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
#include "nsPermissions.h"
#include "nsUtils.h"
#include "nsIFileSpec.h"
#include "nsVoidArray.h"
#include "prprf.h"
#include "xp_core.h"
#include "prmem.h"
#include "nsXPIDLString.h"
#include "nsReadableUtils.h"
#include "nsIPref.h"
#include "nsTextFormatter.h"
#include "nsAppDirectoryServiceDefs.h"

#define MAX_NUMBER_OF_COOKIES 300
#define MAX_COOKIES_PER_SERVER 20
#define MAX_BYTES_PER_COOKIE 4096  /* must be at least 1 */

#define cookie_behaviorPref "network.cookie.cookieBehavior"
#define cookie_warningPref "network.cookie.warnAboutCookies"
#define cookie_strictDomainsPref "network.cookie.strictDomains"
#define cookie_lifetimePref "network.cookie.lifetimeOption"
#define cookie_lifetimeValue "network.cookie.lifetimeLimit"

#define cookie_lifetimeEnabledPref "network.cookie.lifetime.enabled"
#define cookie_lifetimeBehaviorPref "network.cookie.lifetime.behavior"
#define cookie_lifetimeDaysPref "network.cookie.lifetime.days"
#define cookie_p3pPref "network.cookie.p3p"

static const char *kCookiesFileName = "cookies.txt";

MODULE_PRIVATE time_t 
cookie_ParseDate(char *date_string);

typedef struct _cookie_CookieStruct {
  char * path;
  char * host;
  char * name;
  char * cookie;
  time_t expires;
  time_t lastAccessed;
  PRBool isSecure;
  PRBool isDomain;   /* is it a domain instead of an absolute host? */
} cookie_CookieStruct;

typedef enum {
  COOKIE_Normal,
  COOKIE_Discard,
  COOKIE_Trim,
  COOKIE_Ask
} COOKIE_LifetimeEnum;

PRIVATE PRBool cookie_changed = PR_FALSE;
PRIVATE PERMISSION_BehaviorEnum cookie_behavior = PERMISSION_Accept;
PRIVATE PRBool cookie_warning = PR_FALSE;
PRIVATE COOKIE_LifetimeEnum cookie_lifetimeOpt = COOKIE_Normal;
PRIVATE time_t cookie_lifetimeLimit = 90*24*60*60;
PRIVATE time_t cookie_lifetimeDays;
PRIVATE PRBool cookie_lifetimeCurrentSession;

PRIVATE char* cookie_P3P = nsnull;

/* cookie_P3P (above) consists of 8 characters having the following interpretation:
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

#define P3P_NoPolicy         0
#define P3P_NoConsent        2
#define P3P_ImplicitConsent  4
#define P3P_ExplicitConsent  6

#define P3P_Accept     'a'
#define P3P_Downgrade  'd'
#define P3P_Reject     'r'

#define cookie_P3P_Default    "drdraaaa"

PRIVATE nsVoidArray * cookie_list=0;

static
time_t
get_current_time()
    /*
      We call this routine instead of |time()| because the latter returns
      different values on the Mac than on all other platforms (i.e., based on
      the Mac's 1900 epoch, vs all others 1970).  We can't call |PR_Now|
      directly, since the value is 64bits and too hard to manipulate.
      Hence, this cross-platform convenience routine.
    */
  {
    PRInt64 usecPerSec;
    LL_I2L(usecPerSec, 1000000L);

    PRTime now_useconds = PR_Now();

    PRInt64 now_seconds;
    LL_DIV(now_seconds, now_useconds, usecPerSec);

    time_t current_time_in_seconds;
    LL_L2I(current_time_in_seconds, now_seconds);

    return current_time_in_seconds;
  }

PRBool PR_CALLBACK deleteCookie(void *aElement, void *aData) {
  cookie_CookieStruct *cookie = (cookie_CookieStruct*)aElement;
  PR_FREEIF(cookie->path);
  PR_FREEIF(cookie->host);
  PR_FREEIF(cookie->name);
  PR_FREEIF(cookie->cookie);
  PR_Free(cookie);
  return PR_TRUE;
}

PUBLIC void
COOKIE_RemoveAll()
{
  if (cookie_list) {
    cookie_list->EnumerateBackwards(deleteCookie, nsnull);
    cookie_changed = PR_TRUE;
    delete cookie_list;
    cookie_list = nsnull;
    Recycle(cookie_P3P);
  }
}

PUBLIC void
COOKIE_DeletePersistentUserData(void)
{
  nsresult res;
  
  nsCOMPtr<nsIFile> cookiesFile;
  res = NS_GetSpecialDirectory(NS_APP_USER_PROFILE_50_DIR, getter_AddRefs(cookiesFile));
  if (NS_SUCCEEDED(res)) {
    res = cookiesFile->Append(kCookiesFileName);
    if (NS_SUCCEEDED(res))
        (void) cookiesFile->Remove(PR_FALSE);
  }
}

PRIVATE void
cookie_RemoveOldestCookie(void) {
  cookie_CookieStruct * cookie_s;
  cookie_CookieStruct * oldest_cookie;

  if (cookie_list == nsnull) {
    return;
  }
   
  PRInt32 count = cookie_list->Count();
  if (count == 0) {
    return;
  }
  oldest_cookie = NS_STATIC_CAST(cookie_CookieStruct*, cookie_list->ElementAt(0));
  PRInt32 oldestLoc = 0;
  for (PRInt32 i = 1; i < count; ++i) {
    cookie_s = NS_STATIC_CAST(cookie_CookieStruct*, cookie_list->ElementAt(i));
    NS_ASSERTION(cookie_s, "cookie list is corrupt");
    if(cookie_s->lastAccessed < oldest_cookie->lastAccessed) {
      oldest_cookie = cookie_s;
      oldestLoc = i;
    }
  }
  if(oldest_cookie) {
    cookie_list->RemoveElementAt(oldestLoc);
    deleteCookie((void*)oldest_cookie, nsnull);
    cookie_changed = PR_TRUE;
  }

}

/* Remove any expired cookies from memory */
PRIVATE void
cookie_RemoveExpiredCookies() {
  cookie_CookieStruct * cookie_s;
  time_t cur_time = get_current_time();
  
  if (cookie_list == nsnull) {
    return;
  }
  
  for (PRInt32 i = cookie_list->Count(); i > 0;) {
    i--;
    cookie_s = NS_STATIC_CAST(cookie_CookieStruct*, cookie_list->ElementAt(i));
    NS_ASSERTION(cookie_s, "corrupt cookie list");
      /* Don't get rid of expire time 0 because these need to last for 
       * the entire session. They'll get cleared on exit. */
      if( cookie_s->expires && (cookie_s->expires < cur_time) ) {
        cookie_list->RemoveElementAt(i);
        deleteCookie((void*)cookie_s, nsnull);
        cookie_changed = PR_TRUE;
      }
  }
}

/* checks to see if the maximum number of cookies per host
 * is being exceeded and deletes the oldest one in that
 * case
 */
PRIVATE void
cookie_CheckForMaxCookiesFromHost(const char * cur_host) {
  cookie_CookieStruct * cookie_s;
  cookie_CookieStruct * oldest_cookie = 0;
  int cookie_count = 0;
  
  if (cookie_list == nsnull) {
    return;
  }
  
  PRInt32 count = cookie_list->Count();
  PRInt32 oldestLoc = -1;
  for (PRInt32 i = 0; i < count; ++i) {
    cookie_s = NS_STATIC_CAST(cookie_CookieStruct*, cookie_list->ElementAt(i));
    NS_ASSERTION(cookie_s, "corrupt cookie list");
    if(!PL_strcasecmp(cookie_s->host, cur_host)) {
      cookie_count++;
      if(!oldest_cookie || oldest_cookie->lastAccessed > cookie_s->lastAccessed) {
        oldest_cookie = cookie_s;
        oldestLoc = i;
      }
    }
  }
  if(cookie_count >= MAX_COOKIES_PER_SERVER && oldest_cookie) {
    NS_ASSERTION(oldestLoc > -1, "oldestLoc got out of sync with oldest_cookie");
    cookie_list->RemoveElementAt(oldestLoc);
    deleteCookie((void*)oldest_cookie, nsnull);
    cookie_changed = PR_TRUE;
  }
}


/* search for previous exact match */
PRIVATE cookie_CookieStruct *
cookie_CheckForPrevCookie(char * path, char * hostname, char * name) {
  cookie_CookieStruct * cookie_s;
  if (cookie_list == nsnull) {
    return nsnull;
  }
  
  PRInt32 count = cookie_list->Count();
  for (PRInt32 i = 0; i < count; ++i) {
    cookie_s = NS_STATIC_CAST(cookie_CookieStruct*, cookie_list->ElementAt(i));
    NS_ASSERTION(cookie_s, "corrupt cookie list");
    if(path && hostname && cookie_s->path && cookie_s->host && cookie_s->name
      && !PL_strcmp(name, cookie_s->name) && !PL_strcmp(path, cookie_s->path)
      && !PL_strcasecmp(hostname, cookie_s->host)) {
      return(cookie_s);
    }
  }
  return(nsnull);
}

/* cookie utility functions */
PRIVATE void
cookie_SetBehaviorPref(PERMISSION_BehaviorEnum x) {
  cookie_behavior = x;
}

PRIVATE void
cookie_SetWarningPref(PRBool x) {
  cookie_warning = x;
}

PRIVATE void
cookie_SetLifetimePref(COOKIE_LifetimeEnum x) {
  cookie_lifetimeOpt = x;
}

PRIVATE void
cookie_SetLifetimeLimit(PRInt32 x) {
  // save limit as seconds instead of days
  cookie_lifetimeLimit = x*24*60*60;
}

PRIVATE PERMISSION_BehaviorEnum
cookie_GetBehaviorPref() {
  return cookie_behavior;
}

PRIVATE PRBool
cookie_GetWarningPref() {
  return cookie_warning;
}

PRIVATE COOKIE_LifetimeEnum
cookie_GetLifetimePref() {
  return cookie_lifetimeOpt;
}

PRIVATE time_t
cookie_GetLifetimeTime() {
  // return time after which lifetime is excessive
  return get_current_time() + cookie_lifetimeLimit;
}

#if 0
PRIVATE PRBool
cookie_GetLifetimeAsk(time_t expireTime) {
  // return true if we should ask about this cookie
  return (cookie_GetLifetimePref() == COOKIE_Ask)
    && (cookie_GetLifetimeTime() < expireTime);
}
#endif

PRIVATE time_t
cookie_TrimLifetime(time_t expires) {
  // return potentially-trimmed lifetime
  if (cookie_GetLifetimePref() == COOKIE_Trim) {
    // a limit of zero means expire cookies at end of session
    if (cookie_lifetimeLimit == 0) {
      return 0;
    }
    time_t limit = cookie_GetLifetimeTime();
    if ((unsigned)expires > (unsigned)limit) {
      return limit;
    }
  }
  return expires;
}

MODULE_PRIVATE int PR_CALLBACK
cookie_BehaviorPrefChanged(const char * newpref, void * data) {
  PRInt32 n;
  nsresult rv;
  nsCOMPtr<nsIPref> prefs(do_GetService(NS_PREF_CONTRACTID, &rv));
  if (!prefs || NS_FAILED(prefs->GetIntPref(cookie_behaviorPref, &n))) {
    n = PERMISSION_Accept;
  }
  cookie_SetBehaviorPref((PERMISSION_BehaviorEnum)n);
  return 0;
}

MODULE_PRIVATE int PR_CALLBACK
cookie_WarningPrefChanged(const char * newpref, void * data) {
  PRBool x;
  nsresult rv;
  nsCOMPtr<nsIPref> prefs(do_GetService(NS_PREF_CONTRACTID, &rv));
  if (!prefs || NS_FAILED(prefs->GetBoolPref(cookie_warningPref, &x))) {
    x = PR_FALSE;
  }
  cookie_SetWarningPref(x);
  return 0;
}

MODULE_PRIVATE int PR_CALLBACK
cookie_LifetimeOptPrefChanged(const char * newpref, void * data) {
  PRInt32 n;
  nsresult rv;
  nsCOMPtr<nsIPref> prefs(do_GetService(NS_PREF_CONTRACTID, &rv));
  if (!prefs || NS_FAILED(prefs->GetIntPref(cookie_lifetimePref, &n))) {
    n = COOKIE_Normal;
  }
  cookie_SetLifetimePref((COOKIE_LifetimeEnum)n);
  return 0;
}

MODULE_PRIVATE int PR_CALLBACK
cookie_LifetimeLimitPrefChanged(const char * newpref, void * data) {
  PRInt32 n;
  nsresult rv;
  nsCOMPtr<nsIPref> prefs(do_GetService(NS_PREF_CONTRACTID, &rv));
  if (!NS_FAILED(rv) && !NS_FAILED(prefs->GetIntPref(cookie_lifetimeValue, &n))) {
    cookie_SetLifetimeLimit(n);
  }
  return 0;
}

MODULE_PRIVATE int PR_CALLBACK
cookie_LifetimeEnabledPrefChanged(const char * newpref, void * data) {
  PRInt32 n;
  nsresult rv;
  nsCOMPtr<nsIPref> prefs(do_GetService(NS_PREF_CONTRACTID, &rv));
  if (!prefs || NS_FAILED(prefs->GetBoolPref(cookie_lifetimeEnabledPref, &n))) {
    n = PR_FALSE;
  }
  cookie_SetLifetimePref(n ? COOKIE_Trim : COOKIE_Normal);
  return 0;
}

MODULE_PRIVATE int PR_CALLBACK
cookie_LifetimeBehaviorPrefChanged(const char * newpref, void * data) {
  PRInt32 n;
  nsresult rv;
  nsCOMPtr<nsIPref> prefs(do_GetService(NS_PREF_CONTRACTID, &rv));
  if (!prefs || NS_FAILED(prefs->GetIntPref(cookie_lifetimeBehaviorPref, &n))) {
    n = 0;
  }
  cookie_SetLifetimeLimit((n==0) ? 0 : cookie_lifetimeDays);
  cookie_lifetimeCurrentSession = (n==0);
  return 0;
}

MODULE_PRIVATE int PR_CALLBACK
cookie_LifetimeDaysPrefChanged(const char * newpref, void * data) {
  PRInt32 n;
  nsresult rv;
  nsCOMPtr<nsIPref> prefs(do_GetService(NS_PREF_CONTRACTID, &rv));
  if (!prefs || !NS_FAILED(prefs->GetIntPref(cookie_lifetimeDaysPref, &n))) {
    cookie_lifetimeDays = n;
    if (!cookie_lifetimeCurrentSession) {
      cookie_SetLifetimeLimit(n);
    }
  }
  return 0;
}

MODULE_PRIVATE int PR_CALLBACK
cookie_P3PPrefChanged(const char * newpref, void * data) {
  nsresult rv;
  nsCOMPtr<nsIPref> prefs(do_GetService(NS_PREF_CONTRACTID, &rv));
  if (!prefs || NS_FAILED(prefs->CopyCharPref(cookie_p3pPref, &cookie_P3P))) {
    cookie_P3P = PL_strdup(cookie_P3P_Default);
  }
  return 0;
}

PRIVATE int
cookie_SameDomain(char * currentHost, char * firstHost);

PUBLIC void
COOKIE_RegisterPrefCallbacks(void) {
  PRInt32 n;
  PRBool x;
  nsresult rv;
  nsCOMPtr<nsIPref> prefs(do_GetService(NS_PREF_CONTRACTID, &rv));
  if (!prefs) {
    return;
  }

  // Initialize for cookie_behaviorPref
  if (NS_FAILED(prefs->GetIntPref(cookie_behaviorPref, &n))) {
    n = PERMISSION_Accept;
  }
  cookie_SetBehaviorPref((PERMISSION_BehaviorEnum)n);
  prefs->RegisterCallback(cookie_behaviorPref, cookie_BehaviorPrefChanged, nsnull);

  // Initialize for cookie_warningPref
  if (NS_FAILED(prefs->GetBoolPref(cookie_warningPref, &x))) {
    x = PR_FALSE;
  }
  cookie_SetWarningPref(x);
  prefs->RegisterCallback(cookie_warningPref, cookie_WarningPrefChanged, nsnull);

  // Initialize for cookie_lifetime
  cookie_SetLifetimePref(COOKIE_Normal);
  cookie_lifetimeDays = 90;
  cookie_lifetimeCurrentSession = PR_FALSE;

  if (!NS_FAILED(prefs->GetIntPref(cookie_lifetimeDaysPref, &n))) {
    cookie_lifetimeDays = n;
  }
  if (!NS_FAILED(prefs->GetIntPref(cookie_lifetimeBehaviorPref, &n))) {
    cookie_lifetimeCurrentSession = (n==0);
    cookie_SetLifetimeLimit((n==0) ? 0 : cookie_lifetimeDays);
  }
  if (!NS_FAILED(prefs->GetBoolPref(cookie_lifetimeEnabledPref, &n))) {
    cookie_SetLifetimePref(n ? COOKIE_Trim : COOKIE_Normal);
  }
  prefs->RegisterCallback(cookie_lifetimeEnabledPref, cookie_LifetimeEnabledPrefChanged, nsnull);
  prefs->RegisterCallback(cookie_lifetimeBehaviorPref, cookie_LifetimeBehaviorPrefChanged, nsnull);
  prefs->RegisterCallback(cookie_lifetimeDaysPref, cookie_LifetimeDaysPrefChanged, nsnull);

  // Override cookie_lifetime initialization if the older prefs (with no UI) are used
  if (!NS_FAILED(prefs->GetIntPref(cookie_lifetimePref, &n))) {
    cookie_SetLifetimePref((COOKIE_LifetimeEnum)n);
  }
  prefs->RegisterCallback(cookie_lifetimePref, cookie_LifetimeOptPrefChanged, nsnull);

  if (!NS_FAILED(prefs->GetIntPref(cookie_lifetimeValue, &n))) {
    cookie_SetLifetimeLimit(n);
  }
  prefs->RegisterCallback(cookie_lifetimeValue, cookie_LifetimeLimitPrefChanged, nsnull);

  // Initialize for P3P prefs
  if (NS_FAILED(prefs->CopyCharPref(cookie_p3pPref, &cookie_P3P))) {
    cookie_P3P = PL_strdup(cookie_P3P_Default);
  }
  prefs->RegisterCallback(cookie_p3pPref, cookie_P3PPrefChanged, nsnull);
}

PRBool
cookie_IsInDomain(char* domain, char* host, int hostLength) {
  int domainLength = PL_strlen(domain);

  /*
   * test for domain name being an IP address (e.g., 105.217) and reject if so
   */
  PRBool hasNonDigitChar = PR_FALSE;
  for (int i = 0; i<domainLength; i++) {
    if (!nsCRT::IsAsciiDigit(domain[i]) && domain[i] != '.') {
      hasNonDigitChar = PR_TRUE;
      break;
    }
  }
  if (!hasNonDigitChar) {
    return PR_FALSE;
  }

  /*
   * special case for domainName = .hostName
   *    e.g., hostName = netscape.com
   *          domainName = .netscape.com
   */
  if ((domainLength == hostLength+1) && (domain[0] == '.') && 
      !PL_strncasecmp(&domain[1], host, hostLength)) {
    return PR_TRUE;
  }

  /*
   * normal case for hostName = x<domainName>
   *    e.g., hostName = home.netscape.com
   *          domainName = .netscape.com
   */
  if(domainLength <= hostLength &&
      !PL_strncasecmp(domain, &host[hostLength-domainLength], domainLength)) {
    return PR_TRUE;
  }

  /* tests failed, not in domain */
  return PR_FALSE;
}

/* returns PR_TRUE if authorization is required
** 
**
** IMPORTANT:  Now that this routine is multi-threaded it is up
**             to the caller to free any returned string
*/
PUBLIC char *
COOKIE_GetCookie(char * address) {
  char *name=0;
  cookie_CookieStruct * cookie_s;
  PRBool isSecure = PR_FALSE;
  time_t cur_time = get_current_time();

  int host_length;

  /* return string to build */
  char * rv=0;

  /* disable cookies if the user's prefs say so */
  if(cookie_GetBehaviorPref() == PERMISSION_DontUse) {
    return nsnull;
  }
  if (!PL_strncasecmp(address, "https", 5)) {
     isSecure = PR_TRUE;
  }

  /* search for all cookies */
  if (cookie_list == nsnull) {
    return nsnull;
  }
  char *host = CKutil_ParseURL(address, GET_HOST_PART);
  char *path = CKutil_ParseURL(address, GET_PATH_PART);
  for (PRInt32 i = 0; i <cookie_list->Count(); i++) {
    cookie_s = NS_STATIC_CAST(cookie_CookieStruct*, cookie_list->ElementAt(i));
    NS_ASSERTION(cookie_s, "corrupt cookie list");
    if(!cookie_s->host) continue;

    /* check the host or domain first */
    if(cookie_s->isDomain) {
      char *cp;

      /* calculate the host length by looking at all characters up to a
       * colon or '\0'.  That way we won't include port numbers in domains
       */
      for(cp=host; *cp != '\0' && *cp != ':'; cp++) {
        ; /* null body */ 
      }
      host_length = cp - host;
      if(!cookie_IsInDomain(cookie_s->host, host, host_length)) {
        continue;
      }
    } else if(PL_strcasecmp(host, cookie_s->host)) {
      /* hostname matchup failed. FAIL */
      continue;
    }

    /* shorter strings always come last so there can be no ambiquity */
    if(cookie_s->path && !PL_strncmp(path, cookie_s->path, PL_strlen(cookie_s->path))) {

      /* if the cookie is secure and the path isn't, dont send it */
      if (cookie_s->isSecure & !isSecure) {
        continue; /* back to top of while */
      }

      /* check for expired cookies */
      if( cookie_s->expires && (cookie_s->expires < cur_time) ) {
        /* expire and remove the cookie */
        cookie_list->RemoveElementAt(i--); /* decr i so next cookie isn't skipped */
        deleteCookie((void*)cookie_s, nsnull);
        cookie_changed = PR_TRUE;
        continue;
      }

      /* if we've already added a cookie to the return list, append a "; " so
       * subsequent cookies are delimited in the final list. */
      if (rv) CKutil_StrAllocCat(rv, "; ");

      if(cookie_s->name && *cookie_s->name != '\0') {
        cookie_s->lastAccessed = cur_time;
        CKutil_StrAllocCopy(name, cookie_s->name);
        CKutil_StrAllocCat(name, "=");

#ifdef PREVENT_DUPLICATE_NAMES
        /* make sure we don't have a previous name mapping already in the string */
        if(!rv || !PL_strstr(rv, name)) {
          CKutil_StrAllocCat(rv, name);
          CKutil_StrAllocCat(rv, cookie_s->cookie);
        }
#else
        CKutil_StrAllocCat(rv, name);
        CKutil_StrAllocCat(rv, cookie_s->cookie);
#endif /* PREVENT_DUPLICATE_NAMES */
      } else {
        CKutil_StrAllocCat(rv, cookie_s->cookie);
      }
    }
  }

  PR_FREEIF(name);
  PR_FREEIF(path);
  PR_FREEIF(host);

  /* may be nsnull */
  return(rv);
}

/* Determines whether the inlineHost is in the same domain as the currentHost.
 * For use with rfc 2109 compliance/non-compliance.
 */
PRIVATE int
cookie_SameDomain(char * currentHost, char * firstHost) {
  char * dot = 0;
  char * currentDomain = 0;
  char * firstDomain = 0;
  if(!currentHost || !firstHost) {
    return 0;
  }

  /* case insensitive compare */
  if(PL_strcasecmp(currentHost, firstHost) == 0) {
    return 1;
  }
  currentDomain = PL_strchr(currentHost, '.');
  firstDomain = PL_strchr(firstHost, '.');
  if(!currentDomain || !firstDomain) {
    return 0;
  }

  /* check for at least two dots before continuing, if there are
   * not two dots we don't have enough information to determine
   * whether or not the firstDomain is within the currentDomain
   */
  dot = PL_strchr(firstDomain, '.');
  if(dot) {
    dot = PL_strchr(dot+1, '.');
  } else {
    return 0;
  }

  /* handle .com. case */
  if(!dot || (*(dot+1) == '\0')) {
    return 0;
  }
  if(!PL_strcasecmp(firstDomain, currentDomain)) {
    return 1;
  }
  return 0;
}

PRBool
cookie_isForeign (char * curURL, char * firstURL) {
  if (!firstURL) {
    return PR_FALSE;
  }
  char * curHost = CKutil_ParseURL(curURL, GET_HOST_PART);
  char * firstHost = CKutil_ParseURL(firstURL, GET_HOST_PART);
  char * curHostColon = 0;
  char * firstHostColon = 0;

  /* strip ports */
  curHostColon = PL_strchr(curHost, ':');
  if(curHostColon) {
    *curHostColon = '\0';
  }
  firstHostColon = PL_strchr(firstHost, ':');
  if(firstHostColon) {
    *firstHostColon = '\0';
  }

  /* determine if it's foreign */
  PRBool retval = (!cookie_SameDomain(curHost, firstHost));

  /* clean up our garbage and return */
  if(curHostColon) {
    *curHostColon = ':';
  }
  if(firstHostColon) {
    *firstHostColon = ':';
  }
  PR_FREEIF(curHost);
  PR_FREEIF(firstHost);
  return retval;
}

/*
 * returns P3P_NoPolicy, P3P_NoConsent, P3P_ImplicitConsent, or P3P_ExplicitConsent
 * based on site
 */
int
P3P_SitePolicy(char * curURL) {
  // to be replaced with harishd's routine when available
  return P3P_ImplicitConsent;
}

/*
 * returns P3P_Accept, P3P_Downgrade, or P3P_Reject based on user's preferences
 */
int
cookie_P3PUserPref(PRInt32 policy, PRBool foreign) {
  NS_ASSERTION(policy == P3P_NoPolicy ||
               policy == P3P_NoConsent ||
               policy == P3P_ImplicitConsent ||
               policy == P3P_ExplicitConsent,
               "invalid value for p3p policy");
  if (cookie_P3P && PL_strlen(cookie_P3P) == 8) {
    return (foreign ? cookie_P3P[policy+1] : cookie_P3P[policy]);
  } else {
    return P3P_Accept;
  }
}

/*
 * returns P3P_Accept, P3P_Downgrade, or P3P_Reject based on user's preferences
 */
int
cookie_P3PDecision (char * curURL, char * firstURL) {
  return cookie_P3PUserPref(P3P_SitePolicy(curURL), cookie_isForeign(curURL, firstURL));
}

/* returns PR_TRUE if authorization is required
** 
**
** IMPORTANT:  Now that this routine is multi-threaded it is up
**             to the caller to free any returned string
*/
PUBLIC char *
COOKIE_GetCookieFromHttp(char * address, char * firstAddress) {

  if ((cookie_GetBehaviorPref() == PERMISSION_P3P) &&
      (cookie_P3PDecision(address, firstAddress) == P3P_Reject)) {
    return nsnull;
  }

  if ((cookie_GetBehaviorPref() == PERMISSION_DontAcceptForeign) &&
      (!firstAddress || cookie_isForeign(address, firstAddress))) {

    /*
     * WARNING!!! This is a different behavior than 4.x.  In 4.x we used this pref to
     * control the setting of cookies only.  Here we are also blocking the getting of
     * cookies if the pref is set.  It may be that we need a separate pref to block the
     * getting of cookies.  But for now we are putting both under one pref since that
     * is cleaner.  If it turns out that this breaks some important websites, we may
     * have to resort to two prefs
     */

    return nsnull;
  }
  return COOKIE_GetCookie(address);
}

MODULE_PRIVATE PRBool
cookie_IsFromHost(cookie_CookieStruct *cookie_s, char *host) {
  if (!cookie_s || !(cookie_s->host)) {
    return PR_FALSE;
  }
  if (cookie_s->isDomain) {
    char *cp;
    int host_length;

    /* calculate the host length by looking at all characters up to
     * a colon or '\0'.  That way we won't include port numbers
     * in domains
     */
    for(cp=host; *cp != '\0' && *cp != ':'; cp++) {
      ; /* null body */
    }
    host_length = cp - host;

    /* compare the tail end of host to cook_s->host */
    return cookie_IsInDomain(cookie_s->host, host, host_length);
  } else {
    return PL_strcasecmp(host, cookie_s->host) == 0;
  }
}

/* find out how many cookies this host has already set */
PRIVATE int
cookie_Count(char * host) {
  int count = 0;
  cookie_CookieStruct * cookie;
  if (!cookie_list || !host) return 0;

  for (PRInt32 i = cookie_list->Count(); i > 0;) {
    i--;
    cookie = NS_STATIC_CAST(cookie_CookieStruct*, cookie_list->ElementAt(i));
    NS_ASSERTION(cookie, "corrupt cookie list");
    if (cookie_IsFromHost(cookie, host)) count++;
  }
  return count;
}

/* Java script is calling COOKIE_SetCookieString, netlib is calling 
 * this via COOKIE_SetCookieStringFromHttp.
 */
PRIVATE void
cookie_SetCookieString(char * curURL, nsIPrompt *aPrompter, const char * setCookieHeader, time_t timeToExpire) {
  cookie_CookieStruct * prev_cookie;
  char *path_from_header=nsnull, *host_from_header=nsnull;
  char *name_from_header=nsnull, *cookie_from_header=nsnull;
  char *cur_path = CKutil_ParseURL(curURL, GET_PATH_PART);
  char *cur_host = CKutil_ParseURL(curURL, GET_HOST_PART);
  char *semi_colon, *ptr, *equal;
  char *setCookieHeaderInternal = (char *) setCookieHeader;
  PRBool isSecure=PR_FALSE, isDomain=PR_FALSE;
  PRBool bCookieAdded;
  PRBool pref_scd = PR_FALSE;

  /* Only allow cookies to be set in the listed contexts.
   * We don't want cookies being set in html mail. 
   */
/* We need to come back and work on this - Neeti 
  type = context->type;
  if(!((type==MWContextBrowser) || (type==MWContextHTMLHelp) || (type==MWContextPane))) {
    PR_Free(cur_path);
    PR_Free(cur_host);
    return;
  }
*/
  if(cookie_GetBehaviorPref() == PERMISSION_DontUse) {
    PR_Free(cur_path);
    PR_Free(cur_host);
    return;
  }

//printf("\nSetCookieString(URL '%s', header '%s') time %d == %s\n",curURL,setCookieHeader,timeToExpire,asctime(gmtime(&timeToExpire)));
  if(cookie_GetLifetimePref() == COOKIE_Discard) {
    if(cookie_GetLifetimeTime() < timeToExpire) {
      PR_Free(cur_path);
      PR_Free(cur_host);
      return;
    }
  }

//HG87358 -- @@??

  /* terminate at any carriage return or linefeed */
  for(ptr=setCookieHeaderInternal; *ptr; ptr++) {
    if(*ptr == nsCRT::LF || *ptr == nsCRT::CR) {
      *ptr = '\0';
      break;
    }
  }

  /* parse path and expires attributes from header if present */
  semi_colon = PL_strchr(setCookieHeaderInternal, ';');
  if(semi_colon) {
    /* truncate at semi-colon and advance */
    *semi_colon++ = '\0';

    /* there must be some attributes. (hopefully) */
    if ((ptr=PL_strcasestr(semi_colon, "secure"))) {
      char cPre=*(ptr-1), cPost=*(ptr+6);
      if (((cPre==' ') || (cPre==';')) && (!cPost || (cPost==' ') || (cPost==';'))) {
        isSecure = PR_TRUE;
      } 
    }

    /* look for the path attribute */
    ptr = PL_strcasestr(semi_colon, "path=");
    if(ptr) {
      nsCAutoString path(ptr+5);
      path.CompressWhitespace();
      CKutil_StrAllocCopy(path_from_header, path.get());
      /* terminate at first space or semi-colon */
      for(ptr=path_from_header; *ptr != '\0'; ptr++) {
        if(nsString::IsSpace(*ptr) || *ptr == ';' || *ptr == ',') {
          *ptr = '\0';
          break;
        }
      }
    }

    /* look for the URI or URL attribute
     *
     * This might be a security hole so I'm removing
     * it for now.
     */

    /* look for a domain */
    ptr = PL_strcasestr(semi_colon, "domain=");
    if(ptr) {
      char *domain_from_header=nsnull;
      char *dot, *colon;
      int domain_length, cur_host_length;

      /* allocate more than we need */
      nsCAutoString domain(ptr+7);
      domain.CompressWhitespace();
      CKutil_StrAllocCopy(domain_from_header, domain.get());

      /* terminate at first space or semi-colon */
      for(ptr=domain_from_header; *ptr != '\0'; ptr++) {
        if(nsString::IsSpace(*ptr) || *ptr == ';' || *ptr == ',') {
          *ptr = '\0';
          break;
        }
      }

      /* verify that this host has the authority to set for this domain.   We do
       * this by making sure that the host is in the domain.  We also require
       * that a domain have at least two periods to prevent domains of the form
       * ".com" and ".edu"
       *
       * Also make sure that there is more stuff after
       * the second dot to prevent ".com."
       */
      dot = PL_strchr(domain_from_header, '.');
      if(dot) {
        dot = PL_strchr(dot+1, '.');
      }
      if(!dot || *(dot+1) == '\0') {
        /* did not pass two dot test. FAIL */
        PR_FREEIF(path_from_header);
        PR_Free(domain_from_header);
        PR_Free(cur_path);
        PR_Free(cur_host);
        // TRACEMSG(("DOMAIN failed two dot test"));
        return;
      }

      /* strip port numbers from the current host for the domain test */
      colon = PL_strchr(cur_host, ':');
      if(colon) {
        *colon = '\0';
      }
      domain_length   = PL_strlen(domain_from_header);
      cur_host_length = PL_strlen(cur_host);

      /* check to see if the host is in the domain */
      if (!cookie_IsInDomain(domain_from_header, cur_host, cur_host_length)) {
          // TRACEMSG(("DOMAIN failed host within domain test."
//        " Domain: %s, Host: %s", domain_from_header, cur_host));
        PR_FREEIF(path_from_header);
        PR_Free(domain_from_header);
        PR_Free(cur_path);
        PR_Free(cur_host);
        return;
      }

      /*
       * check that portion of host not in domain does not contain a dot
       *    This satisfies the fourth requirement in section 4.3.2 of the cookie
       *    spec rfc 2109 (see www.cis.ohio-state.edu/htbin/rfc/rfc2109.html).
       *    It prevents host of the form x.y.co.nz from setting cookies in the
       *    entire .co.nz domain.  Note that this doesn't really solve the problem,
       *    it justs makes it more unlikely.  Sites such as y.co.nz can still set
       *    cookies for the entire .co.nz domain.
       */

      /*
       *  Although this is the right thing to do(tm), it breaks too many sites.  
       *  So only do it if the restrictCookieDomains pref is PR_TRUE.
       *
       */
      nsresult rv;
      nsCOMPtr<nsIPref> prefs(do_GetService(NS_PREF_CONTRACTID, &rv));
      if (!prefs || NS_FAILED(prefs->GetBoolPref(cookie_strictDomainsPref, &pref_scd))) {
        pref_scd = PR_FALSE;
      }
      if ( pref_scd == PR_TRUE ) {
        cur_host[cur_host_length-domain_length] = '\0';
        dot = PL_strchr(cur_host, '.');
        cur_host[cur_host_length-domain_length] = '.';
        if (dot) {
        // TRACEMSG(("host minus domain failed no-dot test."
        //  " Domain: %s, Host: %s", domain_from_header, cur_host));
          PR_FREEIF(path_from_header);
          PR_Free(domain_from_header);
          PR_Free(cur_path);
          PR_Free(cur_host);
          return;
        }
      }

      /* all tests passed, copy in domain to hostname field */
      CKutil_StrAllocCopy(host_from_header, domain_from_header);
      isDomain = PR_TRUE;
      // TRACEMSG(("Accepted domain: %s", host_from_header));
      PR_Free(domain_from_header);
    }
  }
  if(!path_from_header) {
    /* strip down everything after the last slash to get the path. */
    char * slash = PL_strrchr(cur_path, '/');
    if(slash) {
      *slash = '\0';
    }
    path_from_header = cur_path;
  } else {
    PR_Free(cur_path);
  }
  if(!host_from_header) {
    host_from_header = cur_host;
  } else {
    PR_Free(cur_host);
  }

  /* keep cookies under the max bytes limit */
  if(PL_strlen(setCookieHeaderInternal) > MAX_BYTES_PER_COOKIE) {
    setCookieHeaderInternal[MAX_BYTES_PER_COOKIE-1] = '\0';
  }

  /* separate the name from the cookie */
  equal = PL_strchr(setCookieHeaderInternal, '=');
  if (equal)
      *equal = '\0';

  nsCAutoString cookieHeader(setCookieHeaderInternal);
  cookieHeader.CompressWhitespace();
  if(equal) {
    CKutil_StrAllocCopy(name_from_header, cookieHeader.get());
    nsCAutoString value(equal+1);
    value.CompressWhitespace();
    CKutil_StrAllocCopy(cookie_from_header, value.get());
  } else {
    CKutil_StrAllocCopy(cookie_from_header, cookieHeader.get());
    CKutil_StrAllocCopy(name_from_header, "");
  }

  /* generate the message for the nag box */
  PRUnichar * new_string=0;
  int count = cookie_Count(host_from_header);
  prev_cookie = cookie_CheckForPrevCookie
    (path_from_header, host_from_header, name_from_header);
  PRUnichar * message;
  if (prev_cookie) {
    message = CKutil_Localize(NS_LITERAL_STRING("PermissionToModifyCookie").get());
    new_string = nsTextFormatter::smprintf(message, host_from_header ? host_from_header : "");
  } else if (count>1) {
    message = CKutil_Localize(NS_LITERAL_STRING("PermissionToSetAnotherCookie").get());
    new_string = nsTextFormatter::smprintf(message, host_from_header ? host_from_header : "", count);
  } else if (count==1){
    message = CKutil_Localize(NS_LITERAL_STRING("PermissionToSetSecondCookie").get());
    new_string = nsTextFormatter::smprintf(message, host_from_header ? host_from_header : "");
  } else {
    message = CKutil_Localize(NS_LITERAL_STRING("PermissionToSetACookie").get());
    new_string = nsTextFormatter::smprintf(message, host_from_header ? host_from_header : "");
  }
  Recycle(message);

  //TRACEMSG(("mkaccess.c: Setting cookie: %s for host: %s for path: %s",
  //          cookie_from_header, host_from_header, path_from_header));

  /* use common code to determine if we can set the cookie */
  PRBool permission = PR_TRUE;
  if (NS_SUCCEEDED(PERMISSION_Read())) {
    permission = Permission_Check(aPrompter, host_from_header, COOKIEPERMISSION,
// I believe this is the right place to eventually add the logic to ask
// about cookies that have excessive lifetimes, but it shouldn't be done
// until generalized per-site preferences are available.
                                       //cookie_GetLifetimeAsk(timeToExpire) ||
                                       cookie_GetWarningPref(),
                                       new_string);
  }
  PR_FREEIF(new_string);
  if (!permission) {
    PR_FREEIF(path_from_header);
    PR_FREEIF(host_from_header);
    PR_FREEIF(name_from_header);
    PR_FREEIF(cookie_from_header);
    return;
  }

  /* limit the number of cookies from a specific host or domain */
  cookie_CheckForMaxCookiesFromHost(host_from_header);

  if (cookie_list) {
    if(cookie_list->Count() > MAX_NUMBER_OF_COOKIES-1) {
      cookie_RemoveOldestCookie();
    }
  }
  prev_cookie = cookie_CheckForPrevCookie (path_from_header, host_from_header, name_from_header);
  if(prev_cookie) {
    prev_cookie->expires = cookie_TrimLifetime(timeToExpire);
    PR_FREEIF(prev_cookie->cookie);
    PR_FREEIF(prev_cookie->path);
    PR_FREEIF(prev_cookie->host);
    PR_FREEIF(prev_cookie->name);
    prev_cookie->cookie = cookie_from_header;
    prev_cookie->path = path_from_header;
    prev_cookie->host = host_from_header;
    prev_cookie->name = name_from_header;
    prev_cookie->isSecure = isSecure;
    prev_cookie->isDomain = isDomain;
    prev_cookie->lastAccessed = get_current_time();
  } else {
    cookie_CookieStruct * tmp_cookie_ptr;
    size_t new_len;

    /* construct a new cookie_struct */
    prev_cookie = PR_NEW(cookie_CookieStruct);
    if(!prev_cookie) {
      PR_FREEIF(path_from_header);
      PR_FREEIF(host_from_header);
      PR_FREEIF(name_from_header);
      PR_FREEIF(cookie_from_header);
      return;
    }
    
    /* copy */
    prev_cookie->cookie = cookie_from_header;
    prev_cookie->name = name_from_header;
    prev_cookie->path = path_from_header;
    prev_cookie->host = host_from_header;
    prev_cookie->expires = cookie_TrimLifetime(timeToExpire);
    prev_cookie->isSecure = isSecure;
    prev_cookie->isDomain = isDomain;
    prev_cookie->lastAccessed = get_current_time();
    if(!cookie_list) {
      cookie_list = new nsVoidArray();
      if(!cookie_list) {
        PR_FREEIF(path_from_header);
        PR_FREEIF(name_from_header);
        PR_FREEIF(host_from_header);
        PR_FREEIF(cookie_from_header);
        PR_Free(prev_cookie);
        return;
      }
    }

    /* add it to the list so that it is before any strings of smaller length */
    bCookieAdded = PR_FALSE;
    new_len = PL_strlen(prev_cookie->path);
    for (PRInt32 i = cookie_list->Count(); i > 0;) {
      i--;
      tmp_cookie_ptr = NS_STATIC_CAST(cookie_CookieStruct*, cookie_list->ElementAt(i));
      NS_ASSERTION(tmp_cookie_ptr, "corrupt cookie list");
      if(new_len <= PL_strlen(tmp_cookie_ptr->path)) {
        cookie_list->InsertElementAt(prev_cookie, i+1);
        bCookieAdded = PR_TRUE;
        break;
      }
    }
    if ( !bCookieAdded ) {
      /* no shorter strings found in list */
      cookie_list->AppendElement(prev_cookie);
    }
  }

  /* At this point we know a cookie has changed. Make a note to write the cookies to file. */
  cookie_changed = PR_TRUE;
  return;
}

PUBLIC void
COOKIE_SetCookieString(char * curURL, nsIPrompt *aPrompter, const char * setCookieHeader) {
  COOKIE_SetCookieStringFromHttp(curURL, nsnull, aPrompter, setCookieHeader, 0);
}

/* This function wrapper wraps COOKIE_SetCookieString for the purposes of 
 * determining whether or not a cookie is inline (we need the URL struct, 
 * and outputFormat to do so).  this is called from NET_ParseMimeHeaders 
 * in mkhttp.c
 * This routine does not need to worry about the cookie lock since all of
 *   the work is handled by sub-routines
*/

PUBLIC void
COOKIE_SetCookieStringFromHttp(char * curURL, char * firstURL, nsIPrompt *aPrompter, const char * setCookieHeader, char * server_date) {

  /* allow for multiple cookies separated by newlines */
   char *newline = PL_strchr(setCookieHeader, '\n');
   if(newline) {
     *newline = '\0';
     COOKIE_SetCookieStringFromHttp(curURL, firstURL, aPrompter, setCookieHeader, server_date);
     *newline = '\n';
     COOKIE_SetCookieStringFromHttp(curURL, firstURL, aPrompter, newline+1, server_date);
     return;
   }

  /* If the outputFormat is not PRESENT (the url is not going to the screen), and not
   *  SAVE AS (shift-click) then 
   *  the cookie being set is defined as inline so we need to do what the user wants us
   *  to based on his preference to deal with foreign cookies. If it's not inline, just set
   *  the cookie.
   */
  char *ptr=nsnull;
  time_t gmtCookieExpires=0, expires=0, sDate;

  PRBool downgrade = PR_FALSE;

  /* check to see if P3P pref is satisfied */
  if (cookie_GetBehaviorPref() == PERMISSION_P3P) {
    PRInt32 decision = cookie_P3PDecision(curURL, firstURL);
    if (decision == P3P_Reject) {
      return;
    }
    if (decision == P3P_Downgrade) {
      downgrade = PR_TRUE;
    }
  }
 
  /* check for foreign cookie if pref says to reject such */
  if ((cookie_GetBehaviorPref() == PERMISSION_DontAcceptForeign) &&
      cookie_isForeign(curURL, firstURL)) {
    /* it's a foreign cookie so don't set the cookie */
    return;
  }

  /* Determine when the cookie should expire. This is done by taking the difference between 
   * the server time and the time the server wants the cookie to expire, and adding that 
   * difference to the client time. This localizes the client time regardless of whether or
   * not the TZ environment variable was set on the client.
   */

  /* Get the time the cookie is supposed to expire according to the attribute*/
  ptr = PL_strcasestr(setCookieHeader, "expires=");
  if(ptr && !downgrade) {
    char *date =  ptr+8;
    char origLast = '\0';
    for(ptr=date; *ptr != '\0'; ptr++) {
      if(*ptr == ';') {
        origLast = ';';
        *ptr = '\0';
        break;
      }
    }
    expires = cookie_ParseDate(date);
    *ptr=origLast;
  }
  if (server_date) {
    sDate = cookie_ParseDate(server_date);
  } else {
    sDate = get_current_time();
  }
  if( sDate && expires ) {
    if( expires < sDate ) {
      gmtCookieExpires=1;
    } else {
      gmtCookieExpires = expires - sDate + get_current_time();
      // if overflow 
      if( gmtCookieExpires < get_current_time()) {
        gmtCookieExpires = (((unsigned) (~0) << 1) >> 1); // max int 
      }
    }
  }
  cookie_SetCookieString(curURL, aPrompter, setCookieHeader, gmtCookieExpires);
}

/* saves out the HTTP cookies to disk */
PUBLIC nsresult
COOKIE_Write() {
  if (!cookie_changed || !cookie_list) {
    return NS_OK;
  }
  cookie_CookieStruct * cookie_s;
  time_t cur_date = get_current_time();
  char date_string[36];
  nsFileSpec dirSpec;
  nsresult rv = CKutil_ProfileDirectory(dirSpec);
  if (NS_FAILED(rv)) {
    return rv;
  }
  nsOutputFileStream strm(dirSpec + kCookiesFileName);
  if (!strm.is_open()) {
    /* file doesn't exist -- that's not an error */
    return NS_OK;
  }

#define COOKIEFILE_LINE1 "# HTTP Cookie File\n"
#define COOKIEFILE_LINE2 "# http://www.netscape.com/newsref/std/cookie_spec.html\n"
#define COOKIEFILE_LINE3 "# This is a generated file!  Do not edit.\n"
#define COOKIEFILE_LINE4 "# To delete cookies, use the Cookie Manager.\n\n"

  strm.write(COOKIEFILE_LINE1, PL_strlen(COOKIEFILE_LINE1));
  strm.write(COOKIEFILE_LINE2, PL_strlen(COOKIEFILE_LINE2));
  strm.write(COOKIEFILE_LINE3, PL_strlen(COOKIEFILE_LINE3));
  strm.write(COOKIEFILE_LINE4, PL_strlen(COOKIEFILE_LINE4));

  /* format shall be:
   *
   * host \t isDomain \t path \t secure \t expires \t name \t cookie
   *
   * isDomain is PR_TRUE or PR_FALSE
   * secure is PR_TRUE or PR_FALSE
   * expires is a time_t integer
   * cookie can have tabs
   */
  PRInt32 count = cookie_list->Count();
  for (PRInt32 i = 0; i < count; ++i) {
    cookie_s = NS_STATIC_CAST(cookie_CookieStruct*, cookie_list->ElementAt(i));
    NS_ASSERTION(cookie_s, "corrupt cookie list");
      if (cookie_s->expires < cur_date) {
        /* don't write entry if cookie has expired or has no expiration date */
        continue;
      }

      strm.write(cookie_s->host, nsCRT::strlen(cookie_s->host));

      if (cookie_s->isDomain) {
        strm.write("\tTRUE\t", 6);
      } else {
        strm.write("\tFALSE\t", 7);
      }

      strm.write(cookie_s->path, nsCRT::strlen(cookie_s->path));

      if (cookie_s->isSecure) {
        strm.write("\tTRUE\t", 6);
      } else {
        strm.write("\tFALSE\t", 7);
      }

      PR_snprintf(date_string, sizeof(date_string), "%lu", cookie_s->expires);

      strm.write(date_string, nsCRT::strlen(date_string));
      strm.write("\t", 1);
      strm.write(cookie_s->name, nsCRT::strlen(cookie_s->name));
      strm.write("\t", 1);
      strm.write(cookie_s->cookie, nsCRT::strlen(cookie_s->cookie));
      strm.write("\n", 1);
  }

  cookie_changed = PR_FALSE;
  strm.flush();
  strm.close();
  return NS_OK;
}

/* reads HTTP cookies from disk */
PUBLIC nsresult
COOKIE_Read() {
  if (cookie_list) {
    return NS_OK;
  }
  cookie_CookieStruct *new_cookie, *tmp_cookie_ptr;
  size_t new_len;
  nsAutoString buffer;
  PRBool added_to_list;
  nsFileSpec dirSpec;
  nsresult rv = CKutil_ProfileDirectory(dirSpec);
  if (NS_FAILED(rv)) {
    return rv;
  }
  nsInputFileStream strm(dirSpec + kCookiesFileName);
  if (!strm.is_open()) {
    /* file doesn't exist -- that's not an error */
    return NS_OK;
  }

  /* format is:
   *
   * host \t isDomain \t path \t secure \t expires \t name \t cookie
   *
   * if this format isn't respected we move onto the next line in the file.
   * isDomain is PR_TRUE or PR_FALSE -- defaulting to PR_FALSE
   * secure is PR_TRUE or PR_FALSE   -- should default to PR_TRUE
   * expires is a time_t integer
   * cookie can have tabs
   */
  while (CKutil_GetLine(strm,buffer) != -1){
    added_to_list = PR_FALSE;

    if ( !buffer.IsEmpty() ) {
      PRUnichar firstChar = buffer.CharAt(0);
      if (firstChar == '#' || firstChar == nsCRT::CR ||
          firstChar == nsCRT::LF || firstChar == 0) {
        continue;
      }
    }
    int hostIndex, isDomainIndex, pathIndex, secureIndex, expiresIndex, nameIndex, cookieIndex;
    hostIndex = 0;
    if ((isDomainIndex=buffer.FindChar('\t', PR_FALSE,hostIndex)+1) == 0 ||
        (pathIndex=buffer.FindChar('\t', PR_FALSE,isDomainIndex)+1) == 0 ||
        (secureIndex=buffer.FindChar('\t', PR_FALSE,pathIndex)+1) == 0 ||
        (expiresIndex=buffer.FindChar('\t', PR_FALSE,secureIndex)+1) == 0 ||
        (nameIndex=buffer.FindChar('\t', PR_FALSE,expiresIndex)+1) == 0 ||
        (cookieIndex=buffer.FindChar('\t', PR_FALSE,nameIndex)+1) == 0 ) {
      continue;
    }
    nsAutoString host, isDomain, path, isSecure, expires, name, cookie;
    buffer.Mid(host, hostIndex, isDomainIndex-hostIndex-1);
    buffer.Mid(isDomain, isDomainIndex, pathIndex-isDomainIndex-1);
    buffer.Mid(path, pathIndex, secureIndex-pathIndex-1);
    buffer.Mid(isSecure, secureIndex, expiresIndex-secureIndex-1);
    buffer.Mid(expires, expiresIndex, nameIndex-expiresIndex-1);
    buffer.Mid(name, nameIndex, cookieIndex-nameIndex-1);
    buffer.Mid(cookie, cookieIndex, buffer.Length()-cookieIndex);

    /* create a new cookie_struct and fill it in */
    new_cookie = PR_NEW(cookie_CookieStruct);
    if (!new_cookie) {
      strm.close();
      return NS_ERROR_OUT_OF_MEMORY;
    }
    memset(new_cookie, 0, sizeof(cookie_CookieStruct));
    new_cookie->name = ToNewCString(name);
    new_cookie->cookie = ToNewCString(cookie);
    new_cookie->host = ToNewCString(host);
    new_cookie->path = ToNewCString(path);
    if (isDomain.EqualsWithConversion("TRUE")) {
      new_cookie->isDomain = PR_TRUE;
    } else {
      new_cookie->isDomain = PR_FALSE;
    }
    if (isSecure.EqualsWithConversion("TRUE")) {
      new_cookie->isSecure = PR_TRUE;
    } else {
      new_cookie->isSecure = PR_FALSE;
    }
    char * expiresCString = ToNewCString(expires);
    new_cookie->expires = strtoul(expiresCString, nsnull, 10);
    nsCRT::free(expiresCString);

    /* start new cookie list if one does not already exist */
    if (!cookie_list) {
      cookie_list = new nsVoidArray();
      if (!cookie_list) {
        strm.close();
        return NS_ERROR_OUT_OF_MEMORY;
      }
    }

    /* add new cookie to the list so that it is before any strings of smaller length */
    new_len = PL_strlen(new_cookie->path);
    for (PRInt32 i = cookie_list->Count(); i > 0;) {
      i--;
      tmp_cookie_ptr = NS_STATIC_CAST(cookie_CookieStruct*, cookie_list->ElementAt(i));
      NS_ASSERTION(tmp_cookie_ptr, "corrupt cookie list");
      if (new_len <= PL_strlen(tmp_cookie_ptr->path)) {
        cookie_list->InsertElementAt(new_cookie, i);
        added_to_list = PR_TRUE;
        break;
      }
    }

    /* no shorter strings found in list so add new cookie at end */
    if (!added_to_list) {
      cookie_list->AppendElement(new_cookie);
    }
  }

  strm.close();
  cookie_changed = PR_FALSE;
  return NS_OK;
}

PRIVATE PRBool
CookieCompare (cookie_CookieStruct * cookie1, cookie_CookieStruct * cookie2) {
  char * host1 = cookie1->host;
  char * host2 = cookie2->host;

  /* get rid of leading period on host name, if any */
  if (*host1 == '.') {
    host1++;
  }
  if (*host2 == '.') {
    host2++;
  }

  /* make decision based on host name if they are unequal */
  if (PL_strcmp (host1, host2) < 0) {
    return -1;
  }
  if (PL_strcmp (host1, host2) > 0) {
    return 1;
  }

  /* if host names are equal, make decision based on cookie name if they are unequal */
  if (PL_strcmp (cookie1->name, cookie2->name) < 0) {
    return -1;
  }
  if (PL_strcmp (cookie1->name, cookie2->name) > 0) {
    return 1;
  }
  
  /* if host and cookie names are equal, make decision based on path */
  /*    It may seem like this should never happen but it does.
   *    Go to groups.aol.com and you will get two cookies with
   *    identical host and cookie names but different paths
   */
  return (PL_strcmp (cookie1->path, cookie2->path));
}

/*
 * return a string that has each " of the argument sting
 * replaced with \" so it can be used inside a quoted string
 */
PRIVATE char*
cookie_FixQuoted(char* s) {
  char * result;
  int count = PL_strlen(s);
  char *quote = s;
  unsigned int i, j;
  while ((quote = PL_strchr(quote, '"'))) {
    count++;
    quote++;
  }
  result = (char*)PR_Malloc(count + 1);
  for (i=0, j=0; i<PL_strlen(s); i++) {
    if (s[i] == '"') {
      result[i+(j++)] = '\\';
    }
    result[i+j] = s[i];
  }
  result[i+j] = '\0';
  return result;
}

PUBLIC PRInt32
COOKIE_Count() {
  if (!cookie_list) {
    return 0;
  }
  /* Get rid of any expired cookies now so user doesn't
   * think/see that we're keeping cookies in memory.
   */
  cookie_RemoveExpiredCookies();

  return cookie_list->Count();
}

PUBLIC nsresult
COOKIE_Enumerate
    (PRInt32 count,
     char **name,
     char **value,
     PRBool *isDomain,
     char ** host,
     char ** path,
     PRBool * isSecure,
     PRUint64 * expires) {
  if (count > COOKIE_Count()) {
    return NS_ERROR_FAILURE;
  }
  cookie_CookieStruct *cookie;
  cookie = NS_STATIC_CAST(cookie_CookieStruct*, cookie_list->ElementAt(count));
  NS_ASSERTION(cookie, "corrupt cookie list");

  *name = cookie_FixQuoted(cookie->name);
  *value = cookie_FixQuoted(cookie->cookie);
  *isDomain = cookie->isDomain;
  *host = cookie_FixQuoted(cookie->host);
  *path = cookie_FixQuoted(cookie->path);
  *isSecure = cookie->isSecure;
  time_t expiresTime=0;
  if (cookie->expires) {
    /*
     * Cookie expiration times on mac will not be decoded correctly because
     * they were based on get_current_time() instead of time(nsnull) -- see comments in
     * get_current_time.  So we need to adjust for that now in order for the
     * display of the expiration time to be correct
     */
    expiresTime = cookie->expires + (time(nsnull) - get_current_time());
  }
  // *expires = expiresTime; -- no good no mac, using next line instead
  LL_UI2L(*expires, expiresTime);
  return NS_OK;
}

PUBLIC void
COOKIE_Remove
    (const char* host, const char* name, const char* path, const PRBool permanent) {
  cookie_CookieStruct * cookie;
  PRInt32 count = 0;

  /* step through all cookies searching for indicated one */
  if (cookie_list) {
    count = cookie_list->Count();
    while (count>0) {
      count--;
      cookie = NS_STATIC_CAST(cookie_CookieStruct*, cookie_list->ElementAt(count));
      NS_ASSERTION(cookie, "corrupt cookie list");
      if ((PL_strcmp(cookie->host, host) == 0) &&
          (PL_strcmp(cookie->name, name) == 0) &&
          (PL_strcmp(cookie->path, path) == 0)) {
        if (permanent && cookie->host) {
          char * hostname = nsnull;
          char * hostnameAfterDot = cookie->host;
          while (*hostnameAfterDot == '.') {
            hostnameAfterDot++;
          }
          CKutil_StrAllocCopy(hostname, hostnameAfterDot);
          if (hostname) {
            if (NS_SUCCEEDED(PERMISSION_Read())) {
              Permission_AddHost(hostname, PR_FALSE, COOKIEPERMISSION, PR_TRUE);
            }
          }
        }
        cookie_list->RemoveElementAt(count);
        deleteCookie((void*)cookie, nsnull);
        cookie_changed = PR_TRUE;
        COOKIE_Write();
        break;
      }
    }
  }
}

MODULE_PRIVATE time_t 
cookie_ParseDate(char *date_string) {

  PRTime prdate;
  time_t date = 0;
  // TRACEMSG(("Parsing date string: %s\n",date_string));

  if(PR_ParseTimeString(date_string, PR_TRUE, &prdate) == PR_SUCCESS) {
    PRInt64 r, u;
    LL_I2L(u, PR_USEC_PER_SEC);
    LL_DIV(r, prdate, u);
    LL_L2I(date, r);
    if (date < 0) {
      date = 0;
    }
    // TRACEMSG(("Parsed date as GMT: %s\n", asctime(gmtime(&date))));    // TRACEMSG(("Parsed date as local: %s\n", ctime(&date)));
  } else {
    // TRACEMSG(("Could not parse date"));
  }
  return (date);
}
