/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* ----- BEGIN LICENSE BLOCK -----
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public
 * License Version 1.1 (the "License"); you may not use this file
 * except in compliance with the License. You may obtain a copy of
 * the License at http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS
 * IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
 * implied. See the License for the specific language governing
 * rights and limitations under the License.
 *
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is Netscape Communications Corporation.
 * Portions created by Netscape Communications Corporation are
 * Copyright (C) 1998 Netscape Communications Corporation. All
 * Rights Reserved.
 *
 * Contributor(s):
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either of the GNU General Public License Version 2 or later (the "GPL"),
 * or the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the LGPL or the GPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ----- END LICENSE BLOCK ----- */

#ifndef nsCookieHTTPNotify_h___
#define nsCookieHTTPNotify_h___

#include "nsIHttpNotify.h"
#include "nsICookieService.h"
#include "nsIComponentManager.h"

// {6BC1F522-1F45-11d3-8AD4-00105A1B8860}
#define NS_COOKIEHTTPNOTIFY_CID \
{ 0x6bc1f522, 0x1f45, 0x11d3, { 0x8a, 0xd4, 0x0, 0x10, 0x5a, 0x1b, 0x88, 0x60 } }

#define NS_COOKIEHTTPNOTIFY_CONTRACTID "@mozilla.org/cookie-notifier;1"
#define NS_COOKIEHTTPNOTIFY_CLASSNAME "Cookie Notifier"

struct nsModuleComponentInfo;   // forward declaration

class nsCookieHTTPNotify : public nsIHttpNotify {
public:

  // nsISupports
  NS_DECL_ISUPPORTS

  // Init method
  NS_IMETHOD Init();

  // nsIHttpNotify methods:
  NS_DECL_NSIHTTPNOTIFY
   
  // nsCookieHTTPNotify methods:
  nsCookieHTTPNotify();
  virtual ~nsCookieHTTPNotify();

  static nsresult Create(nsISupports *aOuter, REFNSIID aIID, void **aResult);
  static NS_METHOD RegisterProc(nsIComponentManager *aCompMgr,
                                nsIFile *aPath,
                                const char *registryLocation,
                                const char *componentType,
                                const nsModuleComponentInfo *info);
  static NS_METHOD UnregisterProc(nsIComponentManager *aCompMgr,
                                  nsIFile *aPath,
                                  const char *registryLocation,
                                  const nsModuleComponentInfo *info);

private:
    nsCOMPtr<nsICookieService> mCookieService;

    NS_IMETHOD SetupCookieService();
};
#endif /* nsCookieHTTPNotify_h___ */
