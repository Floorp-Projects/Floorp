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
  /**
   * Sends an attribution request to an anonymizing proxy.
   *
   * @param {string} targetURL
   *   The URL we are routing through the anonmyzing proxy.
   * @param {string} source
   *   The source of the anonmized request, e.g. "urlbar".
   * @param {string} [campaignID]
   *   The campaign ID for attribution. This should be a valid path on the
   *   anonymizing proxy. For example, if `campaignID` was `foo`, we'd send an
   *   attribution request to https://topsites.mozilla.com/cid/foo.
   *   Optional. If it's not provided, we default to the topsites campaign.
   */
  async makeRequest({ targetURL, source, campaignID }) {
    let partner = targetURL.match(/^https?:\/\/(?:www.)?([^.]*)/)[1];

    function record(method, objectString) {
      recordTelemetryEvent({
        method,
        objectString,
        value: partner,
      });
    }
    record("click", source);

    let attributionUrl = Services.prefs.getStringPref(
      "browser.partnerlink.attributionURL"
    );
    if (!attributionUrl) {
      record("attribution", "abort");
      return;
    }

    // The default campaign is topsites.
    if (!campaignID) {
      campaignID = Services.prefs.getStringPref(
        "browser.partnerlink.campaign.topsites"
      );
    }
    attributionUrl = attributionUrl + campaignID;
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
    let cid;
    if (engine.attribution?.cid) {
      cid = engine.attribution.cid;
    } else if (engine.sendAttributionRequest) {
      cid = Services.prefs.getStringPref(
        "browser.partnerlink.campaign.topsites"
      );
    } else {
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

    let attributionUrl = Services.prefs.getStringPref(
      "browser.partnerlink.attributionURL",
      ""
    );
    attributionUrl = attributionUrl + cid;

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

function recordTelemetryEvent({ method, objectString, value }) {
  Services.telemetry.setEventRecordingEnabled("partner_link", true);
  Services.telemetry.recordEvent("partner_link", method, objectString, value);
}
