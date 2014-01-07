/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_battery_BatteryManager_h
#define mozilla_dom_battery_BatteryManager_h

#include "nsDOMEventTargetHelper.h"
#include "nsCycleCollectionParticipant.h"
#include "mozilla/Observer.h"
#include "Types.h"
#include "nsDOMEventTargetHelper.h"

class nsPIDOMWindow;
class nsIScriptContext;

namespace mozilla {

namespace hal {
class BatteryInformation;
} // namespace hal

namespace dom {
namespace battery {

class BatteryManager : public nsDOMEventTargetHelper
                     , public BatteryObserver
{
public:
  BatteryManager(nsPIDOMWindow* aWindow);

  void Init();
  void Shutdown();

  // For IObserver.
  void Notify(const hal::BatteryInformation& aBatteryInfo);

  /**
   * Returns whether the battery api is supported (ie. not disabled by the user)
   * @return whether the battery api is supported.
   */
  static bool HasSupport();

  /**
   * WebIDL Interface
   */

  nsPIDOMWindow* GetParentObject() const
  {
     return GetOwner();
  }

  virtual JSObject* WrapObject(JSContext* aCx,
                               JS::Handle<JSObject*> aScope) MOZ_OVERRIDE;

  bool Charging() const
  {
    return mCharging;
  }

  double ChargingTime() const;

  double DischargingTime() const;

  double Level() const
  {
    return mLevel;
  }

  IMPL_EVENT_HANDLER(chargingchange)
  IMPL_EVENT_HANDLER(chargingtimechange)
  IMPL_EVENT_HANDLER(dischargingtimechange)
  IMPL_EVENT_HANDLER(levelchange)

private:
  /**
   * Update the battery information stored in the battery manager object using
   * a battery information object.
   */
  void UpdateFromBatteryInfo(const hal::BatteryInformation& aBatteryInfo);

  double mLevel;
  bool   mCharging;
  /**
   * Represents the discharging time or the charging time, dpending on the
   * current battery status (charging or not).
   */
  double mRemainingTime;
};

} // namespace battery
} // namespace dom
} // namespace mozilla

#endif // mozilla_dom_battery_BatteryManager_h
