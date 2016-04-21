/* -*- Mode: IDL; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/.
 */

[ChromeOnly,
 Pref="dom.resource_stats.enabled",
 JSImplementation="@mozilla.org/networkStatsData;1"]
interface NetworkStatsData
{
  readonly attribute unsigned long long   receivedBytes;
  readonly attribute unsigned long long   sentBytes;
  readonly attribute DOMTimeStamp         timestamp;      // timestamp of the record
};

[ChromeOnly,
 Pref="dom.resource_stats.enabled",
 JSImplementation="@mozilla.org/powerStatsData;1"]
interface PowerStatsData
{
  readonly attribute unsigned long long   consumedPower;  // unit: mW
  readonly attribute DOMTimeStamp         timestamp;      // timestamp of the record
};

[ChromeOnly,
 Pref="dom.resource_stats.enabled",
 JSImplementation="@mozilla.org/resourceStats;1"]
interface ResourceStats
{
  /**
   * Type of statistics/
   */
  readonly attribute ResourceType   type;

  /**
   * The |component| specifies statistics belongs to. This will be null if
   * the ResourceStatsOptions.component argument passed to getStats() is null.
   */
  readonly attribute DOMString?     component;

  /**
   * |serviceType| specifies the system service. This will be null if the
   * ResourceStatsOptions.serviceType argument passed to getStats() is null.
   */
  readonly attribute SystemService? serviceType;

  /**
   * |manifestURL| specifies the manifestURL of an application. This will be
   * null if the ResourceStatsOptions.manifestURL argument passed to getStats()
   * is null.
   */
  readonly attribute DOMString?     manifestURL;

  /**
   * Statistics, one element per day
   */
  sequence<(NetworkStatsData or PowerStatsData)> getData();

  /**
   * Date range
   */
  readonly attribute DOMTimeStamp   start;  // start timestamp
  readonly attribute DOMTimeStamp   end;    // end timestamp
};
