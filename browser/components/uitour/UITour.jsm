// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

"use strict";

var EXPORTED_SYMBOLS = ["UITour"];

const {AppConstants} = ChromeUtils.import("resource://gre/modules/AppConstants.jsm");
const {Services} = ChromeUtils.import("resource://gre/modules/Services.jsm");
const {TelemetryController} = ChromeUtils.import("resource://gre/modules/TelemetryController.jsm");
const {XPCOMUtils} = ChromeUtils.import("resource://gre/modules/XPCOMUtils.jsm");

XPCOMUtils.defineLazyGlobalGetters(this, ["URL"]);

ChromeUtils.defineModuleGetter(this, "CustomizableUI",
  "resource:///modules/CustomizableUI.jsm");
ChromeUtils.defineModuleGetter(this, "FxAccounts",
  "resource://gre/modules/FxAccounts.jsm");
ChromeUtils.defineModuleGetter(this, "PageActions",
  "resource:///modules/PageActions.jsm");
ChromeUtils.defineModuleGetter(this, "PrivateBrowsingUtils",
  "resource://gre/modules/PrivateBrowsingUtils.jsm");
ChromeUtils.defineModuleGetter(this, "ProfileAge",
  "resource://gre/modules/ProfileAge.jsm");
ChromeUtils.defineModuleGetter(this, "ReaderParent",
  "resource:///modules/ReaderParent.jsm");
ChromeUtils.defineModuleGetter(this, "ResetProfile",
  "resource://gre/modules/ResetProfile.jsm");
ChromeUtils.defineModuleGetter(this, "UpdateUtils",
  "resource://gre/modules/UpdateUtils.jsm");
ChromeUtils.defineModuleGetter(this, "UrlbarPrefs",
  "resource:///modules/UrlbarPrefs.jsm");

// See LOG_LEVELS in Console.jsm. Common examples: "All", "Info", "Warn", & "Error".
const PREF_LOG_LEVEL      = "browser.uitour.loglevel";

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

// Prefix for any target matching a search engine.
const TARGET_SEARCHENGINE_PREFIX = "searchEngine-";

// Create a new instance of the ConsoleAPI so we can control the maxLogLevel with a pref.
XPCOMUtils.defineLazyGetter(this, "log", () => {
  let ConsoleAPI = ChromeUtils.import("resource://gre/modules/Console.jsm", {}).ConsoleAPI;
  let consoleOptions = {
    maxLogLevelPref: PREF_LOG_LEVEL,
    prefix: "UITour",
  };
  return new ConsoleAPI(consoleOptions);
});

