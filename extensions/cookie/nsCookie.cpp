/* -*- Mode: C; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
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
 *   Pierre Phaneuf <pp@ludusdesign.com>
 *   Henrik Gemal <gemal@gemal.dk>
 */

#define alphabetize 1

#include "nsCookie.h"
#include "nsString.h"
#include "nsIServiceManager.h"
#include "nsFileStream.h"
#include "nsIDirectoryService.h"
#include "nsAppDirectoryServiceDefs.h"
#include "nsIFile.h"
#include "nsXPIDLString.h"
#include "nsIFileSpec.h"
#include "nsINetSupportDialogService.h"
#include "nsIURL.h"
#include "nsIStringBundle.h"
#include "nsVoidArray.h"
#include "prprf.h"
#include "xp_core.h" // for time_t
#include "nsXPIDLString.h"

#include "nsIPref.h"
#include "nsTextFormatter.h"

static NS_DEFINE_CID(kNetSupportDialogCID, NS_NETSUPPORTDIALOG_CID);
static NS_DEFINE_IID(kStringBundleServiceCID, NS_STRINGBUNDLESERVICE_CID);

#define MAX_NUMBER_OF_COOKIES 300
#define MAX_COOKIES_PER_SERVER 20
#define MAX_BYTES_PER_COOKIE 4096  /* must be at least 1 */
#ifndef MAX_HOST_NAME_LEN
#define MAX_HOST_NAME_LEN 64
#endif

#define image_behaviorPref "network.image.imageBehavior"
#define image_warningPref "network.image.warnAboutImages"
#define cookie_behaviorPref "network.cookie.cookieBehavior"
#define cookie_warningPref "network.cookie.warnAboutCookies"
#define cookie_strictDomainsPref "network.cookie.strictDomains"
#define cookie_lifetimePref "network.cookie.lifetimeOption"
#define cookie_lifetimeValue "network.cookie.lifetimeLimit"
#define cookie_localization "chrome://communicator/locale/wallet/cookie.properties"
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

#define COOKIEPERMISSION 0
#define IMAGEPERMISSION 1
#define NUMBER_OF_PERMISSIONS 2
typedef struct _permission_HostStruct {
  char * host;
  nsVoidArray * permissionList;
} permission_HostStruct;

typedef struct _permission_TypeStruct {
  PRInt32 type;
  PRBool permission;
} permission_TypeStruct;

typedef enum {
  COOKIE_Normal,
  COOKIE_Discard,
  COOKIE_Trim,
  COOKIE_Ask
} COOKIE_LifetimeEnum;

PRIVATE PRBool cookie_cookiesChanged = PR_FALSE;
PRIVATE PRBool cookie_permissionsChanged = PR_FALSE;
PRIVATE PRBool cookie_rememberChecked;
PRIVATE PRBool image_rememberChecked;
PRIVATE COOKIE_BehaviorEnum cookie_behavior = COOKIE_Accept;
PRIVATE PRBool cookie_warning = PR_FALSE;
PRIVATE COOKIE_BehaviorEnum image_behavior = COOKIE_Accept;
PRIVATE PRBool image_warning = PR_FALSE;
PRIVATE COOKIE_LifetimeEnum cookie_lifetimeOpt = COOKIE_Normal;
PRIVATE time_t cookie_lifetimeLimit = 90*24*60*60;

PRIVATE nsVoidArray * cookie_cookieList=0;
PRIVATE nsVoidArray * cookie_permissionList=0;

#define REAL_DIALOG 1

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

PRIVATE PRUnichar*
cookie_Localize(char* genericString) {
  nsresult ret;
  nsAutoString v;

  /* create a bundle for the localization */
  NS_WITH_SERVICE(nsIStringBundleService, pStringService, kStringBundleServiceCID, &ret);
  if (NS_FAILED(ret)) {
    printf("cannot get string service\n");
    return v.ToNewUnicode();
  }
  nsCOMPtr<nsILocale> locale;
  nsCOMPtr<nsIStringBundle> bundle;
  ret = pStringService->CreateBundle(cookie_localization, locale, getter_AddRefs(bundle));
  if (NS_FAILED(ret)) {
    printf("cannot create instance\n");
    return v.ToNewUnicode();
  }

  /* localize the given string */
  nsString   strtmp; strtmp.AssignWithConversion(genericString);
  const PRUnichar *ptrtmp = strtmp.GetUnicode();
  nsXPIDLString ptrv;
  ret = bundle->GetStringFromName(ptrtmp, getter_Copies(ptrv));
  v = ptrv;
  if (NS_FAILED(ret)) {
    printf("cannot get string from name\n");
    return v.ToNewUnicode();
  }
  return v.ToNewUnicode();
}

PRBool
cookie_CheckConfirmYN(nsIPrompt *aPrompter, PRUnichar * szMessage, PRUnichar * szCheckMessage, PRBool* checkValue) {

  nsresult res;
  nsCOMPtr<nsIPrompt> dialog;

  if (aPrompter)
    dialog = aPrompter;
  else
    dialog = do_GetService(kNetSupportDialogCID);
  if (!dialog) {
    *checkValue = 0;
    return PR_FALSE;
  }

  PRInt32 buttonPressed = 1; /* in case user exits dialog by clickin X */
  PRUnichar * yes_string = cookie_Localize("Yes");
  PRUnichar * no_string = cookie_Localize("No");
  PRUnichar * confirm_string = cookie_Localize("Confirm");

  nsString tempStr; tempStr.AssignWithConversion("chrome://global/skin/question-icon.gif");
  res = dialog->UniversalDialog(
    NULL, /* title message */
    confirm_string, /* title text in top line of window */
    szMessage, /* this is the main message */
    szCheckMessage, /* This is the checkbox message */
    yes_string, /* first button text */
    no_string, /* second button text */
    NULL, /* third button text */
    NULL, /* fourth button text */
    NULL, /* first edit field label */
    NULL, /* second edit field label */
    NULL, /* first edit field initial and final value */
    NULL, /* second edit field initial and final value */
    tempStr.GetUnicode() ,
    checkValue, /* initial and final value of checkbox */
    2, /* number of buttons */
    0, /* number of edit fields */
    0, /* is first edit field a password field */
    &buttonPressed);

  if (NS_FAILED(res)) {
    *checkValue = 0;
  }
  if (*checkValue!=0 && *checkValue!=1) {
    *checkValue = 0; /* this should never happen but it is happening!!! */
  }
  Recycle(yes_string);
  Recycle(no_string);
  Recycle(confirm_string);
  return (buttonPressed == 0);

#ifdef yyy
  /* following is an example of the most general usage of UniversalDialog */
  PRUnichar* inoutEdit1 = nsString("Edit field1 initial value").GetUnicode();
  PRUnichar* inoutEdit2 = nsString("Edit field2 initial value").GetUnicode();
  PRBool inoutCheckbox = PR_TRUE;
  PRInt32 buttonPressed;

  res = dialog->UniversalDialog(
    nsString("Title Message").GetUnicode(),
    nsString("Dialog Title").GetUnicode(),
    nsString("This is the main message").GetUnicode(),
    nsString("This is the checkbox message").GetUnicode(),
    nsString("First Button").GetUnicode(),
    nsString("Second Button").GetUnicode(),
    nsString("Third Button").GetUnicode(),
    nsString("Fourth Button").GetUnicode(),
    nsString("First Edit field").GetUnicode(),
    nsString("Second Edit field").GetUnicode(),
    &inoutEdit1,
    &inoutEdit2,
    nsString("chrome://global/skin/question-icon.gif").GetUnicode() ,
    &inoutCheckbox,
    4, /* number of buttons */
    2, /* number of edit fields */
    0, /* is first edit field a password field */
    &buttonPressed);
#endif

}

