/* -*- Mode: C; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * The contents of this file are subject to the Netscape Public License
 * Version 1.0 (the "NPL"); you may not use this file except in
 * compliance with the NPL.  You may obtain a copy of the NPL at
 * http://www.mozilla.org/NPL/
 *
 * Software distributed under the NPL is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the NPL
 * for the specific language governing rights and limitations under the
 * NPL.
 *
 * The Initial Developer of this code under the NPL is Netscape
 * Communications Corporation.  Portions created by Netscape are
 * Copyright (C) 1998 Netscape Communications Corporation.  All Rights
 * Reserved.
 */

#define alphabetize 1

#include "nsString.h"
#include "nsIServiceManager.h"
#include "nsFileStream.h"
#include "nsIFileLocator.h"
#include "nsIFileSpec.h"
#include "nsFileLocations.h"
#include "nsINetSupportDialogService.h"
#include "nsIIOService.h"
#include "nsIURL.h"
#include "nsIStringBundle.h"
#include "nsVoidArray.h"

extern "C" {
#include "prefapi.h"
#include "prmon.h"
#ifdef XP_MAC
#include "prpriv.h"             /* for NewNamedMonitor */
#else
#include "private/prpriv.h"
#endif
#include "sechash.h"
#include "rosetta.h"
}

static NS_DEFINE_IID(kIFileLocatorIID, NS_IFILELOCATOR_IID);
static NS_DEFINE_CID(kFileLocatorCID, NS_FILELOCATOR_CID);
static NS_DEFINE_CID(kNetSupportDialogCID, NS_NETSUPPORTDIALOG_CID);
static NS_DEFINE_IID(kIIOServiceIID, NS_IIOSERVICE_IID);
static NS_DEFINE_CID(kIOServiceCID, NS_IOSERVICE_CID);
static NS_DEFINE_IID(kIStringBundleServiceIID, NS_ISTRINGBUNDLESERVICE_IID);
static NS_DEFINE_IID(kStringBundleServiceCID, NS_STRINGBUNDLESERVICE_CID);

#define MAX_NUMBER_OF_COOKIES 300
#define MAX_COOKIES_PER_SERVER 20
#define MAX_BYTES_PER_COOKIE 4096  /* must be at least 1 */
#ifndef MAX_HOST_NAME_LEN
#define MAX_HOST_NAME_LEN 64
#endif

#define cookie_behaviorPref "network.cookie.cookieBehavior"
#define cookie_warningPref "network.cookie.warnAboutCookies"
#define cookie_strictDomainsPref "network.cookie.strictDomains"
#define cookie_localization "resource:/res/cookie.properties"
#define COOKIE_IS_SPACE(x) ((((unsigned int) (x)) > 0x7f) ? 0 : isspace(x))

//#define GET_ALL_PARTS              127
#define GET_PASSWORD_PART           64
#define GET_USERNAME_PART           32
#define GET_PROTOCOL_PART          16
#define GET_HOST_PART               8
#define GET_PATH_PART               4
#define GET_HASH_PART               2
#define GET_SEARCH_PART             1

MODULE_PRIVATE time_t 
cookie_ParseDate(char *date_string);

MODULE_PRIVATE char * 
cookie_ParseURL (const char *url, int parts_requested);

MODULE_PRIVATE char *
XP_StripLine (char *string);

typedef struct _cookie_CookieStruct {
  char * path;
  char * host;
  char * name;
  char * cookie;
  time_t expires;
  time_t lastAccessed;
  PRBool xxx;
  PRBool isDomain;   /* is it a domain instead of an absolute host? */
} cookie_CookieStruct;

typedef struct _cookie_PermissionStruct {
  char * host;
  PRBool permission;
} cookie_PermissionStruct;

typedef struct _cookie_DeferStruct {
  char * curURL;
  char * setCookieHeader;
  time_t timeToExpire;
} cookie_DeferStruct;

typedef enum {
  COOKIE_Accept,
  COOKIE_DontAcceptForeign,
  COOKIE_DontUse
} cookie_BehaviorEnum;

#include "prthread.h"
#include "prmon.h"

PRBool cookie_SetCookieStringInUse = PR_FALSE;
PRIVATE PRBool cookie_cookiesChanged = PR_FALSE;
PRIVATE PRBool cookie_permissionsChanged = PR_FALSE;
PRIVATE PRBool cookie_rememberChecked = PR_FALSE;
PRIVATE cookie_BehaviorEnum cookie_behavior = COOKIE_Accept;
PRIVATE PRBool cookie_warning = PR_FALSE;

PRIVATE nsVoidArray * cookie_cookieList=0;
PRIVATE nsVoidArray * cookie_permissionList=0;
PRIVATE nsVoidArray * cookie_deferList=0;

static PRMonitor * cookie_cookieLockMonitor = NULL;
static PRThread * cookie_cookieLockOwner = NULL;
static int cookie_cookieLockCount = 0;

static PRMonitor * cookie_deferLockMonitor = NULL;
static PRThread  * cookie_deferLockOwner = NULL;
static int cookie_deferLockCount = 0;

//#define REAL_DIALOG 1

/* StrAllocCopy and StrAllocCat should really be defined elsewhere */
#include "plstr.h"
#include "prmem.h"
#undef StrAllocCopy
#define StrAllocCopy(dest, src) Local_SACopy (&(dest), src)
char *
Local_SACopy(char **destination, const char *source)
{
  if(*destination) {
    PL_strfree(*destination);
    *destination = 0;
  }
  *destination = PL_strdup(source);
  return *destination;
}

#undef StrAllocCat
#define StrAllocCat(dest, src) Local_SACat (&(dest), src)
char *
Local_SACat(char **destination, const char *source)
{
  if (source && *source) {
    if (*destination) {
      int length = PL_strlen (*destination);
      *destination = (char *) PR_Realloc(*destination, length + PL_strlen(source) + 1);
      if (*destination == NULL) {
        return(NULL);
      }
      PL_strcpy (*destination + length, source);
    } else {
      *destination = PL_strdup(source);
    }
  }
  return *destination;
}

PRBool
cookie_CheckConfirmYN(char * szMessage, char * szCheckMessage, PRBool* checkValue) {
#ifdef REAL_DIALOG
  PRBool retval = PR_TRUE; /* default value */
  nsresult res;  
  NS_WITH_SERVICE(nsIPrompt, dialog, kNetSupportDialogCID, &res);
  if (NS_FAILED(res)) {
    *checkValue = 0;
    return retval;
  }
  const nsString message = szMessage;
  const nsString checkMessage = szCheckMessage;
  retval = PR_FALSE; /* in case user exits dialog by clicking X */
  res = dialog->ConfirmCheckYN(message.GetUnicode(), checkMessage.GetUnicode(), checkValue, &retval);
  if (NS_FAILED(res)) {
    *checkValue = 0;
  }
  if (*checkValue!=0 && *checkValue!=1) {
    *checkValue = 0; /* this should never happen but it is happening!!! */
  }
  return retval;

#else

  fprintf(stdout, "%c%s  (y/n)?  ", '\007', szMessage); /* \007 is BELL */
  char c;
  PRBool result;
  for (;;) {
    c = getchar();
    if (tolower(c) == 'y') {
      result = PR_TRUE;
      break;
    }
    if (tolower(c) == 'n') {
      result = PR_FALSE;
      break;
    }
  }
  fprintf(stdout, "%c%s  y/n?  ", '\007', szCheckMessage); /* \007 is BELL */
  for (;;) {
    c = getchar();
    if (tolower(c) == 'y') {
      *checkValue = PR_TRUE;
      break;
    }
    if (tolower(c) == 'n') {
      *checkValue = PR_FALSE;
      break;
    }
  }
  return result;
#endif
}

PRIVATE char*
cookie_Localize(char* genericString) {
  nsresult ret;
  nsAutoString v("");

  /* create a URL for the string resource file */
  nsIIOService* pNetService = nsnull;
  ret = nsServiceManager::GetService(kIOServiceCID, kIIOServiceIID,
    (nsISupports**) &pNetService);

  if (NS_FAILED(ret)) {
    printf("cannot get net service\n");
    return v.ToNewCString();
  }
  nsIURI *url = nsnull;

  nsIURI *uri = nsnull;
  ret = pNetService->NewURI(cookie_localization, nsnull, &uri);
  if (NS_FAILED(ret)) {
    printf("cannot create URI\n");
    nsServiceManager::ReleaseService(kIOServiceCID, pNetService);
    return v.ToNewCString();
  }

  ret = uri->QueryInterface(nsIURI::GetIID(), (void**)&url);
  nsServiceManager::ReleaseService(kIOServiceCID, pNetService);

  if (NS_FAILED(ret)) {
    printf("cannot create URL\n");
    return v.ToNewCString();
  }

  /* create a bundle for the localization */
  nsIStringBundleService* pStringService = nsnull;
  ret = nsServiceManager::GetService(kStringBundleServiceCID,
    kIStringBundleServiceIID, (nsISupports**) &pStringService);
  if (NS_FAILED(ret)) {
    printf("cannot get string service\n");
    return v.ToNewCString();
  }
  nsILocale* locale = nsnull;
  nsIStringBundle* bundle = nsnull;
  char* spec = nsnull;
  ret = url->GetSpec(&spec);
  if (NS_FAILED(ret)) {
    printf("cannot get url spec\n");
    nsServiceManager::ReleaseService(kStringBundleServiceCID, pStringService);
    nsCRT::free(spec);
    return v.ToNewCString();
  }
  ret = pStringService->CreateBundle(spec, locale, &bundle);
  nsCRT::free(spec);
  if (NS_FAILED(ret)) {
    printf("cannot create instance\n");
    nsServiceManager::ReleaseService(kStringBundleServiceCID, pStringService);
    return v.ToNewCString();
  }
  nsServiceManager::ReleaseService(kStringBundleServiceCID, pStringService);

  /* localize the given string */
  nsString   strtmp(genericString);
  const PRUnichar *ptrtmp = strtmp.GetUnicode();
  PRUnichar *ptrv = nsnull;
  ret = bundle->GetStringFromName(ptrtmp, &ptrv);
  v = ptrv;
  NS_RELEASE(bundle);
  if (NS_FAILED(ret)) {
    printf("cannot get string from name\n");
    return v.ToNewCString();
  }
  return v.ToNewCString();
}

PRIVATE nsresult cookie_ProfileDirectory(nsFileSpec& dirSpec) {
  nsIFileSpec* spec = 
    NS_LocateFileOrDirectory(nsSpecialFileSpec::App_UserProfileDirectory50);
  if (!spec) {
    return NS_ERROR_FAILURE;
  }
  return spec->GetFileSpec(&dirSpec);
}


/*
 * Write a line to a file
 * return NS_OK if no error occurs
 */
