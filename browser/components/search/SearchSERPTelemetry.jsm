/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

var EXPORTED_SYMBOLS = ["SearchSERPTelemetry"];

const { XPCOMUtils } = ChromeUtils.import(
  "resource://gre/modules/XPCOMUtils.jsm"
);
const { Services } = ChromeUtils.import("resource://gre/modules/Services.jsm");

const lazy = {};

XPCOMUtils.defineLazyModuleGetters(lazy, {
  BrowserSearchTelemetry: "resource:///modules/BrowserSearchTelemetry.jsm",
  RemoteSettings: "resource://services-settings/remote-settings.js",
  SearchUtils: "resource://gre/modules/SearchUtils.jsm",
});

// The various histograms and scalars that we report to.
const SEARCH_COUNTS_HISTOGRAM_KEY = "SEARCH_COUNTS";
const SEARCH_CONTENT_SCALAR_BASE = "browser.search.content.";
const SEARCH_WITH_ADS_SCALAR_OLD = "browser.search.with_ads";
const SEARCH_WITH_ADS_SCALAR_BASE = "browser.search.withads.";
const SEARCH_AD_CLICKS_SCALAR_OLD = "browser.search.ad_clicks";
const SEARCH_AD_CLICKS_SCALAR_BASE = "browser.search.adclicks.";
const SEARCH_DATA_TRANSFERRED_SCALAR = "browser.search.data_transferred";
const SEARCH_TELEMETRY_PRIVATE_BROWSING_KEY_SUFFIX = "pb";

const TELEMETRY_SETTINGS_KEY = "search-telemetry-v2";

XPCOMUtils.defineLazyGetter(lazy, "logConsole", () => {
  return console.createInstance({
    prefix: "SearchTelemetry",
    maxLogLevel: lazy.SearchUtils.loggingEnabled ? "Debug" : "Warn",
  });
});

/**
 * TelemetryHandler is the main class handling Search Engine Result Page (SERP)
 * telemetry. It primarily deals with tracking of what pages are loaded into tabs.
 *
 * It handles the *in-content:sap* keys of the SEARCH_COUNTS histogram.
 */
class TelemetryHandler {
  // Whether or not this class is initialised.
  _initialized = false;

  // An instance of ContentHandler.
  _contentHandler;

  // The original provider information, mainly used for tests.
  _originalProviderInfo = null;

  // The current search provider info.
  _searchProviderInfo = null;

  // An instance of remote settings that is used to access the provider info.
  _telemetrySettings;

  // _browserInfoByURL is a map of tracked search urls to objects containing:
  // * {object} info
  //   the search provider information associated with the url.
  // * {WeakMap} browsers
  //   a weak map of browsers that have the url loaded and their ad report state.
  // * {integer} count
  //   a manual count of browsers logged.
  // We keep a weak map of browsers, in case we miss something on our counts
  // and cause a memory leak - worst case our map is slightly bigger than it
  // needs to be.
  // The manual count is because WeakMap doesn't give us size/length
  // information, but we want to know when we can clean up our associated
  // entry.
  _browserInfoByURL = new Map();

  // _browserSourceMap is a map of the latest search source for a particular
  // browser - one of the KNOWN_SEARCH_SOURCES in BrowserSearchTelemetry.
  _browserSourceMap = new WeakMap();

  constructor() {
    this._contentHandler = new ContentHandler({
      browserInfoByURL: this._browserInfoByURL,
      findBrowserItemForURL: (...args) => this._findBrowserItemForURL(...args),
      getProviderInfoForURL: (...args) => this._getProviderInfoForURL(...args),
      checkURLForSerpMatch: (...args) => this._checkURLForSerpMatch(...args),
    });
  }

  /**
   * Initializes the TelemetryHandler and its ContentHandler. It will add
   * appropriate listeners to the window so that window opening and closing
   * can be tracked.
   */
  async init() {
    if (this._initialized) {
      return;
    }

    this._telemetrySettings = lazy.RemoteSettings(TELEMETRY_SETTINGS_KEY);
    let rawProviderInfo = [];
    try {
      rawProviderInfo = await this._telemetrySettings.get();
    } catch (ex) {
      lazy.logConsole.error("Could not get settings:", ex);
    }

    // Send the provider info to the child handler.
    this._contentHandler.init(rawProviderInfo);
    this._originalProviderInfo = rawProviderInfo;

    // Now convert the regexps into
    this._setSearchProviderInfo(rawProviderInfo);

    for (let win of Services.wm.getEnumerator("navigator:browser")) {
      this._registerWindow(win);
    }
    Services.wm.addListener(this);

    this._initialized = true;
  }

