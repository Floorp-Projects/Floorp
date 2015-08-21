// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

"use strict";

this.EXPORTED_SYMBOLS = ["UITour", "UITourMetricsProvider"];

const {classes: Cc, interfaces: Ci, utils: Cu, results: Cr} = Components;

Cu.import("resource://gre/modules/Services.jsm");
Cu.import("resource://gre/modules/XPCOMUtils.jsm");
Cu.import("resource://gre/modules/Promise.jsm");
Cu.import("resource://gre/modules/Task.jsm");
Cu.import("resource://gre/modules/TelemetryController.jsm");

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
XPCOMUtils.defineLazyModuleGetter(this, "Metrics",
  "resource://gre/modules/Metrics.jsm");
XPCOMUtils.defineLazyModuleGetter(this, "ReaderMode",
  "resource://gre/modules/ReaderMode.jsm");
XPCOMUtils.defineLazyModuleGetter(this, "ReaderParent",
  "resource:///modules/ReaderParent.jsm");

// See LOG_LEVELS in Console.jsm. Common examples: "All", "Info", "Warn", & "Error".
const PREF_LOG_LEVEL      = "browser.uitour.loglevel";
const PREF_SEENPAGEIDS    = "browser.uitour.seenPageIDs";
const PREF_READERVIEW_TRIGGER = "browser.uitour.readerViewTrigger";

