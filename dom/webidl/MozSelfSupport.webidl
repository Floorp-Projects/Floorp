/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/**
 * The MozSelfSupport interface allows external Mozilla support sites such as
 * FHR and SUMO to access data and control settings that are not otherwise
 * exposed to external content.
 *
 * At the moment, this is a ChromeOnly interface, but the plan is to allow
 * specific Mozilla domains to access it directly.
 */
[ChromeOnly,
 JSImplementation="@mozilla.org/mozselfsupport;1",
 Constructor()]
interface MozSelfSupport
{
  /**
   * Controls whether uploading FHR data is allowed.
   */
  attribute boolean healthReportDataSubmissionEnabled;

  /**
   * Retrieve a list of the archived Telemetry pings.
   * This contains objects with ping info, which are of the form:
   * {
   *   type: <string>, // The pings type, e.g. "main", "environment-change", ...
   *   timestampCreated: <number>, // The time the ping was created (ms since unix epoch).
   *   id: <string>, // The pings UUID.
   * }
   *
   * @return Promise<sequence<Object>>
   *         Resolved with the ping infos when the archived ping list has been built.
   */
  Promise<sequence<object>> getTelemetryPingList();

  /**
   * Retrieve an archived Telemetry ping by it's id.
   * This will load the ping data async from the archive, possibly hitting the disk.
   *
   * @return Promise<Object>
   *         Resolved with the ping data, see the Telemetry "main" ping documentation for the format.
   */
  Promise<object> getTelemetryPing(DOMString pingID);

  /**
   * Get the current Telemetry environment - see the Telemetry documentation for details on the format.
   *
   * @return Promise<Object>
   *         Resolved with an object containing the Telemetry environment data.
   */
  Promise<object> getCurrentTelemetryEnvironment();

  /**
   * Get a Telemetry "main" ping containing the current session measurements.
   *
   * @return Promise<Object>
   *         Resolved with the ping data, see the Telemetry "main" ping documentation for the format.
   */
  Promise<object> getCurrentTelemetrySubsessionPing();

  /**
   * Resets a named pref:
   * - if there is a default value, then change the value back to default,
   * - if there's no default value, then delete the pref,
   * - no-op otherwise.
   *
   * @param DOMString
   *        The name of the pref to reset.
   */
  void resetPref(DOMString name);

  /**
   * Resets original search engines, and resets the default one.
   */
  void resetSearchEngines();
};