  /**
   * Uninitializes the TelemetryHandler and its ContentHandler.
   */
  uninit() {
    if (!this._initialized) {
      return;
    }

    this._contentHandler.uninit();

    for (let win of Services.wm.getEnumerator("navigator:browser")) {
      this._unregisterWindow(win);
    }
    Services.wm.removeListener(this);

    this._initialized = false;
  }

  /**
   * Records the search source for particular browsers, in case it needs
   * to be associated with a SERP.
   *
   * @param {browser} browser
   *   The browser where the search originated.
   * @param {string} source
   *    Where the search originated from.
   */
  recordBrowserSource(browser, source) {
    this._browserSourceMap.set(browser, source);
  }

  /**
   * Handles the TabClose event received from the listeners.
   *
   * @param {object} event
   */
  handleEvent(event) {
    if (event.type != "TabClose") {
      Cu.reportError(`Received unexpected event type ${event.type}`);
      return;
    }

    this.stopTrackingBrowser(event.target.linkedBrowser);
  }

  /**
   * Test-only function, used to override the provider information, so that
   * unit tests can set it to easy to test values.
   *
   * @param {array} providerInfo @see search-telemetry-schema.json for type information.
   */
  overrideSearchTelemetryForTests(providerInfo) {
    let info = providerInfo ? providerInfo : this._originalProviderInfo;
    this._contentHandler.overrideSearchTelemetryForTests(info);
    this._setSearchProviderInfo(info);
  }

  /**
   * Used to set the local version of the search provider information.
   * This automatically maps the regexps to RegExp objects so that
   * we don't have to create a new instance each time.
   *
   * @param {array} providerInfo
   *   A raw array of provider information to set.
   */
  _setSearchProviderInfo(providerInfo) {
    this._searchProviderInfo = providerInfo.map(provider => {
      let newProvider = {
        ...provider,
        searchPageRegexp: new RegExp(provider.searchPageRegexp),
      };
      if (provider.extraAdServersRegexps) {
        newProvider.extraAdServersRegexps = provider.extraAdServersRegexps.map(
          r => new RegExp(r)
        );
      }
      return newProvider;
    });
  }

  reportPageWithAds(info, browser) {
    this._contentHandler._reportPageWithAds(info, browser);
  }

  /**
   * This may start tracking a tab based on the URL. If the URL matches a search
   * partner, and it has a code, then we'll start tracking it. This will aid
   * determining if it is a page we should be tracking for adverts.
   *
   * @param {object} browser
   *   The browser associated with the page.
   * @param {string} url
   *   The url that was loaded in the browser.
   * @param {nsIDocShell.LoadCommand} loadType
   *   The load type associated with the page load.
   */
  updateTrackingStatus(browser, url, loadType) {
    if (
      !lazy.BrowserSearchTelemetry.shouldRecordSearchCount(
        browser.getTabBrowser()
      )
    ) {
      return;
    }
    let info = this._checkURLForSerpMatch(url);
    if (!info) {
      this.stopTrackingBrowser(browser);
      return;
    }

    let source = "unknown";
    if (loadType & Ci.nsIDocShell.LOAD_CMD_RELOAD) {
      source = "reload";
    } else if (loadType & Ci.nsIDocShell.LOAD_CMD_HISTORY) {
      source = "tabhistory";
    } else if (this._browserSourceMap.has(browser)) {
      source = this._browserSourceMap.get(browser);
      this._browserSourceMap.delete(browser);
    }

    this._reportSerpPage(info, source, url);

    let item = this._browserInfoByURL.get(url);

    if (item) {
      item.browsers.set(browser, "no ads reported");
      item.count++;
      item.source = source;
    } else {
      item = this._browserInfoByURL.set(url, {
        browsers: new WeakMap().set(browser, "no ads reported"),
        info,
        count: 1,
        source,
      });
    }
  }

