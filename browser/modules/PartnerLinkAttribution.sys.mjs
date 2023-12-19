/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

const lazy = {};

ChromeUtils.defineESModuleGetters(lazy, {
  Region: "resource://gre/modules/Region.sys.mjs",
});

export const CONTEXTUAL_SERVICES_PING_TYPES = {
  TOPSITES_IMPRESSION: "topsites-impression",
  TOPSITES_SELECTION: "topsites-click",
  QS_BLOCK: "quicksuggest-block",
  QS_IMPRESSION: "quicksuggest-impression",
  QS_SELECTION: "quicksuggest-click",
};

export var PartnerLinkAttribution = {
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
};

async function sendRequest(attributionUrl, source, targetURL) {
  const request = new Request(attributionUrl);
  request.headers.set("X-Region", lazy.Region.home);
  request.headers.set("X-Source", source);
  request.headers.set("X-Target-URL", targetURL);
  const response = await fetch(request);
  return response.ok;
}

function recordTelemetryEvent({ method, objectString, value }) {
  Services.telemetry.setEventRecordingEnabled("partner_link", true);
  Services.telemetry.recordEvent("partner_link", method, objectString, value);
}
