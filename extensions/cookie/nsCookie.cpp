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
/*
 *
 * Designed and Implemented by Lou Montulli '94
 * Heavily modified by Judson Valeski '97
 * Yada yada yada... Gagan Saksena '98
 *
 * This file implements HTTP access authorization
 * and HTTP cookies
 */

// #define newI18N 1

#define alphabetize 1
#include "nsString.h"
#include "nsIServiceManager.h"
#include "nsFileStream.h"
#include "nsIFileLocator.h"
#include "nsIFileSpec.h"
#include "nsFileLocations.h"
#include "nsINetSupportDialogService.h"

extern "C" {
#include "xp.h"
#include "prefapi.h"
#include "prmon.h"
}
extern "C" {
#ifdef XP_MAC
#include "prpriv.h"             /* for NewNamedMonitor */
#else
#include "private/prpriv.h"
#endif
}

extern "C" {
#include "sechash.h"
#include "rosetta.h"
}

extern "C" {
#include "xpgetstr.h" /* for XP_GetString() */
extern int MK_ACCESS_COOKIES_WISHES0;
extern int MK_ACCESS_COOKIES_WISHES1;
extern int MK_ACCESS_COOKIES_WISHESN;
extern int MK_ACCESS_COOKIES_WISHES_MODIFY;
extern int MK_ACCESS_COOKIES_REMEMBER;
}
/*

static NS_DEFINE_IID(kIStringBundleServiceIID, NS_ISTRINGBUNDLESERVICE_IID);
static NS_DEFINE_IID(kStringBundleServiceCID, NS_STRINGBUNDLESERVICE_CID);*/
static NS_DEFINE_IID(kIFileLocatorIID, NS_IFILELOCATOR_IID);
static NS_DEFINE_CID(kFileLocatorCID, NS_FILELOCATOR_CID);
static NS_DEFINE_CID(kNetSupportDialogCID, NS_NETSUPPORTDIALOG_CID);

#define MAX_NUMBER_OF_COOKIES  300
#define MAX_COOKIES_PER_SERVER 20
#define MAX_BYTES_PER_COOKIE   4096  /* must be at least 1 */

#define pref_cookieBehavior         "network.cookie.cookieBehavior"
#define pref_warnAboutCookies       "network.cookie.warnAboutCookies"
#define pref_scriptName             "network.cookie.filterName"
#define pref_strictCookieDomains    "network.cookie.strictDomains"

#define DEF_COOKIE_BEHAVIOR     0

#define NET_IS_SPACE(x) ((((unsigned int) (x)) > 0x7f) ? 0 : isspace(x))

#ifndef MAXHOSTNAMELEN
#define MAXHOSTNAMELEN 64
#endif

MODULE_PRIVATE time_t 
NET_ParseDate(char *date_string);

PRBool
Cookie_CheckConfirm(char * szMessage, char * szCheckMessage, PRBool* checkValue);

PRIVATE Bool cookies_changed = FALSE;
PRIVATE Bool cookie_permissions_changed = FALSE;
PRIVATE Bool cookie_remember_checked = FALSE;

PRIVATE NET_CookieBehaviorEnum net_CookieBehavior = NET_Accept;
PRIVATE Bool net_WarnAboutCookies = FALSE;


/*--------------------------------------------------
 * The following routines support the 
 * Set-Cookie: / Cookie: headers
 */

PRIVATE XP_List * net_cookie_list=0;
PRIVATE XP_List * net_cookie_permission_list=0;
PRIVATE XP_List * net_defer_cookie_list=0;

typedef struct _net_CookieStruct {
    char * path;
	char * host;
	char * name;
    char * cookie;
	time_t expires;
	time_t last_accessed;
	Bool xxx;
	Bool   is_domain;   /* is it a domain instead of an absolute host? */
} net_CookieStruct;

typedef struct _net_CookiePermissionStruct {
    char * host;
    Bool permission;
} net_CookiePermissionStruct;

typedef struct _net_DeferCookieStruct {
    char * cur_url;
    char * set_cookie_header;
    time_t timeToExpire;
} net_DeferCookieStruct;

/* Routines and data to protect the cookie list so it
**   can be accessed by mulitple threads
*/

#include "prthread.h"
#include "prmon.h"

static PRMonitor * cookie_lock_monitor = NULL;
static PRThread  * cookie_lock_owner = NULL;
static int cookie_lock_count = 0;

#define TEST_URL "resource:/res/cookie.properties"

PRIVATE char*
cookie_Localize(char* genericString) {
#ifdef newI18NFinished
  nsresult ret;
  nsAutoString v("");
  /* create a URL for the string resource file *//*
  nsINetService* pNetService = nsnull;
  ret = nsServiceManager::GetService(kNetServiceCID, kINetServiceIID,
    (nsISupports**) &pNetService);
  if (NS_FAILED(ret)) {
    printf("cannot get net service\n");
    return v.ToNewCString();
  }
  nsIURI *url = nsnull;
  ret = pNetService->CreateURL(&url, nsString(TEST_URL), nsnull, nsnull,
    nsnull);
  if (NS_FAILED(ret)) {
    printf("cannot create URL\n");
    nsServiceManager::ReleaseService(kNetServiceCID, pNetService);
    return v.ToNewCString();
  }
  nsServiceManager::ReleaseService(kNetServiceCID, pNetService);*/

  /* create a bundle for the localization */ /*
  nsIStringBundleService* pStringService = nsnull;
  ret = nsServiceManager::GetService(kStringBundleServiceCID,
    kIStringBundleServiceIID, (nsISupports**) &pStringService);
  if (NS_FAILED(ret)) {
    printf("cannot get string service\n");
    return v.ToNewCString();
  }
  nsILocale* locale = nsnull;
  nsIStringBundle* bundle = nsnull;
  ret = pStringService->CreateBundle(url, locale, &bundle);
  if (NS_FAILED(ret)) {
    printf("cannot create instance\n");
    nsServiceManager::ReleaseService(kStringBundleServiceCID, pStringService);
    return v.ToNewCString();
  }
  nsServiceManager::ReleaseService(kStringBundleServiceCID, pStringService);*/

  /* localize the given string *//*
  ret = bundle->GetStringFromName(nsString(genericString), v);
  PR_Free(bundle);
  if (NS_FAILED(ret)) {
    printf("cannot get string from name\n");
    return v.ToNewCString();
  }*/
//  return v.ToNewCString();
#else
  return PL_strdup(genericString);
#endif /* newI18NFinished */
}

PRIVATE nsresult cookie_ProfileDirectory(nsFileSpec& dirSpec) {
  nsIFileSpec* spec = NS_LocateFileOrDirectory(
  							nsSpecialFileSpec::App_UserProfileDirectory50);
  if (!spec)
  	return NS_ERROR_FAILURE;
  return spec->GetFileSpec(&dirSpec);
}

#ifndef XP_MAC
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
#endif

PRIVATE void
net_lock_cookie_list(void)
{
    if(!cookie_lock_monitor)
	cookie_lock_monitor = PR_NewNamedMonitor("cookie-lock");

    PR_EnterMonitor(cookie_lock_monitor);

    while(TRUE) {

	/* no current owner or owned by this thread */
	PRThread * t = PR_CurrentThread();
	if(cookie_lock_owner == NULL || cookie_lock_owner == t) {
	    cookie_lock_owner = t;
	    cookie_lock_count++;

	    PR_ExitMonitor(cookie_lock_monitor);
	    return;
	}

	/* owned by someone else -- wait till we can get it */
	PR_Wait(cookie_lock_monitor, PR_INTERVAL_NO_TIMEOUT);

    }
}

PRIVATE void
net_unlock_cookie_list(void)
{
   PR_EnterMonitor(cookie_lock_monitor);

#ifdef DEBUG
    /* make sure someone doesn't try to free a lock they don't own */
    PR_ASSERT(cookie_lock_owner == PR_CurrentThread());
#endif

    cookie_lock_count--;

    if(cookie_lock_count == 0) {
	cookie_lock_owner = NULL;
	PR_Notify(cookie_lock_monitor);
    }
    PR_ExitMonitor(cookie_lock_monitor);

}

static PRMonitor * defer_cookie_lock_monitor = NULL;
static PRThread  * defer_cookie_lock_owner = NULL;
static int defer_cookie_lock_count = 0;

PRIVATE void
net_lock_defer_cookie_list(void)
{
    if(!defer_cookie_lock_monitor)
	defer_cookie_lock_monitor =
            PR_NewNamedMonitor("defer_cookie-lock");

    PR_EnterMonitor(defer_cookie_lock_monitor);

    while(TRUE) {

	/* no current owner or owned by this thread */
	PRThread * t = PR_CurrentThread();
	if(defer_cookie_lock_owner == NULL || defer_cookie_lock_owner == t) {
	    defer_cookie_lock_owner = t;
	    defer_cookie_lock_count++;

	    PR_ExitMonitor(defer_cookie_lock_monitor);
	    return;
	}

	/* owned by someone else -- wait till we can get it */
	PR_Wait(defer_cookie_lock_monitor, PR_INTERVAL_NO_TIMEOUT);

    }
}

PRIVATE void
net_unlock_defer_cookie_list(void)
{
   PR_EnterMonitor(defer_cookie_lock_monitor);

#ifdef DEBUG
    /* make sure someone doesn't try to free a lock they don't own */
    PR_ASSERT(defer_cookie_lock_owner == PR_CurrentThread());
#endif

    defer_cookie_lock_count--;

    if(defer_cookie_lock_count == 0) {
	defer_cookie_lock_owner = NULL;
	PR_Notify(defer_cookie_lock_monitor);
    }
    PR_ExitMonitor(defer_cookie_lock_monitor);

}

static PRMonitor * cookie_permission_lock_monitor = NULL;
static PRThread  * cookie_permission_lock_owner = NULL;
static int cookie_permission_lock_count = 0;