nsresult
cookie_Put(nsOutputFileStream strm, const nsString& aLine)
{
  /* allocate a buffer from the heap */
  char * cp = new char[aLine.Length() + 1];
  if (! cp) {
    return NS_ERROR_FAILURE;
  }
  aLine.ToCString(cp, aLine.Length() + 1);

  /* output each character */
  char* p = cp;
  while (*p) {
    strm.put(*(p++));
  }
  delete[] cp;
  return NS_OK;
}

/*
 * get a line from a file
 * return -1 if end of file reached
 * strip carriage returns and line feeds from end of line
 */
PRInt32
cookie_GetLine(nsInputFileStream strm, nsAutoString*& aLine) {

  /* read the line */
  aLine = new nsAutoString("");   
  char c;
  for (;;) {
    c = strm.get();
    if (c == '\n') {
      break;
    }

    /* note that eof is not set until we read past the end of the file */
    if (strm.eof()) {
      return -1;
    }
    if (c != '\r') {
      *aLine += c;
    }
  }
  return 0;
}


PRIVATE void
cookie_LockCookieList(void) {
  if(!cookie_cookieLockMonitor) {
    cookie_cookieLockMonitor = PR_NewNamedMonitor("cookie-lock");
  }
  PR_EnterMonitor(cookie_cookieLockMonitor);
  while(PR_TRUE) {

    /* no current owner or owned by this thread */
    PRThread * t = PR_CurrentThread();
    if(cookie_cookieLockOwner == NULL || cookie_cookieLockOwner == t) {
      cookie_cookieLockOwner = t;
      cookie_cookieLockCount++;
      PR_ExitMonitor(cookie_cookieLockMonitor);
      return;
    }

    /* owned by someone else -- wait till we can get it */
    PR_Wait(cookie_cookieLockMonitor, PR_INTERVAL_NO_TIMEOUT);
  }
}

PRIVATE void
cookie_UnlockCookieList(void) {
   PR_EnterMonitor(cookie_cookieLockMonitor);

#ifdef DEBUG
  /* make sure someone doesn't try to free a lock they don't own */
  PR_ASSERT(cookie_cookieLockOwner == PR_CurrentThread());
#endif

  cookie_cookieLockCount--;
  if(cookie_cookieLockCount == 0) {
    cookie_cookieLockOwner = NULL;
    PR_Notify(cookie_cookieLockMonitor);
  }
  PR_ExitMonitor(cookie_cookieLockMonitor);
}

PRIVATE void
cookie_LockDeferList(void) {
  if(!cookie_deferLockMonitor) {
    cookie_deferLockMonitor = PR_NewNamedMonitor("defer_cookie-lock");
  }
  PR_EnterMonitor(cookie_deferLockMonitor);
  while(PR_TRUE) {

    /* no current owner or owned by this thread */
    PRThread * t = PR_CurrentThread();
    if(cookie_deferLockOwner == NULL || cookie_deferLockOwner == t) {
      cookie_deferLockOwner = t;
      cookie_deferLockCount++;
      PR_ExitMonitor(cookie_deferLockMonitor);
      return;
    }

    /* owned by someone else -- wait till we can get it */
    PR_Wait(cookie_deferLockMonitor, PR_INTERVAL_NO_TIMEOUT);
  }
}

PRIVATE void
cookie_UnlockDeferList(void) {
  PR_EnterMonitor(cookie_deferLockMonitor);

#ifdef DEBUG
  /* make sure someone doesn't try to free a lock they don't own */
  PR_ASSERT(cookie_deferLockOwner == PR_CurrentThread());
#endif

  cookie_deferLockCount--;
  if(cookie_deferLockCount == 0) {
    cookie_deferLockOwner = NULL;
    PR_Notify(cookie_deferLockMonitor);
  }
  PR_ExitMonitor(cookie_deferLockMonitor);
}

static PRMonitor * cookie_permission_lock_monitor = NULL;
static PRThread  * cookie_permission_lock_owner = NULL;
static int cookie_permission_lock_count = 0;

PRIVATE void
cookie_LockPermissionList(void) {
  if(!cookie_permission_lock_monitor)
  cookie_permission_lock_monitor = PR_NewNamedMonitor("cookie_permission-lock");
  PR_EnterMonitor(cookie_permission_lock_monitor);
  while(PR_TRUE) {

    /* no current owner or owned by this thread */
    PRThread * t = PR_CurrentThread();
    if(cookie_permission_lock_owner == NULL || cookie_permission_lock_owner == t) {
      cookie_permission_lock_owner = t;
      cookie_permission_lock_count++;
      PR_ExitMonitor(cookie_permission_lock_monitor);
      return;
    }

    /* owned by someone else -- wait till we can get it */
    PR_Wait(cookie_permission_lock_monitor, PR_INTERVAL_NO_TIMEOUT);
  }
}

PRIVATE void
cookie_UnlockPermissionListst(void) {
  PR_EnterMonitor(cookie_permission_lock_monitor);

#ifdef DEBUG
  /* make sure someone doesn't try to free a lock they don't own */
  PR_ASSERT(cookie_permission_lock_owner == PR_CurrentThread());
#endif

  cookie_permission_lock_count--;
  if(cookie_permission_lock_count == 0) {
    cookie_permission_lock_owner = NULL;
    PR_Notify(cookie_permission_lock_monitor);
  }
  PR_ExitMonitor(cookie_permission_lock_monitor);
}

PRIVATE void
cookie_SavePermissions();
PRIVATE void
cookie_SaveCookies();

PRIVATE void
cookie_FreePermission(cookie_PermissionStruct * cookie_permission, PRBool save) {
  /*
   * This routine should only be called while holding the
   * cookie permission list lock
   */
  if(!cookie_permission) {
    return;
  }
  if (cookie_permissionList == nsnull)
    return;
  cookie_permissionList->RemoveElement(cookie_permission);
  PR_FREEIF(cookie_permission->host);
  PR_Free(cookie_permission);
  if (save) {
    cookie_permissionsChanged = PR_TRUE;
    cookie_SavePermissions();
  }
}

/* blows away all cookie permissions currently in the list */
PRIVATE void
cookie_RemoveAllPermissions() {
  cookie_PermissionStruct * victim;
 
  /* check for NULL or empty list */
  cookie_LockPermissionList();
  if (cookie_permissionList == nsnull) {
    cookie_UnlockPermissionListst();
    return;
  }
  PRInt32 count = cookie_permissionList->Count();
  for (PRInt32 i = 0; i < count; ++i) {
    victim = NS_STATIC_CAST(cookie_PermissionStruct*, cookie_permissionList->ElementAt(i));
    if (victim) {
      cookie_FreePermission(victim, PR_FALSE);
	  i = -1;
    }
  }
  delete cookie_permissionList;
  cookie_permissionList = NULL;
  cookie_UnlockPermissionListst();
}

/* This should only get called while holding the cookie-lock
**
*/
PRIVATE void
cookie_FreeCookie(cookie_CookieStruct * cookie) {
  if(!cookie) {
    return;
  }
  if (cookie_cookieList == nsnull) {
    return;
  }
  cookie_cookieList->RemoveElement(cookie);
  PR_FREEIF(cookie->path);
  PR_FREEIF(cookie->host);
  PR_FREEIF(cookie->name);
  PR_FREEIF(cookie->cookie);
  PR_Free(cookie);
  cookie_cookiesChanged = PR_TRUE;
}

/* blows away all cookies currently in the list, then blows away the list itself
 * nulling it after it's free'd
 */
PRIVATE void
cookie_RemoveAllCookies() {
  cookie_CookieStruct * victim;

  /* check for NULL or empty list */
  cookie_LockCookieList();
  if (cookie_cookieList == nsnull) {
    cookie_UnlockCookieList();
    return;
  }
  PRInt32 count = cookie_cookieList->Count();
  for (PRInt32 i = 0; i < count; ++i) {
    victim = NS_STATIC_CAST(cookie_CookieStruct*, cookie_cookieList->ElementAt(i));
    if (victim) {
      cookie_FreeCookie(victim);
	  i = -1;
    }
  }
  delete cookie_cookieList;
  cookie_cookieList = NULL;
  cookie_UnlockCookieList();
}

PUBLIC void
COOKIE_RemoveAllCookies()
{

	cookie_RemoveAllPermissions();
	cookie_RemoveAllCookies();
}

PRIVATE void
cookie_RemoveOldestCookie(void) {
  cookie_CookieStruct * cookie_s;
  cookie_CookieStruct * oldest_cookie;
  cookie_LockCookieList();

  if (cookie_cookieList == nsnull) {
    cookie_UnlockCookieList();
    return;
  }
   
  PRInt32 count = cookie_cookieList->Count();
  if (count == 0) {
    cookie_UnlockCookieList();
    return;
  }
  oldest_cookie = NS_STATIC_CAST(cookie_CookieStruct*, cookie_cookieList->ElementAt(0));
  for (PRInt32 i = 1; i < count; ++i) {
    cookie_s = NS_STATIC_CAST(cookie_CookieStruct*, cookie_cookieList->ElementAt(i));
    if (cookie_s) {
      if(cookie_s->lastAccessed < oldest_cookie->lastAccessed) {
        oldest_cookie = cookie_s;
      }
    }
  }
  if(oldest_cookie) {
    // TRACEMSG(("Freeing cookie because global max cookies has been exceeded"));
    cookie_FreeCookie(oldest_cookie);
  }

  cookie_UnlockCookieList();
}

/* Remove any expired cookies from memory 
** This routine should only be called while holding the cookie list lock
*/
PRIVATE void
cookie_RemoveExpiredCookies() {
  cookie_CookieStruct * cookie_s;
  time_t cur_time = time(NULL);
  
  if (cookie_cookieList == nsnull) {
    return;
  }
  
  PRInt32 count = cookie_cookieList->Count();
  for (PRInt32 i = 0; i < count; ++i) {
    cookie_s = NS_STATIC_CAST(cookie_CookieStruct*, cookie_cookieList->ElementAt(i));
    if (cookie_s) {
     /*
       * Don't get rid of expire time 0 because these need to last for 
       * the entire session. They'll get cleared on exit.
       */
      if( cookie_s->expires && (cookie_s->expires < cur_time) ) {
        cookie_FreeCookie(cookie_s);
        /*
         * Reset the list_ptr to the beginning of the list.
         * Do this because list_ptr's object was just freed
         * by the call to cookie_FreeCookie struct, even
         * though it's inefficient.
         */
      }
    }
  }
}

/* checks to see if the maximum number of cookies per host
 * is being exceeded and deletes the oldest one in that
 * case
 * This routine should only be called while holding the cookie lock
 */
