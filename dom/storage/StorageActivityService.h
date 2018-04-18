/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_StorageActivityService_h
#define mozilla_dom_StorageActivityService_h

#include "nsDataHashtable.h"
#include "nsIStorageActivityService.h"
#include "nsITimer.h"
#include "nsWeakReference.h"

namespace mozilla {

namespace ipc {
class PrincipalInfo;
} // ipc

namespace dom {

class StorageActivityService final : public nsIStorageActivityService
                                   , public nsIObserver
                                   , public nsITimerCallback
                                   , public nsSupportsWeakReference
{
public:
  NS_DECL_ISUPPORTS
  NS_DECL_NSISTORAGEACTIVITYSERVICE
  NS_DECL_NSIOBSERVER
  NS_DECL_NSITIMERCALLBACK

  // Main-thread only.
  static void
  SendActivity(nsIPrincipal* aPrincipal);

  // Thread-safe.
  static void
  SendActivity(const mozilla::ipc::PrincipalInfo& aPrincipalInfo);

  // Thread-safe but for parent process only!
  static void
  SendActivity(const nsACString& aOrigin);

  // Used by XPCOM. Don't use it, use SendActivity() instead.
  static already_AddRefed<StorageActivityService>
  GetOrCreate();

private:
  StorageActivityService();
  ~StorageActivityService();

  void
  SendActivityInternal(nsIPrincipal* aPrincipal);

  void
  SendActivityInternal(const nsACString& aOrigin);

  void
  SendActivityToParent(nsIPrincipal* aPrincipal);

  void
  MaybeStartTimer();

  void
  MaybeStopTimer();

  // Activities grouped by origin (+OriginAttributes).
  nsDataHashtable<nsCStringHashKey, PRTime> mActivities;

  nsCOMPtr<nsITimer> mTimer;
};

} // namespace dom
} // namespace mozilla

#endif // mozilla_dom_StorageActivityService_h
