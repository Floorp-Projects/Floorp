/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef nsCookiePermission_h__
#define nsCookiePermission_h__

#include "nsICookiePermission.h"
#include "nsIPermissionManager.h"
#include "nsIObserver.h"
#include "nsCOMPtr.h"
#include "prlong.h"
#include "nsIPrivateBrowsingService.h"

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
    , mCookiesAlwaysAcceptSession(false)
    {}
  virtual ~nsCookiePermission() {}

  bool Init();
  void PrefChanged(nsIPrefBranch *, const char *);

private:
  bool EnsureInitialized() { return mPermMgr != NULL || Init(); };
  bool InPrivateBrowsing();

  nsCOMPtr<nsIPermissionManager> mPermMgr;
  nsCOMPtr<nsIPrivateBrowsingService> mPBService;

  PRInt64      mCookiesLifetimeSec;            // lifetime limit specified in seconds
  PRUint8      mCookiesLifetimePolicy;         // pref for how long cookies are stored
  bool mCookiesAlwaysAcceptSession;    // don't prompt for session cookies
};

// {EF565D0A-AB9A-4A13-9160-0644CDFD859A}
#define NS_COOKIEPERMISSION_CID \
 {0xEF565D0A, 0xAB9A, 0x4A13, {0x91, 0x60, 0x06, 0x44, 0xcd, 0xfd, 0x85, 0x9a }}

#endif