PRIVATE nsresult cookie_ProfileDirectory(nsFileSpec& dirSpec) {
    
  nsresult res;
  nsCOMPtr<nsIFile> aFile;
  nsCOMPtr<nsIFileSpec> tempSpec;
  
  res = NS_GetSpecialDirectory(NS_APP_USER_PROFILE_50_DIR, getter_AddRefs(aFile));
  if (NS_FAILED(res)) return res;
  
  // TODO: When the calling code can take an nsIFile,
  // this conversion to nsFileSpec can be avoided. 
  nsXPIDLCString pathBuf;
  aFile->GetPath(getter_Copies(pathBuf));
  res = NS_NewFileSpec(getter_AddRefs(tempSpec));
  if (NS_FAILED(res)) return res;
  res = tempSpec->SetNativePath(pathBuf);
  if (NS_FAILED(res)) return res;
  res = tempSpec->GetFileSpec(&dirSpec);
  
  return res;
}

nsresult cookie_Get(nsInputFileStream strm, char& c) {
#define BUFSIZE 128
  static char buffer[BUFSIZE];
  static PRInt32 next = BUFSIZE, count = BUFSIZE;

  if (next == count) {
    if (BUFSIZE > count) { // never say "count < ..." vc6.0 thinks this is a template beginning and crashes
      next = BUFSIZE;
      count = BUFSIZE;
      return NS_ERROR_FAILURE;
    }
    count = strm.read(buffer, BUFSIZE);
    next = 0;
    if (count == 0) {
      next = BUFSIZE;
      count = BUFSIZE;
      return NS_ERROR_FAILURE;
    }
  }
  c = buffer[next++];
  return NS_OK;
}

/*
 * get a line from a file
 * return -1 if end of file reached
 * strip carriage returns and line feeds from end of line
 */
PRInt32
cookie_GetLine(nsInputFileStream strm, nsAutoString& aLine) {

  /* read the line */
  aLine.Truncate();
  char c;
  for (;;) {
    if NS_FAILED(cookie_Get(strm, c)) {
      return -1;
    }
    if (c == '\n') {
      break;
    }

    if (c != '\r') {
      aLine.AppendWithConversion(c);
    }
  }
  return 0;
}


PRIVATE void
permission_Save();
PRIVATE void
cookie_Save();

PRIVATE void
permission_Free(PRInt32 hostNumber, PRInt32 type, PRBool save) {
  /* get the indicated host in the list */
  if (cookie_permissionList == nsnull) {
    return;
  }

  permission_HostStruct * hostStruct;
  permission_TypeStruct * typeStruct;
  PRInt32 actualHostNumber = -1;
  PRInt32 typeIndex;

  PRInt32 count = cookie_permissionList->Count();
  for (PRInt32 j=0, k=0; j<count; j++) {
    hostStruct = NS_STATIC_CAST
      (permission_HostStruct*, cookie_permissionList->ElementAt(j));
    if (!hostStruct) {
      return;
    }
    PRBool present = PR_FALSE;
    PRInt32 count2 = hostStruct->permissionList->Count();
    for (typeIndex=0; typeIndex<count2; typeIndex++) {
      typeStruct = NS_STATIC_CAST
        (permission_TypeStruct*, hostStruct->permissionList->ElementAt(typeIndex));
      if (typeStruct->type == type) {
        present = PR_TRUE;
        break;
      }
    }
    if (present) {
      if (k == hostNumber) {
        actualHostNumber = j;
        break;
      }
      k++;
    }
  }

  if (actualHostNumber == -1) {
    /* host not found, something is wrong */
    return;
  }

  /* remove the particular type */
  hostStruct->permissionList->RemoveElementAt(typeIndex);

  /* if no types are present, remove the entry */
  PRInt32 count2 = hostStruct->permissionList->Count();
  if (count2 == 0) {
    PR_FREEIF(hostStruct->permissionList);
    cookie_permissionList->RemoveElementAt(actualHostNumber);
    PR_FREEIF(hostStruct->host);
    PR_Free(hostStruct);
  }

  /* write the changes out to disk */
  if (save) {
    cookie_permissionsChanged = PR_TRUE;
    permission_Save();
  }
}

