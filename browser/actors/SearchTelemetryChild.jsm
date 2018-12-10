/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
"use strict";

var EXPORTED_SYMBOLS = ["SearchTelemetryChild"];

ChromeUtils.import("resource://gre/modules/ActorChild.jsm");
ChromeUtils.import("resource://gre/modules/Services.jsm");

const SHARED_DATA_KEY = "SearchTelemetry:ProviderInfo";

/**
 * SearchProviders looks after keeping track of the search provider information
 * received from the main process.
 *
 * It is separate to SearchTelemetryChild so that it is not constructed for each
 * tab, but once per process.
 */
class SearchProviders {
  constructor() {
    this._searchProviderInfo = null;
    Services.cpmm.sharedData.addEventListener("change", this);
  }

  /**
   * Gets the search provider information for any provider with advert information.
   * If there is nothing in the cache, it will obtain it from shared data.
   *
   * @returns {object} Returns the search provider information. @see SearchTelemetry.jsm
   */
  get info() {
    if (this._searchProviderInfo) {
      return this._searchProviderInfo;
    }

    this._searchProviderInfo = Services.cpmm.sharedData.get(SHARED_DATA_KEY);

    if (!this._searchProviderInfo) {
      return null;
    }

    // Filter-out non-ad providers so that we're not trying to match against
    // those unnecessarily.
    for (let [providerName, info] of Object.entries(this._searchProviderInfo)) {
      if (!("extraAdServersRegexps" in info)) {
        delete this._searchProviderInfo[providerName];
      }
    }

    return this._searchProviderInfo;
  }

  /**
   * Handles events received from sharedData notifications.
   *
   * @param {object} event The event details.
   */
  handleEvent(event) {
    switch (event.type) {
      case "change": {
        if (event.changedKeys.includes(SHARED_DATA_KEY)) {
          // Just null out the provider information for now, we'll fetch it next
          // time we need it.
          this._searchProviderInfo = null;
        }
        break;
      }
    }
  }
}

const searchProviders = new SearchProviders();

/**
 * SearchTelemetryChild monitors for pages that are partner searches, and
 * looks through them to find links which looks like adverts and sends back
 * a notification to SearchTelemetry for possible telemetry reporting.
 *
 * Only the partner details and the fact that at least one ad was found on the
 * page are returned to SearchTelemetry. If no ads are found, no notification is
 * given.
 */
class SearchTelemetryChild extends ActorChild {
  /**
   * Determines if there is a provider that matches the supplied URL and returns
   * the information associated with that provider.
   *
   * @param {string} url The url to check
   * @returns {array|null} Returns null if there's no match, otherwise an array
   *   of provider name and the provider information.
   */
  _getProviderInfoForUrl(url) {
    return Object.entries(searchProviders.info || []).find(
      ([_, info]) => info.regexp.test(url)
    );
  }

  /**
   * Checks to see if the page is a partner and has an ad link within it. If so,
   * it will notify SearchTelemetry.
   *
   * @param {object} doc The document object to check.
   */
  _checkForAdLink(doc) {
    let providerInfo = this._getProviderInfoForUrl(doc.documentURI);
    if (!providerInfo) {
      return;
    }

    let regexps = providerInfo[1].extraAdServersRegexps;

    let anchors = doc.getElementsByTagName("a");
    let hasAds = false;
    for (let anchor of anchors) {
      if (!anchor.href) {
        continue;
      }
      for (let regexp of regexps) {
        if (regexp.test(anchor.href)) {
          hasAds = true;
          break;
        }
      }
      if (hasAds) {
        break;
      }
    }
    if (hasAds) {
      this.sendAsyncMessage("SearchTelemetry:PageInfo", {
        hasAds: true,
        url: doc.documentURI,
      });
    }
  }

  /**
   * Handles events received from the actor child notifications.
   *
   * @param {object} event The event details.
   */
  handleEvent(event) {
    // We are only interested in the top-level frame.
    if (event.target.ownerGlobal != this.content) {
      return;
    }

    switch (event.type) {
      case "pageshow": {
        // If a page is loaded from the bfcache, we won't get a "DOMContentLoaded"
        // event, so we need to rely on "pageshow" in this case. Note: we do this
        // so that we remain consistent with the *.in-content:sap* count for the
        // SEARCH_COUNTS histogram.
        if (event.persisted) {
          this._checkForAdLink(this.content.document);
        }
        break;
      }
      case "DOMContentLoaded": {
        this._checkForAdLink(this.content.document);
        break;
      }
    }
  }
}
