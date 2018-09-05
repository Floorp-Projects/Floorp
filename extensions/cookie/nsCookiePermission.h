/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef nsCookiePermission_h__
#define nsCookiePermission_h__

#include "nsICookiePermission.h"
#include "nsIPermissionManager.h"
#include "nsIObserver.h"
#include "nsCOMPtr.h"
#include "mozIThirdPartyUtil.h"

class nsIPrefBranch;

class nsCookiePermission final : public nsICookiePermission
                               , public nsIObserver
{
public:
  NS_DECL_ISUPPORTS
  NS_DECL_NSICOOKIEPERMISSION
  NS_DECL_NSIOBSERVER

  // Singleton accessor
  static already_AddRefed<nsICookiePermission> GetOrCreate();
  static void Shutdown();

  bool Init();
  void PrefChanged(nsIPrefBranch *, const char *);

private:
  nsCookiePermission()
    : mCookiesLifetimePolicy(0) // ACCEPT_NORMALLY
    {}
  virtual ~nsCookiePermission() {}

  bool EnsureInitialized() { return (mPermMgr != nullptr && mThirdPartyUtil != nullptr) || Init(); };

  nsCOMPtr<nsIPermissionManager> mPermMgr;
  nsCOMPtr<mozIThirdPartyUtil> mThirdPartyUtil;

  uint8_t      mCookiesLifetimePolicy;         // pref for how long cookies are stored
};

#endif
