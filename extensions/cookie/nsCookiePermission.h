/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef nsCookiePermission_h__
#define nsCookiePermission_h__

#include "nsICookiePermission.h"
#include "nsIPermissionManager.h"
#include "nsCOMPtr.h"

class nsCookiePermission final : public nsICookiePermission {
 public:
  NS_DECL_ISUPPORTS
  NS_DECL_NSICOOKIEPERMISSION

  // Singleton accessor
  static already_AddRefed<nsICookiePermission> GetOrCreate();

  bool Init();

 private:
  ~nsCookiePermission() = default;

  bool EnsureInitialized() { return (mPermMgr != nullptr) || Init(); };

  nsCOMPtr<nsIPermissionManager> mPermMgr;
};

#endif