  /**
   * Stops tracking of a tab, for example the tab has loaded a different URL.
   *
   * @param {object} browser The browser associated with the tab to stop being
   *                         tracked.
   */
  stopTrackingBrowser(browser) {
    for (let [url, item] of this._browserInfoByURL) {
      if (item.browsers.has(browser)) {
        item.browsers.delete(browser);
        item.count--;
      }

      if (!item.count) {
        this._browserInfoByURL.delete(url);
      }
    }
  }

  /**
   * Parts of the URL, like search params and hashes, may be mutated by scripts
   * on a page we're tracking. Since we don't want to keep track of that
   * ourselves in order to keep the list of browser objects a weak-referenced
   * set, we do optional fuzzy matching of URLs to fetch the most relevant item
   * that contains tracking information.
   *
   * @param {string} url URL to fetch the tracking data for.
   * @returns {object} Map containing the following members:
   *   - {WeakMap} browsers
   *     Map of browser elements that belong to `url` and their ad report state.
   *   - {object} info
   *     Info dictionary as returned by `_checkURLForSerpMatch`.
   *   - {number} count
   *     The number of browser element we can most accurately tell we're
   *     tracking, since they're inside a WeakMap.
   */
  _findBrowserItemForURL(url) {
    try {
      url = new URL(url);
    } catch (ex) {
      return null;
    }

    const compareURLs = (url1, url2) => {
      // In case of an exact match, well, that's an obvious winner.
      if (url1.href == url2.href) {
        return Infinity;
      }

      // Each step we get closer to the two URLs being the same, we increase the
      // score. The consumer of this method will use these scores to see which
      // of the URLs is the best match.
      let score = 0;
      if (url1.hostname == url2.hostname) {
        ++score;
        if (url1.pathname == url2.pathname) {
          ++score;
          for (let [key1, value1] of url1.searchParams) {
            // Let's not fuss about the ordering of search params, since the
            // score effect will solve that.
            if (url2.searchParams.has(key1)) {
              ++score;
              if (url2.searchParams.get(key1) == value1) {
                ++score;
              }
            }
          }
          if (url1.hash == url2.hash) {
            ++score;
          }
        }
      }
      return score;
    };

    let item;
    let currentBestMatch = 0;
    for (let [trackingURL, candidateItem] of this._browserInfoByURL) {
      if (currentBestMatch === Infinity) {
        break;
      }
      try {
        // Make sure to cache the parsed URL object, since there's no reason to
        // do it twice.
        trackingURL =
          candidateItem._trackingURL ||
          (candidateItem._trackingURL = new URL(trackingURL));
      } catch (ex) {
        continue;
      }
      let score = compareURLs(url, trackingURL);
      if (score > currentBestMatch) {
        item = candidateItem;
        currentBestMatch = score;
      }
    }

    return item;
  }

  // nsIWindowMediatorListener

  /**
   * This is called when a new window is opened, and handles registration of
   * that window if it is a browser window.
   *
   * @param {nsIAppWindow} appWin The xul window that was opened.
   */
  onOpenWindow(appWin) {
    let win = appWin.docShell.domWindow;
    win.addEventListener(
      "load",
      () => {
        if (
          win.document.documentElement.getAttribute("windowtype") !=
          "navigator:browser"
        ) {
          return;
        }

        this._registerWindow(win);
      },
      { once: true }
    );
  }

  /**
   * Listener that is called when a window is closed, and handles deregistration of
   * that window if it is a browser window.
   *
   * @param {nsIAppWindow} appWin The xul window that was closed.
   */
  onCloseWindow(appWin) {
    let win = appWin.docShell.domWindow;

    if (
      win.document.documentElement.getAttribute("windowtype") !=
      "navigator:browser"
    ) {
      return;
    }

    this._unregisterWindow(win);
  }

  /**
   * Adds event listeners for the window and registers it with the content handler.
   *
   * @param {object} win The window to register.
   */
  _registerWindow(win) {
    win.gBrowser.tabContainer.addEventListener("TabClose", this);
  }

