// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

"use strict";

this.EXPORTED_SYMBOLS = ["UITour"];

const {classes: Cc, interfaces: Ci, utils: Cu, results: Cr} = Components;

Cu.import("resource://gre/modules/AppConstants.jsm");
Cu.import("resource://gre/modules/Services.jsm");
Cu.import("resource://gre/modules/XPCOMUtils.jsm");
Cu.import("resource://gre/modules/Preferences.jsm");
Cu.import("resource://gre/modules/Promise.jsm");
Cu.import("resource:///modules/RecentWindow.jsm");
Cu.import("resource://gre/modules/Task.jsm");
Cu.import("resource://gre/modules/TelemetryController.jsm");
Cu.import("resource://gre/modules/Timer.jsm");

Cu.importGlobalProperties(["URL"]);

XPCOMUtils.defineLazyModuleGetter(this, "LightweightThemeManager",
  "resource://gre/modules/LightweightThemeManager.jsm");
XPCOMUtils.defineLazyModuleGetter(this, "ResetProfile",
  "resource://gre/modules/ResetProfile.jsm");
XPCOMUtils.defineLazyModuleGetter(this, "CustomizableUI",
  "resource:///modules/CustomizableUI.jsm");
XPCOMUtils.defineLazyModuleGetter(this, "UITelemetry",
  "resource://gre/modules/UITelemetry.jsm");
XPCOMUtils.defineLazyModuleGetter(this, "BrowserUITelemetry",
  "resource:///modules/BrowserUITelemetry.jsm");
XPCOMUtils.defineLazyModuleGetter(this, "PrivateBrowsingUtils",
  "resource://gre/modules/PrivateBrowsingUtils.jsm");
XPCOMUtils.defineLazyModuleGetter(this, "ReaderParent",
  "resource:///modules/ReaderParent.jsm");

// See LOG_LEVELS in Console.jsm. Common examples: "All", "Info", "Warn", & "Error".
const PREF_LOG_LEVEL      = "browser.uitour.loglevel";
const PREF_SEENPAGEIDS    = "browser.uitour.seenPageIDs";
const PREF_SURVEY_DURATION = "browser.uitour.surveyDuration";

const BACKGROUND_PAGE_ACTIONS_ALLOWED = new Set([
  "forceShowReaderIcon",
  "getConfiguration",
  "getTreatmentTag",
  "hideHighlight",
  "hideInfo",
  "hideMenu",
  "ping",
  "registerPageID",
  "setConfiguration",
  "setTreatmentTag",
]);
const MAX_BUTTONS         = 4;

const BUCKET_NAME         = "UITour";
const BUCKET_TIMESTEPS    = [
  1 * 60 * 1000, // Until 1 minute after tab is closed/inactive.
  3 * 60 * 1000, // Until 3 minutes after tab is closed/inactive.
  10 * 60 * 1000, // Until 10 minutes after tab is closed/inactive.
  60 * 60 * 1000, // Until 1 hour after tab is closed/inactive.
];

// Time after which seen Page IDs expire.
const SEENPAGEID_EXPIRY  = 8 * 7 * 24 * 60 * 60 * 1000; // 8 weeks.

// Prefix for any target matching a search engine.
const TARGET_SEARCHENGINE_PREFIX = "searchEngine-";

// Create a new instance of the ConsoleAPI so we can control the maxLogLevel with a pref.
XPCOMUtils.defineLazyGetter(this, "log", () => {
  let ConsoleAPI = Cu.import("resource://gre/modules/Console.jsm", {}).ConsoleAPI;
  let consoleOptions = {
    maxLogLevelPref: PREF_LOG_LEVEL,
    prefix: "UITour",
  };
  return new ConsoleAPI(consoleOptions);
});