PRIVATE void
cookie_CheckForMaxCookiesFromHo(const char * cur_host) {
  cookie_CookieStruct * cookie_s;
  cookie_CookieStruct * oldest_cookie = 0;
  int cookie_count = 0;
  
  if (cookie_cookieList == nsnull) {
    return;
  }
  
  PRInt32 count = cookie_cookieList->Count();
  for (PRInt32 i = 0; i < count; ++i) {
    cookie_s = NS_STATIC_CAST(cookie_CookieStruct*, cookie_cookieList->ElementAt(i));
    if (cookie_s) {
      if(!PL_strcasecmp(cookie_s->host, cur_host)) {
        cookie_count++;
        if(!oldest_cookie || oldest_cookie->lastAccessed > cookie_s->lastAccessed) {
          oldest_cookie = cookie_s;
        }
      }
    }
  }
  if(cookie_count >= MAX_COOKIES_PER_SERVER && oldest_cookie) {
    // TRACEMSG(("Freeing cookie because max cookies per server has been exceeded"));
    cookie_FreeCookie(oldest_cookie);
  }
}


/* search for previous exact match
** This routine should only be called while holding the cookie lock
*/
PRIVATE cookie_CookieStruct *
cookie_CheckForPrevCookie(char * path, char * hostname, char * name) {
  cookie_CookieStruct * cookie_s;
  if (cookie_cookieList == nsnull) {
    return NULL;
  }
  
  PRInt32 count = cookie_cookieList->Count();
  for (PRInt32 i = 0; i < count; ++i) {
    cookie_s = NS_STATIC_CAST(cookie_CookieStruct*, cookie_cookieList->ElementAt(i));
    if (cookie_s) {
      if(path && hostname && cookie_s->path && cookie_s->host && cookie_s->name
        && !PL_strcmp(name, cookie_s->name) && !PL_strcmp(path, cookie_s->path)
        && !PL_strcasecmp(hostname, cookie_s->host)) {
        return(cookie_s);
      }
    }
  }
  return(NULL);
}

/* cookie utility functions */
PRIVATE void
cookie_SetBehaviorPref(cookie_BehaviorEnum x) {
  cookie_behavior = x;
//HG83330 -- @@??
  if(cookie_behavior == COOKIE_DontUse) {
//  NET_XP_FileRemove("", xpHTTPCookie);
//  NET_XP_FileRemove("", xpHTTPCookiePermission);
  }
}

PRIVATE void
cookie_SetWarningPref(PRBool x) {
  cookie_warning = x;
}

PRIVATE cookie_BehaviorEnum
cookie_GetBehaviorPref() {
  return cookie_behavior;
}

PRIVATE PRBool
cookie_GetWarningPref() {
  return cookie_warning;
}

MODULE_PRIVATE int PR_CALLBACK
cookie_BehaviorPrefChanged(const char * newpref, void * data) {
  PRInt32 n;
  if ((PREF_OK != PREF_GetIntPref(cookie_behaviorPref, &n))) {
    cookie_SetBehaviorPref(COOKIE_Accept);
  } else {
    cookie_SetBehaviorPref((cookie_BehaviorEnum)n);
  }
  return PREF_NOERROR;
}

MODULE_PRIVATE int PR_CALLBACK
cookie_WarningPrefChanged(const char * newpref, void * data) {
  PRBool x;
  if ((PREF_OK != PREF_GetBoolPref(cookie_warningPref, &x))) {
    x = PR_FALSE;
  }
  cookie_SetWarningPref(x);
  return PREF_NOERROR;
}

/*
 * search if permission already exists
 */
PRIVATE cookie_PermissionStruct *
cookie_CheckForPermission(char * hostname) {
  cookie_PermissionStruct * cookie_s;

  /* ignore leading period in host name */
  while (hostname && (*hostname == '.')) {
    hostname++;
  }

  cookie_LockPermissionList();
  if (cookie_permissionList == nsnull) {
    cookie_UnlockPermissionListst();
    return(NULL);
  }
 
  PRInt32 count = cookie_permissionList->Count();
  for (PRInt32 i = 0; i < count; ++i) {
    cookie_s = NS_STATIC_CAST(cookie_PermissionStruct*, cookie_permissionList->ElementAt(i));
    if (cookie_s) {
      if(hostname && cookie_s->host && !PL_strcasecmp(hostname, cookie_s->host)) {
        cookie_UnlockPermissionListst();
        return(cookie_s);
      }
    }
  }
  cookie_UnlockPermissionListst();
  return(NULL);
}

/* called from mkgeturl.c, NET_InitNetLib(). This sets the module local 
 * cookie pref variables and registers the callbacks
 */
PUBLIC void
COOKIE_RegisterCookiePrefCallbacks(void) {
  PRInt32 n;
  PRBool x;
  if ((PREF_OK != PREF_GetIntPref(cookie_behaviorPref, &n))) {
    cookie_SetBehaviorPref(COOKIE_Accept);
  } else {
    cookie_SetBehaviorPref((cookie_BehaviorEnum)n);
  }
  cookie_SetBehaviorPref((cookie_BehaviorEnum)n);
  PREF_RegisterCallback(cookie_behaviorPref, cookie_BehaviorPrefChanged, NULL);
  if ((PREF_OK != PREF_GetBoolPref(cookie_warningPref, &x))) {
    x = PR_FALSE;
  }
  cookie_SetWarningPref(x);
  PREF_RegisterCallback(cookie_warningPref, cookie_WarningPrefChanged, NULL);
}

/* returns PR_TRUE if authorization is required
** 
**
** IMPORTANT:  Now that this routine is mutli-threaded it is up
**             to the caller to free any returned string
*/
PUBLIC char *
COOKIE_GetCookie(char * address) {
  char *name=0;
  char *host = cookie_ParseURL(address, GET_HOST_PART);
  char *path = cookie_ParseURL(address, GET_PATH_PART);
  cookie_CookieStruct * cookie_s;
  PRBool first=PR_TRUE;
  PRBool xxx = PR_FALSE;
  time_t cur_time = time(NULL);
  int host_length;
  int domain_length;

  /* return string to build */
  char * rv=0;

  /* disable cookies if the user's prefs say so */
  if(cookie_GetBehaviorPref() == COOKIE_DontUse) {
    return NULL;
  }
  if (!PL_strncasecmp(address, "https", 5)) {
     xxx = PR_TRUE;
  }

  /* search for all cookies */
  cookie_LockCookieList();
  if (cookie_cookieList == nsnull) {
    cookie_UnlockCookieList();
    return NULL;
  }
  PRInt32 count = cookie_cookieList->Count();
  for (PRInt32 i = 0; i < count; ++i) {
    cookie_s = NS_STATIC_CAST(cookie_CookieStruct*, cookie_cookieList->ElementAt(i));
    if (cookie_s == nsnull) {
      continue;
	}
    if(!cookie_s->host) {
		continue;
	}

    /* check the host or domain first */
    if(cookie_s->isDomain) {
      char *cp;
      domain_length = PL_strlen(cookie_s->host);

      /* calculate the host length by looking at all characters up to a
       * colon or '\0'.  That way we won't include port numbers in domains
       */
      for(cp=host; *cp != '\0' && *cp != ':'; cp++) {
        ; /* null body */ 
      }
      host_length = cp - host;
      if(domain_length > host_length ||
          PL_strncasecmp(cookie_s->host, &host[host_length - domain_length], domain_length)) {
//      HG20476 -- @@??
        /* no match. FAIL */
        continue;
      }
    } else if(PL_strcasecmp(host, cookie_s->host)) {
      /* hostname matchup failed. FAIL */
      continue;
    }

    /* shorter strings always come last so there can be no ambiquity */
    if(cookie_s->path && !PL_strncmp(path, cookie_s->path, PL_strlen(cookie_s->path))) {

      /* if the cookie is xxx and the path isn't, dont send it */
      if (cookie_s->xxx & !xxx) {
        continue; /* back to top of while */
      }

      /* check for expired cookies */
      if( cookie_s->expires && (cookie_s->expires < cur_time) ) {
        /* expire and remove the cookie */
        cookie_FreeCookie(cookie_s);

        /* start the list parsing over :( we must also start the string over */
        PR_FREEIF(rv);
        rv = NULL;
        i = -1;
        first = PR_TRUE; /* reset first */
        continue;
      }
      if(first) {
        first = PR_FALSE;
      } else {
        StrAllocCat(rv, "; ");
      }
      if(cookie_s->name && *cookie_s->name != '\0') {
        cookie_s->lastAccessed = cur_time;
        StrAllocCopy(name, cookie_s->name);
        StrAllocCat(name, "=");

#ifdef PREVENT_DUPLICATE_NAMES
        /* make sure we don't have a previous name mapping already in the string */
        if(!rv || !PL_strstr(rv, name)) {
          StrAllocCat(rv, name);
          StrAllocCat(rv, cookie_s->cookie);
        }
#else
        StrAllocCat(rv, name);
        StrAllocCat(rv, cookie_s->cookie);
#endif /* PREVENT_DUPLICATE_NAMES */
      }	else {
        StrAllocCat(rv, cookie_s->cookie);
      }
    }
  }

  cookie_UnlockCookieList();
  PR_FREEIF(name);
  PR_Free(path);
  PR_Free(host);

  /* may be NULL */
  return(rv);
}

void
cookie_AddPermission(cookie_PermissionStruct * cookie_permission, PRBool save ) {
  /*
   * This routine should only be called while holding the
   * cookie permission list lock
   */
  if (cookie_permission) {
    if(!cookie_permissionList) {
      cookie_permissionList = new nsVoidArray();
      if(!cookie_permissionList) {
        PR_Free(cookie_permission->host);
        PR_Free(cookie_permission);
        return;
      }
    }

#ifdef alphabetize
    /* add it to the list in alphabetical order */
    {
      cookie_PermissionStruct * tmp_cookie_permission;
      PRBool permissionAdded = PR_FALSE;
      PRInt32 count = cookie_permissionList->Count();
      for (PRInt32 i = 0; i < count; ++i) {
        tmp_cookie_permission = NS_STATIC_CAST(cookie_PermissionStruct*, cookie_permissionList->ElementAt(i));
        if (tmp_cookie_permission) {
          if (PL_strcasecmp(cookie_permission->host,tmp_cookie_permission->host)<0) {
            cookie_permissionList->InsertElementAt(cookie_permission, i);
            permissionAdded = PR_TRUE;
            break;
          }

        }
      }
      if (!permissionAdded) {
        cookie_permissionList->AppendElement(cookie_permission);
      }
    }
#else
    /* add it to the end of the list */
    cookie_permissionList->AppendElement(cookie_permission);
#endif

    if (save) {
      cookie_permissionsChanged = PR_TRUE;
      cookie_SavePermissions();
    }
  }
}

