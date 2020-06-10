/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

Cu.importGlobalProperties(["fetch"]);

var EXPORTED_SYMBOLS = ["TopSiteAttribution"];

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

var TopSiteAttribution = {
  async makeRequest({ searchProvider, siteURL, source }) {
    function record(objectString, value = "") {
      recordTelemetryEvent("search_override_exp", objectString, value, {
        searchProvider,
        source,
      });
    }
    record("click");

    const attributionUrl = Services.prefs.getStringPref(
      `browser.newtabpage.searchTileOverride.${searchProvider}.attributionURL`,
      ""
    );
    if (!attributionUrl) {
      record("attribution", "abort");
      return;
    }
    const request = new Request(attributionUrl);
    request.headers.set("X-Region", Region.home);
    request.headers.set("X-Source", source);
    const response = await fetch(request);

    if (response.ok) {
      if (siteURL == response.responseText) {
        record("attribution", "success");
      } else {
        record("attribution", "url_mismatch");
      }
    } else {
      record("attribution", "failure");
    }
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
