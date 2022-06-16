/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

var EXPORTED_SYMBOLS = [
  "PartnerLinkAttribution",
  "CONTEXTUAL_SERVICES_PING_TYPES",
];

const { XPCOMUtils } = ChromeUtils.import(
  "resource://gre/modules/XPCOMUtils.jsm"
);
const { Services } = ChromeUtils.import("resource://gre/modules/Services.jsm");

const lazy = {};

XPCOMUtils.defineLazyModuleGetters(lazy, {
  Region: "resource://gre/modules/Region.jsm",
  PingCentre: "resource:///modules/PingCentre.jsm",
});

// Endpoint base URL for Structured Ingestion
XPCOMUtils.defineLazyPreferenceGetter(
  lazy,
  "structuredIngestionEndpointBase",
  "browser.newtabpage.activity-stream.telemetry.structuredIngestion.endpoint",
  ""
);
const NAMESPACE_CONTEXUAL_SERVICES = "contextual-services";

// PingCentre client to send custom pings
XPCOMUtils.defineLazyGetter(lazy, "pingcentre", () => {
  return new lazy.PingCentre({ topic: "contextual-services" });
});

// `contextId` is a unique identifier used by Contextual Services
const CONTEXT_ID_PREF = "browser.contextual-services.contextId";
XPCOMUtils.defineLazyGetter(lazy, "contextId", () => {
  let _contextId = Services.prefs.getStringPref(CONTEXT_ID_PREF, null);
  if (!_contextId) {
    _contextId = String(Services.uuid.generateUUID());
    Services.prefs.setStringPref(CONTEXT_ID_PREF, _contextId);
  }
  return _contextId;
});

const CONTEXTUAL_SERVICES_PING_TYPES = {
  TOPSITES_IMPRESSION: "topsites-impression",
  TOPSITES_SELECTION: "topsites-click",
  QS_BLOCK: "quicksuggest-block",
  QS_IMPRESSION: "quicksuggest-impression",
  QS_SELECTION: "quicksuggest-click",
};

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

  /**
   * Sends a Contextual Services ping to the Mozilla data pipeline.
   *
   * Note:
   *   * All Contextual Services pings are sent as custom pings
   *     (https://docs.telemetry.mozilla.org/cookbooks/new_ping.html#sending-a-custom-ping)
   *
   *   * The full event list can be found at https://github.com/mozilla-services/mozilla-pipeline-schemas
   *     under the "contextual-services" namespace
   *
   * @param {object} payload
   *   The ping payload to be sent to the Mozilla Structured Ingestion endpoint
   * @param {String} pingType
   *   The ping type. Must be one of CONTEXTUAL_SERVICES_PING_TYPES
   */
  sendContextualServicesPing(payload, pingType) {
    if (!Object.values(CONTEXTUAL_SERVICES_PING_TYPES).includes(pingType)) {
      Cu.reportError("Invalid Contextual Services ping type");
      return;
    }

    const endpoint = makeEndpointUrl(pingType, "1");
    payload.context_id = lazy.contextId;
    lazy.pingcentre.sendStructuredIngestionPing(payload, endpoint);
  },

  /**
   * Gets the underlying PingCentre client, only used for tests.
   */
  get _pingCentre() {
    return lazy.pingcentre;
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

/**
 * Makes a new endpoint URL for a ping submission. Note that each submission
 * to Structured Ingesttion requires a new endpoint. See more details about
 * the specs:
 *
 * https://docs.telemetry.mozilla.org/concepts/pipeline/http_edge_spec.html?highlight=docId#postput-request
 *
 * @param {String} pingType
 *   The ping type. Must be one of CONTEXTUAL_SERVICES_PING_TYPES
 * @param {String} version
 *   The schema version of the ping.
 */
function makeEndpointUrl(pingType, version) {
  // Structured Ingestion does not support the UUID generated by gUUIDGenerator.
  // Stripping off the leading and trailing braces to make it happy.
  const docID = Services.uuid
    .generateUUID()
    .toString()
    .slice(1, -1);
  const extension = `${NAMESPACE_CONTEXUAL_SERVICES}/${pingType}/${version}/${docID}`;
  return `${lazy.structuredIngestionEndpointBase}/${extension}`;
}
