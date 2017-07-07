/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#ifndef mozilla_dom_power_PowerManagerService_h
#define mozilla_dom_power_PowerManagerService_h

#include "nsCOMPtr.h"
#include "nsDataHashtable.h"
#include "nsHashKeys.h"
#include "nsTArray.h"
#include "nsIDOMWakeLockListener.h"
#include "nsIPowerManagerService.h"
#include "mozilla/Observer.h"
#include "Types.h"
#include "mozilla/StaticPtr.h"
#include "mozilla/dom/WakeLock.h"

namespace mozilla {
namespace dom {

class ContentParent;

namespace power {

class PowerManagerService
  : public nsIPowerManagerService
  , public WakeLockObserver
{
public:
  NS_DECL_ISUPPORTS
  NS_DECL_NSIPOWERMANAGERSERVICE

  PowerManagerService()
    : mWatchdogTimeoutSecs(0)
  {}

  static already_AddRefed<PowerManagerService> GetInstance();

  void Init();

  // Implement WakeLockObserver
  void Notify(const hal::WakeLockInformation& aWakeLockInfo) override;

  /**
   * Acquire a wake lock on behalf of a given process (aContentParent).
   *
   * This method stands in contrast to nsIPowerManagerService::NewWakeLock,
   * which acquires a wake lock on behalf of the /current/ process.
   *
   * NewWakeLockOnBehalfOfProcess is different from NewWakeLock in that
   *
   *  - The wake lock unlocks itself if the /given/ process dies, and
   *  - The /given/ process shows up in WakeLockInfo::lockingProcesses.
   *
   */
  already_AddRefed<WakeLock>
  NewWakeLockOnBehalfOfProcess(const nsAString& aTopic,
                               ContentParent* aContentParent);

  already_AddRefed<WakeLock>
  NewWakeLock(const nsAString& aTopic, nsPIDOMWindowInner* aWindow,
              mozilla::ErrorResult& aRv);

private:

  ~PowerManagerService();

  void ComputeWakeLockState(const hal::WakeLockInformation& aWakeLockInfo,
                            nsAString &aState);

  void SyncProfile();

  static StaticRefPtr<PowerManagerService> sSingleton;

  nsTArray<nsCOMPtr<nsIDOMMozWakeLockListener>> mWakeLockListeners;

  int32_t mWatchdogTimeoutSecs;
};

} // namespace power
} // namespace dom
} // namespace mozilla

#endif // mozilla_dom_power_PowerManagerService_h
