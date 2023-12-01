/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

import { XPCOMUtils } from "resource://gre/modules/XPCOMUtils.sys.mjs";

const lazy = {};

ChromeUtils.defineESModuleGetters(lazy, {
  BrowserSearchTelemetry: "resource:///modules/BrowserSearchTelemetry.sys.mjs",
  PrivateBrowsingUtils: "resource://gre/modules/PrivateBrowsingUtils.sys.mjs",
  RemoteSettings: "resource://services-settings/remote-settings.sys.mjs",
  SearchUtils: "resource://gre/modules/SearchUtils.sys.mjs",
});

ChromeUtils.defineLazyGetter(lazy, "gCryptoHash", () => {
  return Cc["@mozilla.org/security/hash;1"].createInstance(Ci.nsICryptoHash);
});

// The various histograms and scalars that we report to.
const SEARCH_CONTENT_SCALAR_BASE = "browser.search.content.";
const SEARCH_WITH_ADS_SCALAR_BASE = "browser.search.withads.";
const SEARCH_AD_CLICKS_SCALAR_BASE = "browser.search.adclicks.";
const SEARCH_DATA_TRANSFERRED_SCALAR = "browser.search.data_transferred";
const SEARCH_TELEMETRY_PRIVATE_BROWSING_KEY_SUFFIX = "pb";

// Exported for tests.
export const ADLINK_CHECK_TIMEOUT_MS = 1000;
export const TELEMETRY_SETTINGS_KEY = "search-telemetry-v2";
export const TELEMETRY_CATEGORIZATION_KEY = "search-categorization";
export const TELEMETRY_CATEGORIZATION_DOWNLOAD_SETTINGS = {
  // Units are in milliseconds.
  base: 3600000,
  minAdjust: 60000,
  maxAdjust: 600000,
  maxTriesPerSession: 2,
};

export const SEARCH_TELEMETRY_SHARED = {
  PROVIDER_INFO: "SearchTelemetry:ProviderInfo",
  LOAD_TIMEOUT: "SearchTelemetry:LoadTimeout",
};

const impressionIdsWithoutEngagementsSet = new Set();

const maxDomainsToCategorize = 10;

ChromeUtils.defineLazyGetter(lazy, "logConsole", () => {
  return console.createInstance({
    prefix: "SearchTelemetry",
    maxLogLevel: lazy.SearchUtils.loggingEnabled ? "Debug" : "Warn",
  });
});

XPCOMUtils.defineLazyPreferenceGetter(
  lazy,
  "serpEventsEnabled",
  "browser.search.serpEventTelemetry.enabled",
  true
);

const CATEGORIZATION_PREF =
  "browser.search.serpEventTelemetryCategorization.enabled";

XPCOMUtils.defineLazyPreferenceGetter(
  lazy,
  "serpEventTelemetryCategorization",
  CATEGORIZATION_PREF,
  false
);