MODULE_PRIVATE PRBool
cookie_IsFromHost(cookie_CookieStruct *cookie_s, char *host) {
  if (!cookie_s || !(cookie_s->host)) {
    return PR_FALSE;
  }
  if (cookie_s->isDomain) {
    char *cp;
    int domain_length, host_length;
    domain_length = PL_strlen(cookie_s->host);

    /* calculate the host length by looking at all characters up to
     * a colon or '\0'.  That way we won't include port numbers
     * in domains
     */
    for(cp=host; *cp != '\0' && *cp != ':'; cp++) {
      ; /* null body */
    }
    host_length = cp - host;

    /* compare the tail end of host to cook_s->host */
    return 
      (domain_length <= host_length && 
        PL_strncasecmp
          (cookie_s->host, &host[host_length - domain_length], domain_length) == 0);
  } else {
    return PL_strcasecmp(host, cookie_s->host) == 0;
  }
}

/* find out how many cookies this host has already set */
PRIVATE int
cookie_Count(char * host) {
  int count = 0;
  cookie_CookieStruct * cookie;
  cookie_LockCookieList();
  if (cookie_cookieList == nsnull) {
    cookie_UnlockCookieList();
    return count;
  }
  PRInt32 cookie_count = cookie_cookieList->Count();
  for (PRInt32 i = 0; i < cookie_count; ++i) {
    cookie = NS_STATIC_CAST(cookie_CookieStruct*, cookie_cookieList->ElementAt(i));
    if (cookie) {
      if (host && cookie_IsFromHost(cookie, host)) {
        count++;
      }
    }
  }
  cookie_UnlockCookieList();
  return count;
}

PRIVATE void
cookie_Defer(char * curURL, char * setCookieHeader, time_t timeToExpire) {
  cookie_DeferStruct * defer_cookie = PR_NEW(cookie_DeferStruct);
  defer_cookie->curURL = NULL;
  StrAllocCopy(defer_cookie->curURL, curURL);
  defer_cookie->setCookieHeader = NULL;
  StrAllocCopy(defer_cookie->setCookieHeader, setCookieHeader);
  defer_cookie->timeToExpire = timeToExpire;
  cookie_LockDeferList();
  if (!cookie_deferList) {
    cookie_deferList = new nsVoidArray();
    if (!cookie_deferList) {
      PR_FREEIF(defer_cookie->curURL);
      PR_FREEIF(defer_cookie->setCookieHeader);
      PR_Free(defer_cookie);
      cookie_UnlockDeferList();
      return;
    }
  }
  cookie_deferList->InsertElementAt(defer_cookie, 0);
  cookie_UnlockDeferList();
}
           
PRIVATE void
cookie_SetCookieString(char * curURL, char * setCookieHeader, time_t timeToExpire );

PRIVATE void
cookie_Undefer() {
  cookie_DeferStruct * defer_cookie;
  cookie_LockDeferList();
  if(cookie_deferList == nsnull) {
    cookie_UnlockDeferList();
    return;
  }
  PRInt32 count = cookie_deferList->Count();
  if (count == 0) {
    cookie_UnlockDeferList();
    return;
  }

  defer_cookie = NS_STATIC_CAST(cookie_DeferStruct*, cookie_deferList->ElementAt(count-1));
  cookie_deferList->RemoveElementAt(count-1);
  cookie_UnlockDeferList();
  if (defer_cookie) {
    cookie_SetCookieString
      (defer_cookie->curURL, defer_cookie->setCookieHeader, defer_cookie->timeToExpire);
    PR_FREEIF(defer_cookie->curURL);
    PR_FREEIF(defer_cookie->setCookieHeader);
    PR_Free(defer_cookie);
  }
}

/* Java script is calling COOKIE_SetCookieString, netlib is calling 
 * this via COOKIE_SetCookieStringFromHttp.
 */
