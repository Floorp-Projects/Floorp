/* -*- js-indent-level: 2; indent-tabs-mode: nil -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

this.EXPORTED_SYMBOLS = ["BrowserUsageTelemetry"];

const {classes: Cc, interfaces: Ci, utils: Cu} = Components;

Cu.import("resource://gre/modules/Services.jsm");
Cu.import("resource://gre/modules/XPCOMUtils.jsm");

XPCOMUtils.defineLazyModuleGetter(this, "PrivateBrowsingUtils",
                                  "resource://gre/modules/PrivateBrowsingUtils.jsm");

// The upper bound for the count of the visited unique domain names.
const MAX_UNIQUE_VISITED_DOMAINS = 100;

// Observed topic names.
const WINDOWS_RESTORED_TOPIC = "sessionstore-windows-restored";
const TAB_RESTORING_TOPIC = "SSTabRestoring";
const TELEMETRY_SUBSESSIONSPLIT_TOPIC = "internal-telemetry-after-subsession-split";
const DOMWINDOW_OPENED_TOPIC = "domwindowopened";

// Probe names.
const MAX_TAB_COUNT_SCALAR_NAME = "browser.engagement.max_concurrent_tab_count";
const MAX_WINDOW_COUNT_SCALAR_NAME = "browser.engagement.max_concurrent_window_count";
const TAB_OPEN_EVENT_COUNT_SCALAR_NAME = "browser.engagement.tab_open_event_count";
const WINDOW_OPEN_EVENT_COUNT_SCALAR_NAME = "browser.engagement.window_open_event_count";
const UNIQUE_DOMAINS_COUNT_SCALAR_NAME = "browser.engagement.unique_domains_count";
const TOTAL_URI_COUNT_SCALAR_NAME = "browser.engagement.total_uri_count";
const UNFILTERED_URI_COUNT_SCALAR_NAME = "browser.engagement.unfiltered_uri_count";

// A list of known search origins.
const KNOWN_SEARCH_SOURCES = [
  "abouthome",
  "contextmenu",
  "newtab",
  "searchbar",
  "urlbar",
];

const KNOWN_ONEOFF_SOURCES = [
  "oneoff-urlbar",
  "oneoff-searchbar",
  "unknown", // Edge case: this is the searchbar (see bug 1195733 comment 7).
];

function getOpenTabsAndWinsCounts() {
  let tabCount = 0;
  let winCount = 0;

  let browserEnum = Services.wm.getEnumerator("navigator:browser");
  while (browserEnum.hasMoreElements()) {
    let win = browserEnum.getNext();
    winCount++;
    tabCount += win.gBrowser.tabs.length;
  }

  return { tabCount, winCount };
}

function getSearchEngineId(engine) {
  if (engine) {
    if (engine.identifier) {
      return engine.identifier;
    }
    // Due to bug 1222070, we can't directly check Services.telemetry.canRecordExtended
    // here.
    const extendedTelemetry = Services.prefs.getBoolPref("toolkit.telemetry.enabled");
    if (engine.name && extendedTelemetry) {
      // If it's a custom search engine only report the engine name
      // if extended Telemetry is enabled.
      return "other-" + engine.name;
    }
  }
  return "other";
}

let URICountListener = {
  // A set containing the visited domains, see bug 1271310.
  _domainSet: new Set(),
  // A map to keep track of the URIs loaded from the restored tabs.
  _restoredURIsMap: new WeakMap(),

  isHttpURI(uri) {
    // Only consider http(s) schemas.
    return uri.schemeIs("http") || uri.schemeIs("https");
  },

  addRestoredURI(browser, uri) {
    if (!this.isHttpURI(uri)) {
      return;
    }

    this._restoredURIsMap.set(browser, uri.spec);
  },

  onLocationChange(browser, webProgress, request, uri, flags) {
    // Don't count this URI if it's an error page.
    if (flags & Ci.nsIWebProgressListener.LOCATION_CHANGE_ERROR_PAGE) {
      return;
    }

    // We only care about top level loads.
    if (!webProgress.isTopLevel) {
      return;
    }

    // The SessionStore sets the URI of a tab first, firing onLocationChange the
    // first time, then manages content loading using its scheduler. Once content
    // loads, we will hit onLocationChange again.
    // We can catch the first case by checking for null requests: be advised that
    // this can also happen when navigating page fragments, so account for it.
    if (!request &&
        !(flags & Ci.nsIWebProgressListener.LOCATION_CHANGE_SAME_DOCUMENT)) {
      return;
    }

    // Track URI loads, even if they're not http(s).
    let uriSpec = null;
    try {
      uriSpec = uri.spec;
    } catch (e) {
      // If we have troubles parsing the spec, still count this as
      // an unfiltered URI.
      Services.telemetry.scalarAdd(UNFILTERED_URI_COUNT_SCALAR_NAME, 1);
      return;
    }


    // Don't count about:blank and similar pages, as they would artificially
    // inflate the counts.
    if (browser.ownerDocument.defaultView.gInitialPages.includes(uriSpec)) {
      return;
    }

    // If the URI we're loading is in the _restoredURIsMap, then it comes from a
    // restored tab. If so, let's skip it and remove it from the map as we want to
    // count page refreshes.
    if (this._restoredURIsMap.get(browser) === uriSpec) {
      this._restoredURIsMap.delete(browser);
      return;
    }

    // The URI wasn't from a restored tab. Count it among the unfiltered URIs.
    // If this is an http(s) URI, this also gets counted by the "total_uri_count"
    // probe.
    Services.telemetry.scalarAdd(UNFILTERED_URI_COUNT_SCALAR_NAME, 1);

    if (!this.isHttpURI(uri)) {
      return;
    }

    // Update the URI counts.
    Services.telemetry.scalarAdd(TOTAL_URI_COUNT_SCALAR_NAME, 1);

    // We only want to count the unique domains up to MAX_UNIQUE_VISITED_DOMAINS.
    if (this._domainSet.size == MAX_UNIQUE_VISITED_DOMAINS) {
      return;
    }

    // Unique domains should be aggregated by (eTLD + 1): x.test.com and y.test.com
    // are counted once as test.com.
    try {
      // Even if only considering http(s) URIs, |getBaseDomain| could still throw
      // due to the URI containing invalid characters or the domain actually being
      // an ipv4 or ipv6 address.
      this._domainSet.add(Services.eTLD.getBaseDomain(uri));
    } catch (e) {
      return;
    }

    Services.telemetry.scalarSet(UNIQUE_DOMAINS_COUNT_SCALAR_NAME, this._domainSet.size);
  },

  /**
   * Reset the counts. This should be called when breaking a session in Telemetry.
   */
  reset() {
    this._domainSet.clear();
  },

  QueryInterface: XPCOMUtils.generateQI([Ci.nsIWebProgressListener,
                                         Ci.nsISupportsWeakReference]),
};

