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

#ifndef COOKIES_H
#define COOKIES_H

#include "nsCookie.h"
#include "nsString.h"
#include "nsInt64.h"
#include "prtime.h"
#include "nsVoidArray.h"
#include "nsIURI.h"
#include "nsIHttpChannel.h"
#include "nsIDOMWindowInternal.h"

// XXX these casts and constructs are horrible, but our nsInt64/nsTime
// classes are lacking so we need them for now. see bug 198694.
#define USEC_PER_SEC   (nsInt64(PRInt64(1000000)))
#define NOW_IN_SECONDS (nsInt64(PR_Now()) / USEC_PER_SEC)

// main cookie storage struct
typedef struct _cookie_CookieStruct {
  nsCString path;
  nsCString host;
  nsCString name;
  nsCString cookie;
  PRInt64 expires;
  PRInt64 lastAccessed;
  PRPackedBool isSession;
  PRPackedBool isSecure;
  PRPackedBool isDomain;
  nsCookieStatus status;
  nsCookiePolicy policy;
} cookie_CookieStruct;

// define logging macros for convenience
#define SET_COOKIE PR_TRUE
#define GET_COOKIE PR_FALSE

// logging handlers
#ifdef MOZ_LOGGING
// in order to do logging, the following environment variables need to be set:
//
//    set NSPR_LOG_MODULES=cookie:3 -- shows rejected cookies
//    set NSPR_LOG_MODULES=cookie:4 -- shows accepted and rejected cookies
//    set NSPR_LOG_FILE=c:\cookie.log
//
// this next define has to appear before the include of prlog.h
#define FORCE_PR_LOG /* Allow logging in the release build */
#include "prlog.h"
#endif

#ifdef PR_LOGGING
#define COOKIE_LOGFAILURE(a, b, c, d) cookie_LogFailure(a, b, c, d)
#define COOKIE_LOGSUCCESS(a, b, c, d) cookie_LogSuccess(a, b, c, d)

extern void cookie_LogFailure(PRBool aSetCookie, nsIURI *aHostURI, const char *aCookieString, const char *aReason);
extern void cookie_LogSuccess(PRBool aSetCookie, nsIURI *aHostURI, const char *aCookieString, cookie_CookieStruct *aCookie);
extern void cookie_LogFailure(PRBool aSetCookie, nsIURI *aHostURI, const nsAFlatCString &aCookieString, const char *aReason);
extern void cookie_LogSuccess(PRBool aSetCookie, nsIURI *aHostURI, const nsAFlatCString &aCookieString, cookie_CookieStruct *aCookie);
#else
#define COOKIE_LOGFAILURE(a, b, c, d) /* nothing */
#define COOKIE_LOGSUCCESS(a, b, c, d) /* nothing */
#endif

// function prototypes
extern nsresult COOKIE_Read();
extern nsresult COOKIE_Write();
extern void COOKIE_RemoveExpiredCookies(nsInt64 aCurrentTime, PRInt32 &aOldestPosition);
extern char * COOKIE_GetCookie(nsIURI *aHostURI, nsIURI *aFirstURI);
extern void COOKIE_SetCookie(nsIURI *aHostURI, nsIURI *aFirstURI, nsIPrompt *aPrompt, const char *aCookieHeader, const char *aServerTime, nsIHttpChannel *aHttpChannel);
extern void COOKIE_RemoveAll();
extern void COOKIE_Remove(const nsACString &host, const nsACString &name, const nsACString &path, PRBool blocked);
extern nsresult COOKIE_Add(cookie_CookieStruct *aCookie, nsInt64 aCurrentTime, nsIURI *aHostURI, const char *aCookieHeader);
extern already_AddRefed<nsICookie> COOKIE_ChangeFormat(cookie_CookieStruct *aCookie);

// constants & variables
extern nsVoidArray *sCookieList;

#endif /* COOKIES_H */
