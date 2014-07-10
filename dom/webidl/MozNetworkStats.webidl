/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

/**
 * Provide the detailed options for specifying different kinds of data filtering
 * in getSamples function.
 */
dictionary NetworkStatsGetOptions
{
  /**
   * App manifest URL is used to filter network stats by app, while service type
   * is used to filter stats by system service.
   * Note that, these two options cannot be specified at the same time for now;
   * others, an NS_ERROR_NOT_IMPLMENTED exception will be thrown.
   */
  DOMString appManifestURL;
  DOMString serviceType;
};

dictionary NetworkStatsAlarmOptions
{
  Date startTime;
  Date data;
};

[JSImplementation="@mozilla.org/networkstats;1",
 ChromeOnly,
 CheckPermissions="networkstats-manage",
 Pref="dom.mozNetworkStats.enabled"]
interface MozNetworkStats {
  /**
   * App manifest URL of an application for specifying the per-app stats of the
   * specified app.
   */
  readonly attribute DOMString    appManifestURL;

  /**
   * Service type is used to retrieve the corresponding "system-only" stats.
   * E.g., "Tethering", "OTA", etc.
   */
  readonly attribute DOMString    serviceType;

  /**
   * Network the returned data belongs to.
   */
  readonly attribute MozNetworkStatsInterface network;

  /**
   * Stats for a network.
   */
  [Cached, Pure]
  readonly attribute sequence<MozNetworkStatsData> data;

  /**
   * Dates
   */
  readonly attribute object start;
  readonly attribute object end;
};
