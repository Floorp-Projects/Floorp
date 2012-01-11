/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

let EXPORTED_SYMBOLS = ["TelemetryTimestamps"];

let TelemetryTimestamps = {
  timeStamps: {},
  add: function TT_add(name, value) {
    // Default to "now" if not specified
    if (value == null)
      value = Date.now();

    if (isNaN(value))
      throw new Error("Value must be a timestamp");

    // If there's an existing value, just ignore the new value.
    if (this.timeStamps.hasOwnProperty(name))
      return;

    this.timeStamps[name] = value;
  },
  get: function TT_get() {
    return JSON.parse(JSON.stringify(this.timeStamps));
  }
};
