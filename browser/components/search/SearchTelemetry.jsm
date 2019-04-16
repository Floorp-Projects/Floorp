/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

var EXPORTED_SYMBOLS = ["SearchTelemetry"];

const {XPCOMUtils} = ChromeUtils.import("resource://gre/modules/XPCOMUtils.jsm");

XPCOMUtils.defineLazyModuleGetters(this, {
  Services: "resource://gre/modules/Services.jsm",
});

// The various histograms and scalars that we report to.
const SEARCH_COUNTS_HISTOGRAM_KEY = "SEARCH_COUNTS";
const SEARCH_WITH_ADS_SCALAR = "browser.search.with_ads";
const SEARCH_AD_CLICKS_SCALAR = "browser.search.ad_clicks";

/**
 * Used to identify various parameters used with partner search providers. This
 * consists of the following structure:
 * - {<string>} name
 *     Details for a particular provider with the string name.
 * - {regexp} <string>.regexp
 *     The regular expression used to match the url for the search providers main page.
 * - {string} <string>.queryParam
 *     The query parameter name that indicates a search has been made.
 * - {string} [<string>.codeParam]
 *     The query parameter name that indicates a search provider's code.
 * - {array} [<string>.codePrefixes]
 *     An array of the possible string prefixes for a codeParam, indicating a
 *     partner search.
 * - {array} [<string>.followOnParams]
 *     An array of parameters name that indicates this is a follow-on search.
 * - {array} [<string>.extraAdServersRegexps]
 *     An array of regular expressions used to determine if a link on a search
 *     page mightbe an advert.
 */
const SEARCH_PROVIDER_INFO = {
  "google": {
    "regexp": /^https:\/\/www\.google\.(?:.+)\/search/,
    "queryParam": "q",
    "codeParam": "client",
    "codePrefixes": ["firefox"],
    "followonParams": ["oq", "ved", "ei"],
    "extraAdServersRegexps": [/^https:\/\/www\.googleadservices\.com\/(?:pagead\/)?aclk/],
  },
  "duckduckgo": {
    "regexp": /^https:\/\/duckduckgo\.com\//,
    "queryParam": "q",
    "codeParam": "t",
    "codePrefixes": ["ff"],
  },
  "yahoo": {
    "regexp": /^https:\/\/(?:.*)search\.yahoo\.com\/search/,
    "queryParam": "p",
  },
  "baidu": {
    "regexp": /^https:\/\/www\.baidu\.com\/(?:s|baidu)/,
    "queryParam": "wd",
    "codeParam": "tn",
    "codePrefixes": ["34046034_", "monline_"],
    "followonParams": ["oq"],
  },
  "bing": {
    "regexp": /^https:\/\/www\.bing\.com\/search/,
    "queryParam": "q",
    "codeParam": "pc",
    "codePrefixes": ["MOZ", "MZ"],
  },
};

const BROWSER_SEARCH_PREF = "browser.search.";

XPCOMUtils.defineLazyPreferenceGetter(this, "loggingEnabled", BROWSER_SEARCH_PREF + "log", false);

/**
 * TelemetryHandler is the main class handling search telemetry. It primarily
 * deals with tracking of what pages are loaded into tabs.
 *
 * It handles the *in-content:sap* keys of the SEARCH_COUNTS histogram.
 */
class TelemetryHandler {
  constructor() {
    this._browserInfoByUrl = new Map();
    this._initialized = false;
    this.__searchProviderInfo = null;
    this._contentHandler = new ContentHandler({
      browserInfoByUrl: this._browserInfoByUrl,
      getProviderInfoForUrl: this._getProviderInfoForUrl.bind(this),
    });
  }

