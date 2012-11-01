/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

this.EXPORTED_SYMBOLS = ["TelemetryTimestamps"];

/**
 * This module's purpose is to collect timestamps for important
 * application-specific events.
 *
 * The TelemetryPing component attaches the timestamps stored by this module to
 * the telemetry submission, substracting the process lifetime so that the times
 * are relative to process startup. The overall goal is to produce a basic
 * timeline of the startup process.
 */
let timeStamps = {};

this.TelemetryTimestamps = {
  /**
   * Adds a timestamp to the list. The addition of TimeStamps that already have
   * a value stored is ignored.
   *
   * @param name must be a unique, generally "camelCase" descriptor of what the
   *             timestamp represents. e.g.: "delayedStartupStarted"
   * @param value is a timeStamp in milliseconds since the epoch. If omitted,
   *              defaults to Date.now().
   */
  add: function TT_add(name, value) {
    // Default to "now" if not specified
    if (value == null)
      value = Date.now();

    if (isNaN(value))
      throw new Error("Value must be a timestamp");

    // If there's an existing value, just ignore the new value.
    if (timeStamps.hasOwnProperty(name))
      return;

    timeStamps[name] = value;
  },

  /**
   * Returns a JS object containing all of the timeStamps as properties (can be
   * easily serialized to JSON). Used by TelemetryPing to retrieve the data
   * to attach to the telemetry submission.
   */
  get: function TT_get() {
    // Return a copy of the object by passing it through JSON.
    return JSON.parse(JSON.stringify(timeStamps));
  }
};
