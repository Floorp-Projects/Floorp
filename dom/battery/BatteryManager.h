/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
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
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is Mozilla Foundation
 * Portions created by the Initial Developer are Copyright (C) 2011
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Mounir Lamouri <mounir.lamouri@mozilla.com> (Original Author)
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either of the GNU General Public License Version 2 or later (the "GPL"),
 * or the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
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

#ifndef mozilla_dom_battery_BatteryManager_h
#define mozilla_dom_battery_BatteryManager_h

#include "nsIDOMBatteryManager.h"
#include "nsDOMEventTargetHelper.h"
#include "nsCycleCollectionParticipant.h"
#include "mozilla/Observer.h"
#include "Types.h"

namespace mozilla {

namespace hal {
class BatteryInformation;
} // namespace hal

namespace dom {
namespace battery {

class BatteryManager : public nsIDOMBatteryManager
                     , public nsDOMEventTargetHelper
                     , public BatteryObserver
{
public:
  NS_DECL_ISUPPORTS
  NS_DECL_NSIDOMBATTERYMANAGER
  NS_FORWARD_NSIDOMEVENTTARGET(nsDOMEventTargetHelper::)

  BatteryManager();
  virtual ~BatteryManager();

  void Init();
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

  nsRefPtr<nsDOMEventListenerWrapper> mOnLevelChangeListener;
  nsRefPtr<nsDOMEventListenerWrapper> mOnChargingChangeListener;
  nsRefPtr<nsDOMEventListenerWrapper> mOnDischargingTimeChangeListener;
  nsRefPtr<nsDOMEventListenerWrapper> mOnChargingTimeChangeListener;
};

} // namespace battery
} // namespace dom
} // namespace mozilla

#endif // mozilla_dom_battery_BatteryManager_h
