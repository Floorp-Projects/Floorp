/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
"use strict";

var EXPORTED_SYMBOLS = ["SearchSERPTelemetryParent"];

ChromeUtils.defineModuleGetter(
  this,
  "SearchSERPTelemetry",
  "resource:///modules/SearchSERPTelemetry.jsm"
);

class SearchSERPTelemetryParent extends JSWindowActorParent {
  receiveMessage(msg) {
    if (msg.name == "SearchTelemetry:PageInfo") {
      SearchSERPTelemetry.reportPageWithAds(msg.data);
    }
  }
}
