/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

Cu.importGlobalProperties(["fetch"]);

var EXPORTED_SYMBOLS = ["PartnerLinkAttribution"];

const { XPCOMUtils } = ChromeUtils.import(
  "resource://gre/modules/XPCOMUtils.jsm"
);

XPCOMUtils.defineLazyModuleGetters(this, {
  Services: "resource://gre/modules/Services.jsm",
  Region: "resource://gre/modules/Region.jsm",
});

var PartnerLinkAttribution = {
  async makeRequest({ targetURL, source }) {
    let partner = targetURL.match(/^https?:\/\/(?:www.)?([^.]*)/)[1];

    function record(objectString, value = "") {
      recordTelemetryEvent("interaction", objectString, value, {
        partner,
        source,
      });
    }
    record("click");

    const attributionUrl = Services.prefs.getStringPref(
      Services.prefs.getBoolPref("browser.topsites.useRemoteSetting")
        ? "browser.partnerlink.attributionURL"
        : `browser.newtabpage.searchTileOverride.${partner}.attributionURL`,
      ""
    );
    if (!attributionUrl) {
      record("attribution", "abort");
      return;
    }
    let result = await sendRequest(attributionUrl, source, targetURL);
    record("attribution", result ? "success" : "failure");
  },

  /**
   * Makes a request to the attribution URL for a search engine search.
   *
   * @param {nsISearchEngine} engine
   *   The search engine to save the attribution for.
   * @param {nsIURI} targetUrl
   *   The target URL to filter and include in the attribution.
   */
  async makeSearchEngineRequest(engine, targetUrl) {
    if (!engine.sendAttributionRequest) {
      return;
    }

    let searchUrlQueryParamName = engine.searchUrlQueryParamName;
    if (!searchUrlQueryParamName) {
      Cu.reportError("makeSearchEngineRequest can't find search terms key");
      return;
    }

    let url = targetUrl;
    if (typeof url == "string") {
      url = Services.io.newURI(url);
    }

    let targetParams = new URLSearchParams(url.query);
    if (!targetParams.has(searchUrlQueryParamName)) {
      Cu.reportError(
        "makeSearchEngineRequest can't remove target search terms"
      );
      return;
    }

    const attributionUrl = Services.prefs.getStringPref(
      "browser.partnerlink.attributionURL",
      ""
    );

    targetParams.delete(searchUrlQueryParamName);
    let strippedTargetUrl = `${url.prePath}${url.filePath}`;
    let newParams = targetParams.toString();
    if (newParams) {
      strippedTargetUrl += "?" + newParams;
    }

    await sendRequest(attributionUrl, "searchurl", strippedTargetUrl);
  },
};

async function sendRequest(attributionUrl, source, targetURL) {
  const request = new Request(attributionUrl);
  request.headers.set("X-Region", Region.home);
  request.headers.set("X-Source", source);
  request.headers.set("X-Target-URL", targetURL);
  const response = await fetch(request);
  return response.ok;
}

function recordTelemetryEvent(method, objectString, value, extra) {
  Services.telemetry.setEventRecordingEnabled("partner_link", true);
  Services.telemetry.recordEvent(
    "partner_link",
    method,
    objectString,
    value,
    extra
  );
}