const BACKGROUND_PAGE_ACTIONS_ALLOWED = new Set([
  "endUrlbarCapture",
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
  let ConsoleAPI = Cu.import("resource://gre/modules/devtools/Console.jsm", {}).ConsoleAPI;
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
  urlbarCapture: new WeakMap(),
  appMenuOpenForAnnotation: new Set(),
  availableTargetsCache: new WeakMap(),

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
    ["loop",        {
      allowAdd: true,
      query: "#loop-button",
      widgetName: "loop-button",
    }],
    ["loop-newRoom", {
      infoPanelPosition: "leftcenter topright",
      query: (aDocument) => {
        let loopUI = aDocument.defaultView.LoopUI;
        if (loopUI.selectedTab != "rooms") {
          return null;
        }
        // Use the parentElement full-width container of the button so our arrow
        // doesn't overlap the panel contents much.
        return loopUI.browser.contentDocument.querySelector(".new-room-button").parentElement;
      },
    }],
    ["loop-roomList", {
      infoPanelPosition: "leftcenter topright",
      query: (aDocument) => {
        let loopUI = aDocument.defaultView.LoopUI;
        if (loopUI.selectedTab != "rooms") {
          return null;
        }
        return loopUI.browser.contentDocument.querySelector(".room-list");
      },
    }],
    ["loop-selectedRoomButtons", {
      infoPanelOffsetY: -20,
      infoPanelPosition: "start_after",
      query: (aDocument) => {
        let chatbox = aDocument.querySelector("chatbox[src^='about\:loopconversation'][selected]");

        // Check that the real target actually exists
        if (!chatbox || !chatbox.contentDocument ||
            !chatbox.contentDocument.querySelector(".call-action-group")) {
          return null;
        }

        // But anchor on the <browser> in the chatbox so the panel doesn't jump to undefined
        // positions when the copy/email buttons disappear e.g. when the feedback form opens or
        // somebody else joins the room.
        return chatbox.content;
      },
    }],
    ["loop-signInUpLink", {
      query: (aDocument) => {
        let loopBrowser = aDocument.defaultView.LoopUI.browser;
        if (!loopBrowser) {
          return null;
        }
        return loopBrowser.contentDocument.querySelector(".signin-link");
      },
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
    ["searchProvider", {
      query: (aDocument) => {
        let searchbar = aDocument.getElementById("searchbar");
        if (searchbar.hasAttribute("oneoffui")) {
          return null;
        }
        return aDocument.getAnonymousElementByAttribute(searchbar,
                                                        "anonid",
                                                        "searchbar-engine-button");
      },
      widgetName: "search-container",
    }],
    ["searchIcon", {
      query: (aDocument) => {
        let searchbar = aDocument.getElementById("searchbar");
        if (!searchbar.hasAttribute("oneoffui")) {
          return null;
        }
        return aDocument.getAnonymousElementByAttribute(searchbar,
                                                        "anonid",
                                                        "searchbar-search-button");
      },
      widgetName: "search-container",
    }],
    ["searchPrefsLink", {
      query: (aDocument) => {
        let element = null;
        let searchbar = aDocument.getElementById("searchbar");
        if (searchbar.hasAttribute("oneoffui")) {
          let popup = aDocument.getElementById("PopupSearchAutoComplete");
          if (popup.state != "open")
            return null;
          element = aDocument.getAnonymousElementByAttribute(popup,
                                                             "anonid",
                                                             "search-settings");
        } else {
          element = aDocument.getAnonymousElementByAttribute(searchbar,
                                                             "anonid",
                                                             "open-engine-manager");
        }
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
    ["urlbar",      {
      query: "#urlbar",
      widgetName: "urlbar-container",
    }],
    ["webide",      {query: "#webide-button"}],
  ]),

  init: function() {
    log.debug("Initializing UITour");
    // Lazy getter is initialized here so it can be replicated any time
    // in a test.
    delete this.seenPageIDs;
    Object.defineProperty(this, "seenPageIDs", {
      get: this.restoreSeenPageIDs.bind(this),
      configurable: true,
    });

    delete this.url;
    XPCOMUtils.defineLazyGetter(this, "url", function () {
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
      listener[method] = () => this.availableTargetsCache.clear();
      return listener;
    }, {}));
  },

  restoreSeenPageIDs: function() {
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

  addSeenPageID: function(aPageID) {
    if (!UITelemetry.enabled)
      return;

    this.seenPageIDs.set(aPageID, {
      lastSeen: Date.now(),
    });

    this.persistSeenIDs();
  },

  persistSeenIDs: function() {
    if (this.seenPageIDs.size === 0) {
      Services.prefs.clearUserPref(PREF_SEENPAGEIDS);
      return;
    }

    Services.prefs.setCharPref(PREF_SEENPAGEIDS,
                               JSON.stringify([...this.seenPageIDs]));
  },

  get _readerViewTriggerRegEx() {
    delete this._readerViewTriggerRegEx;
    let readerViewUITourTrigger = Services.prefs.getCharPref(PREF_READERVIEW_TRIGGER);
    return this._readerViewTriggerRegEx = new RegExp(readerViewUITourTrigger, "i");
  },

  onLocationChange: function(aLocation) {
    // The ReadingList/ReaderView tour page is expected to run in Reader View,
    // which disables JavaScript on the page. To get around that, we
    // automatically start a pre-defined tour on page load.
    let originalUrl = ReaderMode.getOriginalUrl(aLocation);
    if (this._readerViewTriggerRegEx.test(originalUrl)) {
      this.startSubTour("readinglist");
    }
  },

  onPageEvent: function(aMessage, aEvent) {
    let browser = aMessage.target;
    let window = browser.ownerDocument.defaultView;

    // Does the window have tabs? We need to make sure since windowless browsers do
    // not have tabs.
    if (!window.gBrowser) {
      // When using windowless browsers we don't have a valid |window|. If that's the case,
      // use the most recent window as a target for UITour functions (see Bug 1111022).
      window = Services.wm.getMostRecentWindow("navigator:browser");
    }

    let tab = window.gBrowser.getTabForBrowser(browser);
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

    // Do this before bailing if there's no tab, so later we can pick up the pieces:
    window.gBrowser.tabContainer.addEventListener("TabSelect", this);

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

        // Finally show the Heartbeat UI.
        this.showHeartbeat(window, data);
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
                let button = {
                  label: buttonData.label,
                  callbackID: buttonData.callbackID,
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

          if (typeof data.closeButtonCallbackID == "string")
            infoOptions.closeButtonCallbackID = data.closeButtonCallbackID;
          if (typeof data.targetCallbackID == "string")
            infoOptions.targetCallbackID = data.targetCallbackID;

          this.showInfo(window, messageManager, target, data.title, data.text, iconURL, buttons, infoOptions);
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

      case "startUrlbarCapture": {
        if (typeof data.text != "string" || !data.text ||
            typeof data.url != "string" || !data.url) {
          log.warn("startUrlbarCapture: Text or URL not specified");
          return false;
        }

        let uri = null;
        try {
          uri = Services.io.newURI(data.url, null, null);
        } catch (e) {
          log.warn("startUrlbarCapture: Malformed URL specified");
          return false;
        }

        let secman = Services.scriptSecurityManager;
        let contentDocument = browser.contentWindow.document;
        let principal = contentDocument.nodePrincipal;
        let flags = secman.DISALLOW_INHERIT_PRINCIPAL;
        try {
          secman.checkLoadURIWithPrincipal(principal, uri, flags);
        } catch (e) {
          log.warn("startUrlbarCapture: Orginating page doesn't have permission to open specified URL");
          return false;
        }

        this.startUrlbarCapture(window, data.text, data.url);
        break;
      }

      case "endUrlbarCapture": {
        this.endUrlbarCapture(window);
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

      case "showFirefoxAccounts": {
        // 'signup' is the only action that makes sense currently, so we don't
        // accept arbitrary actions just to be safe...
        // We want to replace the current tab.
        browser.loadURI("about:accounts?action=signup&entrypoint=uitour");
        break;
      }

      case "resetFirefox": {
        // Open a reset profile dialog window.
        ResetProfile.openConfirmationDialog(window);
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
        let string = Cc["@mozilla.org/supports-string;1"].createInstance(Ci.nsISupportsString);
        string.data = value;
        Services.prefs.setComplexValue("browser.uitour.treatment." + name,
                                       Ci.nsISupportsString, string);
        UITourHealthReport.recordTreatmentTag(name, value);
        break;
      }

      case "getTreatmentTag": {
        let name = data.name;
        let value;
        try {
          value = Services.prefs.getComplexValue("browser.uitour.treatment." + name,
                                                 Ci.nsISupportsString).data;
        } catch (ex) {}
        this.sendPageCallback(messageManager, data.callbackID, { value: value });
        break;
      }

      case "setSearchTerm": {
        let targetPromise = this.getTarget(window, "search");
        targetPromise.then(target => {
          let searchbar = target.node;
          searchbar.value = data.term;
          searchbar.inputChanged();
        }).then(null, Cu.reportError);
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
    }

    if (!this.tourBrowsersByWindow.has(window)) {
      this.tourBrowsersByWindow.set(window, new Set());
    }
    this.tourBrowsersByWindow.get(window).add(browser);

    Services.obs.addObserver(this, "message-manager-close", false);

    window.addEventListener("SSWindowClosing", this);

    return true;
  },

  handleEvent: function(aEvent) {
    log.debug("handleEvent: type =", aEvent.type, "event =", aEvent);
    switch (aEvent.type) {
      case "pagehide": {
        let window = this.getChromeWindow(aEvent.target);
        this.teardownTourForWindow(window);
        break;
      }

      case "TabSelect": {
        let window = aEvent.target.ownerDocument.defaultView;

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

      case "input": {
        if (aEvent.target.id == "urlbar") {
          let window = aEvent.target.ownerDocument.defaultView;
          this.handleUrlbarInput(window);
        }
        break;
      }
    }
  },

  observe: function(aSubject, aTopic, aData) {
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

  setTelemetryBucket: function(aPageID) {
    let bucket = BUCKET_NAME + BrowserUITelemetry.BUCKET_SEPARATOR + aPageID;
    BrowserUITelemetry.setBucket(bucket);
  },

  setExpiringTelemetryBucket: function(aPageID, aType) {
    let bucket = BUCKET_NAME + BrowserUITelemetry.BUCKET_SEPARATOR + aPageID +
                 BrowserUITelemetry.BUCKET_SEPARATOR + aType;

    BrowserUITelemetry.setExpiringBucket(bucket,
                                         BUCKET_TIMESTEPS);
  },

  // This is registered with UITelemetry by BrowserUITelemetry, so that UITour
  // can remain lazy-loaded on-demand.
  getTelemetry: function() {
    return {
      seenPageIDs: [...this.seenPageIDs.keys()],
    };
  },

  /**
   * Tear down a tour from a tab e.g. upon switching/closing tabs.
   */
  teardownTourForBrowser: function(aWindow, aBrowser, aTourPageClosing = false) {
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
    this.hideMenu(aWindow, "loop");

    // Clean up panel listeners after calling hideMenu above.
    aWindow.PanelUI.panel.removeEventListener("popuphiding", this.hideAppMenuAnnotations);
    aWindow.PanelUI.panel.removeEventListener("ViewShowing", this.hideAppMenuAnnotations);
    aWindow.PanelUI.panel.removeEventListener("popuphidden", this.onPanelHidden);
    let loopPanel = aWindow.document.getElementById("loop-notification-panel");
    loopPanel.removeEventListener("popuphidden", this.onPanelHidden);
    loopPanel.removeEventListener("popuphiding", this.hideLoopPanelAnnotations);

    this.endUrlbarCapture(aWindow);
    this.resetTheme();

    // If there are no more tour tabs left in the window, teardown the tour for the whole window.
    if (!openTourBrowsers || openTourBrowsers.size == 0) {
      this.teardownTourForWindow(aWindow);
    }
  },

  /**
   * Tear down all tours for a ChromeWindow.
   */
  teardownTourForWindow: function(aWindow) {
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

  getChromeWindow: function(aContentDocument) {
    return aContentDocument.defaultView
                           .window
                           .QueryInterface(Ci.nsIInterfaceRequestor)
                           .getInterface(Ci.nsIWebNavigation)
                           .QueryInterface(Ci.nsIDocShellTreeItem)
                           .rootTreeItem
                           .QueryInterface(Ci.nsIInterfaceRequestor)
                           .getInterface(Ci.nsIDOMWindow)
                           .wrappedJSObject;
  },

  // This function is copied to UITourListener.
  isSafeScheme: function(aURI) {
    let allowedSchemes = new Set(["https", "about"]);
    if (!Services.prefs.getBoolPref("browser.uitour.requireSecure"))
      allowedSchemes.add("http");

    if (!allowedSchemes.has(aURI.scheme)) {
      log.error("Unsafe scheme:", aURI.scheme);
      return false;
    }

    return true;
  },

  resolveURL: function(aBrowser, aURL) {
    try {
      let uri = Services.io.newURI(aURL, null, aBrowser.currentURI);

      if (!this.isSafeScheme(uri))
        return null;

      return uri.spec;
    } catch (e) {}

    return null;
  },

  sendPageCallback: function(aMessageManager, aCallbackID, aData = {}) {
    let detail = {data: aData, callbackID: aCallbackID};
    log.debug("sendPageCallback", detail);
    aMessageManager.sendAsyncMessage("UITour:SendPageCallback", detail);
  },

  isElementVisible: function(aElement) {
    let targetStyle = aElement.ownerDocument.defaultView.getComputedStyle(aElement);
    return !aElement.ownerDocument.hidden &&
             targetStyle.display != "none" &&
             targetStyle.visibility == "visible";
  },

  getTarget: function(aWindow, aTargetName, aSticky = false) {
    log.debug("getTarget:", aTargetName);
    let deferred = Promise.defer();
    if (typeof aTargetName != "string" || !aTargetName) {
      log.warn("getTarget: Invalid target name specified");
      deferred.reject("Invalid target name specified");
      return deferred.promise;
    }

    if (aTargetName.startsWith(TARGET_SEARCHENGINE_PREFIX)) {
      let engineID = aTargetName.slice(TARGET_SEARCHENGINE_PREFIX.length);
      return this.getSearchEngineTarget(aWindow, engineID);
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
        node: node,
        removeTargetListener: targetObject.removeTargetListener,
        targetName: aTargetName,
        widgetName: targetObject.widgetName,
        allowAdd: targetObject.allowAdd,
      });
    }).catch(log.error);
    return deferred.promise;
  },

  targetIsInAppMenu: function(aTarget) {
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
  _setAppMenuStateForAnnotation: function(aWindow, aAnnotationType, aShouldOpenForHighlight, aCallback = null) {
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

  previewTheme: function(aTheme) {
    let origin = Services.prefs.getCharPref("browser.uitour.themeOrigin");
    let data = LightweightThemeManager.parseTheme(aTheme, origin);
    if (data)
      LightweightThemeManager.previewTheme(data);
  },

  resetTheme: function() {
    LightweightThemeManager.resetPreview();
  },

  /**
   * Show the Heartbeat UI to request user feedback. This function reports back to the
   * caller using |notify|. The notification event name reflects the current status the UI
   * is in (either "Heartbeat:NotificationOffered", "Heartbeat:NotificationClosed",
   * "Heartbeat:LearnMore", "Heartbeat:Engaged" or "Heartbeat:Voted").
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
   */
  showHeartbeat(aChromeWindow, aOptions) {
    let nb = aChromeWindow.document.getElementById("high-priority-global-notificationbox");
    let buttons = null;

    if (aOptions.engagementButtonLabel) {
      buttons = [{
        label: aOptions.engagementButtonLabel,
        callback: () => {
          // Let the consumer know user engaged.
          this.notify("Heartbeat:Engaged", { flowId: aOptions.flowId, timestamp: Date.now() });

          userEngaged(new Map([
            ["type", "button"],
            ["flowid", aOptions.flowId]
          ]));
        },
      }];
    }
    // Create the notification. Prefix its ID to decrease the chances of collisions.
    let notice = nb.appendNotification(aOptions.message, "heartbeat-" + aOptions.flowId,
      "chrome://browser/skin/heartbeat-icon.svg", nb.PRIORITY_INFO_HIGH, buttons, function() {
        // Let the consumer know the notification bar was closed. This also happens
        // after voting.
        this.notify("Heartbeat:NotificationClosed", { flowId: aOptions.flowId, timestamp: Date.now() });
    }.bind(this));

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
        notice.removeChild(notice.firstChild);
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
      ratingElement.addEventListener("click", function (evt) {
        let rating = Number(evt.target.getAttribute("data-score"), 10);

        // Let the consumer know user voted.
        this.notify("Heartbeat:Voted", { flowId: aOptions.flowId, score: rating, timestamp: Date.now() });

        // Append the score data to the engagement URL.
        userEngaged(new Map([
          ["type", "stars"],
          ["score", rating],
          ["flowid", aOptions.flowId]
        ]));
      }.bind(this));

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
      learnMore.addEventListener("click", () => this.notify("Heartbeat:LearnMore",
        { flowId: aOptions.flowId, timestamp: Date.now() }));
      frag.appendChild(learnMore);
    }

    // Append the fragment and apply the styling.
    notice.appendChild(frag);
    notice.classList.add("heartbeat");
    messageImage.classList.add("heartbeat", "pulse-onshow");
    messageText.classList.add("heartbeat");

    // Let the consumer know the notification was shown.
    this.notify("Heartbeat:NotificationOffered", { flowId: aOptions.flowId, timestamp: Date.now() });
  },

  /**
   * The node to which a highlight or notification(-popup) is anchored is sometimes
   * obscured because it may be inside an overflow menu. This function should figure
   * that out and offer the overflow chevron as an alternative.
   *
   * @param {Node} aAnchor The element that's supposed to be the anchor
   * @type {Node}
   */
  _correctAnchor: function(aAnchor) {
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
  showHighlight: function(aChromeWindow, aTarget, aEffect = "none") {
    function showHighlightPanel() {
      if (aTarget.targetName.startsWith(TARGET_SEARCHENGINE_PREFIX)) {
        // This won't affect normal higlights done via the panel, so we need to
        // manually hide those.
        this.hideHighlight(aChromeWindow);
        aTarget.node.setAttribute("_moz-menuactive", true);
        return;
      }

      // Conversely, highlights for search engines are highlighted via CSS
      // rather than a panel, so need to be manually removed.
      this._hideSearchEngineHighlight(aChromeWindow);

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

  hideHighlight: function(aWindow) {
    let highlighter = aWindow.document.getElementById("UITourHighlight");
    this._removeAnnotationPanelMutationObserver(highlighter.parentElement);
    highlighter.parentElement.hidePopup();
    highlighter.removeAttribute("active");

    this._setAppMenuStateForAnnotation(aWindow, "highlight", false);
    this._hideSearchEngineHighlight(aWindow);
  },

  _hideSearchEngineHighlight: function(aWindow) {
    // We special case highlighting items in the search engines dropdown,
    // so just blindly remove any highlight there.
    let searchMenuBtn = null;
    try {
      searchMenuBtn = this.targets.get("searchProvider").query(aWindow.document);
    } catch (e) { /* This is ok to fail. */ }
    if (searchMenuBtn) {
      let searchPopup = aWindow.document
                               .getAnonymousElementByAttribute(searchMenuBtn,
                                                               "anonid",
                                                               "searchbar-popup");
      for (let menuItem of searchPopup.children)
        menuItem.removeAttribute("_moz-menuactive");
    }
  },

  /**
   * Show an info panel.
   *
   * @param {ChromeWindow} aChromeWindow
   * @param {nsIMessageSender} aMessageManager
   * @param {Node}     aAnchor
   * @param {String}   [aTitle=""]
   * @param {String}   [aDescription=""]
   * @param {String}   [aIconURL=""]
   * @param {Object[]} [aButtons=[]]
   * @param {Object}   [aOptions={}]
   * @param {String}   [aOptions.closeButtonCallbackID]
   */
  showInfo: function(aChromeWindow, aMessageManager, aAnchor, aTitle = "", aDescription = "", aIconURL = "",
                     aButtons = [], aOptions = {}) {
    function showInfoPanel(aAnchorEl) {
      aAnchorEl.focus();

      let document = aChromeWindow.document;
      let tooltip = document.getElementById("UITourTooltip");
      let tooltipTitle = document.getElementById("UITourTooltipTitle");
      let tooltipDesc = document.getElementById("UITourTooltipDescription");
      let tooltipIcon = document.getElementById("UITourTooltipIcon");
      let tooltipIconContainer = document.getElementById("UITourTooltipIconContainer");
      let tooltipButtons = document.getElementById("UITourTooltipButtons");

      if (tooltip.state == "showing" || tooltip.state == "open") {
        tooltip.hidePopup();
      }

      tooltipTitle.textContent = aTitle || "";
      tooltipDesc.textContent = aDescription || "";
      tooltipIcon.src = aIconURL || "";
      tooltipIconContainer.hidden = !aIconURL;

      while (tooltipButtons.firstChild)
        tooltipButtons.firstChild.remove();

      for (let button of aButtons) {
        let el = document.createElement("button");
        el.setAttribute("label", button.label);
        if (button.iconURL)
          el.setAttribute("image", button.iconURL);

        if (button.style == "link")
          el.setAttribute("class", "button-link");

        if (button.style == "primary")
          el.setAttribute("class", "button-primary");

        let callbackID = button.callbackID;
        el.addEventListener("command", event => {
          tooltip.hidePopup();
          this.sendPageCallback(aMessageManager, callbackID);
        });

        tooltipButtons.appendChild(el);
      }

      tooltipButtons.hidden = !aButtons.length;

      let tooltipClose = document.getElementById("UITourTooltipClose");
      let closeButtonCallback = (event) => {
        this.hideInfo(document.defaultView);
        if (aOptions && aOptions.closeButtonCallbackID)
          this.sendPageCallback(aMessageManager, aOptions.closeButtonCallbackID);
      };
      tooltipClose.addEventListener("command", closeButtonCallback);

      let targetCallback = (event) => {
        let details = {
          target: aAnchor.targetName,
          type: event.type,
        };
        this.sendPageCallback(aMessageManager, aOptions.targetCallbackID, details);
      };
      if (aOptions.targetCallbackID && aAnchor.addTargetListener) {
        aAnchor.addTargetListener(document, targetCallback);
      }

      tooltip.addEventListener("popuphiding", function tooltipHiding(event) {
        tooltip.removeEventListener("popuphiding", tooltipHiding);
        tooltipClose.removeEventListener("command", closeButtonCallback);
        if (aOptions.targetCallbackID && aAnchor.removeTargetListener) {
          aAnchor.removeTargetListener(document, targetCallback);
        }
      });

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
        document.defaultView.addEventListener("endmodalstate", function endModalStateHandler() {
          document.defaultView.removeEventListener("endmodalstate", endModalStateHandler);
          tooltip.openPopup(aAnchorEl, alignment);
        }, false);
      }
    }

    // Prevent showing a panel at an undefined position.
    if (!this.isElementVisible(aAnchor.node)) {
      log.warn("showInfo: Not showing since the target isn't visible", aAnchor);
      return;
    }

    // Due to a platform limitation, we can't anchor a panel to an element in a
    // <menupopup>. So we can't support showing info panels for search engines.
    if (aAnchor.targetName.startsWith(TARGET_SEARCHENGINE_PREFIX))
      return;

    this._setAppMenuStateForAnnotation(aChromeWindow, "info",
                                       this.targetIsInAppMenu(aAnchor),
                                       showInfoPanel.bind(this, this._correctAnchor(aAnchor.node)));
  },

  isInfoOnTarget(aChromeWindow, aTargetName) {
    let document = aChromeWindow.document;
    let tooltip = document.getElementById("UITourTooltip");
    return tooltip.getAttribute("targetName") == aTargetName && tooltip.state != "closed";
  },

  hideInfo: function(aWindow) {
    let document = aWindow.document;

    let tooltip = document.getElementById("UITourTooltip");
    this._removeAnnotationPanelMutationObserver(tooltip);
    tooltip.hidePopup();
    this._setAppMenuStateForAnnotation(aWindow, "info", false);

    let tooltipButtons = document.getElementById("UITourTooltipButtons");
    while (tooltipButtons.firstChild)
      tooltipButtons.firstChild.remove();
  },

  showMenu: function(aWindow, aMenuName, aOpenCallback = null) {
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
    } else if (aMenuName == "loop") {
      let toolbarButton = aWindow.LoopUI.toolbarButton;
      // It's possible to have a node that isn't placed anywhere
      if (!toolbarButton || !toolbarButton.node ||
          !CustomizableUI.getPlacementOfWidget(toolbarButton.node.id)) {
        log.debug("Can't show the Loop menu since the toolbarButton isn't placed");
        return;
      }

      let panel = aWindow.document.getElementById("loop-notification-panel");
      panel.setAttribute("noautohide", true);
      if (panel.state != "open") {
        this.recreatePopup(panel);
        this.availableTargetsCache.clear();
      }

      // An event object is expected but we don't want to toggle the panel with a click if the panel
      // is already open.
      aWindow.LoopUI.openCallPanel({ target: toolbarButton.node, }, "rooms").then(() => {
        if (aOpenCallback) {
          aOpenCallback();
        }
      });
      panel.addEventListener("popuphidden", this.onPanelHidden);
      panel.addEventListener("popuphiding", this.hideLoopPanelAnnotations);
    } else if (aMenuName == "searchEngines") {
      this.getTarget(aWindow, "searchProvider").then(target => {
        openMenuButton(target.node);
      }).catch(log.error);
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

            aWindow.PanelUI.panel.addEventListener("popupshown", function onShown() {
              aWindow.PanelUI.panel.removeEventListener("popupshown", onShown);
              resolve();
            });

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

  hideMenu: function(aWindow, aMenuName) {
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
    } else if (aMenuName == "loop") {
      let panel = aWindow.document.getElementById("loop-notification-panel");
      panel.hidePopup();
    } else if (aMenuName == "searchEngines") {
      let menuBtn = this.targets.get("searchProvider").query(aWindow.document);
      closeMenuButton(menuBtn);
    }
  },

  hideAnnotationsForPanel: function(aEvent, aTargetPositionCallback) {
    let win = aEvent.target.ownerDocument.defaultView;
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

  hideAppMenuAnnotations: function(aEvent) {
    UITour.hideAnnotationsForPanel(aEvent, UITour.targetIsInAppMenu);
  },

  hideLoopPanelAnnotations: function(aEvent) {
    UITour.hideAnnotationsForPanel(aEvent, (aTarget) => {
      return aTarget.targetName.startsWith("loop-") && aTarget.targetName != "loop-selectedRoomButtons";
    });
  },

  onPanelHidden: function(aEvent) {
    aEvent.target.removeAttribute("noautohide");
    UITour.recreatePopup(aEvent.target);
  },

  recreatePopup: function(aPanel) {
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

  startUrlbarCapture: function(aWindow, aExpectedText, aUrl) {
    let urlbar = aWindow.document.getElementById("urlbar");
    this.urlbarCapture.set(aWindow, {
      expected: aExpectedText.toLocaleLowerCase(),
      url: aUrl
    });
    urlbar.addEventListener("input", this);
  },

  endUrlbarCapture: function(aWindow) {
    let urlbar = aWindow.document.getElementById("urlbar");
    urlbar.removeEventListener("input", this);
    this.urlbarCapture.delete(aWindow);
  },

  handleUrlbarInput: function(aWindow) {
    if (!this.urlbarCapture.has(aWindow))
      return;

    let urlbar = aWindow.document.getElementById("urlbar");

    let {expected, url} = this.urlbarCapture.get(aWindow);

    if (urlbar.value.toLocaleLowerCase().localeCompare(expected) != 0)
      return;

    urlbar.handleRevert();

    let tab = aWindow.gBrowser.addTab(url, {
      owner: aWindow.gBrowser.selectedTab,
      relatedToCurrent: true
    });
    aWindow.gBrowser.selectedTab = tab;
  },

  getConfiguration: function(aMessageManager, aWindow, aConfiguration, aCallbackID) {
    switch (aConfiguration) {
      case "appinfo":
        let props = ["defaultUpdateChannel", "version"];
        let appinfo = {};
        props.forEach(property => appinfo[property] = Services.appinfo[property]);
        let isDefaultBrowser = null;
        try {
          let shell = aWindow.getShellService();
          if (shell) {
            isDefaultBrowser = shell.isDefaultBrowser(false);
          }
        } catch (e) {}
        appinfo["defaultBrowser"] = isDefaultBrowser;
        this.sendPageCallback(aMessageManager, aCallbackID, appinfo);
        break;
      case "availableTargets":
        this.getAvailableTargets(aMessageManager, aWindow, aCallbackID);
        break;
      case "loop":
        this.sendPageCallback(aMessageManager, aCallbackID, {
          gettingStartedSeen: Services.prefs.getBoolPref("loop.gettingStarted.seen"),
        });
        break;
      case "selectedSearchEngine":
        Services.search.init(rv => {
          let engine;
          if (Components.isSuccessCode(rv)) {
            engine = Services.search.defaultEngine;
          } else {
            engine = { identifier: "" };
          }
          this.sendPageCallback(aMessageManager, aCallbackID, {
            searchEngineIdentifier: engine.identifier
          });
        });
        break;
      case "sync":
        this.sendPageCallback(aMessageManager, aCallbackID, {
          setup: Services.prefs.prefHasUserValue("services.sync.username"),
        });
        break;
      default:
        log.error("getConfiguration: Unknown configuration requested: " + aConfiguration);
        break;
    }
  },

  setConfiguration: function(aWindow, aConfiguration, aValue) {
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
      case "Loop:ResumeTourOnFirstJoin":
        // Ignore aValue in this case to avoid accidentally setting it to false.
        Services.prefs.setBoolPref("loop.gettingStarted.resumeOnFirstJoin", true);
        break;
      default:
        log.error("setConfiguration: Unknown configuration requested: " + aConfiguration);
        break;
    }
  },

  getAvailableTargets: function(aMessageManager, aChromeWindow, aCallbackID) {
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

      targetNames = targetNames.concat(
        yield this.getAvailableSearchEngineTargets(window)
      );

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

  startSubTour: function (aFeature) {
    if (aFeature != "string") {
      log.error("startSubTour: No feature option specified");
      return;
    }

    if (aFeature == "readinglist") {
      ReaderParent.showReaderModeInfoPanel(browser);
    } else {
      log.error("startSubTour: Unknown feature option specified");
      return;
    }
  },

  addNavBarWidget: function (aTarget, aMessageManager, aCallbackID) {
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

  _addAnnotationPanelMutationObserver: function(aPanelEl) {
#ifdef XP_LINUX
    let observer = this._annotationPanelMutationObservers.get(aPanelEl);
    if (observer) {
      return;
    }
    let win = aPanelEl.ownerDocument.defaultView;
    observer = new win.MutationObserver(this._annotationMutationCallback);
    this._annotationPanelMutationObservers.set(aPanelEl, observer);
    let observerOptions = {
      attributeFilter: ["height", "width"],
      attributes: true,
    };
    observer.observe(aPanelEl, observerOptions);
#endif
  },

  _removeAnnotationPanelMutationObserver: function(aPanelEl) {
#ifdef XP_LINUX
    let observer = this._annotationPanelMutationObservers.get(aPanelEl);
    if (observer) {
      observer.disconnect();
      this._annotationPanelMutationObservers.delete(aPanelEl);
    }
#endif
  },

/**
 * Workaround for Ubuntu panel craziness in bug 970788 where incorrect sizes get passed to
 * nsXULPopupManager::PopupResized and lead to incorrect width and height attributes getting
 * set on the panel.
 */
  _annotationMutationCallback: function(aMutations) {
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
            return resolve();
          }
        }
        reject("selectSearchEngine could not find engine with given ID");
      });
    });
  },

  getAvailableSearchEngineTargets(aWindow) {
    return new Promise(resolve => {
      this.getTarget(aWindow, "search").then(searchTarget => {
        if (!searchTarget.node || this.targetIsInAppMenu(searchTarget))
          return resolve([]);

        Services.search.init(() => {
          let engines = Services.search.getVisibleEngines();
          resolve([TARGET_SEARCHENGINE_PREFIX + engine.identifier
                   for (engine of engines)
                   if (engine.identifier)]);
        });
      }).catch(() => resolve([]));
    });
  },

  // We only allow matching based on a search engine's identifier - this gives
  // us a non-changing ID and guarentees we only match against app-provided
  // engines.
  getSearchEngineTarget(aWindow, aIdentifier) {
    return new Promise((resolve, reject) => {
      Task.spawn(function*() {
        let searchTarget = yield this.getTarget(aWindow, "search");
        // We're not supporting having the searchbar in the app-menu, because
        // popups within popups gets crazy. This restriction should be lifted
        // once bug 988151 is implemented, as the page can then be responsible
        // for opening each menu when appropriate.
        if (!searchTarget.node || this.targetIsInAppMenu(searchTarget))
          return reject("Search engine not available");

        yield Services.search.init();

        let searchPopup = searchTarget.node._popup;
        for (let engineNode of searchPopup.children) {
          let engine = engineNode.engine;
          if (engine && engine.identifier == aIdentifier) {
            return resolve({
              targetName: TARGET_SEARCHENGINE_PREFIX + engine.identifier,
              node: engineNode,
            });
          }
        }
        reject("Search engine not available");
      }.bind(this)).catch(() => {
        reject("Search engine not available");
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
          params: params,
        };
        messageManager.sendAsyncMessage("UITour:SendPageNotification", detail);
      }
    }
  },
};

this.UITour.init();

/**
 * UITour Health Report
 */
const DAILY_DISCRETE_TEXT_FIELD = Metrics.Storage.FIELD_DAILY_DISCRETE_TEXT;

/**
 * Public API to be called by the UITour code
 */
const UITourHealthReport = {
  recordTreatmentTag: function(tag, value) {
  TelemetryController.submitExternalPing("uitour-tag",
    {
      version: 1,
      tagName: tag,
      tagValue: value,
    },
    {
      addClientId: true,
      addEnvironment: true,
    });
#ifdef MOZ_SERVICES_HEALTHREPORT
    Task.spawn(function*() {
      let reporter = Cc["@mozilla.org/datareporting/service;1"]
                       .getService()
                       .wrappedJSObject
                       .healthReporter;

      // This can happen if the FHR component of the data reporting service is
      // disabled. This is controlled by a pref that most will never use.
      if (!reporter) {
        return;
      }

      yield reporter.onInit();

      // Get the UITourMetricsProvider instance from the Health Reporter
      reporter.getProvider("org.mozilla.uitour").recordTreatmentTag(tag, value);
    });
#endif
  }
};

this.UITourMetricsProvider = function() {
  Metrics.Provider.call(this);
}

UITourMetricsProvider.prototype = Object.freeze({
  __proto__: Metrics.Provider.prototype,

  name: "org.mozilla.uitour",

  measurementTypes: [
    UITourTreatmentMeasurement1,
  ],

  recordTreatmentTag: function(tag, value) {
    let m = this.getMeasurement(UITourTreatmentMeasurement1.prototype.name,
                                UITourTreatmentMeasurement1.prototype.version);
    let field = tag;

    if (this.storage.hasFieldFromMeasurement(m.id, field,
                                             DAILY_DISCRETE_TEXT_FIELD)) {
      let fieldID = this.storage.fieldIDFromMeasurement(m.id, field);
      return this.enqueueStorageOperation(function recordKnownField() {
        return this.storage.addDailyDiscreteTextFromFieldID(fieldID, value);
      }.bind(this));
    }

    // Otherwise, we first need to create the field.
    return this.enqueueStorageOperation(function recordField() {
      // This function has to return a promise.
      return Task.spawn(function () {
        let fieldID = yield this.storage.registerField(m.id, field,
                                                       DAILY_DISCRETE_TEXT_FIELD);
        yield this.storage.addDailyDiscreteTextFromFieldID(fieldID, value);
      }.bind(this));
    }.bind(this));
  },
});

function UITourTreatmentMeasurement1() {
  Metrics.Measurement.call(this);

  this._serializers = {};
  this._serializers[this.SERIALIZE_JSON] = {
    //singular: We don't need a singular serializer because we have none of this data
    daily: this._serializeJSONDaily.bind(this)
  };

}

UITourTreatmentMeasurement1.prototype = Object.freeze({
  __proto__: Metrics.Measurement.prototype,

  name: "treatment",
  version: 1,

  // our fields are dynamic
  fields: { },

  // We need a custom serializer because the default one doesn't accept unknown fields
  _serializeJSONDaily: function(data) {
    let result = {_v: this.version };

    for (let [field, data] of data) {
      result[field] = data;
    }

    return result;
  }
});