export var SearchSERPTelemetryUtils = {
  ACTIONS: {
    CLICKED: "clicked",
    EXPANDED: "expanded",
    SUBMITTED: "submitted",
  },
  COMPONENTS: {
    AD_CAROUSEL: "ad_carousel",
    AD_LINK: "ad_link",
    AD_SIDEBAR: "ad_sidebar",
    AD_SITELINK: "ad_sitelink",
    INCONTENT_SEARCHBOX: "incontent_searchbox",
    NON_ADS_LINK: "non_ads_link",
    REFINED_SEARCH_BUTTONS: "refined_search_buttons",
    SHOPPING_TAB: "shopping_tab",
  },
  ABANDONMENTS: {
    NAVIGATION: "navigation",
    TAB_CLOSE: "tab_close",
    WINDOW_CLOSE: "window_close",
  },
  INCONTENT_SOURCES: {
    OPENED_IN_NEW_TAB: "opened_in_new_tab",
    REFINE_ON_SERP: "follow_on_from_refine_on_SERP",
    SEARCHBOX: "follow_on_from_refine_on_incontent_search",
  },
  CATEGORIZATION: {
    INCONCLUSIVE: 0,
  },
};

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

  // Callback used when syncing telemetry settings.
  #telemetrySettingsSync;

  // _browserInfoByURL is a map of tracked search urls to objects containing:
  // * {object} info
  //   the search provider information associated with the url.
  // * {WeakMap} browserTelemetryStateMap
  //   a weak map of browsers that have the url loaded, their ad report state,
  //   and their impression id.
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

  /**
   * A WeakMap whose key is a browser with value of a source type found in
   * INCONTENT_SOURCES. Kept separate to avoid overlapping with legacy
   * search sources. These sources are specific to the content of a search
   * provider page rather than something from within the browser itself.
   */
  #browserContentSourceMap = new WeakMap();

  /**
   * Sets the source of a SERP visit from something that occured in content
   * rather than from the browser.
   *
   * @param {browser} browser
   *   The browser object associated with the page that should be a SERP.
   * @param {string} source
   *   The source that started the load. One of
   *   SearchSERPTelemetryUtils.COMPONENTS.INCONTENT_SEARCHBOX,
   *   SearchSERPTelemetryUtils.INCONTENT_SOURCES.OPENED_IN_NEW_TAB or
   *   SearchSERPTelemetryUtils.INCONTENT_SOURCES.REFINE_ON_SERP.
   */
  setBrowserContentSource(browser, source) {
    this.#browserContentSourceMap.set(browser, source);
  }

  // _browserNewtabSessionMap is a map of the newtab session id for particular
  // browsers.
  _browserNewtabSessionMap = new WeakMap();

  constructor() {
    this._contentHandler = new ContentHandler({
      browserInfoByURL: this._browserInfoByURL,
      findBrowserItemForURL: (...args) => this._findBrowserItemForURL(...args),
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

    this.#telemetrySettingsSync = event => this.#onSettingsSync(event);
    this._telemetrySettings.on("sync", this.#telemetrySettingsSync);

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

  async #onSettingsSync(event) {
    let current = event.data?.current;
    if (current) {
      lazy.logConsole.debug(
        "Update provider info due to Remote Settings sync."
      );
      this._originalProviderInfo = current;
      this._setSearchProviderInfo(current);
      Services.ppmm.sharedData.set(
        SEARCH_TELEMETRY_SHARED.PROVIDER_INFO,
        current
      );
      Services.ppmm.sharedData.flush();
    } else {
      lazy.logConsole.debug(
        "Ignoring Remote Settings sync data due to missing records."
      );
    }
    Services.obs.notifyObservers(null, "search-telemetry-v2-synced");
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

    try {
      this._telemetrySettings.off("sync", this.#telemetrySettingsSync);
    } catch (ex) {
      lazy.logConsole.error(
        "Failed to shutdown SearchSERPTelemetry Remote Settings.",
        ex
      );
    }
    this._telemetrySettings = null;
    this.#telemetrySettingsSync = null;

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
   * Records the newtab source for particular browsers, in case it needs
   * to be associated with a SERP.
   *
   * @param {browser} browser
   *   The browser where the search originated.
   * @param {string} newtabSessionId
   *    The sessionId of the newtab session the search originated from.
   */
  recordBrowserNewtabSession(browser, newtabSessionId) {
    this._browserNewtabSessionMap.set(browser, newtabSessionId);
  }

  /**
   * Helper function for recording the reason for a Glean abandonment event.
   *
   * @param {string} impressionId
   *    The impression id for the abandonment event about to be recorded.
   * @param {string} reason
   *    The reason the SERP is deemed abandoned.
   *    One of SearchSERPTelemetryUtils.ABANDONMENTS.
   */
  recordAbandonmentTelemetry(impressionId, reason) {
    impressionIdsWithoutEngagementsSet.delete(impressionId);

    lazy.logConsole.debug(
      `Recording an abandonment event for impression id ${impressionId} with reason: ${reason}`
    );

    Glean.serp.abandonment.record({
      impression_id: impressionId,
      reason,
    });
  }

  /**
   * Handles the TabClose event received from the listeners.
   *
   * @param {object} event
   *   The event object provided by the listener.
   */
  handleEvent(event) {
    if (event.type != "TabClose") {
      console.error("Received unexpected event type", event.type);
      return;
    }

    this._browserNewtabSessionMap.delete(event.target.linkedBrowser);
    this.stopTrackingBrowser(
      event.target.linkedBrowser,
      SearchSERPTelemetryUtils.ABANDONMENTS.TAB_CLOSE
    );
  }

  /**
   * Test-only function, used to override the provider information, so that
   * unit tests can set it to easy to test values.
   *
   * @param {Array} providerInfo
   *   See {@link https://searchfox.org/mozilla-central/search?q=search-telemetry-schema.json}
   *   for type information.
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
   * @param {Array} providerInfo
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

      newProvider.nonAdsLinkRegexps = provider.nonAdsLinkRegexps?.length
        ? provider.nonAdsLinkRegexps.map(r => new RegExp(r))
        : [];
      if (provider.shoppingTab?.regexp) {
        newProvider.shoppingTab = {
          selector: provider.shoppingTab.selector,
          regexp: new RegExp(provider.shoppingTab.regexp),
        };
      }
      return newProvider;
    });
    this._contentHandler._searchProviderInfo = this._searchProviderInfo;
  }

  reportPageAction(info, browser) {
    this._contentHandler._reportPageAction(info, browser);
  }

  reportPageWithAds(info, browser) {
    this._contentHandler._reportPageWithAds(info, browser);
  }

  reportPageWithAdImpressions(info, browser) {
    this._contentHandler._reportPageWithAdImpressions(info, browser);
  }

  reportPageDomains(info, browser) {
    this._contentHandler._reportPageDomains(info, browser);
  }

  reportPageImpression(info, browser) {
    this._contentHandler._reportPageImpression(info, browser);
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
      this._browserNewtabSessionMap.delete(browser);
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

    // If it's a SERP but doesn't have a browser source, the source might be
    // from something that happened in content. We keep this separate from
    // source because legacy telemetry should not change its reporting.
    let inContentSource;
    if (
      lazy.serpEventsEnabled &&
      info.hasComponents &&
      this.#browserContentSourceMap.has(browser)
    ) {
      inContentSource = this.#browserContentSourceMap.get(browser);
      this.#browserContentSourceMap.delete(browser);
    }

    let newtabSessionId;
    if (this._browserNewtabSessionMap.has(browser)) {
      newtabSessionId = this._browserNewtabSessionMap.get(browser);
      // We leave the newtabSessionId in the map for this browser
      // until we stop loading SERP pages or the tab is closed.
    }

    let impressionId;
    if (lazy.serpEventsEnabled && info.hasComponents) {
      // The UUID generated by Services.uuid contains leading and trailing braces.
      // Need to trim them first.
      impressionId = Services.uuid.generateUUID().toString().slice(1, -1);

      impressionIdsWithoutEngagementsSet.add(impressionId);
    }

    this._reportSerpPage(info, source, url);

    let item = this._browserInfoByURL.get(url);

    let impressionInfo;
    if (lazy.serpEventsEnabled && info.hasComponents) {
      let partnerCode = "";
      if (info.code != "none" && info.code != null) {
        partnerCode = info.code;
      }
      impressionInfo = {
        provider: info.provider,
        tagged: info.type.startsWith("tagged"),
        partnerCode,
        source: inContentSource ?? source,
        isShoppingPage: info.isShoppingPage,
        isPrivate: lazy.PrivateBrowsingUtils.isBrowserPrivate(browser),
      };
    }

    if (item) {
      item.browserTelemetryStateMap.set(browser, {
        adsReported: false,
        adImpressionsReported: false,
        impressionId,
        urlToComponentMap: null,
        impressionInfo,
        searchBoxSubmitted: false,
      });
      item.count++;
      item.source = source;
      item.newtabSessionId = newtabSessionId;
    } else {
      item = this._browserInfoByURL.set(url, {
        browserTelemetryStateMap: new WeakMap().set(browser, {
          adsReported: false,
          adImpressionsReported: false,
          impressionId,
          urlToComponentMap: null,
          impressionInfo,
          searchBoxSubmitted: false,
        }),
        info,
        count: 1,
        source,
        newtabSessionId,
      });
    }
  }

  /**
   * Stops tracking of a tab, for example the tab has loaded a different URL.
   * Also records a Glean abandonment event if appropriate.
   *
   * @param {object} browser The browser associated with the tab to stop being
   *   tracked.
   * @param {string} abandonmentReason
   *   An optional parameter that specifies why the browser is deemed abandoned.
   *   The reason will be recorded as part of Glean abandonment telemetry.
   *   One of SearchSERPTelemetryUtils.ABANDONMENTS.
   */
  stopTrackingBrowser(browser, abandonmentReason) {
    for (let [url, item] of this._browserInfoByURL) {
      if (item.browserTelemetryStateMap.has(browser)) {
        let impressionId =
          item.browserTelemetryStateMap.get(browser).impressionId;
        if (impressionIdsWithoutEngagementsSet.has(impressionId)) {
          this.recordAbandonmentTelemetry(impressionId, abandonmentReason);
        }

        item.browserTelemetryStateMap.delete(browser);
        item.count--;
      }

      if (!item.count) {
        this._browserInfoByURL.delete(url);
      }
    }
  }

  /**
   * Calculate how close two urls are in equality.
   *
   * The scoring system:
   * - If the URLs look exactly the same, including the ordering of query
   *   parameters, the score is Infinity.
   * - If the origin is the same, the score is increased by 1. Otherwise the
   *   score is 0.
   * - If the path is the same, the score is increased by 1.
   * - For each query parameter, if the key exists the score is increased by 1.
   *   Likewise if the query parameter values match.
   * - If the hash is the same, the score is increased by 1. This includes if
   *   the hash is missing in both URLs.
   *
   * @param {URL} url1
   *   Url to compare.
   * @param {URL} url2
   *   Other url to compare. Ordering shouldn't matter.
   * @param {object} [matchOptions]
   *   Options for checking equality.
   * @param {boolean} [matchOptions.path]
   *   Whether the path must match. Default to false.
   * @param {boolean} [matchOptions.paramValues]
   *   Whether the values of the query parameters must match if the query
   *   parameter key exists in the other. Defaults to false.
   * @returns {number}
   *   A score of how closely the two URLs match. Returns 0 if there is no
   *   match or the equality check failed for an enabled match option.
   */
  compareUrls(url1, url2, matchOptions = {}) {
    // In case of an exact match, well, that's an obvious winner.
    if (url1.href == url2.href) {
      return Infinity;
    }

    // Each step we get closer to the two URLs being the same, we increase the
    // score. The consumer of this method will use these scores to see which
    // of the URLs is the best match.
    let score = 0;
    if (url1.origin == url2.origin) {
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
            } else if (matchOptions.paramValues) {
              return 0;
            }
          }
        }
        if (url1.hash == url2.hash) {
          ++score;
        }
      } else if (matchOptions.path) {
        return 0;
      }
    }
    return score;
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
      let score = this.compareUrls(url, trackingURL);
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
      this.stopTrackingBrowser(
        tab.linkedBrowser,
        SearchSERPTelemetryUtils.ABANDONMENTS.WINDOW_CLOSE
      );
    }

    win.gBrowser.tabContainer.removeEventListener("TabClose", this);
  }

  /**
   * Searches for provider information for a given url.
   *
   * @param {string} url The url to match for a provider.
   * @returns {Array | null} Returns an array of provider name and the provider information.
   */
  _getProviderInfoForURL(url) {
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
    // Some URLs can match provider info but also be the provider's homepage
    // instead of a SERP.
    // e.g. https://example.com/ vs. https://example.com/?foo=bar
    // To check this, we look for the presence of the query parameter
    // that contains a search term.
    let queries = new URLSearchParams(url.split("#")[0].split("?")[1]);
    let hasQuery = false;
    for (let queryParamName of searchProviderInfo.queryParamNames) {
      if (queries.get(queryParamName)) {
        hasQuery = true;
        break;
      }
    }
    if (!hasQuery) {
      return null;
    }
    // Default to organic to simplify things.
    // We override type in the sap cases.
    let type = "organic";
    let code;
    if (searchProviderInfo.codeParamName) {
      code = queries.get(searchProviderInfo.codeParamName);
      if (code) {
        // The code is only included if it matches one of the specific ones.
        if (searchProviderInfo.taggedCodes.includes(code)) {
          type = "tagged";
          if (
            searchProviderInfo.followOnParamNames &&
            searchProviderInfo.followOnParamNames.some(p => queries.has(p))
          ) {
            type += "-follow-on";
          }
        } else if (searchProviderInfo.organicCodes.includes(code)) {
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
              type = "tagged-follow-on";
              code = cookieValue;
              break;
            }
          }
        }
      }
    }
    let isShoppingPage = false;
    let hasComponents = false;
    if (lazy.serpEventsEnabled) {
      if (searchProviderInfo.shoppingTab?.regexp) {
        isShoppingPage = searchProviderInfo.shoppingTab.regexp.test(url);
      }
      if (searchProviderInfo.components?.length) {
        hasComponents = true;
      }
    }
    return {
      provider: searchProviderInfo.telemetryId,
      type,
      code,
      isShoppingPage,
      hasComponents,
    };
  }

  /**
   * Logs telemetry for a search provider visit.
   *
   * @param {object} info The search provider information.
   * @param {string} info.provider The name of the provider.
   * @param {string} info.type The type of search.
   * @param {string} [info.code] The code for the provider.
   * @param {string} source Where the search originated from.
   * @param {string} url The url that was matched (for debug logging only).
   */
  _reportSerpPage(info, source, url) {
    let payload = `${info.provider}:${info.type}:${info.code || "none"}`;
    Services.telemetry.keyedScalarAdd(
      SEARCH_CONTENT_SCALAR_BASE + source,
      payload,
      1
    );
    lazy.logConsole.debug("Impression:", payload, url);
  }
}

