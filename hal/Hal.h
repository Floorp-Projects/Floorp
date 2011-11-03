/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: sw=2 ts=8 et ft=cpp : */
/* ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at:
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is Mozilla Code.
 *
 * The Initial Developer of the Original Code is
 *   The Mozilla Foundation
 * Portions created by the Initial Developer are Copyright (C) 2011
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Chris Jones <jones.chris.g@gmail.com>
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
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

#ifndef mozilla_Hal_h
#define mozilla_Hal_h 1

#include "base/basictypes.h"
#include "mozilla/Types.h"
#include "nsTArray.h"
#include "mozilla/dom/battery/Types.h"

#ifndef MOZ_HAL_NAMESPACE
// This goop plays some cpp tricks to ensure a uniform API across the
// API entry point, "sandbox" implementations (for content processes),
// and "impl" backends where the real work happens.  After this runs
// through cpp, there will be three sets of identical APIs
//   hal_impl:: --- the platform-specific implementation of an API.
//   hal_sandbox:: --- forwards calls up to the parent process
//   hal:: --- invokes sandboxed impl if in a sandboxed process,
//             otherwise forwards to hal_impl
//
// External code should never invoke hal_impl:: or hal_sandbox:: code
// directly.
# include "HalImpl.h"
# include "HalSandbox.h"
# define MOZ_HAL_NAMESPACE hal
# define MOZ_DEFINED_HAL_NAMESPACE 1
#endif

namespace mozilla {

namespace hal {
class BatteryInformation;
} // namespace hal

namespace MOZ_HAL_NAMESPACE /*hal*/ {

/**
 * Turn the default vibrator device on/off per the pattern specified
 * by |pattern|.  Each element in the pattern is the number of
 * milliseconds to turn the vibrator on or off.  The first element in
 * |pattern| is an "on" element, the next is "off", and so on.
 *
 * If |pattern| is empty, any in-progress vibration is canceled.
 */
void Vibrate(const nsTArray<uint32>& pattern);

/**
 * Inform the battery backend there is a new battery observer.
 * @param aBatteryObserver The observer that should be added.
 */
void RegisterBatteryObserver(BatteryObserver* aBatteryObserver);

/**
 * Inform the battery backend a battery observer unregistered.
 * @param aBatteryObserver The observer that should be removed.
 */
void UnregisterBatteryObserver(BatteryObserver* aBatteryObserver);

/**
 * Enables battery notifications from the backend.
 *
 * This method is semi-private in the sense of it is visible in the hal
 * namespace but should not be used. Calls to this method from the hal
 * namespace will produce a link error because it is not defined.
 */
void EnableBatteryNotifications();

/**
 * Disables battery notifications from the backend.
 *
 * This method is semi-private in the sense of it is visible in the hal
 * namespace but should not be used. Calls to this method from the hal
 * namespace will produce a link error because it is not defined.
 */
void DisableBatteryNotifications();

/**
 * Returns the current battery information.
 */
void GetCurrentBatteryInformation(hal::BatteryInformation* aBatteryInfo);

/**
 * Notify of a change in the battery state.
 * @param aBatteryInfo The new battery information.
 */
void NotifyBatteryChange(const hal::BatteryInformation& aBatteryInfo);

}
}

#ifdef MOZ_DEFINED_HAL_NAMESPACE
# undef MOZ_DEFINED_HAL_NAMESPACE
# undef MOZ_HAL_NAMESPACE
#endif

#endif  // mozilla_Hal_h
