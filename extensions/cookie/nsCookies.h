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

#include "nsString.h"
#include "nsCookie.h"

#include "nsIIOService.h"
#include "nsIURI.h"

#include <time.h>

class nsIPrompt;
class nsIHttpChannel;

extern nsresult COOKIE_Read();
extern nsresult COOKIE_Write(nsIFile* dir);
extern nsresult COOKIE_Notify();
//XXX these should operate on |const char*|
extern char * COOKIE_GetCookie(nsIURI * address);
extern char * COOKIE_GetCookieFromHttp(nsIURI * address, nsIURI * firstAddress);
extern void COOKIE_SetCookieString(nsIURI * cur_url, nsIPrompt * aPrompter, const char * set_cookie_header, nsIHttpChannel* aHttpChannel);
extern void COOKIE_SetCookieStringFromHttp(nsIURI * cur_url, nsIURI * first_url, nsIPrompt *aPrompter, const char * set_cookie_header, char * server_date, nsIHttpChannel* aHttpChannel);
extern void COOKIE_RegisterPrefCallbacks(void);

extern void COOKIE_RemoveSessionCookies();
extern void COOKIE_RemoveAll(void);
extern void COOKIE_DeletePersistentUserData(void);
extern PRInt32 COOKIE_Count();
extern nsresult COOKIE_Enumerate
    (PRInt32 count,
     char **name,
     char **value,
     PRBool *isDomain,
     char ** host,
     char ** path,
     PRBool * isSecure,
     PRUint64 * expires,
     nsCookieStatus * status,
     nsCookiePolicy * policy);
extern void COOKIE_Remove
  (const char* host, const char* name, const char* path, const PRBool blocked);
extern nsresult COOKIE_AddCookie(char *aDomain, char *aPath,
                  char *aName, char *aValue,
                  PRBool aSecure, PRBool aIsDomain,
                  time_t aExpires,
                  nsCookieStatus aStatus, nsCookiePolicy aPolicy);

typedef struct _cookie_CookieStruct {
  char * path;
  char * host;
  char * name;
  char * cookie;
  time_t expires;
  time_t lastAccessed;
  PRBool isSecure;
  PRBool isDomain;   /* is it a domain instead of an absolute host? */
  nsCookieStatus status;
  nsCookiePolicy policy;
} cookie_CookieStruct;

#endif /* COOKIES_H */