this.UITour = {
  url: null,
  seenPageIDs: null,
  // This map is not persisted and is used for
  // building the content source of a potential tour.
  pageIDsForSession: new Map(),
  pageIDSourceBrowsers: new WeakMap(),
  /* Map from browser chrome windows to a Set of <browser>s in which a tour is open (both visible and hidden) */
  tourBrowsersByWindow: new WeakMap(),
  appMenuOpenForAnnotation: new Set(),
  availableTargetsCache: new WeakMap(),
  clearAvailableTargetsCache() {
    this.availableTargetsCache = new WeakMap();
  },

  _annotationPanelMutationObservers: new WeakMap(),

  highlightEffects: ["random", "wobble", "zoom", "color"],
  targets: new Map([
    ["accountStatus", {
      query: (aDocument) => {
        // If the user is logged in, use the avatar element.
        let fxAFooter = aDocument.getElementById("PanelUI-footer-fxa");
        if (fxAFooter.getAttribute("fxastatus")) {
          return aDocument.getElementById("PanelUI-fxa-avatar");
        }

        // Otherwise use the sync setup icon.
        let statusButton = aDocument.getElementById("PanelUI-fxa-label");
        return aDocument.getAnonymousElementByAttribute(statusButton,
                                                        "class",
                                                        "toolbarbutton-icon");
      },
      // This is a fake widgetName starting with the "PanelUI-" prefix so we know
      // to automatically open the appMenu when annotating this target.
      widgetName: "PanelUI-fxa-label",
    }],
    ["addons",      {query: "#add-ons-button"}],
    ["appMenu",     {
      addTargetListener: (aDocument, aCallback) => {
        let panelPopup = aDocument.getElementById("PanelUI-popup");
        panelPopup.addEventListener("popupshown", aCallback);
      },
      query: "#PanelUI-button",
      removeTargetListener: (aDocument, aCallback) => {
        let panelPopup = aDocument.getElementById("PanelUI-popup");
        panelPopup.removeEventListener("popupshown", aCallback);
      },
    }],
    ["backForward", {
      query: "#back-button",
      widgetName: "urlbar-container",
    }],
    ["bookmarks",   {query: "#bookmarks-menu-button"}],
    ["controlCenter-trackingUnblock", controlCenterTrackingToggleTarget(true)],
    ["controlCenter-trackingBlock", controlCenterTrackingToggleTarget(false)],
    ["customize",   {
      query: (aDocument) => {
        let customizeButton = aDocument.getElementById("PanelUI-customize");
        return aDocument.getAnonymousElementByAttribute(customizeButton,
                                                        "class",
                                                        "toolbarbutton-icon");
      },
      widgetName: "PanelUI-customize",
    }],
    ["devtools",    {query: "#developer-button"}],
    ["help",        {query: "#PanelUI-help"}],
    ["home",        {query: "#home-button"}],
    ["forget", {
      allowAdd: true,
      query: "#panic-button",
      widgetName: "panic-button",
    }],
    ["pocket", {
      allowAdd: true,
      query: "#pocket-button",
      widgetName: "pocket-button",
    }],
    ["privateWindow",  {query: "#privatebrowsing-button"}],
    ["quit",        {query: "#PanelUI-quit"}],
    ["readerMode-urlBar", {query: "#reader-mode-button"}],
    ["search",      {
      infoPanelOffsetX: 18,
      infoPanelPosition: "after_start",
      query: "#searchbar",
      widgetName: "search-container",
    }],
    ["searchIcon", {
      query: (aDocument) => {
        let searchbar = aDocument.getElementById("searchbar");
        return aDocument.getAnonymousElementByAttribute(searchbar,
                                                        "anonid",
                                                        "searchbar-search-button");
      },
      widgetName: "search-container",
    }],
    ["searchPrefsLink", {
      query: (aDocument) => {
        let element = null;
        let popup = aDocument.getElementById("PopupSearchAutoComplete");
        if (popup.state != "open")
          return null;
        element = aDocument.getAnonymousElementByAttribute(popup,
                                                           "anonid",
                                                           "search-settings");
        if (!element || !UITour.isElementVisible(element)) {
          return null;
        }
        return element;
      },
    }],
    ["selectedTabIcon", {
      query: (aDocument) => {
        let selectedtab = aDocument.defaultView.gBrowser.selectedTab;
        let element = aDocument.getAnonymousElementByAttribute(selectedtab,
                                                               "anonid",
                                                               "tab-icon-image");
        if (!element || !UITour.isElementVisible(element)) {
          return null;
        }
        return element;
      },
    }],
    ["trackingProtection", {
      query: "#tracking-protection-icon",
    }],
    ["urlbar",      {
      query: "#urlbar",
      widgetName: "urlbar-container",
    }],
  ]),

  init() {
    log.debug("Initializing UITour");
    // Lazy getter is initialized here so it can be replicated any time
    // in a test.
    delete this.seenPageIDs;
    Object.defineProperty(this, "seenPageIDs", {
      get: this.restoreSeenPageIDs.bind(this),
      configurable: true,
    });

    delete this.url;
    XPCOMUtils.defineLazyGetter(this, "url", function() {
      return Services.urlFormatter.formatURLPref("browser.uitour.url");
    });

    // Clear the availableTargetsCache on widget changes.
    let listenerMethods = [
      "onWidgetAdded",
      "onWidgetMoved",
      "onWidgetRemoved",
      "onWidgetReset",
      "onAreaReset",
    ];
    CustomizableUI.addListener(listenerMethods.reduce((listener, method) => {
      listener[method] = () => this.clearAvailableTargetsCache();
      return listener;
    }, {}));
  },

  restoreSeenPageIDs() {
    delete this.seenPageIDs;

    if (UITelemetry.enabled) {
      let dateThreshold = Date.now() - SEENPAGEID_EXPIRY;

      try {
        let data = Services.prefs.getCharPref(PREF_SEENPAGEIDS);
        data = new Map(JSON.parse(data));

        for (let [pageID, details] of data) {

          if (typeof pageID != "string" ||
              typeof details != "object" ||
              typeof details.lastSeen != "number" ||
              details.lastSeen < dateThreshold) {

            data.delete(pageID);
          }
        }

        this.seenPageIDs = data;
      } catch (e) {}
    }

    if (!this.seenPageIDs)
      this.seenPageIDs = new Map();

    this.persistSeenIDs();

    return this.seenPageIDs;
  },

  addSeenPageID(aPageID) {
    if (!UITelemetry.enabled)
      return;

    this.seenPageIDs.set(aPageID, {
      lastSeen: Date.now(),
    });

    this.persistSeenIDs();
  },

  persistSeenIDs() {
    if (this.seenPageIDs.size === 0) {
      Services.prefs.clearUserPref(PREF_SEENPAGEIDS);
      return;
    }

    Services.prefs.setCharPref(PREF_SEENPAGEIDS,
                               JSON.stringify([...this.seenPageIDs]));
  },

  onPageEvent(aMessage, aEvent) {
    let browser = aMessage.target;
    let window = browser.ownerGlobal;

    // Does the window have tabs? We need to make sure since windowless browsers do
    // not have tabs.
    if (!window.gBrowser) {
      // When using windowless browsers we don't have a valid |window|. If that's the case,
      // use the most recent window as a target for UITour functions (see Bug 1111022).
      window = Services.wm.getMostRecentWindow("navigator:browser");
    }

    let messageManager = browser.messageManager;

    log.debug("onPageEvent:", aEvent.detail, aMessage);

    if (typeof aEvent.detail != "object") {
      log.warn("Malformed event - detail not an object");
      return false;
    }

    let action = aEvent.detail.action;
    if (typeof action != "string" || !action) {
      log.warn("Action not defined");
      return false;
    }

    let data = aEvent.detail.data;
    if (typeof data != "object") {
      log.warn("Malformed event - data not an object");
      return false;
    }

    if ((aEvent.pageVisibilityState == "hidden" ||
         aEvent.pageVisibilityState == "unloaded") &&
        !BACKGROUND_PAGE_ACTIONS_ALLOWED.has(action)) {
      log.warn("Ignoring disallowed action from a hidden page:", action);
      return false;
    }

    switch (action) {
      case "registerPageID": {
        if (typeof data.pageID != "string") {
          log.warn("registerPageID: pageID must be a string");
          break;
        }

        this.pageIDsForSession.set(data.pageID, {lastSeen: Date.now()});

        // The rest is only relevant if Telemetry is enabled.
        if (!UITelemetry.enabled) {
          log.debug("registerPageID: Telemetry disabled, not doing anything");
          break;
        }

        // We don't want to allow BrowserUITelemetry.BUCKET_SEPARATOR in the
        // pageID, as it could make parsing the telemetry bucket name difficult.
        if (data.pageID.includes(BrowserUITelemetry.BUCKET_SEPARATOR)) {
          log.warn("registerPageID: Invalid page ID specified");
          break;
        }

        this.addSeenPageID(data.pageID);
        this.pageIDSourceBrowsers.set(browser, data.pageID);
        this.setTelemetryBucket(data.pageID);

        break;
      }

      case "showHeartbeat": {
        // Validate the input parameters.
        if (typeof data.message !== "string" || data.message === "") {
          log.error("showHeartbeat: Invalid message specified.");
          return false;
        }

        if (typeof data.thankyouMessage !== "string" || data.thankyouMessage === "") {
          log.error("showHeartbeat: Invalid thank you message specified.");
          return false;
        }

        if (typeof data.flowId !== "string" || data.flowId === "") {
          log.error("showHeartbeat: Invalid flowId specified.");
          return false;
        }

        if (data.engagementButtonLabel && typeof data.engagementButtonLabel != "string") {
          log.error("showHeartbeat: Invalid engagementButtonLabel specified");
          return false;
        }

        let heartbeatWindow = window;
        if (data.privateWindowsOnly && !PrivateBrowsingUtils.isWindowPrivate(heartbeatWindow)) {
          heartbeatWindow = RecentWindow.getMostRecentBrowserWindow({ private: true });
          if (!heartbeatWindow) {
            log.debug("showHeartbeat: No private window found");
            return false;
          }
        }

        // Finally show the Heartbeat UI.
        this.showHeartbeat(heartbeatWindow, data);
        break;
      }

      case "showHighlight": {
        let targetPromise = this.getTarget(window, data.target);
        targetPromise.then(target => {
          if (!target.node) {
            log.error("UITour: Target could not be resolved: " + data.target);
            return;
          }
          let effect = undefined;
          if (this.highlightEffects.indexOf(data.effect) !== -1) {
            effect = data.effect;
          }
          this.showHighlight(window, target, effect);
        }).catch(log.error);
        break;
      }

      case "hideHighlight": {
        this.hideHighlight(window);
        break;
      }

      case "showInfo": {
        let targetPromise = this.getTarget(window, data.target, true);
        targetPromise.then(target => {
          if (!target.node) {
            log.error("UITour: Target could not be resolved: " + data.target);
            return;
          }

          let iconURL = null;
          if (typeof data.icon == "string")
            iconURL = this.resolveURL(browser, data.icon);

          let buttons = [];
          if (Array.isArray(data.buttons) && data.buttons.length > 0) {
            for (let buttonData of data.buttons) {
              if (typeof buttonData == "object" &&
                  typeof buttonData.label == "string" &&
                  typeof buttonData.callbackID == "string") {
                let callback = buttonData.callbackID;
                let button = {
                  label: buttonData.label,
                  callback: event => {
                    this.sendPageCallback(messageManager, callback);
                  },
                };

                if (typeof buttonData.icon == "string")
                  button.iconURL = this.resolveURL(browser, buttonData.icon);

                if (typeof buttonData.style == "string")
                  button.style = buttonData.style;

                buttons.push(button);

                if (buttons.length == MAX_BUTTONS) {
                  log.warn("showInfo: Reached limit of allowed number of buttons");
                  break;
                }
              }
            }
          }

          let infoOptions = {};
          if (typeof data.closeButtonCallbackID == "string") {
            infoOptions.closeButtonCallback = () => {
              this.sendPageCallback(messageManager, data.closeButtonCallbackID);
            };
          }
          if (typeof data.targetCallbackID == "string") {
            infoOptions.targetCallback = details => {
              this.sendPageCallback(messageManager, data.targetCallbackID, details);
            };
          }

          this.showInfo(window, target, data.title, data.text, iconURL, buttons, infoOptions);
        }).catch(log.error);
        break;
      }

      case "hideInfo": {
        this.hideInfo(window);
        break;
      }

      case "previewTheme": {
        this.previewTheme(data.theme);
        break;
      }

      case "resetTheme": {
        this.resetTheme();
        break;
      }

      case "showMenu": {
        this.showMenu(window, data.name, () => {
          if (typeof data.showCallbackID == "string")
            this.sendPageCallback(messageManager, data.showCallbackID);
        });
        break;
      }

      case "hideMenu": {
        this.hideMenu(window, data.name);
        break;
      }

      case "showNewTab": {
        this.showNewTab(window, browser);
        break;
      }

      case "getConfiguration": {
        if (typeof data.configuration != "string") {
          log.warn("getConfiguration: No configuration option specified");
          return false;
        }

        this.getConfiguration(messageManager, window, data.configuration, data.callbackID);
        break;
      }

      case "setConfiguration": {
        if (typeof data.configuration != "string") {
          log.warn("setConfiguration: No configuration option specified");
          return false;
        }

        this.setConfiguration(window, data.configuration, data.value);
        break;
      }

      case "openPreferences": {
        if (typeof data.pane != "string" && typeof data.pane != "undefined") {
          log.warn("openPreferences: Invalid pane specified");
          return false;
        }

        window.openPreferences(data.pane, { origin: "UITour" });
        break;
      }

      case "showFirefoxAccounts": {
        // 'signup' is the only action that makes sense currently, so we don't
        // accept arbitrary actions just to be safe...
        let p = new URLSearchParams("action=signup&entrypoint=uitour");
        // Call our helper to validate extraURLCampaignParams and populate URLSearchParams
        if (!this._populateCampaignParams(p, data.extraURLCampaignParams)) {
          log.warn("showFirefoxAccounts: invalid campaign args specified");
          return false;
        }

        // We want to replace the current tab.
        browser.loadURI("about:accounts?" + p.toString());
        break;
      }

      case "resetFirefox": {
        // Open a reset profile dialog window.
        if (ResetProfile.resetSupported()) {
          ResetProfile.openConfirmationDialog(window);
        }
        break;
      }

      case "addNavBarWidget": {
        // Add a widget to the toolbar
        let targetPromise = this.getTarget(window, data.name);
        targetPromise.then(target => {
          this.addNavBarWidget(target, messageManager, data.callbackID);
        }).catch(log.error);
        break;
      }

      case "setDefaultSearchEngine": {
        let enginePromise = this.selectSearchEngine(data.identifier);
        enginePromise.catch(Cu.reportError);
        break;
      }

      case "setTreatmentTag": {
        let name = data.name;
        let value = data.value;
        Services.prefs.setStringPref("browser.uitour.treatment." + name, value);
        // The notification is only meant to be used in tests.
        UITourHealthReport.recordTreatmentTag(name, value)
                          .then(() => this.notify("TreatmentTag:TelemetrySent"));
        break;
      }

      case "getTreatmentTag": {
        let name = data.name;
        let value;
        try {
          value = Services.prefs.getStringPref("browser.uitour.treatment." + name);
        } catch (ex) {}
        this.sendPageCallback(messageManager, data.callbackID, { value });
        break;
      }

      case "setSearchTerm": {
        let targetPromise = this.getTarget(window, "search");
        targetPromise.then(target => {
          let searchbar = target.node;
          searchbar.value = data.term;
          searchbar.updateGoButtonVisibility();
        });
        break;
      }

      case "openSearchPanel": {
        let targetPromise = this.getTarget(window, "search");
        targetPromise.then(target => {
          let searchbar = target.node;

          if (searchbar.textbox.open) {
            this.sendPageCallback(messageManager, data.callbackID);
          } else {
            let onPopupShown = () => {
              searchbar.textbox.popup.removeEventListener("popupshown", onPopupShown);
              this.sendPageCallback(messageManager, data.callbackID);
            };

            searchbar.textbox.popup.addEventListener("popupshown", onPopupShown);
            searchbar.openSuggestionsPanel();
          }
        }).then(null, Cu.reportError);
        break;
      }

      case "ping": {
        if (typeof data.callbackID == "string")
          this.sendPageCallback(messageManager, data.callbackID);
        break;
      }

      case "forceShowReaderIcon": {
        ReaderParent.forceShowReaderIcon(browser);
        break;
      }

      case "toggleReaderMode": {
        let targetPromise = this.getTarget(window, "readerMode-urlBar");
        targetPromise.then(target => {
          ReaderParent.toggleReaderMode({target: target.node});
        });
        break;
      }

      case "closeTab": {
        // Find the <tabbrowser> element of the <browser> for which the event
        // was generated originally. If the browser where the UI tour is loaded
        // is windowless, just ignore the request to close the tab. The request
        // is also ignored if this is the only tab in the window.
        let tabBrowser = browser.ownerGlobal.gBrowser;
        if (tabBrowser && tabBrowser.browsers.length > 1) {
          tabBrowser.removeTab(tabBrowser.getTabForBrowser(browser));
        }
        break;
      }
    }

    // For performance reasons, only call initForBrowser if we did something
    // that will require a teardownTourForBrowser call later.
    // getConfiguration (called from about:home) doesn't require any future
    // uninitialization.
    if (action != "getConfiguration")
      this.initForBrowser(browser, window);

    return true;
  },

  initForBrowser(aBrowser, window) {
    let gBrowser = window.gBrowser;

    if (gBrowser) {
        gBrowser.tabContainer.addEventListener("TabSelect", this);
    }

    if (!this.tourBrowsersByWindow.has(window)) {
      this.tourBrowsersByWindow.set(window, new Set());
    }
    this.tourBrowsersByWindow.get(window).add(aBrowser);

    Services.obs.addObserver(this, "message-manager-close");

    window.addEventListener("SSWindowClosing", this);
  },

  handleEvent(aEvent) {
    log.debug("handleEvent: type =", aEvent.type, "event =", aEvent);
    switch (aEvent.type) {
      case "TabSelect": {
        let window = aEvent.target.ownerGlobal;

        // Teardown the browser of the tab we just switched away from.
        if (aEvent.detail && aEvent.detail.previousTab) {
          let previousTab = aEvent.detail.previousTab;
          let openTourWindows = this.tourBrowsersByWindow.get(window);
          if (openTourWindows.has(previousTab.linkedBrowser)) {
            this.teardownTourForBrowser(window, previousTab.linkedBrowser, false);
          }
        }

        break;
      }

      case "SSWindowClosing": {
        let window = aEvent.target;
        this.teardownTourForWindow(window);
        break;
      }
    }
  },

  observe(aSubject, aTopic, aData) {
    log.debug("observe: aTopic =", aTopic);
    switch (aTopic) {
      // The browser message manager is disconnected when the <browser> is
      // destroyed and we want to teardown at that point.
      case "message-manager-close": {
        let winEnum = Services.wm.getEnumerator("navigator:browser");
        while (winEnum.hasMoreElements()) {
          let window = winEnum.getNext();
          if (window.closed)
            continue;

          let tourBrowsers = this.tourBrowsersByWindow.get(window);
          if (!tourBrowsers)
            continue;

          for (let browser of tourBrowsers) {
            let messageManager = browser.messageManager;
            if (aSubject != messageManager) {
              continue;
            }

            this.teardownTourForBrowser(window, browser, true);
            return;
          }
        }
        break;
      }
    }
  },

  // Given a string that is a JSONified represenation of an object with
  // additional utm_* URL params that should be appended, validate and append
  // them to the passed URLSearchParams object. Returns true if the params
  // were validated and appended, and false if the request should be ignored.
  _populateCampaignParams(urlSearchParams, extraURLCampaignParams) {
    // We are extra paranoid about what params we allow to be appended.
    if (typeof extraURLCampaignParams == "undefined") {
      // no params, so it's all good.
      return true;
    }
    if (typeof extraURLCampaignParams != "string") {
      log.warn("_populateCampaignParams: extraURLCampaignParams is not a string");
      return false;
    }
    let campaignParams;
    try {
      if (extraURLCampaignParams) {
        campaignParams = JSON.parse(extraURLCampaignParams);
        if (typeof campaignParams != "object") {
          log.warn("_populateCampaignParams: extraURLCampaignParams is not a stringified object");
          return false;
        }
      }
    } catch (ex) {
      log.warn("_populateCampaignParams: extraURLCampaignParams is not a JSON object");
      return false;
    }
    if (campaignParams) {
      // The regex that the name of each param must match - there's no
      // character restriction on the value - they will be escaped as necessary.
      let reSimpleString = /^[-_a-zA-Z0-9]*$/;
      for (let name in campaignParams) {
        let value = campaignParams[name];
        if (typeof name != "string" || typeof value != "string" ||
            !name.startsWith("utm_") ||
            value.length == 0 ||
            !reSimpleString.test(name)) {
          log.warn("_populateCampaignParams: invalid campaign param specified");
          return false;
        }
        urlSearchParams.append(name, value);
      }
    }
    return true;
  },

  setTelemetryBucket(aPageID) {
    let bucket = BUCKET_NAME + BrowserUITelemetry.BUCKET_SEPARATOR + aPageID;
    BrowserUITelemetry.setBucket(bucket);
  },

  setExpiringTelemetryBucket(aPageID, aType) {
    let bucket = BUCKET_NAME + BrowserUITelemetry.BUCKET_SEPARATOR + aPageID +
                 BrowserUITelemetry.BUCKET_SEPARATOR + aType;

    BrowserUITelemetry.setExpiringBucket(bucket,
                                         BUCKET_TIMESTEPS);
  },

  // This is registered with UITelemetry by BrowserUITelemetry, so that UITour
  // can remain lazy-loaded on-demand.
  getTelemetry() {
    return {
      seenPageIDs: [...this.seenPageIDs.keys()],
    };
  },

  /**
   * Tear down a tour from a tab e.g. upon switching/closing tabs.
   */
  teardownTourForBrowser(aWindow, aBrowser, aTourPageClosing = false) {
    log.debug("teardownTourForBrowser: aBrowser = ", aBrowser, aTourPageClosing);

    if (this.pageIDSourceBrowsers.has(aBrowser)) {
      let pageID = this.pageIDSourceBrowsers.get(aBrowser);
      this.setExpiringTelemetryBucket(pageID, aTourPageClosing ? "closed" : "inactive");
    }

    let openTourBrowsers = this.tourBrowsersByWindow.get(aWindow);
    if (aTourPageClosing && openTourBrowsers) {
      openTourBrowsers.delete(aBrowser);
    }

    this.hideHighlight(aWindow);
    this.hideInfo(aWindow);
    // Ensure the menu panel is hidden before calling recreatePopup so popup events occur.
    this.hideMenu(aWindow, "appMenu");
    this.hideMenu(aWindow, "controlCenter");

    // Clean up panel listeners after calling hideMenu above.
    aWindow.PanelUI.panel.removeEventListener("popuphiding", this.hideAppMenuAnnotations);
    aWindow.PanelUI.panel.removeEventListener("ViewShowing", this.hideAppMenuAnnotations);
    aWindow.PanelUI.panel.removeEventListener("popuphidden", this.onPanelHidden);
    let controlCenterPanel = aWindow.gIdentityHandler._identityPopup;
    controlCenterPanel.removeEventListener("popuphidden", this.onPanelHidden);
    controlCenterPanel.removeEventListener("popuphiding", this.hideControlCenterAnnotations);

    this.resetTheme();

    // If there are no more tour tabs left in the window, teardown the tour for the whole window.
    if (!openTourBrowsers || openTourBrowsers.size == 0) {
      this.teardownTourForWindow(aWindow);
    }
  },

  /**
   * Tear down all tours for a ChromeWindow.
   */
  teardownTourForWindow(aWindow) {
    log.debug("teardownTourForWindow");
    aWindow.gBrowser.tabContainer.removeEventListener("TabSelect", this);
    aWindow.removeEventListener("SSWindowClosing", this);

    let openTourBrowsers = this.tourBrowsersByWindow.get(aWindow);
    if (openTourBrowsers) {
      for (let browser of openTourBrowsers) {
        if (this.pageIDSourceBrowsers.has(browser)) {
          let pageID = this.pageIDSourceBrowsers.get(browser);
          this.setExpiringTelemetryBucket(pageID, "closed");
        }
      }
    }

    this.tourBrowsersByWindow.delete(aWindow);
  },

  // This function is copied to UITourListener.
  isSafeScheme(aURI) {
    let allowedSchemes = new Set(["https", "about"]);
    if (!Services.prefs.getBoolPref("browser.uitour.requireSecure"))
      allowedSchemes.add("http");

    if (!allowedSchemes.has(aURI.scheme)) {
      log.error("Unsafe scheme:", aURI.scheme);
      return false;
    }

    return true;
  },

  resolveURL(aBrowser, aURL) {
    try {
      let uri = Services.io.newURI(aURL, null, aBrowser.currentURI);

      if (!this.isSafeScheme(uri))
        return null;

      return uri.spec;
    } catch (e) {}

    return null;
  },

  sendPageCallback(aMessageManager, aCallbackID, aData = {}) {
    let detail = {data: aData, callbackID: aCallbackID};
    log.debug("sendPageCallback", detail);
    aMessageManager.sendAsyncMessage("UITour:SendPageCallback", detail);
  },

  isElementVisible(aElement) {
    let targetStyle = aElement.ownerGlobal.getComputedStyle(aElement);
    return !aElement.ownerDocument.hidden &&
             targetStyle.display != "none" &&
             targetStyle.visibility == "visible";
  },

  getTarget(aWindow, aTargetName, aSticky = false) {
    log.debug("getTarget:", aTargetName);
    let deferred = Promise.defer();
    if (typeof aTargetName != "string" || !aTargetName) {
      log.warn("getTarget: Invalid target name specified");
      deferred.reject("Invalid target name specified");
      return deferred.promise;
    }

    let targetObject = this.targets.get(aTargetName);
    if (!targetObject) {
      log.warn("getTarget: The specified target name is not in the allowed set");
      deferred.reject("The specified target name is not in the allowed set");
      return deferred.promise;
    }

    let targetQuery = targetObject.query;
    aWindow.PanelUI.ensureReady().then(() => {
      let node;
      if (typeof targetQuery == "function") {
        try {
          node = targetQuery(aWindow.document);
        } catch (ex) {
          log.warn("getTarget: Error running target query:", ex);
          node = null;
        }
      } else {
        node = aWindow.document.querySelector(targetQuery);
      }

      deferred.resolve({
        addTargetListener: targetObject.addTargetListener,
        infoPanelOffsetX: targetObject.infoPanelOffsetX,
        infoPanelOffsetY: targetObject.infoPanelOffsetY,
        infoPanelPosition: targetObject.infoPanelPosition,
        node,
        removeTargetListener: targetObject.removeTargetListener,
        targetName: aTargetName,
        widgetName: targetObject.widgetName,
        allowAdd: targetObject.allowAdd,
      });
    }).catch(log.error);
    return deferred.promise;
  },

  targetIsInAppMenu(aTarget) {
    let placement = CustomizableUI.getPlacementOfWidget(aTarget.widgetName || aTarget.node.id);
    if (placement && placement.area == CustomizableUI.AREA_PANEL) {
      return true;
    }

    let targetElement = aTarget.node;
    // Use the widget for filtering if it exists since the target may be the icon inside.
    if (aTarget.widgetName) {
      targetElement = aTarget.node.ownerDocument.getElementById(aTarget.widgetName);
    }

    // Handle the non-customizable buttons at the bottom of the menu which aren't proper widgets.
    return targetElement.id.startsWith("PanelUI-")
             && targetElement.id != "PanelUI-button";
  },

  /**
   * Called before opening or after closing a highlight or info panel to see if
   * we need to open or close the appMenu to see the annotation's anchor.
   */
  _setAppMenuStateForAnnotation(aWindow, aAnnotationType, aShouldOpenForHighlight, aCallback = null) {
    log.debug("_setAppMenuStateForAnnotation:", aAnnotationType);
    log.debug("_setAppMenuStateForAnnotation: Menu is expected to be:", aShouldOpenForHighlight ? "open" : "closed");

    // If the panel is in the desired state, we're done.
    let panelIsOpen = aWindow.PanelUI.panel.state != "closed";
    if (aShouldOpenForHighlight == panelIsOpen) {
      log.debug("_setAppMenuStateForAnnotation: Panel already in expected state");
      if (aCallback)
        aCallback();
      return;
    }

    // Don't close the menu if it wasn't opened by us (e.g. via showmenu instead).
    if (!aShouldOpenForHighlight && !this.appMenuOpenForAnnotation.has(aAnnotationType)) {
      log.debug("_setAppMenuStateForAnnotation: Menu not opened by us, not closing");
      if (aCallback)
        aCallback();
      return;
    }

    if (aShouldOpenForHighlight) {
      this.appMenuOpenForAnnotation.add(aAnnotationType);
    } else {
      this.appMenuOpenForAnnotation.delete(aAnnotationType);
    }

    // Actually show or hide the menu
    if (this.appMenuOpenForAnnotation.size) {
      log.debug("_setAppMenuStateForAnnotation: Opening the menu");
      this.showMenu(aWindow, "appMenu", aCallback);
    } else {
      log.debug("_setAppMenuStateForAnnotation: Closing the menu");
      this.hideMenu(aWindow, "appMenu");
      if (aCallback)
        aCallback();
    }

  },

  previewTheme(aTheme) {
    let origin = Services.prefs.getCharPref("browser.uitour.themeOrigin");
    let data = LightweightThemeManager.parseTheme(aTheme, origin);
    if (data)
      LightweightThemeManager.previewTheme(data);
  },

  resetTheme() {
    LightweightThemeManager.resetPreview();
  },

  /**
   * Show the Heartbeat UI to request user feedback. This function reports back to the
   * caller using |notify|. The notification event name reflects the current status the UI
   * is in (either "Heartbeat:NotificationOffered", "Heartbeat:NotificationClosed",
   * "Heartbeat:LearnMore", "Heartbeat:Engaged", "Heartbeat:Voted",
   * "Heartbeat:SurveyExpired" or "Heartbeat:WindowClosed").
   * When a "Heartbeat:Voted" event is notified
   * the data payload contains a |score| field which holds the rating picked by the user.
   * Please note that input parameters are already validated by the caller.
   *
   * @param aChromeWindow
   *        The chrome window that the heartbeat notification is displayed in.
   * @param {Object} aOptions Options object.
   * @param {String} aOptions.message
   *        The message, or question, to display on the notification.
   * @param {String} aOptions.thankyouMessage
   *        The thank you message to display after user votes.
   * @param {String} aOptions.flowId
   *        An identifier for this rating flow. Please note that this is only used to
   *        identify the notification box.
   * @param {String} [aOptions.engagementButtonLabel=null]
   *        The text of the engagement button to use instad of stars. If this is null
   *        or invalid, rating stars are used.
   * @param {String} [aOptions.engagementURL=null]
   *        The engagement URL to open in a new tab once user has engaged. If this is null
   *        or invalid, no new tab is opened.
   * @param {String} [aOptions.learnMoreLabel=null]
   *        The label of the learn more link. No link will be shown if this is null.
   * @param {String} [aOptions.learnMoreURL=null]
   *        The learn more URL to open when clicking on the learn more link. No learn more
   *        will be shown if this is an invalid URL.
   * @param {boolean} [aOptions.privateWindowsOnly=false]
   *        Whether the heartbeat UI should only be targeted at a private window (if one exists).
   *        No notifications should be fired when this is true.
   * @param {String} [aOptions.surveyId]
   *        An ID for the survey, reflected in the Telemetry ping.
   * @param {Number} [aOptions.surveyVersion]
   *        Survey's version number, reflected in the Telemetry ping.
   * @param {boolean} [aOptions.testing]
   *        Whether this is a test survey, reflected in the Telemetry ping.
   */
  showHeartbeat(aChromeWindow, aOptions) {
    // Initialize survey state
    let pingSent = false;
    let surveyResults = {};
    let surveyEndTimer = null;

    /**
     * Accumulates survey events and submits to Telemetry after the survey ends.
     *
     * @param {String} aEventName
     *        Heartbeat event name
     * @param {Object} aParams
     *        Additional parameters and their values
     */
    let maybeNotifyHeartbeat = (aEventName, aParams = {}) => {
      // Return if event occurred after the ping was sent
      if (pingSent) {
        log.warn("maybeNotifyHeartbeat: event occurred after ping sent:", aEventName, aParams);
        return;
      }

      // No Telemetry from private-window-only Heartbeats
      if (aOptions.privateWindowsOnly) {
        return;
      }

      let ts = Date.now();
      let sendPing = false;
      switch (aEventName) {
        case "Heartbeat:NotificationOffered":
          surveyResults.flowId = aOptions.flowId;
          surveyResults.offeredTS = ts;
          break;
        case "Heartbeat:LearnMore":
          // record only the first click
          if (!surveyResults.learnMoreTS) {
            surveyResults.learnMoreTS = ts;
          }
          break;
        case "Heartbeat:Engaged":
          surveyResults.engagedTS = ts;
          break;
        case "Heartbeat:Voted":
          surveyResults.votedTS = ts;
          surveyResults.score = aParams.score;
          break;
        case "Heartbeat:SurveyExpired":
          surveyResults.expiredTS = ts;
          break;
        case "Heartbeat:NotificationClosed":
          // this is the final event in most surveys
          surveyResults.closedTS = ts;
          sendPing = true;
          break;
        case "Heartbeat:WindowClosed":
          surveyResults.windowClosedTS = ts;
          sendPing = true;
          break;
        default:
          log.error("maybeNotifyHeartbeat: unrecognized event:", aEventName);
          break;
      }

      aParams.timestamp = ts;
      aParams.flowId = aOptions.flowId;
      this.notify(aEventName, aParams);

      if (!sendPing) {
        return;
      }

      // Send the ping to Telemetry
      let payload = Object.assign({}, surveyResults);
      payload.version = 1;
      for (let meta of ["surveyId", "surveyVersion", "testing"]) {
        if (aOptions.hasOwnProperty(meta)) {
          payload[meta] = aOptions[meta];
        }
      }

      log.debug("Sending payload to Telemetry: aEventName:", aEventName,
                "payload:", payload);

      TelemetryController.submitExternalPing("heartbeat", payload, {
        addClientId: true,
        addEnvironment: true,
      });

      // only for testing
      this.notify("Heartbeat:TelemetrySent", payload);

      // Survey is complete, clear out the expiry timer & survey configuration
      if (surveyEndTimer) {
        clearTimeout(surveyEndTimer);
        surveyEndTimer = null;
      }

      pingSent = true;
      surveyResults = {};
    };

    let nb = aChromeWindow.document.getElementById("high-priority-global-notificationbox");
    let buttons = null;

    if (aOptions.engagementButtonLabel) {
      buttons = [{
        label: aOptions.engagementButtonLabel,
        callback: () => {
          // Let the consumer know user engaged.
          maybeNotifyHeartbeat("Heartbeat:Engaged");

          userEngaged(new Map([
            ["type", "button"],
            ["flowid", aOptions.flowId]
          ]));

          // Return true so that the notification bar doesn't close itself since
          // we have a thank you message to show.
          return true;
        },
      }];
    }

    let defaultIcon = "chrome://browser/skin/heartbeat-icon.svg";
    let iconURL = defaultIcon;
    try {
      // Take the optional icon URL if specified
      if (aOptions.iconURL) {
        iconURL = new URL(aOptions.iconURL);
        // For now, only allow chrome URIs.
        if (iconURL.protocol != "chrome:") {
          iconURL = defaultIcon;
          throw new Error("Invalid protocol");
        }
      }
    } catch (error) {
      log.error("showHeartbeat: Invalid icon URL specified.");
    }

    // Create the notification. Prefix its ID to decrease the chances of collisions.
    let notice = nb.appendNotification(aOptions.message, "heartbeat-" + aOptions.flowId,
                                       iconURL,
                                       nb.PRIORITY_INFO_HIGH, buttons,
                                       (aEventType) => {
                                         if (aEventType != "removed") {
                                           return;
                                         }
                                         // Let the consumer know the notification bar was closed.
                                         // This also happens after voting.
                                         maybeNotifyHeartbeat("Heartbeat:NotificationClosed");
                                       });

    // Get the elements we need to style.
    let messageImage =
      aChromeWindow.document.getAnonymousElementByAttribute(notice, "anonid", "messageImage");
    let messageText =
      aChromeWindow.document.getAnonymousElementByAttribute(notice, "anonid", "messageText");

    function userEngaged(aEngagementParams) {
      // Make the heartbeat icon pulse twice.
      notice.label = aOptions.thankyouMessage;
      messageImage.classList.remove("pulse-onshow");
      messageImage.classList.add("pulse-twice");

      // Remove all the children of the notice (rating container
      // and the flex).
      while (notice.firstChild) {
        notice.firstChild.remove();
      }

      // Make sure that we have a valid URL. If we haven't, do not open the engagement page.
      let engagementURL = null;
      try {
        engagementURL = new URL(aOptions.engagementURL);
      } catch (error) {
        log.error("showHeartbeat: Invalid URL specified.");
      }

      // Just open the engagement tab if we have a valid engagement URL.
      if (engagementURL) {
        for (let [param, value] of aEngagementParams) {
          engagementURL.searchParams.append(param, value);
        }

        // Open the engagement URL in a new tab.
        aChromeWindow.gBrowser.selectedTab =
          aChromeWindow.gBrowser.addTab(engagementURL.toString(), {
            owner: aChromeWindow.gBrowser.selectedTab,
            relatedToCurrent: true
          });
      }

      // Remove the notification bar after 3 seconds.
      aChromeWindow.setTimeout(() => {
        nb.removeNotification(notice);
      }, 3000);
    }

    // Create the fragment holding the rating UI.
    let frag = aChromeWindow.document.createDocumentFragment();

    // Build the Heartbeat star rating.
    const numStars = aOptions.engagementButtonLabel ? 0 : 5;
    let ratingContainer = aChromeWindow.document.createElement("hbox");
    ratingContainer.id = "star-rating-container";

    for (let i = 0; i < numStars; i++) {
      // Create a star rating element.
      let ratingElement = aChromeWindow.document.createElement("toolbarbutton");

      // Style it.
      let starIndex = numStars - i;
      ratingElement.className = "plain star-x";
      ratingElement.id = "star" + starIndex;
      ratingElement.setAttribute("data-score", starIndex);

      // Add the click handler.
      ratingElement.addEventListener("click", function(evt) {
        let rating = Number(evt.target.getAttribute("data-score"), 10);

        // Let the consumer know user voted.
        maybeNotifyHeartbeat("Heartbeat:Voted", { score: rating });

        // Append the score data to the engagement URL.
        userEngaged(new Map([
          ["type", "stars"],
          ["score", rating],
          ["flowid", aOptions.flowId]
        ]));
      });

      // Add it to the container.
      ratingContainer.appendChild(ratingElement);
    }

    frag.appendChild(ratingContainer);

    // Make sure the stars are not pushed to the right by the spacer.
    let rightSpacer = aChromeWindow.document.createElement("spacer");
    rightSpacer.flex = 20;
    frag.appendChild(rightSpacer);

    messageText.flex = 0; // Collapse the space before the stars.
    let leftSpacer = messageText.nextSibling;
    leftSpacer.flex = 0;

    // Make sure that we have a valid learn more URL.
    let learnMoreURL = null;
    try {
      learnMoreURL = new URL(aOptions.learnMoreURL);
    } catch (error) {
      log.error("showHeartbeat: Invalid learnMore URL specified.");
    }

    // Add the learn more link.
    if (aOptions.learnMoreLabel && learnMoreURL) {
      let learnMore = aChromeWindow.document.createElement("label");
      learnMore.className = "text-link";
      learnMore.href = learnMoreURL.toString();
      learnMore.setAttribute("value", aOptions.learnMoreLabel);
      learnMore.addEventListener("click", () => maybeNotifyHeartbeat("Heartbeat:LearnMore"));
      frag.appendChild(learnMore);
    }

    // Append the fragment and apply the styling.
    notice.appendChild(frag);
    notice.classList.add("heartbeat");
    messageImage.classList.add("heartbeat", "pulse-onshow");
    messageText.classList.add("heartbeat");

    // Let the consumer know the notification was shown.
    maybeNotifyHeartbeat("Heartbeat:NotificationOffered");

    // End the survey if the user quits, closes the window, or
    // hasn't responded before expiration.
    if (!aOptions.privateWindowsOnly) {
      function handleWindowClosed(aTopic) {
        maybeNotifyHeartbeat("Heartbeat:WindowClosed");
        aChromeWindow.removeEventListener("SSWindowClosing", handleWindowClosed);
      }
      aChromeWindow.addEventListener("SSWindowClosing", handleWindowClosed);

      let surveyDuration = Services.prefs.getIntPref(PREF_SURVEY_DURATION) * 1000;
      surveyEndTimer = setTimeout(() => {
        maybeNotifyHeartbeat("Heartbeat:SurveyExpired");
        nb.removeNotification(notice);
      }, surveyDuration);
    }
  },

  /**
   * The node to which a highlight or notification(-popup) is anchored is sometimes
   * obscured because it may be inside an overflow menu. This function should figure
   * that out and offer the overflow chevron as an alternative.
   *
   * @param {Node} aAnchor The element that's supposed to be the anchor
   * @type {Node}
   */
  _correctAnchor(aAnchor) {
    // If the target is in the overflow panel, just return the overflow button.
    if (aAnchor.getAttribute("overflowedItem")) {
      let doc = aAnchor.ownerDocument;
      let placement = CustomizableUI.getPlacementOfWidget(aAnchor.id);
      let areaNode = doc.getElementById(placement.area);
      return areaNode.overflowable._chevron;
    }

    return aAnchor;
  },

  /**
   * @param aChromeWindow The chrome window that the highlight is in. Necessary since some targets
   *                      are in a sub-frame so the defaultView is not the same as the chrome
   *                      window.
   * @param aTarget    The element to highlight.
   * @param aEffect    (optional) The effect to use from UITour.highlightEffects or "none".
   * @see UITour.highlightEffects
   */
  showHighlight(aChromeWindow, aTarget, aEffect = "none") {
    function showHighlightPanel() {
      let highlighter = aChromeWindow.document.getElementById("UITourHighlight");

      let effect = aEffect;
      if (effect == "random") {
        // Exclude "random" from the randomly selected effects.
        let randomEffect = 1 + Math.floor(Math.random() * (this.highlightEffects.length - 1));
        if (randomEffect == this.highlightEffects.length)
          randomEffect--; // On the order of 1 in 2^62 chance of this happening.
        effect = this.highlightEffects[randomEffect];
      }
      // Toggle the effect attribute to "none" and flush layout before setting it so the effect plays.
      highlighter.setAttribute("active", "none");
      aChromeWindow.getComputedStyle(highlighter).animationName;
      highlighter.setAttribute("active", effect);
      highlighter.parentElement.setAttribute("targetName", aTarget.targetName);
      highlighter.parentElement.hidden = false;

      let highlightAnchor = this._correctAnchor(aTarget.node);
      let targetRect = highlightAnchor.getBoundingClientRect();
      let highlightHeight = targetRect.height;
      let highlightWidth = targetRect.width;
      let minDimension = Math.min(highlightHeight, highlightWidth);
      let maxDimension = Math.max(highlightHeight, highlightWidth);

      // If the dimensions are within 200% of each other (to include the bookmarks button),
      // make the highlight a circle with the largest dimension as the diameter.
      if (maxDimension / minDimension <= 3.0) {
        highlightHeight = highlightWidth = maxDimension;
        highlighter.style.borderRadius = "100%";
      } else {
        highlighter.style.borderRadius = "";
      }

      highlighter.style.height = highlightHeight + "px";
      highlighter.style.width = highlightWidth + "px";

      // Close a previous highlight so we can relocate the panel.
      if (highlighter.parentElement.state == "showing" || highlighter.parentElement.state == "open") {
        log.debug("showHighlight: Closing previous highlight first");
        highlighter.parentElement.hidePopup();
      }
      /* The "overlap" position anchors from the top-left but we want to centre highlights at their
         minimum size. */
      let highlightWindow = aChromeWindow;
      let containerStyle = highlightWindow.getComputedStyle(highlighter.parentElement);
      let paddingTopPx = 0 - parseFloat(containerStyle.paddingTop);
      let paddingLeftPx = 0 - parseFloat(containerStyle.paddingLeft);
      let highlightStyle = highlightWindow.getComputedStyle(highlighter);
      let highlightHeightWithMin = Math.max(highlightHeight, parseFloat(highlightStyle.minHeight));
      let highlightWidthWithMin = Math.max(highlightWidth, parseFloat(highlightStyle.minWidth));
      let offsetX = paddingTopPx
                      - (Math.max(0, highlightWidthWithMin - targetRect.width) / 2);
      let offsetY = paddingLeftPx
                      - (Math.max(0, highlightHeightWithMin - targetRect.height) / 2);
      this._addAnnotationPanelMutationObserver(highlighter.parentElement);
      highlighter.parentElement.openPopup(highlightAnchor, "overlap", offsetX, offsetY);
    }

    // Prevent showing a panel at an undefined position.
    if (!this.isElementVisible(aTarget.node)) {
      log.warn("showHighlight: Not showing a highlight since the target isn't visible", aTarget);
      return;
    }

    this._setAppMenuStateForAnnotation(aChromeWindow, "highlight",
                                       this.targetIsInAppMenu(aTarget),
                                       showHighlightPanel.bind(this));
  },

  hideHighlight(aWindow) {
    let highlighter = aWindow.document.getElementById("UITourHighlight");
    this._removeAnnotationPanelMutationObserver(highlighter.parentElement);
    highlighter.parentElement.hidePopup();
    highlighter.removeAttribute("active");

    this._setAppMenuStateForAnnotation(aWindow, "highlight", false);
  },

  /**
   * Show an info panel.
   *
   * @param {ChromeWindow} aChromeWindow
   * @param {Node}     aAnchor
   * @param {String}   [aTitle=""]
   * @param {String}   [aDescription=""]
   * @param {String}   [aIconURL=""]
   * @param {Object[]} [aButtons=[]]
   * @param {Object}   [aOptions={}]
   * @param {String}   [aOptions.closeButtonCallback]
   * @param {String}   [aOptions.targetCallback]
   */
  showInfo(aChromeWindow, aAnchor, aTitle = "", aDescription = "",
           aIconURL = "", aButtons = [], aOptions = {}) {
    function showInfoPanel(aAnchorEl) {
      aAnchorEl.focus();

      let document = aChromeWindow.document;
      let tooltip = document.getElementById("UITourTooltip");
      let tooltipTitle = document.getElementById("UITourTooltipTitle");
      let tooltipDesc = document.getElementById("UITourTooltipDescription");
      let tooltipIcon = document.getElementById("UITourTooltipIcon");
      let tooltipButtons = document.getElementById("UITourTooltipButtons");

      if (tooltip.state == "showing" || tooltip.state == "open") {
        tooltip.hidePopup();
      }

      tooltipTitle.textContent = aTitle || "";
      tooltipDesc.textContent = aDescription || "";
      tooltipIcon.src = aIconURL || "";
      tooltipIcon.hidden = !aIconURL;

      while (tooltipButtons.firstChild)
        tooltipButtons.firstChild.remove();

      for (let button of aButtons) {
        let isButton = button.style != "text";
        let el = document.createElement(isButton ? "button" : "label");
        el.setAttribute(isButton ? "label" : "value", button.label);

        if (isButton) {
          if (button.iconURL)
            el.setAttribute("image", button.iconURL);

          if (button.style == "link")
            el.setAttribute("class", "button-link");

          if (button.style == "primary")
            el.setAttribute("class", "button-primary");

          // Don't close the popup or call the callback for style=text as they
          // aren't links/buttons.
          let callback = button.callback;
          el.addEventListener("command", event => {
            tooltip.hidePopup();
            callback(event);
          });
        }

        tooltipButtons.appendChild(el);
      }

      tooltipButtons.hidden = !aButtons.length;

      let tooltipClose = document.getElementById("UITourTooltipClose");
      let closeButtonCallback = (event) => {
        this.hideInfo(document.defaultView);
        if (aOptions && aOptions.closeButtonCallback) {
          aOptions.closeButtonCallback();
        }
      };
      tooltipClose.addEventListener("command", closeButtonCallback);

      let targetCallback = (event) => {
        let details = {
          target: aAnchor.targetName,
          type: event.type,
        };
        aOptions.targetCallback(details);
      };
      if (aOptions.targetCallback && aAnchor.addTargetListener) {
        aAnchor.addTargetListener(document, targetCallback);
      }

      tooltip.addEventListener("popuphiding", function(event) {
        tooltipClose.removeEventListener("command", closeButtonCallback);
        if (aOptions.targetCallback && aAnchor.removeTargetListener) {
          aAnchor.removeTargetListener(document, targetCallback);
        }
      }, {once: true});

      tooltip.setAttribute("targetName", aAnchor.targetName);
      tooltip.hidden = false;
      let alignment = "bottomcenter topright";
      if (aAnchor.infoPanelPosition) {
        alignment = aAnchor.infoPanelPosition;
      }

      let { infoPanelOffsetX: xOffset, infoPanelOffsetY: yOffset } = aAnchor;

      this._addAnnotationPanelMutationObserver(tooltip);
      tooltip.openPopup(aAnchorEl, alignment, xOffset || 0, yOffset || 0);
      if (tooltip.state == "closed") {
        document.defaultView.addEventListener("endmodalstate", function() {
          tooltip.openPopup(aAnchorEl, alignment);
        }, {once: true});
      }
    }

    // Prevent showing a panel at an undefined position.
    if (!this.isElementVisible(aAnchor.node)) {
      log.warn("showInfo: Not showing since the target isn't visible", aAnchor);
      return;
    }

    this._setAppMenuStateForAnnotation(aChromeWindow, "info",
                                       this.targetIsInAppMenu(aAnchor),
                                       showInfoPanel.bind(this, this._correctAnchor(aAnchor.node)));
  },

  isInfoOnTarget(aChromeWindow, aTargetName) {
    let document = aChromeWindow.document;
    let tooltip = document.getElementById("UITourTooltip");
    return tooltip.getAttribute("targetName") == aTargetName && tooltip.state != "closed";
  },

  hideInfo(aWindow) {
    let document = aWindow.document;

    let tooltip = document.getElementById("UITourTooltip");
    this._removeAnnotationPanelMutationObserver(tooltip);
    tooltip.hidePopup();
    this._setAppMenuStateForAnnotation(aWindow, "info", false);

    let tooltipButtons = document.getElementById("UITourTooltipButtons");
    while (tooltipButtons.firstChild)
      tooltipButtons.firstChild.remove();
  },

  showMenu(aWindow, aMenuName, aOpenCallback = null) {
    log.debug("showMenu:", aMenuName);
    function openMenuButton(aMenuBtn) {
      if (!aMenuBtn || !aMenuBtn.boxObject || aMenuBtn.open) {
        if (aOpenCallback)
          aOpenCallback();
        return;
      }
      if (aOpenCallback)
        aMenuBtn.addEventListener("popupshown", onPopupShown);
      aMenuBtn.boxObject.openMenu(true);
    }
    function onPopupShown(event) {
      this.removeEventListener("popupshown", onPopupShown);
      aOpenCallback(event);
    }

    if (aMenuName == "appMenu") {
      aWindow.PanelUI.panel.setAttribute("noautohide", "true");
      // If the popup is already opened, don't recreate the widget as it may cause a flicker.
      if (aWindow.PanelUI.panel.state != "open") {
        this.recreatePopup(aWindow.PanelUI.panel);
      }
      aWindow.PanelUI.panel.addEventListener("popuphiding", this.hideAppMenuAnnotations);
      aWindow.PanelUI.panel.addEventListener("ViewShowing", this.hideAppMenuAnnotations);
      aWindow.PanelUI.panel.addEventListener("popuphidden", this.onPanelHidden);
      if (aOpenCallback) {
        aWindow.PanelUI.panel.addEventListener("popupshown", onPopupShown);
      }
      aWindow.PanelUI.show();
    } else if (aMenuName == "bookmarks") {
      let menuBtn = aWindow.document.getElementById("bookmarks-menu-button");
      openMenuButton(menuBtn);
    } else if (aMenuName == "controlCenter") {
      let popup = aWindow.gIdentityHandler._identityPopup;

      // Add the listener even if the panel is already open since it will still
      // only get registered once even if it was UITour that opened it.
      popup.addEventListener("popuphiding", this.hideControlCenterAnnotations);
      popup.addEventListener("popuphidden", this.onPanelHidden);

      popup.setAttribute("noautohide", true);
      this.clearAvailableTargetsCache();

      if (popup.state == "open") {
        if (aOpenCallback) {
          aOpenCallback();
        }
        return;
      }

      this.recreatePopup(popup);

      // Open the control center
      if (aOpenCallback) {
        popup.addEventListener("popupshown", onPopupShown);
      }
      aWindow.document.getElementById("identity-box").click();
    } else if (aMenuName == "pocket") {
      this.getTarget(aWindow, "pocket").then(Task.async(function* onPocketTarget(target) {
        let widgetGroupWrapper = CustomizableUI.getWidget(target.widgetName);
        if (widgetGroupWrapper.type != "view" || !widgetGroupWrapper.viewId) {
          log.error("Can't open the pocket menu without a view");
          return;
        }
        let placement = CustomizableUI.getPlacementOfWidget(target.widgetName);
        if (!placement || !placement.area) {
          log.error("Can't open the pocket menu without a placement");
          return;
        }

        if (placement.area == CustomizableUI.AREA_PANEL) {
          // Open the appMenu and wait for it if it's not already opened or showing a subview.
          yield new Promise((resolve, reject) => {
            if (aWindow.PanelUI.panel.state != "closed") {
              if (aWindow.PanelUI.multiView.showingSubView) {
                reject("A subview is already showing");
                return;
              }

              resolve();
              return;
            }

            aWindow.PanelUI.panel.addEventListener("popupshown", function() {
              resolve();
            }, {once: true});

            aWindow.PanelUI.show();
          });
        }

        let widgetWrapper = widgetGroupWrapper.forWindow(aWindow);
        aWindow.PanelUI.showSubView(widgetGroupWrapper.viewId,
                                    widgetWrapper.anchor,
                                    placement.area);
      })).catch(log.error);
    }
  },

  hideMenu(aWindow, aMenuName) {
    log.debug("hideMenu:", aMenuName);
    function closeMenuButton(aMenuBtn) {
      if (aMenuBtn && aMenuBtn.boxObject)
        aMenuBtn.boxObject.openMenu(false);
    }

    if (aMenuName == "appMenu") {
      aWindow.PanelUI.hide();
    } else if (aMenuName == "bookmarks") {
      let menuBtn = aWindow.document.getElementById("bookmarks-menu-button");
      closeMenuButton(menuBtn);
    } else if (aMenuName == "controlCenter") {
      let panel = aWindow.gIdentityHandler._identityPopup;
      panel.hidePopup();
    }
  },

  showNewTab(aWindow, aBrowser) {
    aWindow.openLinkIn("about:newtab", "current", {targetBrowser: aBrowser});
  },

  hideAnnotationsForPanel(aEvent, aTargetPositionCallback) {
    let win = aEvent.target.ownerGlobal;
    let annotationElements = new Map([
      // [annotationElement (panel), method to hide the annotation]
      [win.document.getElementById("UITourHighlightContainer"), UITour.hideHighlight.bind(UITour)],
      [win.document.getElementById("UITourTooltip"), UITour.hideInfo.bind(UITour)],
    ]);
    annotationElements.forEach((hideMethod, annotationElement) => {
      if (annotationElement.state != "closed") {
        let targetName = annotationElement.getAttribute("targetName");
        UITour.getTarget(win, targetName).then((aTarget) => {
          // Since getTarget is async, we need to make sure that the target hasn't
          // changed since it may have just moved to somewhere outside of the app menu.
          if (annotationElement.getAttribute("targetName") != aTarget.targetName ||
              annotationElement.state == "closed" ||
              !aTargetPositionCallback(aTarget)) {
            return;
          }
          hideMethod(win);
        }).catch(log.error);
      }
    });
    UITour.appMenuOpenForAnnotation.clear();
  },

  hideAppMenuAnnotations(aEvent) {
    UITour.hideAnnotationsForPanel(aEvent, UITour.targetIsInAppMenu);
  },

  hideControlCenterAnnotations(aEvent) {
    UITour.hideAnnotationsForPanel(aEvent, (aTarget) => {
      return aTarget.targetName.startsWith("controlCenter-");
    });
  },

  onPanelHidden(aEvent) {
    aEvent.target.removeAttribute("noautohide");
    UITour.recreatePopup(aEvent.target);
    UITour.clearAvailableTargetsCache();
  },

  recreatePopup(aPanel) {
    // After changing popup attributes that relate to how the native widget is created
    // (e.g. @noautohide) we need to re-create the frame/widget for it to take effect.
    if (aPanel.hidden) {
      // If the panel is already hidden, we don't need to recreate it but flush
      // in case someone just hid it.
      aPanel.clientWidth; // flush
      return;
    }
    aPanel.hidden = true;
    aPanel.clientWidth; // flush
    aPanel.hidden = false;
  },

  getConfiguration(aMessageManager, aWindow, aConfiguration, aCallbackID) {
    switch (aConfiguration) {
      case "appinfo":
        let props = ["defaultUpdateChannel", "version"];
        let appinfo = {};
        props.forEach(property => appinfo[property] = Services.appinfo[property]);

        // Identifier of the partner repack, as stored in preference "distribution.id"
        // and included in Firefox and other update pings. Note this is not the same as
        // Services.appinfo.distributionID (value of MOZ_DISTRIBUTION_ID is set at build time).
        let distribution =
          Services.prefs.getDefaultBranch("distribution.").getCharPref("id", "default");
        appinfo["distribution"] = distribution;

        let isDefaultBrowser = null;
        try {
          let shell = aWindow.getShellService();
          if (shell) {
            isDefaultBrowser = shell.isDefaultBrowser(false);
          }
        } catch (e) {}
        appinfo["defaultBrowser"] = isDefaultBrowser;

        let canSetDefaultBrowserInBackground = true;
        if (AppConstants.isPlatformAndVersionAtLeast("win", "6.2") ||
            AppConstants.isPlatformAndVersionAtLeast("macosx", "10.10")) {
          canSetDefaultBrowserInBackground = false;
        } else if (AppConstants.platform == "linux") {
          // The ShellService may not exist on some versions of Linux.
          try {
            aWindow.getShellService();
          } catch (e) {
            canSetDefaultBrowserInBackground = null;
          }
        }

        appinfo["canSetDefaultBrowserInBackground"] =
          canSetDefaultBrowserInBackground;

        this.sendPageCallback(aMessageManager, aCallbackID, appinfo);
        break;
      case "availableTargets":
        this.getAvailableTargets(aMessageManager, aWindow, aCallbackID);
        break;
      case "search":
      case "selectedSearchEngine":
        Services.search.init(rv => {
          let data;
          if (Components.isSuccessCode(rv)) {
            let engines = Services.search.getVisibleEngines();
            data = {
              searchEngineIdentifier: Services.search.defaultEngine.identifier,
              engines: engines.filter((engine) => engine.identifier)
                              .map((engine) => TARGET_SEARCHENGINE_PREFIX + engine.identifier)
            };
          } else {
            data = {engines: [], searchEngineIdentifier: ""};
          }
          this.sendPageCallback(aMessageManager, aCallbackID, data);
        });
        break;
      case "sync":
        this.sendPageCallback(aMessageManager, aCallbackID, {
          setup: Services.prefs.prefHasUserValue("services.sync.username"),
          desktopDevices: Preferences.get("services.sync.clients.devices.desktop", 0),
          mobileDevices: Preferences.get("services.sync.clients.devices.mobile", 0),
          totalDevices: Preferences.get("services.sync.numClients", 0),
        });
        break;
      case "canReset":
        this.sendPageCallback(aMessageManager, aCallbackID, ResetProfile.resetSupported());
        break;
      default:
        log.error("getConfiguration: Unknown configuration requested: " + aConfiguration);
        break;
    }
  },

  setConfiguration(aWindow, aConfiguration, aValue) {
    switch (aConfiguration) {
      case "defaultBrowser":
        // Ignore aValue in this case because the default browser can only
        // be set, not unset.
        try {
          let shell = aWindow.getShellService();
          if (shell) {
            shell.setDefaultBrowser(true, false);
          }
        } catch (e) {}
        break;
      default:
        log.error("setConfiguration: Unknown configuration requested: " + aConfiguration);
        break;
    }
  },

  getAvailableTargets(aMessageManager, aChromeWindow, aCallbackID) {
    Task.spawn(function*() {
      let window = aChromeWindow;
      let data = this.availableTargetsCache.get(window);
      if (data) {
        log.debug("getAvailableTargets: Using cached targets list", data.targets.join(","));
        this.sendPageCallback(aMessageManager, aCallbackID, data);
        return;
      }

      let promises = [];
      for (let targetName of this.targets.keys()) {
        promises.push(this.getTarget(window, targetName));
      }
      let targetObjects = yield Promise.all(promises);

      let targetNames = [];
      for (let targetObject of targetObjects) {
        if (targetObject.node)
          targetNames.push(targetObject.targetName);
      }

      data = {
        targets: targetNames,
      };
      this.availableTargetsCache.set(window, data);
      this.sendPageCallback(aMessageManager, aCallbackID, data);
    }.bind(this)).catch(err => {
      log.error(err);
      this.sendPageCallback(aMessageManager, aCallbackID, {
        targets: [],
      });
    });
  },

  addNavBarWidget(aTarget, aMessageManager, aCallbackID) {
    if (aTarget.node) {
      log.error("addNavBarWidget: can't add a widget already present:", aTarget);
      return;
    }
    if (!aTarget.allowAdd) {
      log.error("addNavBarWidget: not allowed to add this widget:", aTarget);
      return;
    }
    if (!aTarget.widgetName) {
      log.error("addNavBarWidget: can't add a widget without a widgetName property:", aTarget);
      return;
    }

    CustomizableUI.addWidgetToArea(aTarget.widgetName, CustomizableUI.AREA_NAVBAR);
    this.sendPageCallback(aMessageManager, aCallbackID);
  },

  _addAnnotationPanelMutationObserver(aPanelEl) {
    if (AppConstants.platform == "linux") {
      let observer = this._annotationPanelMutationObservers.get(aPanelEl);
      if (observer) {
        return;
      }
      let win = aPanelEl.ownerGlobal;
      observer = new win.MutationObserver(this._annotationMutationCallback);
      this._annotationPanelMutationObservers.set(aPanelEl, observer);
      let observerOptions = {
        attributeFilter: ["height", "width"],
        attributes: true,
      };
      observer.observe(aPanelEl, observerOptions);
    }
  },

  _removeAnnotationPanelMutationObserver(aPanelEl) {
    if (AppConstants.platform == "linux") {
      let observer = this._annotationPanelMutationObservers.get(aPanelEl);
      if (observer) {
        observer.disconnect();
        this._annotationPanelMutationObservers.delete(aPanelEl);
      }
    }
  },

/**
 * Workaround for Ubuntu panel craziness in bug 970788 where incorrect sizes get passed to
 * nsXULPopupManager::PopupResized and lead to incorrect width and height attributes getting
 * set on the panel.
 */
  _annotationMutationCallback(aMutations) {
    for (let mutation of aMutations) {
      // Remove both attributes at once and ignore remaining mutations to be proccessed.
      mutation.target.removeAttribute("width");
      mutation.target.removeAttribute("height");
      return;
    }
  },

  selectSearchEngine(aID) {
    return new Promise((resolve, reject) => {
      Services.search.init((rv) => {
        if (!Components.isSuccessCode(rv)) {
          reject("selectSearchEngine: search service init failed: " + rv);
          return;
        }

        let engines = Services.search.getVisibleEngines();
        for (let engine of engines) {
          if (engine.identifier == aID) {
            Services.search.defaultEngine = engine;
            resolve();
            return;
          }
        }
        reject("selectSearchEngine could not find engine with given ID");
      });
    });
  },

  notify(eventName, params) {
    let winEnum = Services.wm.getEnumerator("navigator:browser");
    while (winEnum.hasMoreElements()) {
      let window = winEnum.getNext();
      if (window.closed)
        continue;

      let openTourBrowsers = this.tourBrowsersByWindow.get(window);
      if (!openTourBrowsers)
        continue;

      for (let browser of openTourBrowsers) {
        let messageManager = browser.messageManager;
        if (!messageManager) {
          log.error("notify: Trying to notify a browser without a messageManager", browser);
          continue;
        }
        let detail = {
          event: eventName,
          params,
        };
        messageManager.sendAsyncMessage("UITour:SendPageNotification", detail);
      }
    }
  },
};

function controlCenterTrackingToggleTarget(aUnblock) {
  return {
    infoPanelPosition: "rightcenter topleft",
    query(aDocument) {
      let popup = aDocument.defaultView.gIdentityHandler._identityPopup;
      if (popup.state != "open") {
        return null;
      }
      let buttonId = null;
      if (aUnblock) {
        if (PrivateBrowsingUtils.isWindowPrivate(aDocument.defaultView)) {
          buttonId = "tracking-action-unblock-private";
        } else {
          buttonId = "tracking-action-unblock";
        }
      } else {
        buttonId = "tracking-action-block";
      }
      let element = aDocument.getElementById(buttonId);
      return UITour.isElementVisible(element) ? element : null;
    },
  };
}

this.UITour.init();

/**
 * UITour Health Report
 */
/**
 * Public API to be called by the UITour code
 */
const UITourHealthReport = {
  recordTreatmentTag(tag, value) {
    return TelemetryController.submitExternalPing("uitour-tag",
      {
        version: 1,
        tagName: tag,
        tagValue: value,
      },
      {
        addClientId: true,
        addEnvironment: true,
      });
  }
};
