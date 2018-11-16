/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

var EXPORTED_SYMBOLS = ["SearchTelemetry"];

const {XPCOMUtils} = ChromeUtils.import("resource://gre/modules/XPCOMUtils.jsm", null);

XPCOMUtils.defineLazyModuleGetters(this, {
  Services: "resource://gre/modules/Services.jsm",
});

const SEARCH_COUNTS_HISTOGRAM_KEY = "SEARCH_COUNTS";

// Used to identify various parameters (query, code, etc.) in search URLS.
const SEARCH_PROVIDER_INFO = {
  "google": {
    "regexp": /^https:\/\/www\.(google)\.(?:.+)\/search/,
    "queryParam": "q",
    "codeParam": "client",
    "codePrefixes": ["firefox"],
    "followonParams": ["oq", "ved", "ei"],
  },
  "duckduckgo": {
    "regexp": /^https:\/\/(duckduckgo)\.com\//,
    "queryParam": "q",
    "codeParam": "t",
    "codePrefixes": ["ff"],
  },
  "yahoo": {
    "regexp": /^https:\/\/(?:.*)search\.(yahoo)\.com\/search/,
    "queryParam": "p",
  },
  "baidu": {
    "regexp": /^https:\/\/www\.(baidu)\.com\/(?:s|baidu)/,
    "queryParam": "wd",
    "codeParam": "tn",
    "codePrefixes": ["monline_dg"],
    "followonParams": ["oq"],
  },
  "bing": {
    "regexp": /^https:\/\/www\.(bing)\.com\/search/,
    "queryParam": "q",
    "codeParam": "pc",
    "codePrefixes": ["MOZ", "MZ"],
  },
};

const BROWSER_SEARCH_PREF = "browser.search.";

XPCOMUtils.defineLazyPreferenceGetter(this, "loggingEnabled", BROWSER_SEARCH_PREF + "log", false);

class TelemetryHandler {
  constructor() {
    this.__searchProviderInfo = null;
  }

  overrideSearchTelemetryForTests(infoByProvider) {
    if (infoByProvider) {
      for (let info of Object.values(infoByProvider)) {
        info.regexp = new RegExp(info.regexp);
      }
      this.__searchProviderInfo = infoByProvider;
    } else {
      this.__searchProviderInfo = SEARCH_PROVIDER_INFO;
    }
  }

  recordSearchURLTelemetry(url) {
    let entry = Object.entries(this._searchProviderInfo).find(
      ([_, info]) => info.regexp.test(url)
    );
    if (!entry) {
      return;
    }
    let [provider, searchProviderInfo] = entry;
    let queries = new URLSearchParams(url.split("#")[0].split("?")[1]);
    if (!queries.get(searchProviderInfo.queryParam)) {
      return;
    }
    // Default to organic to simplify things.
    // We override type in the sap cases.
    let type = "organic";
    let code;
    if (searchProviderInfo.codeParam) {
      code = queries.get(searchProviderInfo.codeParam);
      if (code &&
          searchProviderInfo.codePrefixes.some(p => code.startsWith(p))) {
        if (searchProviderInfo.followonParams &&
           searchProviderInfo.followonParams.some(p => queries.has(p))) {
          type = "sap-follow-on";
        } else {
          type = "sap";
        }
      } else if (provider == "bing") {
        // Bing requires lots of extra work related to cookies.
        let secondaryCode = queries.get("form");
        // This code is used for all Bing follow-on searches.
        if (secondaryCode == "QBRE") {
          for (let cookie of Services.cookies.getCookiesFromHost("www.bing.com", {})) {
            if (cookie.name == "SRCHS") {
              // If this cookie is present, it's probably an SAP follow-on.
              // This might be an organic follow-on in the same session,
              // but there is no way to tell the difference.
              if (searchProviderInfo.codePrefixes.some(p => cookie.value.startsWith("PC=" + p))) {
                type = "sap-follow-on";
                code = cookie.value.split("=")[1];
                break;
              }
            }
          }
        }
      }
    }

    let payload = `${provider}.in-content:${type}:${code || "none"}`;
    let histogram = Services.telemetry.getKeyedHistogramById(SEARCH_COUNTS_HISTOGRAM_KEY);
    histogram.add(payload);
    LOG("recordSearchURLTelemetry: " + payload);
  }

  get _searchProviderInfo() {
    if (!this.__searchProviderInfo) {
      this.__searchProviderInfo = SEARCH_PROVIDER_INFO;
    }
    return this.__searchProviderInfo;
  }
}

/**
 * Outputs aText to the JavaScript console as well as to stdout.
 */
function LOG(aText) {
  if (loggingEnabled) {
    dump(`*** SearchTelemetry: ${aText}\n"`);
    Services.console.logStringMessage(aText);
  }
}

var SearchTelemetry = new TelemetryHandler();