var UITour = {
  url: null,
  /* Map from browser chrome windows to a Set of <browser>s in which a tour is open (both visible and hidden) */
  tourBrowsersByWindow: new WeakMap(),
  // Menus opened by api users explictly through `Mozilla.UITour.showMenu` call
  noautohideMenus: new Set(),
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
        let fxAFooter = aDocument.getElementById("appMenu-fxa-container");
        if (fxAFooter.getAttribute("fxastatus")) {
          return aDocument.getElementById("appMenu-fxa-avatar");
        }

        // Otherwise use the sync setup icon.
        let statusButton = aDocument.getElementById("appMenu-fxa-label");
        return aDocument.getAnonymousElementByAttribute(statusButton,
                                                        "class",
                                                        "toolbarbutton-icon");
      },
      // This is a fake widgetName starting with the "appMenu-" prefix so we know
      // to automatically open the appMenu when annotating this target.
      widgetName: "appMenu-fxa-label",
    }],
    ["addons",      {query: "#appMenu-addons-button"}],
    ["appMenu",     {
      addTargetListener: (aDocument, aCallback) => {
        let panelPopup = aDocument.defaultView.PanelUI.panel;
        panelPopup.addEventListener("popupshown", aCallback);
      },
      query: "#PanelUI-button",
      removeTargetListener: (aDocument, aCallback) => {
        let panelPopup = aDocument.defaultView.PanelUI.panel;
        panelPopup.removeEventListener("popupshown", aCallback);
      },
    }],
    ["backForward", {query: "#back-button"}],
    ["bookmarks",   {query: "#bookmarks-menu-button"}],
    ["controlCenter-trackingUnblock", controlCenterTrackingToggleTarget(true)],
    ["controlCenter-trackingBlock", controlCenterTrackingToggleTarget(false)],
    ["customize",   {
      query: "#appMenu-customize-button",
      widgetName: "appMenu-customize-button",
    }],
    ["devtools",    {
      query: "#appMenu-developer-button",
      widgetName: "appMenu-developer-button",
    }],
    ["forget", {
      allowAdd: true,
      query: "#panic-button",
      widgetName: "panic-button",
    }],
    ["help",        {query: "#appMenu-help-button"}],
    ["home",        {query: "#home-button"}],
    ["library",     {query: "#appMenu-library-button"}],
    ["pocket", {
      allowAdd: true,
      query: (aDocument) => {
        // The pocket's urlbar page action button is pre-defined in the DOM.
        // It would be hidden if toggled off from the urlbar.
        let node = aDocument.getElementById("pocket-button-box");
        if (node && !node.hidden) {
          return node;
        }
        aDocument.ownerGlobal.BrowserPageActions.placeLazyActionsInPanel();
        return aDocument.getElementById("pageAction-panel-pocket");
      },
    }],
    ["privateWindow", {query: "#appMenu-private-window-button"}],
    ["quit",        {query: "#appMenu-quit-button"}],
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
        return searchbar.querySelector(".searchbar-search-button");
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
      query: (aDocument) => {
        if (Services.prefs.getBoolPref("toolkit.cosmeticAnimations.enabled", false)) {
          return aDocument.getElementById("tracking-protection-icon-animatable-box");
        }
        return aDocument.getElementById("tracking-protection-icon");
      },
    }],
    ["urlbar",      {
      query: "#urlbar",
      widgetName: "urlbar-container",
    }],
    ["pageActionButton", {
      query: "#pageActionButton",
    }],
    ["pageAction-bookmark", {
      query: (aDocument) => {
        // The bookmark's urlbar page action button is pre-defined in the DOM.
        // It would be hidden if toggled off from the urlbar.
        let node = aDocument.getElementById("star-button-box");
        if (node && !node.hidden) {
          return node;
        }
        aDocument.ownerGlobal.BrowserPageActions.placeLazyActionsInPanel();
        return aDocument.getElementById("pageAction-panel-bookmark");
      },
    }],
    ["pageAction-copyURL", {
      query: (aDocument) => {
        aDocument.ownerGlobal.BrowserPageActions.placeLazyActionsInPanel();
        return aDocument.getElementById("pageAction-urlbar-copyURL") ||
               aDocument.getElementById("pageAction-panel-copyURL");
      },
    }],
    ["pageAction-emailLink", {
      query: (aDocument) => {
        aDocument.ownerGlobal.BrowserPageActions.placeLazyActionsInPanel();
        return aDocument.getElementById("pageAction-urlbar-emailLink") ||
               aDocument.getElementById("pageAction-panel-emailLink");
      },
    }],
    ["pageAction-sendToDevice", {
      query: (aDocument) => {
        aDocument.ownerGlobal.BrowserPageActions.placeLazyActionsInPanel();
        return aDocument.getElementById("pageAction-urlbar-sendToDevice") ||
               aDocument.getElementById("pageAction-panel-sendToDevice");
      },
    }],
    ["screenshots", {
      query: (aDocument) => {
        aDocument.ownerGlobal.BrowserPageActions.placeLazyActionsInPanel();
        return aDocument.getElementById("pageAction-urlbar-screenshots") ||
               aDocument.getElementById("pageAction-panel-screenshots_mozilla_org");
      },
    }],
  ]),

  init() {
    log.debug("Initializing UITour");
    // Lazy getter is initialized here so it can be replicated any time
    // in a test.
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
          if (this.highlightEffects.includes(data.effect)) {
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

      case "showMenu": {
        this.noautohideMenus.add(data.name);
        this.showMenu(window, data.name, () => {
          if (typeof data.showCallbackID == "string")
            this.sendPageCallback(messageManager, data.showCallbackID);
        });
        break;
      }

      case "hideMenu": {
        this.noautohideMenus.delete(data.name);
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
        window.openPreferences(data.pane);
        break;
      }

      case "showFirefoxAccounts": {
        Promise.resolve().then(() => {
          return data.email ? FxAccounts.config.promiseEmailURI(data.email, "uitour") :
                              FxAccounts.config.promiseSignUpURI("uitour");
        }).then(uri => {
          const url = new URL(uri);
          // Call our helper to validate extraURLCampaignParams and populate URLSearchParams
          if (!this._populateCampaignParams(url, data.extraURLCampaignParams)) {
            log.warn("showFirefoxAccounts: invalid campaign args specified");
            return;
          }

          // We want to replace the current tab.
          browser.loadURI(url.href, {
            triggeringPrincipal: Services.scriptSecurityManager.createNullPrincipal({}),
          });
        });
        break;
      }

      case "showConnectAnotherDevice": {
        FxAccounts.config.promiseConnectDeviceURI("uitour").then(uri => {
          const url = new URL(uri);
          // Call our helper to validate extraURLCampaignParams and populate URLSearchParams
          if (!this._populateCampaignParams(url, data.extraURLCampaignParams)) {
            log.warn("showConnectAnotherDevice: invalid campaign args specified");
            return;
          }

          // We want to replace the current tab.
          browser.loadURI(url.href, {
            triggeringPrincipal: Services.scriptSecurityManager.createNullPrincipal({}),
          });
        });
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
        }).catch(Cu.reportError);
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
        for (let window of Services.wm.getEnumerator("navigator:browser")) {
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
  // them to the passed URL object. Returns true if the params
  // were validated and appended, and false if the request should be ignored.
  _populateCampaignParams(url, extraURLCampaignParams) {
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
        url.searchParams.append(name, value);
      }
    }
    return true;
  },

  /**
   * Tear down a tour from a tab e.g. upon switching/closing tabs.
   */
  async teardownTourForBrowser(aWindow, aBrowser, aTourPageClosing = false) {
    log.debug("teardownTourForBrowser: aBrowser = ", aBrowser, aTourPageClosing);

    let openTourBrowsers = this.tourBrowsersByWindow.get(aWindow);
    if (aTourPageClosing && openTourBrowsers) {
      openTourBrowsers.delete(aBrowser);
    }

    this.hideHighlight(aWindow);
    this.hideInfo(aWindow);

    let panels = [
      {
        name: "appMenu",
        node: aWindow.PanelUI.panel,
        events: [
          [ "popuphidden", this.onPanelHidden ],
          [ "popuphiding", this.onAppMenuHiding ],
          [ "ViewShowing", this.onAppMenuSubviewShowing ],
        ],
      },
      {
        name: "pageActionPanel",
        node: aWindow.BrowserPageActions.panelNode,
        events: [
          [ "popuphidden", this.onPanelHidden ],
          [ "popuphiding", this.onPageActionPanelHiding ],
          [ "ViewShowing", this.onPageActionPanelSubviewShowing ],
        ],
      },
      {
        name: "controlCenter",
        node: aWindow.gIdentityHandler._identityPopup,
        events: [
          [ "popuphidden", this.onPanelHidden ],
          [ "popuphiding", this.onControlCenterHiding ],
        ],
      },
    ];
    for (let panel of panels) {
      // Ensure the menu panel is hidden and clean up panel listeners after calling hideMenu.
      if (panel.node.state != "closed") {
        await new Promise(resolve => {
          panel.node.addEventListener("popuphidden", resolve, { once: true });
          this.hideMenu(aWindow, panel.name);
        });
      }
      for (let [ name, listener ] of panel.events) {
        panel.node.removeEventListener(name, listener);
      }
    }

    this.noautohideMenus.clear();

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
    if (typeof aTargetName != "string" || !aTargetName) {
      log.warn("getTarget: Invalid target name specified");
      return Promise.reject("Invalid target name specified");
    }

    let targetObject = this.targets.get(aTargetName);
    if (!targetObject) {
      log.warn("getTarget: The specified target name is not in the allowed set");
      return Promise.reject("The specified target name is not in the allowed set");
    }

    return new Promise(resolve => {
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

        resolve({
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
    });
  },

  targetIsInAppMenu(aTarget) {
    let targetElement = aTarget.node;
    // Use the widget for filtering if it exists since the target may be the icon inside.
    if (aTarget.widgetName) {
      targetElement = aTarget.node.ownerDocument.getElementById(aTarget.widgetName);
    }

    return targetElement.id.startsWith("appMenu-");
  },

  targetIsInPageActionPanel(aTarget) {
    return aTarget.node.id.startsWith("pageAction-panel-");
  },

  /**
   * Called before opening or after closing a highlight or an info tooltip to see if
   * we need to open or close the menu to see the annotation's anchor.
   *
   * @param {ChromeWindow} aWindow the chrome window
   * @param {bool} aShouldOpen true means we should open the menu, otherwise false
   * @param {String} aMenuName "appMenu" or "pageActionPanel"
   */
  _setMenuStateForAnnotation(aWindow, aShouldOpen, aMenuName) {
    log.debug("_setMenuStateForAnnotation: Menu is ", aMenuName);
    log.debug("_setMenuStateForAnnotation: Menu is expected to be:", aShouldOpen ? "open" : "closed");
    let menu = aMenuName == "appMenu" ? aWindow.PanelUI.panel : aWindow.BrowserPageActions.panelNode;

    // If the panel is in the desired state, we're done.
    let panelIsOpen = menu.state != "closed";
    if (aShouldOpen == panelIsOpen) {
      log.debug("_setMenuStateForAnnotation: Menu already in expected state");
      return Promise.resolve();
    }

    // Actually show or hide the menu
    let promise = null;
    if (aShouldOpen) {
      log.debug("_setMenuStateForAnnotation: Opening the menu");
      promise = new Promise(resolve => {
        this.showMenu(aWindow, aMenuName, resolve);
      });
    } else if (!this.noautohideMenus.has(aMenuName)) {
      // If the menu was opened explictly by api user through `Mozilla.UITour.showMenu`,
      // it should be closed explictly by api user through `Mozilla.UITour.hideMenu`.
      // So we shouldn't get to here to close it for the highlight/info annotation.
      log.debug("_setMenuStateForAnnotation: Closing the menu");
      promise = new Promise(resolve => {
        menu.addEventListener("popuphidden", resolve, { once: true });
        this.hideMenu(aWindow, aMenuName);
      });
    }
    return promise;
  },

  /**
   * Ensure the target's visibility and the open/close states of menus for the target.
   *
   * @param {ChromeWindow} aChromeWindow The chrome window
   * @param {Object} aTarget The target on which we show highlight or show info.
   */
  async _ensureTarget(aChromeWindow, aTarget) {
    let shouldOpenAppMenu = false;
    let shouldOpenPageActionPanel = false;
    if (this.targetIsInAppMenu(aTarget)) {
      shouldOpenAppMenu = true;
    } else if (this.targetIsInPageActionPanel(aTarget)) {
      shouldOpenPageActionPanel = true;
      // Ensure the panel visibility so as to ensure the visibility of the target
      // element inside the panel otherwise we would be rejected in the below
      // `isElementVisible` checking.
      aChromeWindow.BrowserPageActions.panelNode.hidden = false;
    }

    // Prevent showing a panel at an undefined position, but when it's tucked
    // away inside a panel, we skip this check.
    if (!aTarget.node.closest("panelview") && !this.isElementVisible(aTarget.node)) {
      return Promise.reject(`_ensureTarget: Reject the ${aTarget.name || aTarget.targetName} target since it isn't visible.`);
    }

    let menuToOpen = null;
    let menuClosePromises = [];
    if (shouldOpenAppMenu) {
      menuToOpen = "appMenu";
      menuClosePromises.push(this._setMenuStateForAnnotation(aChromeWindow, false, "pageActionPanel"));
    } else if (shouldOpenPageActionPanel) {
      menuToOpen = "pageActionPanel";
      menuClosePromises.push(this._setMenuStateForAnnotation(aChromeWindow, false, "appMenu"));
    } else {
      menuClosePromises.push(this._setMenuStateForAnnotation(aChromeWindow, false, "appMenu"));
      menuClosePromises.push(this._setMenuStateForAnnotation(aChromeWindow, false, "pageActionPanel"));
    }

    let promise = Promise.all(menuClosePromises);
    await promise;
    if (menuToOpen) {
      promise = this._setMenuStateForAnnotation(aChromeWindow, true, menuToOpen);
    }
    return promise;
  },

  /**
   * The node to which a highlight or notification(-popup) is anchored is sometimes
   * obscured because it may be inside an overflow menu. This function should figure
   * that out and offer the overflow chevron as an alternative.
   *
   * @param {ChromeWindow} aChromeWindow The chrome window
   * @param {Object} aTarget The target object whose node is supposed to be the anchor
   * @type {Node}
   */
  async _correctAnchor(aChromeWindow, aTarget) {
    // PanelMultiView's like the AppMenu might shuffle the DOM, which might result
    // in our anchor being invalidated if it was anonymous content (since the XBL
    // binding it belonged to got destroyed). We work around this by re-querying for
    // the node and stuffing it into the old anchor structure.
    let refreshedTarget = await this.getTarget(aChromeWindow, aTarget.targetName);
    let node = aTarget.node = refreshedTarget.node;
    // If the target is in the overflow panel, just return the overflow button.
    if (node.closest("#widget-overflow-mainView")) {
      return CustomizableUI.getWidget(node.id).forWindow(aChromeWindow).anchor;
    }
    return node;
  },

  /**
   * @param aChromeWindow The chrome window that the highlight is in. Necessary since some targets
   *                      are in a sub-frame so the defaultView is not the same as the chrome
   *                      window.
   * @param aTarget    The element to highlight.
   * @param aEffect    (optional) The effect to use from UITour.highlightEffects or "none".
   * @see UITour.highlightEffects
   */
  async showHighlight(aChromeWindow, aTarget, aEffect = "none") {
    let showHighlightElement = (aAnchorEl) => {
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

      let highlightAnchor = aAnchorEl;
      let targetRect = highlightAnchor.getBoundingClientRect();
      let highlightHeight = targetRect.height;
      let highlightWidth = targetRect.width;

      if (this.targetIsInAppMenu(aTarget) || this.targetIsInPageActionPanel(aTarget)) {
        highlighter.classList.remove("rounded-highlight");
      } else {
        highlighter.classList.add("rounded-highlight");
      }
      if (highlightAnchor.classList.contains("toolbarbutton-1") &&
          highlightAnchor.getAttribute("cui-areatype") === "toolbar" &&
          highlightAnchor.getAttribute("overflowedItem") !== "true") {
        // A toolbar button in navbar has its clickable area an
        // inner-contained square while the button component itself is a tall
        // rectangle. We adjust the highlight area to a square as well.
        highlightHeight = highlightWidth;
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
      let highlightStyle = highlightWindow.getComputedStyle(highlighter);
      let highlightHeightWithMin = Math.max(highlightHeight, parseFloat(highlightStyle.minHeight));
      let highlightWidthWithMin = Math.max(highlightWidth, parseFloat(highlightStyle.minWidth));
      let offsetX = (targetRect.width - highlightWidthWithMin) / 2;
      let offsetY = (targetRect.height - highlightHeightWithMin) / 2;
      this._addAnnotationPanelMutationObserver(highlighter.parentElement);
      highlighter.parentElement.openPopup(highlightAnchor, "overlap", offsetX, offsetY);
    };

    try {
      await this._ensureTarget(aChromeWindow, aTarget);
      let anchorEl = await this._correctAnchor(aChromeWindow, aTarget);
      showHighlightElement(anchorEl);
    } catch (e) {
      log.warn(e);
    }
  },

  _hideHighlightElement(aWindow) {
    let highlighter = aWindow.document.getElementById("UITourHighlight");
    this._removeAnnotationPanelMutationObserver(highlighter.parentElement);
    highlighter.parentElement.hidePopup();
    highlighter.removeAttribute("active");
  },

  hideHighlight(aWindow) {
    this._hideHighlightElement(aWindow);
    this._setMenuStateForAnnotation(aWindow, false, "appMenu");
    this._setMenuStateForAnnotation(aWindow, false, "pageActionPanel");
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
  async showInfo(aChromeWindow, aAnchor, aTitle = "", aDescription = "",
           aIconURL = "", aButtons = [], aOptions = {}) {
    let showInfoElement = (aAnchorEl) => {
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
        let el = document.createXULElement(isButton ? "button" : "label");
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
    };

    try {
      await this._ensureTarget(aChromeWindow, aAnchor);
      let anchorEl = await this._correctAnchor(aChromeWindow, aAnchor);
      showInfoElement(anchorEl);
    } catch (e) {
      log.warn(e);
    }
  },

  isInfoOnTarget(aChromeWindow, aTargetName) {
    let document = aChromeWindow.document;
    let tooltip = document.getElementById("UITourTooltip");
    return tooltip.getAttribute("targetName") == aTargetName && tooltip.state != "closed";
  },

  _hideInfoElement(aWindow) {
    let document = aWindow.document;
    let tooltip = document.getElementById("UITourTooltip");
    this._removeAnnotationPanelMutationObserver(tooltip);
    tooltip.hidePopup();
    let tooltipButtons = document.getElementById("UITourTooltipButtons");
    while (tooltipButtons.firstChild)
      tooltipButtons.firstChild.remove();
  },

  hideInfo(aWindow) {
    this._hideInfoElement(aWindow);
    this._setMenuStateForAnnotation(aWindow, false, "appMenu");
    this._setMenuStateForAnnotation(aWindow, false, "pageActionPanel");
  },

  showMenu(aWindow, aMenuName, aOpenCallback = null) {
    log.debug("showMenu:", aMenuName);
    function openMenuButton(aMenuBtn) {
      if (!aMenuBtn || !aMenuBtn.hasMenu() || aMenuBtn.open) {
        if (aOpenCallback)
          aOpenCallback();
        return;
      }
      if (aOpenCallback)
        aMenuBtn.addEventListener("popupshown", aOpenCallback, { once: true });
      aMenuBtn.openMenu(true);
    }

    if (aMenuName == "appMenu" || aMenuName == "pageActionPanel") {
      let menu = {
        onPanelHidden: this.onPanelHidden,
      };
      if (aMenuName == "appMenu") {
        menu.node = aWindow.PanelUI.panel;
        menu.onPopupHiding = this.onAppMenuHiding;
        menu.onViewShowing = this.onAppMenuSubviewShowing;
        menu.show = () => aWindow.PanelUI.show();
      } else {
        menu.node = aWindow.BrowserPageActions.panelNode;
        menu.onPopupHiding = this.onPageActionPanelHiding;
        menu.onViewShowing = this.onPageActionPanelSubviewShowing;
        menu.show = () => aWindow.BrowserPageActions.showPanel();
      }

      menu.node.setAttribute("noautohide", "true");
      // If the popup is already opened, don't recreate the widget as it may cause a flicker.
      if (menu.node.state != "open") {
        this.recreatePopup(menu.node);
      }
      if (aOpenCallback) {
        menu.node.addEventListener("popupshown", aOpenCallback, { once: true });
      }
      menu.node.addEventListener("popuphidden", menu.onPanelHidden);
      menu.node.addEventListener("popuphiding", menu.onPopupHiding);
      menu.node.addEventListener("ViewShowing", menu.onViewShowing);
      menu.show();
    } else if (aMenuName == "bookmarks") {
      let menuBtn = aWindow.document.getElementById("bookmarks-menu-button");
      openMenuButton(menuBtn);
    } else if (aMenuName == "controlCenter") {
      let popup = aWindow.gIdentityHandler._identityPopup;

      // Add the listener even if the panel is already open since it will still
      // only get registered once even if it was UITour that opened it.
      popup.addEventListener("popuphiding", this.onControlCenterHiding);
      popup.addEventListener("popuphidden", this.onPanelHidden);

      popup.setAttribute("noautohide", "true");
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
        popup.addEventListener("popupshown", aOpenCallback, { once: true });
      }
      aWindow.document.getElementById("identity-box").click();
    } else if (aMenuName == "pocket") {
      let pageAction = PageActions.actionForID("pocket");
      if (!pageAction) {
        log.error("Can't open the pocket menu without a page action");
        return;
      }
      pageAction.doCommand(aWindow);
    } else if (aMenuName == "urlbar") {
      let urlbar = aWindow.gURLBar;
      let quantumbar = UrlbarPrefs.get("quantumbar");
      if (aOpenCallback) {
        if (quantumbar) {
          urlbar.panel.addEventListener("popupshown", aOpenCallback, { once: true });
        } else {
          urlbar.popup.addEventListener("popupshown", aOpenCallback, { once: true });
        }
      }
      urlbar.focus();
      // To demonstrate the ability of searching, we type "Firefox" in advance
      // for URLBar's dropdown. To limit the search results on browser-related
      // items, we use "Firefox" hard-coded rather than l10n brandShortName
      // entity to avoid unrelated or unpredicted results for, like, Nightly
      // or translated entites.
      const SEARCH_STRING = "Firefox";
      urlbar.value = SEARCH_STRING;
      urlbar.select();
      if (quantumbar) {
        urlbar.startQuery({
          searchString: SEARCH_STRING,
          allowAutofill: false,
        });
      } else {
        urlbar.controller.startSearch(SEARCH_STRING);
      }
    }
  },

  hideMenu(aWindow, aMenuName) {
    log.debug("hideMenu:", aMenuName);
    function closeMenuButton(aMenuBtn) {
      if (aMenuBtn && aMenuBtn.hasMenu()) {
        aMenuBtn.openMenu(false);
      }
    }

    if (aMenuName == "appMenu") {
      aWindow.PanelUI.hide();
    } else if (aMenuName == "bookmarks") {
      let menuBtn = aWindow.document.getElementById("bookmarks-menu-button");
      closeMenuButton(menuBtn);
    } else if (aMenuName == "controlCenter") {
      let panel = aWindow.gIdentityHandler._identityPopup;
      panel.hidePopup();
    } else if (aMenuName == "urlbar") {
      aWindow.gURLBar.closePopup();
    } else if (aMenuName == "pageActionPanel") {
      aWindow.BrowserPageActions.panelNode.hidePopup();
    }
  },

  showNewTab(aWindow, aBrowser) {
    aWindow.gURLBar.focus();
    let url = "about:newtab";
    aWindow.openLinkIn(url, "current", {
      targetBrowser: aBrowser,
      triggeringPrincipal: Services.scriptSecurityManager.createCodebasePrincipal(Services.io.newURI(url), {}),
    });
  },

  _hideAnnotationsForPanel(aEvent, aShouldClosePanel, aTargetPositionCallback) {
    let win = aEvent.target.ownerGlobal;
    let hideHighlightMethod = null;
    let hideInfoMethod = null;
    if (aShouldClosePanel) {
      hideHighlightMethod = aWin => this.hideHighlight(aWin);
      hideInfoMethod = aWin => this.hideInfo(aWin);
    } else {
      // Don't have to close panel, let's only hide annotation elements
      hideHighlightMethod = aWin => this._hideHighlightElement(aWin);
      hideInfoMethod = aWin => this._hideInfoElement(aWin);
    }
    let annotationElements = new Map([
      // [annotationElement (panel), method to hide the annotation]
      [win.document.getElementById("UITourHighlightContainer"), hideHighlightMethod],
      [win.document.getElementById("UITourTooltip"), hideInfoMethod],
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
  },

  onAppMenuHiding(aEvent) {
    UITour._hideAnnotationsForPanel(aEvent, true, UITour.targetIsInAppMenu);
  },

  onAppMenuSubviewShowing(aEvent) {
    UITour._hideAnnotationsForPanel(aEvent, false, UITour.targetIsInAppMenu);
  },

  onPageActionPanelHiding(aEvent) {
    UITour._hideAnnotationsForPanel(aEvent, true, UITour.targetIsInPageActionPanel);
  },

  onPageActionPanelSubviewShowing(aEvent) {
    UITour._hideAnnotationsForPanel(aEvent, false, UITour.targetIsInPageActionPanel);
  },

  onControlCenterHiding(aEvent) {
    UITour._hideAnnotationsForPanel(aEvent, true, (aTarget) => {
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
        this.getAppInfo(aMessageManager, aWindow, aCallbackID);
        break;
      case "availableTargets":
        this.getAvailableTargets(aMessageManager, aWindow, aCallbackID);
        break;
      case "search":
      case "selectedSearchEngine":
        Services.search.getVisibleEngines().then(engines => {
          this.sendPageCallback(aMessageManager, aCallbackID, {
            searchEngineIdentifier: Services.search.defaultEngine.identifier,
            engines: engines.filter(engine => engine.identifier)
                            .map(engine => TARGET_SEARCHENGINE_PREFIX + engine.identifier),
          });
        }).catch(() => {
          this.sendPageCallback(aMessageManager, aCallbackID, {engines: [], searchEngineIdentifier: ""});
        });
        break;
      case "sync":
        this.sendPageCallback(aMessageManager, aCallbackID, {
          setup: Services.prefs.prefHasUserValue("services.sync.username"),
          desktopDevices: Services.prefs.getIntPref("services.sync.clients.devices.desktop", 0),
          mobileDevices: Services.prefs.getIntPref("services.sync.clients.devices.mobile", 0),
          totalDevices: Services.prefs.getIntPref("services.sync.numClients", 0),
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

  getAppInfo(aMessageManager, aWindow, aCallbackID) {
    (async () => {
      let appinfo = {version: Services.appinfo.version};

      // Identifier of the partner repack, as stored in preference "distribution.id"
      // and included in Firefox and other update pings. Note this is not the same as
      // Services.appinfo.distributionID (value of MOZ_DISTRIBUTION_ID is set at build time).
      let distribution =
          Services.prefs.getDefaultBranch("distribution.").getCharPref("id", "default");
      appinfo.distribution = distribution;

      // Update channel, in a way that preserves 'beta' for RC beta builds:
      appinfo.defaultUpdateChannel = UpdateUtils.getUpdateChannel(false /* no partner ID */);

      let isDefaultBrowser = null;
      try {
        let shell = aWindow.getShellService();
        if (shell) {
          isDefaultBrowser = shell.isDefaultBrowser(false);
        }
      } catch (e) {}
      appinfo.defaultBrowser = isDefaultBrowser;

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

      appinfo.canSetDefaultBrowserInBackground =
        canSetDefaultBrowserInBackground;

      // Expose Profile creation and last reset dates in weeks.
      const ONE_WEEK = 7 * 24 * 60 * 60 * 1000;
      let profileAge = await ProfileAge();
      let createdDate = await profileAge.created;
      let resetDate = await profileAge.reset;
      let createdWeeksAgo = Math.floor((Date.now() - createdDate) / ONE_WEEK);
      let resetWeeksAgo = null;
      if (resetDate) {
        resetWeeksAgo = Math.floor((Date.now() - resetDate) / ONE_WEEK);
      }
      appinfo.profileCreatedWeeksAgo = createdWeeksAgo;
      appinfo.profileResetWeeksAgo = resetWeeksAgo;

      this.sendPageCallback(aMessageManager, aCallbackID, appinfo);
    })().catch(err => {
      log.error(err);
      this.sendPageCallback(aMessageManager, aCallbackID, {});
    });
  },

  getAvailableTargets(aMessageManager, aChromeWindow, aCallbackID) {
    (async () => {
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
      let targetObjects = await Promise.all(promises);

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
    })().catch(err => {
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
      Services.search.getVisibleEngines().then(engines => {
        for (let engine of engines) {
          if (engine.identifier == aID) {
            Services.search.setDefault(engine).finally(resolve);
            return;
          }
        }
        reject("selectSearchEngine could not find engine with given ID");
      });
    });
  },

  notify(eventName, params) {
    for (let window of Services.wm.getEnumerator("navigator:browser")) {
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
  },
};