/* blows away all permissions currently in the list */
PRIVATE void
cookie_RemoveAllPermissions() {
  permission_HostStruct * hostStruct;
  permission_TypeStruct * typeStruct;
 
  /* check for NULL or empty list */
  if (cookie_permissionList == nsnull) {
    return;
  }
  PRInt32 count = cookie_permissionList->Count();
  for (PRInt32 i = count-1; i >=0; i--) {
    hostStruct = NS_STATIC_CAST
      (permission_HostStruct*, cookie_permissionList->ElementAt(i));
    if (!hostStruct) {
      return;
    }
    PRInt32 count2 = hostStruct->permissionList->Count();
    for (PRInt32 typeIndex = count2-1; typeIndex >=0; typeIndex--) {
      typeStruct = NS_STATIC_CAST
        (permission_TypeStruct*, hostStruct->permissionList->ElementAt(typeIndex));
      permission_Free(i, typeStruct->type, PR_FALSE);
    }
  }
  delete cookie_permissionList;
  cookie_permissionList = NULL;
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
COOKIE_RemoveAllCookies()
{
    cookie_RemoveAllPermissions();

    if (cookie_cookieList) {
        cookie_cookieList->EnumerateBackwards(deleteCookie, nsnull);
        cookie_cookiesChanged = PR_TRUE;
        delete cookie_cookieList;
        cookie_cookieList = nsnull;
    }
}

PRIVATE void
cookie_RemoveOldestCookie(void) {
  cookie_CookieStruct * cookie_s;
  cookie_CookieStruct * oldest_cookie;

  if (cookie_cookieList == nsnull) {
    return;
  }
   
  PRInt32 count = cookie_cookieList->Count();
  if (count == 0) {
    return;
  }
  oldest_cookie = NS_STATIC_CAST(cookie_CookieStruct*, cookie_cookieList->ElementAt(0));
  PRInt32 oldestLoc = 0;
  for (PRInt32 i = 1; i < count; ++i) {
    cookie_s = NS_STATIC_CAST(cookie_CookieStruct*, cookie_cookieList->ElementAt(i));
    NS_ASSERTION(cookie_s, "cookie list is corrupt");
    if(cookie_s->lastAccessed < oldest_cookie->lastAccessed) {
      oldest_cookie = cookie_s;
      oldestLoc = i;
    }
  }
  if(oldest_cookie) {
    cookie_cookieList->RemoveElementAt(oldestLoc);
    deleteCookie((void*)oldest_cookie, nsnull);
    cookie_cookiesChanged = PR_TRUE;
  }

}

/* Remove any expired cookies from memory */
PRIVATE void
cookie_RemoveExpiredCookies() {
  cookie_CookieStruct * cookie_s;
  time_t cur_time = get_current_time();
  
  if (cookie_cookieList == nsnull) {
    return;
  }
  
  for (PRInt32 i = cookie_cookieList->Count(); i > 0;) {
    i--;
    cookie_s = NS_STATIC_CAST(cookie_CookieStruct*, cookie_cookieList->ElementAt(i));
    NS_ASSERTION(cookie_s, "corrupt cookie list");
      /* Don't get rid of expire time 0 because these need to last for 
       * the entire session. They'll get cleared on exit. */
      if( cookie_s->expires && (cookie_s->expires < cur_time) ) {
        cookie_cookieList->RemoveElementAt(i);
        deleteCookie((void*)cookie_s, nsnull);
        cookie_cookiesChanged = PR_TRUE;
      }
  }
}

/* checks to see if the maximum number of cookies per host
 * is being exceeded and deletes the oldest one in that
 * case
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
  PRInt32 oldestLoc = -1;
  for (PRInt32 i = 0; i < count; ++i) {
    cookie_s = NS_STATIC_CAST(cookie_CookieStruct*, cookie_cookieList->ElementAt(i));
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
    cookie_cookieList->RemoveElementAt(oldestLoc);
    deleteCookie((void*)oldest_cookie, nsnull);
    cookie_cookiesChanged = PR_TRUE;
  }
}


/* search for previous exact match */
PRIVATE cookie_CookieStruct *
cookie_CheckForPrevCookie(char * path, char * hostname, char * name) {
  cookie_CookieStruct * cookie_s;
  if (cookie_cookieList == nsnull) {
    return NULL;
  }
  
  PRInt32 count = cookie_cookieList->Count();
  for (PRInt32 i = 0; i < count; ++i) {
    cookie_s = NS_STATIC_CAST(cookie_CookieStruct*, cookie_cookieList->ElementAt(i));
    NS_ASSERTION(cookie_s, "corrupt cookie list");
    if(path && hostname && cookie_s->path && cookie_s->host && cookie_s->name
      && !PL_strcmp(name, cookie_s->name) && !PL_strcmp(path, cookie_s->path)
      && !PL_strcasecmp(hostname, cookie_s->host)) {
      return(cookie_s);
    }
  }
  return(NULL);
}

/* cookie utility functions */
PRIVATE void
cookie_SetBehaviorPref(COOKIE_BehaviorEnum x) {
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

PUBLIC COOKIE_BehaviorEnum
COOKIE_GetBehaviorPref() {
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
  NS_WITH_SERVICE(nsIPref, prefs, "component://netscape/preferences", &rv);
  if (NS_FAILED(prefs->GetIntPref(cookie_behaviorPref, &n))) {
    n = COOKIE_Accept;
  }
  cookie_SetBehaviorPref((COOKIE_BehaviorEnum)n);
  return 0;
}

MODULE_PRIVATE int PR_CALLBACK
cookie_WarningPrefChanged(const char * newpref, void * data) {
  PRBool x;
  nsresult rv;
  NS_WITH_SERVICE(nsIPref, prefs, "component://netscape/preferences", &rv);
  if (NS_FAILED(prefs->GetBoolPref(cookie_warningPref, &x))) {
    x = PR_FALSE;
  }
  cookie_SetWarningPref(x);
  return 0;
}

/* cookie utility functions */
PRIVATE void
image_SetBehaviorPref(COOKIE_BehaviorEnum x) {
  image_behavior = x;
}

PRIVATE void
image_SetWarningPref(PRBool x) {
  image_warning = x;
}

PRIVATE COOKIE_BehaviorEnum
image_GetBehaviorPref() {
  return image_behavior;
}

PRIVATE PRBool
image_GetWarningPref() {
  return image_warning;
}

MODULE_PRIVATE int PR_CALLBACK
image_BehaviorPrefChanged(const char * newpref, void * data) {
  PRInt32 n;
  nsresult rv;
  NS_WITH_SERVICE(nsIPref, prefs, "component://netscape/preferences", &rv);
  if (NS_FAILED(prefs->GetIntPref(image_behaviorPref, &n))) {
    image_SetBehaviorPref(COOKIE_Accept);
  } else {
    image_SetBehaviorPref((COOKIE_BehaviorEnum)n);
  }
  return 0;
}

MODULE_PRIVATE int PR_CALLBACK
image_WarningPrefChanged(const char * newpref, void * data) {
  PRBool x;
  nsresult rv;
  NS_WITH_SERVICE(nsIPref, prefs, "component://netscape/preferences", &rv);
  if (NS_FAILED(prefs->GetBoolPref(image_warningPref, &x))) {
    x = PR_FALSE;
  }
  image_SetWarningPref(x);
  return 0;
}

MODULE_PRIVATE int PR_CALLBACK
cookie_LifetimeOptPrefChanged(const char * newpref, void * data) {
  PRInt32 n;
  nsresult rv;
  NS_WITH_SERVICE(nsIPref, prefs, "component://netscape/preferences", &rv);
  if (NS_FAILED(prefs->GetIntPref(cookie_lifetimePref, &n))) {
    n = COOKIE_Normal;
  }
  cookie_SetLifetimePref((COOKIE_LifetimeEnum)n);
  return 0;
}

MODULE_PRIVATE int PR_CALLBACK
cookie_LifetimeLimitPrefChanged(const char * newpref, void * data) {
  PRInt32 n;
  nsresult rv;
  NS_WITH_SERVICE(nsIPref, prefs, "component://netscape/preferences", &rv);
  if (!NS_FAILED(prefs->GetIntPref(cookie_lifetimeValue, &n))) {
    cookie_SetLifetimeLimit(n);
  }
  return 0;
}

/*
 * search if permission already exists
 */
nsresult
permission_CheckFromList(char * hostname, PRBool &permission, PRInt32 type) {
  permission_HostStruct * hostStruct;
  permission_TypeStruct * typeStruct;

  /* ignore leading period in host name */
  while (hostname && (*hostname == '.')) {
    hostname++;
  }

  /* return if cookie_permission list does not exist */
  if (cookie_permissionList == nsnull) {
    return NS_ERROR_FAILURE;
  }

  /* find host name within list */
  PRInt32 count = cookie_permissionList->Count();
  for (PRInt32 i = 0; i < count; ++i) {
    hostStruct = NS_STATIC_CAST(permission_HostStruct*, cookie_permissionList->ElementAt(i));
    if (hostStruct) {
      if(hostname && hostStruct->host && !PL_strcasecmp(hostname, hostStruct->host)) {

        /* search for type in the permission list for this host */
        PRInt32 count2 = hostStruct->permissionList->Count();
        for (PRInt32 typeIndex=0; typeIndex<count2; typeIndex++) {
          typeStruct = NS_STATIC_CAST
            (permission_TypeStruct*, hostStruct->permissionList->ElementAt(typeIndex));
          if (typeStruct->type == type) {

            /* type found.  Obtain the corresponding permission */
            permission = typeStruct->permission;
            return NS_OK;
          }
        }

        /* type not found, return failure */
        return NS_ERROR_FAILURE;
      }
    }
  }

  /* not found, return failure */
  return NS_ERROR_FAILURE;
}

PRBool
permission_GetRememberChecked(PRInt32 type) {
  if (type == COOKIEPERMISSION) {
    return cookie_rememberChecked;
  } else if (type == IMAGEPERMISSION) {
    return image_rememberChecked;
  } else {
    return PR_FALSE;
  }
}

void
permission_SetRememberChecked(PRInt32 type, PRBool value) {
  if (type == COOKIEPERMISSION) {
    cookie_rememberChecked = value;
  } else if (type == IMAGEPERMISSION) {
    image_rememberChecked = value;
  }
}

nsresult
permission_Add(char * host, PRBool permission, PRInt32 type, PRBool save);

PRBool
permission_Check(
     nsIPrompt *aPrompter,
     char * hostname,
     PRInt32 type,
     PRBool warningPref,
     PRUnichar * message)
{
  PRBool permission;

  /* try to make decision based on saved permissions */
  if (NS_SUCCEEDED(permission_CheckFromList(hostname, permission, type))) {
    return permission;
  }

  /* see if we need to prompt */
  if(!warningPref) {
    return PR_TRUE;
  }

  /* we need to prompt */
  PRBool rememberChecked = permission_GetRememberChecked(type);
  PRUnichar * remember_string = cookie_Localize("RememberThisDecision");
  permission = cookie_CheckConfirmYN(aPrompter, message, remember_string, &rememberChecked);

  /* see if we need to remember this decision */
  if (rememberChecked) {
    char * hostname2 = NULL;
    /* ignore leading periods in host name */
    while (hostname && (*hostname == '.')) {
      hostname++;
    }
    StrAllocCopy(hostname2, hostname);
    permission_Add(hostname2, permission, type, PR_TRUE);
  }
  if (rememberChecked != permission_GetRememberChecked(type)) {
    permission_SetRememberChecked(type, rememberChecked);
    cookie_permissionsChanged = PR_TRUE;
    permission_Save();
  }
  return permission;
}

PRIVATE int
cookie_SameDomain(char * currentHost, char * firstHost);

PUBLIC nsresult
Image_CheckForPermission(char * hostname, char * firstHostname, PRBool &permission) {

  /* exit if imageblocker is not enabled */
  nsresult rv;
  PRBool prefvalue = PR_FALSE;
  NS_WITH_SERVICE(nsIPref, prefs, "component://netscape/preferences", &rv);
  if (NS_FAILED(rv) || 
      NS_FAILED(prefs->GetBoolPref("imageblocker.enabled", &prefvalue)) ||
      !prefvalue) {
    permission = PR_TRUE;
    return NS_OK;
  }

  /* try to make a decision based on pref settings */
  if ((image_GetBehaviorPref() == COOKIE_DontUse)) {
    permission = PR_FALSE;
    return NS_OK;
  }
  if (image_GetBehaviorPref() == COOKIE_DontAcceptForeign) {
    /* compare tails of names checking to see if they have a common domain */
    /* we do this by comparing the tails of both names where each tail includes at least one dot */
    PRInt32 dotcount = 0;
    char * tailHostname = hostname + PL_strlen(hostname) - 1;
    while (tailHostname > hostname) { 
      if (*tailHostname == '.') {
        dotcount++;
      }
      if (dotcount == 2) {
        tailHostname++;
        break;
      }
      tailHostname--;
    }
    dotcount = 0;
    char * tailFirstHostname = firstHostname + PL_strlen(firstHostname) - 1;
    while (tailFirstHostname > firstHostname) { 
      if (*tailFirstHostname == '.') {
        dotcount++;
      }
      if (dotcount == 2) {
        tailFirstHostname++;
        break;
      }
      tailFirstHostname--;
    }
    if (PL_strcmp(tailFirstHostname, tailHostname)) {
      permission = PR_FALSE;
      return NS_OK;
    }
  }

  /* use common routine to make decision */
  PRUnichar * message = cookie_Localize("PermissionToAcceptImage");
  PRUnichar * new_string = nsTextFormatter::smprintf(message, hostname ? hostname : "");
  permission = permission_Check(0, hostname, IMAGEPERMISSION,
                                image_GetWarningPref(), new_string);
  PR_FREEIF(new_string);
  Recycle(message);
  return NS_OK;
}

/* called from mkgeturl.c, NET_InitNetLib(). This sets the module local 
 * cookie pref variables and registers the callbacks
 */
PUBLIC void
COOKIE_RegisterCookiePrefCallbacks(void) {
  PRInt32 n;
  PRBool x;
  nsresult rv;
  NS_WITH_SERVICE(nsIPref, prefs, "component://netscape/preferences", &rv);

  // Initialize for cookie_behaviorPref
  if (NS_FAILED(prefs->GetIntPref(cookie_behaviorPref, &n))) {
    n = COOKIE_Accept;
  }
  cookie_SetBehaviorPref((COOKIE_BehaviorEnum)n);
  prefs->RegisterCallback(cookie_behaviorPref, cookie_BehaviorPrefChanged, NULL);

  // Initialize for cookie_warningPref
  if (NS_FAILED(prefs->GetBoolPref(cookie_warningPref, &x))) {
    x = PR_FALSE;
  }
  cookie_SetWarningPref(x);
  prefs->RegisterCallback(cookie_warningPref, cookie_WarningPrefChanged, NULL);

  if (NS_FAILED(prefs->GetIntPref(image_behaviorPref, &n))) {
    n = COOKIE_Accept;
  }
  image_SetBehaviorPref((COOKIE_BehaviorEnum)n);
  prefs->RegisterCallback(image_behaviorPref, image_BehaviorPrefChanged, NULL);

  if (NS_FAILED(prefs->GetBoolPref(image_warningPref, &x))) {
    x = PR_FALSE;
  }
  image_SetWarningPref(x);
  prefs->RegisterCallback(image_warningPref, image_WarningPrefChanged, NULL);

  // Initialize for cookie_lifetimePref
  if (NS_FAILED(prefs->GetIntPref(cookie_lifetimePref, &n))) {
    n = COOKIE_Normal;
  }
  cookie_SetLifetimePref((COOKIE_LifetimeEnum)n);
  prefs->RegisterCallback(cookie_lifetimePref, cookie_LifetimeOptPrefChanged, NULL);

  // Initialize for cookie_lifetimeValue
  if (!NS_FAILED(prefs->GetIntPref(cookie_lifetimeValue, &n))) {
    cookie_SetLifetimeLimit(n);
  }
  prefs->RegisterCallback(cookie_lifetimeValue, cookie_LifetimeLimitPrefChanged, NULL);
}

PRBool
cookie_IsInDomain(char* domain, char* host, int hostLength) {
  int domainLength = PL_strlen(domain);

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
  PRBool xxx = PR_FALSE;
  time_t cur_time = get_current_time();

  int host_length;

  /* return string to build */
  char * rv=0;

  /* disable cookies if the user's prefs say so */
  if(COOKIE_GetBehaviorPref() == COOKIE_DontUse) {
    return NULL;
  }
  if (!PL_strncasecmp(address, "https", 5)) {
     xxx = PR_TRUE;
  }

  /* search for all cookies */
  if (cookie_cookieList == nsnull) {
    return NULL;
  }
  char *host = cookie_ParseURL(address, GET_HOST_PART);
  char *path = cookie_ParseURL(address, GET_PATH_PART);
  for (PRInt32 i = cookie_cookieList->Count(); i > 0;) {
    i--;
    cookie_s = NS_STATIC_CAST(cookie_CookieStruct*, cookie_cookieList->ElementAt(i));
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

      /* if the cookie is xxx and the path isn't, dont send it */
      if (cookie_s->xxx & !xxx) {
        continue; /* back to top of while */
      }

      /* check for expired cookies */
      if( cookie_s->expires && (cookie_s->expires < cur_time) ) {
        /* expire and remove the cookie */
        cookie_cookieList->RemoveElementAt(i);
        deleteCookie((void*)cookie_s, nsnull);
        cookie_cookiesChanged = PR_TRUE;
        continue;
      }

      /* if we've already added a cookie to the return list, append a "; " so
       * subsequent cookies are delimited in the final list. */
      if (rv) StrAllocCat(rv, "; ");

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
      } else {
        StrAllocCat(rv, cookie_s->cookie);
      }
    }
  }

  PR_FREEIF(name);
  PR_FREEIF(path);
  PR_FREEIF(host);

  /* may be NULL */
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
  char * curHost = cookie_ParseURL(curURL, GET_HOST_PART);
  char * firstHost = cookie_ParseURL(firstURL, GET_HOST_PART);
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

/* returns PR_TRUE if authorization is required
** 
**
** IMPORTANT:  Now that this routine is multi-threaded it is up
**             to the caller to free any returned string
*/
PUBLIC char *
COOKIE_GetCookieFromHttp(char * address, char * firstAddress) {

  if ((COOKIE_GetBehaviorPref() == COOKIE_DontAcceptForeign) &&
      cookie_isForeign(address, firstAddress)) {

    /*
     * WARNING!!! This is a different behavior than 4.x.  In 4.x we used this pref to
     * control the setting of cookies only.  Here we are also blocking the getting of
     * cookies if the pref is set.  It may be that we need a separate pref to block the
     * getting of cookies.  But for now we are putting both under one pref since that
     * is cleaner.  If it turns out that this breaks some important websites, we may
     * have to resort to two prefs
     */

    return NULL;
  }
  return COOKIE_GetCookie(address);
}

nsresult
permission_Add(char * host, PRBool permission, PRInt32 type, PRBool save) {
  /* create permission list if it does not yet exist */
  if(!cookie_permissionList) {
    cookie_permissionList = new nsVoidArray();
    if(!cookie_permissionList) {
      Recycle(host);
      return NS_ERROR_FAILURE;
    }
  }

  /* find existing entry for host */
  permission_HostStruct * hostStruct;
  PRBool HostFound = PR_FALSE;
  PRInt32 count = cookie_permissionList->Count();
  PRInt32 i;
  for (i = 0; i < count; ++i) {
    hostStruct = NS_STATIC_CAST(permission_HostStruct*, cookie_permissionList->ElementAt(i));
    if (hostStruct) {
      if (PL_strcasecmp(host,hostStruct->host)==0) {

        /* host found in list */
        Recycle(host);
        HostFound = PR_TRUE;
        break;
#ifdef alphabetize
      } else if (PL_strcasecmp(host,hostStruct->host)<0) {

        /* need to insert new entry here */
        break;
#endif
      }
    }
  }

  if (!HostFound) {

    /* create a host structure for the host */
    hostStruct = PR_NEW(permission_HostStruct);
    if (!hostStruct) {
      Recycle(host);
      return NS_ERROR_FAILURE;
    }
    hostStruct->host = host;
    hostStruct->permissionList = new nsVoidArray();
    if(!hostStruct->permissionList) {
      PR_Free(hostStruct);
      Recycle(host);
      return NS_ERROR_FAILURE;
    }

    /* insert host structure into the list */
    if (i == cookie_permissionList->Count()) {
      cookie_permissionList->AppendElement(hostStruct);
    } else {
      cookie_permissionList->InsertElementAt(hostStruct, i);
    }
  }

  /* see if host already has an entry for this type */
  permission_TypeStruct * typeStruct;
  PRBool typeFound = PR_FALSE;
  PRInt32 count2 = hostStruct->permissionList->Count();
  for (PRInt32 typeIndex=0; typeIndex<count2; typeIndex++) {
    typeStruct = NS_STATIC_CAST
      (permission_TypeStruct*, hostStruct->permissionList->ElementAt(typeIndex));
    if (typeStruct->type == type) {

      /* type found.  Modify the corresponding permission */
      typeStruct->permission = permission;
      typeFound = PR_TRUE;
      break;
    }
  }

  /* create a type structure and attach it to the host structure */
  if (!typeFound) {
    typeStruct = PR_NEW(permission_TypeStruct);
    typeStruct->type = type;
    typeStruct->permission = permission;
    hostStruct->permissionList->AppendElement(typeStruct);
  }

  /* write the changes out to a file */
  if (save) {
    cookie_permissionsChanged = PR_TRUE;
    permission_Save();
  }
  return NS_OK;
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
  if (!cookie_cookieList || !host) return 0;

  for (PRInt32 i = cookie_cookieList->Count(); i > 0;) {
    i--;
    cookie = NS_STATIC_CAST(cookie_CookieStruct*, cookie_cookieList->ElementAt(i));
    NS_ASSERTION(cookie, "corrupt cookie list");
    if (cookie_IsFromHost(cookie, host)) count++;
  }
  return count;
}

PRIVATE void
cookie_SetCookieString(char * curURL, nsIPrompt *aPrompter, char * setCookieHeader, time_t timeToExpire );

/* Java script is calling COOKIE_SetCookieString, netlib is calling 
 * this via COOKIE_SetCookieStringFromHttp.
 */
PRIVATE void
cookie_SetCookieString(char * curURL, nsIPrompt *aPrompter, char * setCookieHeader, time_t timeToExpire) {
  cookie_CookieStruct * prev_cookie;
  char *path_from_header=NULL, *host_from_header=NULL;
  char *name_from_header=NULL, *cookie_from_header=NULL;
  char *cur_path = cookie_ParseURL(curURL, GET_PATH_PART);
  char *cur_host = cookie_ParseURL(curURL, GET_HOST_PART);
  char *semi_colon, *ptr, *equal;
  PRBool xxx=PR_FALSE, isDomain=PR_FALSE;
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
  if(COOKIE_GetBehaviorPref() == COOKIE_DontUse) {
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
    if ((ptr=PL_strcasestr(semi_colon, "secure="))) {
      char cPre=*(ptr-1), cPost=*(ptr+6);
      if (((cPre==' ') || (cPre==';')) && (!cPost || (cPost==' ') || (cPost==';'))) {
        xxx = PR_TRUE;
      } 
    }

    /* look for the path attribute */
    ptr = PL_strcasestr(semi_colon, "path=");
    if(ptr) {
      nsCString path(ptr+5);
      path.CompressWhitespace();
      StrAllocCopy(path_from_header, path.GetBuffer());
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
      nsCString domain(ptr+7);
      domain.CompressWhitespace();
      StrAllocCopy(domain_from_header, domain.GetBuffer());

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
      NS_WITH_SERVICE(nsIPref, prefs, "component://netscape/preferences", &rv);
      if (NS_FAILED(prefs->GetBoolPref(cookie_strictDomainsPref, &pref_scd))) {
        pref_scd = PR_FALSE;
      }
      if ( pref_scd == PR_TRUE ) {
        cur_host[cur_host_length-domain_length] = '\0';
        dot = PL_strchr(cur_host, '.');
        cur_host[cur_host_length-domain_length] = '.';
        if (dot) {
        // TRACEMSG(("host minus domain failed no-dot test."
//          " Domain: %s, Host: %s", domain_from_header, cur_host));
          PR_FREEIF(path_from_header);
          PR_Free(domain_from_header);
          PR_Free(cur_path);
          PR_Free(cur_host);
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
      if(timeToExpire == 0) {
        timeToExpire = cookie_ParseDate(date);
      }
      // TRACEMSG(("Have expires date: %ld", timeToExpire));
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
  if (equal)
      *equal = '\0';

  nsCString cookieHeader(setCookieHeader);
  cookieHeader.CompressWhitespace();
  if(equal) {
    StrAllocCopy(name_from_header, cookieHeader.GetBuffer());
    nsCString value(equal+1);
    value.CompressWhitespace();
    StrAllocCopy(cookie_from_header, value.GetBuffer());
  } else {
    StrAllocCopy(cookie_from_header, cookieHeader.GetBuffer());
    StrAllocCopy(name_from_header, "");
  }

  /* generate the message for the nag box */
  PRUnichar * new_string=0;
  int count = cookie_Count(host_from_header);
  prev_cookie = cookie_CheckForPrevCookie
    (path_from_header, host_from_header, name_from_header);
  PRUnichar * message;
  if (prev_cookie) {
    message = cookie_Localize("PermissionToModifyCookie");
    new_string = nsTextFormatter::smprintf(message, host_from_header ? host_from_header : "");
  } else if (count>1) {
    message = cookie_Localize("PermissionToSetAnotherCookie");
    new_string = nsTextFormatter::smprintf(message, host_from_header ? host_from_header : "", count);
  } else if (count==1){
    message = cookie_Localize("PermissionToSetSecondCookie");
    new_string = nsTextFormatter::smprintf(message, host_from_header ? host_from_header : "");
  } else {
    message = cookie_Localize("PermissionToSetACookie");
    new_string = nsTextFormatter::smprintf(message, host_from_header ? host_from_header : "");
  }
  Recycle(message);

  //TRACEMSG(("mkaccess.c: Setting cookie: %s for host: %s for path: %s",
  //          cookie_from_header, host_from_header, path_from_header));

  /* use common code to determine if we can set the cookie */
  PRBool permission = permission_Check(aPrompter, host_from_header, COOKIEPERMISSION,
// I believe this is the right place to eventually add the logic to ask
// about cookies that have excessive lifetimes, but it shouldn't be done
// until generalized per-site preferences are available.
                                       //cookie_GetLifetimeAsk(timeToExpire) ||
                                       cookie_GetWarningPref(),
                                       new_string);
  PR_FREEIF(new_string);
  if (!permission) {
    PR_FREEIF(path_from_header);
    PR_FREEIF(host_from_header);
    PR_FREEIF(name_from_header);
    PR_FREEIF(cookie_from_header);
    return;
  }

  /* limit the number of cookies from a specific host or domain */
  cookie_CheckForMaxCookiesFromHo(host_from_header);

  if (cookie_cookieList) {
    if(cookie_cookieList->Count() > MAX_NUMBER_OF_COOKIES-1) {
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
    prev_cookie->xxx = xxx;
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
    prev_cookie->xxx = xxx;
    prev_cookie->isDomain = isDomain;
    prev_cookie->lastAccessed = get_current_time();
    if(!cookie_cookieList) {
      cookie_cookieList = new nsVoidArray();
      if(!cookie_cookieList) {
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
    for (PRInt32 i = cookie_cookieList->Count(); i > 0;) {
      i--;
      tmp_cookie_ptr = NS_STATIC_CAST(cookie_CookieStruct*, cookie_cookieList->ElementAt(i));
      NS_ASSERTION(tmp_cookie_ptr, "corrupt cookie list");
      if(new_len <= PL_strlen(tmp_cookie_ptr->path)) {
        cookie_cookieList->InsertElementAt(prev_cookie, i);
        bCookieAdded = PR_TRUE;
        break;
      }
    }
    if ( !bCookieAdded ) {
      /* no shorter strings found in list */
      cookie_cookieList->AppendElement(prev_cookie);
    }
  }

  /* At this point we know a cookie has changed. Write the cookies to file. */
  cookie_cookiesChanged = PR_TRUE;
  cookie_Save();
  return;
}

PUBLIC void
COOKIE_SetCookieString(char * curURL, char * setCookieHeader) {
  cookie_SetCookieString(curURL, 0, setCookieHeader, 0);
}

/* This function wrapper wraps COOKIE_SetCookieString for the purposes of 
 * determining whether or not a cookie is inline (we need the URL struct, 
 * and outputFormat to do so).  this is called from NET_ParseMimeHeaders 
 * in mkhttp.c
 * This routine does not need to worry about the cookie lock since all of
 *   the work is handled by sub-routines
*/

PUBLIC void
COOKIE_SetCookieStringFromHttp(char * curURL, char * firstURL, nsIPrompt *aPrompter, char * setCookieHeader, char * server_date) {

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
  char *ptr=NULL;
  time_t gmtCookieExpires=0, expires=0, sDate;

  /* check for foreign cookie if pref says to reject such */
  if ((COOKIE_GetBehaviorPref() == COOKIE_DontAcceptForeign) &&
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

/* saves the HTTP cookies permissions to disk */
PRIVATE void
permission_Save() {
  permission_HostStruct * hostStruct;
  permission_TypeStruct * typeStruct;

  if (COOKIE_GetBehaviorPref() == COOKIE_DontUse) {
    return;
  }
  if (!cookie_permissionsChanged) {
    return;
  }
  if (cookie_permissionList == nsnull) {
    return;
  }
  nsFileSpec dirSpec;
  nsresult rval = cookie_ProfileDirectory(dirSpec);
  if (NS_FAILED(rval)) {
    return;
  }
  nsOutputFileStream strm(dirSpec + "cookperm.txt");
  if (!strm.is_open()) {
    return;
  }

  strm.write("# HTTP Cookie Permission File\n", 30); 
  strm.write("# http://www.netscape.com/newsref/std/cookie_spec.html\n", 55);
  strm.write("# This is a generated file!  Do not edit.\n\n", 43);

  /* format shall be:
   * host \t permission \t permission ... \n
   */

  
  PRInt32 count = cookie_permissionList->Count();
  for (PRInt32 i = 0; i < count; ++i) {
    hostStruct = NS_STATIC_CAST(permission_HostStruct*, cookie_permissionList->ElementAt(i));
    if (hostStruct) {
      strm.write(hostStruct->host, nsCRT::strlen(hostStruct->host));

      PRInt32 count2 = hostStruct->permissionList->Count();
      for (PRInt32 typeIndex=0; typeIndex<count2; typeIndex++) {
        typeStruct = NS_STATIC_CAST
          (permission_TypeStruct*, hostStruct->permissionList->ElementAt(typeIndex));
        strm.write("\t", 1);
        nsCAutoString tmp; tmp.AppendInt(typeStruct->type);
        strm.write(tmp.GetBuffer(), tmp.Length());
        if (typeStruct->permission) {
          strm.write("T", 1);
        } else {
          strm.write("F", 1);
        }
      }
      strm.write("\n", 1);
    }
  }

  /* save current state of nag boxs' checkmarks */
  strm.write("@@@@", 4);
  for (PRInt32 type = 0; type < NUMBER_OF_PERMISSIONS; type++) {
    strm.write("\t", 1);
    nsCAutoString tmp; tmp.AppendInt(type);
    strm.write(tmp.GetBuffer(), tmp.Length());
    if (permission_GetRememberChecked(type)) {
      strm.write("T", 1);
    } else {
      strm.write("F", 1);
    }
  }

  strm.write("\n", 1);

  cookie_permissionsChanged = PR_FALSE;
  strm.flush();
  strm.close();
}

/* reads the HTTP cookies permission from disk */
PRIVATE void
permission_Load() {
  nsAutoString buffer;
  nsFileSpec dirSpec;
  nsresult rv = cookie_ProfileDirectory(dirSpec);
  if (NS_FAILED(rv)) {
    return;
  }
  nsInputFileStream strm(dirSpec + "cookperm.txt");
  if (!strm.is_open()) {
    /* file doesn't exist -- that's not an error */
    for (PRInt32 type=0; type<NUMBER_OF_PERMISSIONS; type++) {
      permission_SetRememberChecked(type, PR_FALSE);
    }
    return;
  }

  /* format is:
   * host \t number permission \t number permission ... \n
   * if this format isn't respected we move onto the next line in the file.
   */
  while(cookie_GetLine(strm,buffer) != -1) {
    if ( !buffer.IsEmpty() ) {
      PRUnichar firstChar = buffer.CharAt(0);
      if (firstChar == '#' || firstChar == CR ||
          firstChar == LF || firstChar == 0) {
        continue;
      }
    }

    int hostIndex, permissionIndex;
    PRUint32 nextPermissionIndex = 0;
    hostIndex = 0;

    if ((permissionIndex=buffer.FindChar('\t', PR_FALSE, hostIndex)+1) == 0) {
      continue;      
    }

    /* ignore leading periods in host name */
    while (hostIndex < permissionIndex && (buffer.CharAt(hostIndex) == '.')) {
      hostIndex++;
    }

    nsString host;
    buffer.Mid(host, hostIndex, permissionIndex-hostIndex-1);

    nsString permissionString;
    for (;;) {
      if (nextPermissionIndex == buffer.Length()+1) {
        break;
      }
      if ((nextPermissionIndex=buffer.FindChar('\t', PR_FALSE, permissionIndex)+1) == 0) {
        nextPermissionIndex = buffer.Length()+1;
      }
      buffer.Mid(permissionString, permissionIndex, nextPermissionIndex-permissionIndex-1);
      permissionIndex = nextPermissionIndex;

      PRInt32 type = 0;
      PRUint32 index = 0;

      if (permissionString.IsEmpty()) {
        continue; /* empty permission entry -- should never happen */
      }
      char c = (char)permissionString.CharAt(index);
      while (index < permissionString.Length() && c >= '0' && c <= '9') {
        type = 10*type + (c-'0');
        c = (char)permissionString.CharAt(++index);
      }
      if (index >= permissionString.Length()) {
        continue; /* bad format for this permission entry */
      }
      PRBool permission = (permissionString.CharAt(index) == 'T');

      /*
       * a host value of "@@@@" is a special code designating the
       * state of the cookie nag-box's checkmark
       */
      if (host.EqualsWithConversion("@@@@")) {
        if (!permissionString.IsEmpty()) {
          permission_SetRememberChecked(type, permission);
        }
      } else {
        if (!permissionString.IsEmpty()) {
          rv = permission_Add(host.ToNewCString(), permission, type, PR_FALSE);
          if (NS_FAILED(rv)) {
            strm.close();
            return;
          }
        }
      }
    }
  }

  strm.close();
  cookie_permissionsChanged = PR_FALSE;
  return;
}

/* saves out the HTTP cookies to disk */
PRIVATE void
cookie_Save() {
  cookie_CookieStruct * cookie_s;
  time_t cur_date = get_current_time();
  char date_string[36];
  if (COOKIE_GetBehaviorPref() == COOKIE_DontUse) {
    return;
  }
  if (!cookie_cookiesChanged) {
    return;
  }
  if (cookie_cookieList == nsnull) {
    return;
  }
  nsFileSpec dirSpec;
  nsresult rv = cookie_ProfileDirectory(dirSpec);
  if (NS_FAILED(rv)) {
    return;
  }
  nsOutputFileStream strm(dirSpec + "cookies.txt");
  if (!strm.is_open()) {
    /* file doesn't exist -- that's not an error */
    return;
  }

  strm.write("# HTTP Cookie File\n", 19);
  strm.write("# http://www.netscape.com/newsref/std/cookie_spec.html\n", 55);
  strm.write("# This is a generated file!  Do not edit.\n", 42);
  strm.write("# To delete cookies, use the Cookie Manager.\n\n", 46);

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

      if (cookie_s->xxx) {
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

  cookie_cookiesChanged = PR_FALSE;
  strm.flush();
  strm.close();
}

/* reads HTTP cookies from disk */
PRIVATE void
cookie_Load() {
  cookie_CookieStruct *new_cookie, *tmp_cookie_ptr;
  size_t new_len;
  nsAutoString buffer;
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

  /* format is:
   *
   * host \t isDomain \t path \t xxx \t expires \t name \t cookie
   *
   * if this format isn't respected we move onto the next line in the file.
   * isDomain is PR_TRUE or PR_FALSE -- defaulting to PR_FALSE
   * xxx is PR_TRUE or PR_FALSE   -- should default to PR_TRUE
   * expires is a time_t integer
   * cookie can have tabs
   */
  while (cookie_GetLine(strm,buffer) != -1){
    added_to_list = PR_FALSE;

    if ( !buffer.IsEmpty() ) {
      PRUnichar firstChar = buffer.CharAt(0);
      if (firstChar == '#' || firstChar == CR ||
          firstChar == LF || firstChar == 0) {
        continue;
      }
    }
    int hostIndex, isDomainIndex, pathIndex, xxxIndex, expiresIndex, nameIndex, cookieIndex;
    hostIndex = 0;
    if ((isDomainIndex=buffer.FindChar('\t', PR_FALSE,hostIndex)+1) == 0 ||
        (pathIndex=buffer.FindChar('\t', PR_FALSE,isDomainIndex)+1) == 0 ||
        (xxxIndex=buffer.FindChar('\t', PR_FALSE,pathIndex)+1) == 0 ||
        (expiresIndex=buffer.FindChar('\t', PR_FALSE,xxxIndex)+1) == 0 ||
        (nameIndex=buffer.FindChar('\t', PR_FALSE,expiresIndex)+1) == 0 ||
        (cookieIndex=buffer.FindChar('\t', PR_FALSE,nameIndex)+1) == 0 ) {
      continue;
    }
    nsAutoString host, isDomain, path, xxx, expires, name, cookie;
    buffer.Mid(host, hostIndex, isDomainIndex-hostIndex-1);
    buffer.Mid(isDomain, isDomainIndex, pathIndex-isDomainIndex-1);
    buffer.Mid(path, pathIndex, xxxIndex-pathIndex-1);
    buffer.Mid(xxx, xxxIndex, expiresIndex-xxxIndex-1);
    buffer.Mid(expires, expiresIndex, nameIndex-expiresIndex-1);
    buffer.Mid(name, nameIndex, cookieIndex-nameIndex-1);
    buffer.Mid(cookie, cookieIndex, buffer.Length()-cookieIndex);

    /* create a new cookie_struct and fill it in */
    new_cookie = PR_NEW(cookie_CookieStruct);
    if (!new_cookie) {
      strm.close();
      return;
    }
    memset(new_cookie, 0, sizeof(cookie_CookieStruct));
    new_cookie->name = name.ToNewCString();
    new_cookie->cookie = cookie.ToNewCString();
    new_cookie->host = host.ToNewCString();
    new_cookie->path = path.ToNewCString();
    if (isDomain.EqualsWithConversion("TRUE")) {
      new_cookie->isDomain = PR_TRUE;
    } else {
      new_cookie->isDomain = PR_FALSE;
    }
    if (xxx.EqualsWithConversion("TRUE")) {
      new_cookie->xxx = PR_TRUE;
    } else {
      new_cookie->xxx = PR_FALSE;
    }
    char * expiresCString = expires.ToNewCString();
    new_cookie->expires = strtoul(expiresCString, nsnull, 10);
    nsCRT::free(expiresCString);

    /* start new cookie list if one does not already exist */
    if (!cookie_cookieList) {
      cookie_cookieList = new nsVoidArray();
      if (!cookie_cookieList) {
        strm.close();
        return;
      }
    }

    /* add new cookie to the list so that it is before any strings of smaller length */
    new_len = PL_strlen(new_cookie->path);
    for (PRInt32 i = cookie_cookieList->Count(); i > 0;) {
      i--;
      tmp_cookie_ptr = NS_STATIC_CAST(cookie_CookieStruct*, cookie_cookieList->ElementAt(i));
      NS_ASSERTION(tmp_cookie_ptr, "corrupt cookie list");
      if (new_len <= PL_strlen(tmp_cookie_ptr->path)) {
        cookie_cookieList->InsertElementAt(new_cookie, i);
        added_to_list = PR_TRUE;
        break;
      }
    }

    /* no shorter strings found in list so add new cookie at end */
    if (!added_to_list) {
      cookie_cookieList->AppendElement(new_cookie);
    }
  }

  strm.close();
  cookie_cookiesChanged = PR_FALSE;
  return;
}


PUBLIC int
COOKIE_ReadCookies()
{
  static PRBool sReadCookies = PR_FALSE;
  
  if (sReadCookies)
    NS_WARNING("We are reading the cookies more than once. Probably bad");
    
  cookie_Load();
  permission_Load();
  sReadCookies = PR_TRUE;
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
 * find the next cookie that is alphabetically after the specified cookie
 *    ordering is based on hostname and the cookie name
 *    if specified cookie is NULL, find the first cookie alphabetically
 */
PRIVATE cookie_CookieStruct *
NextCookieAfter(cookie_CookieStruct * cookie, int * cookieNum) {
  cookie_CookieStruct *cookie_ptr;
  cookie_CookieStruct *lowestCookie = NULL;

  if (!cookie_cookieList) return NULL;

  for (PRInt32 i = cookie_cookieList->Count(); i > 0;) {
    i--;
    cookie_ptr = NS_STATIC_CAST(cookie_CookieStruct*, cookie_cookieList->ElementAt(i));
    NS_ASSERTION(cookie_ptr, "corrupt cookie list");
    if (!cookie || (CookieCompare(cookie_ptr, cookie) > 0)) {
      if (!lowestCookie || (CookieCompare(cookie_ptr, lowestCookie) < 0)) {
        lowestCookie = cookie_ptr;
        *cookieNum = i;
      }
    }
  }

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
  NS_ASSERTION(start >= 0, "bad data");
  if (start < 0) {
    return nsCAutoString().ToNewCString();
  }
  start += PL_strlen(name); /* get passed the |name| part */
  length = results.FindChar('|', PR_FALSE,start) - start;
  results.Mid(value, start, length);
  return value.ToNewCString();
}

PUBLIC void
COOKIE_CookieViewerReturn(nsAutoString results) {
  cookie_CookieStruct * cookie;
  PRInt32 count = 0;

  /* step through all cookies and delete those that are in the sequence */
  char * gone = cookie_FindValueInArgs(results, "|goneC|");
  char * block = cookie_FindValueInArgs(results, "|block|");
  if (cookie_cookieList) {
    count = cookie_cookieList->Count();
    while (count>0) {
      count--;
      cookie = NS_STATIC_CAST(cookie_CookieStruct*, cookie_cookieList->ElementAt(count));
      NS_ASSERTION(cookie, "corrupt cookie list");
      if (cookie_InSequence(gone, count)) {
        if (PL_strlen(block) && block[0]=='t') {
          char * hostname = nsnull;
          StrAllocCopy(hostname, cookie->host);
          permission_Add(hostname, PR_FALSE, COOKIEPERMISSION, PR_TRUE);
        }
        cookie_cookieList->RemoveElementAt(count);
        deleteCookie((void*)cookie, nsnull);
        cookie_cookiesChanged = PR_TRUE;
      }
    }
  }
  cookie_Save();
  nsCRT::free(gone);
  nsCRT::free(block);

  /* step through all permissions and delete those that are in the sequence */
  for (PRInt32 type = 0; type < NUMBER_OF_PERMISSIONS; type++) {
    switch (type) {
      case COOKIEPERMISSION:
        gone = cookie_FindValueInArgs(results, "|goneP|");
        break;
      case IMAGEPERMISSION:
        gone = cookie_FindValueInArgs(results, "|goneI|");
        break;
    }
    if (cookie_permissionList) {
      count = cookie_permissionList->Count();
      while (count>0) {
        count--;
        if (cookie_InSequence(gone, count)) {
          permission_Free(count, type, PR_FALSE);
          cookie_permissionsChanged = PR_TRUE;
        }
      }
    }
    permission_Save();
    nsCRT::free(gone);
  }
}

#define BUFLEN2 5000
#define BREAK '\001'

PUBLIC void
COOKIE_GetCookieListForViewer(nsString& aCookieList) {
  PRUnichar *buffer = (PRUnichar*)PR_Malloc(2*BUFLEN2);
  int g, cookieNum;
  cookie_CookieStruct * cookie;


  /* Get rid of any expired cookies now so user doesn't
   * think/see that we're keeping cookies in memory.
   */
  cookie_RemoveExpiredCookies();

  /* generate the list of cookies in alphabetical order */
  cookie = NULL;
  while ((cookie = NextCookieAfter(cookie, &cookieNum))) {
    char *fixed_name = cookie_FixQuoted(cookie->name);
    char *fixed_value = cookie_FixQuoted(cookie->cookie);
    char *fixed_domain_or_host = cookie_FixQuoted(cookie->host);
    char *fixed_path = cookie_FixQuoted(cookie->path);
    PRUnichar * Domain = cookie_Localize("Domain");
    PRUnichar * Host = cookie_Localize("Host");
    PRUnichar * Yes = cookie_Localize("Yes");
    PRUnichar * No = cookie_Localize("No");
    PRUnichar * AtEnd = cookie_Localize("AtEndOfSession");
    buffer[0] = '\0';
    g = 0;

    /*
     * Cookie expiration times on mac will not be decoded correctly because
     * they were based on get_current_time() instead of time(NULL) -- see comments in
     * get_current_time.  So we need to adjust for that now in order for the
     * display of the expiration time to be correct
     */
    time_t expires;
    expires = cookie->expires + (time(NULL) - get_current_time());

    nsString temp1; temp1.AssignWithConversion("%c%d%c%s%c%s%c%S%c%s%c%s%c%S%c%S");
    nsString temp2; temp2.AssignWithConversion(ctime(&(expires)));
    g += nsTextFormatter::snprintf(buffer+g, BUFLEN2-g,
      temp1.GetUnicode(),
      BREAK, cookieNum,
      BREAK, fixed_name,
      BREAK, fixed_value,
      BREAK, cookie->isDomain ? Domain : Host,
      BREAK, fixed_domain_or_host,
      BREAK, fixed_path,
      BREAK, cookie->xxx ? Yes : No,
      BREAK, cookie->expires ? (temp2.GetUnicode()) : AtEnd
    );
    PR_FREEIF(fixed_name);
    PR_FREEIF(fixed_value);
    PR_FREEIF(fixed_domain_or_host);
    PR_FREEIF(fixed_path);
    Recycle(Domain);
    Recycle(Host);
    Recycle(Yes);
    Recycle(No);
    Recycle(AtEnd);
    aCookieList += buffer;
  }
  PR_FREEIF(buffer);
}

PUBLIC void
COOKIE_GetPermissionListForViewer(nsString& aPermissionList, PRInt32 type) {
  char *buffer = (char*)PR_Malloc(BUFLEN2);
  int g = 0, permissionNum;
  permission_HostStruct * hostStruct;
  permission_TypeStruct * typeStruct;

  buffer[0] = '\0';
  permissionNum = 0;

  if (cookie_permissionList == nsnull) {
    PR_FREEIF(buffer);
    return;
  }

  PRInt32 count = cookie_permissionList->Count();
  for (PRInt32 i = 0; i < count; ++i) {
    hostStruct = NS_STATIC_CAST(permission_HostStruct*, cookie_permissionList->ElementAt(i));
    if (hostStruct) {
      PRInt32 count2 = hostStruct->permissionList->Count();
      for (PRInt32 typeIndex=0; typeIndex<count2; typeIndex++) {
        typeStruct = NS_STATIC_CAST
          (permission_TypeStruct*, hostStruct->permissionList->ElementAt(typeIndex));
        if (typeStruct->type == type) {
          g += PR_snprintf(buffer+g, BUFLEN2-g,
            "%c%d%c%c%s",
            BREAK,
            permissionNum,
            BREAK,
            (typeStruct->permission) ? '+' : '-',
            hostStruct->host
          );
          permissionNum++;
        }
      }
    }
  }
  aPermissionList.AssignWithConversion(buffer);
  PR_FREEIF(buffer);
}

PUBLIC void
Image_Block(nsString imageURL) {
  if (imageURL.Length() == 0) {
    return;
  }
  char * imageURLCString = imageURL.ToNewCString();
  char *host = cookie_ParseURL(imageURLCString, GET_HOST_PART);
  Recycle(imageURLCString);
  if (PL_strlen(host) != 0) {
    permission_Add(host, PR_FALSE, IMAGEPERMISSION, PR_TRUE);
  }
}

PUBLIC void
Permission_Add(nsString imageURL, PRBool permission, PRInt32 type) {
  if (imageURL.Length() == 0) {
    return;
  }
  char * imageURLCString = imageURL.ToNewCString();
  char *host = cookie_ParseURL(imageURLCString, GET_HOST_PART);
  Recycle(imageURLCString);
  if (PL_strlen(host) != 0) {
    permission_Add(host, permission, type, PR_TRUE);
  }
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
      } else {
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