/**
 * ContentHandler deals with handling telemetry of the content within a tab -
 * when ads detected and when they are selected.
 */
class ContentHandler {
  /**
   * Constructor.
   *
   * @param {object} options
   *   The options for the handler.
   * @param {Map} options.browserInfoByURL
   *   The map of urls from TelemetryHandler.
   * @param {Function} options.getProviderInfoForURL
   *   A function that obtains the provider information for a url.
   */
  constructor(options) {
    this._browserInfoByURL = options.browserInfoByURL;
    this._findBrowserItemForURL = options.findBrowserItemForURL;
    this._checkURLForSerpMatch = options.checkURLForSerpMatch;
  }

  /**
   * Initializes the content handler. This will also set up the shared data that is
   * shared with the SearchTelemetryChild actor.
   *
   * @param {Array} providerInfo
   *  The provider information for the search telemetry to record.
   */
  init(providerInfo) {
    Services.ppmm.sharedData.set(
      SEARCH_TELEMETRY_SHARED.PROVIDER_INFO,
      providerInfo
    );
    Services.ppmm.sharedData.set(
      SEARCH_TELEMETRY_SHARED.LOAD_TIMEOUT,
      ADLINK_CHECK_TIMEOUT_MS
    );

    Services.obs.addObserver(this, "http-on-examine-response");
    Services.obs.addObserver(this, "http-on-examine-cached-response");
    Services.obs.addObserver(this, "http-on-stop-request");
  }

