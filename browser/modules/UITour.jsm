// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

"use strict";

this.EXPORTED_SYMBOLS = ["UITour"];

const {classes: Cc, interfaces: Ci, utils: Cu, results: Cr} = Components;

Cu.import("resource://gre/modules/Services.jsm");
Cu.import("resource://gre/modules/XPCOMUtils.jsm");
Cu.import("resource://gre/modules/Promise.jsm");
Cu.import("resource://gre/modules/Task.jsm");

XPCOMUtils.defineLazyModuleGetter(this, "LightweightThemeManager",
  "resource://gre/modules/LightweightThemeManager.jsm");
XPCOMUtils.defineLazyModuleGetter(this, "PermissionsUtils",
  "resource://gre/modules/PermissionsUtils.jsm");
XPCOMUtils.defineLazyModuleGetter(this, "CustomizableUI",
  "resource:///modules/CustomizableUI.jsm");
XPCOMUtils.defineLazyModuleGetter(this, "UITelemetry",
  "resource://gre/modules/UITelemetry.jsm");
XPCOMUtils.defineLazyModuleGetter(this, "BrowserUITelemetry",
  "resource:///modules/BrowserUITelemetry.jsm");


const UITOUR_PERMISSION   = "uitour";
const PREF_PERM_BRANCH    = "browser.uitour.";
const PREF_SEENPAGEIDS    = "browser.uitour.seenPageIDs";
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


