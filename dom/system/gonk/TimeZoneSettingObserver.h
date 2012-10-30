/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_system_timesetting_h__
#define mozilla_system_timesetting_h__

namespace mozilla {
namespace system {

// Initialize TimeZoneSettingObserver which observes the time zone change
// event from settings service. When receiving the event, it modifies the
// system time zone.
void InitializeTimeZoneSettingObserver();

} // namespace system
} // namespace mozilla

#endif // mozilla_system_timesetting_h__

