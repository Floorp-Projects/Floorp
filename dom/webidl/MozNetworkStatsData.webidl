/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

[JSImplementation="@mozilla.org/networkStatsdata;1",
 ChromeOnly,
 Pref="dom.mozNetworkStats.enabled"]
interface MozNetworkStatsData {
  readonly attribute unsigned long   rxBytes;   // Received bytes.
  readonly attribute unsigned long   txBytes;   // Sent bytes.
  readonly attribute Date            date;      // Date.
};