  /**
   * Uninitializes the content handler.
   */
  uninit() {
    Services.obs.removeObserver(this, "http-on-examine-response");
    Services.obs.removeObserver(this, "http-on-examine-cached-response");
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
      case "http-on-examine-cached-response":
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
    // The channel we're observing might be a redirect of a channel we've
    // observed before.
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

      let url = wrappedChannel.finalURL;

      let providerInfo = item.info.provider;
      let info = this._searchProviderInfo.find(provider => {
        return provider.telemetryId == providerInfo;
      });

      // If an error occurs with Glean SERP telemetry logic, avoid
      // disrupting legacy telemetry.
      try {
        this.#maybeRecordSERPTelemetry(wrappedChannel, item, info);
      } catch (ex) {
        lazy.logConsole.error(ex);
      }

      if (!info.extraAdServersRegexps?.some(regex => regex.test(url))) {
        return;
      }

      try {
        Services.telemetry.keyedScalarAdd(
          SEARCH_AD_CLICKS_SCALAR_BASE + item.source,
          `${info.telemetryId}:${item.info.type}`,
          1
        );
        wrappedChannel._adClickRecorded = true;
        if (item.newtabSessionId) {
          Glean.newtabSearchAd.click.record({
            newtab_visit_id: item.newtabSessionId,
            search_access_point: item.source,
            is_follow_on: item.info.type.endsWith("follow-on"),
            is_tagged: item.info.type.startsWith("tagged"),
            telemetry_id: item.info.provider,
          });
        }

        lazy.logConsole.debug("Counting ad click in page for:", {
          source: item.source,
          originURL,
          URL: url,
        });
      } catch (e) {
        console.error(e);
      }
    });
  }

  /**
   * Checks if a request should record an ad click if it can be traced to a
   * browser containing an observed SERP.
   *
   * @param {ChannelWrapper} wrappedChannel
   *   The wrapped channel.
   * @param {object} item
   *   The browser item associated with the origin URL of the request.
   * @param {object} info
   *   The search provider info associated with the item.
   */
  #maybeRecordSERPTelemetry(wrappedChannel, item, info) {
    if (!lazy.serpEventsEnabled) {
      return;
    }

    if (wrappedChannel._recordedClick) {
      lazy.logConsole.debug("Click already recorded.");
      return;
    }

    let originURL = wrappedChannel.originURI?.spec;
    let url = wrappedChannel.finalURL;
    // Some channels re-direct by loading pages that return 200. The result
    // is the channel will have an originURL that changes from the SERP to
    // either a nonAdsRegexp or an extraAdServersRegexps. This is typical
    // for loading a page in a new tab. The channel will have changed so any
    // properties attached to them to record state (e.g. _recordedClick)
    // won't be present.
    if (
      info.nonAdsLinkRegexps.some(r => r.test(originURL)) ||
      info.extraAdServersRegexps.some(r => r.test(originURL))
    ) {
      return;
    }

    // A click event is recorded if a user loads a resource from an
    // originURL that is a SERP.
    //
    // Typically, we only want top level loads containing documents to avoid
    // recording any event on an in-page resource a SERP might load
    // (e.g. CSS files).
    //
    // The exception to this is if a subframe loads a resource that matches
    // a non ad link. Some SERPs encode non ad search results with a URL
    // that gets loaded into an iframe, which then tells the container of
    // the iframe to change the location of the page.
    if (
      wrappedChannel.channel.isDocument &&
      (wrappedChannel.channel.loadInfo.isTopLevelLoad ||
        info.nonAdsLinkRegexps.some(r => r.test(url)))
    ) {
      let browser = wrappedChannel.browserElement;

      // If the load is from history, don't record an event.
      if (
        browser?.browsingContext.webProgress?.loadType &
        Ci.nsIDocShell.LOAD_CMD_HISTORY
      ) {
        lazy.logConsole.debug("Ignoring load from history");
        return;
      }

      // Step 1: Check if the browser associated with the request was a
      // tracked SERP.
      let start = Cu.now();
      let telemetryState;
      let isFromNewtab = false;
      if (item.browserTelemetryStateMap.has(browser)) {
        // Current browser is tracked.
        telemetryState = item.browserTelemetryStateMap.get(browser);
      } else if (browser) {
        // Current browser might have been created by a browser in a
        // different tab.
        let tabBrowser = browser.getTabBrowser();
        // A tab will not always have an openerTab, such as if a tab was
        // created as a first tab in a new window. Bug 1866548: additional
        // conditions need to be added in order to track clicks that open in
        // new windows.
        let tab = tabBrowser.getTabForBrowser(browser).openerTab;
        if (tab) {
          telemetryState = item.browserTelemetryStateMap.get(tab.linkedBrowser);
          if (telemetryState) {
            isFromNewtab = true;
          }
        }
      }

      // Step 2: If we have telemetryState, the browser object must be
      // associated with another browser that is tracked. Try to find the
      // component type on the SERP responsible for the request.
      // Exceptions:
      // - If a searchbox was used to initiate the load, don't record another
      //   engagement because the event was logged elsewhere.
      // - If the ad impression hasn't been recorded yet, we have no way of
      //   knowing precisely what kind of component was selected.
      let isSerp = false;
      if (
        telemetryState &&
        telemetryState.adImpressionsReported &&
        !telemetryState.searchBoxSubmitted
      ) {
        if (info.searchPageRegexp?.test(originURL)) {
          isSerp = true;
        }

        let startFindComponent = Cu.now();
        let parsedUrl = new URL(url);
        // Determine the component type of the link.
        let type;
        for (let [
          storedUrl,
          componentType,
        ] of telemetryState.urlToComponentMap.entries()) {
          // The URL we're navigating to may have more query parameters if
          // the provider adds query parameters when the user clicks on a link.
          // On the other hand, the URL we are navigating to may have have
          // fewer query parameters because of query param stripping.
          // Thus, if a query parameter is missing, a match can still be made
          // provided keys that exist in both URLs contain equal values.
          let score = SearchSERPTelemetry.compareUrls(storedUrl, parsedUrl, {
            paramValues: true,
            path: true,
          });
          if (score) {
            type = componentType;
            break;
          }
        }
        ChromeUtils.addProfilerMarker(
          "SearchSERPTelemetry._observeActivity",
          startFindComponent,
          "Find component for URL"
        );

        // Default value for URLs that don't match any components categorized
        // on the page.
        if (!type) {
          type = SearchSERPTelemetryUtils.COMPONENTS.NON_ADS_LINK;
        }

        if (
          type == SearchSERPTelemetryUtils.COMPONENTS.REFINED_SEARCH_BUTTONS
        ) {
          SearchSERPTelemetry.setBrowserContentSource(
            browser,
            SearchSERPTelemetryUtils.INCONTENT_SOURCES.REFINE_ON_SERP
          );
        } else if (isSerp && isFromNewtab) {
          SearchSERPTelemetry.setBrowserContentSource(
            browser,
            SearchSERPTelemetryUtils.INCONTENT_SOURCES.OPENED_IN_NEW_TAB
          );
        }

        // Step 3: Record the engagement.
        impressionIdsWithoutEngagementsSet.delete(telemetryState.impressionId);
        Glean.serp.engagement.record({
          impression_id: telemetryState.impressionId,
          action: SearchSERPTelemetryUtils.ACTIONS.CLICKED,
          target: type,
        });
        lazy.logConsole.debug("Counting click:", {
          impressionId: telemetryState.impressionId,
          type,
          URL: url,
        });
        // Prevent re-directed channels from being examined more than once.
        wrappedChannel._recordedClick = true;
      }
      ChromeUtils.addProfilerMarker(
        "SearchSERPTelemetry._observeActivity",
        start,
        "Maybe record user engagement."
      );
    }
  }

  /**
   * Logs telemetry for a page with adverts, if it is one of the partner search
   * provider pages that we're tracking.
   *
   * @param {object} info
   *     The search provider information for the page.
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

    let telemetryState = item.browserTelemetryStateMap.get(browser);
    if (telemetryState.adsReported) {
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
      SEARCH_WITH_ADS_SCALAR_BASE + item.source,
      `${item.info.provider}:${item.info.type}`,
      1
    );

    telemetryState.adsReported = true;

    if (item.newtabSessionId) {
      Glean.newtabSearchAd.impression.record({
        newtab_visit_id: item.newtabSessionId,
        search_access_point: item.source,
        is_follow_on: item.info.type.endsWith("follow-on"),
        is_tagged: item.info.type.startsWith("tagged"),
        telemetry_id: item.info.provider,
      });
    }
  }

  /**
   * Logs ad impression telemetry for a page with adverts, if it is
   * one of the partner search provider pages that we're tracking.
   *
   * @param {object} info
   *     The search provider information for the page.
   * @param {string} info.url
   *     The url of the page.
   * @param {Map<string, object>} info.adImpressions
   *     A map of ad impressions found for the page, where the key
   *     is the type of ad component and the value is an object
   *     containing the number of ads that were loaded, visible,
   *     and hidden.
   * @param {Map<string, string>} info.hrefToComponentMap
   *     A map of hrefs to their component type. Contains both ads
   *     and non-ads.
   * @param {object} browser
   *     The browser associated with the page.
   */
  _reportPageWithAdImpressions(info, browser) {
    let item = this._findBrowserItemForURL(info.url);
    if (!item) {
      return;
    }
    let telemetryState = item.browserTelemetryStateMap.get(browser);
    if (
      lazy.serpEventsEnabled &&
      info.adImpressions &&
      telemetryState &&
      !telemetryState.adImpressionsReported
    ) {
      for (let [componentType, data] of info.adImpressions.entries()) {
        lazy.logConsole.debug("Counting ad:", { type: componentType, ...data });
        Glean.serp.adImpression.record({
          impression_id: telemetryState.impressionId,
          component: componentType,
          ads_loaded: data.adsLoaded,
          ads_visible: data.adsVisible,
          ads_hidden: data.adsHidden,
        });
      }
      // Convert hrefToComponentMap to a urlToComponentMap in order to cache
      // the query parameters of the href.
      let urlToComponentMap = new Map();
      for (let [href, adType] of info.hrefToComponentMap) {
        urlToComponentMap.set(new URL(href), adType);
      }
      telemetryState.urlToComponentMap = urlToComponentMap;
      telemetryState.adImpressionsReported = true;
      Services.obs.notifyObservers(null, "reported-page-with-ad-impressions");
    }
  }

  /**
   * Records a page action from a SERP page. Normally, actions are tracked in
   * parent process by observing network events but some actions are not
   * possible to detect outside of subscribing to the child process.
   *
   * @param {object} info
   *   The search provider infomation for the page.
   * @param {string} info.type
   *   The component type that was clicked on.
   * @param {string} info.action
   *   The action taken on the page.
   * @param {object} browser
   *   The browser associated with the page.
   */
  _reportPageAction(info, browser) {
    let item = this._findBrowserItemForURL(info.url);
    if (!item) {
      return;
    }
    let telemetryState = item.browserTelemetryStateMap.get(browser);
    let impressionId = telemetryState?.impressionId;
    if (info.type && impressionId) {
      lazy.logConsole.debug(`Recorded page action:`, {
        impressionId: telemetryState.impressionId,
        type: info.type,
        action: info.action,
      });
      Glean.serp.engagement.record({
        impression_id: impressionId,
        action: info.action,
        target: info.type,
      });
      impressionIdsWithoutEngagementsSet.delete(impressionId);
      // In-content searches are not be categorized with a type, so they will
      // not be picked up in the network processes.
      if (
        info.type == SearchSERPTelemetryUtils.COMPONENTS.INCONTENT_SEARCHBOX &&
        info.action == SearchSERPTelemetryUtils.ACTIONS.SUBMITTED
      ) {
        telemetryState.searchBoxSubmitted = true;
        SearchSERPTelemetry.setBrowserContentSource(
          browser,
          SearchSERPTelemetryUtils.INCONTENT_SOURCES.SEARCHBOX
        );
      }
    } else {
      lazy.logConsole.warn(
        "Expected to report a",
        info.action,
        "engagement for",
        info.url,
        "but couldn't find an impression id."
      );
    }
  }

  _reportPageImpression(info, browser) {
    let item = this._findBrowserItemForURL(info.url);
    let telemetryState = item.browserTelemetryStateMap.get(browser);
    if (!telemetryState?.impressionInfo) {
      lazy.logConsole.debug(
        "Could not find telemetry state or impression info."
      );
      return;
    }
    let impressionId = telemetryState.impressionId;
    if (impressionId) {
      let impressionInfo = telemetryState.impressionInfo;
      Glean.serp.impression.record({
        impression_id: impressionId,
        provider: impressionInfo.provider,
        tagged: impressionInfo.tagged,
        partner_code: impressionInfo.partnerCode,
        source: impressionInfo.source,
        shopping_tab_displayed: info.shoppingTabDisplayed,
        is_shopping_page: impressionInfo.isShoppingPage,
        is_private: impressionInfo.isPrivate,
      });
      lazy.logConsole.debug(`Reported Impression:`, {
        impressionId,
        ...impressionInfo,
        shoppingTabDisplayed: info.shoppingTabDisplayed,
      });
      Services.obs.notifyObservers(null, "reported-page-with-impression");
    } else {
      lazy.logConsole.debug("Could not find an impression id.");
    }
  }

  /**
   * Initiates the categorization and reporting of domains extracted from
   * SERPs.
   *
   * @param {object} info
   *   The search provider infomation for the page.
   * @param {Set} info.nonAdDomains
       The non-ad domains extracted from the page. 
   * @param {Set} info.adDomains
       The ad domains extracted from the page. 
   * @param {object} browser
   *   The browser associated with the page.
   */
  _reportPageDomains(info, browser) {
    let item = this._findBrowserItemForURL(info.url);
    let telemetryState = item.browserTelemetryStateMap.get(browser);
    if (lazy.serpEventTelemetryCategorization && telemetryState) {
      let provider = item?.info.provider;
      if (provider) {
        SearchSERPCategorization.maybeCategorizeAndReportDomainsFromProvider(
          info.nonAdDomains,
          info.adDomains,
          provider
        );
        Services.obs.notifyObservers(
          null,
          "reported-page-with-categorized-domains"
        );
      }
    }
  }
}

