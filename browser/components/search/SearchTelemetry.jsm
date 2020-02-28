/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

var EXPORTED_SYMBOLS = ["SearchTelemetry"];

const { XPCOMUtils } = ChromeUtils.import(
  "resource://gre/modules/XPCOMUtils.jsm"
);

XPCOMUtils.defineLazyModuleGetters(this, {
  Services: "resource://gre/modules/Services.jsm",
});

// The various histograms and scalars that we report to.
const SEARCH_COUNTS_HISTOGRAM_KEY = "SEARCH_COUNTS";
const SEARCH_WITH_ADS_SCALAR = "browser.search.with_ads";
const SEARCH_AD_CLICKS_SCALAR = "browser.search.ad_clicks";
const SEARCH_DATA_TRANSFERRED_SCALAR = "browser.search.data_transferred";
const SEARCH_TELEMETRY_PRIVATE_BROWSING_KEY_SUFFIX = "pb";

/**
 * Used to identify various parameters used with partner search providers. This
 * consists of the following structure:
 * - {<string>} name
 *     Details for a particular provider with the string name.
 * - {regexp} <string>.regexp
 *     The regular expression used to match the url for the search providers
 *     main page.
 * - {string} <string>.queryParam
 *     The query parameter name that indicates a search has been made.
 * - {string} [<string>.codeParam]
 *     The query parameter name that indicates a search provider's code.
 * - {array} [<string>.codePrefixes]
 *     An array of the possible string prefixes for a codeParam, indicating a
 *     partner search.
 * - {array} [<string>.followonParams]
 *     An array of parameters name that indicates this is a follow-on search.
 * - {array} [<string>.extraAdServersRegexps]
 *     An array of regular expressions used to determine if a link on a search
 *     page might be an advert.
 * - {array} [<object>.followonCookies]
 *     An array of cookie details, which should look like:
 *     - {string} [extraCodeParam]
 *         The query parameter name that indicates an extra search provider's
 *         code.
 *     - {array} [<string>.extraCodePrefixes]
 *         An array of the possible string prefixes for a codeParam, indicating
 *         a partner search.
 *     - {string} host
 *         Host name to which the cookie is linked to.
 *     - {string} name
 *         Name of the cookie to look for that should contain the search
 *         provider's code.
 *     - {string} codeParam
 *         The cookie parameter name that indicates a search provider's code.
 *     - {array} <string>.codePrefixes
 *         An array of the possible string prefixes for a codeParam, indicating
 *         a partner search.
 */
const SEARCH_PROVIDER_INFO = {
  google: {
    regexp: /^https:\/\/www\.google\.(?:.+)\/search/,
    queryParam: "q",
    codeParam: "client",
    codePrefixes: ["firefox"],
    followonParams: ["oq", "ved", "ei"],
    extraAdServersRegexps: [
      /^https:\/\/www\.google(?:adservices)?\.com\/(?:pagead\/)?aclk/,
    ],
  },
  duckduckgo: {
    regexp: /^https:\/\/duckduckgo\.com\//,
    queryParam: "q",
    codeParam: "t",
    codePrefixes: ["ff"],
    extraAdServersRegexps: [
      /^https:\/\/duckduckgo.com\/y\.js/,
      /^https:\/\/www\.amazon\.(?:[a-z.]{2,24}).*(?:tag=duckduckgo-)/,
    ],
  },
  yahoo: {
    regexp: /^https:\/\/(?:.*)search\.yahoo\.com\/search/,
    queryParam: "p",
  },
  baidu: {
    regexp: /^https:\/\/www\.baidu\.com\/(?:s|baidu)/,
    queryParam: "wd",
    codeParam: "tn",
    codePrefixes: ["34046034_", "monline_"],
    followonParams: ["oq"],
  },
  bing: {
    regexp: /^https:\/\/www\.bing\.com\/search/,
    queryParam: "q",
    codeParam: "pc",
    codePrefixes: ["MOZ", "MZ"],
    followonCookies: [
      {
        extraCodeParam: "form",
        extraCodePrefixes: ["QBRE"],
        host: "www.bing.com",
        name: "SRCHS",
        codeParam: "PC",
        codePrefixes: ["MOZ", "MZ"],
      },
    ],
    extraAdServersRegexps: [
      /^https:\/\/www\.bing\.com\/acli?c?k/,
      /^https:\/\/www\.bing\.com\/fd\/ls\/GLinkPingPost\.aspx.*acli?c?k/,
    ],
  },
};

