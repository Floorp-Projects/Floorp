/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/*
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

#ifndef nsCookieService_h__
#define nsCookieService_h__

#include "nsICookieService.h"

// {AB397774-12D3-11d3-8AD1-00105A1B8860}
#define NS_COOKIESERVICE_CID \
{ 0xab397774, 0x12d3, 0x11d3, { 0x8a, 0xd1, 0x0, 0x10, 0x5a, 0x1b, 0x88, 0x60 } }

////////////////////////////////////////////////////////////////////////////////

class nsCookieService : public nsICookieService {
public:

  // nsISupports
  NS_DECL_ISUPPORTS

  NS_IMETHOD GetCookieString(nsIURI *aURL, nsString& aCookie);
  NS_IMETHOD GetCookieStringFromHTTP(nsIURI *aURL, nsIURI *aFirstURL, nsString& aCookie);
  NS_IMETHOD SetCookieString(nsIURI *aURL, const nsString& aCookie);
  NS_IMETHOD SetCookieStringFromHttp(nsIURI *aURL, nsIURI *aFirstURL, const char *aCookie, const char *aExpires);
  NS_IMETHOD Cookie_RemoveAllCookies(void);
  NS_IMETHOD Cookie_CookieViewerReturn(nsAutoString results);
  NS_IMETHOD Cookie_GetCookieListForViewer(nsString& aCookieList);
  NS_IMETHOD Cookie_GetPermissionListForViewer(nsString& aPermissionList);
  NS_IMETHOD CookieEnabled(PRBool* aEnabled);

  nsCookieService();
  virtual ~nsCookieService(void);
  nsresult Init();
  
protected:
  PRBool   mInitted;
};

#endif /* nsCookieService_h__ */
