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
 * The Original Code is cookie manager code.
 *
 * The Initial Developer of the Original Code is
 * Michiel van Leeuwen (mvl@exedo.nl).
 * Portions created by the Initial Developer are Copyright (C) 2003
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Darin Fisher <darin@meer.net>
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

#ifndef nsCookiePermission_h__
#define nsCookiePermission_h__

#include "nsICookiePermission.h"
#include "nsIPermissionManager.h"
#include "nsIObserver.h"
#include "nsCOMPtr.h"
#include "nsInt64.h"
#include "prlong.h"

class nsIPrefBranch;

class nsCookiePermission : public nsICookiePermission
                         , public nsIObserver
{
public:
  NS_DECL_ISUPPORTS
  NS_DECL_NSICOOKIEPERMISSION
  NS_DECL_NSIOBSERVER

  nsCookiePermission() 
    : mCookiesLifetimeSec(LL_MAXINT)
    , mCookiesLifetimePolicy(0) // ACCEPT_NORMALLY
    , mCookiesAlwaysAcceptSession(PR_FALSE)
#ifdef MOZ_MAIL_NEWS
    , mCookiesDisabledForMailNews(PR_TRUE)
#endif
    {}
  virtual ~nsCookiePermission() {}

  nsresult Init();
  void     PrefChanged(nsIPrefBranch *, const char *);

private:
  nsCOMPtr<nsIPermissionManager> mPermMgr;

  nsInt64      mCookiesLifetimeSec;            // lifetime limit specified in seconds
  PRUint8      mCookiesLifetimePolicy;         // pref for how long cookies are stored
  PRPackedBool mCookiesAlwaysAcceptSession;    // don't prompt for session cookies
#ifdef MOZ_MAIL_NEWS
  PRPackedBool mCookiesDisabledForMailNews;
#endif

};

// {CE002B28-92B7-4701-8621-CC925866FB87}
#define NS_COOKIEPERMISSION_CID \
 {0xEF565D0A, 0xAB9A, 0x4A13, {0x91, 0x60, 0x06, 0x44, 0xcd, 0xfd, 0x85, 0x9a }}

#endif
