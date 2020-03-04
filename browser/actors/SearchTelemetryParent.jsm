/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
"use strict";

var EXPORTED_SYMBOLS = ["SearchTelemetryParent"];

ChromeUtils.defineModuleGetter(
  this,
  "SearchTelemetry",
  "resource:///modules/SearchTelemetry.jsm"
);

class SearchTelemetryParent extends JSWindowActorParent {
  receiveMessage(msg) {
    if (msg.name == "SearchTelemetry:PageInfo") {
      SearchTelemetry.reportPageWithAds(msg.data);
    }
  }
}