/**
 * Categorizes SERPs.
 */
class DomainCategorizer {
  /**
   * Categorizes and reports domains extracted from SERPs. Note that we don't
   * process domains if the domain-to-categories map is empty (if the client
   * couldn't download Remote Settings attachments, for example).
   *
   * @param {Set} nonAdDomains
   *   The non-ad domains extracted from the page.
   * @param {Set} adDomains
   *   The ad domains extracted from the page.
   * @param {string} provider
   *   The provider associated with the page.
   */
  maybeCategorizeAndReportDomainsFromProvider(
    nonAdDomains,
    adDomains,
    provider
  ) {
    for (let domains of [nonAdDomains, adDomains]) {
      // We don't want to generate and report telemetry if a client was unable
      // to download the domain-to-categories mapping from Remote Settings.
      if (SearchSERPDomainToCategoriesMap.empty) {
        continue;
      }
      domains = this.processDomains(domains, provider);
      // Per a request from Data Science, we need to limit the number of domains
      // categorized to 10 non ad domains and 10 ad domains.
      domains = new Set([...domains].slice(0, maxDomainsToCategorize));
      let resultsToReport = this.applyCategorizationLogic(domains);
      this.dummyLogger(
        domains,
        resultsToReport,
        SearchSERPDomainToCategoriesMap.version
      );
    }
  }