  /**
   * Removes event listeners for the window and unregisters it with the content
   * handler.
   *
   * @param {object} win The window to unregister.
   */
  _unregisterWindow(win) {
    for (let tab of win.gBrowser.tabs) {
      this.stopTrackingBrowser(tab);
    }

    win.gBrowser.tabContainer.removeEventListener("TabClose", this);
  }

  /**
   * Searches for provider information for a given url.
   *
   * @param {string} url The url to match for a provider.
   * @param {boolean} useOnlyExtraAdServers If true, this will use the extra
   *   ad server regexp to match instead of the main regexp.
   * @returns {array|null} Returns an array of provider name and the provider information.
   */
  _getProviderInfoForURL(url, useOnlyExtraAdServers = false) {
    if (useOnlyExtraAdServers) {
      return this._searchProviderInfo.find(info => {
        if (info.extraAdServersRegexps) {
          for (let regexp of info.extraAdServersRegexps) {
            if (regexp.test(url)) {
              return true;
            }
          }
        }
        return false;
      });
    }

    return this._searchProviderInfo.find(info =>
      info.searchPageRegexp.test(url)
    );
  }

  /**
   * Checks to see if a url is a search partner location, and determines the
   * provider and codes used.
   *
   * @param {string} url The url to match.
   * @returns {null|object} Returns null if there is no match found. Otherwise,
   *   returns an object of strings for provider, code and type.
   */
  _checkURLForSerpMatch(url) {
    let searchProviderInfo = this._getProviderInfoForURL(url);
    if (!searchProviderInfo) {
      return null;
    }
    let queries = new URLSearchParams(url.split("#")[0].split("?")[1]);
    if (!queries.get(searchProviderInfo.queryParamName)) {
      return null;
    }
    // Default to organic to simplify things.
    // We override type in the sap cases.
    // We have an oldType and type split, because the older telemetry uses "sap"
    // and "sap-follow-on" versus "tagged-sap" and "tagged-follow-on".
    // The latter is a more accurate description of what we're reporting.
    let oldType = "organic";
    let type = "organic";
    let code;
    if (searchProviderInfo.codeParamName) {
      code = queries.get(searchProviderInfo.codeParamName);
      if (code) {
        // The code is only included if it matches one of the specific ones.
        if (searchProviderInfo.taggedCodes.includes(code)) {
          oldType = "sap";
          type = "tagged";
          if (
            searchProviderInfo.followOnParamNames &&
            searchProviderInfo.followOnParamNames.some(p => queries.has(p))
          ) {
            oldType += "-follow-on";
            type += "-follow-on";
          }
        } else if (searchProviderInfo.organicCodes.includes(code)) {
          oldType = "organic";
          type = "organic";
        } else if (searchProviderInfo.expectedOrganicCodes?.includes(code)) {
          code = "none";
        } else {
          code = "other";
        }
      } else if (searchProviderInfo.followOnCookies) {
        // Especially Bing requires lots of extra work related to cookies.
        for (let followOnCookie of searchProviderInfo.followOnCookies) {
          if (followOnCookie.extraCodeParamName) {
            let eCode = queries.get(followOnCookie.extraCodeParamName);
            if (
              !eCode ||
              !followOnCookie.extraCodePrefixes.some(p => eCode.startsWith(p))
            ) {
              continue;
            }
          }

          // If this cookie is present, it's probably an SAP follow-on.
          // This might be an organic follow-on in the same session, but there
          // is no way to tell the difference.
          for (let cookie of Services.cookies.getCookiesFromHost(
            followOnCookie.host,
            {}
          )) {
            if (cookie.name != followOnCookie.name) {
              continue;
            }

            let [cookieParam, cookieValue] = cookie.value
              .split("=")
              .map(p => p.trim());
            if (
              cookieParam == followOnCookie.codeParamName &&
              searchProviderInfo.taggedCodes.includes(cookieValue)
            ) {
              oldType = "sap-follow-on";
              type = "tagged-follow-on";
              code = cookieValue;
              break;
            }
          }
        }
      }
    }
    return { provider: searchProviderInfo.telemetryId, oldType, type, code };
  }

