/* vim: set ts=2 sw=2 sts=2 et tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
"use strict";

const EXPORTED_SYMBOLS = ["ASRouterTelemetry"];

ChromeUtils.defineModuleGetter(
  this,
  "TelemetryFeed",
  "resource://activity-stream/lib/TelemetryFeed.jsm"
);

class ASRouterTelemetry {
  constructor(options) {
    this.telemetryFeed = new TelemetryFeed(options);
    this.sendTelemetry = this.sendTelemetry.bind(this);
  }

  sendTelemetry(action) {
    return this.telemetryFeed.onAction(action);
  }
}
