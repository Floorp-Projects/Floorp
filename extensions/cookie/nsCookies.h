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

#ifdef _IMPL_NS_COOKIE
#define NS_COOKIE NS_EXPORT
#else
#define NS_COOKIE NS_IMPORT
#endif


class nsIPrompt;

extern nsresult COOKIE_Read();
extern nsresult COOKIE_Write();
extern char * COOKIE_GetCookie(char * address);
extern char * COOKIE_GetCookieFromHttp(char * address, char * firstAddress);
extern void COOKIE_SetCookieString(char * cur_url, nsIPrompt *aPrompter, const char * set_cookie_header);
extern void COOKIE_SetCookieStringFromHttp(char * cur_url, char * first_url, nsIPrompt *aPRompter, char * set_cookie_header, char * server_date);
extern void COOKIE_RegisterPrefCallbacks(void);

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
     PRUint64 * expires);
extern void COOKIE_Remove
  (const char* host, const char* name, const char* path, const PRBool permanent);

#endif /* COOKIES_H */
