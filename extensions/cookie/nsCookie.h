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
 */

#ifndef COOKIES_H
#define COOKIES_H

#include "nscore.h"
#include "nsError.h"
#include "nsString.h"

#ifdef _IMPL_NS_COOKIE
#define NS_COOKIE NS_EXPORT
#else
#define NS_COOKIE NS_IMPORT
#endif


typedef enum {
  COOKIE_Accept,
  COOKIE_DontAcceptForeign,
  COOKIE_DontUse
} COOKIE_BehaviorEnum;

class nsIPrompt;

extern char * COOKIE_GetCookie(char * address);
extern char * COOKIE_GetCookieFromHttp(char * address, char * firstAddress);
extern void COOKIE_SetCookieString(char * cur_url, nsIPrompt *aPrompter, char * set_cookie_header);
extern int COOKIE_ReadCookies();
extern void COOKIE_RegisterCookiePrefCallbacks(void);
extern void COOKIE_RemoveAllCookies(void);
extern void COOKIE_SetCookieStringFromHttp(char * cur_url, char * first_url, nsIPrompt *aPRompter, char * set_cookie_header, char * server_date);
extern void COOKIE_GetCookieListForViewer (nsString& aCookieList);
extern void COOKIE_GetPermissionListForViewer (nsString& aPermissionList, PRInt32 type);
extern void COOKIE_CookieViewerReturn(nsAutoString results);
extern COOKIE_BehaviorEnum COOKIE_GetBehaviorPref();
extern void Image_Block(nsString imageURL);
extern void Permission_Add(nsString imageURL, PRBool permission, PRInt32 type);
extern nsresult Image_CheckForPermission
  (char * hostname, char * firstHostname, PRBool &permission);
#endif /* COOKIES_H */
