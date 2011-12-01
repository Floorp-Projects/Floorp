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

#include <limits>
#include "mozilla/Hal.h"
#include "BatteryManager.h"
#include "nsIDOMClassInfo.h"
#include "Constants.h"
#include "nsDOMEvent.h"
#include "mozilla/Preferences.h"

/**
 * We have to use macros here because our leak analysis tool things we are
 * leaking strings when we have |static const nsString|. Sad :(
 */
#define LEVELCHANGE_EVENT_NAME           NS_LITERAL_STRING("levelchange")
#define CHARGINGCHANGE_EVENT_NAME        NS_LITERAL_STRING("chargingchange")
#define DISCHARGINGTIMECHANGE_EVENT_NAME NS_LITERAL_STRING("dischargingtimechange")
#define CHARGINGTIMECHANGE_EVENT_NAME    NS_LITERAL_STRING("chargingtimechange")

DOMCI_DATA(MozBatteryManager, mozilla::dom::battery::BatteryManager)

namespace mozilla {
namespace dom {
namespace battery {

NS_IMPL_CYCLE_COLLECTION_CLASS(BatteryManager)

NS_IMPL_CYCLE_COLLECTION_TRAVERSE_BEGIN_INHERITED(BatteryManager,
                                                  nsDOMEventTargetWrapperCache)
  NS_CYCLE_COLLECTION_TRAVERSE_EVENT_HANDLER(levelchange)
  NS_CYCLE_COLLECTION_TRAVERSE_EVENT_HANDLER(chargingchange)
  NS_CYCLE_COLLECTION_TRAVERSE_EVENT_HANDLER(chargingtimechange)
  NS_CYCLE_COLLECTION_TRAVERSE_EVENT_HANDLER(dischargingtimechange)
NS_IMPL_CYCLE_COLLECTION_TRAVERSE_END

NS_IMPL_CYCLE_COLLECTION_UNLINK_BEGIN_INHERITED(BatteryManager,
                                                nsDOMEventTargetWrapperCache)
  NS_CYCLE_COLLECTION_UNLINK_EVENT_HANDLER(levelchange)
  NS_CYCLE_COLLECTION_UNLINK_EVENT_HANDLER(chargingchange)
  NS_CYCLE_COLLECTION_UNLINK_EVENT_HANDLER(chargingtimechange)
  NS_CYCLE_COLLECTION_UNLINK_EVENT_HANDLER(dischargingtimechange)
NS_IMPL_CYCLE_COLLECTION_UNLINK_END

NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION_INHERITED(BatteryManager)
  NS_INTERFACE_MAP_ENTRY(nsIDOMMozBatteryManager)
  NS_DOM_INTERFACE_MAP_ENTRY_CLASSINFO(MozBatteryManager)
NS_INTERFACE_MAP_END_INHERITING(nsDOMEventTargetWrapperCache)

NS_IMPL_ADDREF_INHERITED(BatteryManager, nsDOMEventTargetWrapperCache)
NS_IMPL_RELEASE_INHERITED(BatteryManager, nsDOMEventTargetWrapperCache)

BatteryManager::BatteryManager()
  : mLevel(kDefaultLevel)
  , mCharging(kDefaultCharging)
  , mRemainingTime(kDefaultRemainingTime)
{
}

BatteryManager::~BatteryManager()
{
  if (mListenerManager) {
    mListenerManager->Disconnect();
  }
}

void
BatteryManager::Init(nsPIDOMWindow *aWindow, nsIScriptContext* aScriptContext)
{
  // Those vars come from nsDOMEventTargetHelper.
  mOwner = aWindow;
  mScriptContext = aScriptContext;

  hal::RegisterBatteryObserver(this);

  hal::BatteryInformation* batteryInfo = new hal::BatteryInformation();
  hal::GetCurrentBatteryInformation(batteryInfo);

  UpdateFromBatteryInfo(*batteryInfo);

  delete batteryInfo;
}

void
BatteryManager::Shutdown()
{
  hal::UnregisterBatteryObserver(this);
}

NS_IMETHODIMP
BatteryManager::GetCharging(bool* aCharging)
{
  *aCharging = mCharging;

  return NS_OK;
}

NS_IMETHODIMP
BatteryManager::GetLevel(double* aLevel)
{
  *aLevel = mLevel;

  return NS_OK;
}

NS_IMETHODIMP
BatteryManager::GetDischargingTime(double* aDischargingTime)
{
  if (mCharging || mRemainingTime == kUnknownRemainingTime) {
    *aDischargingTime = std::numeric_limits<double>::infinity();
    return NS_OK;
  }

  *aDischargingTime = mRemainingTime;

  return NS_OK;
}

NS_IMETHODIMP
BatteryManager::GetChargingTime(double* aChargingTime)
{
  if (!mCharging || mRemainingTime == kUnknownRemainingTime) {
    *aChargingTime = std::numeric_limits<double>::infinity();
    return NS_OK;
  }

  *aChargingTime = mRemainingTime;

  return NS_OK;
}

NS_IMPL_EVENT_HANDLER(BatteryManager, levelchange)
NS_IMPL_EVENT_HANDLER(BatteryManager, chargingchange)
NS_IMPL_EVENT_HANDLER(BatteryManager, chargingtimechange)
NS_IMPL_EVENT_HANDLER(BatteryManager, dischargingtimechange)

nsresult
BatteryManager::DispatchTrustedEventToSelf(const nsAString& aEventName)
{
  nsRefPtr<nsDOMEvent> event = new nsDOMEvent(nsnull, nsnull);
  nsresult rv = event->InitEvent(aEventName, false, false);
  NS_ENSURE_SUCCESS(rv, rv);

  rv = event->SetTrusted(PR_TRUE);
  NS_ENSURE_SUCCESS(rv, rv);

  bool dummy;
  rv = DispatchEvent(event, &dummy);
  NS_ENSURE_SUCCESS(rv, rv);

  return NS_OK;
}

void
BatteryManager::UpdateFromBatteryInfo(const hal::BatteryInformation& aBatteryInfo)
{
  mLevel = aBatteryInfo.level();
  mCharging = aBatteryInfo.charging();
  mRemainingTime = aBatteryInfo.remainingTime();

  // Add some guards to make sure the values are coherent.
  if (mLevel == 1.0 && mCharging == true &&
      mRemainingTime != kDefaultRemainingTime) {
    mRemainingTime = kDefaultRemainingTime;
    NS_ERROR("Battery API: When charging and level at 1.0, remaining time "
             "should be 0. Please fix your backend!");
  }
}

void
BatteryManager::Notify(const hal::BatteryInformation& aBatteryInfo)
{
  double previousLevel = mLevel;
  bool previousCharging = mCharging;
  double previousRemainingTime = mRemainingTime;

  UpdateFromBatteryInfo(aBatteryInfo);

  if (previousCharging != mCharging) {
    DispatchTrustedEventToSelf(CHARGINGCHANGE_EVENT_NAME);
  }

  if (previousLevel != mLevel) {
    DispatchTrustedEventToSelf(LEVELCHANGE_EVENT_NAME);
  }

  /*
   * There are a few situations that could happen here:
   * 1. Charging state changed:
   *   a. Previous remaining time wasn't unkwonw, we have to fire an event for
   *      the change.
   *   b. New remaining time isn't unkwonw, we have to fire an event for it.
   * 2. Charging state didn't change but remainingTime did, we have to fire
   *    the event that correspond to the current charging state.
   */
  if (mCharging != previousCharging) {
    if (previousRemainingTime != kUnknownRemainingTime) {
      DispatchTrustedEventToSelf(previousCharging ? CHARGINGTIMECHANGE_EVENT_NAME
                                                  : DISCHARGINGTIMECHANGE_EVENT_NAME);
    }
    if (mRemainingTime != kUnknownRemainingTime) {
      DispatchTrustedEventToSelf(mCharging ? CHARGINGTIMECHANGE_EVENT_NAME
                                           : DISCHARGINGTIMECHANGE_EVENT_NAME);
    }
  } else if (previousRemainingTime != mRemainingTime) {
    DispatchTrustedEventToSelf(mCharging ? CHARGINGTIMECHANGE_EVENT_NAME
                                         : DISCHARGINGTIMECHANGE_EVENT_NAME);
  }
}

/* static */ bool
BatteryManager::HasSupport()
{
  return Preferences::GetBool("dom.battery.enabled", true);
}

} // namespace battery
} // namespace dom
} // namespace mozilla
