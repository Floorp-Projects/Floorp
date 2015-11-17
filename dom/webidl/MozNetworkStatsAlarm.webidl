/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

[JSImplementation="@mozilla.org/networkstatsalarm;1",
 ChromeOnly,
 CheckAnyPermissions="networkstats-manage",
 Pref="dom.mozNetworkStats.enabled"]
interface MozNetworkStatsAlarm {
  readonly attribute unsigned long alarmId;
  readonly attribute MozNetworkStatsInterface network;
  readonly attribute long long threshold;
  readonly attribute any data;
};
