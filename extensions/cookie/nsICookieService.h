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

#ifndef nsICookieService_h__
#define nsICookieService_h__

#include "nsISupports.h"
#include "nsIURL.h"
#include "nsString.h"
#include "nsIDocument.h"

// {AB397774-12D3-11d3-8AD1-00105A1B8860}
#define NS_COOKIESERVICE_CID \
{ 0xab397774, 0x12d3, 0x11d3, { 0x8a, 0xd1, 0x0, 0x10, 0x5a, 0x1b, 0x88, 0x60 } }

/* ContractID prefixes for Cookie DLL registration. */
#define NS_COOKIESERVICE_CONTRACTID                           "@mozilla.org/cookie;1"

// {AB397772-12D3-11d3-8AD1-00105A1B8860}
#define NS_ICOOKIESERVICE_IID \
{ 0xab397772, 0x12d3, 0x11d3, { 0x8a, 0xd1, 0x0, 0x10, 0x5a, 0x1b, 0x88, 0x60 } }

class nsIPrompt;

class nsICookieService : public nsISupports {
public:
  
  static const nsIID& GetIID() { static nsIID iid = NS_ICOOKIESERVICE_IID; return iid; }

  /*
   * Get the complete cookie string associated with the URL
   *
   * @param aURL The URL for which to get the cookie string
   * @param aCookie The string object which will hold the result
   * @return Returns NS_OK if successful, or NS_FALSE if an error occurred.
   */
  NS_IMETHOD GetCookieString(nsIURI *aURL, nsString& aCookie)=0;

  /*
   * Get the complete cookie string associated with the URL
   *
   * @param aURL The URL for which to get the cookie string
   * @param aFirstURL The URL which the user typed in or clicked on
   * @param aCookie The string object which will hold the result
   * @return Returns NS_OK if successful, or NS_FALSE if an error occurred.
   */
  NS_IMETHOD GetCookieStringFromHTTP(nsIURI *aURL, nsIURI *aFirstURL, nsString& aCookie)=0;

  /*
   * Set the cookie string associated with the URL
   *
   * @param aURL The URL for which to set the cookie string
   * @param aCookie The string to set
   * @return Returns NS_OK if successful, or NS_FALSE if an error occurred.
   */
  NS_IMETHOD SetCookieString(nsIURI *aURL, nsIDocument* aDoc, const nsString& aCookie)=0;

  /*
   * Set the cookie string and expires associated with the URL
   *
   * @param aURL The URL for which to set the cookie string
   * @param aFirstURL The URL which the user typed in or clicked on
   * @param aPrompter The nsIPrompt implementation to use. If null, a default
   *                  will be used.
   * @param aCookie The char * string to set
   * @param aExpires The expiry information of the cookie
   * @return Returns NS_OK if successful, or NS_FALSE if an error occurred.
   */
  NS_IMETHOD SetCookieStringFromHttp(nsIURI *aURL, nsIURI *aFirstURL, nsIPrompt *aPrompter, const char *aCookie, const char *aExpires)=0;

  /* 
   * Blows away all permissions currently in the cookie permissions list,
   * and then blows away all cookies currently in the cookie list.
   */
  NS_IMETHOD Cookie_RemoveAllCookies(void)=0;

  /*
   * Interface routines for cookie viewer
   */
  NS_IMETHOD Cookie_CookieViewerReturn(nsAutoString results)=0;
  NS_IMETHOD Cookie_GetCookieListForViewer(nsString& aCookieList)=0;
  NS_IMETHOD Cookie_GetPermissionListForViewer(nsString& aPermissionList, PRInt32 type)=0;
  NS_IMETHOD Image_Block(nsAutoString imageURL)=0;
  NS_IMETHOD Permission_Add(nsString imageURL, PRBool permission, PRInt32 type)=0;
  NS_IMETHOD Image_CheckForPermission
    (char * hostname, char * firstHostname, PRBool &permission)=0;

  /*
   * Specifies whether cookies will be accepted or not. 
   * XXX This method can be refined to return more specific information
   * (i.e. whether we accept foreign cookies or not, etc.) if necessary.
   */
  NS_IMETHOD CookieEnabled(PRBool* aEnabled)=0;
};

#endif /* nsICookieService_h__ */
