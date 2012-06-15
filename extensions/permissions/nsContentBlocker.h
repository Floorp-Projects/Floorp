/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#ifndef nsContentBlocker_h__
#define nsContentBlocker_h__

#include "nsIContentPolicy.h"
#include "nsIObserver.h"
#include "nsWeakReference.h"
#include "nsIPermissionManager.h"
#include "nsIPrefBranch.h"
#include "mozilla/Attributes.h"

class nsIPrefBranch;

////////////////////////////////////////////////////////////////////////////////

class nsContentBlocker MOZ_FINAL : public nsIContentPolicy,
                                   public nsIObserver,
                                   public nsSupportsWeakReference
{
public:

  // nsISupports
  NS_DECL_ISUPPORTS
  NS_DECL_NSICONTENTPOLICY
  NS_DECL_NSIOBSERVER

  nsContentBlocker();
  nsresult Init();

private:
  ~nsContentBlocker() {}

  void PrefChanged(nsIPrefBranch *, const char *);
  nsresult TestPermission(nsIURI *aCurrentURI,
                          nsIURI *aFirstURI,
                          PRInt32 aContentType,
                          bool *aPermission,
                          bool *aFromPrefs);

  nsCOMPtr<nsIPermissionManager> mPermissionManager;
  nsCOMPtr<nsIPrefBranch> mPrefBranchInternal;
  static PRUint8 mBehaviorPref[];
};

#define NS_CONTENTBLOCKER_CID \
{ 0x4ca6b67b, 0x5cc7, 0x4e71, \
  { 0xa9, 0x8a, 0x97, 0xaf, 0x1c, 0x13, 0x48, 0x62 } }

#define NS_CONTENTBLOCKER_CONTRACTID "@mozilla.org/permissions/contentblocker;1"

#endif /* nsContentBlocker_h__ */
