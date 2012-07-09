/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_battery_BatteryManager_h
#define mozilla_dom_battery_BatteryManager_h

#include "nsIDOMBatteryManager.h"
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
                     , public nsIDOMBatteryManager
                     , public BatteryObserver
{
public:
  NS_DECL_ISUPPORTS
  NS_DECL_NSIDOMBATTERYMANAGER
  NS_FORWARD_NSIDOMEVENTTARGET(nsDOMEventTargetHelper::)

  BatteryManager();

  void Init(nsPIDOMWindow *aWindow);
  void Shutdown();

  // For IObserver.
  void Notify(const hal::BatteryInformation& aBatteryInfo);

  NS_DECL_CYCLE_COLLECTION_CLASS_INHERITED(BatteryManager,
                                           nsDOMEventTargetHelper)

  /**
   * Returns whether the battery api is supported (ie. not disabled by the user)
   * @return whether the battery api is supported.
   */
  static bool HasSupport();


private:
  /**
   * Dispatch a trusted non-cancellable and non-bubbling event to itself.
   */
  nsresult DispatchTrustedEventToSelf(const nsAString& aEventName);

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

  NS_DECL_EVENT_HANDLER(levelchange)
  NS_DECL_EVENT_HANDLER(chargingchange)
  NS_DECL_EVENT_HANDLER(chargingtimechange)
  NS_DECL_EVENT_HANDLER(dischargingtimechange)
};

} // namespace battery
} // namespace dom
} // namespace mozilla

#endif // mozilla_dom_battery_BatteryManager_h
