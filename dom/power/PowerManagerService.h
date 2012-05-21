/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#ifndef mozilla_dom_power_PowerManagerService_h
#define mozilla_dom_power_PowerManagerService_h

#include "nsCOMPtr.h"
#include "nsDataHashtable.h"
#include "nsHashKeys.h"
#include "nsTArray.h"
#include "nsIPowerManagerService.h"
#include "mozilla/Observer.h"
#include "Types.h"

namespace mozilla {
namespace dom {
namespace power {

class PowerManagerService
  : public nsIPowerManagerService
  , public WakeLockObserver
{
public:
  NS_DECL_ISUPPORTS
  NS_DECL_NSIPOWERMANAGERSERVICE

  static already_AddRefed<nsIPowerManagerService> GetInstance();

  void Init();

  // Implement WakeLockObserver
  void Notify(const hal::WakeLockInformation& aWakeLockInfo);

private:

  ~PowerManagerService();

  void ComputeWakeLockState(const hal::WakeLockInformation& aWakeLockInfo,
                            nsAString &aState);

  static nsRefPtr<PowerManagerService> sSingleton;

  nsTArray<nsCOMPtr<nsIDOMMozWakeLockListener> > mWakeLockListeners;
};

} // namespace power
} // namespace dom
} // namespace mozilla

#endif // mozilla_dom_power_PowerManagerService_h