  /**
   * Logs telemetry for a search provider visit.
   *
   * @param {object} info
   * @param {string} info.provider The name of the provider.
   * @param {string} info.oldType The type of search.
   * @param {string} [info.code] The code for the provider.
   * @param {string} source Where the search originated from.
   * @param {string} url The url that was matched (for debug logging only).
   */
  _reportSerpPage(info, source, url) {
    Services.telemetry.keyedScalarAdd(
      SEARCH_CONTENT_SCALAR_BASE + source,
      `${info.provider}:${info.type}:${info.code || "none"}`,
      1
    );

    // SEARCH_COUNTS is now obsolete with the new scalar above, but is being
    // kept whilst data is verified and telemetry is transitioned.
    let payload = `${info.provider}.in-content:${info.oldType}:${info.code ||
      "none"}`;
    let histogram = Services.telemetry.getKeyedHistogramById(
      SEARCH_COUNTS_HISTOGRAM_KEY
    );
    histogram.add(payload);
    lazy.logConsole.debug("Counting", payload, "for", url);
  }
}

/**
 * ContentHandler deals with handling telemetry of the content within a tab -
 * when ads detected and when they are selected.
 *
 * It handles the "browser.search.with_ads" and "browser.search.ad_clicks"
 * scalars.
 */
class ContentHandler {
  /**
   * Constructor.
   *
   * @param {object} options
   * @param {Map} options.browserInfoByURL The  map of urls from TelemetryHandler.
   * @param {function} options.getProviderInfoForURL A function that obtains
   *   the provider information for a url.
   */
  constructor(options) {
    this._browserInfoByURL = options.browserInfoByURL;
    this._findBrowserItemForURL = options.findBrowserItemForURL;
    this._getProviderInfoForURL = options.getProviderInfoForURL;
    this._checkURLForSerpMatch = options.checkURLForSerpMatch;
  }

  /**
   * Initializes the content handler. This will also set up the shared data that is
   * shared with the SearchTelemetryChild actor.
   *
   * @param {array} providerInfo
   *  The provider information for the search telemetry to record.
   */
  init(providerInfo) {
    Services.ppmm.sharedData.set("SearchTelemetry:ProviderInfo", providerInfo);

    Services.obs.addObserver(this, "http-on-examine-response");
    Services.obs.addObserver(this, "http-on-stop-request");
  }

  /**
   * Uninitializes the content handler.
   */
  uninit() {
    Services.obs.removeObserver(this, "http-on-examine-response");
    Services.obs.removeObserver(this, "http-on-stop-request");
  }

  /**
   * Test-only function to override the search provider information for use
   * with tests. Passes it to the SearchTelemetryChild actor.
   *
   * @param {object} providerInfo @see SEARCH_PROVIDER_INFO for type information.
   */
  overrideSearchTelemetryForTests(providerInfo) {
    Services.ppmm.sharedData.set("SearchTelemetry:ProviderInfo", providerInfo);
  }

  /**
   * Reports bandwidth used by the given channel if it is used by search requests.
   *
   * @param {object} aChannel The channel that generated the activity.
   */
  _reportChannelBandwidth(aChannel) {
    if (!(aChannel instanceof Ci.nsIChannel)) {
      return;
    }
    let wrappedChannel = ChannelWrapper.get(aChannel);

    let getTopURL = channel => {
      // top-level document
      if (
        channel.loadInfo &&
        channel.loadInfo.externalContentPolicyType ==
          Ci.nsIContentPolicy.TYPE_DOCUMENT
      ) {
        return channel.finalURL;
      }

      // iframe
      let frameAncestors;
      try {
        frameAncestors = channel.frameAncestors;
      } catch (e) {
        frameAncestors = null;
      }
      if (frameAncestors) {
        let ancestor = frameAncestors.find(obj => obj.frameId == 0);
        if (ancestor) {
          return ancestor.url;
        }
      }

      // top-level resource
      if (channel.loadInfo && channel.loadInfo.loadingPrincipal) {
        return channel.loadInfo.loadingPrincipal.spec;
      }

      return null;
    };

    let topUrl = getTopURL(wrappedChannel);
    if (!topUrl) {
      return;
    }

    let info = this._checkURLForSerpMatch(topUrl);
    if (!info) {
      return;
    }

    let bytesTransferred =
      wrappedChannel.requestSize + wrappedChannel.responseSize;
    let { provider } = info;

    let isPrivate =
      wrappedChannel.loadInfo &&
      wrappedChannel.loadInfo.originAttributes.privateBrowsingId > 0;
    if (isPrivate) {
      provider += `-${SEARCH_TELEMETRY_PRIVATE_BROWSING_KEY_SUFFIX}`;
    }

    Services.telemetry.keyedScalarAdd(
      SEARCH_DATA_TRANSFERRED_SCALAR,
      provider,
      bytesTransferred
    );
  }