  // TODO: check with DS to get the final aggregation logic. (Bug 1854196)
  /**
   * Applies the logic for reducing extracted domains to a single category for
   * the SERP.
   *
   * @param {Set} domains
   *   The domains extracted from the page.
   * @returns {object} resultsToReport
   *   The final categorization results. Keys are: "category", "num_domains",
   *   "num_unknown" and "num_inconclusive".
   */
  applyCategorizationLogic(domains) {
    let totalScoresPerCategory = {};
    let domainsCount = 0;
    let unknownsCount = 0;
    let inconclusivesCount = 0;

    for (let domain of domains) {
      domainsCount++;

      let categoryCandidates = SearchSERPDomainToCategoriesMap.get(domain);
      if (!categoryCandidates.length) {
        unknownsCount++;
        continue;
      }

      for (let candidate of categoryCandidates) {
        if (
          candidate.category ==
          SearchSERPTelemetryUtils.CATEGORIZATION.INCONCLUSIVE
        ) {
          inconclusivesCount++;
          continue;
        }

        if (totalScoresPerCategory[candidate.category]) {
          totalScoresPerCategory[candidate.category] += candidate.score;
        } else {
          totalScoresPerCategory[candidate.category] = candidate.score;
        }
      }
    }

    let finalCategory;
    // Determine if all domains were unknown or inconclusive.
    if (unknownsCount + inconclusivesCount == domainsCount) {
      finalCategory = "inconclusive";
    } else {
      let maxScore = Math.max(...Object.values(totalScoresPerCategory));
      // Handles ties by randomly returning one of the categories with the
      // maximum score.
      let topCategories = [];
      for (let category in totalScoresPerCategory) {
        if (totalScoresPerCategory[category] == maxScore) {
          topCategories.push(category);
        }
      }
      finalCategory =
        topCategories.length > 1
          ? this.#chooseRandomlyFrom(topCategories)
          : topCategories[0];
    }

    return {
      category: finalCategory,
      num_domains: domainsCount,
      num_unknown: unknownsCount,
      num_inconclusive: inconclusivesCount,
    };
  }

  // TODO: replace this method once we know where to send the categorized
  // domains and overall SERP category. (Bug 1854692)
  dummyLogger(domains, resultsToReport, version) {
    lazy.logConsole.debug("Version of the attachments:", version);
    lazy.logConsole.debug("Domains extracted from SERP:", [...domains]);
    lazy.logConsole.debug(
      "Categorization results to report to Glean:",
      resultsToReport
    );
  }

