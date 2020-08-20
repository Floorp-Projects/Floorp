/* -*- js-indent-level: 2; indent-tabs-mode: nil -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

var EXPORTED_SYMBOLS = [
  "BrowserUsageTelemetry",
  "getUniqueDomainsVisitedInPast24Hours",
  "URICountListener",
  "MINIMUM_TAB_COUNT_INTERVAL_MS",
];

const { XPCOMUtils } = ChromeUtils.import(
  "resource://gre/modules/XPCOMUtils.jsm"
);

XPCOMUtils.defineLazyModuleGetters(this, {
  AppConstants: "resource://gre/modules/AppConstants.jsm",
  ClientID: "resource://gre/modules/ClientID.jsm",
  BrowserUtils: "resource://gre/modules/BrowserUtils.jsm",
  CustomizableUI: "resource:///modules/CustomizableUI.jsm",
  OS: "resource://gre/modules/osfile.jsm",
  PageActions: "resource:///modules/PageActions.jsm",
  PartnerLinkAttribution: "resource:///modules/PartnerLinkAttribution.jsm",
  PrivateBrowsingUtils: "resource://gre/modules/PrivateBrowsingUtils.jsm",
  SearchTelemetry: "resource:///modules/SearchTelemetry.jsm",
  Services: "resource://gre/modules/Services.jsm",
  setTimeout: "resource://gre/modules/Timer.jsm",
  clearTimeout: "resource://gre/modules/Timer.jsm",
});

// This pref is in seconds!
XPCOMUtils.defineLazyPreferenceGetter(
  this,
  "gRecentVisitedOriginsExpiry",
  "browser.engagement.recent_visited_origins.expiry"
);

// The upper bound for the count of the visited unique domain names.
const MAX_UNIQUE_VISITED_DOMAINS = 100;

// Observed topic names.
const TAB_RESTORING_TOPIC = "SSTabRestoring";
const TELEMETRY_SUBSESSIONSPLIT_TOPIC =
  "internal-telemetry-after-subsession-split";
const DOMWINDOW_OPENED_TOPIC = "domwindowopened";

// Probe names.
const MAX_TAB_COUNT_SCALAR_NAME = "browser.engagement.max_concurrent_tab_count";
const MAX_WINDOW_COUNT_SCALAR_NAME =
  "browser.engagement.max_concurrent_window_count";
const TAB_OPEN_EVENT_COUNT_SCALAR_NAME =
  "browser.engagement.tab_open_event_count";
const MAX_TAB_PINNED_COUNT_SCALAR_NAME =
  "browser.engagement.max_concurrent_tab_pinned_count";
const TAB_PINNED_EVENT_COUNT_SCALAR_NAME =
  "browser.engagement.tab_pinned_event_count";
const WINDOW_OPEN_EVENT_COUNT_SCALAR_NAME =
  "browser.engagement.window_open_event_count";
const UNIQUE_DOMAINS_COUNT_SCALAR_NAME =
  "browser.engagement.unique_domains_count";
const TOTAL_URI_COUNT_SCALAR_NAME = "browser.engagement.total_uri_count";
const UNFILTERED_URI_COUNT_SCALAR_NAME =
  "browser.engagement.unfiltered_uri_count";

// A list of known search origins.
const KNOWN_SEARCH_SOURCES = [
  "abouthome",
  "contextmenu",
  "newtab",
  "searchbar",
  "system",
  "urlbar",
  "webextension",
];

const KNOWN_ONEOFF_SOURCES = [
  "oneoff-urlbar",
  "oneoff-searchbar",
  "unknown", // Edge case: this is the searchbar (see bug 1195733 comment 7).
];

const MINIMUM_TAB_COUNT_INTERVAL_MS = 5 * 60 * 1000; // 5 minutes, in ms

// The elements we consider to be interactive.
const UI_TARGET_ELEMENTS = [
  "menuitem",
  "toolbarbutton",
  "key",
  "command",
  "checkbox",
  "input",
  "button",
  "image",
  "radio",
  "richlistitem",
];

// The containers of interactive elements that we care about and their pretty
// names. These should be listed in order of most-specific to least-specific,
// when iterating JavaScript will guarantee that ordering and so we will find
// the most specific area first.
const BROWSER_UI_CONTAINER_IDS = {
  "toolbar-menubar": "menu-bar",
  TabsToolbar: "tabs-bar",
  PersonalToolbar: "bookmarks-bar",
  "appMenu-popup": "app-menu",
  tabContextMenu: "tabs-context",
  contentAreaContextMenu: "content-context",
  "widget-overflow-list": "overflow-menu",
  "widget-overflow-fixed-list": "pinned-overflow-menu",
  "page-action-buttons": "pageaction-urlbar",
  pageActionPanel: "pageaction-panel",

  // This should appear last as some of the above are inside the nav bar.
  "nav-bar": "nav-bar",
};

// A list of the expected panes in about:preferences
const PREFERENCES_PANES = [
  "paneHome",
  "paneGeneral",
  "panePrivacy",
  "paneSearch",
  "paneSearchResults",
  "paneSync",
  "paneContainers",
  "paneExperimental",
];

const IGNORABLE_EVENTS = new WeakMap();

const KNOWN_ADDONS = [];

function telemetryId(widgetId, obscureAddons = true) {
  // Add-on IDs need to be obscured.
  function addonId(id) {
    if (!obscureAddons) {
      return id;
    }

    let pos = KNOWN_ADDONS.indexOf(id);
    if (pos < 0) {
      pos = KNOWN_ADDONS.length;
      KNOWN_ADDONS.push(id);
    }
    return `addon${pos}`;
  }

  if (widgetId.endsWith("-browser-action")) {
    widgetId = addonId(
      widgetId.substring(0, widgetId.length - "-browser-action".length)
    );
  } else if (widgetId.startsWith("pageAction-")) {
    let actionId;
    if (widgetId.startsWith("pageAction-urlbar-")) {
      actionId = widgetId.substring("pageAction-urlbar-".length);
    } else if (widgetId.startsWith("pageAction-panel-")) {
      actionId = widgetId.substring("pageAction-panel-".length);
    }

    if (actionId) {
      let action = PageActions.actionForID(actionId);
      widgetId = action?._isMozillaAction ? actionId : addonId(actionId);
    }
  } else if (widgetId.startsWith("ext-keyset-id-")) {
    // Webextension command shortcuts don't have an id on their key element so
    // we see the id from the keyset that contains them.
    widgetId = addonId(widgetId.substring("ext-keyset-id-".length));
  } else if (widgetId.startsWith("ext-key-id-")) {
    // The command for a webextension sidebar action is an exception to the above rule.
    widgetId = widgetId.substring("ext-key-id-".length);
    if (widgetId.endsWith("-sidebar-action")) {
      widgetId = addonId(
        widgetId.substring(0, widgetId.length - "-sidebar-action".length)
      );
    }
  }

  return widgetId.replace(/_/g, "-");
}

function getOpenTabsAndWinsCounts() {
  let loadedTabCount = 0;
  let tabCount = 0;
  let winCount = 0;

  for (let win of Services.wm.getEnumerator("navigator:browser")) {
    winCount++;
    tabCount += win.gBrowser.tabs.length;
    for (const tab of win.gBrowser.tabs) {
      if (tab.getAttribute("pending") !== "true") {
        loadedTabCount += 1;
      }
    }
  }

  return { loadedTabCount, tabCount, winCount };
}

function getPinnedTabsCount() {
  let pinnedTabs = 0;

  for (let win of Services.wm.getEnumerator("navigator:browser")) {
    pinnedTabs += [...win.ownerGlobal.gBrowser.tabs].filter(t => t.pinned)
      .length;
  }

  return pinnedTabs;
}

function shouldRecordSearchCount(tabbrowser) {
  return (
    !PrivateBrowsingUtils.isWindowPrivate(tabbrowser.ownerGlobal) ||
    !Services.prefs.getBoolPref("browser.engagement.search_counts.pbm", false)
  );
}

let URICountListener = {
  // A set containing the visited domains, see bug 1271310.
  _domainSet: new Set(),
  // A set containing the visited origins during the last 24 hours (similar to domains, but not quite the same)
  _domain24hrSet: new Set(),
  // A map to keep track of the URIs loaded from the restored tabs.
  _restoredURIsMap: new WeakMap(),
  // Ongoing expiration timeouts.
  _timeouts: new Set(),

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

  onStateChange(browser, webProgress, request, stateFlags, status) {
    if (
      !webProgress.isTopLevel ||
      !(stateFlags & Ci.nsIWebProgressListener.STATE_STOP) ||
      !(stateFlags & Ci.nsIWebProgressListener.STATE_IS_WINDOW)
    ) {
      return;
    }

    if (!(request instanceof Ci.nsIChannel) || !this.isHttpURI(request.URI)) {
      return;
    }

    BrowserUsageTelemetry._recordSiteOriginsPerLoadedTabs();
  },

  onLocationChange(browser, webProgress, request, uri, flags) {
    if (!(flags & Ci.nsIWebProgressListener.LOCATION_CHANGE_SAME_DOCUMENT)) {
      // By default, assume we no longer need to track this tab.
      SearchTelemetry.stopTrackingBrowser(browser);
    }

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
    if (
      !request &&
      !(flags & Ci.nsIWebProgressListener.LOCATION_CHANGE_SAME_DOCUMENT)
    ) {
      return;
    }

    // Don't include URI and domain counts when in private mode.
    let shouldCountURI =
      !PrivateBrowsingUtils.isWindowPrivate(browser.ownerGlobal) ||
      Services.prefs.getBoolPref(
        "browser.engagement.total_uri_count.pbm",
        false
      );

    // Track URI loads, even if they're not http(s).
    let uriSpec = null;
    try {
      uriSpec = uri.spec;
    } catch (e) {
      // If we have troubles parsing the spec, still count this as
      // an unfiltered URI.
      if (shouldCountURI) {
        Services.telemetry.scalarAdd(UNFILTERED_URI_COUNT_SCALAR_NAME, 1);
      }
      return;
    }

    // Don't count about:blank and similar pages, as they would artificially
    // inflate the counts.
    if (browser.ownerGlobal.gInitialPages.includes(uriSpec)) {
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
    if (shouldCountURI) {
      Services.telemetry.scalarAdd(UNFILTERED_URI_COUNT_SCALAR_NAME, 1);
    }

    if (!this.isHttpURI(uri)) {
      return;
    }

    if (
      shouldRecordSearchCount(browser.getTabBrowser()) &&
      !(flags & Ci.nsIWebProgressListener.LOCATION_CHANGE_SAME_DOCUMENT)
    ) {
      SearchTelemetry.updateTrackingStatus(browser, uriSpec);
    }

    if (!shouldCountURI) {
      return;
    }

    // Update the URI counts.
    Services.telemetry.scalarAdd(TOTAL_URI_COUNT_SCALAR_NAME, 1);

    // Update tab count
    BrowserUsageTelemetry._recordTabCounts(getOpenTabsAndWinsCounts());

    // Unique domains should be aggregated by (eTLD + 1): x.test.com and y.test.com
    // are counted once as test.com.
    let baseDomain;
    try {
      // Even if only considering http(s) URIs, |getBaseDomain| could still throw
      // due to the URI containing invalid characters or the domain actually being
      // an ipv4 or ipv6 address.
      baseDomain = Services.eTLD.getBaseDomain(uri);
    } catch (e) {
      return;
    }

    // We only want to count the unique domains up to MAX_UNIQUE_VISITED_DOMAINS.
    if (this._domainSet.size < MAX_UNIQUE_VISITED_DOMAINS) {
      this._domainSet.add(baseDomain);
      Services.telemetry.scalarSet(
        UNIQUE_DOMAINS_COUNT_SCALAR_NAME,
        this._domainSet.size
      );
    }

    this._domain24hrSet.add(baseDomain);
    if (gRecentVisitedOriginsExpiry) {
      let timeoutId = setTimeout(() => {
        this._domain24hrSet.delete(baseDomain);
        this._timeouts.delete(timeoutId);
      }, gRecentVisitedOriginsExpiry * 1000);
      this._timeouts.add(timeoutId);
    }
  },

  /**
   * Reset the counts. This should be called when breaking a session in Telemetry.
   */
  reset() {
    this._domainSet.clear();
  },

  /**
   * Returns the number of unique domains visited in this session during the
   * last 24 hours.
   */
  get uniqueDomainsVisitedInPast24Hours() {
    return this._domain24hrSet.size;
  },

  /**
   * Resets the number of unique domains visited in this session.
   */
  resetUniqueDomainsVisitedInPast24Hours() {
    this._timeouts.forEach(timeoutId => clearTimeout(timeoutId));
    this._timeouts.clear();
    this._domain24hrSet.clear();
  },

  QueryInterface: ChromeUtils.generateQI([
    "nsIWebProgressListener",
    "nsISupportsWeakReference",
  ]),
};