  observe(aSubject, aTopic, aData) {
    switch (aTopic) {
      case "http-on-stop-request":
        this._reportChannelBandwidth(aSubject);
        break;
      case "http-on-examine-response":
        this.observeActivity(aSubject);
        break;
    }
  }

  /**
   * Listener that observes network activity, so that we can determine if a link
   * from a search provider page was followed, and if then if that link was an
   * ad click or not.
   *
   * @param {nsIChannel} channel   The channel that generated the activity.
   */
  observeActivity(channel) {
    if (!(channel instanceof Ci.nsIChannel)) {
      return;
    }

    let wrappedChannel = ChannelWrapper.get(channel);
    if (wrappedChannel._adClickRecorded) {
      lazy.logConsole.debug("Ad click already recorded");
      return;
    }

    Services.tm.dispatchToMainThread(() => {
      // We suspect that No Content (204) responses are used to transfer or
      // update beacons. They used to lead to double-counting ad-clicks, so let's
      // ignore them.
      if (wrappedChannel.statusCode == 204) {
        lazy.logConsole.debug("Ignoring activity from ambiguous responses");
        return;
      }

      // The wrapper is consistent across redirects, so we can use it to track state.
      let originURL = wrappedChannel.originURI && wrappedChannel.originURI.spec;
      let item = this._findBrowserItemForURL(originURL);
      if (!originURL || !item) {
        return;
      }

      let URL = wrappedChannel.finalURL;

      let info = this._getProviderInfoForURL(URL, true);
      if (!info) {
        return;
      }

      try {
        lazy.logConsole.debug(
          "Counting ad click in page for",
          info.telemetryId,
          item.source,
          originURL,
          URL
        );
        Services.telemetry.keyedScalarAdd(
          SEARCH_AD_CLICKS_SCALAR_OLD,
          `${info.telemetryId}:${item.info.oldType}`,
          1
        );
        Services.telemetry.keyedScalarAdd(
          SEARCH_AD_CLICKS_SCALAR_BASE + item.source,
          `${info.telemetryId}:${item.info.type}`,
          1
        );
        wrappedChannel._adClickRecorded = true;
      } catch (e) {
        Cu.reportError(e);
      }
    });
  }

  /**
   * Logs telemetry for a page with adverts, if it is one of the partner search
   * provider pages that we're tracking.
   *
   * @param {object} info
   * @param {boolean} info.hasAds
   *     Whether or not the page has adverts.
   * @param {string} info.url
   *     The url of the page.
   * @param {object} browser
   *     The browser associated with the page.
   */
  _reportPageWithAds(info, browser) {
    let item = this._findBrowserItemForURL(info.url);
    if (!item) {
      lazy.logConsole.warn(
        "Expected to report URI for",
        info.url,
        "with ads but couldn't find the information"
      );
      return;
    }

    let adReportState = item.browsers.get(browser);
    if (adReportState == "ad reported") {
      lazy.logConsole.debug(
        "Ad was previously reported for browser with URI",
        info.url
      );
      return;
    }

    lazy.logConsole.debug(
      "Counting ads in page for",
      item.info.provider,
      item.info.type,
      item.source,
      info.url
    );
    Services.telemetry.keyedScalarAdd(
      SEARCH_WITH_ADS_SCALAR_OLD,
      `${item.info.provider}:${item.info.oldType}`,
      1
    );
    Services.telemetry.keyedScalarAdd(
      SEARCH_WITH_ADS_SCALAR_BASE + item.source,
      `${item.info.provider}:${item.info.type}`,
      1
    );

    item.browsers.set(browser, "ad reported");
  }
}

var SearchSERPTelemetry = new TelemetryHandler();