  /**
   * Processes raw domains extracted from the SERP into their final form before
   * categorization.
   *
   * @param {Set} domains
   *   The domains extracted from the page.
   * @param {string} provider
   *   The provider associated with the page.
   * @returns {Set} processedDomains
   *   The final set of processed domains for a page.
   */
  processDomains(domains, provider) {
    let processedDomains = new Set();

    for (let domain of domains) {
      // Don't include domains associated with the search provider.
      if (
        domain.startsWith(`${provider}.`) ||
        domain.includes(`.${provider}.`)
      ) {
        continue;
      }
      let domainWithoutSubdomains = this.#stripDomainOfSubdomains(domain);
      // We may have come across the same domain twice, once with www. prefixed
      // and another time without.
      if (
        domainWithoutSubdomains &&
        !processedDomains.has(domainWithoutSubdomains)
      ) {
        processedDomains.add(domainWithoutSubdomains);
      }
    }

    return processedDomains;
  }

  /**
   * Helper to strip domains of any subdomains.
   *
   * @param {string} domain
   *   The domain to strip of any subdomains.
   * @returns {object} browser
   *   The given domain with any subdomains removed.
   */
  #stripDomainOfSubdomains(domain) {
    let tld;
    // Can throw an exception if the input has too few domain levels.
    try {
      tld = Services.eTLD.getKnownPublicSuffixFromHost(domain);
    } catch (ex) {
      return "";
    }

    let domainWithoutTLD = domain.substring(0, domain.length - tld.length);
    let secondLevelDomain = domainWithoutTLD.split(".").at(-2);

    return secondLevelDomain ? `${secondLevelDomain}.${tld}` : "";
  }

  #chooseRandomlyFrom(categories) {
    let randIdx = Math.floor(Math.random() * categories.length);
    return categories[randIdx];
  }
}

/**
 * @typedef {object} DomainToCategoriesRecord
 * @property {number} version
 *  The version of the record.
 */

/**
 * @typedef {object} DomainCategoryScore
 * @property {number} category
 *  The index of the category.
 * @property {number} score
 *  The score associated with the category.
 */

/**
 * Maps domain to categories, with data synced with Remote Settings.
 */
class DomainToCategoriesMap {
  /**
   * Contains the domain to category scores.
   *
   * @type {Object<string, Array<DomainCategoryScore>> | null}
   */
  #map = null;

  /**
   * Latest version number of the attachments.
   *
   * @type {number | null}
   */
  #version = null;

  /**
   * The Remote Settings client.
   *
   * @type {object | null}
   */
  #client = null;

  /**
   * Whether this is synced with Remote Settings.
   *
   * @type {boolean}
   */
  #init = false;

  /**
   * Callback when Remote Settings syncs.
   *
   * @type {Function | null}
   */
  #onSettingsSync = null;

  /**
   * When downloading an attachment from Remote Settings fails, this will
   * contain a timer which will eventually attempt to retry downloading
   * attachments.
   */
  #downloadTimer = null;

  /**
   * Number of times this has attempted to try another download. Will reset
   * if the categorization preference has been toggled, or a sync event has
   * been detected.
   *
   * @type {number}
   */
  #downloadRetries = 0;

  /**
   * Runs at application startup with startup idle tasks. Creates a listener
   * to changes of the SERP categorization preference. Additionally, if the
   * SERP categorization preference is enabled, it creates a Remote Settings
   * client to listen to updates, and populates the map.
   */
  async init() {
    if (this.#init) {
      return;
    }

    Services.prefs.addObserver(CATEGORIZATION_PREF, this);
    this.#init = true;

    if (lazy.serpEventTelemetryCategorization) {
      this.#setupClientAndMap();
    }
  }

  /**
   * Predominantly a test-only function.
   */
  uninit() {
    lazy.logConsole.debug("Un-initialize domain-to-categories map.");
    if (this.#init) {
      if (this.#map) {
        this.#clearClientAndMap();
      } else {
        this.#cancelAndNullifyTimer();
      }
      this.#init = false;
      Services.prefs.removeObserver(CATEGORIZATION_PREF, this);
    }
  }

  observe(subject, topic, data) {
    if (topic != "nsPref:changed") {
      return;
    }
    if (data == CATEGORIZATION_PREF) {
      if (lazy.serpEventTelemetryCategorization) {
        this.#setupClientAndMap();
      } else {
        this.#cancelAndNullifyTimer();
        this.#clearClientAndMap();
      }
    }
  }

  /**
   * Given a domain, find categories and relevant scores.
   *
   * @param {string} domain Domain to lookup.
   * @returns {Array<DomainCategoryScore>}
   *  An array containing categories and their respective score. If no record
   *  for the domain is available, return an empty array.
   */
  get(domain) {
    if (this.empty) {
      return [];
    }
    lazy.gCryptoHash.init(lazy.gCryptoHash.MD5);
    let bytes = new TextEncoder().encode(domain);
    lazy.gCryptoHash.update(bytes, domain.length);
    let hash = lazy.gCryptoHash.finish(true);
    let rawValues = this.#map[hash] ?? [];
    if (rawValues.length) {
      let output = [];
      // Transform data into a more readable format.
      // [x, y] => { category: x, score: y }
      for (let i = 0; i < rawValues.length; i += 2) {
        output.push({ category: rawValues[i], score: rawValues[i + 1] });
      }
      return output;
    }
    return [];
  }

  /**
   * If the map was initialized, returns the version number for the data.
   * The version number is determined by the record with the highest version
   * number. Even if the records have different versions, only records from the
   * latest version should be available. Returns null if the map was not
   * initialized.
   *
   * @returns {null | number} The version number.
   */
  get version() {
    return this.#version;
  }

  /**
   * Whether the map is empty of data.
   *
   * @returns {boolean}
   */
  get empty() {
    return !this.#map;
  }

  /**
   * Unit test-only function, used to override the domainToCategoriesMap so
   * that tests can set it to easy to test values.
   *
   * @param {object} domainToCategoriesMap
   *   An object where the key is a hashed domain and the value is an array
   *   containing an arbitrary number of DomainCategoryScores.
   */
  overrideMapForTests(domainToCategoriesMap) {
    this.#map = domainToCategoriesMap;
  }

