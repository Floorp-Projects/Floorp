/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

[Func="mozilla::dom::time::TimeManager::PrefEnabled"]
interface MozTimeManager {
  /* Set the system time.
   *
   * The |time| argument can be either a Date object or a number.
   *
   * - If |time| is a number, it's interpreted as milliseconds
   *   since the epoch (midnight UTC on January 1, 1970).
   * - If |time| is a Date object, |set(time)| is equivalent to
   *   |set(time.getTime())|.
   */
  void set(Date time);
  void set(double time);
};
