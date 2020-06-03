/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

Cu.importGlobalProperties(["fetch"]);

var EXPORTED_SYMBOLS = ["TopSiteAttribution"];

const { Services } = ChromeUtils.import("resource://gre/modules/Services.jsm");

var TopSiteAttribution = {
  async makeRequest({ searchProvider, siteURL }) {
    function record(objectString, value = "") {
      recordTelemetryEvent("search_override_exp", objectString, value, {
        searchProvider,
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