const BROWSER_SEARCH_PREF = "browser.search.";

XPCOMUtils.defineLazyPreferenceGetter(
  this,
  "loggingEnabled",
  BROWSER_SEARCH_PREF + "log",
  false
);

/**
 * TelemetryHandler is the main class handling search telemetry. It primarily
 * deals with tracking of what pages are loaded into tabs.
 *
 * It handles the *in-content:sap* keys of the SEARCH_COUNTS histogram.
 */
class TelemetryHandler {
  constructor() {
    // _browserInfoByURL is a map of tracked search urls to objects containing:
    // * {object} info
    //   the search provider information associated with the url.
    // * {WeakSet} browsers
    //   a weak set of browsers that have the url loaded.
    // * {integer} count
    //   a manual count of browsers logged.
    // We keep a weak set of browsers, in case we miss something on our counts
    // and cause a memory leak - worst case our map is slightly bigger than it
    // needs to be.
    // The manual count is because WeakSet doesn't give us size/length
    // information, but we want to know when we can clean up our associated
    // entry.
    this._browserInfoByURL = new Map();
    this._initialized = false;
    this.__searchProviderInfo = null;
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
    this._contentHandler.overrideSearchTelemetryForTests(
      this.__searchProviderInfo
    );
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
      let item = this._browserInfoByURL.get(url);
      if (item) {
        item.browsers.add(browser);
        item.count++;
      } else {
        this._browserInfoByURL.set(url, {
          browsers: new WeakSet([browser]),
          info,
          count: 1,
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
   *   - {WeakSet} browsers
   *     Set of browser elements that belong to `url`.
   *   - {object} info
   *     Info dictionary as returned by `_checkURLForSerpMatch`.
   *   - {number} count
   *     The number of browser element we can most accurately tell we're
   *     tracking, since they're inside a WeakSet.
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
  _getProviderInfoForURL(url, useOnlyExtraAdServers = false) {
    if (useOnlyExtraAdServers) {
      return Object.entries(this._searchProviderInfo).find(([_, info]) => {
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

    return Object.entries(this._searchProviderInfo).find(([_, info]) =>
      info.regexp.test(url)
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
    let info = this._getProviderInfoForURL(url);
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
      if (
        code &&
        searchProviderInfo.codePrefixes.some(p => code.startsWith(p))
      ) {
        if (
          searchProviderInfo.followonParams &&
          searchProviderInfo.followonParams.some(p => queries.has(p))
        ) {
          type = "sap-follow-on";
        } else {
          type = "sap";
        }
      } else if (searchProviderInfo.followonCookies) {
        // Especially Bing requires lots of extra work related to cookies.
        for (let followonCookie of searchProviderInfo.followonCookies) {
          if (followonCookie.extraCodeParam) {
            let eCode = queries.get(followonCookie.extraCodeParam);
            if (
              !eCode ||
              !followonCookie.extraCodePrefixes.some(p => eCode.startsWith(p))
            ) {
              continue;
            }
          }

          // If this cookie is present, it's probably an SAP follow-on.
          // This might be an organic follow-on in the same session, but there
          // is no way to tell the difference.
          for (let cookie of Services.cookies.getCookiesFromHost(
            followonCookie.host,
            {}
          )) {
            if (cookie.name != followonCookie.name) {
              continue;
            }

            let [cookieParam, cookieValue] = cookie.value
              .split("=")
              .map(p => p.trim());
            if (
              cookieParam == followonCookie.codeParam &&
              followonCookie.codePrefixes.some(p => cookieValue.startsWith(p))
            ) {
              type = "sap-follow-on";
              code = cookieValue;
              break;
            }
          }
        }
      }
    }
    return { provider, type, code };
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
    let payload = `${info.provider}.in-content:${info.type}:${info.code ||
      "none"}`;
    let histogram = Services.telemetry.getKeyedHistogramById(
      SEARCH_COUNTS_HISTOGRAM_KEY
    );
    histogram.add(payload);
    LOG(`${payload} for ${url}`);
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
   */
  init() {
    Services.ppmm.sharedData.set(
      "SearchTelemetry:ProviderInfo",
      SEARCH_PROVIDER_INFO
    );

    Cc["@mozilla.org/network/http-activity-distributor;1"]
      .getService(Ci.nsIHttpActivityDistributor)
      .addObserver(this);

    Services.obs.addObserver(this, "http-on-stop-request");
  }

  /**
   * Uninitializes the content handler.
   */
  uninit() {
    Cc["@mozilla.org/network/http-activity-distributor;1"]
      .getService(Ci.nsIHttpActivityDistributor)
      .removeObserver(this);

    Services.obs.removeObserver(this, "http-on-stop-request");
  }

  /**
   * Receives a message from the SearchTelemetryChild actor.
   *
   * @param {object} msg
   */
  receiveMessage(msg) {
    if (msg.name != "SearchTelemetry:PageInfo") {
      LOG("Received unexpected message: " + msg.name);
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
      if (
        channel.loadInfo.loadingPrincipal &&
        channel.loadInfo.loadingPrincipal.URI
      ) {
        return channel.loadInfo.loadingPrincipal.URI.spec;
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
    }
  }

  /**
   * Listener that observes network activity, so that we can determine if a link
   * from a search provider page was followed, and if then if that link was an
   * ad click or not.
   *
   * @param {nsIChannel} nativeChannel   The channel that generated the activity.
   * @param {number}     activityType    The type of activity.
   * @param {number}     activitySubtype The subtype for the activity.
   */
  observeActivity(
    nativeChannel,
    activityType,
    activitySubtype /*,
    timestamp,
    extraSizeData,
    extraStringData*/
  ) {
    // NOTE: the channel handling code here is inspired by WebRequest.jsm.
    if (
      !this._browserInfoByURL.size ||
      activityType !=
        Ci.nsIHttpActivityObserver.ACTIVITY_TYPE_HTTP_TRANSACTION ||
      activitySubtype !=
        Ci.nsIHttpActivityObserver.ACTIVITY_SUBTYPE_TRANSACTION_CLOSE
    ) {
      return;
    }

    // Sometimes we get a NullHttpChannel, which implements nsIHttpChannel but
    // not nsIChannel.
    if (!(nativeChannel instanceof Ci.nsIChannel)) {
      return;
    }
    let channel = ChannelWrapper.get(nativeChannel);
    // The wrapper is consistent across redirects, so we can use it to track state.
    if (channel._adClickRecorded) {
      LOG("Ad click already recorded");
      return;
    }

    // Make a trip through the event loop to make sure statuses have a chance to
    // be processed before we get all the info.
    Services.tm.dispatchToMainThread(() => {
      // We suspect that No Content (204) responses are used to transfer or
      // update beacons. They lead to use double-counting ad-clicks, so let's
      // ignore them.
      if (channel.statusCode == 204) {
        LOG("Ignoring activity from ambiguous responses");
        return;
      }

      let originURL = channel.originURI && channel.originURI.spec;
      let info = this._findBrowserItemForURL(originURL);
      if (!originURL || !info) {
        return;
      }

      let URL = channel.finalURL;
      info = this._getProviderInfoForURL(URL, true);
      if (!info) {
        return;
      }

      try {
        Services.telemetry.keyedScalarAdd(SEARCH_AD_CLICKS_SCALAR, info[0], 1);
        channel._adClickRecorded = true;
        LOG(`Counting ad click in page for ${info[0]} ${originURL} ${URL}`);
      } catch (e) {
        Cu.reportError(e);
      }
    });
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
    let item = this._findBrowserItemForURL(info.url);
    if (!item) {
      LOG(
        `Expected to report URI for ${
          info.url
        } with ads but couldn't find the information`
      );
      return;
    }

    Services.telemetry.keyedScalarAdd(
      SEARCH_WITH_ADS_SCALAR,
      item.info.provider,
      1
    );
    LOG(`Counting ads in page for ${item.info.provider} ${info.url}`);
  }
}

/**
 * Outputs the message to the JavaScript console as well as to stdout.
 *
 * @param {string} msg The message to output.
 */
function LOG(msg) {
  if (loggingEnabled) {
    dump(`*** SearchTelemetry: ${msg}\n`);
    Services.console.logStringMessage(msg);
  }
}

var SearchTelemetry = new TelemetryHandler();
