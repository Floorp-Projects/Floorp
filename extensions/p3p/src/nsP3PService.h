/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is the Platform for Privacy Preferences.
 *
 * The Initial Developer of the Original Code is
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 2002
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s): Harish Dhurvasula <harishd@netscape.com>
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

#ifndef NS_P3PSERVICE_H__
#define NS_P3PSERVICE_H__

#include "nsICookieConsent.h"
#include "nsIObserver.h"
#include "nsAutoPtr.h"
#include "nsString.h"

class nsCompactPolicy;
class nsIPrefBranch;

class nsP3PService : public nsICookieConsent
                   , public nsIObserver
{
public:
  // nsISupports
  NS_DECL_ISUPPORTS
  // nsICookieConsent
  NS_DECL_NSICOOKIECONSENT
  NS_DECL_NSIOBSERVER

  nsP3PService( );
  virtual ~nsP3PService( );
  
protected:
  void     PrefChanged(nsIPrefBranch *aPrefBranch);
  nsresult ProcessResponseHeader(nsIHttpChannel* aHttpChannel);
  
  nsAutoPtr<nsCompactPolicy> mCompactPolicy;

  /* mCookiesP3PString (below) consists of 8 characters having the following interpretation:
   *   [0]: behavior for first-party cookies when site has no privacy policy
   *   [1]: behavior for third-party cookies when site has no privacy policy
   *   [2]: behavior for first-party cookies when site uses PII with no user consent
   *   [3]: behavior for third-party cookies when site uses PII with no user consent
   *   [4]: behavior for first-party cookies when site uses PII with implicit consent only
   *   [5]: behavior for third-party cookies when site uses PII with implicit consent only
   *   [6]: behavior for first-party cookies when site uses PII with explicit consent
   *   [7]: behavior for third-party cookies when site uses PII with explicit consent
   *
   *   (note: PII = personally identifiable information)
   *
   * each of the eight characters can be one of the following:
   *   'a': accept the cookie
   *   'd': accept the cookie but downgrade it to a session cookie
   *   'r': reject the cookie
   *   'f': flag the cookie to the user
   */
  nsXPIDLCString             mCookiesP3PString;
};

#endif /* nsP3PService_h__  */