  async #setupClientAndMap() {
    if (this.#client && !this.empty) {
      return;
    }
    lazy.logConsole.debug("Initializing domain-to-categories map.");
    this.#client = lazy.RemoteSettings(TELEMETRY_CATEGORIZATION_KEY);

    this.#onSettingsSync = event => this.#sync(event.data);
    this.#client.on("sync", this.#onSettingsSync);

    let records = await this.#client.get();
    await this.#clearAndPopulateMap(records);
  }

  #clearClientAndMap() {
    if (this.#client) {
      lazy.logConsole.debug("Removing Remote Settings client.");
      this.#client.off("sync", this.#onSettingsSync);
      this.#client = null;
      this.#onSettingsSync = null;
      this.#downloadRetries = 0;
    }

    if (this.#map) {
      lazy.logConsole.debug("Clearing domain-to-categories map.");
      this.#map = null;
      this.#version = null;
    }
  }

  /**
   * Inspects a list of records from the categorization domain bucket and finds
   * the maximum version score from the set of records. Each record should have
   * the same version number but if for any reason one entry has a lower
   * version number, the latest version can be used to filter it out.
   *
   * @param {Array<DomainToCategoriesRecord>} records
   *   An array containing the records from a Remote Settings collection.
   * @returns {number}
   */
  #retrieveLatestVersion(records) {
    return records.reduce((version, record) => {
      if (record.version > version) {
        return record.version;
      }
      return version;
    }, 0);
  }

  /**
   * Callback when Remote Settings has indicated the collection has been
   * synced. Since the records in the collection will be updated all at once,
   * use the array of current records which at this point in time would have
   * the latest records from Remote Settings. Additionally, delete any
   * attachment for records that no longer exist.
   *
   * @param {object} data
   *  Object containing records that are current, deleted, created, or updated.
   *
   */
  async #sync(data) {
    lazy.logConsole.debug("Syncing domain-to-categories with Remote Settings.");

    // Remove local files of deleted records.
    let toDelete = data?.deleted.filter(d => d.attachment);
    await Promise.all(
      toDelete.map(record => this.#client.attachments.deleteDownloaded(record))
    );

    // In case a user encountered network failures in the past and kept their
    // session on, this will ensure the next sync event will retry downloading
    // again in case there's a new download error.
    this.#downloadRetries = 0;

    this.#clearAndPopulateMap(data?.current);
  }

  /**
   * Clear the existing map and populate it with attachments found in the
   * records. If no attachments are found, or no record containing an
   * attachment contained the latest version, then nothing will change.
   *
   * @param {Array<DomainToCategoriesRecord>} records
   *  The records containing attachments.
   *
   */
  async #clearAndPopulateMap(records) {
    // Set map to null so that if there are errors in the downloads, consumers
    // will be able to know whether the map has information. Once we've
    // successfully downloaded attachments and are parsing them, a non-null
    // object will be created.
    this.#map = null;
    this.#version = null;
    this.#cancelAndNullifyTimer();

    if (!records?.length) {
      lazy.logConsole.debug("No records found for domain-to-categories map.");
      return;
    }

    let fileContents = [];
    for (let record of records) {
      let result;
      // Downloading attachments can fail.
      try {
        result = await this.#client.attachments.download(record);
      } catch (ex) {
        lazy.logConsole.error("Could not download file:", ex);
        this.#createTimerToPopulateMap();
        return;
      }
      fileContents.push(result.buffer);
    }

    // All attachments should have the same version number. If for whatever
    // reason they don't, we should only use the attachments with the latest
    // version.
    this.#version = this.#retrieveLatestVersion(records);

    if (!this.#version) {
      lazy.logConsole.debug("Could not find a version number for any record.");
      return;
    }

    // Queue the series of assignments.
    for (let i = 0; i < fileContents.length; ++i) {
      let buffer = fileContents[i];
      Services.tm.idleDispatchToMainThread(() => {
        let start = Cu.now();
        let json;
        try {
          json = JSON.parse(new TextDecoder().decode(buffer));
        } catch (ex) {
          // TODO: If there was an error decoding the buffer, we may want to
          // dispatch an error in telemetry or try again.
          return;
        }
        ChromeUtils.addProfilerMarker(
          "SearchSERPTelemetry.#clearAndPopulateMap",
          start,
          "Convert buffer to JSON."
        );
        if (!this.#map) {
          this.#map = {};
        }
        Object.assign(this.#map, json);
        lazy.logConsole.debug("Updated domain-to-categories map.");
        if (i == fileContents.length - 1) {
          Services.obs.notifyObservers(
            null,
            "domain-to-categories-map-update-complete"
          );
        }
      });
    }
  }

  #cancelAndNullifyTimer() {
    if (this.#downloadTimer) {
      lazy.logConsole.debug("Cancel and nullify download timer.");
      this.#downloadTimer.cancel();
      this.#downloadTimer = null;
    }
  }

  #createTimerToPopulateMap() {
    if (
      this.#downloadRetries >=
      TELEMETRY_CATEGORIZATION_DOWNLOAD_SETTINGS.maxTriesPerSession
    ) {
      return;
    }
    if (!this.#downloadTimer) {
      this.#downloadTimer = Cc["@mozilla.org/timer;1"].createInstance(
        Ci.nsITimer
      );
    }
    lazy.logConsole.debug("Create timer to retry downloading attachments.");
    let delay =
      TELEMETRY_CATEGORIZATION_DOWNLOAD_SETTINGS.base +
      randomInteger(
        TELEMETRY_CATEGORIZATION_DOWNLOAD_SETTINGS.minAdjust,
        TELEMETRY_CATEGORIZATION_DOWNLOAD_SETTINGS.maxAdjust
      );
    this.#downloadTimer.initWithCallback(
      async () => {
        this.#downloadRetries += 1;
        let records = await this.#client.get();
        this.#clearAndPopulateMap(records);
      },
      delay,
      Ci.nsITimer.TYPE_ONE_SHOT
    );
  }
}

function randomInteger(min, max) {
  return Math.floor(Math.random() * (max - min + 1)) + min;
}

export var SearchSERPDomainToCategoriesMap = new DomainToCategoriesMap();
export var SearchSERPTelemetry = new TelemetryHandler();
export var SearchSERPCategorization = new DomainCategorizer();
