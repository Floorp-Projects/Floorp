/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

// These type strings are used for logging events to Telemetry.
// You must update Histograms.json if new types are added.
const RuntimeTypes = {
  USB: "USB",
  WIFI: "WIFI",
  REMOTE: "REMOTE",
  LOCAL: "LOCAL",
  OTHER: "OTHER",
};

exports.RuntimeTypes = RuntimeTypes;