PRIVATE void
cookie_SetCookieString(char * curURL, char * setCookieHeader, time_t timeToExpire) {
  cookie_CookieStruct * prev_cookie;
  char *path_from_header=NULL, *host_from_header=NULL;
  char *host_from_header2=NULL;
  char *name_from_header=NULL, *cookie_from_header=NULL;
  time_t expires=0;
  char *cur_path = cookie_ParseURL(curURL, GET_PATH_PART);
  char *cur_host = cookie_ParseURL(curURL, GET_HOST_PART);
  char *semi_colon, *ptr, *equal;
  PRBool xxx=PR_FALSE, isDomain=PR_FALSE, accept=PR_FALSE;
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
  if(cookie_GetBehaviorPref() == COOKIE_DontUse) {
    PR_Free(cur_path);
    PR_Free(cur_host);
    return;
  }

  /* Don't enter this routine if it is already in use by another
     thread.  Otherwise the "remember this decision" result of the
     other cookie (which came first) won't get applied to this cookie.
   */
  if (cookie_SetCookieStringInUse) {
    PR_Free(cur_path);
    PR_Free(cur_host);
    cookie_Defer(curURL, setCookieHeader, timeToExpire);
    return;
  }
  cookie_SetCookieStringInUse = PR_TRUE;
//HG87358 -- @@??

  /* terminate at any carriage return or linefeed */
  for(ptr=setCookieHeader; *ptr; ptr++) {
    if(*ptr == LF || *ptr == CR) {
      *ptr = '\0';
      break;
    }
  }

  /* parse path and expires attributes from header if present */
  semi_colon = PL_strchr(setCookieHeader, ';');
  if(semi_colon) {
    /* truncate at semi-colon and advance */
    *semi_colon++ = '\0';

    /* there must be some attributes. (hopefully) */
    if (ptr=PL_strcasestr(semi_colon, "secure=")) {
      char cPre=*(ptr-1), cPost=*(ptr+6);
      if (((cPre==' ') || (cPre==';')) && (!cPost || (cPost==' ') || (cPost==';'))) {
        xxx = PR_TRUE;
      } 
    }

    /* look for the path attribute */
    ptr = PL_strcasestr(semi_colon, "path=");
    if(ptr) {
      /* allocate more than we need */
      StrAllocCopy(path_from_header, XP_StripLine(ptr+5));
      /* terminate at first space or semi-colon */
      for(ptr=path_from_header; *ptr != '\0'; ptr++) {
        if(COOKIE_IS_SPACE(*ptr) || *ptr == ';' || *ptr == ',') {
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
      char *domain_from_header=NULL;
      char *dot, *colon;
      int domain_length, cur_host_length;

      /* allocate more than we need */
      StrAllocCopy(domain_from_header, XP_StripLine(ptr+7));

      /* terminate at first space or semi-colon */
      for(ptr=domain_from_header; *ptr != '\0'; ptr++) {
        if(COOKIE_IS_SPACE(*ptr) || *ptr == ';' || *ptr == ',') {
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
        PR_Free(domain_from_header);
        PR_Free(cur_path);
        PR_Free(cur_host);
        // TRACEMSG(("DOMAIN failed two dot test"));
        cookie_SetCookieStringInUse = PR_FALSE;
        cookie_Undefer();
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
      if(domain_length > cur_host_length ||
          PL_strcasecmp(domain_from_header, &cur_host[cur_host_length-domain_length])) {
          // TRACEMSG(("DOMAIN failed host within domain test."
//        " Domain: %s, Host: %s", domain_from_header, cur_host));
        PR_Free(domain_from_header);
        PR_Free(cur_path);
        PR_Free(cur_host);
        cookie_SetCookieStringInUse = PR_FALSE;
        cookie_Undefer();
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
      if ( PREF_GetBoolPref(cookie_strictDomainsPref, &pref_scd) < 0 ) {
        pref_scd = PR_FALSE;
      }
      if ( pref_scd == PR_TRUE ) {
        cur_host[cur_host_length-domain_length] = '\0';
        dot = PL_strchr(cur_host, '.');
        cur_host[cur_host_length-domain_length] = '.';
        if (dot) {
        // TRACEMSG(("host minus domain failed no-dot test."
//          " Domain: %s, Host: %s", domain_from_header, cur_host));
          PR_Free(domain_from_header);
          PR_Free(cur_path);
          PR_Free(cur_host);
          cookie_SetCookieStringInUse = PR_FALSE;
          cookie_Undefer();
          return;
        }
      }

      /* all tests passed, copy in domain to hostname field */
      StrAllocCopy(host_from_header, domain_from_header);
      isDomain = PR_TRUE;
      // TRACEMSG(("Accepted domain: %s", host_from_header));
      PR_Free(domain_from_header);
    }

    /* now search for the expires header 
     * NOTE: that this part of the parsing
     * destroys the original part of the string
     */
    ptr = PL_strcasestr(semi_colon, "expires=");
    if(ptr) {
      char *date =  ptr+8;
      /* terminate the string at the next semi-colon */
      for(ptr=date; *ptr != '\0'; ptr++) {
        if(*ptr == ';') {
          *ptr = '\0';
          break;
        }
      }
      if(timeToExpire) {
        expires = timeToExpire;
      } else {
        expires = cookie_ParseDate(date);
      }
      // TRACEMSG(("Have expires date: %ld", expires));
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
  if(PL_strlen(setCookieHeader) > MAX_BYTES_PER_COOKIE) {
    setCookieHeader[MAX_BYTES_PER_COOKIE-1] = '\0';
  }

  /* separate the name from the cookie */
  equal = PL_strchr(setCookieHeader, '=');
  if(equal) {
    *equal = '\0';
    StrAllocCopy(name_from_header, XP_StripLine(setCookieHeader));
    StrAllocCopy(cookie_from_header, XP_StripLine(equal+1));
  } else {
    // TRACEMSG(("Warning: no name found for cookie"));
    StrAllocCopy(cookie_from_header, XP_StripLine(setCookieHeader));
    StrAllocCopy(name_from_header, "");
  }

  cookie_PermissionStruct * cookie_permission;
  cookie_permission = cookie_CheckForPermission(host_from_header);
  if (cookie_permission != NULL) {
    if (cookie_permission->permission == PR_FALSE) {
      PR_FREEIF(path_from_header);
      PR_FREEIF(host_from_header);
      PR_FREEIF(name_from_header);
      PR_FREEIF(cookie_from_header);
      cookie_SetCookieStringInUse = PR_FALSE;
      cookie_Undefer();
      return;
    } else {
      accept = PR_TRUE;
    }
  }
  if( (cookie_GetWarningPref() ) && !accept ) {
    /* the user wants to know about cookies so let them know about every one that
     * is set and give them the choice to accept it or not
     */
    char * new_string=0;
    int count;
    char * remember_string = cookie_Localize("RememberThisDecision");

    /* find out how many cookies this host has already set */
    count = cookie_Count(host_from_header);
    cookie_LockCookieList();
    prev_cookie = cookie_CheckForPrevCookie
      (path_from_header, host_from_header, name_from_header);
    cookie_UnlockCookieList();
    char * message;
    if (prev_cookie) {
      message = cookie_Localize("PermissionToModifyCookie");
      new_string = PR_smprintf(message, host_from_header ? host_from_header : "");
    } else if (count>1) {
      message = cookie_Localize("PermissionToSetAnotherCookie");
      new_string = PR_smprintf(message, host_from_header ? host_from_header : "", count);
    } else if (count==1){
      message = cookie_Localize("PermissionToSetSecondCookie");
      new_string = PR_smprintf(message, host_from_header ? host_from_header : "");
    } else {
      message = cookie_Localize("PermissionToSetACookie");
      new_string = PR_smprintf(message, host_from_header ? host_from_header : "");
    }
    PR_FREEIF(message);

    /* 
     * Who knows what thread we are on.  Only the mozilla thread
     *   is allowed to post dialogs so, if need be, go over there
     */

    {
      PRBool rememberChecked = cookie_rememberChecked;
      PRBool userHasAccepted =
        cookie_CheckConfirmYN(new_string, remember_string, &cookie_rememberChecked);
      PR_FREEIF(new_string);
      PR_FREEIF(remember_string);
      if (cookie_rememberChecked) {
        cookie_PermissionStruct * cookie_permission;
        cookie_permission = PR_NEW(cookie_PermissionStruct);
        if (cookie_permission) {
          cookie_LockPermissionList();
          StrAllocCopy(host_from_header2, host_from_header);
          /* ignore leading periods in host name */
          while (host_from_header2 && (*host_from_header2 == '.')) {
            host_from_header2++;
          }
          cookie_permission->host = host_from_header2; /* set host string */
          cookie_permission->permission = userHasAccepted;
          cookie_AddPermission(cookie_permission, PR_TRUE);
          cookie_UnlockPermissionListst();
        }
      }
      if (rememberChecked != cookie_rememberChecked) {
        cookie_permissionsChanged = PR_TRUE;
        cookie_SavePermissions();
      }
      if (!userHasAccepted) {
        PR_FREEIF(path_from_header);
        PR_FREEIF(host_from_header);
        PR_FREEIF(name_from_header);
        PR_FREEIF(cookie_from_header);
        cookie_SetCookieStringInUse = PR_FALSE;
        return;
      }
    }
  }

  //TRACEMSG(("mkaccess.c: Setting cookie: %s for host: %s for path: %s",
  //          cookie_from_header, host_from_header, path_from_header));

  /* 
   * We have decided we are going to insert a cookie into the list
   *   Get the cookie lock so that we can munge on the list
   */
  cookie_LockCookieList();

  /* limit the number of cookies from a specific host or domain */
  cookie_CheckForMaxCookiesFromHo(host_from_header);

  if (cookie_cookieList) {
    if(cookie_cookieList->Count() > MAX_NUMBER_OF_COOKIES-1) {
      cookie_RemoveOldestCookie();
    }
  }
  prev_cookie = cookie_CheckForPrevCookie (path_from_header, host_from_header, name_from_header);
  if(prev_cookie) {
    prev_cookie->expires = expires;
    PR_FREEIF(prev_cookie->cookie);
    PR_FREEIF(prev_cookie->path);
    PR_FREEIF(prev_cookie->host);
    PR_FREEIF(prev_cookie->name);
    prev_cookie->cookie = cookie_from_header;
    prev_cookie->path = path_from_header;
    prev_cookie->host = host_from_header;
    prev_cookie->name = name_from_header;
    prev_cookie->xxx = xxx;
    prev_cookie->isDomain = isDomain;
    prev_cookie->lastAccessed = time(NULL);
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
      cookie_UnlockCookieList();
      cookie_SetCookieStringInUse = PR_FALSE;
      cookie_Undefer();
      return;
    }
    
    /* copy */
    prev_cookie->cookie = cookie_from_header;
    prev_cookie->name = name_from_header;
    prev_cookie->path = path_from_header;
    prev_cookie->host = host_from_header;
    prev_cookie->expires = expires;
    prev_cookie->xxx = xxx;
    prev_cookie->isDomain = isDomain;
    prev_cookie->lastAccessed = time(NULL);
    if(!cookie_cookieList) {
      cookie_cookieList = new nsVoidArray();
      if(!cookie_cookieList) {
        PR_FREEIF(path_from_header);
        PR_FREEIF(name_from_header);
        PR_FREEIF(host_from_header);
        PR_FREEIF(cookie_from_header);
        PR_Free(prev_cookie);
        cookie_UnlockCookieList();
        cookie_SetCookieStringInUse = PR_FALSE;
        cookie_Undefer();
        return;
      }
    }		

    /* add it to the list so that it is before any strings of smaller length */
    bCookieAdded = PR_FALSE;
    new_len = PL_strlen(prev_cookie->path);
    PRInt32 count = cookie_cookieList->Count();
    for (PRInt32 i = 0; i < count; ++i) {
      tmp_cookie_ptr = NS_STATIC_CAST(cookie_CookieStruct*, cookie_cookieList->ElementAt(i));
      if (tmp_cookie_ptr) {
        if(new_len > PL_strlen(tmp_cookie_ptr->path)) {
          cookie_cookieList->InsertElementAt(tmp_cookie_ptr, i);
          bCookieAdded = PR_TRUE;
          break;
        }
      }
    }
    if ( !bCookieAdded ) {
      /* no shorter strings found in list */
      cookie_cookieList->AppendElement(prev_cookie);
    }
  }

  /* At this point we know a cookie has changed. Write the cookies to file. */
  cookie_cookiesChanged = PR_TRUE;
  cookie_SaveCookies();
  cookie_UnlockCookieList();
  cookie_SetCookieStringInUse = PR_FALSE;
  cookie_Undefer();
  return;
}

PUBLIC void
COOKIE_SetCookieString(char * curURL, char * setCookieHeader) {
  cookie_SetCookieString(curURL, setCookieHeader, 0);
}

/* Determines whether the inlineHost is in the same domain as the currentHost.
 * For use with rfc 2109 compliance/non-compliance.
 */
PRIVATE int
cookie_SameDomain(char * currentHost, char * inlineHost) {
  char * dot = 0;
  char * currentDomain = 0;
  char * inlineDomain = 0;
  if(!currentHost || !inlineHost) {
    return 0;
  }

  /* case insensitive compare */
  if(PL_strcasecmp(currentHost, inlineHost) == 0) {
    return 1;
  }
  currentDomain = PL_strchr(currentHost, '.');
  inlineDomain = PL_strchr(inlineHost, '.');
  if(!currentDomain || !inlineDomain) {
    return 0;
  }

  /* check for at least two dots before continuing, if there are
   * not two dots we don't have enough information to determine
   * whether or not the inlineDomain is within the currentDomain
   */
  dot = PL_strchr(inlineDomain, '.');
  if(dot) {
    dot = PL_strchr(dot+1, '.');
  } else {
    return 0;
  }

  /* handle .com. case */
  if(!dot || (*(dot+1) == '\0')) {
    return 0;
  }
  if(!PL_strcasecmp(inlineDomain, currentDomain)) {
    return 1;
  }
  return 0;
}

/* This function wrapper wraps COOKIE_SetCookieString for the purposes of 
 * determining whether or not a cookie is inline (we need the URL struct, 
 * and outputFormat to do so).  this is called from NET_ParseMimeHeaders 
 * in mkhttp.c
 * This routine does not need to worry about the cookie lock since all of
 *   the work is handled by sub-routines
*/

PUBLIC void
COOKIE_SetCookieStringFromHttp(char * curURL, char * setCookieHeader, char * server_date) {
  /* If the outputFormat is not PRESENT (the url is not going to the screen), and not
   *  SAVE AS (shift-click) then 
   *  the cookie being set is defined as inline so we need to do what the user wants us
   *  to based on his preference to deal with foreign cookies. If it's not inline, just set
   *  the cookie.
   */
  char *ptr=NULL, *date=NULL;
  time_t gmtCookieExpires=0, expires=0, sDate;

#ifdef later // We need to come back and fix this.  - Neeti
  if(CLEAR_CACHE_BIT(outputFormat) != FO_PRESENT &&
      CLEAR_CACHE_BIT(outputFormat) != FO_SAVE_AS) {
    if (cookie_GetBehaviorPref() == COOKIE_DontAcceptForeign) {
      // the user doesn't want foreign cookies, check to see if its foreign 
      char * curSessionHistHost = 0;
      char * theColon = 0;
      char * curHost = cookie_ParseURL(curURL, GET_HOST_PART);
      History_entry * shistEntry = SHIST_GetCurrent(&context->hist);
      if (shistEntry) {
        curSessionHistHost = cookie_ParseURL(shistEntry->address, GET_HOST_PART);
      } 
      if(!curHost || !curSessionHistHost) {
        PR_FREEIF(curHost);
        PR_FREEIF(curSessionHistHost);
        return;
      }

      /* strip ports */
      theColon = PL_strchr(curHost, ':');
      if(theColon) {
        *theColon = '\0';
      }
      theColon = PL_strchr(curSessionHistHost, ':');
      if(theColon) {
        *theColon = '\0';
      }
      /* if it's foreign, get out of here after a little clean up */
      if(!cookie_SameDomain(curHost, curSessionHistHost)) {
        PR_FREEIF(curHost);	
        PR_FREEIF(curSessionHistHost);
        return;
      }
      PR_FREEIF(curHost);	
      PR_FREEIF(curSessionHistHost);
    }
  }
#endif /* later */

  /* Determine when the cookie should expire. This is done by taking the difference between 
   * the server time and the time the server wants the cookie to expire, and adding that 
   * difference to the client time. This localizes the client time regardless of whether or
   * not the TZ environment variable was set on the client.
   */

  /* Get the time the cookie is supposed to expire according to the attribute*/
  ptr = PL_strcasestr(setCookieHeader, "expires=");
  if(ptr) {
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
  sDate = cookie_ParseDate(server_date);
  if( sDate && expires ) {
    if( expires < sDate ) {
      gmtCookieExpires=1;
    } else {
      gmtCookieExpires = expires - sDate + time(NULL);
      // if overflow 
      if( gmtCookieExpires < time(NULL)) {
        gmtCookieExpires = (((unsigned) (~0) << 1) >> 1); // max int 
      }
    }
  }
  cookie_SetCookieString(curURL, setCookieHeader, gmtCookieExpires);
}


/* saves the HTTP cookies permissions to disk */
PRIVATE void
cookie_SavePermissions() {
  cookie_PermissionStruct * cookie_permission_s;
  if (cookie_GetBehaviorPref() == COOKIE_DontUse) {
    return;
  }
  if (!cookie_permissionsChanged) {
    return;
  }
  cookie_LockPermissionList();
  if (cookie_permissionList == nsnull) {
    cookie_UnlockPermissionListst();
    return;
  }
  nsFileSpec dirSpec;
  nsresult rval = cookie_ProfileDirectory(dirSpec);
  if (NS_FAILED(rval)) {
    cookie_UnlockPermissionListst();
    return;
  }
  nsOutputFileStream strm(dirSpec + "cookperm.txt");
  if (!strm.is_open()) {
    cookie_UnlockPermissionListst();
    return;
  }
  cookie_Put(strm, "# Netscape HTTP Cookie Permission File\n");
  cookie_Put(strm, "# http://www.netscape.com/newsref/std/cookie_spec.html\n");
  cookie_Put(strm, "# This is a generated file!  Do not edit.\n\n");

  /* format shall be:
   * host \t permission
   */
   PRInt32 count = cookie_permissionList->Count();
   for (PRInt32 i = 0; i < count; ++i) {
     cookie_permission_s = NS_STATIC_CAST(cookie_PermissionStruct*, cookie_permissionList->ElementAt(i));
    if (cookie_permission_s) {
      cookie_Put(strm, cookie_permission_s->host);
      if (cookie_permission_s->permission) {
        cookie_Put(strm, "\tTRUE\n");
      } else {
        cookie_Put(strm, "\tFALSE\n");
      }
    }
  }

  /* save current state of cookie nag-box's checkmark */
  if (cookie_rememberChecked) {
    cookie_Put(strm, "@@@@\tTRUE\n");
  } else {
    cookie_Put(strm, "@@@@\tFALSE\n");
  }

  cookie_permissionsChanged = PR_FALSE;
  strm.flush();
  strm.close();
  cookie_UnlockPermissionListst();
}

/* reads the HTTP cookies permission from disk */
PRIVATE void
cookie_LoadPermissions() {
  nsAutoString * buffer;
  cookie_PermissionStruct * new_cookie_permission;
  nsFileSpec dirSpec;
  nsresult rv = cookie_ProfileDirectory(dirSpec);
  if (NS_FAILED(rv)) {
    return;
  }
  nsInputFileStream strm(dirSpec + "cookperm.txt");
  if (!strm.is_open()) {
    /* file doesn't exist -- that's not an error */
    return;
  }

  /* format is:
   * host \t permission
   * if this format isn't respected we move onto the next line in the file.
   */
  cookie_LockPermissionList();
  while(cookie_GetLine(strm,buffer) != -1) {
    if (buffer->CharAt(0) == '#' || buffer->CharAt(0) == CR ||
        buffer->CharAt(0) == LF || buffer->CharAt(0) == 0) {
      delete buffer;
      continue;
    }
    int hostIndex, permissionIndex;
    hostIndex = 0;
    if ((permissionIndex=buffer->Find('\t', hostIndex)+1) == 0) {
      delete buffer;
      continue;      
    }

    /* ignore leading periods in host name */
    while (hostIndex < permissionIndex && (buffer->CharAt(hostIndex) == '.')) {
      hostIndex++;
    }

    nsString host, permission;
    buffer->Mid(host, hostIndex, permissionIndex-hostIndex-1);
    buffer->Mid(permission, permissionIndex, buffer->Length()-permissionIndex);

    /* create a new cookie_struct and fill it in */
    new_cookie_permission = PR_NEW(cookie_PermissionStruct);
    if (!new_cookie_permission) {
      delete buffer;
      cookie_UnlockPermissionListst();
      strm.close();
      return;
    }

    new_cookie_permission->host = host.ToNewCString();
    if (permission == "TRUE") {
      new_cookie_permission->permission = PR_TRUE;
    } else {
      new_cookie_permission->permission = PR_FALSE;
    }
    delete buffer;

    /*
     * a host value of "@@@@" is a special code designating the
     * state of the cookie nag-box's checkmark
     */
    if (host == "@@@@") {
      cookie_rememberChecked = new_cookie_permission->permission;
    } else {
      /* add the permission entry */
      cookie_AddPermission(new_cookie_permission, PR_FALSE );
    }
  }

  strm.close();
  cookie_UnlockPermissionListst();
  cookie_permissionsChanged = PR_FALSE;
  return;
}

/* saves out the HTTP cookies to disk */
PRIVATE void
cookie_SaveCookies() {
  cookie_CookieStruct * cookie_s;
  time_t cur_date = time(NULL);
  char date_string[36];
  if (cookie_GetBehaviorPref() == COOKIE_DontUse) {
    return;
  }
  if (!cookie_cookiesChanged) {
    return;
  }
  cookie_LockCookieList();
  if (cookie_cookieList == nsnull) {
    cookie_UnlockCookieList();
    return;
  }
  nsFileSpec dirSpec;
  nsresult rv = cookie_ProfileDirectory(dirSpec);
  if (NS_FAILED(rv)) {
    cookie_UnlockCookieList();
    return;
  }
  nsOutputFileStream strm(dirSpec + "cookies.txt");
  if (!strm.is_open()) {
    /* file doesn't exist -- that's not an error */
    cookie_UnlockCookieList();
    return;
  }
  cookie_Put(strm, "# Netscape HTTP Cookie File\n");
  cookie_Put(strm, "# http://www.netscape.com/newsref/std/cookie_spec.html\n");
  cookie_Put(strm, "# This is a generated file!  Do not edit.\n\n");

  /* format shall be:
   *
   * host \t isDomain \t path \t xxx \t expires \t name \t cookie
   *
   * isDomain is PR_TRUE or PR_FALSE
   * xxx is PR_TRUE or PR_FALSE  
   * expires is a time_t integer
   * cookie can have tabs
   */
  PRInt32 count = cookie_cookieList->Count();
  for (PRInt32 i = 0; i < count; ++i) {
    cookie_s = NS_STATIC_CAST(cookie_CookieStruct*, cookie_cookieList->ElementAt(i));
    if (cookie_s) {
      if (cookie_s->expires < cur_date) {
        /* don't write entry if cookie has expired or has no expiration date */
        continue;
      }
      cookie_Put(strm, cookie_s->host);
      if (cookie_s->isDomain) {
        cookie_Put(strm, "\tTRUE\t");
      } else {
        cookie_Put(strm, "\tFALSE\t");
      }

      cookie_Put(strm, cookie_s->path);
      if (cookie_s->xxx) {
        cookie_Put(strm, "\tTRUE\t");
      } else {
        cookie_Put(strm, "\tFALSE\t");
      }

      PR_snprintf(date_string, sizeof(date_string), "%lu", cookie_s->expires);
      cookie_Put(strm, date_string);
      cookie_Put(strm, "\t");

      cookie_Put(strm, cookie_s->name);
      cookie_Put(strm, "\t");

      cookie_Put(strm, cookie_s->cookie);
      cookie_Put(strm, "\n");
    }
  }

  cookie_cookiesChanged = PR_FALSE;
  strm.flush();
  strm.close();
  cookie_UnlockCookieList();
}

/* reads HTTP cookies from disk */
PRIVATE void
cookie_LoadCookies() {
  cookie_CookieStruct *new_cookie, *tmp_cookie_ptr;
  size_t new_len;
  nsAutoString * buffer;
  PRBool added_to_list;
  nsFileSpec dirSpec;
  nsresult rv = cookie_ProfileDirectory(dirSpec);
  if (NS_FAILED(rv)) {
    return;
  }
  nsInputFileStream strm(dirSpec + "cookies.txt");
  if (!strm.is_open()) {
    /* file doesn't exist -- that's not an error */
    return;
  }
  cookie_LockCookieList();

  /* format is:
   *
   * host \t isDomain \t path \t xxx \t expires \t name \t cookie
   *
   * if this format isn't respected we move onto the next line in the file.
   * isDomain is PR_TRUE or PR_FALSE	-- defaulting to PR_FALSE
   * xxx is PR_TRUE or PR_FALSE   -- should default to PR_TRUE
   * expires is a time_t integer
   * cookie can have tabs
   */
  while (cookie_GetLine(strm,buffer) != -1){
    added_to_list = PR_FALSE;
    if (buffer->CharAt(0) == '#' || buffer->CharAt(0) == CR ||
        buffer->CharAt(0) == LF || buffer->CharAt(0) == 0) {
      delete buffer;
      continue;
    }
    int hostIndex, isDomainIndex, pathIndex, xxxIndex, expiresIndex, nameIndex, cookieIndex;
    hostIndex = 0;
    if ((isDomainIndex=buffer->Find('\t', hostIndex)+1) == 0 ||
        (pathIndex=buffer->Find('\t', isDomainIndex)+1) == 0 ||
        (xxxIndex=buffer->Find('\t', pathIndex)+1) == 0 ||
        (expiresIndex=buffer->Find('\t', xxxIndex)+1) == 0 ||
        (nameIndex=buffer->Find('\t', expiresIndex)+1) == 0 ||
        (cookieIndex=buffer->Find('\t', nameIndex)+1) == 0 ) {
      delete buffer;
      continue;
    }
    nsString host, isDomain, path, xxx, expires, name, cookie;
    buffer->Mid(host, hostIndex, isDomainIndex-hostIndex-1);
    buffer->Mid(isDomain, isDomainIndex, pathIndex-isDomainIndex-1);
    buffer->Mid(path, pathIndex, xxxIndex-pathIndex-1);
    buffer->Mid(xxx, xxxIndex, expiresIndex-xxxIndex-1);
    buffer->Mid(expires, expiresIndex, nameIndex-expiresIndex-1);
    buffer->Mid(name, nameIndex, cookieIndex-nameIndex-1);
    buffer->Mid(cookie, cookieIndex, buffer->Length()-cookieIndex);

    /* create a new cookie_struct and fill it in */
    new_cookie = PR_NEW(cookie_CookieStruct);
    if (!new_cookie) {
      delete buffer;
      cookie_UnlockCookieList();
      strm.close();
      return;
    }
    memset(new_cookie, 0, sizeof(cookie_CookieStruct));
    new_cookie->name = name.ToNewCString();
    new_cookie->cookie = cookie.ToNewCString();
    new_cookie->host = host.ToNewCString();
    new_cookie->path = path.ToNewCString();
    if (isDomain == "TRUE") {
      new_cookie->isDomain = PR_TRUE;
    } else {
      new_cookie->isDomain = PR_FALSE;
    }
    if (xxx == "TRUE") {
      new_cookie->xxx = PR_TRUE;
    } else {
      new_cookie->xxx = PR_FALSE;
    }
    char * expiresCString = expires.ToNewCString();
    new_cookie->expires = atol(expiresCString);
    delete[] expiresCString;
    delete buffer;

    /* start new cookie list if one does not already exist */
    if (!cookie_cookieList) {
      cookie_cookieList = new nsVoidArray();
      if (!cookie_cookieList) {
        cookie_UnlockCookieList();
        strm.close();
        return;
      }
    }

    /* add new cookie to the list so that it is before any strings of smaller length */
    new_len = PL_strlen(new_cookie->path);
    PRInt32 count = cookie_cookieList->Count();
    for (PRInt32 i = 0; i < count; ++i) {
      tmp_cookie_ptr = NS_STATIC_CAST(cookie_CookieStruct*, cookie_cookieList->ElementAt(i));
      if (tmp_cookie_ptr) {
        if (new_len > PL_strlen(tmp_cookie_ptr->path)) {
          cookie_cookieList->InsertElementAt(new_cookie, i);
          added_to_list = PR_TRUE;
          break;
        }
      }
    }

    /* no shorter strings found in list so add new cookie at end */	
    if (!added_to_list) {
      cookie_cookieList->AppendElement(new_cookie);
    }
  }

  strm.close();
  cookie_UnlockCookieList();
  cookie_cookiesChanged = PR_FALSE;
  return;
}


PUBLIC int
COOKIE_ReadCookies() {
  cookie_LoadCookies();
  cookie_LoadPermissions();
  return 0;
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

  /* if host names are equal, make decision based on cookie name */
  return (PL_strcmp (cookie1->name, cookie2->name));
}

/*
 * find the next cookie that is alphabetically after the specified cookie
 *    ordering is based on hostname and the cookie name
 *    if specified cookie is NULL, find the first cookie alphabetically
 */
PRIVATE cookie_CookieStruct *
NextCookieAfter(cookie_CookieStruct * cookie, int * cookieNum) {
  cookie_CookieStruct *cookie_ptr;
  cookie_CookieStruct *lowestCookie = NULL;
  int localCookieNum = 0;
  int lowestCookieNum;

  if (cookie_cookieList == nsnull) 
    return NULL;
  PRInt32 count = cookie_cookieList->Count();
  for (PRInt32 i = 0; i < count; ++i) {
    cookie_ptr = NS_STATIC_CAST(cookie_CookieStruct*, cookie_cookieList->ElementAt(i));
    if (cookie_ptr) {
      if (!cookie || (CookieCompare(cookie_ptr, cookie) > 0)) {
        if (!lowestCookie || (CookieCompare(cookie_ptr, lowestCookie) < 0)) {
          lowestCookie = cookie_ptr;
          lowestCookieNum = localCookieNum;
        }
      }
      localCookieNum++;
    }
  }

  *cookieNum = lowestCookieNum;
  return lowestCookie;
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
  while (quote = PL_strchr(quote, '"')) {
    count = count++;
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

/* return PR_TRUE if "number" is in sequence of comma-separated numbers */
PRIVATE
PRBool cookie_InSequence(char* sequence, int number) {
  char* ptr;
  char* endptr;
  char* undo = NULL;
  PRBool retval = PR_FALSE;
  int i;

  /* not necessary -- routine will work even with null sequence */
  if (!*sequence) {
    return PR_FALSE;
  }

  for (ptr = sequence ; ptr ; ptr = endptr) {

    /* get to next comma */
    endptr = PL_strchr(ptr, ',');

    /* if comma found, process it */
    if (endptr) {

      /* restore last comma-to-null back to comma */
      if (undo) {
        *undo = ',';
      }

      /* set the comma to a null */
      undo = endptr;
      *endptr++ = '\0';

      /* compare the number before the comma with "number" */
      if (*ptr) {
        i = atoi(ptr);
        if (i == number) {

          /* "number" was in the sequence so return PR_TRUE */
          retval = PR_TRUE;
          break;
        }
      }
    }
  }

  /* restore last comma-to-null back to comma */
  if (undo) {
      *undo = ',';
  }
  return retval;
}

PRIVATE char*
cookie_FindValueInArgs(nsAutoString results, char* name) {
  /* note: name must start and end with a vertical bar */
  nsAutoString value;
  PRInt32 start, length;
  start = results.Find(name);
//  XP_ASSERT(start >= 0);
  NS_ASSERTION(start >= 0, "bad data");
  if (start < 0) {
    return nsAutoString("").ToNewCString();
  }
  start += PL_strlen(name); /* get passed the |name| part */
  length = results.Find('|', start) - start;
  results.Mid(value, start, length);
  return value.ToNewCString();
}

PUBLIC void
COOKIE_CookieViewerReturn(nsAutoString results) {
  cookie_CookieStruct * cookie;
  cookie_CookieStruct * cookieToDelete = 0;
  cookie_PermissionStruct * permission;
  cookie_PermissionStruct * permissionToDelete = 0;
  int cookieNumber;
  int permissionNumber;
  PRInt32 count = 0;

  /* step through all cookies and delete those that are in the sequence
   * Note: we can't delete cookie while "cookie_ptr" is pointing to it because
   * that would destroy "cookie_ptr". So we do a lazy deletion
   */
  char * gone = cookie_FindValueInArgs(results, "|goneC|");
  cookieNumber = 0;
  cookie_LockCookieList();
  if (cookie_cookieList) {
    count = cookie_cookieList->Count();
    for (PRInt32 i = 0; i < count; ++i) {
      cookie = NS_STATIC_CAST(cookie_CookieStruct*, cookie_cookieList->ElementAt(i));
      if (cookie) {
        if (cookie_InSequence(gone, cookieNumber)) {
          if (cookieToDelete) {
            cookie_FreeCookie(cookieToDelete);
          }
          cookieToDelete = cookie;
        }
        cookieNumber++;
      }
    }
    if (cookieToDelete) {
      cookie_FreeCookie(cookieToDelete);
      cookie_SaveCookies();
    }
  }
  cookie_UnlockCookieList();
  delete[] gone;

  /* step through all permissions and delete those that are in the sequence
   * Note: we can't delete permission while "permission_ptr" is pointing to it because
   * that would destroy "permission_ptr". So we do a lazy deletion
   */
  gone = cookie_FindValueInArgs(results, "|goneP|");
  permissionNumber = 0;
  cookie_LockPermissionList();
  if (cookie_permissionList) {
    count = cookie_permissionList->Count();
    for (PRInt32 i = 0; i < count; ++i) {
      permission = NS_STATIC_CAST(cookie_PermissionStruct*, cookie_permissionList->ElementAt(i));
      if (permission) {
        if (cookie_InSequence(gone, permissionNumber)) {
          if (permissionToDelete) {
            cookie_FreePermission(permissionToDelete, PR_FALSE);
          }
          permissionToDelete = permission;
        }
        permissionNumber++;
      }
    }
    if (permissionToDelete) {
      cookie_FreePermission(permissionToDelete, PR_TRUE);
    }
  }
  cookie_UnlockPermissionListst();
  delete[] gone;
}

#define BUFLEN2 5000
#define BREAK '\001'

PUBLIC void
COOKIE_GetCookieListForViewer(nsString& aCookieList) {
  char *buffer = (char*)PR_Malloc(BUFLEN2);
  int g = 0, cookieNum;
  cookie_CookieStruct * cookie;

  cookie_LockCookieList();
  buffer[0] = '\0';
  cookieNum = 0;

  /* Get rid of any expired cookies now so user doesn't
   * think/see that we're keeping cookies in memory.
   */
  cookie_RemoveExpiredCookies();

  /* generate the list of cookies in alphabetical order */
  cookie = NULL;
  while (cookie = NextCookieAfter(cookie, &cookieNum)) {
    char *fixed_name = cookie_FixQuoted(cookie->name);
    char *fixed_value = cookie_FixQuoted(cookie->cookie);
    char *fixed_domain_or_host = cookie_FixQuoted(cookie->host);
    char *fixed_path = cookie_FixQuoted(cookie->path);
    char * Domain = cookie_Localize("Domain");
    char * Host = cookie_Localize("Host");
    char * Yes = cookie_Localize("Yes");
    char * No = cookie_Localize("No");
    char * AtEnd = cookie_Localize("AtEndOfSession");

    g += PR_snprintf(buffer+g, BUFLEN2-g,
      "%c%d%c%s%c%s%c%s%c%s%c%s%c%s%c%s",
      BREAK, cookieNum,
      BREAK, fixed_name,
      BREAK, fixed_value,
      BREAK, cookie->isDomain ? Domain : Host,
      BREAK, fixed_domain_or_host,
      BREAK, fixed_path,
      BREAK, cookie->xxx ? Yes : No,
      BREAK,  cookie->expires ? ctime(&(cookie->expires)) : AtEnd
    );
    cookieNum++;
    PR_FREEIF(fixed_name);
    PR_FREEIF(fixed_value);
    PR_FREEIF(fixed_domain_or_host);
    PR_FREEIF(fixed_path);
    PR_FREEIF(Domain);
    PR_FREEIF(Host);
    PR_FREEIF(Yes);
    PR_FREEIF(No);
    PR_FREEIF(AtEnd);
  }
  aCookieList = buffer;
  PR_FREEIF(buffer);
  cookie_UnlockCookieList();
}

PUBLIC void
COOKIE_GetPermissionListForViewer(nsString& aPermissionList) {
  char *buffer = (char*)PR_Malloc(BUFLEN2);
  int g = 0, permissionNum;
  cookie_PermissionStruct * permission;

  cookie_LockPermissionList();
  buffer[0] = '\0';
  permissionNum = 0;

  if (cookie_permissionList == nsnull) {
    cookie_UnlockPermissionListst();
    return;
  }

  PRInt32 count = cookie_permissionList->Count();
  for (PRInt32 i = 0; i < count; ++i) {
    permission = NS_STATIC_CAST(cookie_PermissionStruct*, cookie_permissionList->ElementAt(i));
    if (permission) {
      g += PR_snprintf(buffer+g, BUFLEN2-g,
        "%c%d%c%c%s",
        BREAK,
        permissionNum,
        BREAK,
        permission->permission ? '+' : '-',
        permission->host
      );
      permissionNum++;
    }
  }
  aPermissionList = buffer;
  PR_FREEIF(buffer);
  cookie_UnlockPermissionListst();
}

/* Hack - Neeti remove this */
/* remove front and back white space
 * modifies the original string
 */
PUBLIC char *
XP_StripLine (char *string) {
  char * ptr;

  /* remove leading blanks */
  while(*string=='\t' || *string==' ' || *string=='\r' || *string=='\n') {
    string++;
  }

  for(ptr=string; *ptr; ptr++) {
    ;   /* NULL BODY; Find end of string */
  }

  /* remove trailing blanks */
  for(ptr--; ptr >= string; ptr--) {
    if(*ptr=='\t' || *ptr==' ' || *ptr=='\r' || *ptr=='\n') { 
      *ptr = '\0'; 
    } else { 
      break;
    }
  }
  return string;
}

#ifdef later
PUBLIC History_entry *
SHIST_GetCurrent(History * hist) {
  /* MOZ_FUNCTION_STUB; */
  return NULL;
}
#endif

/*	Very similar to strdup except it free's too
 */
PUBLIC char * 
NET_SACopy (char **destination, const char *source) {
  if(*destination) {
    PR_Free(*destination);
    *destination = 0;
  }
  if (! source) {
    *destination = NULL;
  } else {
    *destination = (char *) PR_Malloc (PL_strlen(source) + 1);
    if (*destination == NULL) {
      return(NULL);
    }
    PL_strcpy (*destination, source);
  }
  return *destination;
}

/*  Again like strdup but it concatinates and free's and uses Realloc
*/
PUBLIC char *
NET_SACat (char **destination, const char *source) {
  if (source && *source) {
    if (*destination) {
      int length = PL_strlen (*destination);
      *destination = (char *) PR_Realloc (*destination, length + PL_strlen(source) + 1);
      if (*destination == NULL) {
        return(NULL);
      }

      PL_strcpy (*destination + length, source);
    } else {
      *destination = (char *) PR_Malloc (PL_strlen(source) + 1);
      if (*destination == NULL) {
        return(NULL);
      }
      PL_strcpy (*destination, source);
    }
  }
  return *destination;
}

MODULE_PRIVATE time_t 
cookie_ParseDate(char *date_string) {

#ifndef USE_OLD_TIME_FUNC

  PRTime prdate;
  time_t date = 0;
  // TRACEMSG(("Parsing date string: %s\n",date_string));

  /* try using PR_ParseTimeString instead */
	
  if(PR_ParseTimeString(date_string, PR_TRUE, &prdate) == PR_SUCCESS) {
    PRInt64 r, u;
    LL_I2L(u, PR_USEC_PER_SEC);
    LL_DIV(r, prdate, u);
    LL_L2I(date, r);
    if (date < 0) {
      date = 0;
    }
    // TRACEMSG(("Parsed date as GMT: %s\n", asctime(gmtime(&date))));
    // TRACEMSG(("Parsed date as local: %s\n", ctime(&date)));
  } else {
    // TRACEMSG(("Could not parse date"));
  }
  return (date);

#else

  struct tm time_info; /* Points to static tm structure */
  char  *ip;
  char   mname[256];
  time_t rv;
  // TRACEMSG(("Parsing date string: %s\n",date_string));
  memset(&time_info, 0, sizeof(struct tm));

  /* Whatever format we're looking at, it will start with weekday. */
  /* Skip to first space. */
  if(!(ip = PL_strchr(date_string,' '))) {
    return 0;
  } else {
    while(COOKIE_IS_SPACE(*ip)) {
      ++ip;
    }
  }
  /* make sure that the date is less than 256.  That will keep name from ever overflowing */
  if(255 < PL_strlen(ip)) {
    return(0);
  }
  if(isalpha(*ip)) {
    /* ctime */
    sscanf(ip, (strstr(ip, "DST") ? "%s %d %d:%d:%d %*s %d" : "%s %d %d:%d:%d %d"),
           mname, &time_info.tm_mday, &time_info.tm_hour, &time_info.tm_min,
           &time_info.tm_sec, &time_info.tm_year);
    time_info.tm_year -= 1900;
  } else if(ip[2] == '-') {
    /* RFC 850 (normal HTTP) */
    char t[256];
    sscanf(ip,"%s %d:%d:%d", t, &time_info.tm_hour, &time_info.tm_min, &time_info.tm_sec);
    t[2] = '\0';
    time_info.tm_mday = atoi(t);
    t[6] = '\0';
    PL_strcpy(mname,&t[3]);
    time_info.tm_year = atoi(&t[7]);
    /* Prevent wraparound from ambiguity */
    if(time_info.tm_year < 70) {
      time_info.tm_year += 100;
    } else if(time_info.tm_year > 1900) {
      time_info.tm_year -= 1900;
    }
  } else {
    /* RFC 822 */
    sscanf(ip,"%d %s %d %d:%d:%d",&time_info.tm_mday, mname, &time_info.tm_year,
           &time_info.tm_hour, &time_info.tm_min, &time_info.tm_sec);
    /* since tm_year is years since 1900 and the year we parsed
     * is absolute, we need to subtract 1900 years from it
     */
    time_info.tm_year -= 1900;
  }
  time_info.tm_mon = NET_MonthNo(mname);
  if(time_info.tm_mon == -1) { /* check for error */
    return(0);
  }
  // TRACEMSG(("Parsed date as: %s\n", asctime(&time_info)));
  rv = mktime(&time_info);
  if(time_info.tm_isdst) {
    rv -= 3600;
  }
  if(rv == -1) {
    // TRACEMSG(("mktime was unable to resolve date/time: %s\n", asctime(&time_info)));
    return(0);
  } else {
    // TRACEMSG(("Parsed date %s\n", asctime(&time_info)));
    // TRACEMSG(("\tas %s\n", ctime(&rv)));
    return(rv);
  }

#endif
}

/* Also skip '>' as part of host name */
MODULE_PRIVATE char * 
cookie_ParseURL (const char *url, int parts_requested) {
  char *rv=0,*colon, *slash, *ques_mark, *hash_mark;
  char *atSign, *host, *passwordColon, *gtThan;
  assert(url);
  if(!url) {
    return(StrAllocCat(rv, ""));
  }
  colon = PL_strchr(url, ':'); /* returns a const char */
  /* Get the protocol part, not including anything beyond the colon */
  if (parts_requested & GET_PROTOCOL_PART) {
    if(colon) {
      char val = *(colon+1);
      *(colon+1) = '\0';
      StrAllocCopy(rv, url);
      *(colon+1) = val;
      /* If the user wants more url info, tack on extra slashes. */
      if( (parts_requested & GET_HOST_PART)
          || (parts_requested & GET_USERNAME_PART)
          || (parts_requested & GET_PASSWORD_PART)) {
        if( *(colon+1) == '/' && *(colon+2) == '/') {
          StrAllocCat(rv, "//");
        }
        /* If there's a third slash consider it file:/// and tack on the last slash. */
        if( *(colon+3) == '/' ) {
          StrAllocCat(rv, "/");
        }
      }
    }
  }

  /* Get the username if one exists */
  if (parts_requested & GET_USERNAME_PART) {
    if (colon && (*(colon+1) == '/') && (*(colon+2) == '/') && (*(colon+3) != '\0')) {
      if ( (slash = PL_strchr(colon+3, '/')) != NULL) {
        *slash = '\0';
      }
      if ( (atSign = PL_strchr(colon+3, '@')) != NULL) {
        *atSign = '\0';
        if ( (passwordColon = PL_strchr(colon+3, ':')) != NULL) {
          *passwordColon = '\0';
        }
        StrAllocCat(rv, colon+3);

        /* Get the password if one exists */
        if (parts_requested & GET_PASSWORD_PART) {
          if (passwordColon) {
            StrAllocCat(rv, ":");
            StrAllocCat(rv, passwordColon+1);
          }
        }
        if (parts_requested & GET_HOST_PART) {
          StrAllocCat(rv, "@");
        }
        if (passwordColon) {
          *passwordColon = ':';
        }
        *atSign = '@';
      }
      if (slash) {
        *slash = '/';
      }
    }
  }

  /* Get the host part */
  if (parts_requested & GET_HOST_PART) {
    if(colon) {
      if(*(colon+1) == '/' && *(colon+2) == '/') {
        slash = PL_strchr(colon+3, '/');
        if(slash) {
          *slash = '\0';
        }
        if( (atSign = PL_strchr(colon+3, '@')) != NULL) {
          host = atSign+1;
        } else {
          host = colon+3;
        }
        ques_mark = PL_strchr(host, '?');
        if(ques_mark) {
          *ques_mark = '\0';
        }
        gtThan = PL_strchr(host, '>');
        if (gtThan) {
          *gtThan = '\0';
        }

        /*
         * Protect systems whose header files forgot to let the client know when
         * gethostbyname would trash their stack.
         */
#ifndef MAX_HOST_NAME_LEN
#ifdef XP_OS2
#error Managed to get into cookie_ParseURL without defining MAX_HOST_NAME_LEN !!!
#endif
#endif
        /* limit hostnames to within MAX_HOST_NAME_LEN characters to keep from crashing */
        if(PL_strlen(host) > MAX_HOST_NAME_LEN) {
          char * cp;
          char old_char;
          cp = host+MAX_HOST_NAME_LEN;
          old_char = *cp;
          *cp = '\0';
          StrAllocCat(rv, host);
          *cp = old_char;
        } else {
          StrAllocCat(rv, host);
        }
        if(slash) {
          *slash = '/';
        }
        if(ques_mark) {
          *ques_mark = '?';
        }
        if (gtThan) {
          *gtThan = '>';
        }
      }
    }
  }
	
  /* Get the path part */
  if (parts_requested & GET_PATH_PART) {
    if(colon) {
      if(*(colon+1) == '/' && *(colon+2) == '/') {
        /* skip host part */
        slash = PL_strchr(colon+3, '/');
      }	else {
        /* path is right after the colon */
        slash = colon+1;
      }
      if(slash) {
        ques_mark = PL_strchr(slash, '?');
        hash_mark = PL_strchr(slash, '#');
        if(ques_mark) {
          *ques_mark = '\0';
        }
        if(hash_mark) {
          *hash_mark = '\0';
        }
        StrAllocCat(rv, slash);
        if(ques_mark) { 
          *ques_mark = '?';
        }
        if(hash_mark) {
          *hash_mark = '#';
        }
      }
    }
  }
		
  if(parts_requested & GET_HASH_PART) {
    hash_mark = PL_strchr(url, '#'); /* returns a const char * */
    if(hash_mark) {
      ques_mark = PL_strchr(hash_mark, '?');
      if(ques_mark) {
        *ques_mark = '\0';
      }
      StrAllocCat(rv, hash_mark);
      if(ques_mark) {
        *ques_mark = '?';
      }
    }
  }

  if(parts_requested & GET_SEARCH_PART) {
    ques_mark = PL_strchr(url, '?'); /* returns a const char * */
    if(ques_mark) {
      hash_mark = PL_strchr(ques_mark, '#');
      if(hash_mark) {
        *hash_mark = '\0';
      }
      StrAllocCat(rv, ques_mark);
      if(hash_mark) {
        *hash_mark = '#';
      }
    }
  }

  /* copy in a null string if nothing was copied in */
  if(!rv) {
    StrAllocCopy(rv, "");
  }
  /* XP_OS2_FIX IBM-MAS: long URLS blowout tracemsg buffer, set max to 1900 chars */
  // TRACEMSG(("mkparse: ParseURL: parsed - %-.1900s",rv));
  return rv;
}