PRIVATE void
net_lock_cookie_permission_list(void)
{
    if(!cookie_permission_lock_monitor)
	cookie_permission_lock_monitor =
            PR_NewNamedMonitor("cookie_permission-lock");

    PR_EnterMonitor(cookie_permission_lock_monitor);

    while(TRUE) {

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
net_unlock_cookie_permission_list(void)
{
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
net_SaveCookiePermissions();
PRIVATE void
net_SaveCookies();

PRIVATE void
net_FreeCookiePermission
    (net_CookiePermissionStruct * cookie_permission, Bool save)
{

    /*
     * This routine should only be called while holding the
     * cookie permission list lock
     */

    if(!cookie_permission) {
	return;
    }
    XP_ListRemoveObject(net_cookie_permission_list, cookie_permission);
    PR_FREEIF(cookie_permission->host);

    PR_Free(cookie_permission);
    if (save) {
	cookie_permissions_changed = TRUE;
	net_SaveCookiePermissions();
    }
}

/* blows away all cookie permissions currently in the list */
PRIVATE void
net_RemoveAllCookiePermissions()
{
    net_CookiePermissionStruct * victim;
    XP_List * cookiePermissionList;

    /* check for NULL or empty list */
    net_lock_cookie_permission_list();
    cookiePermissionList = net_cookie_permission_list;

    if(XP_ListIsEmpty(cookiePermissionList)) {
	net_unlock_cookie_permission_list();
	return;
    }
    while((victim =
	    (net_CookiePermissionStruct *)
	    XP_ListNextObject(cookiePermissionList)) != 0) {
	net_FreeCookiePermission(victim, FALSE);
	cookiePermissionList = net_cookie_permission_list;
    }
    XP_ListDestroy(net_cookie_permission_list);
    net_cookie_permission_list = NULL;
    net_unlock_cookie_permission_list();
}

/* This should only get called while holding the cookie-lock
**
*/
PRIVATE void
net_FreeCookie(net_CookieStruct * cookie)
{

	if(!cookie)
		return;
	XP_ListRemoveObject(net_cookie_list, cookie);

	PR_FREEIF(cookie->path);
	PR_FREEIF(cookie->host);
	PR_FREEIF(cookie->name);
	PR_FREEIF(cookie->cookie);

	PR_Free(cookie);

	cookies_changed = TRUE;
}


PUBLIC void
NET_DeleteCookie(char* cookieURL)
{
    net_CookieStruct * cookie;
    XP_List * list_ptr;
    char * cookieURL2 = 0;

    net_lock_cookie_list();
    list_ptr = net_cookie_list;
    while((cookie = (net_CookieStruct *) XP_ListNextObject(list_ptr))!=0) {
        StrAllocCopy(cookieURL2, "cookie:");
	StrAllocCat(cookieURL2, cookie->host);
        StrAllocCat(cookieURL2, "!");
	StrAllocCat(cookieURL2, cookie->path);
        StrAllocCat(cookieURL2, "!");
	StrAllocCat(cookieURL2, cookie->name);
	if (PL_strcmp(cookieURL, cookieURL2)==0) {
	    net_FreeCookie(cookie);
	    break;
	}
    }
    PR_FREEIF(cookieURL2);
    net_unlock_cookie_list();
}


/* blows away all cookies currently in the list, then blows away the list itself
 * nulling it after it's free'd
 */
PRIVATE void
net_RemoveAllCookies()
{
	net_CookieStruct * victim;
	XP_List * cookieList;

	/* check for NULL or empty list */
	net_lock_cookie_list();
	cookieList = net_cookie_list;
	if(XP_ListIsEmpty(cookieList)) {
		net_unlock_cookie_list();
		return;
	}

    while((victim = (net_CookieStruct *) XP_ListNextObject(cookieList)) != 0) {
		net_FreeCookie(victim);
		cookieList=net_cookie_list;
	}
	XP_ListDestroy(net_cookie_list);
	net_cookie_list = NULL;
	net_unlock_cookie_list();
}

PUBLIC void
NET_RemoveAllCookies()
{

	net_RemoveAllCookiePermissions();
	net_RemoveAllCookies();
}

PRIVATE void
net_remove_oldest_cookie(void)
{
    XP_List * list_ptr;
    net_CookieStruct * cookie_s;
    net_CookieStruct * oldest_cookie;

	net_lock_cookie_list();
	list_ptr = net_cookie_list;

	if(XP_ListIsEmpty(list_ptr)) {
		net_unlock_cookie_list();
		return;
	}

	oldest_cookie = (net_CookieStruct*) list_ptr->next->object;
	
    while((cookie_s = (net_CookieStruct *) XP_ListNextObject(list_ptr))!=0)
      {
		if(cookie_s->last_accessed < oldest_cookie->last_accessed)
			oldest_cookie = cookie_s;
	  }

	if(oldest_cookie)
	  {
		// TRACEMSG(("Freeing cookie because global max cookies has been exceeded"));
	    net_FreeCookie(oldest_cookie);
	  }
	net_unlock_cookie_list();
}

/* Remove any expired cookies from memory 
** This routine should only be called while holding the cookie list lock
*/
PRIVATE void
net_remove_expired_cookies(void)
{
    XP_List * list_ptr = net_cookie_list;
    net_CookieStruct * cookie_s;
	time_t cur_time = time(NULL);

	if(XP_ListIsEmpty(list_ptr))
		return;

    while((cookie_s = (net_CookieStruct *) XP_ListNextObject(list_ptr))!=0)
      {
		/* Don't get rid of expire time 0 because these need to last for 
		 * the entire session. They'll get cleared on exit.
		 */
		if( cookie_s->expires && (cookie_s->expires < cur_time) ) {
			net_FreeCookie(cookie_s);
			/* Reset the list_ptr to the beginning of the list.
			 * Do this because list_ptr's object was just freed
			 * by the call to net_FreeCookie struct, even
			 * though it's inefficient.
			 */
			list_ptr = net_cookie_list;
		}
	  }
}

/* checks to see if the maximum number of cookies per host
 * is being exceeded and deletes the oldest one in that
 * case
 * This routine should only be called while holding the cookie lock
 */
PRIVATE void
net_CheckForMaxCookiesFromHost(const char * cur_host)
{
    XP_List * list_ptr = net_cookie_list;
    net_CookieStruct * cookie_s;
    net_CookieStruct * oldest_cookie = 0;
	int cookie_count = 0;

    if(XP_ListIsEmpty(list_ptr))
        return;

    while((cookie_s = (net_CookieStruct *) XP_ListNextObject(list_ptr))!=0)
      {
	    if(!PL_strcasecmp(cookie_s->host, cur_host))
		  {
			cookie_count++;
			if(!oldest_cookie 
				|| oldest_cookie->last_accessed > cookie_s->last_accessed)
                oldest_cookie = cookie_s;
		  }
      }

	if(cookie_count >= MAX_COOKIES_PER_SERVER && oldest_cookie)
	  {
	//	TRACEMSG(("Freeing cookie because max cookies per server has been exceeded"));
        net_FreeCookie(oldest_cookie);
	  }
}


/* search for previous exact match
** This routine should only be called while holding the cookie lock
*/
PRIVATE net_CookieStruct *
net_CheckForPrevCookie(char * path,
                   char * hostname,
                   char * name)
{

    XP_List * list_ptr = net_cookie_list;
    net_CookieStruct * cookie_s;

    while((cookie_s = (net_CookieStruct *) XP_ListNextObject(list_ptr))!=0) {
        if(path && hostname && cookie_s->path && cookie_s->host && cookie_s->name
                && !PL_strcmp(name, cookie_s->name)
                && !PL_strcmp(path, cookie_s->path)
                && !PL_strcasecmp(hostname, cookie_s->host))
            return(cookie_s);
    }
    return(NULL);
}

/* cookie utility functions */
PRIVATE void
NET_SetCookieBehaviorPref(NET_CookieBehaviorEnum x)
{
	net_CookieBehavior = x;

	HG83330
	if(net_CookieBehavior == NET_DontUse) {
//		NET_XP_FileRemove("", xpHTTPCookie);
//		NET_XP_FileRemove("", xpHTTPCookiePermission);
	}
}

PRIVATE void
NET_SetCookieWarningPref(Bool x)
{
	net_WarnAboutCookies = x;
}

PRIVATE NET_CookieBehaviorEnum
NET_GetCookieBehaviorPref(void)
{
	return net_CookieBehavior;
}

PRIVATE Bool
NET_GetCookieWarningPref(void)
{
	return net_WarnAboutCookies;
}

MODULE_PRIVATE int PR_CALLBACK
NET_CookieBehaviorPrefChanged(const char * newpref, void * data)
{
	PRInt32	n;
    if ( (PREF_OK != PREF_GetIntPref(pref_cookieBehavior, &n)) ) {
        n = DEF_COOKIE_BEHAVIOR;
    }
	NET_SetCookieBehaviorPref((NET_CookieBehaviorEnum)n);
	return PREF_NOERROR;
}

MODULE_PRIVATE int PR_CALLBACK
NET_CookieWarningPrefChanged(const char * newpref, void * data)
{
	PRBool	x;
    if ( (PREF_OK != PREF_GetBoolPref(pref_warnAboutCookies, &x)) ) {
        x = FALSE;
    }
	NET_SetCookieWarningPref(x);
	return PREF_NOERROR;
}

/*
 * search if permission already exists
 */
PRIVATE net_CookiePermissionStruct *
net_CheckForCookiePermission(char * hostname) {
    XP_List * list_ptr;
    net_CookiePermissionStruct * cookie_s;

    /* ignore leading period in host name */
    while (hostname && (*hostname == '.')) {
	hostname++;
    }

    net_lock_cookie_permission_list();
    list_ptr = net_cookie_permission_list;
    while((cookie_s = (net_CookiePermissionStruct *) XP_ListNextObject(list_ptr))!=0) {
	if(hostname && cookie_s->host
		&& !PL_strcasecmp(hostname, cookie_s->host)) {
	    net_unlock_cookie_permission_list();
	    return(cookie_s);
        }
    }
    net_unlock_cookie_permission_list();
    return(NULL);
}

/*
 * return:
 *  +1 if cookie permission exists for host and indicates host can set cookies
 *   0 if cookie permission does not exist for host
 *  -1 if cookie permission exists for host and indicates host can't set cookies
 */
PUBLIC int
NET_CookiePermission(char* URLName) {
    net_CookiePermissionStruct * cookiePermission;
    char * host;
    char * colon;

    if (!URLName || !(*URLName)) {
	return 0;
    }

    /* remove protocol from URL name */
    host = NET_ParseURL(URLName, GET_HOST_PART);

    /* remove port number from URL name */
    colon = PL_strchr(host, ':');
    if(colon) {
        *colon = '\0';
    }
    cookiePermission = net_CheckForCookiePermission(host);
    if(colon) {
        *colon = ':';
    }
    PR_Free(host);
    if (cookiePermission == NULL) {
	return 0;
    }
    if (cookiePermission->permission) {
	return 1;
    }
    return -1;
}

/* called from mkgeturl.c, NET_InitNetLib(). This sets the module local cookie pref variables
   and registers the callbacks */
PUBLIC void
COOKIE_RegisterCookiePrefCallbacks(void)
{
	PRInt32	n;
	PRBool	x;

    if ( (PREF_OK != PREF_GetIntPref(pref_cookieBehavior, &n)) ) {
        n = DEF_COOKIE_BEHAVIOR;
    }
	NET_SetCookieBehaviorPref((NET_CookieBehaviorEnum)n);
	PREF_RegisterCallback(pref_cookieBehavior, NET_CookieBehaviorPrefChanged, NULL);

    if ( (PREF_OK != PREF_GetBoolPref(pref_warnAboutCookies, &x)) ) {
        x = FALSE;
    }
	NET_SetCookieWarningPref(x);
	PREF_RegisterCallback(pref_warnAboutCookies, NET_CookieWarningPrefChanged, NULL);
}

/* returns TRUE if authorization is required
** 
**
** IMPORTANT:  Now that this routine is mutli-threaded it is up
**             to the caller to free any returned string
*/
PUBLIC char *
NET_GetCookie(char * address)
{
	char *name=0;
	char *host = NET_ParseURL(address, GET_HOST_PART);
	char *path = NET_ParseURL(address, GET_PATH_PART);
    XP_List * list_ptr;
    net_CookieStruct * cookie_s;
	Bool first=TRUE;
	HG26748
	time_t cur_time = time(NULL);
	int host_length;
	int domain_length;

	/* return string to build */
	char * rv=0;

	/* disable cookie's if the user's prefs say so
	 */
	if(NET_GetCookieBehaviorPref() == NET_DontUse)
		return NULL;

	HG98476

	/* search for all cookies
	 */
	net_lock_cookie_list();
	list_ptr = net_cookie_list;
    while((cookie_s = (net_CookieStruct *) XP_ListNextObject(list_ptr))!=0)
      {
		if(!cookie_s->host)
			continue;

		/* check the host or domain first
		 */
		if(cookie_s->is_domain)
		  {
			char *cp;
			domain_length = PL_strlen(cookie_s->host);

			/* calculate the host length by looking at all characters up to
			 * a colon or '\0'.  That way we won't include port numbers
			 * in domains
			 */
			for(cp=host; *cp != '\0' && *cp != ':'; cp++)
				; /* null body */ 

			host_length = cp - host;
			if(domain_length > host_length 
				|| PL_strncasecmp(cookie_s->host, 
								&host[host_length - domain_length], 
								domain_length))
			  {
				HG20476
				/* no match. FAIL 
				 */
				continue;
			  }
			
		  }
		else if(PL_strcasecmp(host, cookie_s->host))
		  {
			/* hostname matchup failed. FAIL
			 */
			continue;
		  }

        /* shorter strings always come last so there can be no
         * ambiquity
         */
        if(cookie_s->path && !PL_strncmp(path,
                                         cookie_s->path,
                                         PL_strlen(cookie_s->path)))
          {

			/* if the cookie is secure and the path isn't
			 * dont send it
			 */
			HG83764
			
			/* check for expired cookies
			 */
			if( cookie_s->expires && (cookie_s->expires < cur_time) )
			  {
				/* expire and remove the cookie 
				 */
		   		net_FreeCookie(cookie_s);

				/* start the list parsing over :(
				 * we must also start the string over
				 */
				PR_FREEIF(rv);
				rv = NULL;
				list_ptr = net_cookie_list;
				first = TRUE; /* reset first */
				continue;
			  }

			if(first)
				first = FALSE;
			else
				StrAllocCat(rv, "; ");
			
			if(cookie_s->name && *cookie_s->name != '\0')
			  {
				cookie_s->last_accessed = cur_time;
            	StrAllocCopy(name, cookie_s->name);
            	StrAllocCat(name, "=");

#ifdef PREVENT_DUPLICATE_NAMES
				/* make sure we don't have a previous
				 * name mapping already in the string
				 */
				if(!rv || !PL_strstr(rv, name))
			      {	
            	    StrAllocCat(rv, name);
            	    StrAllocCat(rv, cookie_s->cookie);
				  }
#else
            	StrAllocCat(rv, name);
                StrAllocCat(rv, cookie_s->cookie);
#endif /* PREVENT_DUPLICATE_NAMES */
			  }
			else
			  {
            	StrAllocCat(rv, cookie_s->cookie);
			  }
          }
	  }

	  net_unlock_cookie_list();
	PR_FREEIF(name);
	PR_Free(path);
	PR_Free(host);

	/* may be NULL */
	return(rv);
}

void
net_AddCookiePermission
	(net_CookiePermissionStruct * cookie_permission, Bool save ) {

    /*
     * This routine should only be called while holding the
     * cookie permission list lock
     */
    if (cookie_permission) {
	XP_List * list_ptr = net_cookie_permission_list;
	if(!net_cookie_permission_list) {
	    net_cookie_permission_list = XP_ListNew();
	    if(!net_cookie_permission_list) {
		PR_Free(cookie_permission->host);
		PR_Free(cookie_permission);
		return;
	    }
	    list_ptr = net_cookie_permission_list;
	}

#ifdef alphabetize
	/* add it to the list in alphabetical order */
	{
	    net_CookiePermissionStruct * tmp_cookie_permission;
	    Bool permissionAdded = FALSE;
	    while((tmp_cookie_permission = (net_CookiePermissionStruct *)
					   XP_ListNextObject(list_ptr))!=0) {
		if (PL_strcasecmp
			(cookie_permission->host,tmp_cookie_permission->host)<0) {
		    XP_ListInsertObject
			(net_cookie_permission_list,
			tmp_cookie_permission,
			cookie_permission);
		    permissionAdded = TRUE;
		    break;
		}
	    }
	    if (!permissionAdded) {
		XP_ListAddObjectToEnd
		    (net_cookie_permission_list, cookie_permission);
	    }
	}
#else
	/* add it to the end of the list */
	XP_ListAddObjectToEnd (net_cookie_permission_list, cookie_permission);
#endif

	if (save) {
	    cookie_permissions_changed = TRUE;
	    net_SaveCookiePermissions();
	}
    }
}

MODULE_PRIVATE PRBool
net_CookieIsFromHost(net_CookieStruct *cookie_s, char *host) {
    if (!cookie_s || !(cookie_s->host)) {
        return FALSE;
    }
    if (cookie_s->is_domain) {
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
        return (domain_length <= host_length &&
                PL_strncasecmp(cookie_s->host,
                    &host[host_length - domain_length],
                    domain_length) == 0);
    } else {
        return PL_strcasecmp(host, cookie_s->host) == 0;
    }
}

/* find out how many cookies this host has already set */
PRIVATE int
net_CookieCount(char * host) {
    int count = 0;
    XP_List * list_ptr;
    net_CookieStruct * cookie;

    net_lock_cookie_list();
    list_ptr = net_cookie_list;
    while((cookie = (net_CookieStruct *) XP_ListNextObject(list_ptr))!=0) {
	if (host && net_CookieIsFromHost(cookie, host)) {
	    count++;
	}
    }
    net_unlock_cookie_list();

    return count;
}

PUBLIC int
NET_CookieCount(char * URLName) {
    char * host;
    char * colon;
    int count = 0;

    if (!URLName || !(*URLName)) {
	return 0;
    }

    /* remove protocol from URL name */
    host = NET_ParseURL(URLName, GET_HOST_PART);

    /* remove port number from URL name */
    colon = PL_strchr(host, ':');
    if(colon) {
        *colon = '\0';
    }

    /* get count */
    count = net_CookieCount(host);

    /* free up allocated string and return */
    if(colon) {
        *colon = ':';
    }
    PR_Free(host);
    return count;
}

PRBool net_IntSetCookieStringInUse = FALSE;

PRIVATE void
net_DeferCookie(
		char * cur_url,
		char * set_cookie_header,
		time_t timeToExpire) {
	net_DeferCookieStruct * defer_cookie = PR_NEW(net_DeferCookieStruct);
	defer_cookie->cur_url = NULL;
	StrAllocCopy(defer_cookie->cur_url, cur_url);
	defer_cookie->set_cookie_header = NULL;
	StrAllocCopy(defer_cookie->set_cookie_header, set_cookie_header);
	defer_cookie->timeToExpire = timeToExpire;
	net_lock_defer_cookie_list();
	if (!net_defer_cookie_list) {
		net_defer_cookie_list = XP_ListNew();
		if (!net_cookie_list) {
			PR_FREEIF(defer_cookie->cur_url);
			PR_FREEIF(defer_cookie->set_cookie_header);
			PR_Free(defer_cookie);
		}
	}
	XP_ListAddObject(net_defer_cookie_list, defer_cookie);
	net_unlock_defer_cookie_list();
}
           
PRIVATE void
net_IntSetCookieString(char * cur_url,
					char * set_cookie_header,
					time_t timeToExpire );

PRIVATE void
net_UndeferCookies() {
	net_DeferCookieStruct * defer_cookie;
	net_lock_defer_cookie_list();
	if(XP_ListIsEmpty(net_defer_cookie_list)) {
		net_unlock_defer_cookie_list();
		return;
	}
	defer_cookie = (net_DeferCookieStruct*)XP_ListRemoveEndObject(net_defer_cookie_list);
	net_unlock_defer_cookie_list();

    net_IntSetCookieString (
		defer_cookie->cur_url,
		defer_cookie->set_cookie_header,
		defer_cookie->timeToExpire);
	PR_FREEIF(defer_cookie->cur_url);
	PR_FREEIF(defer_cookie->set_cookie_header);
	PR_Free(defer_cookie);
}

/* Java script is calling NET_SetCookieString, netlib is calling 
** this via NET_SetCookieStringFromHttp.
*/
/*
PRIVATE void
net_IntSetCookieString(MWContext * context,
					char * cur_url,
					char * set_cookie_header,
					time_t timeToExpire )*/
PRIVATE void
net_IntSetCookieString(char * cur_url,
					char * set_cookie_header,
					time_t timeToExpire )
{
	net_CookieStruct * prev_cookie;
	char *path_from_header=NULL, *host_from_header=NULL;
	char *host_from_header2=NULL;
	char *name_from_header=NULL, *cookie_from_header=NULL;
	time_t expires=0;
	char *cur_path = NET_ParseURL(cur_url, GET_PATH_PART);
	char *cur_host = NET_ParseURL(cur_url, GET_HOST_PART);
	char *semi_colon, *ptr, *equal;
	PRBool HG83744 is_domain=FALSE, accept=FALSE;
//	MWContextType type;
	Bool bCookieAdded;
	PRBool pref_scd = PR_FALSE;

	/* Only allow cookies to be set in the listed contexts. We
	 * don't want cookies being set in html mail. 
	 */
    /* We need to come back and work on this - Neeti 
	type = context->type;
	if(!( (type == MWContextBrowser)
		|| (type == MWContextHTMLHelp)
		|| (type == MWContextPane) )) {
		PR_Free(cur_path);
		PR_Free(cur_host);
		return;
	}
*/	
	if(NET_GetCookieBehaviorPref() == NET_DontUse) {
		PR_Free(cur_path);
		PR_Free(cur_host);
		return;
	}

	/* Don't enter this routine if it is already in use by another
	   thread.  Otherwise the "remember this decision" result of the
	   other cookie (which came first) won't get applied to this cookie.
	 */
	if (net_IntSetCookieStringInUse) {
		PR_Free(cur_path);
		PR_Free(cur_host);
        net_DeferCookie(cur_url, set_cookie_header, timeToExpire);
		return;
	}
	net_IntSetCookieStringInUse = TRUE;

	HG87358
	/* terminate at any carriage return or linefeed */
	for(ptr=set_cookie_header; *ptr; ptr++)
		if(*ptr == LF || *ptr == CR) {
			*ptr = '\0';
			break;
		}

	/* parse path and expires attributes from header if
 	 * present
	 */
	semi_colon = PL_strchr(set_cookie_header, ';');

	if(semi_colon)
	  {
		/* truncate at semi-colon and advance 
		 */
		*semi_colon++ = '\0';

		/* there must be some attributes. (hopefully)
		 */
		HG83476

		/* look for the path attribute
		 */
		ptr = PL_strcasestr(semi_colon, "path=");

		if(ptr) {
			/* allocate more than we need */
			StrAllocCopy(path_from_header, XP_StripLine(ptr+5));
			/* terminate at first space or semi-colon
			 */
			for(ptr=path_from_header; *ptr != '\0'; ptr++)
				if(NET_IS_SPACE(*ptr) || *ptr == ';' || *ptr == ',') {
					*ptr = '\0';
					break;
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

            /* terminate at first space or semi-colon
             */
            for(ptr=domain_from_header; *ptr != '\0'; ptr++)
                if(NET_IS_SPACE(*ptr) || *ptr == ';' || *ptr == ',') {
                    *ptr = '\0';
                    break;
                  }

            /* verify that this host has the authority to set for
             * this domain.   We do this by making sure that the
             * host is in the domain
             * We also require that a domain have at least two
             * periods to prevent domains of the form ".com"
             * and ".edu"
			 *
			 * Also make sure that there is more stuff after
			 * the second dot to prevent ".com."
             */
            dot = PL_strchr(domain_from_header, '.');
			if(dot)
                dot = PL_strchr(dot+1, '.');

			if(!dot || *(dot+1) == '\0') {
				/* did not pass two dot test. FAIL
				 */
				PR_Free(domain_from_header);
				PR_Free(cur_path);
				PR_Free(cur_host);
	//			TRACEMSG(("DOMAIN failed two dot test"));
				net_IntSetCookieStringInUse = FALSE;
				net_UndeferCookies();
				return;
			  }

			/* strip port numbers from the current host
			 * for the domain test
			 */
			colon = PL_strchr(cur_host, ':');
			if(colon)
			   *colon = '\0';

			domain_length   = PL_strlen(domain_from_header);
			cur_host_length = PL_strlen(cur_host);

			/* check to see if the host is in the domain
			 */
			if(domain_length > cur_host_length
				|| PL_strcasecmp(domain_from_header, 
							   &cur_host[cur_host_length-domain_length]))
			  {
	/*			TRACEMSG(("DOMAIN failed host within domain test."
					  " Domain: %s, Host: %s", domain_from_header, cur_host));*/
				PR_Free(domain_from_header);
				PR_Free(cur_path);
				PR_Free(cur_host);
				net_IntSetCookieStringInUse = FALSE;
				net_UndeferCookies();
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
 *  So only do it if the restrictCookieDomains pref is TRUE.
 *
 */
            if ( PREF_GetBoolPref(pref_strictCookieDomains, &pref_scd) < 0 ) {
               pref_scd = PR_FALSE;
            }

            if ( pref_scd == PR_TRUE ) {

			cur_host[cur_host_length-domain_length] = '\0';
			dot = XP_STRCHR(cur_host, '.');
			cur_host[cur_host_length-domain_length] = '.';
			if (dot) {
	/*			TRACEMSG(("host minus domain failed no-dot test."
				  " Domain: %s, Host: %s", domain_from_header, cur_host));*/
				PR_Free(domain_from_header);
				PR_Free(cur_path);
				PR_Free(cur_host);
				net_IntSetCookieStringInUse = FALSE;
				net_UndeferCookies();
				return;
			}
            }

			/* all tests passed, copy in domain to hostname field
			 */
			StrAllocCopy(host_from_header, domain_from_header);
			is_domain = TRUE;

	//		TRACEMSG(("Accepted domain: %s", host_from_header));

			PR_Free(domain_from_header);
          }

		/* now search for the expires header 
		 * NOTE: that this part of the parsing
		 * destroys the original part of the string
		 */
		ptr = PL_strcasestr(semi_colon, "expires=");

		if(ptr) {
			char *date =  ptr+8;
			/* terminate the string at the next semi-colon
			 */
			for(ptr=date; *ptr != '\0'; ptr++)
				if(*ptr == ';') {
					*ptr = '\0';
					break;
				  }
			if(timeToExpire)
				expires = timeToExpire;
			else
				expires = NET_ParseDate(date);

	//		TRACEMSG(("Have expires date: %ld", expires));
		  }
	  }

	if(!path_from_header) {
        /* strip down everything after the last slash
         * to get the path.
         */
        char * slash = PL_strrchr(cur_path, '/');
        if(slash)
            *slash = '\0';

		path_from_header = cur_path;
	  } else {
		PR_Free(cur_path);
	  }

	if(!host_from_header)
		host_from_header = cur_host;
	else
		PR_Free(cur_host);

	/* keep cookies under the max bytes limit */
	if(PL_strlen(set_cookie_header) > MAX_BYTES_PER_COOKIE)
		set_cookie_header[MAX_BYTES_PER_COOKIE-1] = '\0';

	/* separate the name from the cookie */
	equal = PL_strchr(set_cookie_header, '=');

	if(equal) {
		*equal = '\0';
		StrAllocCopy(name_from_header, XP_StripLine(set_cookie_header));
		StrAllocCopy(cookie_from_header, XP_StripLine(equal+1));
	  } else {
//		TRACEMSG(("Warning: no name found for cookie"));
		StrAllocCopy(cookie_from_header, XP_StripLine(set_cookie_header));
		StrAllocCopy(name_from_header, "");
	  }

	net_CookiePermissionStruct * cookie_permission;
	cookie_permission = net_CheckForCookiePermission(host_from_header);
	if (cookie_permission != NULL) {
	    if (cookie_permission->permission == FALSE) {
		PR_FREEIF(path_from_header);
		PR_FREEIF(host_from_header);
		PR_FREEIF(name_from_header);
		PR_FREEIF(cookie_from_header);
		net_IntSetCookieStringInUse = FALSE;
		net_UndeferCookies();
		return;
	    } else {
		accept = TRUE;
            }
	}
 

	if( (NET_GetCookieWarningPref() ) && !accept ) {
		/* the user wants to know about cookies so let them
		 * know about every one that is set and give them
		 * the choice to accept it or not
		 */
		char * new_string=0;
		int count;

#ifdef newI18N
		char * remember_string = cookie_Localize("RememberThisDecision");
#else
                char * remember_string = 0;
                StrAllocCopy
                    (remember_string, XP_GetString(MK_ACCESS_COOKIES_REMEMBER));
#endif

		/* find out how many cookies this host has already set */
		count = net_CookieCount(host_from_header);
		net_lock_cookie_list();
		prev_cookie = net_CheckForPrevCookie
			(path_from_header, host_from_header, name_from_header);
		net_unlock_cookie_list();
                char * message;
		if (prev_cookie) {
#ifdef newI18N
                    message = cookie_Localize("PermissionToModifyCookie");
#else
                    message = XP_GetString(MK_ACCESS_COOKIES_WISHES_MODIFY);
#endif
		    new_string = PR_smprintf(message,
			host_from_header ? host_from_header : "");
		} else if (count>1) {
#ifdef newI18N
                    message = cookie_Localize("PermissionToSetAnotherCookie");
#else
                    message = XP_GetString(MK_ACCESS_COOKIES_WISHESN);
#endif
		    new_string = PR_smprintf(message,
			host_from_header ? host_from_header : "",
			count);
		} else if (count==1){
#ifdef newI18N
                    message = cookie_Localize("PermissionToSetSecondCookie");
#else
                    message = XP_GetString(MK_ACCESS_COOKIES_WISHES1);
#endif
		    new_string = PR_smprintf(message,
			host_from_header ? host_from_header : "");
		} else {
#ifdef newI18N
                    message = cookie_Localize("PermissionToSetACookie");
#else
                    message = XP_GetString(MK_ACCESS_COOKIES_WISHES0);
#endif
		    new_string = PR_smprintf(message,
			host_from_header ? host_from_header : "");
		}
#ifdef newI18N
                PR_FREEIF(message);
#endif

		/* 
		 * Who knows what thread we are on.  Only the mozilla thread
		 *   is allowed to post dialogs so, if need be, go over there
		 */

	    {
		Bool old_cookie_remember_checked = cookie_remember_checked;
        PRBool userHasAccepted = Cookie_CheckConfirm
		    (new_string, remember_string, &cookie_remember_checked);
		PR_FREEIF(new_string);
		PR_FREEIF(remember_string);
		if (cookie_remember_checked) {
                    net_CookiePermissionStruct * cookie_permission;
                    cookie_permission = PR_NEW(net_CookiePermissionStruct);
                    if (cookie_permission) {
                        net_lock_cookie_permission_list();
                        StrAllocCopy(host_from_header2, host_from_header);
                        /* ignore leading periods in host name */
                        while (host_from_header2 && (*host_from_header2 == '.')) {
                            host_from_header2++;
                        }
                        cookie_permission->host = host_from_header2; /* set host string */
                        cookie_permission->permission = userHasAccepted;
                        net_AddCookiePermission(cookie_permission, TRUE);
                        net_unlock_cookie_permission_list();
                    }
                }

		if (old_cookie_remember_checked != cookie_remember_checked) {
		    cookie_permissions_changed = TRUE;
		    net_SaveCookiePermissions();
		}

		if (!userHasAccepted) {
			PR_FREEIF(path_from_header);
			PR_FREEIF(host_from_header);
			PR_FREEIF(name_from_header);
			PR_FREEIF(cookie_from_header);
			net_IntSetCookieStringInUse = FALSE;
			return;
		}
	    }
	  }

//	TRACEMSG(("mkaccess.c: Setting cookie: %s for host: %s for path: %s",
//					cookie_from_header, host_from_header, path_from_header));

	/* 
	 * We have decided we are going to insert a cookie into the list
	 *   Get the cookie lock so that we can munge on the list
	 */
	net_lock_cookie_list();

	/* limit the number of cookies from a specific host or domain */
	net_CheckForMaxCookiesFromHost(host_from_header);

	if(XP_ListCount(net_cookie_list) > MAX_NUMBER_OF_COOKIES-1)
		net_remove_oldest_cookie();


    prev_cookie = net_CheckForPrevCookie
        (path_from_header, host_from_header, name_from_header);

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
        HG83263
        prev_cookie->is_domain = is_domain;
		prev_cookie->last_accessed = time(NULL);
      }	else {
		XP_List * list_ptr = net_cookie_list;
		net_CookieStruct * tmp_cookie_ptr;
		size_t new_len;

        /* construct a new cookie_struct
         */
        prev_cookie = PR_NEW(net_CookieStruct);
        if(!prev_cookie) {
			PR_FREEIF(path_from_header);
			PR_FREEIF(host_from_header);
			PR_FREEIF(name_from_header);
			PR_FREEIF(cookie_from_header);
			net_unlock_cookie_list();
			net_IntSetCookieStringInUse = FALSE;
			net_UndeferCookies();
			return;
          }
    
        /* copy
         */
        prev_cookie->cookie  = cookie_from_header;
        prev_cookie->name    = name_from_header;
        prev_cookie->path    = path_from_header;
        prev_cookie->host    = host_from_header;
        prev_cookie->expires = expires;
        HG22730
        prev_cookie->is_domain = is_domain;
		prev_cookie->last_accessed = time(NULL);

		if(!net_cookie_list) {
			net_cookie_list = XP_ListNew();
		    if(!net_cookie_list) {
				PR_FREEIF(path_from_header);
				PR_FREEIF(name_from_header);
				PR_FREEIF(host_from_header);
				PR_FREEIF(cookie_from_header);
				PR_Free(prev_cookie);
				net_unlock_cookie_list();
				net_IntSetCookieStringInUse = FALSE;
				net_UndeferCookies();
				return;
			  }
		  }		

		/* add it to the list so that it is before any strings of
		 * smaller length
		 */
		bCookieAdded = FALSE;
		new_len = PL_strlen(prev_cookie->path);
		while((tmp_cookie_ptr = (net_CookieStruct *) XP_ListNextObject(list_ptr))!=0) { 
			if(new_len > PL_strlen(tmp_cookie_ptr->path)) {
				XP_ListInsertObject(net_cookie_list, tmp_cookie_ptr, prev_cookie);
				bCookieAdded = TRUE;
				break;
			  }
		  }
		if ( !bCookieAdded ) {
			/* no shorter strings found in list */
			XP_ListAddObjectToEnd(net_cookie_list, prev_cookie);
		}
	  }

	/* At this point we know a cookie has changed. Write the cookies to file. */
	cookies_changed = TRUE;
	net_SaveCookies();
	net_unlock_cookie_list();
	net_IntSetCookieStringInUse = FALSE;
	net_UndeferCookies();
	return;
}

PUBLIC void
NET_SetCookieString(char * cur_url,
					char * set_cookie_header) {
    net_IntSetCookieString(cur_url, set_cookie_header, 0);
}

/* Determines whether the inlineHost is in the same domain as the currentHost. For use with rfc 2109
 * compliance/non-compliance. */
PRIVATE int
NET_SameDomain(char * currentHost, char * inlineHost)
{
	char * dot = 0;
	char * currentDomain = 0;
	char * inlineDomain = 0;

	if(!currentHost || !inlineHost)
		return 0;

	/* case insensitive compare */
	if(PL_strcasecmp(currentHost, inlineHost) == 0)
		return 1;

	currentDomain = PL_strchr(currentHost, '.');
	inlineDomain = PL_strchr(inlineHost, '.');

	if(!currentDomain || !inlineDomain)
		return 0;
	
	/* check for at least two dots before continuing, if there are
	   not two dots we don't have enough information to determine
	   whether or not the inlineDomain is within the currentDomain */
	dot = PL_strchr(inlineDomain, '.');
	if(dot)
		dot = PL_strchr(dot+1, '.');
	else
		return 0;

	/* handle .com. case */
	if(!dot || (*(dot+1) == '\0') )
		return 0;

	if(!PL_strcasecmp(inlineDomain, currentDomain))
		return 1;
	return 0;
}

/* This function wrapper wraps NET_SetCookieString for the purposes of 
** determining whether or not a cookie is inline (we need the URL struct, 
** and outputFormat to do so).  this is called from NET_ParseMimeHeaders 
** in mkhttp.c
** This routine does not need to worry about the cookie lock since all of
**   the work is handled by sub-routines
*/

PUBLIC void
NET_SetCookieStringFromHttp(char * cur_url,
							char * set_cookie_header, char * server_date)
{
	/* If the outputFormat is not PRESENT (the url is not going to the screen), and not
	*  SAVE AS (shift-click) then 
	*  the cookie being set is defined as inline so we need to do what the user wants us
	*  to based on his preference to deal with foreign cookies. If it's not inline, just set
	*  the cookie. */
	char *ptr=NULL, *date=NULL;
	time_t gmtCookieExpires=0, expires=0, sDate;

    /* We need to come back and fix this.  - Neeti
	if(CLEAR_CACHE_BIT(outputFormat) != FO_PRESENT && CLEAR_CACHE_BIT(outputFormat) != FO_SAVE_AS)
	{
		if (NET_GetCookieBehaviorPref() == NET_DontAcceptForeign)
		{
			// the user doesn't want foreign cookies, check to see if its foreign 
			char * curSessionHistHost = 0;
			char * theColon = 0;
			char * curHost = NET_ParseURL(cur_url, GET_HOST_PART);
			History_entry * shistEntry = SHIST_GetCurrent(&context->hist);
			if (shistEntry) {
			curSessionHistHost = NET_ParseURL(shistEntry->address, GET_HOST_PART);
			}
			if(!curHost || !curSessionHistHost)
			{
				PR_FREEIF(curHost);
				PR_FREEIF(curSessionHistHost);
				return;
			}

			// strip ports 
			theColon = PL_strchr(curHost, ':');
			if(theColon)
			   *theColon = '\0';
			theColon = PL_strchr(curSessionHistHost, ':');
			if(theColon)
				*theColon = '\0';

			// if it's foreign, get out of here after a little clean up 
			if(!NET_SameDomain(curHost, curSessionHistHost))
			{
				PR_FREEIF(curHost);	
				PR_FREEIF(curSessionHistHost);
				return;
			}
			PR_FREEIF(curHost);	
			PR_FREEIF(curSessionHistHost);
		}
	}
    */
	/* Determine when the cookie should expire. This is done by taking the difference between 
	   the server time and the time the server wants the cookie to expire, and adding that 
	   difference to the client time. This localizes the client time regardless of whether or
	   not the TZ environment variable was set on the client. */

	/* Get the time the cookie is supposed to expire according to the attribute*/
	ptr = PL_strcasestr(set_cookie_header, "expires=");
	if(ptr)
	{
		char *date =  ptr+8;
		char origLast = '\0';
		for(ptr=date; *ptr != '\0'; ptr++)
			if(*ptr == ';')
			  {
				origLast = ';';
				*ptr = '\0';
				break;
			  }
		expires = NET_ParseDate(date);
		*ptr=origLast;
	}
    sDate = NET_ParseDate(server_date);
	if( sDate && expires )
	{
		if( expires < sDate )
		{
			gmtCookieExpires=1;
		}
		else
		{
			gmtCookieExpires = expires - sDate + time(NULL);
			// if overflow 
			if( gmtCookieExpires < time(NULL) )
				gmtCookieExpires = (((unsigned) (~0) << 1) >> 1); // max int 
		}
	}

    net_IntSetCookieString(cur_url, set_cookie_header, gmtCookieExpires);
}

#ifndef XP_MAC
/* saves the HTTP cookies permissions to disk */
PRIVATE void
net_SaveCookiePermissions()
{
  XP_List * list_ptr;
  net_CookiePermissionStruct * cookie_permission_s;

  if (NET_GetCookieBehaviorPref() == NET_DontUse) {
    return;
  }

  if (!cookie_permissions_changed) {
    return;
  }

  net_lock_cookie_permission_list();
  list_ptr = net_cookie_permission_list;

  if (!(net_cookie_permission_list)) {
    net_unlock_cookie_permission_list();
    return;
  }

  nsFileSpec dirSpec;
  nsresult rval = cookie_ProfileDirectory(dirSpec);
  if (NS_FAILED(rval)) {
    net_unlock_cookie_permission_list();
    return;
  }
  nsOutputFileStream strm(dirSpec + "cookperm.txt");
  if (!strm.is_open()) {
    net_unlock_cookie_permission_list();
    return;
  }

  cookie_Put(strm, "# Netscape HTTP Cookie Permission File\n");
  cookie_Put(strm, "# http://www.netscape.com/newsref/std/cookie_spec.html\n");
  cookie_Put(strm, "# This is a generated file!  Do not edit.\n\n");

  /* format shall be:
   * host \t permission
   */
  while ((cookie_permission_s = (net_CookiePermissionStruct *)
    XP_ListNextObject(list_ptr)) != NULL) {
    cookie_Put(strm, cookie_permission_s->host);
    if (cookie_permission_s->permission) {
      cookie_Put(strm, "\tTRUE\n");
    } else {
      cookie_Put(strm, "\tFALSE\n");
    }
  }

  /* save current state of cookie nag-box's checkmark */
  if (cookie_remember_checked) {
    cookie_Put(strm, "@@@@\tTRUE\n");
  } else {
    cookie_Put(strm, "@@@@\tFALSE\n");
  }

  cookie_permissions_changed = FALSE;
  strm.flush();
  strm.close();
  net_unlock_cookie_permission_list();
}

/* reads the HTTP cookies permission from disk */
PRIVATE void
net_ReadCookiePermissions()
{
  nsAutoString * buffer;
  net_CookiePermissionStruct * new_cookie_permission;

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

  net_lock_cookie_permission_list();
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
    new_cookie_permission = PR_NEW(net_CookiePermissionStruct);
    if (!new_cookie_permission) {
      delete buffer;
      net_unlock_cookie_permission_list();
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
      cookie_remember_checked = new_cookie_permission->permission;
    } else {
      /* add the permission entry */
      net_AddCookiePermission(new_cookie_permission, FALSE );
    }
  }

  strm.close();
  net_unlock_cookie_permission_list();
  cookie_permissions_changed = FALSE;
  return;
}

/* saves out the HTTP cookies to disk */
PRIVATE void
net_SaveCookies()
{
  XP_List * list_ptr;
  net_CookieStruct * cookie_s;
  time_t cur_date = time(NULL);
  char date_string[36];

  if (NET_GetCookieBehaviorPref() == NET_DontUse) {
    return;
  }
  if (!cookies_changed) {
    return;
  }

  net_lock_cookie_list();
  list_ptr = net_cookie_list;
  if (!(list_ptr)) {
    net_unlock_cookie_list();
    return;
  }

  nsFileSpec dirSpec;
  nsresult rv = cookie_ProfileDirectory(dirSpec);
  if (NS_FAILED(rv)) {
    net_unlock_cookie_list();
    return;
  }
  nsOutputFileStream strm(dirSpec + "cookies.txt");
  if (!strm.is_open()) {
    /* file doesn't exist -- that's not an error */
    net_unlock_cookie_list();
    return;
  }

  cookie_Put(strm, "# Netscape HTTP Cookie File\n");
  cookie_Put(strm, "# http://www.netscape.com/newsref/std/cookie_spec.html\n");
  cookie_Put(strm, "# This is a generated file!  Do not edit.\n\n");

  /* format shall be:
   *
   * host \t is_domain \t path \t secure \t expires \t name \t cookie
   *
   * is_domain is TRUE or FALSE
   * xxx is TRUE or FALSE  
   * expires is a time_t integer
   * cookie can have tabs
   */
  while((cookie_s = (net_CookieStruct *) XP_ListNextObject(list_ptr)) != NULL) {
    if (cookie_s->expires < cur_date) {
      /* don't write entry if cookie has expired or has no expiration date */
      continue;
    }
    cookie_Put(strm, cookie_s->host);
		
    if (cookie_s->is_domain) {
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

  cookies_changed = FALSE;
  strm.flush();
  strm.close();
  net_unlock_cookie_list();
}

/* reads HTTP cookies from disk */
PRIVATE void
net_ReadCookies()
{
  XP_List * list_ptr;
  net_CookieStruct *new_cookie, *tmp_cookie_ptr;
  size_t new_len;
  nsAutoString * buffer;
  Bool added_to_list;

  net_ReadCookiePermissions();

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

  net_lock_cookie_list();
  list_ptr = net_cookie_list;

  /* format is:
   *
   * host \t is_domain \t path \t xxx \t expires \t name \t cookie
   *
   * if this format isn't respected we move onto the next line in the file.
   * is_domain is TRUE or FALSE	-- defaulting to FALSE
   * xxx is TRUE or FALSE   -- should default to TRUE
   * expires is a time_t integer
   * cookie can have tabs
   */
  while (cookie_GetLine(strm,buffer) != -1){
    added_to_list = FALSE;

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
    new_cookie = PR_NEW(net_CookieStruct);
    if (!new_cookie) {
      delete buffer;
      net_unlock_cookie_list();
      strm.close();
      return;
    }
    memset(new_cookie, 0, sizeof(net_CookieStruct));
    new_cookie->name = name.ToNewCString();
    new_cookie->cookie = cookie.ToNewCString();
    new_cookie->host = host.ToNewCString();
    new_cookie->path = path.ToNewCString();
    if (isDomain == "TRUE") {
      new_cookie->is_domain = PR_TRUE;
    } else {
      new_cookie->is_domain = PR_FALSE;
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
    if (!net_cookie_list) {
      net_cookie_list = XP_ListNew();
      if (!net_cookie_list) {
        net_unlock_cookie_list();
        strm.close();
        return;
      }
    }

    /* add new cookie to the list so that it is before any strings of smaller length */
    new_len = PL_strlen(new_cookie->path);
    while ((tmp_cookie_ptr = (net_CookieStruct *) XP_ListNextObject(list_ptr)) != NULL) {
      if (new_len > PL_strlen(tmp_cookie_ptr->path)) {
        XP_ListInsertObject(net_cookie_list, tmp_cookie_ptr, new_cookie);
        added_to_list = TRUE;
        break;
      }
    }

    /* no shorter strings found in list so add new cookie at end */	
    if (!added_to_list) {
      XP_ListAddObjectToEnd(net_cookie_list, new_cookie);
    }
  }

  strm.close();
  net_unlock_cookie_list();
  cookies_changed = FALSE;
  return;
}
#else

/* saves the HTTP cookies permissions to disk
 * the parameter passed in on entry is ignored
 * returns 0 on success -1 on failure.
 */
PRIVATE void
net_SaveCookiePermissions()
{
    XP_List * list_ptr;
    net_CookiePermissionStruct * cookie_permission_s;
    XP_File fp;
    int32 len = 0;

    if(NET_GetCookieBehaviorPref() == NET_DontUse) {
	return;
    }

    if(!cookie_permissions_changed) {
	return;
    }

    net_lock_cookie_permission_list();
    list_ptr = net_cookie_permission_list;

    if(!(net_cookie_permission_list)) {
	net_unlock_cookie_permission_list();
	return;
    }

    if(!(fp = NET_XP_FileOpen(NULL, xpHTTPCookiePermission, XP_FILE_WRITE))) {
	net_unlock_cookie_permission_list();
	return;
    }
    len = NET_XP_FileWrite("# Netscape HTTP Cookie Permission File" LINEBREAK
                 "# http://www.netscape.com/newsref/std/cookie_spec.html"
                  LINEBREAK "# This is a generated file!  Do not edit."
		  LINEBREAK LINEBREAK, -1, fp);

    if (len < 0) {
	NET_XP_FileClose(fp);
	net_unlock_cookie_permission_list();
	return;
    }

    /* format shall be:
     * host \t permission <optional trust label information >
     */
    while((cookie_permission_s = (net_CookiePermissionStruct *)
	    XP_ListNextObject(list_ptr)) != NULL) {
	len = NET_XP_FileWrite(cookie_permission_s->host, -1, fp);
	if (len < 0) {
	    NET_XP_FileClose(fp);
	    net_unlock_cookie_permission_list();
	    return;
	}

        NET_XP_FileWrite("\t", 1, fp);

	if(cookie_permission_s->permission) {
            NET_XP_FileWrite("TRUE", -1, fp);
	} else {
            NET_XP_FileWrite("FALSE", -1, fp);
	}

	len = NET_XP_FileWrite(LINEBREAK, -1, fp);
	if (len < 0) {
	    NET_XP_FileClose(fp);
	    net_unlock_cookie_permission_list();
	    return;
	}
    }

    /* save current state of cookie nag-box's checkmark */
    NET_XP_FileWrite("@@@@", -1, fp);
    NET_XP_FileWrite("\t", 1, fp);
    if (cookie_remember_checked) {
        NET_XP_FileWrite("TRUE", -1, fp);
    } else {
        NET_XP_FileWrite("FALSE", -1, fp);
    }
    NET_XP_FileWrite(LINEBREAK, -1, fp);

    cookie_permissions_changed = FALSE;
    NET_XP_FileClose(fp);
    net_unlock_cookie_permission_list();
    return;
}

/* reads the HTTP cookies permission from disk
 * the parameter passed in on entry is ignored
 * returns 0 on success -1 on failure.
 *
 */
#define PERMISSION_LINE_BUFFER_SIZE 4096

PRIVATE void
net_ReadCookiePermissions()
{
    XP_File fp;
    char buffer[PERMISSION_LINE_BUFFER_SIZE];
    char *host, *p;
    Bool permission_value;
    net_CookiePermissionStruct * cookie_permission;
    char *host_from_header2;

    if(!(fp = NET_XP_FileOpen(NULL, xpHTTPCookiePermission, XP_FILE_READ)))
	return;

    /* format is:
     * host \t permission <optional trust label information>
     * if this format isn't respected we move onto the next line in the file.
     */

    net_lock_cookie_permission_list();
    while(NET_XP_FileReadLine(buffer, PERMISSION_LINE_BUFFER_SIZE, fp)) {
        if (*buffer == '#' || *buffer == CR || *buffer == LF || *buffer == 0) {
	    continue;
	}

        XP_StripLine(buffer); /* remove '\n' from end of the line */

        host = buffer;
        /* isolate the host field which is the first field on the line */
        if( !(p = PL_strchr(host, '\t')) ) {
            continue; /* no permission field */
        }
        *p++ = '\0';
        if(*p == CR || *p == LF || *p == 0) {
            continue; /* no permission field */
	}

        /* ignore leading periods in host name */
        while (host && (*host == '.')) {
            host++;
	}

	/*
         * the first part of the permission file is valid -
         * allocate a new permission struct and fill it in
         */
        cookie_permission = PR_NEW(net_CookiePermissionStruct);
        if (cookie_permission) {
            host_from_header2 = PL_strdup(host);
            cookie_permission->host = host_from_header2;

            /*
             *  Now handle the permission field.
             * a host value of "@@@@" is a special code designating the
             * state of the cookie nag-box's checkmark
	     */
            permission_value = (!PL_strncmp(p, "TRUE", sizeof("TRUE")-1));
            if (!PL_strcmp(host, "@@@@")) {
                cookie_remember_checked = permission_value;
            } else {
                cookie_permission->permission = permission_value;
			

                /* add the permission entry */
                net_AddCookiePermission( cookie_permission, FALSE );
            }
        } /* end if (cookie_permission) */
    } /* while(NET_XP_FileReadLine( */

    net_unlock_cookie_permission_list();
    NET_XP_FileClose(fp);
    cookie_permissions_changed = FALSE;
    return;
}

/* saves out the HTTP cookies to disk
 *
 * on entry pass in the name of the file to save
 *
 * returns 0 on success -1 on failure.
 *
 */
PRIVATE void
net_SaveCookies()
{
    XP_List * list_ptr;
    net_CookieStruct * cookie_s;
	time_t cur_date = time(NULL);
	XP_File fp;
	int32 len = 0;
	char date_string[36];

	if(NET_GetCookieBehaviorPref() == NET_DontUse)
	  return;

	if(!cookies_changed)
	  return;

	net_lock_cookie_list();
	list_ptr = net_cookie_list;
	if(!(list_ptr)) {
		net_unlock_cookie_list();
		return;
	}

	if(!(fp = NET_XP_FileOpen(NULL, xpHTTPCookie, XP_FILE_WRITE))) {
		net_unlock_cookie_list();
		return;
	}

	len = NET_XP_FileWrite("# Netscape HTTP Cookie File" LINEBREAK
				 "# http://www.netscape.com/newsref/std/cookie_spec.html"
				 LINEBREAK "# This is a generated file!  Do not edit."
				 LINEBREAK LINEBREAK,
				 -1, fp);
	if (len < 0)
	{
		NET_XP_FileClose(fp);
		net_unlock_cookie_list();
		return;
	}

	/* format shall be:
 	 *
	 * host \t is_domain \t path \t secure \t expires \t name \t cookie
	 *
	 * is_domain is TRUE or FALSE
	 * secure is TRUE or FALSE  
	 * expires is a time_t integer
	 * cookie can have tabs
	 */
    while((cookie_s = (net_CookieStruct *) XP_ListNextObject(list_ptr)) != NULL)
      {
		if(cookie_s->expires < cur_date)
			continue;  /* don't write entry if cookie has expired 
						* or has no expiration date
						*/

		len = NET_XP_FileWrite(cookie_s->host, -1, fp);
		if (len < 0)
		{
			NET_XP_FileClose(fp);
			net_unlock_cookie_list();
			return;
		}
		NET_XP_FileWrite("\t", 1, fp);
		
		if(cookie_s->is_domain)
			NET_XP_FileWrite("TRUE", -1, fp);
		else
			NET_XP_FileWrite("FALSE", -1, fp);
		NET_XP_FileWrite("\t", 1, fp);

		NET_XP_FileWrite(cookie_s->path, -1, fp);
		NET_XP_FileWrite("\t", 1, fp);

		HG74640
		    NET_XP_FileWrite("FALSE", -1, fp);
		NET_XP_FileWrite("\t", 1, fp);

		PR_snprintf(date_string, sizeof(date_string), "%lu", cookie_s->expires);
		NET_XP_FileWrite(date_string, -1, fp);
		NET_XP_FileWrite("\t", 1, fp);

		NET_XP_FileWrite(cookie_s->name, -1, fp);
		NET_XP_FileWrite("\t", 1, fp);

		NET_XP_FileWrite(cookie_s->cookie, -1, fp);
 		len = NET_XP_FileWrite(LINEBREAK, -1, fp);
		if (len < 0)
		{
			NET_XP_FileClose(fp);
			net_unlock_cookie_list();
			return;
		}
      }

	cookies_changed = FALSE;

	NET_XP_FileClose(fp);
	net_unlock_cookie_list();
    return;
}

/* reads HTTP cookies from disk
 *
 * on entry pass in the name of the file to read
 *
 * returns 0 on success -1 on failure.
 *
 *
 */
#define LINE_BUFFER_SIZE 4096

PRIVATE void
net_ReadCookies()
{
    XP_List * list_ptr;
    net_CookieStruct *new_cookie, *tmp_cookie_ptr;
	size_t new_len;
    XP_File fp;
	char buffer[LINE_BUFFER_SIZE];
	char *host, *is_domain, *path, *xxx, *expires, *name, *cookie;
	Bool added_to_list;

    net_ReadCookiePermissions();

    if(!(fp = NET_XP_FileOpen(NULL, xpHTTPCookie, XP_FILE_READ)))
        return;

	net_lock_cookie_list();
	list_ptr = net_cookie_list;

    /* format is:
     *
     * host \t is_domain \t path \t xxx \t expires \t name \t cookie
     *
     * if this format isn't respected we move onto the next line in the file.
     * is_domain is TRUE or FALSE	-- defaulting to FALSE
     * xxx is TRUE or FALSE   -- should default to TRUE
     * expires is a time_t integer
     * cookie can have tabs
     */
    while(NET_XP_FileReadLine(buffer, LINE_BUFFER_SIZE, fp))
      {
		added_to_list = FALSE;

		if (*buffer == '#' || *buffer == CR || *buffer == LF || *buffer == 0)
		  continue;

		host = buffer;
		
		if( !(is_domain = PL_strchr(host, '\t')) )
			continue;
		*is_domain++ = '\0';
		if(*is_domain == CR || *is_domain == LF || *is_domain == 0)
			continue;
		
		if( !(path = PL_strchr(is_domain, '\t')) )
			continue;
		*path++ = '\0';
		if(*path == CR || *path == LF || *path == 0)
			continue;

		if( !(xxx = PL_strchr(path, '\t')) )
			continue;
		*xxx++ = '\0';
		if(*xxx == CR || *xxx == LF || *xxx == 0)
			continue;

		if( !(expires = PL_strchr(xxx, '\t')) )
			continue;
		*expires++ = '\0';
		if(*expires == CR || *expires == LF || *expires == 0)
			continue;

        if( !(name = PL_strchr(expires, '\t')) )
			continue;
		*name++ = '\0';
		if(*name == CR || *name == LF || *name == 0)
			continue;

        if( !(cookie = PL_strchr(name, '\t')) )
			continue;
		*cookie++ = '\0';
		if(*cookie == CR || *cookie == LF || *cookie == 0)
			continue;

		/* remove the '\n' from the end of the cookie */
		XP_StripLine(cookie);

        /* construct a new cookie_struct
         */
        new_cookie = PR_NEW(net_CookieStruct);
        if(!new_cookie)
          {
			net_unlock_cookie_list();
            return;
          }

		memset(new_cookie, 0, sizeof(net_CookieStruct));
    
        /* copy
         */
        StrAllocCopy(new_cookie->cookie, cookie);
        StrAllocCopy(new_cookie->name, name);
        StrAllocCopy(new_cookie->path, path);
        StrAllocCopy(new_cookie->host, host);
        new_cookie->expires = atol(expires);

		HG87365

        if(!PL_strcmp(is_domain, "TRUE"))
        	new_cookie->is_domain = TRUE;
        else
        	new_cookie->is_domain = FALSE;

		if(!net_cookie_list)
		  {
			net_cookie_list = XP_ListNew();
		    if(!net_cookie_list)
			  {
				net_unlock_cookie_list();
				return;
			  }
		  }		

		/* add it to the list so that it is before any strings of
		 * smaller length
		 */
		new_len = PL_strlen(new_cookie->path);
		while((tmp_cookie_ptr = (net_CookieStruct *) XP_ListNextObject(list_ptr)) != NULL)
		  { 
			if(new_len > PL_strlen(tmp_cookie_ptr->path))
			  {
				XP_ListInsertObject(net_cookie_list, tmp_cookie_ptr, new_cookie);
				added_to_list = TRUE;
				break;
			  }
		  }

		/* no shorter strings found in list */	
		if(!added_to_list)
		    XP_ListAddObjectToEnd(net_cookie_list, new_cookie);
	  }

    NET_XP_FileClose(fp);
	net_unlock_cookie_list();

	cookies_changed = FALSE;

    return;
}
#endif

PUBLIC int
NET_SaveCookies(char * filename)
{
  net_SaveCookies();
  return 0;
}

PUBLIC int
NET_ReadCookies(char * filename)
{
  net_ReadCookies();
  return 0;
}

/* return TRUE if "number" is in sequence of comma-separated numbers */
Bool net_InSequence(char* sequence, int number) {
    char* ptr;
    char* endptr;
    char* undo = NULL;
    Bool retval = FALSE;
    int i;

    /* not necessary -- routine will work even with null sequence */
    if (!*sequence) {
	return FALSE;
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

                    /* "number" was in the sequence so return TRUE */
                    retval = TRUE;
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

PRIVATE Bool
CookieCompare (net_CookieStruct * cookie1, net_CookieStruct * cookie2) {
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
PRIVATE net_CookieStruct *
NextCookieAfter(net_CookieStruct * cookie, int * cookieNum) {
    XP_List *cookie_list=net_cookie_list;
    net_CookieStruct *cookie_ptr;
    net_CookieStruct *lowestCookie = NULL;
    int localCookieNum = 0;
    int lowestCookieNum;

    while ( (cookie_ptr=(net_CookieStruct *) XP_ListNextObject(cookie_list)) ) {
	if (!cookie || (CookieCompare(cookie_ptr, cookie) > 0)) {
	    if (!lowestCookie ||
		    (CookieCompare(cookie_ptr, lowestCookie) < 0)) {
		lowestCookie = cookie_ptr;
		lowestCookieNum = localCookieNum;
	    }
	}
	localCookieNum++;
    }

    *cookieNum = lowestCookieNum;
    return lowestCookie;
}

/*
 * return a string that has each " of the argument sting
 * replaced with \" so it can be used inside a quoted string
 */
PRIVATE char*
net_FixQuoted(char* s) {
    char * result;
    int count = PL_strlen(s);
    char *quote = s;
    unsigned int i, j;
    while (quote = PL_strchr(quote, '"')) {
        count = count++;
        quote++;
    }
    result = (char*)XP_ALLOC(count + 1);
    for (i=0, j=0; i<PL_strlen(s); i++) {
        if (s[i] == '"') {
            result[i+(j++)] = '\\';
        }
        result[i+j] = s[i];
    }
    result[i+j] = '\0';
    return result;
}

PUBLIC void
COOKIE_DisplayCookieInfoAsHTML()
{
    /* Get rid of any expired cookies now so user doesn't
     * think/see that we're keeping cookies in memory.
     */
    net_lock_cookie_list();
    net_remove_expired_cookies();
    net_unlock_cookie_list();
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
    XP_ASSERT(start >= 0);
    if (start < 0) {
        return nsAutoString("").ToNewCString();
    }
    start += PL_strlen(name); /* get passed the |name| part */
    length = results.Find('|', start) - start;
    results.Mid(value, start, length);
    return value.ToNewCString();
}

PUBLIC void
COOKIE_CookieViewerReturn(nsAutoString results)
{
    XP_List *cookie_ptr;
    XP_List *permission_ptr;
    net_CookieStruct * cookie;
    net_CookieStruct * cookieToDelete = 0;
    net_CookiePermissionStruct * permission;
    net_CookiePermissionStruct * permissionToDelete = 0;
    int cookieNumber;
    int permissionNumber;

    /* step through all cookies and delete those that are in the sequence
     * Note: we can't delete cookie while "cookie_ptr" is pointing to it because
     * that would destroy "cookie_ptr". So we do a lazy deletion
     */
    char * gone = cookie_FindValueInArgs(results, "|goneC|");
    cookieNumber = 0;
    net_lock_cookie_list();
    cookie_ptr = net_cookie_list;
    while ((cookie = (net_CookieStruct *) XP_ListNextObject(cookie_ptr))) {
        if (cookie_InSequence(gone, cookieNumber)) {
            if (cookieToDelete) {
                net_FreeCookie(cookieToDelete);
            }
            cookieToDelete = cookie;
        }
        cookieNumber++;
    }
    if (cookieToDelete) {
        net_FreeCookie(cookieToDelete);
        NET_SaveCookies(NULL);
    }
    net_unlock_cookie_list();
    delete[] gone;

    /* step through all permissions and delete those that are in the sequence
     * Note: we can't delete permission while "permission_ptr" is pointing to it because
     * that would destroy "permission_ptr". So we do a lazy deletion
     */
    gone = cookie_FindValueInArgs(results, "|goneP|");
    permissionNumber = 0;
    net_lock_cookie_permission_list();
    permission_ptr = net_cookie_permission_list;
    while ((permission = (net_CookiePermissionStruct *) XP_ListNextObject(permission_ptr))) {
        if (cookie_InSequence(gone, permissionNumber)) {
            if (permissionToDelete) {
                net_FreeCookiePermission(permissionToDelete, PR_FALSE);
            }
            permissionToDelete = permission;
        }
        permissionNumber++;
    }
    if (permissionToDelete) {
        net_FreeCookiePermission(permissionToDelete, PR_TRUE);
    }
    net_unlock_cookie_permission_list();
    delete[] gone;
}

#define BUFLEN2 5000
#define BREAK '\001'

PUBLIC void
COOKIE_GetCookieListForViewer(nsString& aCookieList)
{
    char *buffer = (char*)XP_ALLOC(BUFLEN2);
    int g = 0, cookieNum;
    XP_List *cookie_ptr;
    net_CookieStruct * cookie;

    net_lock_cookie_list();
    buffer[0] = '\0';
    cookie_ptr = net_cookie_list;
    cookieNum = 0;

    /* generate the list of cookies in alphabetical order */
    cookie = NULL;
    while (cookie = NextCookieAfter(cookie, &cookieNum)) {
        char *fixed_name = net_FixQuoted(cookie->name);
        char *fixed_value = net_FixQuoted(cookie->cookie);
        char *fixed_domain_or_host = net_FixQuoted(cookie->host);
        char *fixed_path = net_FixQuoted(cookie->path);
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
          BREAK, cookie->is_domain ? Domain : Host,
          BREAK, fixed_domain_or_host,
          BREAK, fixed_path,
          BREAK, HG78111 ? Yes : No,
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
    net_unlock_cookie_list();
}

PUBLIC void
COOKIE_GetPermissionListForViewer(nsString& aPermissionList)
{
    char *buffer = (char*)XP_ALLOC(BUFLEN2);
    int g = 0, permissionNum;
    XP_List *permission_ptr;
    net_CookiePermissionStruct * permission;

    net_lock_cookie_permission_list();
    buffer[0] = '\0';
    permission_ptr = net_cookie_permission_list;
    permissionNum = 0;
    while ( (permission=(net_CookiePermissionStruct *) XP_ListNextObject(permission_ptr)) ) {
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
    aPermissionList = buffer;
    PR_FREEIF(buffer);
    net_unlock_cookie_permission_list();
}

/* Hack - Neeti remove this */
/* remove front and back white space
 * modifies the original string
 */
PUBLIC char *
XP_StripLine (char *string)
{
    char * ptr;

	/* remove leading blanks */
    while(*string=='\t' || *string==' ' || *string=='\r' || *string=='\n')
		string++;    

    for(ptr=string; *ptr; ptr++)
		;   /* NULL BODY; Find end of string */

	/* remove trailing blanks */
    for(ptr--; ptr >= string; ptr--) 
	  {
        if(*ptr=='\t' || *ptr==' ' || *ptr=='\r' || *ptr=='\n') 
			*ptr = '\0'; 
        else 
			break;
	  }

    return string;
}

PUBLIC History_entry *
SHIST_GetCurrent(History * hist)
{
/*    MOZ_FUNCTION_STUB; */
    return NULL;
}


/*	Very similar to strdup except it free's too
 */
PUBLIC char * 
NET_SACopy (char **destination, const char *source)
{
	if(*destination)
	  {
	    XP_FREE(*destination);
		*destination = 0;
	  }
    if (! source)
	  {
        *destination = NULL;
	  }
    else 
	  {
        *destination = (char *) PR_Malloc (PL_strlen(source) + 1);
        if (*destination == NULL) 
 	        return(NULL);

        PL_strcpy (*destination, source);
      }
    return *destination;
}

/*  Again like strdup but it concatinates and free's and uses Realloc
*/
PUBLIC char *
NET_SACat (char **destination, const char *source)
{
    if (source && *source)
      {
        if (*destination)
          {
            int length = PL_strlen (*destination);
            *destination = (char *) PR_Realloc (*destination, length + PL_strlen(source) + 1);
            if (*destination == NULL)
            return(NULL);

            PL_strcpy (*destination + length, source);
          }
        else
          {
            *destination = (char *) PR_Malloc (PL_strlen(source) + 1);
            if (*destination == NULL)
                return(NULL);

             PL_strcpy (*destination, source);
          }
      }
    return *destination;
}

MODULE_PRIVATE time_t 
NET_ParseDate(char *date_string)
{
#ifndef USE_OLD_TIME_FUNC
	PRTime prdate;
    time_t date = 0;

//    TRACEMSG(("Parsing date string: %s\n",date_string));

	/* try using PR_ParseTimeString instead
	 */
	
	if(PR_ParseTimeString(date_string, TRUE, &prdate) == PR_SUCCESS)
	  {
        PRInt64 r, u;
        LL_I2L(u, PR_USEC_PER_SEC);
        LL_DIV(r, prdate, u);
        LL_L2I(date, r);
        if (date < 0) {
            date = 0;
        }
  //      TRACEMSG(("Parsed date as GMT: %s\n", asctime(gmtime(&date))));
  //      TRACEMSG(("Parsed date as local: %s\n", ctime(&date)));
	  }
	else
	  {
//		TRACEMSG(("Could not parse date"));
	  }

	return (date);

#else
    struct tm time_info;         /* Points to static tm structure */
    char  *ip;
    char   mname[256];
    time_t rv;

//    TRACEMSG(("Parsing date string: %s\n",date_string));

    memset(&time_info, 0, sizeof(struct tm));

    /* Whatever format we're looking at, it will start with weekday. */
    /* Skip to first space. */
    if(!(ip = PL_strchr(date_string,' ')))
        return 0;
    else
        while(NET_IS_SPACE(*ip))
            ++ip;

	/* make sure that the date is less than 256 
	 * That will keep mname from ever overflowing 
	 */
	if(255 < PL_strlen(ip))
		return(0);

    if(isalpha(*ip)) 
	  {
        /* ctime */
		sscanf(ip, (strstr(ip, "DST") ? "%s %d %d:%d:%d %*s %d"
					: "%s %d %d:%d:%d %d"),
			   mname,
			   &time_info.tm_mday,
			   &time_info.tm_hour,
			   &time_info.tm_min,
			   &time_info.tm_sec,
			   &time_info.tm_year);
		time_info.tm_year -= 1900;
      }
    else if(ip[2] == '-') 
	  {
        /* RFC 850 (normal HTTP) */
        char t[256];
        sscanf(ip,"%s %d:%d:%d", t,
								&time_info.tm_hour,
								&time_info.tm_min,
								&time_info.tm_sec);
        t[2] = '\0';
        time_info.tm_mday = atoi(t);
        t[6] = '\0';
        PL_strcpy(mname,&t[3]);
        time_info.tm_year = atoi(&t[7]);
        /* Prevent wraparound from ambiguity */
        if(time_info.tm_year < 70)
            time_info.tm_year += 100;
		else if(time_info.tm_year > 1900)
			time_info.tm_year -= 1900;
      }
    else 
      {
        /* RFC 822 */
        sscanf(ip,"%d %s %d %d:%d:%d",&time_info.tm_mday,
									mname,
									&time_info.tm_year,
									&time_info.tm_hour,
									&time_info.tm_min,
									&time_info.tm_sec);
	    /* since tm_year is years since 1900 and the year we parsed
 	     * is absolute, we need to subtract 1900 years from it
	     */
	    time_info.tm_year -= 1900;
      }
    time_info.tm_mon = NET_MonthNo(mname);

    if(time_info.tm_mon == -1) /* check for error */
		return(0);


//	TRACEMSG(("Parsed date as: %s\n", asctime(&time_info)));

    rv = mktime(&time_info);
	
	if(time_info.tm_isdst)
		rv -= 3600;

	if(rv == -1)
	  {
//		TRACEMSG(("mktime was unable to resolve date/time: %s\n", asctime(&time_info)));
        return(0);
	  }
	else
	  {
//		TRACEMSG(("Parsed date %s\n", asctime(&time_info)));
//		TRACEMSG(("\tas %s\n", ctime(&rv)));
        return(rv);
	  }
#endif
}


/* Also skip '>' as part of host name */

MODULE_PRIVATE char * 
NET_ParseURL (const char *url, int parts_requested)
{
	char *rv=0,*colon, *slash, *ques_mark, *hash_mark;
	char *atSign, *host, *passwordColon, *gtThan;

	assert(url);

	if(!url)
		return(StrAllocCat(rv, ""));
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
				|| (parts_requested & GET_PASSWORD_PART)
				)
			  {
				if( *(colon+1) == '/' && *(colon+2) == '/')
				   StrAllocCat(rv, "//");
				/* If there's a third slash consider it file:/// and tack on the last slash. */
				if( *(colon+3) == '/' )
					StrAllocCat(rv, "/");
			  }
		  }
	  }


	/* Get the username if one exists */
	if (parts_requested & GET_USERNAME_PART) {
		if (colon 
			&& (*(colon+1) == '/')
			&& (*(colon+2) == '/')
			&& (*(colon+3) != '\0')) {

			if ( (slash = PL_strchr(colon+3, '/')) != NULL)
				*slash = '\0';
			if ( (atSign = PL_strchr(colon+3, '@')) != NULL) {
				*atSign = '\0';
				if ( (passwordColon = PL_strchr(colon+3, ':')) != NULL)
					*passwordColon = '\0';
				StrAllocCat(rv, colon+3);

				/* Get the password if one exists */
				if (parts_requested & GET_PASSWORD_PART) {
					if (passwordColon) {
						StrAllocCat(rv, ":");
						StrAllocCat(rv, passwordColon+1);
					}
				}
				if (parts_requested & GET_HOST_PART)
					StrAllocCat(rv, "@");
				if (passwordColon)
					*passwordColon = ':';
				*atSign = '@';
			}
			if (slash)
				*slash = '/';
		}
	}

	/* Get the host part */
    if (parts_requested & GET_HOST_PART) {
        if(colon) {
			if(*(colon+1) == '/' && *(colon+2) == '/')
			  {
				slash = PL_strchr(colon+3, '/');

				if(slash)
					*slash = '\0';

				if( (atSign = PL_strchr(colon+3, '@')) != NULL)
					host = atSign+1;
				else
					host = colon+3;
				
				ques_mark = PL_strchr(host, '?');

				if(ques_mark)
					*ques_mark = '\0';

				gtThan = PL_strchr(host, '>');

				if (gtThan)
					*gtThan = '\0';

/*
 * Protect systems whose header files forgot to let the client know when
 * gethostbyname would trash their stack.
 */
#ifndef MAXHOSTNAMELEN
#ifdef XP_OS2
#error Managed to get into NET_ParseURL without defining MAXHOSTNAMELEN !!!
#endif
#endif
                /* limit hostnames to within MAXHOSTNAMELEN characters to keep
                 * from crashing
                 */
                if(PL_strlen(host) > MAXHOSTNAMELEN)
                  {
                    char * cp;
                    char old_char;

                    cp = host+MAXHOSTNAMELEN;
                    old_char = *cp;

                    *cp = '\0';

                    StrAllocCat(rv, host);

                    *cp = old_char;
                  }
				else
				  {
					StrAllocCat(rv, host);
				  }

				if(slash)
					*slash = '/';

				if(ques_mark)
					*ques_mark = '?';

				if (gtThan)
					*gtThan = '>';
			  }
          }
	  }
	
	/* Get the path part */
    if (parts_requested & GET_PATH_PART) {
        if(colon) {
            if(*(colon+1) == '/' && *(colon+2) == '/')
              {
				/* skip host part */
                slash = PL_strchr(colon+3, '/');
			  }
			else 
              {
				/* path is right after the colon
				 */
                slash = colon+1;
			  }
                
			if(slash)
			  {
			    ques_mark = PL_strchr(slash, '?');
			    hash_mark = PL_strchr(slash, '#');

			    if(ques_mark)
				    *ques_mark = '\0';
	
			    if(hash_mark)
				    *hash_mark = '\0';

                StrAllocCat(rv, slash);

			    if(ques_mark)
				    *ques_mark = '?';
	
			    if(hash_mark)
				    *hash_mark = '#';
			  }
		  }
      }
		
    if(parts_requested & GET_HASH_PART) {
		hash_mark = PL_strchr(url, '#'); /* returns a const char * */

		if(hash_mark)
		  {
			ques_mark = PL_strchr(hash_mark, '?');

			if(ques_mark)
				*ques_mark = '\0';

            StrAllocCat(rv, hash_mark);

			if(ques_mark)
				*ques_mark = '?';
		  }
	  }

    if(parts_requested & GET_SEARCH_PART) {
        ques_mark = PL_strchr(url, '?'); /* returns a const char * */

        if(ques_mark)
          {
            hash_mark = PL_strchr(ques_mark, '#');

            if(hash_mark)
                *hash_mark = '\0';

            StrAllocCat(rv, ques_mark);

            if(hash_mark)
                *hash_mark = '#';
          }
      }

	/* copy in a null string if nothing was copied in
	 */
	if(!rv)
	   StrAllocCopy(rv, "");
    
	/* XP_OS2_FIX IBM-MAS: long URLS blowout tracemsg buffer, 
		set max to 1900 chars */
  //  TRACEMSG(("mkparse: ParseURL: parsed - %-.1900s",rv));

    return rv;
}

#define REAL_DIALOG 1

PRBool
Cookie_CheckConfirm(char * szMessage, char * szCheckMessage, PRBool* checkValue)
{
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
  res = dialog->ConfirmCheck(message.GetUnicode(), checkMessage.GetUnicode(), checkValue, &retval);
  if (NS_FAILED(res)) {
    *checkValue = 0;
  }
  if (*checkValue!=0 && *checkValue!=1) {
    *checkValue = 0; /* this should never happen but it is happening!!! */
  }
  return retval;

#else

 //   MOZ_FUNCTION_STUB;
    fprintf(stdout, "%c%s  (y/n)?  ", '\007', szMessage); /* \007 is BELL */
    char c;
    XP_Bool result;
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
