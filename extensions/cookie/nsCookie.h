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

extern char * COOKIE_GetCookie(char * address);
extern void COOKIE_SetCookieString(char * cur_url, char * set_cookie_header);
extern int COOKIE_ReadCookies();
extern void COOKIE_RegisterCookiePrefCallbacks(void);
extern void COOKIE_RemoveAllCookies(void);
extern void COOKIE_SetCookieStringFromHttp(char * cur_url, char * set_cookie_header, char * server_date);
extern void COOKIE_GetCookieListForViewer (nsString& aCookieList);
extern void COOKIE_GetPermissionListForViewer (nsString& aPermissionList);
extern void COOKIE_CookieViewerReturn(nsAutoString results);

#endif /* COOKIES_H */
