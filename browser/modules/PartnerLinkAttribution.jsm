/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

Cu.importGlobalProperties(["fetch"]);

var EXPORTED_SYMBOLS = ["PartnerLinkAttribution"];

ChromeUtils.defineModuleGetter(
  this,
  "Services",
  "resource://gre/modules/Services.jsm"
);

ChromeUtils.defineModuleGetter(
  this,
  "Region",
  "resource://gre/modules/Region.jsm"
);

var PartnerLinkAttribution = {
  async makeRequest({ targetURL, source }) {
    let searchProvider = targetURL.match(/^https?:\/\/(?:www.)?([^.]*)/)[1];

    function record(objectString, value = "") {
      recordTelemetryEvent("search_override_exp", objectString, value, {
        searchProvider,
        source,
      });
    }
    record("click");

    const attributionUrl = Services.prefs.getStringPref(
      Services.prefs.getBoolPref("browser.topsites.useRemoteSetting")
        ? "browser.topsites.attributionURL"
        : `browser.newtabpage.searchTileOverride.${searchProvider}.attributionURL`,
      ""
    );
    if (!attributionUrl) {
      record("attribution", "abort");
      return;
    }
    const request = new Request(attributionUrl);
    request.headers.set("X-Region", Region.home);
    request.headers.set("X-Source", source);
    request.headers.set("X-Target-URL", targetURL);
    const response = await fetch(request);
    record("attribution", response.ok ? "success" : "failure");
  },
};

function recordTelemetryEvent(method, objectString, value, extra) {
  Services.telemetry.setEventRecordingEnabled("top_sites", true);
  Services.telemetry.recordEvent(
    "top_sites",
    method,
    objectString,
    value,
    extra
  );
}