  /**
   * Initializes the TelemetryHandler and its ContentHandler. It will add
   * appropriate listeners to the window so that window opening and closing
   * can be tracked.
   */
  init() {
    if (this._initialized) {
      return;
    }

    this._contentHandler.init();

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
   * @param {object} infoByProvider @see SEARCH_PROVIDER_INFO for type information.
   */
  overrideSearchTelemetryForTests(infoByProvider) {
    if (infoByProvider) {
      for (let info of Object.values(infoByProvider)) {
        info.regexp = new RegExp(info.regexp);
      }
      this.__searchProviderInfo = infoByProvider;
    } else {
      this.__searchProviderInfo = SEARCH_PROVIDER_INFO;
    }
    this._contentHandler.overrideSearchTelemetryForTests(this.__searchProviderInfo);
  }

  /**
   * This may start tracking a tab based on the URL. If the URL matches a search
   * partner, and it has a code, then we'll start tracking it. This will aid
   * determining if it is a page we should be tracking for adverts.
   *
   * @param {object} browser The browser associated with the page.
   * @param {string} url The url that was loaded in the browser.
   */
  updateTrackingStatus(browser, url) {
    let info = this._checkURLForSerpMatch(url);
    if (!info) {
      this.stopTrackingBrowser(browser);
      return;
    }

    this._reportSerpPage(info, url);

    // If we have a code, then we also track this for potential ad clicks.
    if (info.code) {
      let item = this._browserInfoByUrl.get(url);
      if (item) {
        item.browsers.add(browser);
      } else {
        this._browserInfoByUrl.set(url, {
          browser: new WeakSet([browser]),
          info,
        });
      }
    }
  }

  /**
   * Stops tracking of a tab, for example the tab has loaded a different URL.
   *
   * @param {object} browser The browser associated with the tab to stop being
   *                         tracked.
   */
  stopTrackingBrowser(browser) {
    for (let [url, item] of this._browserInfoByUrl) {
      item.browser.delete(browser);

      if (!item.browser.length) {
        this._browserInfoByUrl.delete(url);
      }
    }
  }

  // nsIWindowMediatorListener

  /**
   * This is called when a new window is opened, and handles registration of
   * that window if it is a browser window.
   *
   * @param {nsIXULWindow} xulWin The xul window that was opened.
   */
  onOpenWindow(xulWin) {
    let win = xulWin.docShell.domWindow;
    win.addEventListener("load", () => {
      if (win.document.documentElement.getAttribute("windowtype") != "navigator:browser") {
        return;
      }

      this._registerWindow(win);
    }, {once: true});
  }

  /**
   * Listener that is called when a window is closed, and handles deregistration of
   * that window if it is a browser window.
   *
   * @param {nsIXULWindow} xulWin The xul window that was closed.
   */
  onCloseWindow(xulWin) {
    let win = xulWin.docShell.domWindow;

    if (win.document.documentElement.getAttribute("windowtype") != "navigator:browser") {
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
    this._contentHandler.registerWindow(win);
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

    this._contentHandler.unregisterWindow(win);
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
  _getProviderInfoForUrl(url, useOnlyExtraAdServers = false) {
    if (useOnlyExtraAdServers) {
      return Object.entries(this._searchProviderInfo).find(
        ([_, info]) => {
          if (info.extraAdServersRegexps) {
            for (let regexp of info.extraAdServersRegexps) {
              if (regexp.test(url)) {
                return true;
              }
            }
          }
          return false;
        }
      );
    }

    return Object.entries(this._searchProviderInfo).find(
      ([_, info]) => info.regexp.test(url)
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
    let info = this._getProviderInfoForUrl(url);
    if (!info) {
      return null;
    }
    let [provider, searchProviderInfo] = info;
    let queries = new URLSearchParams(url.split("#")[0].split("?")[1]);
    if (!queries.get(searchProviderInfo.queryParam)) {
      return null;
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
    return {provider, type, code};
  }

  /**
   * Logs telemetry for a search provider visit.
   *
   * @param {object} info
   * @param {string} info.provider The name of the provider.
   * @param {string} info.type The type of search.
   * @param {string} [info.code] The code for the provider.
   * @param {string} url The url that was matched (for debug logging only).
   */
  _reportSerpPage(info, url) {
    let payload = `${info.provider}.in-content:${info.type}:${info.code || "none"}`;
    let histogram = Services.telemetry.getKeyedHistogramById(SEARCH_COUNTS_HISTOGRAM_KEY);
    histogram.add(payload);
    LOG(`SearchTelemetry: ${payload} for ${url}`);
  }

  /**
   * Returns the current search provider information in use.
   * @see SEARCH_PROVIDER_INFO
   */
  get _searchProviderInfo() {
    if (!this.__searchProviderInfo) {
      this.__searchProviderInfo = SEARCH_PROVIDER_INFO;
    }
    return this.__searchProviderInfo;
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
   * @param {Map} options.browserInfoByUrl The  map of urls from TelemetryHandler.
   * @param {function} options.getProviderInfoForUrl A function that obtains
   *   the provider information for a url.
   */
  constructor(options) {
    this._browserInfoByUrl = options.browserInfoByUrl;
    this._getProviderInfoForUrl = options.getProviderInfoForUrl;
  }

  /**
   * Initializes the content handler. This will also set up the shared data that is
   * shared with the SearchTelemetryChild actor.
   */
  init() {
    Services.ppmm.sharedData.set("SearchTelemetry:ProviderInfo", SEARCH_PROVIDER_INFO);

    Cc["@mozilla.org/network/http-activity-distributor;1"]
      .getService(Ci.nsIHttpActivityDistributor)
      .addObserver(this);
  }

  /**
   * Uninitializes the content handler.
   */
  uninit() {
    Cc["@mozilla.org/network/http-activity-distributor;1"]
      .getService(Ci.nsIHttpActivityDistributor)
      .removeObserver(this);
  }

  /**
   * Receives a message from the SearchTelemetryChild actor.
   *
   * @param {object} msg
   */
  receiveMessage(msg) {
    if (msg.name != "SearchTelemetry:PageInfo") {
      LOG(`"Received unexpected message: ${msg.name}`);
      return;
    }

    this._reportPageWithAds(msg.data);
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
   * Listener that observes network activity, so that we can determine if a link
   * from a search provider page was followed, and if then if that link was an
   * ad click or not.
   *
   * @param {nsISupports} httpChannel The channel that generated the activity.
   * @param {number} activityType The type of activity.
   * @param {number} activitySubtype The subtype for the activity.
   * @param {PRTime} timestamp The time of the activity.
   * @param {number} [extraSizeData] Any size data available for the activity.
   * @param {string} [extraStringData] Any extra string data available for the
   *   activity.
   */
  observeActivity(httpChannel, activityType, activitySubtype, timestamp, extraSizeData, extraStringData) {
    if (!this._browserInfoByUrl.size ||
        activityType != Ci.nsIHttpActivityObserver.ACTIVITY_TYPE_HTTP_TRANSACTION ||
        activitySubtype != Ci.nsIHttpActivityObserver.ACTIVITY_SUBTYPE_TRANSACTION_CLOSE) {
      return;
    }

    let channel = httpChannel.QueryInterface(Ci.nsIHttpChannel);
    let loadInfo;
    try {
      loadInfo = channel.loadInfo;
    } catch (e) {
      // Channels without a loadInfo are not pertinent.
      return;
    }

    try {
      let uri = channel.URI;
      let triggerURI = loadInfo.triggeringPrincipal.URI;

      if (!triggerURI || !this._browserInfoByUrl.has(triggerURI.spec)) {
        return;
      }

      let info = this._getProviderInfoForUrl(uri.spec, true);
      if (!info) {
        return;
      }

      Services.telemetry.keyedScalarAdd(SEARCH_AD_CLICKS_SCALAR, info[0], 1);
      LOG(`SearchTelemetry: Counting ad click in page for ${info[0]} ${triggerURI.spec}`);
    } catch (e) {
      Cu.reportError(e);
    }
  }

  /**
   * Adds a message listener for the window being registered to receive messages
   * from SearchTelemetryChild.
   *
   * @param {object} win The window to register.
   */
  registerWindow(win) {
    win.messageManager.addMessageListener("SearchTelemetry:PageInfo", this);
  }

  /**
   * Removes the message listener for the window.
   *
   * @param {object} win The window to unregister.
   */
  unregisterWindow(win) {
    win.messageManager.removeMessageListener("SearchTelemetry:PageInfo", this);
  }

  /**
   * Logs telemetry for a page with adverts, if it is one of the partner search
   * provider pages that we're tracking.
   *
   * @param {object} info
   * @param {boolean} info.hasAds Whether or not the page has adverts.
   * @param {string} info.url The url of the page.
   */
  _reportPageWithAds(info) {
    let item = this._browserInfoByUrl.get(info.url);
    if (!item) {
      LOG(`Expected to report URI with ads but couldn't find the information`);
      return;
    }

    Services.telemetry.keyedScalarAdd(SEARCH_WITH_ADS_SCALAR, item.info.provider, 1);
    LOG(`SearchTelemetry: Counting ads in page for ${item.info.provider} ${info.url}`);
  }
}

/**
 * Outputs the message to the JavaScript console as well as to stdout.
 *
 * @param {string} msg The message to output.
 */
function LOG(msg) {
  if (loggingEnabled) {
    dump(`*** SearchTelemetry: ${msg}\n"`);
    Services.console.logStringMessage(msg);
  }
}

var SearchTelemetry = new TelemetryHandler();