let BrowserUsageTelemetry = {
  /**
   * This is a policy object used to override behavior for testing.
   */
  Policy: {
    getTelemetryClientId: async () => ClientID.getClientID(),
    getUpdateDirectory: () => Services.dirsvc.get("UpdRootD", Ci.nsIFile),
    readProfileCountFile: async path =>
      OS.File.read(path, { encoding: "UTF-8" }),
    writeProfileCountFile: async (path, data) =>
      OS.File.writeAtomic(path, data),
  },

  _inited: false,

  init() {
    this._lastRecordTabCount = 0;
    this._lastRecordLoadedTabCount = 0;
    this._lastRecordSiteOriginsPerLoadedTabs = 0;
    this._setupAfterRestore();
    this._inited = true;

    Services.prefs.addObserver("browser.tabs.extraDragSpace", this);
    Services.prefs.addObserver("browser.tabs.drawInTitlebar", this);

    this._recordUITelemetry();
  },

  /**
   * Resets the masked add-on identifiers. Only for use in tests.
   */
  _resetAddonIds() {
    KNOWN_ADDONS.length = 0;
  },

  /**
   * Handle subsession splits in the parent process.
   */
  afterSubsessionSplit() {
    // Scalars just got cleared due to a subsession split. We need to set the maximum
    // concurrent tab and window counts so that they reflect the correct value for the
    // new subsession.
    const counts = getOpenTabsAndWinsCounts();
    Services.telemetry.scalarSetMaximum(
      MAX_TAB_COUNT_SCALAR_NAME,
      counts.tabCount
    );
    Services.telemetry.scalarSetMaximum(
      MAX_WINDOW_COUNT_SCALAR_NAME,
      counts.winCount
    );

    // Reset the URI counter.
    URICountListener.reset();
  },

  QueryInterface: ChromeUtils.generateQI([
    "nsIObserver",
    "nsISupportsWeakReference",
  ]),

  uninit() {
    if (!this._inited) {
      return;
    }
    Services.obs.removeObserver(this, DOMWINDOW_OPENED_TOPIC);
    Services.obs.removeObserver(this, TELEMETRY_SUBSESSIONSPLIT_TOPIC);
  },

  observe(subject, topic, data) {
    switch (topic) {
      case DOMWINDOW_OPENED_TOPIC:
        this._onWindowOpen(subject);
        break;
      case TELEMETRY_SUBSESSIONSPLIT_TOPIC:
        this.afterSubsessionSplit();
        break;
      case "nsPref:changed":
        switch (data) {
          case "browser.tabs.extraDragSpace":
            this._recordWidgetChange(
              "drag-space",
              Services.prefs.getBoolPref("browser.tabs.extraDragSpace")
                ? "on"
                : "off",
              "pref"
            );
            break;
          case "browser.tabs.drawInTitlebar":
            this._recordWidgetChange(
              "titlebar",
              Services.prefs.getBoolPref("browser.tabs.drawInTitlebar")
                ? "off"
                : "on",
              "pref"
            );
            break;
        }
        break;
    }
  },

  handleEvent(event) {
    switch (event.type) {
      case "TabOpen":
        this._onTabOpen(getOpenTabsAndWinsCounts());
        break;
      case "TabPinned":
        this._onTabPinned();
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

        const { loadedTabCount } = getOpenTabsAndWinsCounts();
        this._recordTabCounts({ loadedTabCount });
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
   * @param {tabbrowser} tabbrowser
   *        The tabbrowser where the search was loaded.
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
   * @param {Boolean} [details.isFormHistory=false]
   *        true if this event was generated by a form history result.
   * @param {String} [details.alias=null]
   *        The search engine alias used in the search, if any.
   * @param {Object} [details.type=null]
   *        The object describing the event that triggered the search.
   * @throws if source is not in the known sources list.
   */
  recordSearch(tabbrowser, engine, source, details = {}) {
    if (!shouldRecordSearchCount(tabbrowser)) {
      return;
    }

    const countIdPrefix = `${engine.telemetryId}.`;
    const countIdSource = countIdPrefix + source;
    let histogram = Services.telemetry.getKeyedHistogramById("SEARCH_COUNTS");

    if (details.isOneOff) {
      if (!KNOWN_ONEOFF_SOURCES.includes(source)) {
        // Silently drop the error if this bogus call
        // came from 'urlbar' or 'searchbar'. They're
        // calling |recordSearch| twice from two different
        // code paths because they want to record the search
        // in SEARCH_COUNTS.
        if (["urlbar", "searchbar"].includes(source)) {
          histogram.add(countIdSource);
          return;
        }
        throw new Error("Unknown source for one-off search: " + source);
      }
    } else {
      if (!KNOWN_SEARCH_SOURCES.includes(source)) {
        throw new Error("Unknown source for search: " + source);
      }
      if (
        details.alias &&
        engine.isAppProvided &&
        engine.aliases.includes(details.alias)
      ) {
        // This is a keyword search using an AppProvided engine.
        // Record the source as "alias", not "urlbar".
        histogram.add(countIdPrefix + "alias");
      } else {
        histogram.add(countIdSource);
      }
    }

    // Dispatch the search signal to other handlers.
    this._handleSearchAction(engine, source, details);
  },

  _recordSearch(engine, url, source, action = null) {
    PartnerLinkAttribution.makeSearchEngineRequest(engine, url).catch(
      Cu.reportError
    );

    let scalarKey = action ? "search_" + action : "search";
    Services.telemetry.keyedScalarAdd(
      "browser.engagement.navigation." + source,
      scalarKey,
      1
    );
    Services.telemetry.recordEvent("navigation", "search", source, action, {
      engine: engine.telemetryId,
    });
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
        this._recordSearch(engine, details.url, "about_home", "enter");
        break;
      case "newtab":
        this._recordSearch(engine, details.url, "about_newtab", "enter");
        break;
      case "contextmenu":
      case "system":
      case "webextension":
        this._recordSearch(engine, details.url, source);
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
      source === "unknown" ? "searchbar" : source.replace("oneoff-", "");

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
      this._recordSearch(engine, details.url, sourceName, "oneoff");
      return;
    }

    // The search was not a one-off. It was a search with the default search engine.
    if (details.isFormHistory) {
      // It came from a form history result.
      this._recordSearch(engine, details.url, sourceName, "formhistory");
      return;
    } else if (details.isSuggestion) {
      // It came from a suggested search, so count it as such.
      this._recordSearch(engine, details.url, sourceName, "suggestion");
      return;
    } else if (details.alias) {
      // This one came from a search that used an alias.
      this._recordSearch(engine, details.url, sourceName, "alias");
      return;
    }

    // The search signal was generated by typing something and pressing enter.
    this._recordSearch(engine, details.url, sourceName, "enter");
  },

  /**
   * Records the method by which the user selected a result from the urlbar.
   *
   * @param {Event} event
   *        The event that triggered the selection.
   * @param {number} index
   *        The index that the user chose in the popup, or -1 if there wasn't a
   *        selection.
   * @param {string} userSelectionBehavior
   *        How the user cycled through results before picking the current match.
   *        Could be one of "tab", "arrow" or "none".
   */
  recordUrlbarSelectedResultMethod(
    event,
    index,
    userSelectionBehavior = "none"
  ) {
    this._recordUrlOrSearchbarSelectedResultMethod(
      event,
      index,
      "FX_URLBAR_SELECTED_RESULT_METHOD",
      userSelectionBehavior
    );
  },

  /**
   * Records the method by which the user selected a searchbar result.
   *
   * @param {Event} event
   *        The event that triggered the selection.
   * @param {number} highlightedIndex
   *        The index that the user chose in the popup, or -1 if there wasn't a
   *        selection.
   */
  recordSearchbarSelectedResultMethod(event, highlightedIndex) {
    this._recordUrlOrSearchbarSelectedResultMethod(
      event,
      highlightedIndex,
      "FX_SEARCHBAR_SELECTED_RESULT_METHOD",
      "none"
    );
  },

  _recordUrlOrSearchbarSelectedResultMethod(
    event,
    highlightedIndex,
    histogramID,
    userSelectionBehavior
  ) {
    // If the contents of the histogram are changed then
    // `UrlbarTestUtils.SELECTED_RESULT_METHODS` should also be updated.

    let histogram = Services.telemetry.getHistogramById(histogramID);
    // command events are from the one-off context menu.  Treat them as clicks.
    // Note that we don't care about MouseEvent subclasses here, since
    // those are not clicks.
    let isClick =
      event &&
      (ChromeUtils.getClassName(event) == "MouseEvent" ||
        event.type == "command");
    let category;
    if (isClick) {
      category = "click";
    } else if (highlightedIndex >= 0) {
      switch (userSelectionBehavior) {
        case "tab":
          category = "tabEnterSelection";
          break;
        case "arrow":
          category = "arrowEnterSelection";
          break;
        case "rightClick":
          // Selected by right mouse button.
          category = "rightClickEnter";
          break;
        default:
          category = "enterSelection";
      }
    } else {
      category = "enter";
    }
    histogram.add(category);
  },

  /**
   * This gets called shortly after the SessionStore has finished restoring
   * windows and tabs. It counts the open tabs and adds listeners to all the
   * windows.
   */
  _setupAfterRestore() {
    // Make sure to catch new chrome windows and subsession splits.
    Services.obs.addObserver(this, DOMWINDOW_OPENED_TOPIC, true);
    Services.obs.addObserver(this, TELEMETRY_SUBSESSIONSPLIT_TOPIC, true);

    // Attach the tabopen handlers to the existing Windows.
    for (let win of Services.wm.getEnumerator("navigator:browser")) {
      this._registerWindow(win);
    }

    // Get the initial tab and windows max counts.
    const counts = getOpenTabsAndWinsCounts();
    Services.telemetry.scalarSetMaximum(
      MAX_TAB_COUNT_SCALAR_NAME,
      counts.tabCount
    );
    Services.telemetry.scalarSetMaximum(
      MAX_WINDOW_COUNT_SCALAR_NAME,
      counts.winCount
    );
  },

  _buildWidgetPositions() {
    let widgetMap = new Map();

    const toolbarState = nodeId => {
      let value = Services.xulStore.getValue(
        AppConstants.BROWSER_CHROME_URL,
        nodeId,
        "collapsed"
      );
      if (value) {
        return value == "true" ? "off" : "on";
      }
      return "off";
    };

    widgetMap.set(
      BROWSER_UI_CONTAINER_IDS.PersonalToolbar,
      toolbarState("PersonalToolbar")
    );

    let menuBarHidden =
      Services.xulStore.getValue(
        AppConstants.BROWSER_CHROME_URL,
        "toolbar-menubar",
        "autohide"
      ) != "false";

    widgetMap.set("menu-toolbar", menuBarHidden ? "off" : "on");

    widgetMap.set(
      "drag-space",
      Services.prefs.getBoolPref("browser.tabs.extraDragSpace") ? "on" : "off"
    );

    // Drawing in the titlebar means not showing the titlebar, hence the negation.
    widgetMap.set(
      "titlebar",
      Services.prefs.getBoolPref("browser.tabs.drawInTitlebar", true)
        ? "off"
        : "on"
    );

    for (let area of CustomizableUI.areas) {
      if (!(area in BROWSER_UI_CONTAINER_IDS)) {
        continue;
      }

      let position = BROWSER_UI_CONTAINER_IDS[area];
      if (area == "nav-bar") {
        position = `${BROWSER_UI_CONTAINER_IDS[area]}-start`;
      }

      let widgets = CustomizableUI.getWidgetsInArea(area);

      for (let widget of widgets) {
        if (!widget) {
          continue;
        }

        if (widget.id.startsWith("customizableui-special-")) {
          continue;
        }

        if (area == "nav-bar" && widget.id == "urlbar-container") {
          position = `${BROWSER_UI_CONTAINER_IDS[area]}-end`;
          continue;
        }

        widgetMap.set(widget.id, position);
      }
    }

    let actions = PageActions.actions;
    for (let action of actions) {
      if (action.pinnedToUrlbar) {
        widgetMap.set(action.id, "pageaction-urlbar");
      }
    }

    return widgetMap;
  },

  _getWidgetID(node) {
    // We want to find a sensible ID for this element.
    if (!node) {
      return null;
    }

    // See if this is a customizable widget.
    if (node.ownerDocument.URL == AppConstants.BROWSER_CHROME_URL) {
      // First find if it is inside one of the customizable areas.
      for (let area of CustomizableUI.areas) {
        if (node.closest(`#${CSS.escape(area)}`)) {
          for (let widget of CustomizableUI.getWidgetIdsInArea(area)) {
            if (
              // We care about the buttons on the tabs themselves.
              widget == "tabbrowser-tabs" ||
              // We care about the page action and other buttons in here.
              widget == "urlbar-container" ||
              // We care about the actual menu items.
              widget == "menubar-items" ||
              // We care about individual bookmarks here.
              widget == "personal-bookmarks"
            ) {
              continue;
            }

            if (node.closest(`#${CSS.escape(widget)}`)) {
              return widget;
            }
          }
          break;
        }
      }
    }

    if (node.id) {
      return node.id;
    }

    // A couple of special cases in the tabs.
    for (let cls of ["bookmark-item", "tab-icon-sound", "tab-close-button"]) {
      if (node.classList.contains(cls)) {
        return cls;
      }
    }

    // One of these will at least let us know what the widget is for.
    let possibleAttributes = [
      "preference",
      "command",
      "observes",
      "data-l10n-id",
    ];

    // The key attribute on key elements is the actual key to listen for.
    if (node.localName != "key") {
      possibleAttributes.unshift("key");
    }

    for (let idAttribute of possibleAttributes) {
      if (node.hasAttribute(idAttribute)) {
        return node.getAttribute(idAttribute);
      }
    }

    return this._getWidgetID(node.parentElement);
  },

  _getWidgetContainer(node) {
    if (node.localName == "key") {
      return "keyboard";
    }

    if (node.ownerDocument.URL == AppConstants.BROWSER_CHROME_URL) {
      // Find the container holding this element.
      for (let containerId of Object.keys(BROWSER_UI_CONTAINER_IDS)) {
        let container = node.ownerDocument.getElementById(containerId);
        if (container && container.contains(node)) {
          return BROWSER_UI_CONTAINER_IDS[containerId];
        }
      }
    } else if (node.ownerDocument.URL.startsWith("about:preferences")) {
      // Find the element's category.
      let container = node.closest("[data-category]");
      if (!container) {
        return null;
      }

      let pane = container.getAttribute("data-category");

      if (!PREFERENCES_PANES.includes(pane)) {
        pane = "paneUnknown";
      }

      return `preferences_${pane}`;
    }

    return null;
  },

  lastClickTarget: null,

  ignoreEvent(event) {
    IGNORABLE_EVENTS.set(event, true);
  },

  _recordCommand(event) {
    if (IGNORABLE_EVENTS.get(event)) {
      return;
    }

    let types = [event.type];
    let sourceEvent = event;
    while (sourceEvent.sourceEvent) {
      sourceEvent = sourceEvent.sourceEvent;
      types.push(sourceEvent.type);
    }

    let lastTarget = this.lastClickTarget?.get();
    if (
      lastTarget &&
      sourceEvent.type == "command" &&
      sourceEvent.target.contains(lastTarget)
    ) {
      // Ignore a command event triggered by a click.
      this.lastClickTarget = null;
      return;
    }

    this.lastClickTarget = null;

    if (sourceEvent.type == "click") {
      // Only care about main button clicks.
      if (sourceEvent.button != 0) {
        return;
      }

      // This click may trigger a command event so retain the target to be able
      // to dedupe that event.
      this.lastClickTarget = Cu.getWeakReference(sourceEvent.target);
    }

    // We should never see events from web content as they are fired in a
    // content process, but let's be safe.
    let url = sourceEvent.target.ownerDocument.documentURIObject;
    if (!url.schemeIs("chrome") && !url.schemeIs("about")) {
      return;
    }

    // This is what events targetted  at content will actually look like.
    if (sourceEvent.target.localName == "browser") {
      return;
    }

    // Find the actual element we're interested in.
    let node = sourceEvent.target;
    while (!UI_TARGET_ELEMENTS.includes(node.localName)) {
      node = node.parentNode;
      if (!node) {
        // A click on a space or label or something we're not interested in.
        return;
      }
    }

    let item = this._getWidgetID(node);
    let source = this._getWidgetContainer(node);

    if (item && source) {
      let scalar = `browser.ui.interaction.${source.replace("-", "_")}`;
      Services.telemetry.keyedScalarAdd(scalar, telemetryId(item), 1);
    }
  },

  /**
   * Listens for UI interactions in the window.
   */
  _addUsageListeners(win) {
    // Listen for command events from the UI.
    win.addEventListener("command", event => this._recordCommand(event), true);
    win.addEventListener("click", event => this._recordCommand(event), true);
  },

  /**
   * A public version of the private method to take care of the `nav-bar-start`,
   * `nav-bar-end` thing that callers shouldn't have to care about. It also
   * accepts the DOM ids for the areas rather than the cleaner ones we report
   * to telemetry.
   */
  recordWidgetChange(widgetId, newPos, reason) {
    try {
      if (newPos) {
        newPos = BROWSER_UI_CONTAINER_IDS[newPos];
      }

      if (newPos == "nav-bar") {
        let { position } = CustomizableUI.getPlacementOfWidget(widgetId);
        let { position: urlPosition } = CustomizableUI.getPlacementOfWidget(
          "urlbar-container"
        );
        newPos = newPos + (urlPosition > position ? "-start" : "-end");
      }

      this._recordWidgetChange(widgetId, newPos, reason);
    } catch (e) {
      console.error(e);
    }
  },

  recordToolbarVisibility(toolbarId, newState, reason) {
    this._recordWidgetChange(
      BROWSER_UI_CONTAINER_IDS[toolbarId],
      newState ? "on" : "off",
      reason
    );
  },

  _recordWidgetChange(widgetId, newPos, reason) {
    // In some cases (like when add-ons are detected during startup) this gets
    // called before we've reported the initial positions. Ignore such cases.
    if (!this.widgetMap) {
      return;
    }

    if (widgetId == "urlbar-container") {
      // We don't report the position of the url bar, it is after nav-bar-start
      // and before nav-bar-end. But moving it means the widgets around it have
      // effectively moved so update those.
      let position = "nav-bar-start";
      let widgets = CustomizableUI.getWidgetsInArea("nav-bar");

      for (let widget of widgets) {
        if (!widget) {
          continue;
        }

        if (widget.id.startsWith("customizableui-special-")) {
          continue;
        }

        if (widget.id == "urlbar-container") {
          position = "nav-bar-end";
          continue;
        }

        // This will do nothing if the position hasn't changed.
        this._recordWidgetChange(widget.id, position, reason);
      }

      return;
    }

    let oldPos = this.widgetMap.get(widgetId);
    if (oldPos == newPos) {
      return;
    }

    let action = "move";

    if (!oldPos) {
      action = "add";
    } else if (!newPos) {
      action = "remove";
    }

    let key = `${telemetryId(widgetId, false)}_${action}_${oldPos ??
      "na"}_${newPos ?? "na"}_${reason}`;
    Services.telemetry.keyedScalarAdd("browser.ui.customized_widgets", key, 1);

    if (newPos) {
      this.widgetMap.set(widgetId, newPos);
    } else {
      this.widgetMap.delete(widgetId);
    }
  },

  _recordUITelemetry() {
    this.widgetMap = this._buildWidgetPositions();

    for (let [widgetId, position] of this.widgetMap.entries()) {
      let key = `${telemetryId(widgetId, false)}_pinned_${position}`;
      Services.telemetry.keyedScalarSet(
        "browser.ui.toolbar_widgets",
        key,
        true
      );
    }
  },

  /**
   * Adds listeners to a single chrome window.
   */
  _registerWindow(win) {
    this._addUsageListeners(win);

    win.addEventListener("unload", this);
    win.addEventListener("TabOpen", this, true);
    win.addEventListener("TabPinned", this, true);

    win.gBrowser.tabContainer.addEventListener(TAB_RESTORING_TOPIC, this);
    win.gBrowser.addTabsProgressListener(URICountListener);
  },

  /**
   * Removes listeners from a single chrome window.
   */
  _unregisterWindow(win) {
    win.removeEventListener("unload", this);
    win.removeEventListener("TabOpen", this, true);
    win.removeEventListener("TabPinned", this, true);

    win.defaultView.gBrowser.tabContainer.removeEventListener(
      TAB_RESTORING_TOPIC,
      this
    );
    win.defaultView.gBrowser.removeTabsProgressListener(URICountListener);
  },

  /**
   * Updates the tab counts.
   * @param {Object} [counts] The counts returned by `getOpenTabsAndWindowCounts`.
   */
  _onTabOpen({ tabCount, loadedTabCount }) {
    // Update the "tab opened" count and its maximum.
    Services.telemetry.scalarAdd(TAB_OPEN_EVENT_COUNT_SCALAR_NAME, 1);
    Services.telemetry.scalarSetMaximum(MAX_TAB_COUNT_SCALAR_NAME, tabCount);

    this._recordTabCounts({ tabCount, loadedTabCount });
  },

  _onTabPinned(target) {
    const pinnedTabs = getPinnedTabsCount();

    // Update the "tab pinned" count and its maximum.
    Services.telemetry.scalarAdd(TAB_PINNED_EVENT_COUNT_SCALAR_NAME, 1);
    Services.telemetry.scalarSetMaximum(
      MAX_TAB_PINNED_COUNT_SCALAR_NAME,
      pinnedTabs
    );
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
      win.removeEventListener("load", onLoad);

      // Ignore non browser windows.
      if (
        win.document.documentElement.getAttribute("windowtype") !=
        "navigator:browser"
      ) {
        return;
      }

      this._registerWindow(win);
      // Track the window open event and check the maximum.
      const counts = getOpenTabsAndWinsCounts();
      Services.telemetry.scalarAdd(WINDOW_OPEN_EVENT_COUNT_SCALAR_NAME, 1);
      Services.telemetry.scalarSetMaximum(
        MAX_WINDOW_COUNT_SCALAR_NAME,
        counts.winCount
      );

      // We won't receive the "TabOpen" event for the first tab within a new window.
      // Account for that.
      this._onTabOpen(counts);
    };
    win.addEventListener("load", onLoad);
  },

  /**
   * Record telemetry about the given tab counts.
   *
   * Telemetry for each count will only be recorded if the value isn't
   * `undefined`.
   *
   * @param {object} [counts] The tab counts to register with telemetry.
   * @param {number} [counts.tabCount] The number of tabs in all browsers.
   * @param {number} [counts.loadedTabCount] The number of loaded (i.e., not
   *                                         pending) tabs in all browsers.
   */
  _recordTabCounts({ tabCount, loadedTabCount }) {
    let currentTime = Date.now();
    if (
      tabCount !== undefined &&
      currentTime > this._lastRecordTabCount + MINIMUM_TAB_COUNT_INTERVAL_MS
    ) {
      Services.telemetry.getHistogramById("TAB_COUNT").add(tabCount);
      this._lastRecordTabCount = currentTime;
    }

    if (
      loadedTabCount !== undefined &&
      currentTime >
        this._lastRecordLoadedTabCount + MINIMUM_TAB_COUNT_INTERVAL_MS
    ) {
      Services.telemetry
        .getHistogramById("LOADED_TAB_COUNT")
        .add(loadedTabCount);
      this._lastRecordLoadedTabCount = currentTime;
    }
  },

  _checkProfileCountFileSchema(fileData) {
    // Verifies that the schema of the file is the expected schema
    if (typeof fileData.version != "string") {
      throw new Error("Schema Mismatch Error: Bad type for 'version' field");
    }
    if (!Array.isArray(fileData.profileTelemetryIds)) {
      throw new Error(
        "Schema Mismatch Error: Bad type for 'profileTelemetryIds' field"
      );
    }
    for (let profileTelemetryId of fileData.profileTelemetryIds) {
      if (typeof profileTelemetryId != "string") {
        throw new Error(
          "Schema Mismatch Error: Bad type for an element of 'profileTelemetryIds'"
        );
      }
    }
  },

  // Reports the number of Firefox profiles on this machine to telemetry.
  async reportProfileCount() {
    if (AppConstants.platform != "win") {
      // This is currently a windows-only feature.
      return;
    }

    // To report only as much data as we need, we will bucket our values.
    // Rather than the raw value, we will report the greatest value in the list
    // below that is no larger than the raw value.
    const buckets = [1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 100, 1000, 10000];

    // We need both the C:\ProgramData\Mozilla directory and the install
    // directory hash to create the profile count file path. We can easily
    // reassemble this from the update directory, which looks like:
    // C:\ProgramData\Mozilla\updates\hash
    // Retrieving the directory this way also ensures that the "Mozilla"
    // directory is created with the correct permissions.
    // The ProgramData directory, by default, grants write permissions only to
    // file creators. The directory service calls GetCommonUpdateDirectory,
    // which makes sure the the directory is created with user-writable
    // permissions.
    const updateDirectory = BrowserUsageTelemetry.Policy.getUpdateDirectory();
    const hash = updateDirectory.leafName;
    const profileCountFilename = "profile_count_" + hash + ".json";
    let profileCountFile = updateDirectory.parent.parent;
    profileCountFile.append(profileCountFilename);

    let readError = false;
    let fileData;
    try {
      let json = await BrowserUsageTelemetry.Policy.readProfileCountFile(
        profileCountFile.path
      );
      fileData = JSON.parse(json);
      BrowserUsageTelemetry._checkProfileCountFileSchema(fileData);
    } catch (ex) {
      // Note that since this also catches the "no such file" error, this is
      // always the template that we use when writing to the file for the first
      // time.
      fileData = { version: "1", profileTelemetryIds: [] };
      if (!(ex instanceof OS.File.Error && ex.becauseNoSuchFile)) {
        Cu.reportError(ex);
        // Don't just return here on a read error. We need to send the error
        // value to telemetry and we want to attempt to fix the file.
        // However, we will still report an error for this ping, even if we
        // fix the file. This is to prevent always sending a profile count of 1
        // if, for some reason, we always get a read error but never a write
        // error.
        readError = true;
      }
    }

    let writeError = false;
    let currentTelemetryId = await BrowserUsageTelemetry.Policy.getTelemetryClientId();
    // Don't add our telemetry ID to the file if we've already reached the
    // largest bucket. This prevents the file size from growing forever.
    if (
      !fileData.profileTelemetryIds.includes(currentTelemetryId) &&
      fileData.profileTelemetryIds.length < Math.max(...buckets)
    ) {
      fileData.profileTelemetryIds.push(currentTelemetryId);
      try {
        await BrowserUsageTelemetry.Policy.writeProfileCountFile(
          profileCountFile.path,
          JSON.stringify(fileData)
        );
      } catch (ex) {
        Cu.reportError(ex);
        writeError = true;
      }
    }

    // Determine the bucketed value to report
    let rawProfileCount = fileData.profileTelemetryIds.length;
    let valueToReport = 0;
    for (let bucket of buckets) {
      if (bucket <= rawProfileCount && bucket > valueToReport) {
        valueToReport = bucket;
      }
    }

    if (readError || writeError) {
      // We convey errors via a profile count of 0.
      valueToReport = 0;
    }

    Services.telemetry.scalarSet(
      "browser.engagement.profile_count",
      valueToReport
    );
  },

  /**
   * Record telemetry about the ratio of number of site origins per number of
   * loaded tabs.
   *
   * This will only record the telemetry if it has been five minutes since the
   * last recording.
   */
  _recordSiteOriginsPerLoadedTabs() {
    const currentTime = Date.now();
    if (
      currentTime >
      this._lastRecordSiteOriginsPerLoadedTabs + MINIMUM_TAB_COUNT_INTERVAL_MS
    ) {
      this._lastRecordSiteOriginsPerLoadedTabs = currentTime;
      // If this is the first load, we discard it because it is likely just the
      // browser opening for the first time.
      if (this._lastRecordSiteOriginsPerLoadedTabs === 0) {
        return;
      }

      const { loadedTabCount } = getOpenTabsAndWinsCounts();
      const siteOrigins = BrowserUtils.computeSiteOriginCount(
        Services.wm.getEnumerator("navigator:browser"),
        false
      );
      const histogramId = this._getSiteOriginHistogram(loadedTabCount);
      // Telemetry doesn't support float values.
      Services.telemetry
        .getHistogramById(histogramId)
        .add(Math.trunc((100 * siteOrigins) / loadedTabCount));
    }
  },

  _siteOriginHistogramIds: [
    [1, 1, "FX_NUMBER_OF_UNIQUE_SITE_ORIGINS_PER_LOADED_TABS_1"],
    [2, 4, "FX_NUMBER_OF_UNIQUE_SITE_ORIGINS_PER_LOADED_TABS_2_4"],
    [5, 9, "FX_NUMBER_OF_UNIQUE_SITE_ORIGINS_PER_LOADED_TABS_5_9"],
    [10, 14, "FX_NUMBER_OF_UNIQUE_SITE_ORIGINS_PER_LOADED_TABS_10_14"],
    [15, 19, "FX_NUMBER_OF_UNIQUE_SITE_ORIGINS_PER_LOADED_TABS_15_19"],
    [20, 24, "FX_NUMBER_OF_UNIQUE_SITE_ORIGINS_PER_LOADED_TABS_20_24"],
    [25, 29, "FX_NUMBER_OF_UNIQUE_SITE_ORIGINS_PER_LOADED_TABS_25_29"],
    [31, 34, "FX_NUMBER_OF_UNIQUE_SITE_ORIGINS_PER_LOADED_TABS_30_34"],
    [35, 39, "FX_NUMBER_OF_UNIQUE_SITE_ORIGINS_PER_LOADED_TABS_35_39"],
    [40, 44, "FX_NUMBER_OF_UNIQUE_SITE_ORIGINS_PER_LOADED_TABS_40_44"],
    [45, 49, "FX_NUMBER_OF_UNIQUE_SITE_ORIGINS_PER_LOADED_TABS_45_49"],
  ],

  /**
   * Return the appropriate histogram ID for the given loaded tab count.
   *
   * Unique site origin telemetry is split across several histograms so that it
   * can approximate a unique site origin vs loaded tab count curve.
   *
   * @param {number} [loadedTabCount] The number of loaded tabs.
   */
  _getSiteOriginHistogram(loadedTabCount) {
    for (const [min, max, histogramId] of this._siteOriginHistogramIds) {
      if (min <= loadedTabCount && loadedTabCount <= max) {
        return histogramId;
      }
    }
    return "FX_NUMBER_OF_UNIQUE_SITE_ORIGINS_PER_LOADED_TABS_50_PLUS";
  },
};

// Used by nsIBrowserUsage
function getUniqueDomainsVisitedInPast24Hours() {
  return URICountListener.uniqueDomainsVisitedInPast24Hours;
}