let BrowserUsageTelemetry = {
  init() {
    Services.obs.addObserver(this, WINDOWS_RESTORED_TOPIC, false);
  },

  /**
   * Handle subsession splits in the parent process.
   */
  afterSubsessionSplit() {
    // Scalars just got cleared due to a subsession split. We need to set the maximum
    // concurrent tab and window counts so that they reflect the correct value for the
    // new subsession.
    const counts = getOpenTabsAndWinsCounts();
    Services.telemetry.scalarSetMaximum(MAX_TAB_COUNT_SCALAR_NAME, counts.tabCount);
    Services.telemetry.scalarSetMaximum(MAX_WINDOW_COUNT_SCALAR_NAME, counts.winCount);

    // Reset the URI counter.
    URICountListener.reset();
  },

  uninit() {
    Services.obs.removeObserver(this, DOMWINDOW_OPENED_TOPIC, false);
    Services.obs.removeObserver(this, TELEMETRY_SUBSESSIONSPLIT_TOPIC, false);
    Services.obs.removeObserver(this, WINDOWS_RESTORED_TOPIC, false);
  },

  observe(subject, topic, data) {
    switch (topic) {
      case WINDOWS_RESTORED_TOPIC:
        this._setupAfterRestore();
        break;
      case DOMWINDOW_OPENED_TOPIC:
        this._onWindowOpen(subject);
        break;
      case TELEMETRY_SUBSESSIONSPLIT_TOPIC:
        this.afterSubsessionSplit();
        break;
    }
  },

  handleEvent(event) {
    switch (event.type) {
      case "TabOpen":
        this._onTabOpen();
        break;
      case "unload":
        this._unregisterWindow(event.target);
        break;
      case TAB_RESTORING_TOPIC:
        // We're restoring a new tab from a previous or crashed session.
        // We don't want to track the URIs from these tabs, so let
        // |URICountListener| know about them.
        let browser = event.target.linkedBrowser;
        URICountListener.addRestoredURI(browser, browser.currentURI);
        break;
    }
  },

  /**
   * The main entry point for recording search related Telemetry. This includes
   * search counts and engagement measurements.
   *
   * Telemetry records only search counts per engine and action origin, but
   * nothing pertaining to the search contents themselves.
   *
   * @param {nsISearchEngine} engine
   *        The engine handling the search.
   * @param {String} source
   *        Where the search originated from. See KNOWN_SEARCH_SOURCES for allowed
   *        values.
   * @param {Object} [details] Options object.
   * @param {Boolean} [details.isOneOff=false]
   *        true if this event was generated by a one-off search.
   * @param {Boolean} [details.isSuggestion=false]
   *        true if this event was generated by a suggested search.
   * @param {Boolean} [details.isAlias=false]
   *        true if this event was generated by a search using an alias.
   * @param {Object} [details.type=null]
   *        The object describing the event that triggered the search.
   * @throws if source is not in the known sources list.
   */
  recordSearch(engine, source, details = {}) {
    const isOneOff = !!details.isOneOff;
    const countId = getSearchEngineId(engine) + "." + source;

    if (isOneOff) {
      if (!KNOWN_ONEOFF_SOURCES.includes(source)) {
        // Silently drop the error if this bogus call
        // came from 'urlbar' or 'searchbar'. They're
        // calling |recordSearch| twice from two different
        // code paths because they want to record the search
        // in SEARCH_COUNTS.
        if (['urlbar', 'searchbar'].includes(source)) {
          Services.telemetry.getKeyedHistogramById("SEARCH_COUNTS").add(countId);
          return;
        }
        throw new Error("Unknown source for one-off search: " + source);
      }
    } else {
      if (!KNOWN_SEARCH_SOURCES.includes(source)) {
        throw new Error("Unknown source for search: " + source);
      }
      Services.telemetry.getKeyedHistogramById("SEARCH_COUNTS").add(countId);
    }

    // Dispatch the search signal to other handlers.
    this._handleSearchAction(engine, source, details);
  },

  _recordSearch(engine, source, action = null) {
    let scalarKey = action ? "search_" + action : "search";
    Services.telemetry.keyedScalarAdd("browser.engagement.navigation." + source,
                                      scalarKey, 1);
    Services.telemetry.recordEvent("navigation", "search", source, action,
                                   { engine: getSearchEngineId(engine) });
  },

  _handleSearchAction(engine, source, details) {
    switch (source) {
      case "urlbar":
      case "oneoff-urlbar":
      case "searchbar":
      case "oneoff-searchbar":
      case "unknown": // Edge case: this is the searchbar (see bug 1195733 comment 7).
        this._handleSearchAndUrlbar(engine, source, details);
        break;
      case "abouthome":
        this._recordSearch(engine, "about_home", "enter");
        break;
      case "newtab":
        this._recordSearch(engine, "about_newtab", "enter");
        break;
      case "contextmenu":
        this._recordSearch(engine, "contextmenu");
        break;
    }
  },

  /**
   * This function handles the "urlbar", "urlbar-oneoff", "searchbar" and
   * "searchbar-oneoff" sources.
   */
  _handleSearchAndUrlbar(engine, source, details) {
    // We want "urlbar" and "urlbar-oneoff" (and similar cases) to go in the same
    // scalar, but in a different key.

    // When using one-offs in the searchbar we get an "unknown" source. See bug
    // 1195733 comment 7 for the context. Fix-up the label here.
    const sourceName =
      (source === "unknown") ? "searchbar" : source.replace("oneoff-", "");

    const isOneOff = !!details.isOneOff;
    if (isOneOff) {
      // We will receive a signal from the "urlbar"/"searchbar" even when the
      // search came from "oneoff-urlbar". That's because both signals
      // are propagated from search.xml. Skip it if that's the case.
      // Moreover, we skip the "unknown" source that comes from the searchbar
      // when performing searches from the default search engine. See bug 1195733
      // comment 7 for context.
      if (["urlbar", "searchbar", "unknown"].includes(source)) {
        return;
      }

      // If that's a legit one-off search signal, record it using the relative key.
      this._recordSearch(engine, sourceName, "oneoff");
      return;
    }

    // The search was not a one-off. It was a search with the default search engine.
    if (details.isSuggestion) {
      // It came from a suggested search, so count it as such.
      this._recordSearch(engine, sourceName, "suggestion");
      return;
    } else if (details.isAlias) {
      // This one came from a search that used an alias.
      this._recordSearch(engine, sourceName, "alias");
      return;
    }

    // The search signal was generated by typing something and pressing enter.
    this._recordSearch(engine, sourceName, "enter");
  },

  /**
   * This gets called shortly after the SessionStore has finished restoring
   * windows and tabs. It counts the open tabs and adds listeners to all the
   * windows.
   */
  _setupAfterRestore() {
    // Make sure to catch new chrome windows and subsession splits.
    Services.obs.addObserver(this, DOMWINDOW_OPENED_TOPIC, false);
    Services.obs.addObserver(this, TELEMETRY_SUBSESSIONSPLIT_TOPIC, false);

    // Attach the tabopen handlers to the existing Windows.
    let browserEnum = Services.wm.getEnumerator("navigator:browser");
    while (browserEnum.hasMoreElements()) {
      this._registerWindow(browserEnum.getNext());
    }

    // Get the initial tab and windows max counts.
    const counts = getOpenTabsAndWinsCounts();
    Services.telemetry.scalarSetMaximum(MAX_TAB_COUNT_SCALAR_NAME, counts.tabCount);
    Services.telemetry.scalarSetMaximum(MAX_WINDOW_COUNT_SCALAR_NAME, counts.winCount);
  },

  /**
   * Adds listeners to a single chrome window.
   */
  _registerWindow(win) {
    win.addEventListener("unload", this);
    win.addEventListener("TabOpen", this, true);

    // Don't include URI and domain counts when in private mode.
    if (PrivateBrowsingUtils.isWindowPrivate(win)) {
      return;
    }
    win.gBrowser.tabContainer.addEventListener(TAB_RESTORING_TOPIC, this);
    win.gBrowser.addTabsProgressListener(URICountListener);
  },

  /**
   * Removes listeners from a single chrome window.
   */
  _unregisterWindow(win) {
    win.removeEventListener("unload", this);
    win.removeEventListener("TabOpen", this, true);

    // Don't include URI and domain counts when in private mode.
    if (PrivateBrowsingUtils.isWindowPrivate(win.defaultView)) {
      return;
    }
    win.defaultView.gBrowser.tabContainer.removeEventListener(TAB_RESTORING_TOPIC, this);
    win.defaultView.gBrowser.removeTabsProgressListener(URICountListener);
  },

  /**
   * Updates the tab counts.
   * @param {Number} [newTabCount=0] The count of the opened tabs across all windows. This
   *        is computed manually if not provided.
   */
  _onTabOpen(tabCount = 0) {
    // Use the provided tab count if available. Otherwise, go on and compute it.
    tabCount = tabCount || getOpenTabsAndWinsCounts().tabCount;
    // Update the "tab opened" count and its maximum.
    Services.telemetry.scalarAdd(TAB_OPEN_EVENT_COUNT_SCALAR_NAME, 1);
    Services.telemetry.scalarSetMaximum(MAX_TAB_COUNT_SCALAR_NAME, tabCount);
  },

  /**
   * Tracks the window count and registers the listeners for the tab count.
   * @param{Object} win The window object.
   */
  _onWindowOpen(win) {
    // Make sure to have a |nsIDOMWindow|.
    if (!(win instanceof Ci.nsIDOMWindow)) {
      return;
    }

    let onLoad = () => {
      win.removeEventListener("load", onLoad, false);

      // Ignore non browser windows.
      if (win.document.documentElement.getAttribute("windowtype") != "navigator:browser") {
        return;
      }

      this._registerWindow(win);
      // Track the window open event and check the maximum.
      const counts = getOpenTabsAndWinsCounts();
      Services.telemetry.scalarAdd(WINDOW_OPEN_EVENT_COUNT_SCALAR_NAME, 1);
      Services.telemetry.scalarSetMaximum(MAX_WINDOW_COUNT_SCALAR_NAME, counts.winCount);

      // We won't receive the "TabOpen" event for the first tab within a new window.
      // Account for that.
      this._onTabOpen(counts.tabCount);
    };
    win.addEventListener("load", onLoad, false);
  },
};