this.UITour = {
  url: null,
  seenPageIDs: null,
  pageIDSourceTabs: new WeakMap(),
  pageIDSourceWindows: new WeakMap(),
  /* Map from browser windows to a set of tabs in which a tour is open */
  originTabs: new WeakMap(),
  /* Map from browser windows to a set of pinned tabs opened by (a) tour(s) */
  pinnedTabs: new WeakMap(),
  urlbarCapture: new WeakMap(),
  appMenuOpenForAnnotation: new Set(),
  availableTargetsCache: new WeakMap(),

  _detachingTab: false,
  _annotationPanelMutationObservers: new WeakMap(),
  _queuedEvents: [],
  _pendingDoc: null,

  highlightEffects: ["random", "wobble", "zoom", "color"],
  targets: new Map([
    ["accountStatus", {
      query: (aDocument) => {
        let statusButton = aDocument.getElementById("PanelUI-fxa-status");
        return aDocument.getAnonymousElementByAttribute(statusButton,
                                                        "class",
                                                        "toolbarbutton-icon");
      },
      widgetName: "PanelUI-fxa-status",
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
    ["help",        {query: "#PanelUI-help"}],
    ["home",        {query: "#home-button"}],
    ["forget", {
      query: "#panic-button",
      widgetName: "panic-button",
      allowAdd: true }],
    ["privateWindow",  {query: "#privatebrowsing-button"}],
    ["quit",        {query: "#PanelUI-quit"}],
    ["search",      {
      query: "#searchbar",
      widgetName: "search-container",
    }],
    ["searchProvider", {
      query: (aDocument) => {
        let searchbar = aDocument.getElementById("searchbar");
        return aDocument.getAnonymousElementByAttribute(searchbar,
                                                        "anonid",
                                                        "searchbar-engine-button");
      },
      widgetName: "search-container",
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
  ]),

  init: function() {
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

  onPageEvent: function(aEvent) {
    let contentDocument = null;
    if (aEvent.target instanceof Ci.nsIDOMHTMLDocument)
      contentDocument = aEvent.target;
    else if (aEvent.target instanceof Ci.nsIDOMHTMLElement)
      contentDocument = aEvent.target.ownerDocument;
    else
      return false;

    // Ignore events if they're not from a trusted origin.
    if (!this.ensureTrustedOrigin(contentDocument))
      return false;

    if (typeof aEvent.detail != "object")
      return false;

    let action = aEvent.detail.action;
    if (typeof action != "string" || !action)
      return false;

    let data = aEvent.detail.data;
    if (typeof data != "object")
      return false;

    let window = this.getChromeWindow(contentDocument);
    // Do this before bailing if there's no tab, so later we can pick up the pieces:
    window.gBrowser.tabContainer.addEventListener("TabSelect", this);
    let tab = window.gBrowser._getTabForContentWindow(contentDocument.defaultView);
    if (!tab) {
      // This should only happen while detaching a tab:
      if (this._detachingTab) {
        this._queuedEvents.push(aEvent);
        this._pendingDoc = Cu.getWeakReference(contentDocument);
        return;
      }
      Cu.reportError("Discarding tabless UITour event (" + action + ") while not detaching a tab." +
                     "This shouldn't happen!");
      return;
    }

    switch (action) {
      case "registerPageID": {
        // This is only relevant if Telemtry is enabled.
        if (!UITelemetry.enabled)
          break;

        // We don't want to allow BrowserUITelemetry.BUCKET_SEPARATOR in the
        // pageID, as it could make parsing the telemetry bucket name difficult.
        if (typeof data.pageID == "string" &&
            !data.pageID.contains(BrowserUITelemetry.BUCKET_SEPARATOR)) {
          this.addSeenPageID(data.pageID);

          // Store tabs and windows separately so we don't need to loop over all
          // tabs when a window is closed.
          this.pageIDSourceTabs.set(tab, data.pageID);
          this.pageIDSourceWindows.set(window, data.pageID);

          this.setTelemetryBucket(data.pageID);
        }
        break;
      }

      case "showHighlight": {
        let targetPromise = this.getTarget(window, data.target);
        targetPromise.then(target => {
          if (!target.node) {
            Cu.reportError("UITour: Target could not be resolved: " + data.target);
            return;
          }
          let effect = undefined;
          if (this.highlightEffects.indexOf(data.effect) !== -1) {
            effect = data.effect;
          }
          this.showHighlight(target, effect);
        }).then(null, Cu.reportError);
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
            Cu.reportError("UITour: Target could not be resolved: " + data.target);
            return;
          }

          let iconURL = null;
          if (typeof data.icon == "string")
            iconURL = this.resolveURL(contentDocument, data.icon);

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
                  button.iconURL = this.resolveURL(contentDocument, buttonData.icon);

                if (typeof buttonData.style == "string")
                  button.style = buttonData.style;

                buttons.push(button);

                if (buttons.length == MAX_BUTTONS)
                  break;
              }
            }
          }

          let infoOptions = {};

          if (typeof data.closeButtonCallbackID == "string")
            infoOptions.closeButtonCallbackID = data.closeButtonCallbackID;
          if (typeof data.targetCallbackID == "string")
            infoOptions.targetCallbackID = data.targetCallbackID;

          this.showInfo(contentDocument, target, data.title, data.text, iconURL, buttons, infoOptions);
        }).then(null, Cu.reportError);
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

      case "addPinnedTab": {
        this.ensurePinnedTab(window, true);
        break;
      }

      case "removePinnedTab": {
        this.removePinnedTab(window);
        break;
      }

      case "showMenu": {
        this.showMenu(window, data.name, () => {
          if (typeof data.showCallbackID == "string")
            this.sendPageCallback(contentDocument, data.showCallbackID);
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
          return false;
        }

        let uri = null;
        try {
          uri = Services.io.newURI(data.url, null, null);
        } catch (e) {
          return false;
        }

        let secman = Services.scriptSecurityManager;
        let principal = contentDocument.nodePrincipal;
        let flags = secman.DISALLOW_INHERIT_PRINCIPAL;
        try {
          secman.checkLoadURIWithPrincipal(principal, uri, flags);
        } catch (e) {
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
          return false;
        }

        this.getConfiguration(contentDocument, data.configuration, data.callbackID);
        break;
      }

      case "showFirefoxAccounts": {
        // 'signup' is the only action that makes sense currently, so we don't
        // accept arbitrary actions just to be safe...
        // We want to replace the current tab.
        contentDocument.location.href = "about:accounts?action=signup";
        break;
      }

      case "addNavBarWidget": {
        // Add a widget to the toolbar
        let targetPromise = this.getTarget(window, data.name);
        targetPromise.then(target => {
          this.addNavBarWidget(target, contentDocument, data.callbackID);
        }).then(null, Cu.reportError);
        break;
      }
    }

    if (!this.originTabs.has(window))
      this.originTabs.set(window, new Set());

    this.originTabs.get(window).add(tab);
    tab.addEventListener("TabClose", this);
    tab.addEventListener("TabBecomingWindow", this);
    window.addEventListener("SSWindowClosing", this);

    return true;
  },

  handleEvent: function(aEvent) {
    switch (aEvent.type) {
      case "pagehide": {
        let window = this.getChromeWindow(aEvent.target);
        this.teardownTour(window);
        break;
      }

      case "TabBecomingWindow":
        this._detachingTab = true;
        // Fall through
      case "TabClose": {
        let tab = aEvent.target;
        if (this.pageIDSourceTabs.has(tab)) {
          let pageID = this.pageIDSourceTabs.get(tab);

          // Delete this from the window cache, so if the window is closed we
          // don't expire this page ID twice.
          let window = tab.ownerDocument.defaultView;
          if (this.pageIDSourceWindows.get(window) == pageID)
            this.pageIDSourceWindows.delete(window);

          this.setExpiringTelemetryBucket(pageID, "closed");
        }

        let window = tab.ownerDocument.defaultView;
        this.teardownTour(window);
        break;
      }

      case "TabSelect": {
        if (aEvent.detail && aEvent.detail.previousTab) {
          let previousTab = aEvent.detail.previousTab;

          if (this.pageIDSourceTabs.has(previousTab)) {
            let pageID = this.pageIDSourceTabs.get(previousTab);
            this.setExpiringTelemetryBucket(pageID, "inactive");
          }
        }

        let window = aEvent.target.ownerDocument.defaultView;
        let selectedTab = window.gBrowser.selectedTab;
        let pinnedTab = this.pinnedTabs.get(window);
        if (pinnedTab && pinnedTab.tab == selectedTab)
          break;
        let originTabs = this.originTabs.get(window);
        if (originTabs && originTabs.has(selectedTab))
          break;

        let pendingDoc;
        if (this._detachingTab && this._pendingDoc && (pendingDoc = this._pendingDoc.get())) {
          if (selectedTab.linkedBrowser.contentDocument == pendingDoc) {
            if (!this.originTabs.get(window)) {
              this.originTabs.set(window, new Set());
            }
            this.originTabs.get(window).add(selectedTab);
            this.pendingDoc = null;
            this._detachingTab = false;
            while (this._queuedEvents.length) {
              try {
                this.onPageEvent(this._queuedEvents.shift());
              } catch (ex) {
                Cu.reportError(ex);
              }
            }
            break;
          }
        }

        this.teardownTour(window);
        break;
      }

      case "SSWindowClosing": {
        let window = aEvent.target;
        if (this.pageIDSourceWindows.has(window)) {
          let pageID = this.pageIDSourceWindows.get(window);
          this.setExpiringTelemetryBucket(pageID, "closed");
        }

        this.teardownTour(window, true);
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

  teardownTour: function(aWindow, aWindowClosing = false) {
    aWindow.gBrowser.tabContainer.removeEventListener("TabSelect", this);
    aWindow.PanelUI.panel.removeEventListener("popuphiding", this.hidePanelAnnotations);
    aWindow.PanelUI.panel.removeEventListener("ViewShowing", this.hidePanelAnnotations);
    aWindow.removeEventListener("SSWindowClosing", this);

    let originTabs = this.originTabs.get(aWindow);
    if (originTabs) {
      for (let tab of originTabs) {
        tab.removeEventListener("TabClose", this);
        tab.removeEventListener("TabBecomingWindow", this);
      }
    }
    this.originTabs.delete(aWindow);

    if (!aWindowClosing) {
      this.hideHighlight(aWindow);
      this.hideInfo(aWindow);
      // Ensure the menu panel is hidden before calling recreatePopup so popup events occur.
      this.hideMenu(aWindow, "appMenu");
    }

    this.endUrlbarCapture(aWindow);
    this.removePinnedTab(aWindow);
    this.resetTheme();
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

  importPermissions: function() {
    try {
      PermissionsUtils.importFromPrefs(PREF_PERM_BRANCH, UITOUR_PERMISSION);
    } catch (e) {
      Cu.reportError(e);
    }
  },

  ensureTrustedOrigin: function(aDocument) {
    if (aDocument.defaultView.top != aDocument.defaultView)
      return false;

    let uri = aDocument.documentURIObject;

    if (uri.schemeIs("chrome"))
      return true;

    if (!this.isSafeScheme(uri))
      return false;

    this.importPermissions();
    let permission = Services.perms.testPermission(uri, UITOUR_PERMISSION);
    return permission == Services.perms.ALLOW_ACTION;
  },

  isSafeScheme: function(aURI) {
    let allowedSchemes = new Set(["https", "about"]);
    if (!Services.prefs.getBoolPref("browser.uitour.requireSecure"))
      allowedSchemes.add("http");

    if (!allowedSchemes.has(aURI.scheme))
      return false;

    return true;
  },

  resolveURL: function(aDocument, aURL) {
    try {
      let uri = Services.io.newURI(aURL, null, aDocument.documentURIObject);

      if (!this.isSafeScheme(uri))
        return null;

      return uri.spec;
    } catch (e) {}

    return null;
  },

  sendPageCallback: function(aDocument, aCallbackID, aData = {}) {

    let detail = {data: aData, callbackID: aCallbackID};
    detail = Cu.cloneInto(detail, aDocument.defaultView);
    let event = new aDocument.defaultView.CustomEvent("mozUITourResponse", {
      bubbles: true,
      detail: detail
    });

    aDocument.dispatchEvent(event);
  },

  isElementVisible: function(aElement) {
    let targetStyle = aElement.ownerDocument.defaultView.getComputedStyle(aElement);
    return (targetStyle.display != "none" && targetStyle.visibility == "visible");
  },

  getTarget: function(aWindow, aTargetName, aSticky = false) {
    let deferred = Promise.defer();
    if (typeof aTargetName != "string" || !aTargetName) {
      deferred.reject("Invalid target name specified");
      return deferred.promise;
    }

    if (aTargetName == "pinnedTab") {
      deferred.resolve({
          targetName: aTargetName,
          node: this.ensurePinnedTab(aWindow, aSticky)
      });
      return deferred.promise;
    }

    if (aTargetName.startsWith(TARGET_SEARCHENGINE_PREFIX)) {
      let engineID = aTargetName.slice(TARGET_SEARCHENGINE_PREFIX.length);
      return this.getSearchEngineTarget(aWindow, engineID);
    }

    let targetObject = this.targets.get(aTargetName);
    if (!targetObject) {
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
          node = null;
        }
      } else {
        node = aWindow.document.querySelector(targetQuery);
      }

      deferred.resolve({
        addTargetListener: targetObject.addTargetListener,
        node: node,
        removeTargetListener: targetObject.removeTargetListener,
        targetName: aTargetName,
        widgetName: targetObject.widgetName,
        allowAdd: targetObject.allowAdd,
      });
    }).then(null, Cu.reportError);
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
    // If the panel is in the desired state, we're done.
    let panelIsOpen = aWindow.PanelUI.panel.state != "closed";
    if (aShouldOpenForHighlight == panelIsOpen) {
      if (aCallback)
        aCallback();
      return;
    }

    // Don't close the menu if it wasn't opened by us (e.g. via showmenu instead).
    if (!aShouldOpenForHighlight && !this.appMenuOpenForAnnotation.has(aAnnotationType)) {
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
      this.showMenu(aWindow, "appMenu", aCallback);
    } else {
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

  ensurePinnedTab: function(aWindow, aSticky = false) {
    let tabInfo = this.pinnedTabs.get(aWindow);

    if (tabInfo) {
      tabInfo.sticky = tabInfo.sticky || aSticky;
    } else {
      let url = Services.urlFormatter.formatURLPref("browser.uitour.pinnedTabUrl");

      let tab = aWindow.gBrowser.addTab(url);
      aWindow.gBrowser.pinTab(tab);
      tab.addEventListener("TabClose", () => {
        this.pinnedTabs.delete(aWindow);
      });

      tabInfo = {
        tab: tab,
        sticky: aSticky
      };
      this.pinnedTabs.set(aWindow, tabInfo);
    }

    return tabInfo.tab;
  },

  removePinnedTab: function(aWindow) {
    let tabInfo = this.pinnedTabs.get(aWindow);
    if (tabInfo)
      aWindow.gBrowser.removeTab(tabInfo.tab);
  },

  /**
   * @param aTarget    The element to highlight.
   * @param aEffect    (optional) The effect to use from UITour.highlightEffects or "none".
   * @see UITour.highlightEffects
   */
  showHighlight: function(aTarget, aEffect = "none") {
    let window = aTarget.node.ownerDocument.defaultView;

    function showHighlightPanel() {
      if (aTarget.targetName.startsWith(TARGET_SEARCHENGINE_PREFIX)) {
        // This won't affect normal higlights done via the panel, so we need to
        // manually hide those.
        this.hideHighlight(window);
        aTarget.node.setAttribute("_moz-menuactive", true);
        return;
      }

      // Conversely, highlights for search engines are highlighted via CSS
      // rather than a panel, so need to be manually removed.
      this._hideSearchEngineHighlight(window);

      let highlighter = aTarget.node.ownerDocument.getElementById("UITourHighlight");

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
      aTarget.node.ownerDocument.defaultView.getComputedStyle(highlighter).animationName;
      highlighter.setAttribute("active", effect);
      highlighter.parentElement.setAttribute("targetName", aTarget.targetName);
      highlighter.parentElement.hidden = false;

      let targetRect = aTarget.node.getBoundingClientRect();
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
        highlighter.parentElement.hidePopup();
      }
      /* The "overlap" position anchors from the top-left but we want to centre highlights at their
         minimum size. */
      let highlightWindow = aTarget.node.ownerDocument.defaultView;
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
      highlighter.parentElement.openPopup(aTarget.node, "overlap", offsetX, offsetY);
    }

    // Prevent showing a panel at an undefined position.
    if (!this.isElementVisible(aTarget.node))
      return;

    this._setAppMenuStateForAnnotation(aTarget.node.ownerDocument.defaultView, "highlight",
                                       this.targetIsInAppMenu(aTarget),
                                       showHighlightPanel.bind(this));
  },

  hideHighlight: function(aWindow) {
    let tabData = this.pinnedTabs.get(aWindow);
    if (tabData && !tabData.sticky)
      this.removePinnedTab(aWindow);

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
   * @param {Document} aContentDocument
   * @param {Node}     aAnchor
   * @param {String}   [aTitle=""]
   * @param {String}   [aDescription=""]
   * @param {String}   [aIconURL=""]
   * @param {Object[]} [aButtons=[]]
   * @param {Object}   [aOptions={}]
   * @param {String}   [aOptions.closeButtonCallbackID]
   */
  showInfo: function(aContentDocument, aAnchor, aTitle = "", aDescription = "", aIconURL = "",
                     aButtons = [], aOptions = {}) {
    function showInfoPanel(aAnchorEl) {
      aAnchorEl.focus();

      let document = aAnchorEl.ownerDocument;
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
          this.sendPageCallback(aContentDocument, callbackID);
        });

        tooltipButtons.appendChild(el);
      }

      tooltipButtons.hidden = !aButtons.length;

      let tooltipClose = document.getElementById("UITourTooltipClose");
      let closeButtonCallback = (event) => {
        this.hideInfo(document.defaultView);
        if (aOptions && aOptions.closeButtonCallbackID)
          this.sendPageCallback(aContentDocument, aOptions.closeButtonCallbackID);
      };
      tooltipClose.addEventListener("command", closeButtonCallback);

      let targetCallback = (event) => {
        let details = {
          target: aAnchor.targetName,
          type: event.type,
        };
        this.sendPageCallback(aContentDocument, aOptions.targetCallbackID, details);
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
      this._addAnnotationPanelMutationObserver(tooltip);
      tooltip.openPopup(aAnchorEl, alignment);
    }

    // Prevent showing a panel at an undefined position.
    if (!this.isElementVisible(aAnchor.node))
      return;

    // Due to a platform limitation, we can't anchor a panel to an element in a
    // <menupopup>. So we can't support showing info panels for search engines.
    if (aAnchor.targetName.startsWith(TARGET_SEARCHENGINE_PREFIX))
      return;

    this._setAppMenuStateForAnnotation(aAnchor.node.ownerDocument.defaultView, "info",
                                       this.targetIsInAppMenu(aAnchor),
                                       showInfoPanel.bind(this, aAnchor.node));
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
    function openMenuButton(aMenuBtn) {
      if (!aMenuBtn || !aMenuBtn.boxObject || aMenuBtn.open) {
        if (aOpenCallback)
          aOpenCallback();
        return;
      }
      if (aOpenCallback)
        aMenuBtn.addEventListener("popupshown", onPopupShown);
      aMenuBtn.boxObject.QueryInterface(Ci.nsIMenuBoxObject).openMenu(true);
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
      aWindow.PanelUI.panel.addEventListener("popuphiding", this.hidePanelAnnotations);
      aWindow.PanelUI.panel.addEventListener("ViewShowing", this.hidePanelAnnotations);
      if (aOpenCallback) {
        aWindow.PanelUI.panel.addEventListener("popupshown", onPopupShown);
      }
      aWindow.PanelUI.show();
    } else if (aMenuName == "bookmarks") {
      let menuBtn = aWindow.document.getElementById("bookmarks-menu-button");
      openMenuButton(menuBtn);
    } else if (aMenuName == "searchEngines") {
      this.getTarget(aWindow, "searchProvider").then(target => {
        openMenuButton(target.node);
      }).catch(Cu.reportError);
    }
  },

  hideMenu: function(aWindow, aMenuName) {
    function closeMenuButton(aMenuBtn) {
      if (aMenuBtn && aMenuBtn.boxObject)
        aMenuBtn.boxObject.QueryInterface(Ci.nsIMenuBoxObject).openMenu(false);
    }

    if (aMenuName == "appMenu") {
      aWindow.PanelUI.panel.removeAttribute("noautohide");
      aWindow.PanelUI.hide();
      this.recreatePopup(aWindow.PanelUI.panel);
    } else if (aMenuName == "bookmarks") {
      let menuBtn = aWindow.document.getElementById("bookmarks-menu-button");
      closeMenuButton(menuBtn);
    } else if (aMenuName == "searchEngines") {
      let menuBtn = this.targets.get("searchProvider").query(aWindow.document);
      closeMenuButton(menuBtn);
    }
  },

  hidePanelAnnotations: function(aEvent) {
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
              !UITour.targetIsInAppMenu(aTarget)) {
            return;
          }
          hideMethod(win);
        }).then(null, Cu.reportError);
      }
    });
    UITour.appMenuOpenForAnnotation.clear();
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

  getConfiguration: function(aContentDocument, aConfiguration, aCallbackID) {
    switch (aConfiguration) {
      case "availableTargets":
        this.getAvailableTargets(aContentDocument, aCallbackID);
        break;
      case "sync":
        this.sendPageCallback(aContentDocument, aCallbackID, {
          setup: Services.prefs.prefHasUserValue("services.sync.username"),
        });
        break;
      default:
        Cu.reportError("getConfiguration: Unknown configuration requested: " + aConfiguration);
        break;
    }
  },

  getAvailableTargets: function(aContentDocument, aCallbackID) {
    Task.spawn(function*() {
      let window = this.getChromeWindow(aContentDocument);
      let data = this.availableTargetsCache.get(window);
      if (data) {
        this.sendPageCallback(aContentDocument, aCallbackID, data);
        return;
      }

      let promises = [];
      for (let targetName of this.targets.keys()) {
        promises.push(this.getTarget(window, targetName));
      }
      let targetObjects = yield Promise.all(promises);

      let targetNames = [
        "pinnedTab",
      ];

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
      this.sendPageCallback(aContentDocument, aCallbackID, data);
    }.bind(this)).catch(err => {
      Cu.reportError(err);
      this.sendPageCallback(aContentDocument, aCallbackID, {
        targets: [],
      });
    });
  },

  addNavBarWidget: function (aTarget, aContentDocument, aCallbackID) {
    if (aTarget.node) {
      Cu.reportError("UITour: can't add a widget already present: " + data.target);
      return;
    }
    if (!aTarget.allowAdd) {
      Cu.reportError("UITour: not allowed to add this widget: " + data.target);
      return;
    }
    if (!aTarget.widgetName) {
      Cu.reportError("UITour: can't add a widget without a widgetName property: " + data.target);
      return;
    }

    CustomizableUI.addWidgetToArea(aTarget.widgetName, CustomizableUI.AREA_NAVBAR);
    this.sendPageCallback(aContentDocument, aCallbackID);
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

  getAvailableSearchEngineTargets: function(aWindow) {
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
  getSearchEngineTarget: function(aWindow, aIdentifier) {
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
  }
};

this.UITour.init();
