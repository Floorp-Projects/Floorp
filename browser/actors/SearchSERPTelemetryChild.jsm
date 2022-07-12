/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
"use strict";

var EXPORTED_SYMBOLS = ["SearchSERPTelemetryChild", "ADLINK_CHECK_TIMEOUT_MS"];

const { XPCOMUtils } = ChromeUtils.importESModule(
  "resource://gre/modules/XPCOMUtils.sys.mjs"
);

const lazy = {};

XPCOMUtils.defineLazyModuleGetters(lazy, {
  clearTimeout: "resource://gre/modules/Timer.jsm",
  setTimeout: "resource://gre/modules/Timer.jsm",
});

const SHARED_DATA_KEY = "SearchTelemetry:ProviderInfo";
const ADLINK_CHECK_TIMEOUT_MS = 1000;

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
    this._searchProviderInfo = this._searchProviderInfo
      .filter(p => "extraAdServersRegexps" in p)
      .map(p => {
        return {
          ...p,
          searchPageRegexp: new RegExp(p.searchPageRegexp),
          extraAdServersRegexps: p.extraAdServersRegexps.map(
            r => new RegExp(r)
          ),
        };
      });

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
class SearchSERPTelemetryChild extends JSWindowActorChild {
  /**
   * Determines if there is a provider that matches the supplied URL and returns
   * the information associated with that provider.
   *
   * @param {string} url The url to check
   * @returns {array|null} Returns null if there's no match, otherwise an array
   *   of provider name and the provider information.
   */
  _getProviderInfoForUrl(url) {
    return searchProviders.info?.find(info => info.searchPageRegexp.test(url));
  }

  /**
   * Checks to see if the page is a partner and has an ad link within it. If so,
   * it will notify SearchTelemetry.
   */
  _checkForAdLink() {
    try {
      if (!this.contentWindow) {
        return;
      }
    } catch (ex) {
      // unload occurred before the timer expired
      return;
    }

    let doc = this.document;
    let url = doc.documentURI;
    let providerInfo = this._getProviderInfoForUrl(url);
    if (!providerInfo) {
      return;
    }

    let regexps = providerInfo.extraAdServersRegexps;
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
        url,
      });
    }
  }

  /**
   * Handles events received from the actor child notifications.
   *
   * @param {object} event The event details.
   */
  handleEvent(event) {
    const cancelCheck = () => {
      if (this._waitForContentTimeout) {
        lazy.clearTimeout(this._waitForContentTimeout);
      }
    };

    const check = () => {
      cancelCheck();
      this._waitForContentTimeout = lazy.setTimeout(() => {
        this._checkForAdLink();
      }, ADLINK_CHECK_TIMEOUT_MS);
    };

    switch (event.type) {
      case "pageshow": {
        // If a page is loaded from the bfcache, we won't get a "DOMContentLoaded"
        // event, so we need to rely on "pageshow" in this case. Note: we do this
        // so that we remain consistent with the *.in-content:sap* count for the
        // SEARCH_COUNTS histogram.
        if (event.persisted) {
          check();
        }
        break;
      }
      case "DOMContentLoaded": {
        check();
        break;
      }
      case "load": {
        // We check both DOMContentLoaded and load in case the page has
        // taken a long time to load and the ad is only detected on load.
        // We still check at DOMContentLoaded because if the page hasn't
        // finished loading and the user navigates away, we still want to know
        // if there were ads on the page or not at that time.
        check();
        break;
      }
      case "unload": {
        cancelCheck();
        break;
      }
    }
  }
}
