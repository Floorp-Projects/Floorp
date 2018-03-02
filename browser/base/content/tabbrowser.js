/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/* eslint-env mozilla/browser-window */

class TabBrowser {
  constructor(container) {
    this.container = container;
    this.requiresAddonInterpositions = true;

    // Pass along any used DOM methods to the container node. When this object turns
    // into a custom element this won't be needed anymore.
    this.addEventListener = this.container.addEventListener.bind(this.container);
    this.removeEventListener = this.container.removeEventListener.bind(this.container);
    this.dispatchEvent = this.container.dispatchEvent.bind(this.container);
    this.getAttribute = this.container.getAttribute.bind(this.container);
    this.hasAttribute = this.container.hasAttribute.bind(this.container);
    this.setAttribute = this.container.setAttribute.bind(this.container);
    this.removeAttribute = this.container.removeAttribute.bind(this.container);
    this.appendChild = this.container.appendChild.bind(this.container);
    this.ownerGlobal = this.container.ownerGlobal;
    this.ownerDocument = this.container.ownerDocument;
    this.namespaceURI = this.container.namespaceURI;
    this.style = this.container.style;

    XPCOMUtils.defineLazyServiceGetters(this, {
      _unifiedComplete: ["@mozilla.org/autocomplete/search;1?name=unifiedcomplete", "mozIPlacesAutoComplete"],
      serializationHelper: ["@mozilla.org/network/serialization-helper;1", "nsISerializationHelper"],
      mURIFixup: ["@mozilla.org/docshell/urifixup;1", "nsIURIFixup"],
    });

    XPCOMUtils.defineLazyGetter(this, "initialBrowser", () => {
      return document.getAnonymousElementByAttribute(this.container, "anonid", "initialBrowser");
    });
    XPCOMUtils.defineLazyGetter(this, "tabContainer", () => {
      return document.getElementById(this.getAttribute("tabcontainer"));
    });
    XPCOMUtils.defineLazyGetter(this, "tabs", () => {
      return this.tabContainer.childNodes;
    });
    XPCOMUtils.defineLazyGetter(this, "tabbox", () => {
      return document.getAnonymousElementByAttribute(this.container, "anonid", "tabbox");
    });
    XPCOMUtils.defineLazyGetter(this, "mPanelContainer", () => {
      return document.getAnonymousElementByAttribute(this.container, "anonid", "panelcontainer");
    });

    this.closingTabsEnum = { ALL: 0, OTHER: 1, TO_END: 2 };

    this._visibleTabs = null;

    this.mCurrentTab = null;

    this._lastRelatedTabMap = new WeakMap();

    this.mCurrentBrowser = null;

    this.mProgressListeners = [];

    this.mTabsProgressListeners = [];

    this._tabListeners = new Map();

    this._tabFilters = new Map();

    this.mIsBusy = false;

    this._outerWindowIDBrowserMap = new Map();

    this.arrowKeysShouldWrap = AppConstants == "macosx";

    this._autoScrollPopup = null;

    this._previewMode = false;

    this._lastFindValue = "";

    this._contentWaitingCount = 0;

    this.tabAnimationsInProgress = 0;

    /**
     * Binding from browser to tab
     */
    this._tabForBrowser = new WeakMap();

    /**
     * Holds a unique ID for the tab change that's currently being timed.
     * Used to make sure that multiple, rapid tab switches do not try to
     * create overlapping timers.
     */
    this._tabSwitchID = null;

    this._preloadedBrowser = null;

    /**
     * `_createLazyBrowser` will define properties on the unbound lazy browser
     * which correspond to properties defined in XBL which will be bound to
     * the browser when it is inserted into the document.  If any of these
     * properties are accessed by consumers, `_insertBrowser` is called and
     * the browser is inserted to ensure that things don't break.  This list
     * provides the names of properties that may be called while the browser
     * is in its unbound (lazy) state.
     */
    this._browserBindingProperties = [
      "canGoBack", "canGoForward", "goBack", "goForward", "permitUnload",
      "reload", "reloadWithFlags", "stop", "loadURI", "loadURIWithFlags",
      "goHome", "homePage", "gotoIndex", "currentURI", "documentURI",
      "preferences", "imageDocument", "isRemoteBrowser", "messageManager",
      "getTabBrowser", "finder", "fastFind", "sessionHistory", "contentTitle",
      "characterSet", "fullZoom", "textZoom", "webProgress",
      "addProgressListener", "removeProgressListener", "audioPlaybackStarted",
      "audioPlaybackStopped", "pauseMedia", "stopMedia",
      "resumeMedia", "mute", "unmute", "blockedPopups", "lastURI",
      "purgeSessionHistory", "stopScroll", "startScroll",
      "userTypedValue", "userTypedClear", "mediaBlocked",
      "didStartLoadSinceLastUserTyping"
    ];

    this._removingTabs = [];

    /**
     * Tab close requests are ignored if the window is closing anyway,
     * e.g. when holding Ctrl+W.
     */
    this._windowIsClosing = false;

    // This defines a proxy which allows us to access browsers by
    // index without actually creating a full array of browsers.
    this.browsers = new Proxy([], {
      has: (target, name) => {
        if (typeof name == "string" && Number.isInteger(parseInt(name))) {
          return (name in this.tabs);
        }
        return false;
      },
      get: (target, name) => {
        if (name == "length") {
          return this.tabs.length;
        }
        if (typeof name == "string" && Number.isInteger(parseInt(name))) {
          if (!(name in this.tabs)) {
            return undefined;
          }
          return this.tabs[name].linkedBrowser;
        }
        return target[name];
      }
    });

    /**
     * List of browsers whose docshells must be active in order for print preview
     * to work.
     */
    this._printPreviewBrowsers = new Set();

    this._switcher = null;

    this._tabMinWidthLimit = 50;

    this._soundPlayingAttrRemovalTimer = 0;

    this._hoverTabTimer = null;

    this.mCurrentBrowser = document.getAnonymousElementByAttribute(this.container, "anonid", "initialBrowser");
    this.mCurrentBrowser.permanentKey = {};

    CustomizableUI.addListener(this);
    this._updateNewTabVisibility();

    Services.obs.addObserver(this, "contextual-identity-updated");

    this.mCurrentTab = this.tabContainer.firstChild;
    const nsIEventListenerService =
      Ci.nsIEventListenerService;
    let els = Cc["@mozilla.org/eventlistenerservice;1"]
      .getService(nsIEventListenerService);
    els.addSystemEventListener(document, "keydown", this, false);
    if (AppConstants.platform == "macosx") {
      els.addSystemEventListener(document, "keypress", this, false);
    }
    window.addEventListener("sizemodechange", this);
    window.addEventListener("occlusionstatechange", this);

    var uniqueId = this._generateUniquePanelID();
    this.mPanelContainer.childNodes[0].id = uniqueId;
    this.mCurrentTab.linkedPanel = uniqueId;
    this.mCurrentTab.permanentKey = this.mCurrentBrowser.permanentKey;
    this.mCurrentTab._tPos = 0;
    this.mCurrentTab._fullyOpen = true;
    this.mCurrentTab.linkedBrowser = this.mCurrentBrowser;
    this._tabForBrowser.set(this.mCurrentBrowser, this.mCurrentTab);

    // set up the shared autoscroll popup
    this._autoScrollPopup = this.mCurrentBrowser._createAutoScrollPopup();
    this._autoScrollPopup.id = "autoscroller";
    this.appendChild(this._autoScrollPopup);
    this.mCurrentBrowser.setAttribute("autoscrollpopup", this._autoScrollPopup.id);
    this.mCurrentBrowser.droppedLinkHandler = handleDroppedLink;

    // Hook up the event listeners to the first browser
    var tabListener = new TabProgressListener(this.mCurrentTab, this.mCurrentBrowser, true, false);
    const nsIWebProgress = Ci.nsIWebProgress;
    const filter = Cc["@mozilla.org/appshell/component/browser-status-filter;1"]
      .createInstance(nsIWebProgress);
    filter.addProgressListener(tabListener, nsIWebProgress.NOTIFY_ALL);
    this._tabListeners.set(this.mCurrentTab, tabListener);
    this._tabFilters.set(this.mCurrentTab, filter);
    this.webProgress.addProgressListener(filter, nsIWebProgress.NOTIFY_ALL);

    if (Services.prefs.getBoolPref("browser.display.use_system_colors"))
      this.style.backgroundColor = "-moz-default-background-color";
    else if (Services.prefs.getIntPref("browser.display.document_color_use") == 2)
      this.style.backgroundColor = Services.prefs.getCharPref("browser.display.background_color");

    let messageManager = window.getGroupMessageManager("browsers");

    let remote = window.QueryInterface(Ci.nsIInterfaceRequestor)
                       .getInterface(Ci.nsIWebNavigation)
                       .QueryInterface(Ci.nsILoadContext)
                       .useRemoteTabs;
    if (remote) {
      messageManager.addMessageListener("DOMTitleChanged", this);
      messageManager.addMessageListener("DOMWindowClose", this);
      window.messageManager.addMessageListener("contextmenu", this);
      messageManager.addMessageListener("Browser:Init", this);

      // If this window has remote tabs, switch to our tabpanels fork
      // which does asynchronous tab switching.
      this.mPanelContainer.classList.add("tabbrowser-tabpanels");
    } else {
      this._outerWindowIDBrowserMap.set(this.mCurrentBrowser.outerWindowID,
        this.mCurrentBrowser);
    }
    messageManager.addMessageListener("DOMWindowFocus", this);
    messageManager.addMessageListener("RefreshBlocker:Blocked", this);
    messageManager.addMessageListener("Browser:WindowCreated", this);

    // To correctly handle keypresses for potential FindAsYouType, while
    // the tab's find bar is not yet initialized.
    this._findAsYouType = Services.prefs.getBoolPref("accessibility.typeaheadfind");
    Services.prefs.addObserver("accessibility.typeaheadfind", this);
    messageManager.addMessageListener("Findbar:Keypress", this);

    XPCOMUtils.defineLazyPreferenceGetter(this, "animationsEnabled",
      "toolkit.cosmeticAnimations.enabled", true);
    XPCOMUtils.defineLazyPreferenceGetter(this, "schedulePressureDefaultCount",
      "browser.schedulePressure.defaultCount", 3);
    XPCOMUtils.defineLazyPreferenceGetter(this, "tabWarmingEnabled",
      "browser.tabs.remote.warmup.enabled", false);
    XPCOMUtils.defineLazyPreferenceGetter(this, "tabWarmingMax",
      "browser.tabs.remote.warmup.maxTabs", 3);
    XPCOMUtils.defineLazyPreferenceGetter(this, "tabWarmingUnloadDelay" /* ms */,
      "browser.tabs.remote.warmup.unloadDelayMs", 2000);
    XPCOMUtils.defineLazyPreferenceGetter(this, "tabMinWidthPref",
      "browser.tabs.tabMinWidth", this._tabMinWidthLimit,
      (pref, prevValue, newValue) => this.tabMinWidth = newValue,
      newValue => Math.max(newValue, this._tabMinWidthLimit),
    );

    this.tabMinWidth = this.tabMinWidthPref;

    this._setupEventListeners();
  }

  get tabContextMenu() {
    return this.tabContainer.contextMenu;
  }

  get visibleTabs() {
    if (!this._visibleTabs)
      this._visibleTabs = Array.filter(this.tabs,
        tab => !tab.hidden && !tab.closing);
    return this._visibleTabs;
  }

  get _numPinnedTabs() {
    for (var i = 0; i < this.tabs.length; i++) {
      if (!this.tabs[i].pinned)
        break;
    }
    return i;
  }

  get popupAnchor() {
    if (this.mCurrentTab._popupAnchor) {
      return this.mCurrentTab._popupAnchor;
    }
    let stack = this.mCurrentBrowser.parentNode;
    // Create an anchor for the popup
    const NS_XUL = "http://www.mozilla.org/keymaster/gatekeeper/there.is.only.xul";
    let popupAnchor = document.createElementNS(NS_XUL, "hbox");
    popupAnchor.className = "popup-anchor";
    popupAnchor.hidden = true;
    stack.appendChild(popupAnchor);
    return this.mCurrentTab._popupAnchor = popupAnchor;
  }

  set selectedTab(val) {
    if (gNavToolbox.collapsed && !this._allowTabChange) {
      return this.tabbox.selectedTab;
    }
    // Update the tab
    this.tabbox.selectedTab = val;
    return val;
  }

  get selectedTab() {
    return this.mCurrentTab;
  }

  get selectedBrowser() {
    return this.mCurrentBrowser;
  }

  /**
   * BEGIN FORWARDED BROWSER PROPERTIES.  IF YOU ADD A PROPERTY TO THE BROWSER ELEMENT
   * MAKE SURE TO ADD IT HERE AS WELL.
   */
  get canGoBack() {
    return this.mCurrentBrowser.canGoBack;
  }

  get canGoForward() {
    return this.mCurrentBrowser.canGoForward;
  }

  goBack() {
    return this.mCurrentBrowser.goBack();
  }

  goForward() {
    return this.mCurrentBrowser.goForward();
  }

  reload() {
    return this.mCurrentBrowser.reload();
  }

  reloadWithFlags(aFlags) {
    return this.mCurrentBrowser.reloadWithFlags(aFlags);
  }

  stop() {
    return this.mCurrentBrowser.stop();
  }

  /**
   * throws exception for unknown schemes
   */
  loadURI(aURI, aReferrerURI, aCharset) {
    return this.mCurrentBrowser.loadURI(aURI, aReferrerURI, aCharset);
  }

  /**
   * throws exception for unknown schemes
   */
  loadURIWithFlags(aURI, aFlags, aReferrerURI, aCharset, aPostData) {
    // Note - the callee understands both:
    // (a) loadURIWithFlags(aURI, aFlags, ...)
    // (b) loadURIWithFlags(aURI, { flags: aFlags, ... })
    // Forwarding it as (a) here actually supports both (a) and (b),
    // so you can call us either way too.
    return this.mCurrentBrowser.loadURIWithFlags(aURI, aFlags, aReferrerURI, aCharset, aPostData);
  }

  goHome() {
    return this.mCurrentBrowser.goHome();
  }

  gotoIndex(aIndex) {
    return this.mCurrentBrowser.gotoIndex(aIndex);
  }

  set homePage(val) {
    this.mCurrentBrowser.homePage = val;
    return val;
  }

  get homePage() {
    return this.mCurrentBrowser.homePage;
  }

  get currentURI() {
    return this.mCurrentBrowser.currentURI;
  }

  get finder() {
    return this.mCurrentBrowser.finder;
  }

  get docShell() {
    return this.mCurrentBrowser.docShell;
  }

  get webNavigation() {
    return this.mCurrentBrowser.webNavigation;
  }

  get webBrowserFind() {
    return this.mCurrentBrowser.webBrowserFind;
  }

  get webProgress() {
    return this.mCurrentBrowser.webProgress;
  }

  get contentWindow() {
    return this.mCurrentBrowser.contentWindow;
  }

  get contentWindowAsCPOW() {
    return this.mCurrentBrowser.contentWindowAsCPOW;
  }

  get sessionHistory() {
    return this.mCurrentBrowser.sessionHistory;
  }

  get markupDocumentViewer() {
    return this.mCurrentBrowser.markupDocumentViewer;
  }

  get contentDocument() {
    return this.mCurrentBrowser.contentDocument;
  }

  get contentDocumentAsCPOW() {
    return this.mCurrentBrowser.contentDocumentAsCPOW;
  }

  get contentTitle() {
    return this.mCurrentBrowser.contentTitle;
  }

  get contentPrincipal() {
    return this.mCurrentBrowser.contentPrincipal;
  }

  get securityUI() {
    return this.mCurrentBrowser.securityUI;
  }

  set fullZoom(val) {
    this.mCurrentBrowser.fullZoom = val;
  }

  get fullZoom() {
    return this.mCurrentBrowser.fullZoom;
  }

  set textZoom(val) {
    this.mCurrentBrowser.textZoom = val;
  }

  get textZoom() {
    return this.mCurrentBrowser.textZoom;
  }

  get isSyntheticDocument() {
    return this.mCurrentBrowser.isSyntheticDocument;
  }

  set userTypedValue(val) {
    return this.mCurrentBrowser.userTypedValue = val;
  }

  get userTypedValue() {
    return this.mCurrentBrowser.userTypedValue;
  }

  set tabMinWidth(val) {
    this.tabContainer.style.setProperty("--tab-min-width", val + "px");
    return val;
  }

  isFindBarInitialized(aTab) {
    return (aTab || this.selectedTab)._findBar != undefined;
  }

  getFindBar(aTab) {
    if (!aTab)
      aTab = this.selectedTab;

    if (aTab._findBar)
      return aTab._findBar;

    let findBar = document.createElementNS(this.namespaceURI, "findbar");
    let browser = this.getBrowserForTab(aTab);
    let browserContainer = this.getBrowserContainer(browser);
    browserContainer.appendChild(findBar);

    // Force a style flush to ensure that our binding is attached.
    findBar.clientTop;

    findBar.browser = browser;
    findBar._findField.value = this._lastFindValue;

    aTab._findBar = findBar;

    let event = document.createEvent("Events");
    event.initEvent("TabFindInitialized", true, false);
    aTab.dispatchEvent(event);

    return findBar;
  }

  getStatusPanel() {
    if (!this._statusPanel) {
      this._statusPanel = document.createElementNS(this.namespaceURI, "statuspanel");
      this._statusPanel.setAttribute("inactive", "true");
      this._statusPanel.setAttribute("layer", "true");
      this._appendStatusPanel();
    }
    return this._statusPanel;
  }

  _appendStatusPanel() {
    if (this._statusPanel) {
      let browser = this.selectedBrowser;
      let browserContainer = this.getBrowserContainer(browser);
      browserContainer.insertBefore(this._statusPanel, browser.parentNode.nextSibling);
    }
  }

  pinTab(aTab) {
    if (aTab.pinned)
      return;

    if (aTab.hidden)
      this.showTab(aTab);

    this.moveTabTo(aTab, this._numPinnedTabs);
    aTab.setAttribute("pinned", "true");
    this.tabContainer._unlockTabSizing();
    this.tabContainer._positionPinnedTabs();
    this.tabContainer._updateCloseButtons();

    this.getBrowserForTab(aTab).messageManager.sendAsyncMessage("Browser:AppTab", { isAppTab: true });

    let event = document.createEvent("Events");
    event.initEvent("TabPinned", true, false);
    aTab.dispatchEvent(event);
  }

  unpinTab(aTab) {
    if (!aTab.pinned)
      return;

    this.moveTabTo(aTab, this._numPinnedTabs - 1);
    aTab.removeAttribute("pinned");
    aTab.style.marginInlineStart = "";
    this.tabContainer._unlockTabSizing();
    this.tabContainer._positionPinnedTabs();
    this.tabContainer._updateCloseButtons();

    this.getBrowserForTab(aTab).messageManager.sendAsyncMessage("Browser:AppTab", { isAppTab: false });

    let event = document.createEvent("Events");
    event.initEvent("TabUnpinned", true, false);
    aTab.dispatchEvent(event);
  }

  previewTab(aTab, aCallback) {
    let currentTab = this.selectedTab;
    try {
      // Suppress focus, ownership and selected tab changes
      this._previewMode = true;
      this.selectedTab = aTab;
      aCallback();
    } finally {
      this.selectedTab = currentTab;
      this._previewMode = false;
    }
  }

  syncThrobberAnimations(aTab) {
    aTab.ownerGlobal.promiseDocumentFlushed(() => {
      if (!aTab.parentNode) {
        return;
      }

      const animations =
        Array.from(aTab.parentNode.getElementsByTagName("tab"))
        .map(tab => {
          const throbber =
            document.getAnonymousElementByAttribute(tab, "anonid", "tab-throbber");
          return throbber ? throbber.getAnimations({ subtree: true }) : [];
        })
        .reduce((a, b) => a.concat(b))
        .filter(anim =>
          anim instanceof CSSAnimation &&
          (anim.animationName === "tab-throbber-animation" ||
            anim.animationName === "tab-throbber-animation-rtl") &&
          (anim.playState === "running" || anim.playState === "pending"));

      // Synchronize with the oldest running animation, if any.
      const firstStartTime = Math.min(
        ...animations.map(anim => anim.startTime === null ? Infinity : anim.startTime)
      );
      if (firstStartTime === Infinity) {
        return;
      }
      requestAnimationFrame(() => {
        for (let animation of animations) {
          // If |animation| has been cancelled since this rAF callback
          // was scheduled we don't want to set its startTime since
          // that would restart it. We check for a cancelled animation
          // by looking for a null currentTime rather than checking
          // the playState, since reading the playState of
          // a CSSAnimation object will flush style.
          if (animation.currentTime !== null) {
            animation.startTime = firstStartTime;
          }
        }
      });
    });
  }

  getBrowserAtIndex(aIndex) {
    return this.browsers[aIndex];
  }

  getBrowserIndexForDocument(aDocument) {
    var tab = this._getTabForContentWindow(aDocument.defaultView);
    return tab ? tab._tPos : -1;
  }

  getBrowserForDocument(aDocument) {
    var tab = this._getTabForContentWindow(aDocument.defaultView);
    return tab ? tab.linkedBrowser : null;
  }

  getBrowserForContentWindow(aWindow) {
    var tab = this._getTabForContentWindow(aWindow);
    return tab ? tab.linkedBrowser : null;
  }

  getBrowserForOuterWindowID(aID) {
    return this._outerWindowIDBrowserMap.get(aID);
  }

  _getTabForContentWindow(aWindow) {
    // When not using remote browsers, we can take a fast path by getting
    // directly from the content window to the browser without looping
    // over all browsers.
    if (!gMultiProcessBrowser) {
      let browser = aWindow.QueryInterface(Ci.nsIInterfaceRequestor)
                           .getInterface(Ci.nsIWebNavigation)
                           .QueryInterface(Ci.nsIDocShell)
                           .chromeEventHandler;
      return this.getTabForBrowser(browser);
    }

    for (let i = 0; i < this.browsers.length; i++) {
      // NB: We use contentWindowAsCPOW so that this code works both
      // for remote browsers as well. aWindow may be a CPOW.
      if (this.browsers[i].contentWindowAsCPOW == aWindow)
        return this.tabs[i];
    }
    return null;
  }

  getTabForBrowser(aBrowser) {
    return this._tabForBrowser.get(aBrowser);
  }

  getNotificationBox(aBrowser) {
    return this.getSidebarContainer(aBrowser).parentNode;
  }

  getSidebarContainer(aBrowser) {
    return this.getBrowserContainer(aBrowser).parentNode;
  }

  getBrowserContainer(aBrowser) {
    return (aBrowser || this.mCurrentBrowser).parentNode.parentNode;
  }

  getTabModalPromptBox(aBrowser) {
    let browser = (aBrowser || this.mCurrentBrowser);
    if (!browser.tabModalPromptBox) {
      browser.tabModalPromptBox = new TabModalPromptBox(browser);
    }
    return browser.tabModalPromptBox;
  }

  getTabFromAudioEvent(aEvent) {
    if (!Services.prefs.getBoolPref("browser.tabs.showAudioPlayingIcon") ||
      !aEvent.isTrusted) {
      return null;
    }

    var browser = aEvent.originalTarget;
    var tab = this.getTabForBrowser(browser);
    return tab;
  }

  _callProgressListeners(aBrowser, aMethod, aArguments, aCallGlobalListeners, aCallTabsListeners) {
    var rv = true;

    function callListeners(listeners, args) {
      for (let p of listeners) {
        if (aMethod in p) {
          try {
            if (!p[aMethod].apply(p, args))
              rv = false;
          } catch (e) {
            // don't inhibit other listeners
            Cu.reportError(e);
          }
        }
      }
    }

    if (!aBrowser)
      aBrowser = this.mCurrentBrowser;

    // eslint-disable-next-line mozilla/no-compare-against-boolean-literals
    if (aCallGlobalListeners != false &&
        aBrowser == this.mCurrentBrowser) {
      callListeners(this.mProgressListeners, aArguments);
    }

    // eslint-disable-next-line mozilla/no-compare-against-boolean-literals
    if (aCallTabsListeners != false) {
      aArguments.unshift(aBrowser);

      callListeners(this.mTabsProgressListeners, aArguments);
    }

    return rv;
  }

  /**
   * Determine if a URI is an about: page pointing to a local resource.
   */
  _isLocalAboutURI(aURI, aResolvedURI) {
    if (!aURI.schemeIs("about")) {
      return false;
    }

    // Specially handle about:blank as local
    if (aURI.pathQueryRef === "blank") {
      return true;
    }

    try {
      // Use the passed in resolvedURI if we have one
      const resolvedURI = aResolvedURI || Services.io.newChannelFromURI2(
        aURI,
        null, // loadingNode
        Services.scriptSecurityManager.getSystemPrincipal(), // loadingPrincipal
        null, // triggeringPrincipal
        Ci.nsILoadInfo.SEC_ALLOW_CROSS_ORIGIN_DATA_IS_NULL, // securityFlags
        Ci.nsIContentPolicy.TYPE_OTHER // contentPolicyType
      ).URI;
      return resolvedURI.schemeIs("jar") || resolvedURI.schemeIs("file");
    } catch (ex) {
      // aURI might be invalid.
      return false;
    }
  }

  storeIcon(aBrowser, aURI, aLoadingPrincipal, aRequestContextID) {
    try {
      if (!(aURI instanceof Ci.nsIURI)) {
        aURI = makeURI(aURI);
      }
      PlacesUIUtils.loadFavicon(aBrowser, aLoadingPrincipal, aURI, aRequestContextID);
    } catch (ex) {
      Cu.reportError(ex);
    }
  }

  setIcon(aTab, aURI, aLoadingPrincipal, aRequestContextID) {
    let browser = this.getBrowserForTab(aTab);
    browser.mIconURL = aURI instanceof Ci.nsIURI ? aURI.spec : aURI;
    let loadingPrincipal = aLoadingPrincipal ||
      Services.scriptSecurityManager.getSystemPrincipal();
    let requestContextID = aRequestContextID || 0;
    let sizedIconUrl = browser.mIconURL || "";
    if (sizedIconUrl != aTab.getAttribute("image")) {
      if (sizedIconUrl) {
        if (!browser.mIconLoadingPrincipal ||
          !browser.mIconLoadingPrincipal.equals(loadingPrincipal)) {
          aTab.setAttribute("iconloadingprincipal",
            this.serializationHelper.serializeToString(loadingPrincipal));
          aTab.setAttribute("requestcontextid", requestContextID);
          browser.mIconLoadingPrincipal = loadingPrincipal;
        }
        aTab.setAttribute("image", sizedIconUrl);
      } else {
        aTab.removeAttribute("iconloadingprincipal");
        delete browser.mIconLoadingPrincipal;
        aTab.removeAttribute("image");
      }
      this._tabAttrModified(aTab, ["image"]);
    }

    this._callProgressListeners(browser, "onLinkIconAvailable", [browser.mIconURL]);
  }

  getIcon(aTab) {
    let browser = aTab ? this.getBrowserForTab(aTab) : this.selectedBrowser;
    return browser.mIconURL;
  }

  setPageInfo(aURL, aDescription, aPreviewImage) {
    if (aURL) {
      let pageInfo = { url: aURL, description: aDescription, previewImageURL: aPreviewImage };
      PlacesUtils.history.update(pageInfo).catch(Cu.reportError);
    }
  }

  shouldLoadFavIcon(aURI) {
    return (aURI &&
            Services.prefs.getBoolPref("browser.chrome.site_icons") &&
            Services.prefs.getBoolPref("browser.chrome.favicons") &&
            ("schemeIs" in aURI) && (aURI.schemeIs("http") || aURI.schemeIs("https")));
  }

  useDefaultIcon(aTab) {
    let browser = this.getBrowserForTab(aTab);
    let documentURI = browser.documentURI;
    let requestContextID = browser.contentRequestContextID;
    let loadingPrincipal = browser.contentPrincipal;
    let icon = null;

    if (browser.imageDocument) {
      if (Services.prefs.getBoolPref("browser.chrome.site_icons")) {
        let sz = Services.prefs.getIntPref("browser.chrome.image_icons.max_size");
        if (browser.imageDocument.width <= sz &&
            browser.imageDocument.height <= sz) {
          // Don't try to store the icon in Places, regardless it would
          // be skipped (see Bug 403651).
          icon = browser.currentURI;
        }
      }
    }

    // Use documentURIObject in the check for shouldLoadFavIcon so that we
    // do the right thing with about:-style error pages.  Bug 453442
    if (!icon && this.shouldLoadFavIcon(documentURI)) {
      let url = documentURI.prePath + "/favicon.ico";
      if (!this.isFailedIcon(url)) {
        icon = url;
        this.storeIcon(browser, icon, loadingPrincipal, requestContextID);
      }
    }

    this.setIcon(aTab, icon, loadingPrincipal, requestContextID);
  }

  isFailedIcon(aURI) {
    if (!(aURI instanceof Ci.nsIURI))
      aURI = makeURI(aURI);
    return PlacesUtils.favicons.isFailedFavicon(aURI);
  }

  getWindowTitleForBrowser(aBrowser) {
    var newTitle = "";
    var docElement = this.ownerDocument.documentElement;
    var sep = docElement.getAttribute("titlemenuseparator");
    let tab = this.getTabForBrowser(aBrowser);
    let docTitle;

    if (tab._labelIsContentTitle) {
      // Strip out any null bytes in the content title, since the
      // underlying widget implementations of nsWindow::SetTitle pass
      // null-terminated strings to system APIs.
      docTitle = tab.getAttribute("label").replace(/\0/g, "");
    }

    if (!docTitle)
      docTitle = docElement.getAttribute("titledefault");

    var modifier = docElement.getAttribute("titlemodifier");
    if (docTitle) {
      newTitle += docElement.getAttribute("titlepreface");
      newTitle += docTitle;
      if (modifier)
        newTitle += sep;
    }
    newTitle += modifier;

    // If location bar is hidden and the URL type supports a host,
    // add the scheme and host to the title to prevent spoofing.
    // XXX https://bugzilla.mozilla.org/show_bug.cgi?id=22183#c239
    try {
      if (docElement.getAttribute("chromehidden").includes("location")) {
        var uri = this.mURIFixup.createExposableURI(
          aBrowser.currentURI);
        if (uri.scheme == "about")
          newTitle = uri.spec + sep + newTitle;
        else
          newTitle = uri.prePath + sep + newTitle;
      }
    } catch (e) {}

    return newTitle;
  }

  updateTitlebar() {
    this.ownerDocument.title = this.getWindowTitleForBrowser(this.mCurrentBrowser);
  }

  updateCurrentBrowser(aForceUpdate) {
    var newBrowser = this.getBrowserAtIndex(this.tabContainer.selectedIndex);
    if (this.mCurrentBrowser == newBrowser && !aForceUpdate)
      return;

    if (!aForceUpdate) {
      document.commandDispatcher.lock();

      TelemetryStopwatch.start("FX_TAB_SWITCH_UPDATE_MS");
      if (!gMultiProcessBrowser) {
        // old way of measuring tab paint which is not valid with e10s.
        // Waiting until the next MozAfterPaint ensures that we capture
        // the time it takes to paint, upload the textures to the compositor,
        // and then composite.
        if (this._tabSwitchID) {
          TelemetryStopwatch.cancel("FX_TAB_SWITCH_TOTAL_MS");
        }

        let tabSwitchID = Symbol();

        TelemetryStopwatch.start("FX_TAB_SWITCH_TOTAL_MS");
        this._tabSwitchID = tabSwitchID;

        let onMozAfterPaint = () => {
          if (this._tabSwitchID === tabSwitchID) {
            TelemetryStopwatch.finish("FX_TAB_SWITCH_TOTAL_MS");
            this._tabSwitchID = null;
          }
          window.removeEventListener("MozAfterPaint", onMozAfterPaint);
        };
        window.addEventListener("MozAfterPaint", onMozAfterPaint);
      }
    }

    var oldTab = this.mCurrentTab;

    // Preview mode should not reset the owner
    if (!this._previewMode && !oldTab.selected)
      oldTab.owner = null;

    let lastRelatedTab = this._lastRelatedTabMap.get(oldTab);
    if (lastRelatedTab) {
      if (!lastRelatedTab.selected)
        lastRelatedTab.owner = null;
    }
    this._lastRelatedTabMap = new WeakMap();

    var oldBrowser = this.mCurrentBrowser;

    if (!gMultiProcessBrowser) {
      oldBrowser.removeAttribute("primary");
      oldBrowser.docShellIsActive = false;
      newBrowser.setAttribute("primary", "true");
      newBrowser.docShellIsActive =
        (window.windowState != window.STATE_MINIMIZED &&
          !window.isFullyOccluded);
    }

    var updateBlockedPopups = false;
    if ((oldBrowser.blockedPopups && !newBrowser.blockedPopups) ||
      (!oldBrowser.blockedPopups && newBrowser.blockedPopups))
      updateBlockedPopups = true;

    this.mCurrentBrowser = newBrowser;
    this.mCurrentTab = this.tabContainer.selectedItem;
    this.showTab(this.mCurrentTab);

    gURLBar.setAttribute("switchingtabs", "true");
    window.addEventListener("MozAfterPaint", function() {
      gURLBar.removeAttribute("switchingtabs");
    }, { once: true });

    this._appendStatusPanel();

    if (updateBlockedPopups)
      this.mCurrentBrowser.updateBlockedPopups();

    // Update the URL bar.
    var loc = this.mCurrentBrowser.currentURI;

    var webProgress = this.mCurrentBrowser.webProgress;
    var securityUI = this.mCurrentBrowser.securityUI;

    this._callProgressListeners(null, "onLocationChange",
                                [webProgress, null, loc, 0],
                                true, false);

    if (securityUI) {
      // Include the true final argument to indicate that this event is
      // simulated (instead of being observed by the webProgressListener).
      this._callProgressListeners(null, "onSecurityChange",
                                  [webProgress, null, securityUI.state, true],
                                  true, false);
    }

    var listener = this._tabListeners.get(this.mCurrentTab);
    if (listener && listener.mStateFlags) {
      this._callProgressListeners(null, "onUpdateCurrentBrowser",
                                  [listener.mStateFlags, listener.mStatus,
                                  listener.mMessage, listener.mTotalProgress],
                                  true, false);
    }

    if (!this._previewMode) {
      this.mCurrentTab.updateLastAccessed();
      this.mCurrentTab.removeAttribute("unread");
      oldTab.updateLastAccessed();

      let oldFindBar = oldTab._findBar;
      if (oldFindBar &&
          oldFindBar.findMode == oldFindBar.FIND_NORMAL &&
          !oldFindBar.hidden)
        this._lastFindValue = oldFindBar._findField.value;

      this.updateTitlebar();

      this.mCurrentTab.removeAttribute("titlechanged");
      this.mCurrentTab.removeAttribute("attention");

      // The tab has been selected, it's not unselected anymore.
      // (1) Call the current tab's finishUnselectedTabHoverTimer()
      //     to save a telemetry record.
      // (2) Call the current browser's unselectedTabHover() with false
      //     to dispatch an event.
      this.mCurrentTab.finishUnselectedTabHoverTimer();
      this.mCurrentBrowser.unselectedTabHover(false);
    }

    // If the new tab is busy, and our current state is not busy, then
    // we need to fire a start to all progress listeners.
    const nsIWebProgressListener = Ci.nsIWebProgressListener;
    if (this.mCurrentTab.hasAttribute("busy") && !this.mIsBusy) {
      this.mIsBusy = true;
      this._callProgressListeners(null, "onStateChange",
                                  [webProgress, null,
                                  nsIWebProgressListener.STATE_START |
                                  nsIWebProgressListener.STATE_IS_NETWORK, 0],
                                  true, false);
    }

    // If the new tab is not busy, and our current state is busy, then
    // we need to fire a stop to all progress listeners.
    if (!this.mCurrentTab.hasAttribute("busy") && this.mIsBusy) {
      this.mIsBusy = false;
      this._callProgressListeners(null, "onStateChange",
                                  [webProgress, null,
                                  nsIWebProgressListener.STATE_STOP |
                                  nsIWebProgressListener.STATE_IS_NETWORK, 0],
                                  true, false);
    }

    // TabSelect events are suppressed during preview mode to avoid confusing extensions and other bits of code
    // that might rely upon the other changes suppressed.
    // Focus is suppressed in the event that the main browser window is minimized - focusing a tab would restore the window
    if (!this._previewMode) {
      // We've selected the new tab, so go ahead and notify listeners.
      let event = new CustomEvent("TabSelect", {
        bubbles: true,
        cancelable: false,
        detail: {
          previousTab: oldTab
        }
      });
      this.mCurrentTab.dispatchEvent(event);

      this._tabAttrModified(oldTab, ["selected"]);
      this._tabAttrModified(this.mCurrentTab, ["selected"]);

      if (oldBrowser != newBrowser &&
          oldBrowser.getInPermitUnload) {
        oldBrowser.getInPermitUnload(inPermitUnload => {
          if (!inPermitUnload) {
            return;
          }
          // Since the user is switching away from a tab that has
          // a beforeunload prompt active, we remove the prompt.
          // This prevents confusing user flows like the following:
          //   1. User attempts to close Firefox
          //   2. User switches tabs (ingoring a beforeunload prompt)
          //   3. User returns to tab, presses "Leave page"
          let promptBox = this.getTabModalPromptBox(oldBrowser);
          let prompts = promptBox.listPrompts();
          // There might not be any prompts here if the tab was closed
          // while in an onbeforeunload prompt, which will have
          // destroyed aforementioned prompt already, so check there's
          // something to remove, first:
          if (prompts.length) {
            // NB: This code assumes that the beforeunload prompt
            //     is the top-most prompt on the tab.
            prompts[prompts.length - 1].abortPrompt();
          }
        });
      }

      if (!gMultiProcessBrowser) {
        this._adjustFocusBeforeTabSwitch(oldTab, this.mCurrentTab);
        this._adjustFocusAfterTabSwitch(this.mCurrentTab);
      }
    }

    updateUserContextUIIndicator();
    gIdentityHandler.updateSharingIndicator();

    this.tabContainer._setPositionalAttributes();

    // Enable touch events to start a native dragging
    // session to allow the user to easily drag the selected tab.
    // This is currently only supported on Windows.
    oldTab.removeAttribute("touchdownstartsdrag");
    this.mCurrentTab.setAttribute("touchdownstartsdrag", "true");

    if (!gMultiProcessBrowser) {
      document.commandDispatcher.unlock();

      let event = new CustomEvent("TabSwitchDone", {
        bubbles: true,
        cancelable: true
      });
      this.dispatchEvent(event);
    }

    if (!aForceUpdate)
      TelemetryStopwatch.finish("FX_TAB_SWITCH_UPDATE_MS");
  }

  _adjustFocusBeforeTabSwitch(oldTab, newTab) {
    if (this._previewMode) {
      return;
    }

    let oldBrowser = oldTab.linkedBrowser;
    let newBrowser = newTab.linkedBrowser;

    oldBrowser._urlbarFocused = (gURLBar && gURLBar.focused);

    if (this.isFindBarInitialized(oldTab)) {
      let findBar = this.getFindBar(oldTab);
      oldTab._findBarFocused = (!findBar.hidden &&
        findBar._findField.getAttribute("focused") == "true");
    }

    let activeEl = document.activeElement;
    // If focus is on the old tab, move it to the new tab.
    if (activeEl == oldTab) {
      newTab.focus();
    } else if (gMultiProcessBrowser && activeEl != newBrowser && activeEl != newTab) {
      // In e10s, if focus isn't already in the tabstrip or on the new browser,
      // and the new browser's previous focus wasn't in the url bar but focus is
      // there now, we need to adjust focus further.
      let keepFocusOnUrlBar = newBrowser &&
        newBrowser._urlbarFocused &&
        gURLBar &&
        gURLBar.focused;
      if (!keepFocusOnUrlBar) {
        // Clear focus so that _adjustFocusAfterTabSwitch can detect if
        // some element has been focused and respect that.
        document.activeElement.blur();
      }
    }
  }

  _adjustFocusAfterTabSwitch(newTab) {
    // Don't steal focus from the tab bar.
    if (document.activeElement == newTab)
      return;

    let newBrowser = this.getBrowserForTab(newTab);

    // If there's a tabmodal prompt showing, focus it.
    if (newBrowser.hasAttribute("tabmodalPromptShowing")) {
      let XUL_NS = "http://www.mozilla.org/keymaster/gatekeeper/there.is.only.xul";
      let prompts = newBrowser.parentNode.getElementsByTagNameNS(XUL_NS, "tabmodalprompt");
      let prompt = prompts[prompts.length - 1];
      prompt.Dialog.setDefaultFocus();
      return;
    }

    // Focus the location bar if it was previously focused for that tab.
    // In full screen mode, only bother making the location bar visible
    // if the tab is a blank one.
    if (newBrowser._urlbarFocused && gURLBar) {
      // Explicitly close the popup if the URL bar retains focus
      gURLBar.closePopup();

      // If the user happened to type into the URL bar for this browser
      // by the time we got here, focusing will cause the text to be
      // selected which could cause them to overwrite what they've
      // already typed in.
      if (gURLBar.focused && newBrowser.userTypedValue) {
        return;
      }

      if (!window.fullScreen || isTabEmpty(newTab)) {
        focusAndSelectUrlBar();
        return;
      }
    }

    // Focus the find bar if it was previously focused for that tab.
    if (gFindBarInitialized && !gFindBar.hidden &&
        this.selectedTab._findBarFocused) {
      gFindBar._findField.focus();
      return;
    }

    // Don't focus the content area if something has been focused after the
    // tab switch was initiated.
    if (gMultiProcessBrowser &&
        document.activeElement != document.documentElement)
      return;

    // We're now committed to focusing the content area.
    let fm = Services.focus;
    let focusFlags = fm.FLAG_NOSCROLL;

    if (!gMultiProcessBrowser) {
      let newFocusedElement = fm.getFocusedElementForWindow(window.content, true, {});

      // for anchors, use FLAG_SHOWRING so that it is clear what link was
      // last clicked when switching back to that tab
      if (newFocusedElement &&
          (newFocusedElement instanceof HTMLAnchorElement ||
            newFocusedElement.getAttributeNS("http://www.w3.org/1999/xlink", "type") == "simple"))
        focusFlags |= fm.FLAG_SHOWRING;
    }

    fm.setFocus(newBrowser, focusFlags);
  }

  _tabAttrModified(aTab, aChanged) {
    if (aTab.closing)
      return;

    let event = new CustomEvent("TabAttrModified", {
      bubbles: true,
      cancelable: false,
      detail: {
        changed: aChanged,
      }
    });
    aTab.dispatchEvent(event);
  }

  setBrowserSharing(aBrowser, aState) {
    let tab = this.getTabForBrowser(aBrowser);
    if (!tab)
      return;

    if (aState.sharing) {
      tab._sharingState = aState;
      if (aState.paused) {
        tab.removeAttribute("sharing");
      } else {
        tab.setAttribute("sharing", aState.sharing);
      }
    } else {
      tab._sharingState = null;
      tab.removeAttribute("sharing");
    }
    this._tabAttrModified(tab, ["sharing"]);

    if (aBrowser == this.mCurrentBrowser)
      gIdentityHandler.updateSharingIndicator();
  }

  getTabSharingState(aTab) {
    // Normalize the state object for consumers (ie.extensions).
    let state = Object.assign({}, aTab._sharingState);
    return {
      camera: !!state.camera,
      microphone: !!state.microphone,
      screen: state.screen && state.screen.replace("Paused", ""),
    };
  }

  setInitialTabTitle(aTab, aTitle, aOptions) {
    if (aTitle) {
      if (!aTab.getAttribute("label")) {
        aTab._labelIsInitialTitle = true;
      }

      this._setTabLabel(aTab, aTitle, aOptions);
    }
  }

  setTabTitle(aTab) {
    var browser = this.getBrowserForTab(aTab);
    var title = browser.contentTitle;

    // Don't replace an initially set label with the URL while the tab
    // is loading.
    if (aTab._labelIsInitialTitle) {
      if (!title) {
        return false;
      }
      delete aTab._labelIsInitialTitle;
    }

    let isContentTitle = false;
    if (title) {
      isContentTitle = true;
    } else if (aTab.hasAttribute("customizemode")) {
      let brandBundle = document.getElementById("bundle_brand");
      let brandShortName = brandBundle.getString("brandShortName");
      title = gNavigatorBundle.getFormattedString("customizeMode.tabTitle",
                                                  [brandShortName]);
      isContentTitle = true;
    } else {
      if (browser.currentURI.displaySpec) {
        try {
          title = this.mURIFixup.createExposableURI(browser.currentURI).displaySpec;
        } catch (ex) {
          title = browser.currentURI.displaySpec;
        }
      }

      if (title && !isBlankPageURL(title)) {
        // At this point, we now have a URI.
        // Let's try to unescape it using a character set
        // in case the URI is not ASCII.
        try {
          // If it's a long data: URI that uses base64 encoding, truncate to
          // a reasonable length rather than trying to display the entire thing.
          // We can't shorten arbitrary URIs like this, as bidi etc might mean
          // we need the trailing characters for display. But a base64-encoded
          // data-URI is plain ASCII, so this is OK for tab-title display.
          // (See bug 1408854.)
          if (title.length > 500 && title.match(/^data:[^,]+;base64,/)) {
            title = title.substring(0, 500) + "\u2026";
          } else {
            var characterSet = browser.characterSet;
            title = Services.textToSubURI.unEscapeNonAsciiURI(characterSet, title);
          }
        } catch (ex) { /* Do nothing. */ }
      } else {
        // Still no title? Fall back to our untitled string.
        title = gTabBrowserBundle.GetStringFromName("tabs.emptyTabTitle");
      }
    }

    return this._setTabLabel(aTab, title, { isContentTitle });
  }

  _setTabLabel(aTab, aLabel, aOptions) {
    if (!aLabel) {
      return false;
    }

    aTab._fullLabel = aLabel;

    aOptions = aOptions || {};
    if (!aOptions.isContentTitle) {
      // Remove protocol and "www."
      if (!("_regex_shortenURLForTabLabel" in this)) {
        this._regex_shortenURLForTabLabel = /^[^:]+:\/\/(?:www\.)?/;
      }
      aLabel = aLabel.replace(this._regex_shortenURLForTabLabel, "");
    }

    aTab._labelIsContentTitle = aOptions.isContentTitle;

    if (aTab.getAttribute("label") == aLabel) {
      return false;
    }

    let dwu = window.QueryInterface(Ci.nsIInterfaceRequestor)
                    .getInterface(Ci.nsIDOMWindowUtils);
    let isRTL = dwu.getDirectionFromText(aLabel) == Ci.nsIDOMWindowUtils.DIRECTION_RTL;

    aTab.setAttribute("label", aLabel);
    aTab.setAttribute("labeldirection", isRTL ? "rtl" : "ltr");

    // Dispatch TabAttrModified event unless we're setting the label
    // before the TabOpen event was dispatched.
    if (!aOptions.beforeTabOpen) {
      this._tabAttrModified(aTab, ["label"]);
    }

    if (aTab.selected) {
      this.updateTitlebar();
    }

    return true;
  }

  loadOneTab(aURI, aReferrerURI, aCharset, aPostData, aLoadInBackground, aAllowThirdPartyFixup) {
    var aTriggeringPrincipal;
    var aReferrerPolicy;
    var aFromExternal;
    var aRelatedToCurrent;
    var aAllowMixedContent;
    var aSkipAnimation;
    var aForceNotRemote;
    var aPreferredRemoteType;
    var aNoReferrer;
    var aUserContextId;
    var aSameProcessAsFrameLoader;
    var aOriginPrincipal;
    var aOpener;
    var aOpenerBrowser;
    var aCreateLazyBrowser;
    var aNextTabParentId;
    var aFocusUrlBar;
    var aName;
    if (arguments.length == 2 &&
        typeof arguments[1] == "object" &&
        !(arguments[1] instanceof Ci.nsIURI)) {
      let params = arguments[1];
      aTriggeringPrincipal = params.triggeringPrincipal;
      aReferrerURI = params.referrerURI;
      aReferrerPolicy = params.referrerPolicy;
      aCharset = params.charset;
      aPostData = params.postData;
      aLoadInBackground = params.inBackground;
      aAllowThirdPartyFixup = params.allowThirdPartyFixup;
      aFromExternal = params.fromExternal;
      aRelatedToCurrent = params.relatedToCurrent;
      aAllowMixedContent = params.allowMixedContent;
      aSkipAnimation = params.skipAnimation;
      aForceNotRemote = params.forceNotRemote;
      aPreferredRemoteType = params.preferredRemoteType;
      aNoReferrer = params.noReferrer;
      aUserContextId = params.userContextId;
      aSameProcessAsFrameLoader = params.sameProcessAsFrameLoader;
      aOriginPrincipal = params.originPrincipal;
      aOpener = params.opener;
      aOpenerBrowser = params.openerBrowser;
      aCreateLazyBrowser = params.createLazyBrowser;
      aNextTabParentId = params.nextTabParentId;
      aFocusUrlBar = params.focusUrlBar;
      aName = params.name;
    }

    var bgLoad = (aLoadInBackground != null) ? aLoadInBackground :
      Services.prefs.getBoolPref("browser.tabs.loadInBackground");
    var owner = bgLoad ? null : this.selectedTab;

    var tab = this.addTab(aURI, {
      triggeringPrincipal: aTriggeringPrincipal,
      referrerURI: aReferrerURI,
      referrerPolicy: aReferrerPolicy,
      charset: aCharset,
      postData: aPostData,
      ownerTab: owner,
      allowThirdPartyFixup: aAllowThirdPartyFixup,
      fromExternal: aFromExternal,
      relatedToCurrent: aRelatedToCurrent,
      skipAnimation: aSkipAnimation,
      allowMixedContent: aAllowMixedContent,
      forceNotRemote: aForceNotRemote,
      createLazyBrowser: aCreateLazyBrowser,
      preferredRemoteType: aPreferredRemoteType,
      noReferrer: aNoReferrer,
      userContextId: aUserContextId,
      originPrincipal: aOriginPrincipal,
      sameProcessAsFrameLoader: aSameProcessAsFrameLoader,
      opener: aOpener,
      openerBrowser: aOpenerBrowser,
      nextTabParentId: aNextTabParentId,
      focusUrlBar: aFocusUrlBar,
      name: aName
    });
    if (!bgLoad)
      this.selectedTab = tab;

    return tab;
  }

  loadTabs(aURIs, aLoadInBackground, aReplace) {
    let aTriggeringPrincipal;
    let aAllowThirdPartyFixup;
    let aTargetTab;
    let aNewIndex = -1;
    let aPostDatas = [];
    let aUserContextId;
    if (arguments.length == 2 &&
        typeof arguments[1] == "object") {
      let params = arguments[1];
      aLoadInBackground = params.inBackground;
      aReplace = params.replace;
      aAllowThirdPartyFixup = params.allowThirdPartyFixup;
      aTargetTab = params.targetTab;
      aNewIndex = typeof params.newIndex === "number" ?
        params.newIndex : aNewIndex;
      aPostDatas = params.postDatas || aPostDatas;
      aUserContextId = params.userContextId;
      aTriggeringPrincipal = params.triggeringPrincipal;
    }

    if (!aURIs.length)
      return;

    // The tab selected after this new tab is closed (i.e. the new tab's
    // "owner") is the next adjacent tab (i.e. not the previously viewed tab)
    // when several urls are opened here (i.e. closing the first should select
    // the next of many URLs opened) or if the pref to have UI links opened in
    // the background is set (i.e. the link is not being opened modally)
    //
    // i.e.
    //    Number of URLs    Load UI Links in BG       Focus Last Viewed?
    //    == 1              false                     YES
    //    == 1              true                      NO
    //    > 1               false/true                NO
    var multiple = aURIs.length > 1;
    var owner = multiple || aLoadInBackground ? null : this.selectedTab;
    var firstTabAdded = null;
    var targetTabIndex = -1;

    if (aReplace) {
      let browser;
      if (aTargetTab) {
        browser = this.getBrowserForTab(aTargetTab);
        targetTabIndex = aTargetTab._tPos;
      } else {
        browser = this.mCurrentBrowser;
        targetTabIndex = this.tabContainer.selectedIndex;
      }
      let flags = Ci.nsIWebNavigation.LOAD_FLAGS_NONE;
      if (aAllowThirdPartyFixup) {
        flags |= Ci.nsIWebNavigation.LOAD_FLAGS_ALLOW_THIRD_PARTY_FIXUP |
          Ci.nsIWebNavigation.LOAD_FLAGS_FIXUP_SCHEME_TYPOS;
      }
      try {
        browser.loadURIWithFlags(aURIs[0], {
          flags,
          postData: aPostDatas[0],
          triggeringPrincipal: aTriggeringPrincipal,
        });
      } catch (e) {
        // Ignore failure in case a URI is wrong, so we can continue
        // opening the next ones.
      }
    } else {
      firstTabAdded = this.addTab(aURIs[0], {
        ownerTab: owner,
        skipAnimation: multiple,
        allowThirdPartyFixup: aAllowThirdPartyFixup,
        postData: aPostDatas[0],
        userContextId: aUserContextId,
        triggeringPrincipal: aTriggeringPrincipal,
      });
      if (aNewIndex !== -1) {
        this.moveTabTo(firstTabAdded, aNewIndex);
        targetTabIndex = firstTabAdded._tPos;
      }
    }

    let tabNum = targetTabIndex;
    for (let i = 1; i < aURIs.length; ++i) {
      let tab = this.addTab(aURIs[i], {
        skipAnimation: true,
        allowThirdPartyFixup: aAllowThirdPartyFixup,
        postData: aPostDatas[i],
        userContextId: aUserContextId,
        triggeringPrincipal: aTriggeringPrincipal,
      });
      if (targetTabIndex !== -1)
        this.moveTabTo(tab, ++tabNum);
    }

    if (firstTabAdded && !aLoadInBackground) {
      this.selectedTab = firstTabAdded;
    }
  }

  updateBrowserRemoteness(aBrowser, aShouldBeRemote, aOptions) {
    aOptions = aOptions || {};
    let isRemote = aBrowser.getAttribute("remote") == "true";

    if (!gMultiProcessBrowser && aShouldBeRemote) {
      throw new Error("Cannot switch to remote browser in a window " +
        "without the remote tabs load context.");
    }

    // Default values for remoteType
    if (!aOptions.remoteType) {
      aOptions.remoteType = aShouldBeRemote ? E10SUtils.DEFAULT_REMOTE_TYPE : E10SUtils.NOT_REMOTE;
    }

    // If we are passed an opener, we must be making the browser non-remote, and
    // if the browser is _currently_ non-remote, we need the openers to match,
    // because it is already too late to change it.
    if (aOptions.opener) {
      if (aShouldBeRemote) {
        throw new Error("Cannot set an opener on a browser which should be remote!");
      }
      if (!isRemote && aBrowser.contentWindow.opener != aOptions.opener) {
        throw new Error("Cannot change opener on an already non-remote browser!");
      }
    }

    // Abort if we're not going to change anything
    let currentRemoteType = aBrowser.getAttribute("remoteType");
    if (isRemote == aShouldBeRemote && !aOptions.newFrameloader &&
        (!isRemote || currentRemoteType == aOptions.remoteType)) {
      return false;
    }

    let tab = this.getTabForBrowser(aBrowser);
    // aBrowser needs to be inserted now if it hasn't been already.
    this._insertBrowser(tab);

    let evt = document.createEvent("Events");
    evt.initEvent("BeforeTabRemotenessChange", true, false);
    tab.dispatchEvent(evt);

    let wasActive = document.activeElement == aBrowser;

    // Unmap the old outerWindowID.
    this._outerWindowIDBrowserMap.delete(aBrowser.outerWindowID);

    // Unhook our progress listener.
    let filter = this._tabFilters.get(tab);
    let listener = this._tabListeners.get(tab);
    aBrowser.webProgress.removeProgressListener(filter);
    filter.removeProgressListener(listener);

    // We'll be creating a new listener, so destroy the old one.
    listener.destroy();

    let oldUserTypedValue = aBrowser.userTypedValue;
    let hadStartedLoad = aBrowser.didStartLoadSinceLastUserTyping();

    // Make sure the browser is destroyed so it unregisters from observer notifications
    aBrowser.destroy();

    // Make sure to restore the original droppedLinkHandler and
    // sameProcessAsFrameLoader.
    let droppedLinkHandler = aBrowser.droppedLinkHandler;
    let sameProcessAsFrameLoader = aBrowser.sameProcessAsFrameLoader;

    // Change the "remote" attribute.
    let parent = aBrowser.parentNode;
    parent.removeChild(aBrowser);
    if (aShouldBeRemote) {
      aBrowser.setAttribute("remote", "true");
      aBrowser.setAttribute("remoteType", aOptions.remoteType);
    } else {
      aBrowser.setAttribute("remote", "false");
      aBrowser.removeAttribute("remoteType");
    }

    // NB: This works with the hack in the browser constructor that
    // turns this normal property into a field.
    if (aOptions.sameProcessAsFrameLoader) {
      // Always set sameProcessAsFrameLoader when passed in aOptions.
      aBrowser.sameProcessAsFrameLoader = aOptions.sameProcessAsFrameLoader;
    } else if (!aShouldBeRemote || currentRemoteType == aOptions.remoteType) {
      // Only copy existing sameProcessAsFrameLoader when not switching
      // remote type otherwise it would stop the switch.
      aBrowser.sameProcessAsFrameLoader = sameProcessAsFrameLoader;
    }

    if (aOptions.opener) {
      // Set the opener window on the browser, such that when the frame
      // loader is created the opener is set correctly.
      aBrowser.presetOpenerWindow(aOptions.opener);
    }

    parent.appendChild(aBrowser);

    aBrowser.userTypedValue = oldUserTypedValue;
    if (hadStartedLoad) {
      aBrowser.urlbarChangeTracker.startedLoad();
    }

    aBrowser.droppedLinkHandler = droppedLinkHandler;

    // Switching a browser's remoteness will create a new frameLoader.
    // As frameLoaders start out with an active docShell we have to
    // deactivate it if this is not the selected tab's browser or the
    // browser window is minimized.
    aBrowser.docShellIsActive = this.shouldActivateDocShell(aBrowser);

    // Create a new tab progress listener for the new browser we just injected,
    // since tab progress listeners have logic for handling the initial about:blank
    // load
    listener = new TabProgressListener(tab, aBrowser, true, false);
    this._tabListeners.set(tab, listener);
    filter.addProgressListener(listener, Ci.nsIWebProgress.NOTIFY_ALL);

    // Restore the progress listener.
    aBrowser.webProgress.addProgressListener(filter, Ci.nsIWebProgress.NOTIFY_ALL);

    // Restore the securityUI state.
    let securityUI = aBrowser.securityUI;
    let state = securityUI ? securityUI.state :
      Ci.nsIWebProgressListener.STATE_IS_INSECURE;
    // Include the true final argument to indicate that this event is
    // simulated (instead of being observed by the webProgressListener).
    this._callProgressListeners(aBrowser, "onSecurityChange",
                                [aBrowser.webProgress, null, state, true],
                                true, false);

    if (aShouldBeRemote) {
      // Switching the browser to be remote will connect to a new child
      // process so the browser can no longer be considered to be
      // crashed.
      tab.removeAttribute("crashed");
    } else {
      aBrowser.messageManager.sendAsyncMessage("Browser:AppTab", { isAppTab: tab.pinned });

      // Register the new outerWindowID.
      this._outerWindowIDBrowserMap.set(aBrowser.outerWindowID, aBrowser);
    }

    if (wasActive)
      aBrowser.focus();

    // If the findbar has been initialised, reset its browser reference.
    if (this.isFindBarInitialized(tab)) {
      this.getFindBar(tab).browser = aBrowser;
    }

    evt = document.createEvent("Events");
    evt.initEvent("TabRemotenessChange", true, false);
    tab.dispatchEvent(evt);

    return true;
  }

  updateBrowserRemotenessByURL(aBrowser, aURL, aOptions) {
    aOptions = aOptions || {};

    if (!gMultiProcessBrowser)
      return this.updateBrowserRemoteness(aBrowser, false);

    let currentRemoteType = aBrowser.getAttribute("remoteType") || null;

    aOptions.remoteType =
      E10SUtils.getRemoteTypeForURI(aURL,
        gMultiProcessBrowser,
        currentRemoteType,
        aBrowser.currentURI);

    // If this URL can't load in the current browser then flip it to the
    // correct type.
    if (currentRemoteType != aOptions.remoteType ||
      aOptions.newFrameloader) {
      let remote = aOptions.remoteType != E10SUtils.NOT_REMOTE;
      return this.updateBrowserRemoteness(aBrowser, remote, aOptions);
    }

    return false;
  }

  removePreloadedBrowser() {
    if (!this._isPreloadingEnabled()) {
      return;
    }

    let browser = this._getPreloadedBrowser();

    if (browser) {
      browser.remove();
    }
  }

  _getPreloadedBrowser() {
    if (!this._isPreloadingEnabled()) {
      return null;
    }

    // The preloaded browser might be null.
    let browser = this._preloadedBrowser;

    // Consume the browser.
    this._preloadedBrowser = null;

    // Attach the nsIFormFillController now that we know the browser
    // will be used. If we do that before and the preloaded browser
    // won't be consumed until shutdown then we leak a docShell.
    // Also, we do not need to take care of attaching nsIFormFillControllers
    // in the case that the browser is remote, as remote browsers take
    // care of that themselves.
    if (browser) {
      browser.setAttribute("preloadedState", "consumed");
      if (this.hasAttribute("autocompletepopup")) {
        browser.setAttribute("autocompletepopup", this.getAttribute("autocompletepopup"));
      }
    }

    return browser;
  }

  _isPreloadingEnabled() {
    // Preloading for the newtab page is enabled when the pref is true
    // and the URL is "about:newtab". We do not support preloading for
    // custom newtab URLs.
    return Services.prefs.getBoolPref("browser.newtab.preload") &&
      !aboutNewTabService.overridden;
  }

  _createPreloadBrowser() {
    // Do nothing if we have a preloaded browser already
    // or preloading of newtab pages is disabled.
    if (this._preloadedBrowser || !this._isPreloadingEnabled()) {
      return;
    }

    let remoteType =
      E10SUtils.getRemoteTypeForURI(BROWSER_NEW_TAB_URL,
        gMultiProcessBrowser);
    let browser = this._createBrowser({ isPreloadBrowser: true, remoteType });
    this._preloadedBrowser = browser;

    let notificationbox = this.getNotificationBox(browser);
    this.mPanelContainer.appendChild(notificationbox);

    if (remoteType != E10SUtils.NOT_REMOTE) {
      // For remote browsers, we need to make sure that the webProgress is
      // instantiated, otherwise the parent won't get informed about the state
      // of the preloaded browser until it gets attached to a tab.
      browser.webProgress;
    }

    browser.loadURI(BROWSER_NEW_TAB_URL);
    browser.docShellIsActive = false;

    // Make sure the preloaded browser is loaded with desired zoom level
    let tabURI = Services.io.newURI(BROWSER_NEW_TAB_URL);
    FullZoom.onLocationChange(tabURI, false, browser);
  }

  _createBrowser(aParams) {
    // Supported parameters:
    // userContextId, remote, remoteType, isPreloadBrowser,
    // uriIsAboutBlank, sameProcessAsFrameLoader

    const NS_XUL = "http://www.mozilla.org/keymaster/gatekeeper/there.is.only.xul";

    let b = document.createElementNS(NS_XUL, "browser");
    b.permanentKey = {};
    b.setAttribute("type", "content");
    b.setAttribute("message", "true");
    b.setAttribute("messagemanagergroup", "browsers");
    b.setAttribute("contextmenu", this.getAttribute("contentcontextmenu"));
    b.setAttribute("tooltip", this.getAttribute("contenttooltip"));

    if (aParams.userContextId) {
      b.setAttribute("usercontextid", aParams.userContextId);
    }

    // remote parameter used by some addons, use default in this case.
    if (aParams.remote && !aParams.remoteType) {
      aParams.remoteType = E10SUtils.DEFAULT_REMOTE_TYPE;
    }

    if (aParams.remoteType) {
      b.setAttribute("remoteType", aParams.remoteType);
      b.setAttribute("remote", "true");
    }

    if (aParams.openerWindow) {
      if (aParams.remoteType) {
        throw new Error("Cannot set opener window on a remote browser!");
      }
      b.presetOpenerWindow(aParams.openerWindow);
    }

    if (!aParams.isPreloadBrowser && this.hasAttribute("autocompletepopup")) {
      b.setAttribute("autocompletepopup", this.getAttribute("autocompletepopup"));
    }

    /*
     * This attribute is meant to describe if the browser is the
     * preloaded browser. There are 2 defined states: "preloaded" or
     * "consumed". The order of events goes as follows:
     *   1. The preloaded browser is created and the 'preloadedState'
     *      attribute for that browser is set to "preloaded".
     *   2. When a new tab is opened and it is time to show that
     *      preloaded browser, the 'preloadedState' attribute for that
     *      browser is set to "consumed"
     *   3. When we then navigate away from about:newtab, the "consumed"
     *      browsers will attempt to switch to a new content process,
     *      therefore the 'preloadedState' attribute is removed from
     *      that browser altogether
     * See more details on Bug 1420285.
     */
    if (aParams.isPreloadBrowser) {
      b.setAttribute("preloadedState", "preloaded");
    }

    if (this.hasAttribute("selectmenulist"))
      b.setAttribute("selectmenulist", this.getAttribute("selectmenulist"));

    if (this.hasAttribute("datetimepicker")) {
      b.setAttribute("datetimepicker", this.getAttribute("datetimepicker"));
    }

    b.setAttribute("autoscrollpopup", this._autoScrollPopup.id);

    if (aParams.nextTabParentId) {
      if (!aParams.remoteType) {
        throw new Error("Cannot have nextTabParentId without a remoteType");
      }
      // Gecko is going to read this attribute and use it.
      b.setAttribute("nextTabParentId", aParams.nextTabParentId.toString());
    }

    if (aParams.sameProcessAsFrameLoader) {
      b.sameProcessAsFrameLoader = aParams.sameProcessAsFrameLoader;
    }

    // This will be used by gecko to control the name of the opened
    // window.
    if (aParams.name) {
      // XXX: The `name` property is special in HTML and XUL. Should
      // we use a different attribute name for this?
      b.setAttribute("name", aParams.name);
    }

    // Create the browserStack container
    var stack = document.createElementNS(NS_XUL, "stack");
    stack.className = "browserStack";
    stack.appendChild(b);
    stack.setAttribute("flex", "1");

    // Create the browserContainer
    var browserContainer = document.createElementNS(NS_XUL, "vbox");
    browserContainer.className = "browserContainer";
    browserContainer.appendChild(stack);
    browserContainer.setAttribute("flex", "1");

    // Create the sidebar container
    var browserSidebarContainer = document.createElementNS(NS_XUL,
      "hbox");
    browserSidebarContainer.className = "browserSidebarContainer";
    browserSidebarContainer.appendChild(browserContainer);
    browserSidebarContainer.setAttribute("flex", "1");

    // Add the Message and the Browser to the box
    var notificationbox = document.createElementNS(NS_XUL,
      "notificationbox");
    notificationbox.setAttribute("flex", "1");
    notificationbox.setAttribute("notificationside", "top");
    notificationbox.appendChild(browserSidebarContainer);

    // Prevent the superfluous initial load of a blank document
    // if we're going to load something other than about:blank.
    if (!aParams.uriIsAboutBlank) {
      b.setAttribute("nodefaultsrc", "true");
    }

    return b;
  }

  _createLazyBrowser(aTab) {
    let browser = aTab.linkedBrowser;

    let names = this._browserBindingProperties;

    for (let i = 0; i < names.length; i++) {
      let name = names[i];
      let getter;
      let setter;
      switch (name) {
        case "audioMuted":
          getter = () => false;
          break;
        case "contentTitle":
          getter = () => SessionStore.getLazyTabValue(aTab, "title");
          break;
        case "currentURI":
          getter = () => {
            let url = SessionStore.getLazyTabValue(aTab, "url");
            return Services.io.newURI(url);
          };
          break;
        case "didStartLoadSinceLastUserTyping":
          getter = () => () => false;
          break;
        case "fullZoom":
        case "textZoom":
          getter = () => 1;
          break;
        case "getTabBrowser":
          getter = () => () => this;
          break;
        case "isRemoteBrowser":
          getter = () => browser.getAttribute("remote") == "true";
          break;
        case "permitUnload":
          getter = () => () => ({ permitUnload: true, timedOut: false });
          break;
        case "reload":
        case "reloadWithFlags":
          getter = () =>
            params => {
              // Wait for load handler to be instantiated before
              // initializing the reload.
              aTab.addEventListener("SSTabRestoring", () => {
                browser[name](params);
              }, { once: true });
              gBrowser._insertBrowser(aTab);
            };
          break;
        case "resumeMedia":
          getter = () =>
            () => {
              // No need to insert a browser, so we just call the browser's
              // method.
              aTab.addEventListener("SSTabRestoring", () => {
                browser[name]();
              }, { once: true });
            };
          break;
        case "userTypedValue":
        case "userTypedClear":
        case "mediaBlocked":
          getter = () => SessionStore.getLazyTabValue(aTab, name);
          break;
        default:
          getter = () => {
            if (AppConstants.NIGHTLY_BUILD) {
              let message =
                `[bug 1345098] Lazy browser prematurely inserted via '${name}' property access:\n`;
              console.log(message + new Error().stack);
            }
            this._insertBrowser(aTab);
            return browser[name];
          };
          setter = value => {
            if (AppConstants.NIGHTLY_BUILD) {
              let message =
                `[bug 1345098] Lazy browser prematurely inserted via '${name}' property access:\n`;
              console.log(message + new Error().stack);
            }
            this._insertBrowser(aTab);
            return browser[name] = value;
          };
      }
      Object.defineProperty(browser, name, {
        get: getter,
        set: setter,
        configurable: true,
        enumerable: true
      });
    }
  }

  _insertBrowser(aTab, aInsertedOnTabCreation) {
    "use strict";

    // If browser is already inserted or window is closed don't do anything.
    if (aTab.linkedPanel || window.closed) {
      return;
    }

    let browser = aTab.linkedBrowser;

    // If browser is a lazy browser, delete the substitute properties.
    if (this._browserBindingProperties[0] in browser) {
      for (let name of this._browserBindingProperties) {
        delete browser[name];
      }
    }

    let { uriIsAboutBlank, remoteType, usingPreloadedContent } =
    aTab._browserParams;
    delete aTab._browserParams;

    let notificationbox = this.getNotificationBox(browser);
    let uniqueId = this._generateUniquePanelID();
    notificationbox.id = uniqueId;
    aTab.linkedPanel = uniqueId;

    // Inject the <browser> into the DOM if necessary.
    if (!notificationbox.parentNode) {
      // NB: this appendChild call causes us to run constructors for the
      // browser element, which fires off a bunch of notifications. Some
      // of those notifications can cause code to run that inspects our
      // state, so it is important that the tab element is fully
      // initialized by this point.
      this.mPanelContainer.appendChild(notificationbox);
    }

    // wire up a progress listener for the new browser object.
    let tabListener = new TabProgressListener(aTab, browser, uriIsAboutBlank, usingPreloadedContent);
    const filter = Cc["@mozilla.org/appshell/component/browser-status-filter;1"]
      .createInstance(Ci.nsIWebProgress);
    filter.addProgressListener(tabListener, Ci.nsIWebProgress.NOTIFY_ALL);
    browser.webProgress.addProgressListener(filter, Ci.nsIWebProgress.NOTIFY_ALL);
    this._tabListeners.set(aTab, tabListener);
    this._tabFilters.set(aTab, filter);

    browser.droppedLinkHandler = handleDroppedLink;

    // We start our browsers out as inactive, and then maintain
    // activeness in the tab switcher.
    browser.docShellIsActive = false;

    // When addTab() is called with an URL that is not "about:blank" we
    // set the "nodefaultsrc" attribute that prevents a frameLoader
    // from being created as soon as the linked <browser> is inserted
    // into the DOM. We thus have to register the new outerWindowID
    // for non-remote browsers after we have called browser.loadURI().
    if (remoteType == E10SUtils.NOT_REMOTE) {
      this._outerWindowIDBrowserMap.set(browser.outerWindowID, browser);
    }

    var evt = new CustomEvent("TabBrowserInserted", { bubbles: true, detail: { insertedOnTabCreation: aInsertedOnTabCreation } });
    aTab.dispatchEvent(evt);
  }

  discardBrowser(aBrowser, aForceDiscard) {
    "use strict";

    let tab = this.getTabForBrowser(aBrowser);

    let permitUnloadFlags = aForceDiscard ? aBrowser.dontPromptAndUnload : aBrowser.dontPromptAndDontUnload;

    if (!tab ||
      tab.selected ||
      tab.closing ||
      this._windowIsClosing ||
      !aBrowser.isConnected ||
      !aBrowser.isRemoteBrowser ||
      !aBrowser.permitUnload(permitUnloadFlags).permitUnload) {
      return;
    }

    // Set browser parameters for when browser is restored.  Also remove
    // listeners and set up lazy restore data in SessionStore. This must
    // be done before aBrowser is destroyed and removed from the document.
    tab._browserParams = {
      uriIsAboutBlank: aBrowser.currentURI.spec == "about:blank",
      remoteType: aBrowser.remoteType,
      usingPreloadedContent: false
    };

    SessionStore.resetBrowserToLazyState(tab);

    this._outerWindowIDBrowserMap.delete(aBrowser.outerWindowID);

    // Remove the tab's filter and progress listener.
    let filter = this._tabFilters.get(tab);
    let listener = this._tabListeners.get(tab);
    aBrowser.webProgress.removeProgressListener(filter);
    filter.removeProgressListener(listener);
    listener.destroy();

    this._tabListeners.delete(tab);
    this._tabFilters.delete(tab);

    // Reset the findbar and remove it if it is attached to the tab.
    if (tab._findBar) {
      tab._findBar.close(true);
      tab._findBar.remove();
      delete tab._findBar;
    }

    aBrowser.destroy();

    let notificationbox = this.getNotificationBox(aBrowser);
    this.mPanelContainer.removeChild(notificationbox);
    tab.removeAttribute("linkedpanel");

    this._createLazyBrowser(tab);

    let evt = new CustomEvent("TabBrowserDiscarded", { bubbles: true });
    tab.dispatchEvent(evt);
  }

  // eslint-disable-next-line complexity
  addTab(aURI, aReferrerURI, aCharset, aPostData, aOwner, aAllowThirdPartyFixup) {
    "use strict";

    const NS_XUL = "http://www.mozilla.org/keymaster/gatekeeper/there.is.only.xul";
    var aTriggeringPrincipal;
    var aReferrerPolicy;
    var aFromExternal;
    var aRelatedToCurrent;
    var aSkipAnimation;
    var aAllowMixedContent;
    var aForceNotRemote;
    var aPreferredRemoteType;
    var aNoReferrer;
    var aUserContextId;
    var aEventDetail;
    var aSameProcessAsFrameLoader;
    var aOriginPrincipal;
    var aDisallowInheritPrincipal;
    var aOpener;
    var aOpenerBrowser;
    var aCreateLazyBrowser;
    var aSkipBackgroundNotify;
    var aNextTabParentId;
    var aNoInitialLabel;
    var aFocusUrlBar;
    var aName;
    if (arguments.length == 2 &&
        typeof arguments[1] == "object" &&
        !(arguments[1] instanceof Ci.nsIURI)) {
      let params = arguments[1];
      aTriggeringPrincipal = params.triggeringPrincipal;
      aReferrerURI = params.referrerURI;
      aReferrerPolicy = params.referrerPolicy;
      aCharset = params.charset;
      aPostData = params.postData;
      aOwner = params.ownerTab;
      aAllowThirdPartyFixup = params.allowThirdPartyFixup;
      aFromExternal = params.fromExternal;
      aRelatedToCurrent = params.relatedToCurrent;
      aSkipAnimation = params.skipAnimation;
      aAllowMixedContent = params.allowMixedContent;
      aForceNotRemote = params.forceNotRemote;
      aPreferredRemoteType = params.preferredRemoteType;
      aNoReferrer = params.noReferrer;
      aUserContextId = params.userContextId;
      aEventDetail = params.eventDetail;
      aSameProcessAsFrameLoader = params.sameProcessAsFrameLoader;
      aOriginPrincipal = params.originPrincipal;
      aDisallowInheritPrincipal = params.disallowInheritPrincipal;
      aOpener = params.opener;
      aOpenerBrowser = params.openerBrowser;
      aCreateLazyBrowser = params.createLazyBrowser;
      aSkipBackgroundNotify = params.skipBackgroundNotify;
      aNextTabParentId = params.nextTabParentId;
      aNoInitialLabel = params.noInitialLabel;
      aFocusUrlBar = params.focusUrlBar;
      aName = params.name;
    }

    // if we're adding tabs, we're past interrupt mode, ditch the owner
    if (this.mCurrentTab.owner)
      this.mCurrentTab.owner = null;

    // Find the tab that opened this one, if any. This is used for
    // determining positioning, and inherited attributes such as the
    // user context ID.
    //
    // If we have a browser opener (which is usually the browser
    // element from a remote window.open() call), use that.
    //
    // Otherwise, if the tab is related to the current tab (e.g.,
    // because it was opened by a link click), use the selected tab as
    // the owner. If aReferrerURI is set, and we don't have an
    // explicit relatedToCurrent arg, we assume that the tab is
    // related to the current tab, since aReferrerURI is null or
    // undefined if the tab is opened from an external application or
    // bookmark (i.e. somewhere other than an existing tab).
    let relatedToCurrent = aRelatedToCurrent == null ? !!aReferrerURI : aRelatedToCurrent;
    let openerTab = ((aOpenerBrowser && this.getTabForBrowser(aOpenerBrowser)) ||
      (relatedToCurrent && this.selectedTab));

    var t = document.createElementNS(NS_XUL, "tab");

    t.openerTab = openerTab;

    aURI = aURI || "about:blank";
    let aURIObject = null;
    try {
      aURIObject = Services.io.newURI(aURI);
    } catch (ex) { /* we'll try to fix up this URL later */ }

    let lazyBrowserURI;
    if (aCreateLazyBrowser && aURI != "about:blank") {
      lazyBrowserURI = aURIObject;
      aURI = "about:blank";
    }

    var uriIsAboutBlank = aURI == "about:blank";

    if (!aNoInitialLabel) {
      if (isBlankPageURL(aURI)) {
        t.setAttribute("label", gTabBrowserBundle.GetStringFromName("tabs.emptyTabTitle"));
      } else {
        // Set URL as label so that the tab isn't empty initially.
        this.setInitialTabTitle(t, aURI, { beforeTabOpen: true });
      }
    }

    // Related tab inherits current tab's user context unless a different
    // usercontextid is specified
    if (aUserContextId == null && openerTab) {
      aUserContextId = openerTab.getAttribute("usercontextid") || 0;
    }

    if (aUserContextId) {
      t.setAttribute("usercontextid", aUserContextId);
      ContextualIdentityService.setTabStyle(t);
    }

    t.setAttribute("onerror", "this.removeAttribute('image');");

    if (aSkipBackgroundNotify) {
      t.setAttribute("skipbackgroundnotify", true);
    }

    t.className = "tabbrowser-tab";

    this.tabContainer._unlockTabSizing();

    // When overflowing, new tabs are scrolled into view smoothly, which
    // doesn't go well together with the width transition. So we skip the
    // transition in that case.
    let animate = !aSkipAnimation &&
      this.tabContainer.getAttribute("overflow") != "true" &&
      this.animationsEnabled;
    if (!animate) {
      t.setAttribute("fadein", "true");

      // Call _handleNewTab asynchronously as it needs to know if the
      // new tab is selected.
      setTimeout(function(tabContainer) {
        tabContainer._handleNewTab(t);
      }, 0, this.tabContainer);
    }

    // invalidate cache
    this._visibleTabs = null;

    this.tabContainer.appendChild(t);

    let usingPreloadedContent = false;
    let b;

    try {
      // If this new tab is owned by another, assert that relationship
      if (aOwner)
        t.owner = aOwner;

      var position = this.tabs.length - 1;
      t._tPos = position;
      this.tabContainer._setPositionalAttributes();

      this.tabContainer.updateVisibility();

      // If we don't have a preferred remote type, and we have a remote
      // opener, use the opener's remote type.
      if (!aPreferredRemoteType && aOpenerBrowser) {
        aPreferredRemoteType = aOpenerBrowser.remoteType;
      }

      // If URI is about:blank and we don't have a preferred remote type,
      // then we need to use the referrer, if we have one, to get the
      // correct remote type for the new tab.
      if (uriIsAboutBlank && !aPreferredRemoteType && aReferrerURI) {
        aPreferredRemoteType =
          E10SUtils.getRemoteTypeForURI(aReferrerURI.spec,
            gMultiProcessBrowser);
      }

      let remoteType =
        aForceNotRemote ? E10SUtils.NOT_REMOTE :
        E10SUtils.getRemoteTypeForURI(aURI, gMultiProcessBrowser,
          aPreferredRemoteType);

      // If we open a new tab with the newtab URL in the default
      // userContext, check if there is a preloaded browser ready.
      // Private windows are not included because both the label and the
      // icon for the tab would be set incorrectly (see bug 1195981).
      if (aURI == BROWSER_NEW_TAB_URL &&
          !aUserContextId &&
          !PrivateBrowsingUtils.isWindowPrivate(window)) {
        b = this._getPreloadedBrowser();
        if (b) {
          usingPreloadedContent = true;
        }
      }

      if (!b) {
        // No preloaded browser found, create one.
        b = this._createBrowser({
          remoteType,
          uriIsAboutBlank,
          userContextId: aUserContextId,
          sameProcessAsFrameLoader: aSameProcessAsFrameLoader,
          openerWindow: aOpener,
          nextTabParentId: aNextTabParentId,
          name: aName
        });
      }

      t.linkedBrowser = b;

      if (aFocusUrlBar) {
        b._urlbarFocused = true;
      }

      this._tabForBrowser.set(b, t);
      t.permanentKey = b.permanentKey;
      t._browserParams = {
        uriIsAboutBlank,
        remoteType,
        usingPreloadedContent
      };

      // If the caller opts in, create a lazy browser.
      if (aCreateLazyBrowser) {
        this._createLazyBrowser(t);

        if (lazyBrowserURI) {
          // Lazy browser must be explicitly registered so tab will appear as
          // a switch-to-tab candidate in autocomplete.
          this._unifiedComplete.registerOpenPage(lazyBrowserURI, aUserContextId);
          b.registeredOpenURI = lazyBrowserURI;
        }
      } else {
        this._insertBrowser(t, true);
      }
    } catch (e) {
      Cu.reportError("Failed to create tab");
      Cu.reportError(e);
      t.remove();
      if (t.linkedBrowser) {
        this._tabFilters.delete(t);
        this._tabListeners.delete(t);
        let notificationbox = this.getNotificationBox(t.linkedBrowser);
        notificationbox.remove();
      }
      throw e;
    }

    // Hack to ensure that the about:newtab favicon is loaded
    // instantaneously, to avoid flickering and improve perceived performance.
    if (aURI == "about:newtab") {
      this.setIcon(t, "chrome://branding/content/icon32.png");
    } else if (aURI == "about:privatebrowsing") {
      this.setIcon(t, "chrome://browser/skin/privatebrowsing/favicon.svg");
    }

    // Dispatch a new tab notification.  We do this once we're
    // entirely done, so that things are in a consistent state
    // even if the event listener opens or closes tabs.
    var detail = aEventDetail || {};
    var evt = new CustomEvent("TabOpen", { bubbles: true, detail });
    t.dispatchEvent(evt);

    if (!usingPreloadedContent && aOriginPrincipal && aURI) {
      let { URI_INHERITS_SECURITY_CONTEXT } = Ci.nsIProtocolHandler;
      // Unless we know for sure we're not inheriting principals,
      // force the about:blank viewer to have the right principal:
      if (!aURIObject ||
        (doGetProtocolFlags(aURIObject) & URI_INHERITS_SECURITY_CONTEXT)) {
        b.createAboutBlankContentViewer(aOriginPrincipal);
      }
    }

    // If we didn't swap docShells with a preloaded browser
    // then let's just continue loading the page normally.
    if (!usingPreloadedContent && (!uriIsAboutBlank || aDisallowInheritPrincipal)) {
      // pretend the user typed this so it'll be available till
      // the document successfully loads
      if (aURI && !gInitialPages.includes(aURI))
        b.userTypedValue = aURI;

      let flags = Ci.nsIWebNavigation.LOAD_FLAGS_NONE;
      if (aAllowThirdPartyFixup) {
        flags |= Ci.nsIWebNavigation.LOAD_FLAGS_ALLOW_THIRD_PARTY_FIXUP;
        flags |= Ci.nsIWebNavigation.LOAD_FLAGS_FIXUP_SCHEME_TYPOS;
      }
      if (aFromExternal)
        flags |= Ci.nsIWebNavigation.LOAD_FLAGS_FROM_EXTERNAL;
      if (aAllowMixedContent)
        flags |= Ci.nsIWebNavigation.LOAD_FLAGS_ALLOW_MIXED_CONTENT;
      if (aDisallowInheritPrincipal)
        flags |= Ci.nsIWebNavigation.LOAD_FLAGS_DISALLOW_INHERIT_PRINCIPAL;
      try {
        b.loadURIWithFlags(aURI, {
          flags,
          triggeringPrincipal: aTriggeringPrincipal,
          referrerURI: aNoReferrer ? null : aReferrerURI,
          referrerPolicy: aReferrerPolicy,
          charset: aCharset,
          postData: aPostData,
        });
      } catch (ex) {
        Cu.reportError(ex);
      }
    }

    // Move the new tab after another tab if needed.
    if ((openerTab &&
         Services.prefs.getBoolPref("browser.tabs.insertRelatedAfterCurrent")) ||
         Services.prefs.getBoolPref("browser.tabs.insertAfterCurrent")) {

    let lastRelatedTab = openerTab && this._lastRelatedTabMap.get(openerTab);
    let newTabPos = (lastRelatedTab || openerTab || this.mCurrentTab)._tPos + 1;

    if (lastRelatedTab)
      lastRelatedTab.owner = null;
    else if (openerTab)
      t.owner = openerTab;
    this.moveTabTo(t, newTabPos, true);
    if (openerTab)
      this._lastRelatedTabMap.set(openerTab, t);
  }

    // This field is updated regardless if we actually animate
    // since it's important that we keep this count correct in all cases.
    this.tabAnimationsInProgress++;

    if (animate) {
      requestAnimationFrame(function() {
        // kick the animation off
        t.setAttribute("fadein", "true");
      });
    }

    return t;
  }

  warnAboutClosingTabs(aCloseTabs, aTab) {
    var tabsToClose;
    switch (aCloseTabs) {
      case this.closingTabsEnum.ALL:
        tabsToClose = this.tabs.length - this._removingTabs.length -
          gBrowser._numPinnedTabs;
        break;
      case this.closingTabsEnum.OTHER:
        tabsToClose = this.visibleTabs.length - 1 - gBrowser._numPinnedTabs;
        break;
      case this.closingTabsEnum.TO_END:
        if (!aTab)
          throw new Error("Required argument missing: aTab");

        tabsToClose = this.getTabsToTheEndFrom(aTab).length;
        break;
      default:
        throw new Error("Invalid argument: " + aCloseTabs);
    }

    if (tabsToClose <= 1)
      return true;

    const pref = aCloseTabs == this.closingTabsEnum.ALL ?
      "browser.tabs.warnOnClose" : "browser.tabs.warnOnCloseOtherTabs";
    var shouldPrompt = Services.prefs.getBoolPref(pref);
    if (!shouldPrompt)
      return true;

    var ps = Services.prompt;

    // default to true: if it were false, we wouldn't get this far
    var warnOnClose = { value: true };

    // focus the window before prompting.
    // this will raise any minimized window, which will
    // make it obvious which window the prompt is for and will
    // solve the problem of windows "obscuring" the prompt.
    // see bug #350299 for more details
    window.focus();
    var warningMessage =
      PluralForm.get(tabsToClose, gTabBrowserBundle.GetStringFromName("tabs.closeWarningMultiple"))
      .replace("#1", tabsToClose);
    var buttonPressed =
      ps.confirmEx(window,
        gTabBrowserBundle.GetStringFromName("tabs.closeWarningTitle"),
        warningMessage,
        (ps.BUTTON_TITLE_IS_STRING * ps.BUTTON_POS_0) +
        (ps.BUTTON_TITLE_CANCEL * ps.BUTTON_POS_1),
        gTabBrowserBundle.GetStringFromName("tabs.closeButtonMultiple"),
        null, null,
        aCloseTabs == this.closingTabsEnum.ALL ?
        gTabBrowserBundle.GetStringFromName("tabs.closeWarningPromptMe") : null,
        warnOnClose);
    var reallyClose = (buttonPressed == 0);

    // don't set the pref unless they press OK and it's false
    if (aCloseTabs == this.closingTabsEnum.ALL && reallyClose && !warnOnClose.value)
      Services.prefs.setBoolPref(pref, false);

    return reallyClose;
  }

  getTabsToTheEndFrom(aTab) {
    let tabsToEnd = [];
    let tabs = this.visibleTabs;
    for (let i = tabs.length - 1; i >= 0; --i) {
      if (tabs[i] == aTab || tabs[i].pinned) {
        break;
      }
      tabsToEnd.push(tabs[i]);
    }
    return tabsToEnd;
  }

  removeTabsToTheEndFrom(aTab, aParams) {
    if (!this.warnAboutClosingTabs(this.closingTabsEnum.TO_END, aTab))
      return;

    let removeTab = tab => {
      // Avoid changing the selected browser several times.
      if (tab.selected)
        this.selectedTab = aTab;

      this.removeTab(tab, aParams);
    };

    let tabs = this.getTabsToTheEndFrom(aTab);
    let tabsWithBeforeUnload = [];
    for (let i = tabs.length - 1; i >= 0; --i) {
      let tab = tabs[i];
      if (this._hasBeforeUnload(tab))
        tabsWithBeforeUnload.push(tab);
      else
        removeTab(tab);
    }
    tabsWithBeforeUnload.forEach(removeTab);
  }

  removeAllTabsBut(aTab) {
    if (!this.warnAboutClosingTabs(this.closingTabsEnum.OTHER)) {
      return;
    }

    let tabs = this.visibleTabs.reverse();
    this.selectedTab = aTab;

    let tabsWithBeforeUnload = [];
    for (let i = tabs.length - 1; i >= 0; --i) {
      let tab = tabs[i];
      if (tab != aTab && !tab.pinned) {
        if (this._hasBeforeUnload(tab))
          tabsWithBeforeUnload.push(tab);
        else
          this.removeTab(tab, { animate: true });
      }
    }
    for (let tab of tabsWithBeforeUnload) {
      this.removeTab(tab, { animate: true });
    }
  }

  removeCurrentTab(aParams) {
    this.removeTab(this.mCurrentTab, aParams);
  }

  removeTab(aTab, aParams) {
    if (aParams) {
      var animate = aParams.animate;
      var byMouse = aParams.byMouse;
      var skipPermitUnload = aParams.skipPermitUnload;
    }

    // Telemetry stopwatches may already be running if removeTab gets
    // called again for an already closing tab.
    if (!TelemetryStopwatch.running("FX_TAB_CLOSE_TIME_ANIM_MS", aTab) &&
        !TelemetryStopwatch.running("FX_TAB_CLOSE_TIME_NO_ANIM_MS", aTab)) {
      // Speculatevely start both stopwatches now. We'll cancel one of
      // the two later depending on whether we're animating.
      TelemetryStopwatch.start("FX_TAB_CLOSE_TIME_ANIM_MS", aTab);
      TelemetryStopwatch.start("FX_TAB_CLOSE_TIME_NO_ANIM_MS", aTab);
    }
    window.maybeRecordAbandonmentTelemetry(aTab, "tabClosed");

    // Handle requests for synchronously removing an already
    // asynchronously closing tab.
    if (!animate &&
        aTab.closing) {
      this._endRemoveTab(aTab);
      return;
    }

    var isLastTab = (this.tabs.length - this._removingTabs.length == 1);

    if (!this._beginRemoveTab(aTab, null, null, true, skipPermitUnload)) {
      TelemetryStopwatch.cancel("FX_TAB_CLOSE_TIME_ANIM_MS", aTab);
      TelemetryStopwatch.cancel("FX_TAB_CLOSE_TIME_NO_ANIM_MS", aTab);
      return;
    }

    if (!aTab.pinned && !aTab.hidden && aTab._fullyOpen && byMouse)
      this.tabContainer._lockTabSizing(aTab);
    else
      this.tabContainer._unlockTabSizing();

    if (!animate /* the caller didn't opt in */ ||
      isLastTab ||
      aTab.pinned ||
      aTab.hidden ||
      this._removingTabs.length > 3 /* don't want lots of concurrent animations */ ||
      aTab.getAttribute("fadein") != "true" /* fade-in transition hasn't been triggered yet */ ||
      window.getComputedStyle(aTab).maxWidth == "0.1px" /* fade-in transition hasn't moved yet */ ||
      !this.animationsEnabled) {
      // We're not animating, so we can cancel the animation stopwatch.
      TelemetryStopwatch.cancel("FX_TAB_CLOSE_TIME_ANIM_MS", aTab);
      this._endRemoveTab(aTab);
      return;
    }

    // We're animating, so we can cancel the non-animation stopwatch.
    TelemetryStopwatch.cancel("FX_TAB_CLOSE_TIME_NO_ANIM_MS", aTab);

    aTab.style.maxWidth = ""; // ensure that fade-out transition happens
    aTab.removeAttribute("fadein");
    aTab.removeAttribute("bursting");

    setTimeout(function(tab, tabbrowser) {
      if (tab.parentNode &&
          window.getComputedStyle(tab).maxWidth == "0.1px") {
        NS_ASSERT(false, "Giving up waiting for the tab closing animation to finish (bug 608589)");
        tabbrowser._endRemoveTab(tab);
      }
    }, 3000, aTab, this);
  }

  _hasBeforeUnload(aTab) {
    let browser = aTab.linkedBrowser;
    return browser.isRemoteBrowser && browser.frameLoader &&
           browser.frameLoader.tabParent &&
           browser.frameLoader.tabParent.hasBeforeUnload;
  }

  _beginRemoveTab(aTab, aAdoptedByTab, aCloseWindowWithLastTab, aCloseWindowFastpath, aSkipPermitUnload) {
    if (aTab.closing ||
      this._windowIsClosing)
      return false;

    var browser = this.getBrowserForTab(aTab);
    if (!aSkipPermitUnload && !aAdoptedByTab &&
        aTab.linkedPanel && !aTab._pendingPermitUnload &&
        (!browser.isRemoteBrowser || this._hasBeforeUnload(aTab))) {
      TelemetryStopwatch.start("FX_TAB_CLOSE_PERMIT_UNLOAD_TIME_MS", aTab);

      // We need to block while calling permitUnload() because it
      // processes the event queue and may lead to another removeTab()
      // call before permitUnload() returns.
      aTab._pendingPermitUnload = true;
      let { permitUnload, timedOut } = browser.permitUnload();
      delete aTab._pendingPermitUnload;

      TelemetryStopwatch.finish("FX_TAB_CLOSE_PERMIT_UNLOAD_TIME_MS", aTab);

      // If we were closed during onbeforeunload, we return false now
      // so we don't (try to) close the same tab again. Of course, we
      // also stop if the unload was cancelled by the user:
      if (aTab.closing || (!timedOut && !permitUnload)) {
        return false;
      }
    }

    this._blurTab(aTab);

    var closeWindow = false;
    var newTab = false;
    if (this.tabs.length - this._removingTabs.length == 1) {
      closeWindow = aCloseWindowWithLastTab != null ? aCloseWindowWithLastTab :
        !window.toolbar.visible ||
        Services.prefs.getBoolPref("browser.tabs.closeWindowWithLastTab");

      if (closeWindow) {
        // We've already called beforeunload on all the relevant tabs if we get here,
        // so avoid calling it again:
        window.skipNextCanClose = true;
      }

      // Closing the tab and replacing it with a blank one is notably slower
      // than closing the window right away. If the caller opts in, take
      // the fast path.
      if (closeWindow &&
          aCloseWindowFastpath &&
          this._removingTabs.length == 0) {
        // This call actually closes the window, unless the user
        // cancels the operation.  We are finished here in both cases.
        this._windowIsClosing = window.closeWindow(true, window.warnAboutClosingWindow);
        return false;
      }

      newTab = true;
    }
    aTab._endRemoveArgs = [closeWindow, newTab];

    // swapBrowsersAndCloseOther will take care of closing the window without animation.
    if (closeWindow && aAdoptedByTab) {
      // Remove the tab's filter to avoid leaking.
      if (aTab.linkedPanel) {
        this._tabFilters.delete(aTab);
      }
      return true;
    }

    if (!aTab._fullyOpen) {
      // If the opening tab animation hasn't finished before we start closing the
      // tab, decrement the animation count since _handleNewTab will not get called.
      this.tabAnimationsInProgress--;
    }

    this.tabAnimationsInProgress++;

    // Mute audio immediately to improve perceived speed of tab closure.
    if (!aAdoptedByTab && aTab.hasAttribute("soundplaying")) {
      // Don't persist the muted state as this wasn't a user action.
      // This lets undo-close-tab return it to an unmuted state.
      aTab.linkedBrowser.mute(true);
    }

    aTab.closing = true;
    this._removingTabs.push(aTab);
    this._visibleTabs = null; // invalidate cache

    // Invalidate hovered tab state tracking for this closing tab.
    if (this.tabContainer._hoveredTab == aTab)
      aTab._mouseleave();

    if (newTab)
      this.addTab(BROWSER_NEW_TAB_URL, { skipAnimation: true });
    else
      this.tabContainer.updateVisibility();

    // We're committed to closing the tab now.
    // Dispatch a notification.
    // We dispatch it before any teardown so that event listeners can
    // inspect the tab that's about to close.
    var evt = new CustomEvent("TabClose", { bubbles: true, detail: { adoptedBy: aAdoptedByTab } });
    aTab.dispatchEvent(evt);

    if (aTab.linkedPanel) {
      if (!aAdoptedByTab && !gMultiProcessBrowser) {
        // Prevent this tab from showing further dialogs, since we're closing it
        var windowUtils = browser.contentWindow.QueryInterface(Ci.nsIInterfaceRequestor)
                                               .getInterface(Ci.nsIDOMWindowUtils);
        windowUtils.disableDialogs();
      }

      // Remove the tab's filter and progress listener.
      const filter = this._tabFilters.get(aTab);

      browser.webProgress.removeProgressListener(filter);

      const listener = this._tabListeners.get(aTab);
      filter.removeProgressListener(listener);
      listener.destroy();
    }

    if (browser.registeredOpenURI && !aAdoptedByTab) {
      this._unifiedComplete.unregisterOpenPage(browser.registeredOpenURI,
        browser.getAttribute("usercontextid") || 0);
      delete browser.registeredOpenURI;
    }

    // We are no longer the primary content area.
    browser.removeAttribute("primary");

    // Remove this tab as the owner of any other tabs, since it's going away.
    for (let tab of this.tabs) {
      if ("owner" in tab && tab.owner == aTab)
        // |tab| is a child of the tab we're removing, make it an orphan
        tab.owner = null;
    }

    return true;
  }

  _endRemoveTab(aTab) {
    if (!aTab || !aTab._endRemoveArgs)
      return;

    var [aCloseWindow, aNewTab] = aTab._endRemoveArgs;
    aTab._endRemoveArgs = null;

    if (this._windowIsClosing) {
      aCloseWindow = false;
      aNewTab = false;
    }

    this.tabAnimationsInProgress--;

    this._lastRelatedTabMap = new WeakMap();

    // update the UI early for responsiveness
    aTab.collapsed = true;
    this._blurTab(aTab);

    this._removingTabs.splice(this._removingTabs.indexOf(aTab), 1);

    if (aCloseWindow) {
      this._windowIsClosing = true;
      while (this._removingTabs.length)
        this._endRemoveTab(this._removingTabs[0]);
    } else if (!this._windowIsClosing) {
      if (aNewTab)
        focusAndSelectUrlBar();

      // workaround for bug 345399
      this.tabContainer.arrowScrollbox._updateScrollButtonsDisabledState();
    }

    // We're going to remove the tab and the browser now.
    this._tabFilters.delete(aTab);
    this._tabListeners.delete(aTab);

    var browser = this.getBrowserForTab(aTab);

    if (aTab.linkedPanel) {
      this._outerWindowIDBrowserMap.delete(browser.outerWindowID);

      // Because of the way XBL works (fields just set JS
      // properties on the element) and the code we have in place
      // to preserve the JS objects for any elements that have
      // JS properties set on them, the browser element won't be
      // destroyed until the document goes away.  So we force a
      // cleanup ourselves.
      // This has to happen before we remove the child so that the
      // XBL implementation of nsIObserver still works.
      browser.destroy();
    }

    var wasPinned = aTab.pinned;

    // Remove the tab ...
    this.tabContainer.removeChild(aTab);

    // ... and fix up the _tPos properties immediately.
    for (let i = aTab._tPos; i < this.tabs.length; i++)
      this.tabs[i]._tPos = i;

    if (!this._windowIsClosing) {
      if (wasPinned)
        this.tabContainer._positionPinnedTabs();

      // update tab close buttons state
      this.tabContainer._updateCloseButtons();

      setTimeout(function(tabs) {
        tabs._lastTabClosedByMouse = false;
      }, 0, this.tabContainer);
    }

    // update tab positional properties and attributes
    this.selectedTab._selected = true;
    this.tabContainer._setPositionalAttributes();

    // Removing the panel requires fixing up selectedPanel immediately
    // (see below), which would be hindered by the potentially expensive
    // browser removal. So we remove the browser and the panel in two
    // steps.

    var panel = this.getNotificationBox(browser);

    // In the multi-process case, it's possible an asynchronous tab switch
    // is still underway. If so, then it's possible that the last visible
    // browser is the one we're in the process of removing. There's the
    // risk of displaying preloaded browsers that are at the end of the
    // deck if we remove the browser before the switch is complete, so
    // we alert the switcher in order to show a spinner instead.
    if (this._switcher) {
      this._switcher.onTabRemoved(aTab);
    }

    // This will unload the document. An unload handler could remove
    // dependant tabs, so it's important that the tabbrowser is now in
    // a consistent state (tab removed, tab positions updated, etc.).
    browser.remove();

    // Release the browser in case something is erroneously holding a
    // reference to the tab after its removal.
    this._tabForBrowser.delete(aTab.linkedBrowser);
    aTab.linkedBrowser = null;

    panel.remove();

    // closeWindow might wait an arbitrary length of time if we're supposed
    // to warn about closing the window, so we'll just stop the tab close
    // stopwatches here instead.
    TelemetryStopwatch.finish("FX_TAB_CLOSE_TIME_ANIM_MS", aTab,
      true /* aCanceledOkay */ );
    TelemetryStopwatch.finish("FX_TAB_CLOSE_TIME_NO_ANIM_MS", aTab,
      true /* aCanceledOkay */ );

    if (aCloseWindow)
      this._windowIsClosing = closeWindow(true, window.warnAboutClosingWindow);
  }

  _findTabToBlurTo(aTab) {
    if (!aTab.selected) {
      return null;
    }

    if (aTab.owner &&
        !aTab.owner.hidden &&
        !aTab.owner.closing &&
        Services.prefs.getBoolPref("browser.tabs.selectOwnerOnClose")) {
      return aTab.owner;
    }

    // Switch to a visible tab unless there aren't any others remaining
    let remainingTabs = this.visibleTabs;
    let numTabs = remainingTabs.length;
    if (numTabs == 0 || numTabs == 1 && remainingTabs[0] == aTab) {
      remainingTabs = Array.filter(this.tabs, function(tab) {
        return !tab.closing;
      }, this);
    }

    // Try to find a remaining tab that comes after the given tab
    let tab = aTab;
    do {
      tab = tab.nextSibling;
    } while (tab && !remainingTabs.includes(tab));

    if (!tab) {
      tab = aTab;

      do {
        tab = tab.previousSibling;
      } while (tab && !remainingTabs.includes(tab));
    }

    return tab;
  }

  _blurTab(aTab) {
    this.selectedTab = this._findTabToBlurTo(aTab);
  }

  swapBrowsersAndCloseOther(aOurTab, aOtherTab) {
    // Do not allow transfering a private tab to a non-private window
    // and vice versa.
    if (PrivateBrowsingUtils.isWindowPrivate(window) !=
      PrivateBrowsingUtils.isWindowPrivate(aOtherTab.ownerGlobal))
      return;

    let ourBrowser = this.getBrowserForTab(aOurTab);
    let otherBrowser = aOtherTab.linkedBrowser;

    // Can't swap between chrome and content processes.
    if (ourBrowser.isRemoteBrowser != otherBrowser.isRemoteBrowser)
      return;

    // Keep the userContextId if set on other browser
    if (otherBrowser.hasAttribute("usercontextid")) {
      ourBrowser.setAttribute("usercontextid", otherBrowser.getAttribute("usercontextid"));
    }

    // That's gBrowser for the other window, not the tab's browser!
    var remoteBrowser = aOtherTab.ownerGlobal.gBrowser;
    var isPending = aOtherTab.hasAttribute("pending");

    let otherTabListener = remoteBrowser._tabListeners.get(aOtherTab);
    let stateFlags = otherTabListener.mStateFlags;

    // Expedite the removal of the icon if it was already scheduled.
    if (aOtherTab._soundPlayingAttrRemovalTimer) {
      clearTimeout(aOtherTab._soundPlayingAttrRemovalTimer);
      aOtherTab._soundPlayingAttrRemovalTimer = 0;
      aOtherTab.removeAttribute("soundplaying");
      remoteBrowser._tabAttrModified(aOtherTab, ["soundplaying"]);
    }

    // First, start teardown of the other browser.  Make sure to not
    // fire the beforeunload event in the process.  Close the other
    // window if this was its last tab.
    if (!remoteBrowser._beginRemoveTab(aOtherTab, aOurTab, true))
      return;

    // If this is the last tab of the window, hide the window
    // immediately without animation before the docshell swap, to avoid
    // about:blank being painted.
    let [closeWindow] = aOtherTab._endRemoveArgs;
    if (closeWindow) {
      let win = aOtherTab.ownerGlobal;
      let dwu = win.QueryInterface(Ci.nsIInterfaceRequestor)
                   .getInterface(Ci.nsIDOMWindowUtils);
      dwu.suppressAnimation(true);
      // Only suppressing window animations isn't enough to avoid
      // an empty content area being painted.
      let baseWin = win.QueryInterface(Ci.nsIInterfaceRequestor)
                       .getInterface(Ci.nsIDocShell)
                       .QueryInterface(Ci.nsIDocShellTreeItem)
                       .treeOwner
                       .QueryInterface(Ci.nsIBaseWindow);
      baseWin.visibility = false;
    }

    let modifiedAttrs = [];
    if (aOtherTab.hasAttribute("muted")) {
      aOurTab.setAttribute("muted", "true");
      aOurTab.muteReason = aOtherTab.muteReason;
      ourBrowser.mute();
      modifiedAttrs.push("muted");
    }
    if (aOtherTab.hasAttribute("soundplaying")) {
      aOurTab.setAttribute("soundplaying", "true");
      modifiedAttrs.push("soundplaying");
    }
    if (aOtherTab.hasAttribute("usercontextid")) {
      aOurTab.setUserContextId(aOtherTab.getAttribute("usercontextid"));
      modifiedAttrs.push("usercontextid");
    }
    if (aOtherTab.hasAttribute("sharing")) {
      aOurTab.setAttribute("sharing", aOtherTab.getAttribute("sharing"));
      modifiedAttrs.push("sharing");
      aOurTab._sharingState = aOtherTab._sharingState;
      webrtcUI.swapBrowserForNotification(otherBrowser, ourBrowser);
    }

    SitePermissions.copyTemporaryPermissions(otherBrowser, ourBrowser);

    // If the other tab is pending (i.e. has not been restored, yet)
    // then do not switch docShells but retrieve the other tab's state
    // and apply it to our tab.
    if (isPending) {
      SessionStore.setTabState(aOurTab, SessionStore.getTabState(aOtherTab));

      // Make sure to unregister any open URIs.
      this._swapRegisteredOpenURIs(ourBrowser, otherBrowser);
    } else {
      // Workarounds for bug 458697
      // Icon might have been set on DOMLinkAdded, don't override that.
      if (!ourBrowser.mIconURL && otherBrowser.mIconURL)
        this.setIcon(aOurTab, otherBrowser.mIconURL, otherBrowser.contentPrincipal, otherBrowser.contentRequestContextID);
      var isBusy = aOtherTab.hasAttribute("busy");
      if (isBusy) {
        aOurTab.setAttribute("busy", "true");
        modifiedAttrs.push("busy");
        if (aOurTab.selected)
          this.mIsBusy = true;
      }

      this._swapBrowserDocShells(aOurTab, otherBrowser, Ci.nsIBrowser.SWAP_DEFAULT, stateFlags);
    }

    // Unregister the previously opened URI
    if (otherBrowser.registeredOpenURI) {
      this._unifiedComplete.unregisterOpenPage(otherBrowser.registeredOpenURI,
        otherBrowser.getAttribute("usercontextid") || 0);
      delete otherBrowser.registeredOpenURI;
    }

    // Handle findbar data (if any)
    let otherFindBar = aOtherTab._findBar;
    if (otherFindBar &&
        otherFindBar.findMode == otherFindBar.FIND_NORMAL) {
      let ourFindBar = this.getFindBar(aOurTab);
      ourFindBar._findField.value = otherFindBar._findField.value;
      if (!otherFindBar.hidden)
        ourFindBar.onFindCommand();
    }

    // Finish tearing down the tab that's going away.
    if (closeWindow) {
      aOtherTab.ownerGlobal.close();
    } else {
      remoteBrowser._endRemoveTab(aOtherTab);
    }

    this.setTabTitle(aOurTab);

    // If the tab was already selected (this happpens in the scenario
    // of replaceTabWithWindow), notify onLocationChange, etc.
    if (aOurTab.selected)
      this.updateCurrentBrowser(true);

    if (modifiedAttrs.length) {
      this._tabAttrModified(aOurTab, modifiedAttrs);
    }
  }

  swapBrowsers(aOurTab, aOtherTab, aFlags) {
    let otherBrowser = aOtherTab.linkedBrowser;
    let otherTabBrowser = otherBrowser.getTabBrowser();

    // We aren't closing the other tab so, we also need to swap its tablisteners.
    let filter = otherTabBrowser._tabFilters.get(aOtherTab);
    let tabListener = otherTabBrowser._tabListeners.get(aOtherTab);
    otherBrowser.webProgress.removeProgressListener(filter);
    filter.removeProgressListener(tabListener);

    // Perform the docshell swap through the common mechanism.
    this._swapBrowserDocShells(aOurTab, otherBrowser, aFlags);

    // Restore the listeners for the swapped in tab.
    tabListener = new otherTabBrowser.ownerGlobal.TabProgressListener(aOtherTab, otherBrowser, false, false);
    otherTabBrowser._tabListeners.set(aOtherTab, tabListener);

    const notifyAll = Ci.nsIWebProgress.NOTIFY_ALL;
    filter.addProgressListener(tabListener, notifyAll);
    otherBrowser.webProgress.addProgressListener(filter, notifyAll);
  }

  _swapBrowserDocShells(aOurTab, aOtherBrowser, aFlags, aStateFlags) {
    // aOurTab's browser needs to be inserted now if it hasn't already.
    this._insertBrowser(aOurTab);

    // Unhook our progress listener
    const filter = this._tabFilters.get(aOurTab);
    let tabListener = this._tabListeners.get(aOurTab);
    let ourBrowser = this.getBrowserForTab(aOurTab);
    ourBrowser.webProgress.removeProgressListener(filter);
    filter.removeProgressListener(tabListener);

    // Make sure to unregister any open URIs.
    this._swapRegisteredOpenURIs(ourBrowser, aOtherBrowser);

    // Unmap old outerWindowIDs.
    this._outerWindowIDBrowserMap.delete(ourBrowser.outerWindowID);
    let remoteBrowser = aOtherBrowser.ownerGlobal.gBrowser;
    if (remoteBrowser) {
      remoteBrowser._outerWindowIDBrowserMap.delete(aOtherBrowser.outerWindowID);
    }

    // If switcher is active, it will intercept swap events and
    // react as needed.
    if (!this._switcher) {
      aOtherBrowser.docShellIsActive = this.shouldActivateDocShell(ourBrowser);
    }

    // Swap the docshells
    ourBrowser.swapDocShells(aOtherBrowser);

    if (ourBrowser.isRemoteBrowser) {
      // Switch outerWindowIDs for remote browsers.
      let ourOuterWindowID = ourBrowser._outerWindowID;
      ourBrowser._outerWindowID = aOtherBrowser._outerWindowID;
      aOtherBrowser._outerWindowID = ourOuterWindowID;
    }

    // Register new outerWindowIDs.
    this._outerWindowIDBrowserMap.set(ourBrowser.outerWindowID, ourBrowser);
    if (remoteBrowser) {
      remoteBrowser._outerWindowIDBrowserMap.set(aOtherBrowser.outerWindowID, aOtherBrowser);
    }

    if (!(aFlags & Ci.nsIBrowser.SWAP_KEEP_PERMANENT_KEY)) {
      // Swap permanentKey properties.
      let ourPermanentKey = ourBrowser.permanentKey;
      ourBrowser.permanentKey = aOtherBrowser.permanentKey;
      aOtherBrowser.permanentKey = ourPermanentKey;
      aOurTab.permanentKey = ourBrowser.permanentKey;
      if (remoteBrowser) {
        let otherTab = remoteBrowser.getTabForBrowser(aOtherBrowser);
        if (otherTab) {
          otherTab.permanentKey = aOtherBrowser.permanentKey;
        }
      }
    }

    // Restore the progress listener
    tabListener = new TabProgressListener(aOurTab, ourBrowser, false, false, aStateFlags);
    this._tabListeners.set(aOurTab, tabListener);

    const notifyAll = Ci.nsIWebProgress.NOTIFY_ALL;
    filter.addProgressListener(tabListener, notifyAll);
    ourBrowser.webProgress.addProgressListener(filter, notifyAll);
  }

  _swapRegisteredOpenURIs(aOurBrowser, aOtherBrowser) {
    // Swap the registeredOpenURI properties of the two browsers
    let tmp = aOurBrowser.registeredOpenURI;
    delete aOurBrowser.registeredOpenURI;
    if (aOtherBrowser.registeredOpenURI) {
      aOurBrowser.registeredOpenURI = aOtherBrowser.registeredOpenURI;
      delete aOtherBrowser.registeredOpenURI;
    }
    if (tmp) {
      aOtherBrowser.registeredOpenURI = tmp;
    }
  }

  reloadAllTabs() {
    let tabs = this.visibleTabs;
    let l = tabs.length;
    for (var i = 0; i < l; i++) {
      try {
        this.getBrowserForTab(tabs[i]).reload();
      } catch (e) {
        // ignore failure to reload so others will be reloaded
      }
    }
  }

  reloadTab(aTab) {
    let browser = this.getBrowserForTab(aTab);
    // Reset temporary permissions on the current tab. This is done here
    // because we only want to reset permissions on user reload.
    SitePermissions.clearTemporaryPermissions(browser);
    browser.reload();
  }

  addProgressListener(aListener) {
    if (arguments.length != 1) {
      Cu.reportError("gBrowser.addProgressListener was " +
        "called with a second argument, " +
        "which is not supported. See bug " +
        "608628. Call stack: " + new Error().stack);
    }

    this.mProgressListeners.push(aListener);
  }

  removeProgressListener(aListener) {
    this.mProgressListeners =
      this.mProgressListeners.filter(l => l != aListener);
  }

  addTabsProgressListener(aListener) {
    this.mTabsProgressListeners.push(aListener);
  }

  removeTabsProgressListener(aListener) {
    this.mTabsProgressListeners =
      this.mTabsProgressListeners.filter(l => l != aListener);
  }

  getBrowserForTab(aTab) {
    return aTab.linkedBrowser;
  }

  showOnlyTheseTabs(aTabs) {
    for (let tab of this.tabs) {
      if (!aTabs.includes(tab))
        this.hideTab(tab);
      else
        this.showTab(tab);
    }

    this.tabContainer._handleTabSelect(true);
  }

  showTab(aTab) {
    if (aTab.hidden) {
      aTab.removeAttribute("hidden");
      this._visibleTabs = null; // invalidate cache

      this.tabContainer._updateCloseButtons();

      this.tabContainer._setPositionalAttributes();

      let event = document.createEvent("Events");
      event.initEvent("TabShow", true, false);
      aTab.dispatchEvent(event);
      SessionStore.deleteTabValue(aTab, "hiddenBy");
    }
  }

  hideTab(aTab, aSource) {
    if (!aTab.hidden && !aTab.pinned && !aTab.selected &&
        !aTab.closing && !aTab._sharingState) {
      aTab.setAttribute("hidden", "true");
      this._visibleTabs = null; // invalidate cache

      this.tabContainer._updateCloseButtons();

      this.tabContainer._setPositionalAttributes();

      let event = document.createEvent("Events");
      event.initEvent("TabHide", true, false);
      aTab.dispatchEvent(event);
      if (aSource) {
        SessionStore.setTabValue(aTab, "hiddenBy", aSource);
      }
    }
  }

  selectTabAtIndex(aIndex, aEvent) {
    let tabs = this.visibleTabs;

    // count backwards for aIndex < 0
    if (aIndex < 0) {
      aIndex += tabs.length;
      // clamp at index 0 if still negative.
      if (aIndex < 0)
        aIndex = 0;
    } else if (aIndex >= tabs.length) {
      // clamp at right-most tab if out of range.
      aIndex = tabs.length - 1;
    }

    this.selectedTab = tabs[aIndex];

    if (aEvent) {
      aEvent.preventDefault();
      aEvent.stopPropagation();
    }
  }

  /**
   * Moves a tab to a new browser window, unless it's already the only tab
   * in the current window, in which case this will do nothing.
   */
  replaceTabWithWindow(aTab, aOptions) {
    if (this.tabs.length == 1)
      return null;

    var options = "chrome,dialog=no,all";
    for (var name in aOptions)
      options += "," + name + "=" + aOptions[name];

    // Play the tab closing animation to give immediate feedback while
    // waiting for the new window to appear.
    // content area when the docshells are swapped.
    if (this.animationsEnabled) {
      aTab.style.maxWidth = ""; // ensure that fade-out transition happens
      aTab.removeAttribute("fadein");
    }

    // tell a new window to take the "dropped" tab
    return window.openDialog(getBrowserURL(), "_blank", options, aTab);
  }

  moveTabTo(aTab, aIndex, aKeepRelatedTabs) {
    var oldPosition = aTab._tPos;
    if (oldPosition == aIndex)
      return;

    // Don't allow mixing pinned and unpinned tabs.
    if (aTab.pinned)
      aIndex = Math.min(aIndex, this._numPinnedTabs - 1);
    else
      aIndex = Math.max(aIndex, this._numPinnedTabs);
    if (oldPosition == aIndex)
      return;

    if (!aKeepRelatedTabs) {
      this._lastRelatedTabMap = new WeakMap();
    }

    let wasFocused = (document.activeElement == this.mCurrentTab);

    aIndex = aIndex < aTab._tPos ? aIndex : aIndex + 1;

    // invalidate cache
    this._visibleTabs = null;

    // use .item() instead of [] because dragging to the end of the strip goes out of
    // bounds: .item() returns null (so it acts like appendChild), but [] throws
    this.tabContainer.insertBefore(aTab, this.tabs.item(aIndex));

    for (let i = 0; i < this.tabs.length; i++) {
      this.tabs[i]._tPos = i;
      this.tabs[i]._selected = false;
    }

    // If we're in the midst of an async tab switch while calling
    // moveTabTo, we can get into a case where _visuallySelected
    // is set to true on two different tabs.
    //
    // What we want to do in moveTabTo is to remove logical selection
    // from all tabs, and then re-add logical selection to mCurrentTab
    // (and visual selection as well if we're not running with e10s, which
    // setting _selected will do automatically).
    //
    // If we're running with e10s, then the visual selection will not
    // be changed, which is fine, since if we weren't in the midst of a
    // tab switch, the previously visually selected tab should still be
    // correct, and if we are in the midst of a tab switch, then the async
    // tab switcher will set the visually selected tab once the tab switch
    // has completed.
    this.mCurrentTab._selected = true;

    if (wasFocused)
      this.mCurrentTab.focus();

    this.tabContainer._handleTabSelect(true);

    if (aTab.pinned)
      this.tabContainer._positionPinnedTabs();

    this.tabContainer._setPositionalAttributes();

    var evt = document.createEvent("UIEvents");
    evt.initUIEvent("TabMove", true, false, window, oldPosition);
    aTab.dispatchEvent(evt);
  }

  moveTabForward() {
    let nextTab = this.mCurrentTab.nextSibling;
    while (nextTab && nextTab.hidden)
      nextTab = nextTab.nextSibling;

    if (nextTab)
      this.moveTabTo(this.mCurrentTab, nextTab._tPos);
    else if (this.arrowKeysShouldWrap)
      this.moveTabToStart();
  }

  /**
   * Adopts a tab from another browser window, and inserts it at aIndex
   */
  adoptTab(aTab, aIndex, aSelectTab) {
    // Swap the dropped tab with a new one we create and then close
    // it in the other window (making it seem to have moved between
    // windows). We also ensure that the tab we create to swap into has
    // the same remote type and process as the one we're swapping in.
    // This makes sure we don't get a short-lived process for the new tab.
    let linkedBrowser = aTab.linkedBrowser;
    let params = {
      eventDetail: { adoptedTab: aTab },
      preferredRemoteType: linkedBrowser.remoteType,
      sameProcessAsFrameLoader: linkedBrowser.frameLoader,
      skipAnimation: true
    };
    if (aTab.hasAttribute("usercontextid")) {
      // new tab must have the same usercontextid as the old one
      params.userContextId = aTab.getAttribute("usercontextid");
    }
    let newTab = this.addTab("about:blank", params);
    let newBrowser = this.getBrowserForTab(newTab);

    // Stop the about:blank load.
    newBrowser.stop();
    // Make sure it has a docshell.
    newBrowser.docShell;

    let numPinned = this._numPinnedTabs;
    if (aIndex < numPinned || (aTab.pinned && aIndex == numPinned)) {
      this.pinTab(newTab);
    }

    this.moveTabTo(newTab, aIndex);

    // We need to select the tab before calling swapBrowsersAndCloseOther
    // so that window.content in chrome windows points to the right tab
    // when pagehide/show events are fired. This is no longer necessary
    // for any exiting browser code, but it may be necessary for add-on
    // compatibility.
    if (aSelectTab) {
      this.selectedTab = newTab;
    }

    aTab.parentNode._finishAnimateTabMove();
    this.swapBrowsersAndCloseOther(newTab, aTab);

    if (aSelectTab) {
      // Call updateCurrentBrowser to make sure the URL bar is up to date
      // for our new tab after we've done swapBrowsersAndCloseOther.
      this.updateCurrentBrowser(true);
    }

    return newTab;
  }

  moveTabBackward() {
    let previousTab = this.mCurrentTab.previousSibling;
    while (previousTab && previousTab.hidden)
      previousTab = previousTab.previousSibling;

    if (previousTab)
      this.moveTabTo(this.mCurrentTab, previousTab._tPos);
    else if (this.arrowKeysShouldWrap)
      this.moveTabToEnd();
  }

  moveTabToStart() {
    var tabPos = this.mCurrentTab._tPos;
    if (tabPos > 0)
      this.moveTabTo(this.mCurrentTab, 0);
  }

  moveTabToEnd() {
    var tabPos = this.mCurrentTab._tPos;
    if (tabPos < this.browsers.length - 1)
      this.moveTabTo(this.mCurrentTab, this.browsers.length - 1);
  }

  moveTabOver(aEvent) {
    var direction = window.getComputedStyle(this.container.parentNode).direction;
    if ((direction == "ltr" && aEvent.keyCode == KeyEvent.DOM_VK_RIGHT) ||
      (direction == "rtl" && aEvent.keyCode == KeyEvent.DOM_VK_LEFT))
      this.moveTabForward();
    else
      this.moveTabBackward();
  }

  /**
   * @param   aTab
   *          Can be from a different window as well
   * @param   aRestoreTabImmediately
   *          Can defer loading of the tab contents
   */
  duplicateTab(aTab, aRestoreTabImmediately) {
    return SessionStore.duplicateTab(window, aTab, 0, aRestoreTabImmediately);
  }

  activateBrowserForPrintPreview(aBrowser) {
    this._printPreviewBrowsers.add(aBrowser);
    if (this._switcher) {
      this._switcher.activateBrowserForPrintPreview(aBrowser);
    }
    aBrowser.docShellIsActive = true;
  }

  deactivatePrintPreviewBrowsers() {
    let browsers = this._printPreviewBrowsers;
    this._printPreviewBrowsers = new Set();
    for (let browser of browsers) {
      browser.docShellIsActive = this.shouldActivateDocShell(browser);
    }
  }

  /**
   * Returns true if a given browser's docshell should be active.
   */
  shouldActivateDocShell(aBrowser) {
    if (this._switcher) {
      return this._switcher.shouldActivateDocShell(aBrowser);
    }
    return (aBrowser == this.selectedBrowser &&
            window.windowState != window.STATE_MINIMIZED &&
            !window.isFullyOccluded) ||
            this._printPreviewBrowsers.has(aBrowser);
  }

  /**
   * The tab switcher is responsible for asynchronously switching
   * tabs in e10s. It waits until the new tab is ready (i.e., the
   * layer tree is available) before switching to it. Then it
   * unloads the layer tree for the old tab.
   *
   * The tab switcher is a state machine. For each tab, it
   * maintains state about whether the layer tree for the tab is
   * available, being loaded, being unloaded, or unavailable. It
   * also keeps track of the tab currently being displayed, the tab
   * it's trying to load, and the tab the user has asked to switch
   * to. The switcher object is created upon tab switch. It is
   * released when there are no pending tabs to load or unload.
   *
   * The following general principles have guided the design:
   *
   * 1. We only request one layer tree at a time. If the user
   * switches to a different tab while waiting, we don't request
   * the new layer tree until the old tab has loaded or timed out.
   *
   * 2. If loading the layers for a tab times out, we show the
   * spinner and possibly request the layer tree for another tab if
   * the user has requested one.
   *
   * 3. We discard layer trees on a delay. This way, if the user is
   * switching among the same tabs frequently, we don't continually
   * load the same tabs.
   *
   * It's important that we always show either the spinner or a tab
   * whose layers are available. Otherwise the compositor will draw
   * an entirely black frame, which is very jarring. To ensure this
   * never happens when switching away from a tab, we assume the
   * old tab might still be drawn until a MozAfterPaint event
   * occurs. Because layout and compositing happen asynchronously,
   * we don't have any other way of knowing when the switch
   * actually takes place. Therefore, we don't unload the old tab
   * until the next MozAfterPaint event.
   */
  _getSwitcher() {
    if (this._switcher) {
      return this._switcher;
    }

    let switcher = {
      // How long to wait for a tab's layers to load. After this
      // time elapses, we're free to put up the spinner and start
      // trying to load a different tab.
      TAB_SWITCH_TIMEOUT: 400 /* ms */,

      // When the user hasn't switched tabs for this long, we unload
      // layers for all tabs that aren't in use.
      UNLOAD_DELAY: 300 /* ms */,

      // The next three tabs form the principal state variables.
      // See the assertions in postActions for their invariants.

      // Tab the user requested most recently.
      requestedTab: this.selectedTab,

      // Tab we're currently trying to load.
      loadingTab: null,

      // We show this tab in case the requestedTab hasn't loaded yet.
      lastVisibleTab: this.selectedTab,

      // Auxilliary state variables:

      visibleTab: this.selectedTab, // Tab that's on screen.
      spinnerTab: null, // Tab showing a spinner.
      blankTab: null, // Tab showing blank.
      lastPrimaryTab: this.selectedTab, // Tab with primary="true"

      tabbrowser: this, // Reference to gBrowser.
      loadTimer: null, // TAB_SWITCH_TIMEOUT nsITimer instance.
      unloadTimer: null, // UNLOAD_DELAY nsITimer instance.

      // Map from tabs to STATE_* (below).
      tabState: new Map(),

      // True if we're in the midst of switching tabs.
      switchInProgress: false,

      // Keep an exact list of content processes (tabParent) in which
      // we're actively suppressing the display port. This gives a robust
      // way to make sure we don't forget to un-suppress.
      activeSuppressDisplayport: new Set(),

      // Set of tabs that might be visible right now. We maintain
      // this set because we can't be sure when a tab is actually
      // drawn. A tab is added to this set when we ask to make it
      // visible. All tabs but the most recently shown tab are
      // removed from the set upon MozAfterPaint.
      maybeVisibleTabs: new Set([this.selectedTab]),

      // This holds onto the set of tabs that we've been asked to warm up.
      // This is used only for Telemetry and logging, and (in order to not
      // over-complicate the async tab switcher any further) has nothing to do
      // with how warmed tabs are loaded and unloaded.
      warmingTabs: new WeakSet(),

      STATE_UNLOADED: 0,
      STATE_LOADING: 1,
      STATE_LOADED: 2,
      STATE_UNLOADING: 3,

      // re-entrancy guard:
      _processing: false,

      // Wraps nsITimer. Must not use the vanilla setTimeout and
      // clearTimeout, because they will be blocked by nsIPromptService
      // dialogs.
      setTimer(callback, timeout) {
        let event = {
          notify: callback
        };

        var timer = Cc["@mozilla.org/timer;1"]
          .createInstance(Ci.nsITimer);
        timer.initWithCallback(event, timeout, Ci.nsITimer.TYPE_ONE_SHOT);
        return timer;
      },

      clearTimer(timer) {
        timer.cancel();
      },

      getTabState(tab) {
        let state = this.tabState.get(tab);

        // As an optimization, we lazily evaluate the state of tabs
        // that we've never seen before. Once we've figured it out,
        // we stash it in our state map.
        if (state === undefined) {
          state = this.STATE_UNLOADED;

          if (tab && tab.linkedPanel) {
            let b = tab.linkedBrowser;
            if (b.renderLayers && b.hasLayers) {
              state = this.STATE_LOADED;
            } else if (b.renderLayers && !b.hasLayers) {
              state = this.STATE_LOADING;
            } else if (!b.renderLayers && b.hasLayers) {
              state = this.STATE_UNLOADING;
            }
          }

          this.setTabStateNoAction(tab, state);
        }

        return state;
      },

      setTabStateNoAction(tab, state) {
        if (state == this.STATE_UNLOADED) {
          this.tabState.delete(tab);
        } else {
          this.tabState.set(tab, state);
        }
      },

      setTabState(tab, state) {
        if (state == this.getTabState(tab)) {
          return;
        }

        this.setTabStateNoAction(tab, state);

        let browser = tab.linkedBrowser;
        let { tabParent } = browser.frameLoader;
        if (state == this.STATE_LOADING) {
          this.assert(!this.minimizedOrFullyOccluded);

          if (!this.tabbrowser.tabWarmingEnabled) {
            browser.docShellIsActive = true;
          }

          if (tabParent) {
            browser.renderLayers = true;
          } else {
            this.onLayersReady(browser);
          }
        } else if (state == this.STATE_UNLOADING) {
          this.unwarmTab(tab);
          // Setting the docShell to be inactive will also cause it
          // to stop rendering layers.
          browser.docShellIsActive = false;
          if (!tabParent) {
            this.onLayersCleared(browser);
          }
        } else if (state == this.STATE_LOADED) {
          this.maybeActivateDocShell(tab);
        }

        if (!tab.linkedBrowser.isRemoteBrowser) {
          // setTabState is potentially re-entrant in the non-remote case,
          // so we must re-get the state for this assertion.
          let nonRemoteState = this.getTabState(tab);
          // Non-remote tabs can never stay in the STATE_LOADING
          // or STATE_UNLOADING states. By the time this function
          // exits, a non-remote tab must be in STATE_LOADED or
          // STATE_UNLOADED, since the painting and the layer
          // upload happen synchronously.
          this.assert(nonRemoteState == this.STATE_UNLOADED ||
            nonRemoteState == this.STATE_LOADED);
        }
      },

      get minimizedOrFullyOccluded() {
        return window.windowState == window.STATE_MINIMIZED ||
          window.isFullyOccluded;
      },

      init() {
        this.log("START");

        window.addEventListener("MozAfterPaint", this);
        window.addEventListener("MozLayerTreeReady", this);
        window.addEventListener("MozLayerTreeCleared", this);
        window.addEventListener("TabRemotenessChange", this);
        window.addEventListener("sizemodechange", this);
        window.addEventListener("occlusionstatechange", this);
        window.addEventListener("SwapDocShells", this, true);
        window.addEventListener("EndSwapDocShells", this, true);

        let initialTab = this.requestedTab;
        let initialBrowser = initialTab.linkedBrowser;

        let tabIsLoaded = !initialBrowser.isRemoteBrowser ||
          initialBrowser.frameLoader.tabParent.hasLayers;

        // If we minimized the window before the switcher was activated,
        // we might have set  the preserveLayers flag for the current
        // browser. Let's clear it.
        initialBrowser.preserveLayers(false);

        if (!this.minimizedOrFullyOccluded) {
          this.log("Initial tab is loaded?: " + tabIsLoaded);
          this.setTabState(initialTab, tabIsLoaded ? this.STATE_LOADED
                                                   : this.STATE_LOADING);
        }

        for (let ppBrowser of this.tabbrowser._printPreviewBrowsers) {
          let ppTab = this.tabbrowser.getTabForBrowser(ppBrowser);
          let state = ppBrowser.hasLayers ? this.STATE_LOADED
                                          : this.STATE_LOADING;
          this.setTabState(ppTab, state);
        }
      },

      destroy() {
        if (this.unloadTimer) {
          this.clearTimer(this.unloadTimer);
          this.unloadTimer = null;
        }
        if (this.loadTimer) {
          this.clearTimer(this.loadTimer);
          this.loadTimer = null;
        }

        window.removeEventListener("MozAfterPaint", this);
        window.removeEventListener("MozLayerTreeReady", this);
        window.removeEventListener("MozLayerTreeCleared", this);
        window.removeEventListener("TabRemotenessChange", this);
        window.removeEventListener("sizemodechange", this);
        window.removeEventListener("occlusionstatechange", this);
        window.removeEventListener("SwapDocShells", this, true);
        window.removeEventListener("EndSwapDocShells", this, true);

        this.tabbrowser._switcher = null;

        this.activeSuppressDisplayport.forEach(function(tabParent) {
          tabParent.suppressDisplayport(false);
        });
        this.activeSuppressDisplayport.clear();
      },

      finish() {
        this.log("FINISH");

        this.assert(this.tabbrowser._switcher);
        this.assert(this.tabbrowser._switcher === this);
        this.assert(!this.spinnerTab);
        this.assert(!this.blankTab);
        this.assert(!this.loadTimer);
        this.assert(!this.loadingTab);
        this.assert(this.lastVisibleTab === this.requestedTab);
        this.assert(this.minimizedOrFullyOccluded ||
          this.getTabState(this.requestedTab) == this.STATE_LOADED);

        this.destroy();

        document.commandDispatcher.unlock();

        let event = new CustomEvent("TabSwitchDone", {
          bubbles: true,
          cancelable: true
        });
        this.tabbrowser.dispatchEvent(event);
      },

      // This function is called after all the main state changes to
      // make sure we display the right tab.
      updateDisplay() {
        let requestedTabState = this.getTabState(this.requestedTab);
        let requestedBrowser = this.requestedTab.linkedBrowser;

        // It is often more desirable to show a blank tab when appropriate than
        // the tab switch spinner - especially since the spinner is usually
        // preceded by a perceived lag of TAB_SWITCH_TIMEOUT ms in the
        // tab switch. We can hide this lag, and hide the time being spent
        // constructing TabChild's, layer trees, etc, by showing a blank
        // tab instead and focusing it immediately.
        let shouldBeBlank = false;
        if (requestedBrowser.isRemoteBrowser) {
          // If a tab is remote and the window is not minimized, we can show a
          // blank tab instead of a spinner in the following cases:
          //
          // 1. The tab has just crashed, and we haven't started showing the
          //    tab crashed page yet (in this case, the TabParent is null)
          // 2. The tab has never presented, and has not finished loading
          //    a non-local-about: page.
          //
          // For (2), "finished loading a non-local-about: page" is
          // determined by the busy state on the tab element and checking
          // if the loaded URI is local.
          let hasSufficientlyLoaded = !this.requestedTab.hasAttribute("busy") &&
            !this.tabbrowser._isLocalAboutURI(requestedBrowser.currentURI);

          let fl = requestedBrowser.frameLoader;
          shouldBeBlank = !this.minimizedOrFullyOccluded &&
            (!fl.tabParent ||
              (!hasSufficientlyLoaded && !fl.tabParent.hasPresented));
        }

        this.log("Tab should be blank: " + shouldBeBlank);
        this.log("Requested tab is remote?: " + requestedBrowser.isRemoteBrowser);

        // Figure out which tab we actually want visible right now.
        let showTab = null;
        if (requestedTabState != this.STATE_LOADED &&
            this.lastVisibleTab && this.loadTimer && !shouldBeBlank) {
          // If we can't show the requestedTab, and lastVisibleTab is
          // available, show it.
          showTab = this.lastVisibleTab;
        } else {
          // Show the requested tab. If it's not available, we'll show the spinner or a blank tab.
          showTab = this.requestedTab;
        }

        // First, let's deal with blank tabs, which we show instead
        // of the spinner when the tab is not currently set up
        // properly in the content process.
        if (!shouldBeBlank && this.blankTab) {
          this.blankTab.linkedBrowser.removeAttribute("blank");
          this.blankTab = null;
        } else if (shouldBeBlank && this.blankTab !== showTab) {
          if (this.blankTab) {
            this.blankTab.linkedBrowser.removeAttribute("blank");
          }
          this.blankTab = showTab;
          this.blankTab.linkedBrowser.setAttribute("blank", "true");
        }

        // Show or hide the spinner as needed.
        let needSpinner = this.getTabState(showTab) != this.STATE_LOADED &&
                          !this.minimizedOrFullyOccluded &&
                          !shouldBeBlank;

        if (!needSpinner && this.spinnerTab) {
          this.spinnerHidden();
          this.tabbrowser.removeAttribute("pendingpaint");
          this.spinnerTab.linkedBrowser.removeAttribute("pendingpaint");
          this.spinnerTab = null;
        } else if (needSpinner && this.spinnerTab !== showTab) {
          if (this.spinnerTab) {
            this.spinnerTab.linkedBrowser.removeAttribute("pendingpaint");
          } else {
            this.spinnerDisplayed();
          }
          this.spinnerTab = showTab;
          this.tabbrowser.setAttribute("pendingpaint", "true");
          this.spinnerTab.linkedBrowser.setAttribute("pendingpaint", "true");
        }

        // Switch to the tab we've decided to make visible.
        if (this.visibleTab !== showTab) {
          this.tabbrowser._adjustFocusBeforeTabSwitch(this.visibleTab, showTab);
          this.visibleTab = showTab;

          this.maybeVisibleTabs.add(showTab);

          let tabs = this.tabbrowser.tabbox.tabs;
          let tabPanel = this.tabbrowser.mPanelContainer;
          let showPanel = tabs.getRelatedElement(showTab);
          let index = Array.indexOf(tabPanel.childNodes, showPanel);
          if (index != -1) {
            this.log(`Switch to tab ${index} - ${this.tinfo(showTab)}`);
            tabPanel.setAttribute("selectedIndex", index);
            if (showTab === this.requestedTab) {
              if (this._requestingTab) {
                /*
                 * If _requestingTab is set, that means that we're switching the
                 * visibility of the tab synchronously, and we need to wait for
                 * the "select" event before shifting focus so that
                 * _adjustFocusAfterTabSwitch runs with the right information for
                 * the tab switch.
                 */
                this.tabbrowser.addEventListener("select", () => {
                  this.tabbrowser._adjustFocusAfterTabSwitch(showTab);
                }, { once: true });
              } else {
                this.tabbrowser._adjustFocusAfterTabSwitch(showTab);
              }

              this.maybeActivateDocShell(this.requestedTab);
            }
          }

          // This doesn't necessarily exist if we're a new window and haven't switched tabs yet
          if (this.lastVisibleTab)
            this.lastVisibleTab._visuallySelected = false;

          this.visibleTab._visuallySelected = true;
        }

        this.lastVisibleTab = this.visibleTab;
      },

      assert(cond) {
        if (!cond) {
          dump("Assertion failure\n" + Error().stack);

          // Don't break a user's browser if an assertion fails.
          if (AppConstants.DEBUG) {
            throw new Error("Assertion failure");
          }
        }
      },

      // We've decided to try to load requestedTab.
      loadRequestedTab() {
        this.assert(!this.loadTimer);
        this.assert(!this.minimizedOrFullyOccluded);

        // loadingTab can be non-null here if we timed out loading the current tab.
        // In that case we just overwrite it with a different tab; it's had its chance.
        this.loadingTab = this.requestedTab;
        this.log("Loading tab " + this.tinfo(this.loadingTab));

        this.loadTimer = this.setTimer(() => this.onLoadTimeout(), this.TAB_SWITCH_TIMEOUT);
        this.setTabState(this.requestedTab, this.STATE_LOADING);
      },

      maybeActivateDocShell(tab) {
        // If we've reached the point where the requested tab has entered
        // the loaded state, but the DocShell is still not yet active, we
        // should activate it.
        let browser = tab.linkedBrowser;
        let state = this.getTabState(tab);
        let canCheckDocShellState = !browser.mDestroyed &&
          (browser.docShell || browser.frameLoader.tabParent);
        if (tab == this.requestedTab &&
            canCheckDocShellState &&
            state == this.STATE_LOADED &&
            !browser.docShellIsActive &&
            !this.minimizedOrFullyOccluded) {
          browser.docShellIsActive = true;
          this.logState("Set requested tab docshell to active and preserveLayers to false");
          // If we minimized the window before the switcher was activated,
          // we might have set the preserveLayers flag for the current
          // browser. Let's clear it.
          browser.preserveLayers(false);
        }
      },

      // This function runs before every event. It fixes up the state
      // to account for closed tabs.
      preActions() {
        this.assert(this.tabbrowser._switcher);
        this.assert(this.tabbrowser._switcher === this);

        for (let [tab, ] of this.tabState) {
          if (!tab.linkedBrowser) {
            this.tabState.delete(tab);
            this.unwarmTab(tab);
          }
        }

        if (this.lastVisibleTab && !this.lastVisibleTab.linkedBrowser) {
          this.lastVisibleTab = null;
        }
        if (this.lastPrimaryTab && !this.lastPrimaryTab.linkedBrowser) {
          this.lastPrimaryTab = null;
        }
        if (this.blankTab && !this.blankTab.linkedBrowser) {
          this.blankTab = null;
        }
        if (this.spinnerTab && !this.spinnerTab.linkedBrowser) {
          this.spinnerHidden();
          this.spinnerTab = null;
        }
        if (this.loadingTab && !this.loadingTab.linkedBrowser) {
          this.loadingTab = null;
          this.clearTimer(this.loadTimer);
          this.loadTimer = null;
        }
      },

      // This code runs after we've responded to an event or requested a new
      // tab. It's expected that we've already updated all the principal
      // state variables. This function takes care of updating any auxilliary
      // state.
      postActions() {
        // Once we finish loading loadingTab, we null it out. So the state should
        // always be LOADING.
        this.assert(!this.loadingTab ||
          this.getTabState(this.loadingTab) == this.STATE_LOADING);

        // We guarantee that loadingTab is non-null iff loadTimer is non-null. So
        // the timer is set only when we're loading something.
        this.assert(!this.loadTimer || this.loadingTab);
        this.assert(!this.loadingTab || this.loadTimer);

        // If we're switching to a non-remote tab, there's no need to wait
        // for it to send layers to the compositor, as this will happen
        // synchronously. Clearing this here means that in the next step,
        // we can load the non-remote browser immediately.
        if (!this.requestedTab.linkedBrowser.isRemoteBrowser) {
          this.loadingTab = null;
          if (this.loadTimer) {
            this.clearTimer(this.loadTimer);
            this.loadTimer = null;
          }
        }

        // If we're not loading anything, try loading the requested tab.
        let stateOfRequestedTab = this.getTabState(this.requestedTab);
        if (!this.loadTimer && !this.minimizedOrFullyOccluded &&
            (stateOfRequestedTab == this.STATE_UNLOADED ||
            stateOfRequestedTab == this.STATE_UNLOADING ||
            this.warmingTabs.has(this.requestedTab))) {
          this.assert(stateOfRequestedTab != this.STATE_LOADED);
          this.loadRequestedTab();
        }

        // See how many tabs still have work to do.
        let numPending = 0;
        let numWarming = 0;
        for (let [tab, state] of this.tabState) {
          // Skip print preview browsers since they shouldn't affect tab switching.
          if (this.tabbrowser._printPreviewBrowsers.has(tab.linkedBrowser)) {
            continue;
          }

          if (state == this.STATE_LOADED && tab !== this.requestedTab) {
            numPending++;

            if (tab !== this.visibleTab) {
              numWarming++;
            }
          }
          if (state == this.STATE_LOADING || state == this.STATE_UNLOADING) {
            numPending++;
          }
        }

        this.updateDisplay();

        // It's possible for updateDisplay to trigger one of our own event
        // handlers, which might cause finish() to already have been called.
        // Check for that before calling finish() again.
        if (!this.tabbrowser._switcher) {
          return;
        }

        this.maybeFinishTabSwitch();

        if (numWarming > this.tabbrowser.tabWarmingMax) {
          this.logState("Hit tabWarmingMax");
          if (this.unloadTimer) {
            this.clearTimer(this.unloadTimer);
          }
          this.unloadNonRequiredTabs();
        }

        if (numPending == 0) {
          this.finish();
        }

        this.logState("done");
      },

      // Fires when we're ready to unload unused tabs.
      onUnloadTimeout() {
        this.logState("onUnloadTimeout");
        this.preActions();
        this.unloadTimer = null;

        this.unloadNonRequiredTabs();

        this.postActions();
      },

      // If there are any non-visible and non-requested tabs in
      // STATE_LOADED, sets them to STATE_UNLOADING. Also queues
      // up the unloadTimer to run onUnloadTimeout if there are still
      // tabs in the process of unloading.
      unloadNonRequiredTabs() {
        this.warmingTabs = new WeakSet();
        let numPending = 0;

        // Unload any tabs that can be unloaded.
        for (let [tab, state] of this.tabState) {
          if (this.tabbrowser._printPreviewBrowsers.has(tab.linkedBrowser)) {
            continue;
          }

          if (state == this.STATE_LOADED &&
              !this.maybeVisibleTabs.has(tab) &&
              tab !== this.lastVisibleTab &&
              tab !== this.loadingTab &&
              tab !== this.requestedTab) {
            this.setTabState(tab, this.STATE_UNLOADING);
          }

          if (state != this.STATE_UNLOADED && tab !== this.requestedTab) {
            numPending++;
          }
        }

        if (numPending) {
          // Keep the timer going since there may be more tabs to unload.
          this.unloadTimer = this.setTimer(() => this.onUnloadTimeout(), this.UNLOAD_DELAY);
        }
      },

      // Fires when an ongoing load has taken too long.
      onLoadTimeout() {
        this.logState("onLoadTimeout");
        this.preActions();
        this.loadTimer = null;
        this.loadingTab = null;
        this.postActions();
      },

      // Fires when the layers become available for a tab.
      onLayersReady(browser) {
        let tab = this.tabbrowser.getTabForBrowser(browser);
        if (!tab) {
          // We probably got a layer update from a tab that got before
          // the switcher was created, or for browser that's not being
          // tracked by the async tab switcher (like the preloaded about:newtab).
          return;
        }

        this.logState(`onLayersReady(${tab._tPos}, ${browser.isRemoteBrowser})`);

        this.assert(this.getTabState(tab) == this.STATE_LOADING ||
          this.getTabState(tab) == this.STATE_LOADED);
        this.setTabState(tab, this.STATE_LOADED);
        this.unwarmTab(tab);

        if (this.loadingTab === tab) {
          this.clearTimer(this.loadTimer);
          this.loadTimer = null;
          this.loadingTab = null;
        }
      },

      // Fires when we paint the screen. Any tab switches we initiated
      // previously are done, so there's no need to keep the old layers
      // around.
      onPaint() {
        this.maybeVisibleTabs.clear();
      },

      // Called when we're done clearing the layers for a tab.
      onLayersCleared(browser) {
        let tab = this.tabbrowser.getTabForBrowser(browser);
        if (tab) {
          this.logState(`onLayersCleared(${tab._tPos})`);
          this.assert(this.getTabState(tab) == this.STATE_UNLOADING ||
            this.getTabState(tab) == this.STATE_UNLOADED);
          this.setTabState(tab, this.STATE_UNLOADED);
        }
      },

      // Called when a tab switches from remote to non-remote. In this case
      // a MozLayerTreeReady notification that we requested may never fire,
      // so we need to simulate it.
      onRemotenessChange(tab) {
        this.logState(`onRemotenessChange(${tab._tPos}, ${tab.linkedBrowser.isRemoteBrowser})`);
        if (!tab.linkedBrowser.isRemoteBrowser) {
          if (this.getTabState(tab) == this.STATE_LOADING) {
            this.onLayersReady(tab.linkedBrowser);
          } else if (this.getTabState(tab) == this.STATE_UNLOADING) {
            this.onLayersCleared(tab.linkedBrowser);
          }
        } else if (this.getTabState(tab) == this.STATE_LOADED) {
          // A tab just changed from non-remote to remote, which means
          // that it's gone back into the STATE_LOADING state until
          // it sends up a layer tree.
          this.setTabState(tab, this.STATE_LOADING);
        }
      },

      // Called when a tab has been removed, and the browser node is
      // about to be removed from the DOM.
      onTabRemoved(tab) {
        if (this.lastVisibleTab == tab) {
          // The browser that was being presented to the user is
          // going to be removed during this tick of the event loop.
          // This will cause us to show a tab spinner instead.
          this.preActions();
          this.lastVisibleTab = null;
          this.postActions();
        }
      },

      onSizeModeOrOcclusionStateChange() {
        if (this.minimizedOrFullyOccluded) {
          for (let [tab, state] of this.tabState) {
            // Skip print preview browsers since they shouldn't affect tab switching.
            if (this.tabbrowser._printPreviewBrowsers.has(tab.linkedBrowser)) {
              continue;
            }

            if (state == this.STATE_LOADING || state == this.STATE_LOADED) {
              this.setTabState(tab, this.STATE_UNLOADING);
            }
          }
          if (this.loadTimer) {
            this.clearTimer(this.loadTimer);
            this.loadTimer = null;
          }
          this.loadingTab = null;
        } else {
          // We're no longer minimized or occluded. This means we might want
          // to activate the current tab's docShell.
          this.maybeActivateDocShell(gBrowser.selectedTab);
        }
      },

      onSwapDocShells(ourBrowser, otherBrowser) {
        // This event fires before the swap. ourBrowser is from
        // our window. We save the state of otherBrowser since ourBrowser
        // needs to take on that state at the end of the swap.

        let otherTabbrowser = otherBrowser.ownerGlobal.gBrowser;
        let otherState;
        if (otherTabbrowser && otherTabbrowser._switcher) {
          let otherTab = otherTabbrowser.getTabForBrowser(otherBrowser);
          let otherSwitcher = otherTabbrowser._switcher;
          otherState = otherSwitcher.getTabState(otherTab);
        } else {
          otherState = otherBrowser.docShellIsActive ? this.STATE_LOADED : this.STATE_UNLOADED;
        }
        if (!this.swapMap) {
          this.swapMap = new WeakMap();
        }
        this.swapMap.set(otherBrowser, {
          state: otherState,
        });
      },

      onEndSwapDocShells(ourBrowser, otherBrowser) {
        // The swap has happened. We reset the loadingTab in
        // case it has been swapped. We also set ourBrowser's state
        // to whatever otherBrowser's state was before the swap.

        if (this.loadTimer) {
          // Clearing the load timer means that we will
          // immediately display a spinner if ourBrowser isn't
          // ready yet. Typically it will already be ready
          // though. If it's not, we're probably in a new window,
          // in which case we have no other tabs to display anyway.
          this.clearTimer(this.loadTimer);
          this.loadTimer = null;
        }
        this.loadingTab = null;

        let { state: otherState } = this.swapMap.get(otherBrowser);

        this.swapMap.delete(otherBrowser);

        let ourTab = this.tabbrowser.getTabForBrowser(ourBrowser);
        if (ourTab) {
          this.setTabStateNoAction(ourTab, otherState);
        }
      },

      shouldActivateDocShell(browser) {
        let tab = this.tabbrowser.getTabForBrowser(browser);
        let state = this.getTabState(tab);
        return state == this.STATE_LOADING || state == this.STATE_LOADED;
      },

      activateBrowserForPrintPreview(browser) {
        let tab = this.tabbrowser.getTabForBrowser(browser);
        let state = this.getTabState(tab);
        if (state != this.STATE_LOADING &&
            state != this.STATE_LOADED) {
          this.setTabState(tab, this.STATE_LOADING);
          this.logState("Activated browser " + this.tinfo(tab) + " for print preview");
        }
      },

      canWarmTab(tab) {
        if (!this.tabbrowser.tabWarmingEnabled) {
          return false;
        }

        if (!tab) {
          return false;
        }

        // If the tab is not yet inserted, closing, not remote,
        // crashed, already visible, or already requested, warming
        // up the tab makes no sense.
        if (this.minimizedOrFullyOccluded ||
          !tab.linkedPanel ||
          tab.closing ||
          !tab.linkedBrowser.isRemoteBrowser ||
          !tab.linkedBrowser.frameLoader.tabParent) {
          return false;
        }

        // Similarly, if the tab is already in STATE_LOADING or
        // STATE_LOADED somehow, there's no point in trying to
        // warm it up.
        let state = this.getTabState(tab);
        if (state === this.STATE_LOADING ||
          state === this.STATE_LOADED) {
          return false;
        }

        return true;
      },

      unwarmTab(tab) {
        this.warmingTabs.delete(tab);
      },

      warmupTab(tab) {
        if (!this.canWarmTab(tab)) {
          return;
        }

        this.logState("warmupTab " + this.tinfo(tab));

        this.warmingTabs.add(tab);
        this.setTabState(tab, this.STATE_LOADING);
        this.suppressDisplayPortAndQueueUnload(tab,
          this.tabbrowser.tabWarmingUnloadDelay);
      },

      // Called when the user asks to switch to a given tab.
      requestTab(tab) {
        if (tab === this.requestedTab) {
          return;
        }

        if (this.tabbrowser.tabWarmingEnabled) {
          let warmingState = "disqualified";

          if (this.warmingTabs.has(tab)) {
            let tabState = this.getTabState(tab);
            if (tabState == this.STATE_LOADING) {
              warmingState = "stillLoading";
            } else if (tabState == this.STATE_LOADED) {
              warmingState = "loaded";
            }
          } else if (this.canWarmTab(tab)) {
            warmingState = "notWarmed";
          }

          Services.telemetry
            .getHistogramById("FX_TAB_SWITCH_REQUEST_TAB_WARMING_STATE")
            .add(warmingState);
        }

        this._requestingTab = true;
        this.logState("requestTab " + this.tinfo(tab));
        this.startTabSwitch();

        this.requestedTab = tab;

        tab.linkedBrowser.setAttribute("primary", "true");
        if (this.lastPrimaryTab && this.lastPrimaryTab != tab) {
          this.lastPrimaryTab.linkedBrowser.removeAttribute("primary");
        }
        this.lastPrimaryTab = tab;

        this.suppressDisplayPortAndQueueUnload(this.requestedTab, this.UNLOAD_DELAY);
        this._requestingTab = false;
      },

      suppressDisplayPortAndQueueUnload(tab, unloadTimeout) {
        let browser = tab.linkedBrowser;
        let fl = browser.frameLoader;

        if (fl && fl.tabParent && !this.activeSuppressDisplayport.has(fl.tabParent)) {
          fl.tabParent.suppressDisplayport(true);
          this.activeSuppressDisplayport.add(fl.tabParent);
        }

        this.preActions();

        if (this.unloadTimer) {
          this.clearTimer(this.unloadTimer);
        }
        this.unloadTimer = this.setTimer(() => this.onUnloadTimeout(), unloadTimeout);

        this.postActions();
      },

      handleEvent(event, delayed = false) {
        if (this._processing) {
          this.setTimer(() => this.handleEvent(event, true), 0);
          return;
        }
        if (delayed && this.tabbrowser._switcher != this) {
          // if we delayed processing this event, we might be out of date, in which
          // case we drop the delayed events
          return;
        }
        this._processing = true;
        this.preActions();

        if (event.type == "MozLayerTreeReady") {
          this.onLayersReady(event.originalTarget);
        }
        if (event.type == "MozAfterPaint") {
          this.onPaint();
        } else if (event.type == "MozLayerTreeCleared") {
          this.onLayersCleared(event.originalTarget);
        } else if (event.type == "TabRemotenessChange") {
          this.onRemotenessChange(event.target);
        } else if (event.type == "sizemodechange" ||
          event.type == "occlusionstatechange") {
          this.onSizeModeOrOcclusionStateChange();
        } else if (event.type == "SwapDocShells") {
          this.onSwapDocShells(event.originalTarget, event.detail);
        } else if (event.type == "EndSwapDocShells") {
          this.onEndSwapDocShells(event.originalTarget, event.detail);
        }

        this.postActions();
        this._processing = false;
      },

      /*
       * Telemetry and Profiler related helpers for recording tab switch
       * timing.
       */

      startTabSwitch() {
        TelemetryStopwatch.cancel("FX_TAB_SWITCH_TOTAL_E10S_MS", window);
        TelemetryStopwatch.start("FX_TAB_SWITCH_TOTAL_E10S_MS", window);
        this.addMarker("AsyncTabSwitch:Start");
        this.switchInProgress = true;
      },

      /**
       * Something has occurred that might mean that we've completed
       * the tab switch (layers are ready, paints are done, spinners
       * are hidden). This checks to make sure all conditions are
       * satisfied, and then records the tab switch as finished.
       */
      maybeFinishTabSwitch() {
        if (this.switchInProgress && this.requestedTab &&
            (this.getTabState(this.requestedTab) == this.STATE_LOADED ||
              this.requestedTab === this.blankTab)) {
          // After this point the tab has switched from the content thread's point of view.
          // The changes will be visible after the next refresh driver tick + composite.
          let time = TelemetryStopwatch.timeElapsed("FX_TAB_SWITCH_TOTAL_E10S_MS", window);
          if (time != -1) {
            TelemetryStopwatch.finish("FX_TAB_SWITCH_TOTAL_E10S_MS", window);
            this.log("DEBUG: tab switch time = " + time);
            this.addMarker("AsyncTabSwitch:Finish");
          }
          this.switchInProgress = false;
        }
      },

      spinnerDisplayed() {
        this.assert(!this.spinnerTab);
        let browser = this.requestedTab.linkedBrowser;
        this.assert(browser.isRemoteBrowser);
        TelemetryStopwatch.start("FX_TAB_SWITCH_SPINNER_VISIBLE_MS", window);
        // We have a second, similar probe for capturing recordings of
        // when the spinner is displayed for very long periods.
        TelemetryStopwatch.start("FX_TAB_SWITCH_SPINNER_VISIBLE_LONG_MS", window);
        this.addMarker("AsyncTabSwitch:SpinnerShown");
      },

      spinnerHidden() {
        this.assert(this.spinnerTab);
        this.log("DEBUG: spinner time = " +
          TelemetryStopwatch.timeElapsed("FX_TAB_SWITCH_SPINNER_VISIBLE_MS", window));
        TelemetryStopwatch.finish("FX_TAB_SWITCH_SPINNER_VISIBLE_MS", window);
        TelemetryStopwatch.finish("FX_TAB_SWITCH_SPINNER_VISIBLE_LONG_MS", window);
        this.addMarker("AsyncTabSwitch:SpinnerHidden");
        // we do not get a onPaint after displaying the spinner
      },

      addMarker(marker) {
        if (Services.profiler) {
          Services.profiler.AddMarker(marker);
        }
      },

      /*
       * Debug related logging for switcher.
       */

      _useDumpForLogging: false,
      _logInit: false,

      logging() {
        if (this._useDumpForLogging)
          return true;
        if (this._logInit)
          return this._shouldLog;
        let result = Services.prefs.getBoolPref("browser.tabs.remote.logSwitchTiming", false);
        this._shouldLog = result;
        this._logInit = true;
        return this._shouldLog;
      },

      tinfo(tab) {
        if (tab) {
          return tab._tPos + "(" + tab.linkedBrowser.currentURI.spec + ")";
        }
        return "null";
      },

      log(s) {
        if (!this.logging())
          return;
        if (this._useDumpForLogging) {
          dump(s + "\n");
        } else {
          Services.console.logStringMessage(s);
        }
      },

      logState(prefix) {
        if (!this.logging())
          return;

        let accum = prefix + " ";
        for (let i = 0; i < this.tabbrowser.tabs.length; i++) {
          let tab = this.tabbrowser.tabs[i];
          let state = this.getTabState(tab);
          let isWarming = this.warmingTabs.has(tab);

          accum += i + ":";
          if (tab === this.lastVisibleTab) accum += "V";
          if (tab === this.loadingTab) accum += "L";
          if (tab === this.requestedTab) accum += "R";
          if (tab === this.blankTab) accum += "B";
          if (isWarming) accum += "(W)";
          if (state == this.STATE_LOADED) accum += "(+)";
          if (state == this.STATE_LOADING) accum += "(+?)";
          if (state == this.STATE_UNLOADED) accum += "(-)";
          if (state == this.STATE_UNLOADING) accum += "(-?)";
          accum += " ";
        }
        if (this._useDumpForLogging) {
          dump(accum + "\n");
        } else {
          Services.console.logStringMessage(accum);
        }
      },
    };
    this._switcher = switcher;
    switcher.init();
    return switcher;
  }

  warmupTab(aTab) {
    if (gMultiProcessBrowser) {
      this._getSwitcher().warmupTab(aTab);
    }
  }

  _handleKeyDownEvent(aEvent) {
    if (!aEvent.isTrusted) {
      // Don't let untrusted events mess with tabs.
      return;
    }

    if (aEvent.altKey)
      return;

    // Don't check if the event was already consumed because tab
    // navigation should always work for better user experience.

    if (aEvent.ctrlKey && aEvent.shiftKey && !aEvent.metaKey) {
      switch (aEvent.keyCode) {
        case aEvent.DOM_VK_PAGE_UP:
          this.moveTabBackward();
          aEvent.preventDefault();
          return;
        case aEvent.DOM_VK_PAGE_DOWN:
          this.moveTabForward();
          aEvent.preventDefault();
          return;
      }
    }

    if (AppConstants.platform != "macosx") {
      if (aEvent.ctrlKey && !aEvent.shiftKey && !aEvent.metaKey &&
          aEvent.keyCode == KeyEvent.DOM_VK_F4 &&
          !this.mCurrentTab.pinned) {
        this.removeCurrentTab({ animate: true });
        aEvent.preventDefault();
      }
    }
  }

  _handleKeyPressEventMac(aEvent) {
    if (!aEvent.isTrusted) {
      // Don't let untrusted events mess with tabs.
      return;
    }

    if (aEvent.altKey)
      return;

    if (AppConstants.platform == "macosx") {
      if (!aEvent.metaKey)
        return;

      var offset = 1;
      switch (aEvent.charCode) {
        case "}".charCodeAt(0):
          offset = -1;
        case "{".charCodeAt(0):
          if (window.getComputedStyle(this.container).direction == "ltr")
            offset *= -1;
          this.tabContainer.advanceSelectedTab(offset, true);
          aEvent.preventDefault();
      }
    }
  }

  createTooltip(event) {
    event.stopPropagation();
    var tab = document.tooltipNode;
    if (tab.localName != "tab") {
      event.preventDefault();
      return;
    }

    let stringWithShortcut = (stringId, keyElemId) => {
      let keyElem = document.getElementById(keyElemId);
      let shortcut = ShortcutUtils.prettifyShortcut(keyElem);
      return gTabBrowserBundle.formatStringFromName(stringId, [shortcut], 1);
    };

    var label;
    if (tab.mOverCloseButton) {
      label = tab.selected ?
        stringWithShortcut("tabs.closeSelectedTab.tooltip", "key_close") :
        gTabBrowserBundle.GetStringFromName("tabs.closeTab.tooltip");
    } else if (tab._overPlayingIcon) {
      let stringID;
      if (tab.selected) {
        stringID = tab.linkedBrowser.audioMuted ?
          "tabs.unmuteAudio.tooltip" :
          "tabs.muteAudio.tooltip";
        label = stringWithShortcut(stringID, "key_toggleMute");
      } else {
        if (tab.hasAttribute("activemedia-blocked")) {
          stringID = "tabs.unblockAudio.tooltip";
        } else {
          stringID = tab.linkedBrowser.audioMuted ?
            "tabs.unmuteAudio.background.tooltip" :
            "tabs.muteAudio.background.tooltip";
        }

        label = gTabBrowserBundle.GetStringFromName(stringID);
      }
    } else {
      label = tab._fullLabel || tab.getAttribute("label");
      if (AppConstants.NIGHTLY_BUILD &&
          tab.linkedBrowser &&
          tab.linkedBrowser.isRemoteBrowser &&
          tab.linkedBrowser.frameLoader) {
        label += " (pid " + tab.linkedBrowser.frameLoader.tabParent.osPid + ")";
      }
      if (tab.userContextId) {
        label = gTabBrowserBundle.formatStringFromName("tabs.containers.tooltip", [label, ContextualIdentityService.getUserContextLabel(tab.userContextId)], 2);
      }
    }

    event.target.setAttribute("label", label);
  }

  handleEvent(aEvent) {
    switch (aEvent.type) {
      case "keydown":
        this._handleKeyDownEvent(aEvent);
        break;
      case "keypress":
        this._handleKeyPressEventMac(aEvent);
        break;
      case "sizemodechange":
      case "occlusionstatechange":
        if (aEvent.target == window && !this._switcher) {
          this.mCurrentBrowser.preserveLayers(
            window.windowState == window.STATE_MINIMIZED || window.isFullyOccluded);
          this.mCurrentBrowser.docShellIsActive = this.shouldActivateDocShell(this.mCurrentBrowser);
        }
        break;
    }
  }

  receiveMessage(aMessage) {
    let data = aMessage.data;
    let browser = aMessage.target;

    switch (aMessage.name) {
      case "DOMTitleChanged":
      {
        let tab = this.getTabForBrowser(browser);
        if (!tab || tab.hasAttribute("pending"))
          return undefined;
        let titleChanged = this.setTabTitle(tab);
        if (titleChanged && !tab.selected && !tab.hasAttribute("busy"))
          tab.setAttribute("titlechanged", "true");
        break;
      }
      case "DOMWindowClose":
      {
        if (this.tabs.length == 1) {
          // We already did PermitUnload in the content process
          // for this tab (the only one in the window). So we don't
          // need to do it again for any tabs.
          window.skipNextCanClose = true;
          window.close();
          return undefined;
        }

        let tab = this.getTabForBrowser(browser);
        if (tab) {
          // Skip running PermitUnload since it already happened in
          // the content process.
          this.removeTab(tab, { skipPermitUnload: true });
        }
        break;
      }
      case "contextmenu":
      {
        openContextMenu(aMessage);
        break;
      }
      case "DOMWindowFocus":
      {
        let tab = this.getTabForBrowser(browser);
        if (!tab)
          return undefined;
        this.selectedTab = tab;
        window.focus();
        break;
      }
      case "Browser:Init":
      {
        let tab = this.getTabForBrowser(browser);
        if (!tab)
          return undefined;

        this._outerWindowIDBrowserMap.set(browser.outerWindowID, browser);
        browser.messageManager.sendAsyncMessage("Browser:AppTab", { isAppTab: tab.pinned });
        break;
      }
      case "Browser:WindowCreated":
      {
        let tab = this.getTabForBrowser(browser);
        if (tab && data.userContextId) {
          ContextualIdentityService.telemetry(data.userContextId);
          tab.setUserContextId(data.userContextId);
        }

        // We don't want to update the container icon and identifier if
        // this is not the selected browser.
        if (browser == gBrowser.selectedBrowser) {
          updateUserContextUIIndicator();
        }

        break;
      }
      case "Findbar:Keypress":
      {
        let tab = this.getTabForBrowser(browser);
        // If the find bar for this tab is not yet alive, only initialize
        // it if there's a possibility FindAsYouType will be used.
        // There's no point in doing it for most random keypresses.
        if (!this.isFindBarInitialized(tab) &&
            data.shouldFastFind) {
          let shouldFastFind = this._findAsYouType;
          if (!shouldFastFind) {
            // Please keep in sync with toolkit/content/widgets/findbar.xml
            const FAYT_LINKS_KEY = "'";
            const FAYT_TEXT_KEY = "/";
            let charCode = data.fakeEvent.charCode;
            let key = charCode ? String.fromCharCode(charCode) : null;
            shouldFastFind = key == FAYT_LINKS_KEY || key == FAYT_TEXT_KEY;
          }
          if (shouldFastFind) {
            // Make sure we return the result.
            return this.getFindBar(tab).receiveMessage(aMessage);
          }
        }
        break;
      }
      case "RefreshBlocker:Blocked":
      {
        // The data object is expected to contain the following properties:
        //  - URI (string)
        //     The URI that a page is attempting to refresh or redirect to.
        //  - delay (int)
        //     The delay (in milliseconds) before the page was going to
        //     reload or redirect.
        //  - sameURI (bool)
        //     true if we're refreshing the page. false if we're redirecting.
        //  - outerWindowID (int)
        //     The outerWindowID of the frame that requested the refresh or
        //     redirect.

        let brandBundle = document.getElementById("bundle_brand");
        let brandShortName = brandBundle.getString("brandShortName");
        let message =
          gNavigatorBundle.getFormattedString("refreshBlocked." +
                                              (data.sameURI ? "refreshLabel"
                                                            : "redirectLabel"),
                                              [brandShortName]);

        let notificationBox = this.getNotificationBox(browser);
        let notification = notificationBox.getNotificationWithValue("refresh-blocked");

        if (notification) {
          notification.label = message;
        } else {
          let refreshButtonText =
            gNavigatorBundle.getString("refreshBlocked.goButton");
          let refreshButtonAccesskey =
            gNavigatorBundle.getString("refreshBlocked.goButton.accesskey");

          let buttons = [{
            label: refreshButtonText,
            accessKey: refreshButtonAccesskey,
            callback() {
              if (browser.messageManager) {
                browser.messageManager.sendAsyncMessage("RefreshBlocker:Refresh", data);
              }
            }
          }];

          notificationBox.appendNotification(message, "refresh-blocked",
            "chrome://browser/skin/notification-icons/popup.svg",
            notificationBox.PRIORITY_INFO_MEDIUM,
            buttons);
        }
        break;
      }
    }
    return undefined;
  }

  observe(aSubject, aTopic, aData) {
    switch (aTopic) {
      case "contextual-identity-updated":
      {
        for (let tab of this.tabs) {
          if (tab.getAttribute("usercontextid") == aData) {
            ContextualIdentityService.setTabStyle(tab);
          }
        }
        break;
      }
      case "nsPref:changed":
      {
        // This is the only pref observed.
        this._findAsYouType = Services.prefs.getBoolPref("accessibility.typeaheadfind");
        break;
      }
    }
  }

  _updateNewTabVisibility() {
    // Helper functions to help deal with customize mode wrapping some items
    let wrap = n => n.parentNode.localName == "toolbarpaletteitem" ? n.parentNode : n;
    let unwrap = n => n && n.localName == "toolbarpaletteitem" ? n.firstElementChild : n;

    let sib = this.tabContainer;
    do {
      sib = unwrap(wrap(sib).nextElementSibling);
    } while (sib && sib.hidden);

    const kAttr = "hasadjacentnewtabbutton";
    if (sib && sib.id == "new-tab-button") {
      this.tabContainer.setAttribute(kAttr, "true");
    } else {
      this.tabContainer.removeAttribute(kAttr);
    }
  }

  onWidgetAfterDOMChange(aNode, aNextNode, aContainer) {
    if (aContainer.ownerDocument == document &&
        aContainer.id == "TabsToolbar") {
      this._updateNewTabVisibility();
    }
  }

  onAreaNodeRegistered(aArea, aContainer) {
    if (aContainer.ownerDocument == document &&
        aArea == "TabsToolbar") {
      this._updateNewTabVisibility();
    }
  }

  onAreaReset(aArea, aContainer) {
    this.onAreaNodeRegistered(aArea, aContainer);
  }

  _generateUniquePanelID() {
    if (!this._uniquePanelIDCounter) {
      this._uniquePanelIDCounter = 0;
    }

    let outerID = window.QueryInterface(Ci.nsIInterfaceRequestor)
                        .getInterface(Ci.nsIDOMWindowUtils)
                        .outerWindowID;

    // We want panel IDs to be globally unique, that's why we include the
    // window ID. We switched to a monotonic counter as Date.now() lead
    // to random failures because of colliding IDs.
    return "panel-" + outerID + "-" + (++this._uniquePanelIDCounter);
  }

  disconnectedCallback() {
    Services.obs.removeObserver(this, "contextual-identity-updated");

    CustomizableUI.removeListener(this);

    for (let tab of this.tabs) {
      let browser = tab.linkedBrowser;
      if (browser.registeredOpenURI) {
        this._unifiedComplete.unregisterOpenPage(browser.registeredOpenURI,
          browser.getAttribute("usercontextid") || 0);
        delete browser.registeredOpenURI;
      }

      let filter = this._tabFilters.get(tab);
      if (filter) {
        browser.webProgress.removeProgressListener(filter);

        let listener = this._tabListeners.get(tab);
        if (listener) {
          filter.removeProgressListener(listener);
          listener.destroy();
        }

        this._tabFilters.delete(tab);
        this._tabListeners.delete(tab);
      }
    }
    const nsIEventListenerService =
      Ci.nsIEventListenerService;
    let els = Cc["@mozilla.org/eventlistenerservice;1"]
      .getService(nsIEventListenerService);
    els.removeSystemEventListener(document, "keydown", this, false);
    if (AppConstants.platform == "macosx") {
      els.removeSystemEventListener(document, "keypress", this, false);
    }
    window.removeEventListener("sizemodechange", this);
    window.removeEventListener("occlusionstatechange", this);

    if (gMultiProcessBrowser) {
      let messageManager = window.getGroupMessageManager("browsers");
      messageManager.removeMessageListener("DOMTitleChanged", this);
      window.messageManager.removeMessageListener("contextmenu", this);

      if (this._switcher) {
        this._switcher.destroy();
      }
    }

    Services.prefs.removeObserver("accessibility.typeaheadfind", this);
  }

  _setupEventListeners() {
    this.addEventListener("DOMWindowClose", (event) => {
      if (!event.isTrusted)
        return;

      if (this.tabs.length == 1) {
        // We already did PermitUnload in nsGlobalWindow::Close
        // for this tab. There are no other tabs we need to do
        // PermitUnload for.
        window.skipNextCanClose = true;
        return;
      }

      var tab = this._getTabForContentWindow(event.target);
      if (tab) {
        // Skip running PermitUnload since it already happened.
        this.removeTab(tab, { skipPermitUnload: true });
        event.preventDefault();
      }
    }, true);

    this.addEventListener("DOMWillOpenModalDialog", (event) => {
      if (!event.isTrusted)
        return;

      let targetIsWindow = event.target instanceof Window;

      // We're about to open a modal dialog, so figure out for which tab:
      // If this is a same-process modal dialog, then we're given its DOM
      // window as the event's target. For remote dialogs, we're given the
      // browser, but that's in the originalTarget and not the target,
      // because it's across the tabbrowser's XBL boundary.
      let tabForEvent = targetIsWindow ?
        this._getTabForContentWindow(event.target.top) :
        this.getTabForBrowser(event.originalTarget);

      // Focus window for beforeunload dialog so it is seen but don't
      // steal focus from other applications.
      if (event.detail &&
          event.detail.tabPrompt &&
          event.detail.inPermitUnload &&
          Services.focus.activeWindow)
        window.focus();

      // Don't need to act if the tab is already selected or if there isn't
      // a tab for the event (e.g. for the webextensions options_ui remote
      // browsers embedded in the "about:addons" page):
      if (!tabForEvent || tabForEvent.selected)
        return;

      // We always switch tabs for beforeunload tab-modal prompts.
      if (event.detail &&
          event.detail.tabPrompt &&
          !event.detail.inPermitUnload) {
        let docPrincipal = targetIsWindow ? event.target.document.nodePrincipal : null;
        // At least one of these should/will be non-null:
        let promptPrincipal = event.detail.promptPrincipal || docPrincipal ||
          tabForEvent.linkedBrowser.contentPrincipal;
        // For null principals, we bail immediately and don't show the checkbox:
        if (!promptPrincipal || promptPrincipal.isNullPrincipal) {
          tabForEvent.setAttribute("attention", "true");
          return;
        }

        // For non-system/expanded principals, we bail and show the checkbox
        if (promptPrincipal.URI &&
            !Services.scriptSecurityManager.isSystemPrincipal(promptPrincipal)) {
          let permission = Services.perms.testPermissionFromPrincipal(promptPrincipal,
            "focus-tab-by-prompt");
          if (permission != Services.perms.ALLOW_ACTION) {
            // Tell the prompt box we want to show the user a checkbox:
            let tabPrompt = this.getTabModalPromptBox(tabForEvent.linkedBrowser);
            tabPrompt.onNextPromptShowAllowFocusCheckboxFor(promptPrincipal);
            tabForEvent.setAttribute("attention", "true");
            return;
          }
        }
        // ... so system and expanded principals, as well as permitted "normal"
        // URI-based principals, always get to steal focus for the tab when prompting.
      }

      // If permissions/origins dictate so, bring tab to the front.
      this.selectedTab = tabForEvent;
    }, true);

    this.addEventListener("DOMTitleChanged", (event) => {
      if (!event.isTrusted)
        return;

      var contentWin = event.target.defaultView;
      if (contentWin != contentWin.top)
        return;

      var tab = this._getTabForContentWindow(contentWin);
      if (!tab || tab.hasAttribute("pending"))
        return;

      var titleChanged = this.setTabTitle(tab);
      if (titleChanged && !tab.selected && !tab.hasAttribute("busy"))
        tab.setAttribute("titlechanged", "true");
    });

    this.addEventListener("oop-browser-crashed", (event) => {
      if (!event.isTrusted)
        return;

      let browser = event.originalTarget;

      // Preloaded browsers do not actually have any tabs. If one crashes,
      // it should be released and removed.
      if (browser === this._preloadedBrowser) {
        this.removePreloadedBrowser();
        return;
      }

      let icon = browser.mIconURL;
      let tab = this.getTabForBrowser(browser);

      if (this.selectedBrowser == browser) {
        TabCrashHandler.onSelectedBrowserCrash(browser);
      } else {
        this.updateBrowserRemoteness(browser, false);
        SessionStore.reviveCrashedTab(tab);
      }

      tab.removeAttribute("soundplaying");
      this.setIcon(tab, icon, browser.contentPrincipal, browser.contentRequestContextID);
    });

    this.addEventListener("DOMAudioPlaybackStarted", (event) => {
      var tab = this.getTabFromAudioEvent(event);
      if (!tab) {
        return;
      }

      clearTimeout(tab._soundPlayingAttrRemovalTimer);
      tab._soundPlayingAttrRemovalTimer = 0;

      let modifiedAttrs = [];
      if (tab.hasAttribute("soundplaying-scheduledremoval")) {
        tab.removeAttribute("soundplaying-scheduledremoval");
        modifiedAttrs.push("soundplaying-scheduledremoval");
      }

      if (!tab.hasAttribute("soundplaying")) {
        tab.setAttribute("soundplaying", true);
        modifiedAttrs.push("soundplaying");
      }

      if (modifiedAttrs.length) {
        // Flush style so that the opacity takes effect immediately, in
        // case the media is stopped before the style flushes naturally.
        getComputedStyle(tab).opacity;
      }

      this._tabAttrModified(tab, modifiedAttrs);
    });

    this.addEventListener("DOMAudioPlaybackStopped", (event) => {
      var tab = this.getTabFromAudioEvent(event);
      if (!tab) {
        return;
      }

      if (tab.hasAttribute("soundplaying")) {
        let removalDelay = Services.prefs.getIntPref("browser.tabs.delayHidingAudioPlayingIconMS");

        tab.style.setProperty("--soundplaying-removal-delay", `${removalDelay - 300}ms`);
        tab.setAttribute("soundplaying-scheduledremoval", "true");
        this._tabAttrModified(tab, ["soundplaying-scheduledremoval"]);

        tab._soundPlayingAttrRemovalTimer = setTimeout(() => {
          tab.removeAttribute("soundplaying-scheduledremoval");
          tab.removeAttribute("soundplaying");
          this._tabAttrModified(tab, ["soundplaying", "soundplaying-scheduledremoval"]);
        }, removalDelay);
      }
    });

    this.addEventListener("DOMAudioPlaybackBlockStarted", (event) => {
      var tab = this.getTabFromAudioEvent(event);
      if (!tab) {
        return;
      }

      if (!tab.hasAttribute("activemedia-blocked")) {
        tab.setAttribute("activemedia-blocked", true);
        this._tabAttrModified(tab, ["activemedia-blocked"]);
        tab.startMediaBlockTimer();
      }
    });

    this.addEventListener("DOMAudioPlaybackBlockStopped", (event) => {
      var tab = this.getTabFromAudioEvent(event);
      if (!tab) {
        return;
      }

      if (tab.hasAttribute("activemedia-blocked")) {
        tab.removeAttribute("activemedia-blocked");
        this._tabAttrModified(tab, ["activemedia-blocked"]);
        let hist = Services.telemetry.getHistogramById("TAB_AUDIO_INDICATOR_USED");
        hist.add(2 /* unblockByVisitingTab */ );
        tab.finishMediaBlockTimer();
      }
    });
  }
}

/**
 * A web progress listener object definition for a given tab.
 */
class TabProgressListener {
  constructor(aTab, aBrowser, aStartsBlank, aWasPreloadedBrowser, aOrigStateFlags) {
    let stateFlags = aOrigStateFlags || 0;
    // Initialize mStateFlags to non-zero e.g. when creating a progress
    // listener for preloaded browsers as there was no progress listener
    // around when the content started loading. If the content didn't
    // quite finish loading yet, mStateFlags will very soon be overridden
    // with the correct value and end up at STATE_STOP again.
    if (aWasPreloadedBrowser) {
      stateFlags = Ci.nsIWebProgressListener.STATE_STOP |
        Ci.nsIWebProgressListener.STATE_IS_REQUEST;
    }

    this.mTab = aTab;
    this.mBrowser = aBrowser;
    this.mBlank = aStartsBlank;

    // cache flags for correct status UI update after tab switching
    this.mStateFlags = stateFlags;
    this.mStatus = 0;
    this.mMessage = "";
    this.mTotalProgress = 0;

    // count of open requests (should always be 0 or 1)
    this.mRequestCount = 0;
  }

  destroy() {
    delete this.mTab;
    delete this.mBrowser;
  }

  _callProgressListeners() {
    Array.unshift(arguments, this.mBrowser);
    return gBrowser._callProgressListeners.apply(gBrowser, arguments);
  }

  _shouldShowProgress(aRequest) {
    if (this.mBlank)
      return false;

    // Don't show progress indicators in tabs for about: URIs
    // pointing to local resources.
    if ((aRequest instanceof Ci.nsIChannel) &&
        gBrowser._isLocalAboutURI(aRequest.originalURI, aRequest.URI)) {
      return false;
    }

    return true;
  }

  _isForInitialAboutBlank(aWebProgress, aStateFlags, aLocation) {
    if (!this.mBlank || !aWebProgress.isTopLevel) {
      return false;
    }

    // If the state has STATE_STOP, and no requests were in flight, then this
    // must be the initial "stop" for the initial about:blank document.
    const nsIWebProgressListener = Ci.nsIWebProgressListener;
    if (aStateFlags & nsIWebProgressListener.STATE_STOP &&
        this.mRequestCount == 0 &&
        !aLocation) {
      return true;
    }

    let location = aLocation ? aLocation.spec : "";
    return location == "about:blank";
  }

  onProgressChange(aWebProgress, aRequest, aCurSelfProgress, aMaxSelfProgress,
                   aCurTotalProgress, aMaxTotalProgress) {
    this.mTotalProgress = aMaxTotalProgress ? aCurTotalProgress / aMaxTotalProgress : 0;

    if (!this._shouldShowProgress(aRequest))
      return;

    if (this.mTotalProgress && this.mTab.hasAttribute("busy"))
      this.mTab.setAttribute("progress", "true");

    this._callProgressListeners("onProgressChange",
                                [aWebProgress, aRequest,
                                 aCurSelfProgress, aMaxSelfProgress,
                                 aCurTotalProgress, aMaxTotalProgress]);
  }

  onProgressChange64(aWebProgress, aRequest, aCurSelfProgress,
                     aMaxSelfProgress, aCurTotalProgress, aMaxTotalProgress) {
    return this.onProgressChange(aWebProgress, aRequest,
      aCurSelfProgress, aMaxSelfProgress, aCurTotalProgress,
      aMaxTotalProgress);
  }

  /* eslint-disable complexity */
  onStateChange(aWebProgress, aRequest, aStateFlags, aStatus) {
    if (!aRequest)
      return;

    const nsIWebProgressListener = Ci.nsIWebProgressListener;
    const nsIChannel = Ci.nsIChannel;
    let location, originalLocation;
    try {
      aRequest.QueryInterface(nsIChannel);
      location = aRequest.URI;
      originalLocation = aRequest.originalURI;
    } catch (ex) {}

    let ignoreBlank = this._isForInitialAboutBlank(aWebProgress, aStateFlags,
      location);

    // If we were ignoring some messages about the initial about:blank, and we
    // got the STATE_STOP for it, we'll want to pay attention to those messages
    // from here forward. Similarly, if we conclude that this state change
    // is one that we shouldn't be ignoring, then stop ignoring.
    if ((ignoreBlank &&
        aStateFlags & nsIWebProgressListener.STATE_STOP &&
        aStateFlags & nsIWebProgressListener.STATE_IS_NETWORK) ||
        !ignoreBlank && this.mBlank) {
      this.mBlank = false;
    }

    if (aStateFlags & nsIWebProgressListener.STATE_START) {
      this.mRequestCount++;
    } else if (aStateFlags & nsIWebProgressListener.STATE_STOP) {
      const NS_ERROR_UNKNOWN_HOST = 2152398878;
      if (--this.mRequestCount > 0 && aStatus == NS_ERROR_UNKNOWN_HOST) {
        // to prevent bug 235825: wait for the request handled
        // by the automatic keyword resolver
        return;
      }
      // since we (try to) only handle STATE_STOP of the last request,
      // the count of open requests should now be 0
      this.mRequestCount = 0;
    }

    if (aStateFlags & nsIWebProgressListener.STATE_START &&
        aStateFlags & nsIWebProgressListener.STATE_IS_NETWORK) {
      if (aWebProgress.isTopLevel) {
        // Need to use originalLocation rather than location because things
        // like about:home and about:privatebrowsing arrive with nsIRequest
        // pointing to their resolved jar: or file: URIs.
        if (!(originalLocation && gInitialPages.includes(originalLocation.spec) &&
            originalLocation != "about:blank" &&
            this.mBrowser.initialPageLoadedFromURLBar != originalLocation.spec &&
            this.mBrowser.currentURI && this.mBrowser.currentURI.spec == "about:blank")) {
          // Indicating that we started a load will allow the location
          // bar to be cleared when the load finishes.
          // In order to not overwrite user-typed content, we avoid it
          // (see if condition above) in a very specific case:
          // If the load is of an 'initial' page (e.g. about:privatebrowsing,
          // about:newtab, etc.), was not explicitly typed in the location
          // bar by the user, is not about:blank (because about:blank can be
          // loaded by websites under their principal), and the current
          // page in the browser is about:blank (indicating it is a newly
          // created or re-created browser, e.g. because it just switched
          // remoteness or is a new tab/window).
          this.mBrowser.urlbarChangeTracker.startedLoad();
        }
        delete this.mBrowser.initialPageLoadedFromURLBar;
        // If the browser is loading it must not be crashed anymore
        this.mTab.removeAttribute("crashed");
      }

      if (this._shouldShowProgress(aRequest)) {
        if (!(aStateFlags & nsIWebProgressListener.STATE_RESTORING) &&
            aWebProgress && aWebProgress.isTopLevel) {
          this.mTab.setAttribute("busy", "true");
          this.mTab._notselectedsinceload = !this.mTab.selected;
          SchedulePressure.startMonitoring(window, {
            highPressureFn() {
              // Only switch back to the SVG loading indicator after getting
              // three consecutive low pressure callbacks. Used to prevent
              // switching quickly between the SVG and APNG loading indicators.
              gBrowser.tabContainer._schedulePressureCount = gBrowser.schedulePressureDefaultCount;
              gBrowser.tabContainer.setAttribute("schedulepressure", "true");
            },
            lowPressureFn() {
              if (!gBrowser.tabContainer._schedulePressureCount ||
                --gBrowser.tabContainer._schedulePressureCount <= 0) {
                gBrowser.tabContainer.removeAttribute("schedulepressure");
              }

              // If tabs are closed while they are loading we need to
              // stop monitoring schedule pressure. We don't stop monitoring
              // during high pressure times because we want to eventually
              // return to the SVG tab loading animations.
              let continueMonitoring = true;
              if (!document.querySelector(".tabbrowser-tab[busy]")) {
                SchedulePressure.stopMonitoring(window);
                continueMonitoring = false;
              }
              return { continueMonitoring };
            },
          });
          gBrowser.syncThrobberAnimations(this.mTab);
        }

        if (this.mTab.selected) {
          gBrowser.mIsBusy = true;
        }
      }
    } else if (aStateFlags & nsIWebProgressListener.STATE_STOP &&
               aStateFlags & nsIWebProgressListener.STATE_IS_NETWORK) {

      if (this.mTab.hasAttribute("busy")) {
        this.mTab.removeAttribute("busy");
        if (!document.querySelector(".tabbrowser-tab[busy]")) {
          SchedulePressure.stopMonitoring(window);
          gBrowser.tabContainer.removeAttribute("schedulepressure");
        }

        // Only animate the "burst" indicating the page has loaded if
        // the top-level page is the one that finished loading.
        if (aWebProgress.isTopLevel && !aWebProgress.isLoadingDocument &&
            Components.isSuccessCode(aStatus) &&
            !gBrowser.tabAnimationsInProgress &&
            Services.prefs.getBoolPref("toolkit.cosmeticAnimations.enabled")) {
          if (this.mTab._notselectedsinceload) {
            this.mTab.setAttribute("notselectedsinceload", "true");
          } else {
            this.mTab.removeAttribute("notselectedsinceload");
          }

          this.mTab.setAttribute("bursting", "true");
        }

        gBrowser._tabAttrModified(this.mTab, ["busy"]);
        if (!this.mTab.selected)
          this.mTab.setAttribute("unread", "true");
      }
      this.mTab.removeAttribute("progress");

      if (aWebProgress.isTopLevel) {
        let isSuccessful = Components.isSuccessCode(aStatus);
        if (!isSuccessful && !isTabEmpty(this.mTab)) {
          // Restore the current document's location in case the
          // request was stopped (possibly from a content script)
          // before the location changed.

          this.mBrowser.userTypedValue = null;

          let inLoadURI = this.mBrowser.inLoadURI;
          if (this.mTab.selected && gURLBar && !inLoadURI) {
            URLBarSetURI();
          }
        } else if (isSuccessful) {
          this.mBrowser.urlbarChangeTracker.finishedLoad();
        }

        // Ignore initial about:blank to prevent flickering.
        if (!this.mBrowser.mIconURL && !ignoreBlank) {
          // Don't switch to the default icon on about:home or about:newtab,
          // since these pages get their favicon set in browser code to
          // improve perceived performance.
          let isNewTab = originalLocation &&
            (originalLocation.spec == "about:newtab" ||
              originalLocation.spec == "about:privatebrowsing" ||
              originalLocation.spec == "about:home");
          if (!isNewTab) {
            gBrowser.useDefaultIcon(this.mTab);
          }
        }
      }

      // For keyword URIs clear the user typed value since they will be changed into real URIs
      if (location.scheme == "keyword")
        this.mBrowser.userTypedValue = null;

      if (this.mTab.selected)
        gBrowser.mIsBusy = false;
    }

    if (ignoreBlank) {
      this._callProgressListeners("onUpdateCurrentBrowser",
                                  [aStateFlags, aStatus, "", 0],
                                  true, false);
    } else {
      this._callProgressListeners("onStateChange",
                                  [aWebProgress, aRequest, aStateFlags, aStatus],
                                  true, false);
    }

    this._callProgressListeners("onStateChange",
                                [aWebProgress, aRequest, aStateFlags, aStatus],
                                false);

    if (aStateFlags & (nsIWebProgressListener.STATE_START |
        nsIWebProgressListener.STATE_STOP)) {
      // reset cached temporary values at beginning and end
      this.mMessage = "";
      this.mTotalProgress = 0;
    }
    this.mStateFlags = aStateFlags;
    this.mStatus = aStatus;
  }
  /* eslint-enable complexity */

  onLocationChange(aWebProgress, aRequest, aLocation, aFlags) {
    // OnLocationChange is called for both the top-level content
    // and the subframes.
    let topLevel = aWebProgress.isTopLevel;

    if (topLevel) {
      let isSameDocument = !!(aFlags & Ci.nsIWebProgressListener.LOCATION_CHANGE_SAME_DOCUMENT);
      // We need to clear the typed value
      // if the document failed to load, to make sure the urlbar reflects the
      // failed URI (particularly for SSL errors). However, don't clear the value
      // if the error page's URI is about:blank, because that causes complete
      // loss of urlbar contents for invalid URI errors (see bug 867957).
      // Another reason to clear the userTypedValue is if this was an anchor
      // navigation initiated by the user.
      if (this.mBrowser.didStartLoadSinceLastUserTyping() ||
          ((aFlags & Ci.nsIWebProgressListener.LOCATION_CHANGE_ERROR_PAGE) &&
            aLocation.spec != "about:blank") ||
          (isSameDocument && this.mBrowser.inLoadURI)) {
        this.mBrowser.userTypedValue = null;
      }

      // If the tab has been set to "busy" outside the stateChange
      // handler below (e.g. by sessionStore.navigateAndRestore), and
      // the load results in an error page, it's possible that there
      // isn't any (STATE_IS_NETWORK & STATE_STOP) state to cause busy
      // attribute being removed. In this case we should remove the
      // attribute here.
      if ((aFlags & Ci.nsIWebProgressListener.LOCATION_CHANGE_ERROR_PAGE) &&
          this.mTab.hasAttribute("busy")) {
        this.mTab.removeAttribute("busy");
        gBrowser._tabAttrModified(this.mTab, ["busy"]);
      }

      // If the browser was playing audio, we should remove the playing state.
      if (this.mTab.hasAttribute("soundplaying") && !isSameDocument) {
        clearTimeout(this.mTab._soundPlayingAttrRemovalTimer);
        this.mTab._soundPlayingAttrRemovalTimer = 0;
        this.mTab.removeAttribute("soundplaying");
        gBrowser._tabAttrModified(this.mTab, ["soundplaying"]);
      }

      // If the browser was previously muted, we should restore the muted state.
      if (this.mTab.hasAttribute("muted")) {
        this.mTab.linkedBrowser.mute();
      }

      if (gBrowser.isFindBarInitialized(this.mTab)) {
        let findBar = gBrowser.getFindBar(this.mTab);

        // Close the Find toolbar if we're in old-style TAF mode
        if (findBar.findMode != findBar.FIND_NORMAL) {
          findBar.close();
        }
      }

      gBrowser.setTabTitle(this.mTab);

      // Don't clear the favicon if this tab is in the pending
      // state, as SessionStore will have set the icon for us even
      // though we're pointed at an about:blank. Also don't clear it
      // if onLocationChange was triggered by a pushState or a
      // replaceState (bug 550565) or a hash change (bug 408415).
      if (!this.mTab.hasAttribute("pending") &&
          aWebProgress.isLoadingDocument &&
          !isSameDocument) {
        this.mBrowser.mIconURL = null;
      }

      let userContextId = this.mBrowser.getAttribute("usercontextid") || 0;
      if (this.mBrowser.registeredOpenURI) {
        gBrowser._unifiedComplete
          .unregisterOpenPage(this.mBrowser.registeredOpenURI, userContextId);
        delete this.mBrowser.registeredOpenURI;
      }
      // Tabs in private windows aren't registered as "Open" so
      // that they don't appear as switch-to-tab candidates.
      if (!isBlankPageURL(aLocation.spec) &&
          (!PrivateBrowsingUtils.isWindowPrivate(window) ||
            PrivateBrowsingUtils.permanentPrivateBrowsing)) {
        gBrowser._unifiedComplete.registerOpenPage(aLocation, userContextId);
        this.mBrowser.registeredOpenURI = aLocation;
      }
    }

    if (!this.mBlank) {
      this._callProgressListeners("onLocationChange",
                                  [aWebProgress, aRequest, aLocation, aFlags]);
    }

    if (topLevel) {
      this.mBrowser.lastURI = aLocation;
      this.mBrowser.lastLocationChange = Date.now();
    }
  }

  onStatusChange(aWebProgress, aRequest, aStatus, aMessage) {
    if (this.mBlank)
      return;

    this._callProgressListeners("onStatusChange",
                                [aWebProgress, aRequest, aStatus, aMessage]);

    this.mMessage = aMessage;
  }

  onSecurityChange(aWebProgress, aRequest, aState) {
    this._callProgressListeners("onSecurityChange",
                                [aWebProgress, aRequest, aState]);
  }

  onRefreshAttempted(aWebProgress, aURI, aDelay, aSameURI) {
    return this._callProgressListeners("onRefreshAttempted",
                                       [aWebProgress, aURI, aDelay, aSameURI]);
  }

  QueryInterface(aIID) {
    if (aIID.equals(Ci.nsIWebProgressListener) ||
        aIID.equals(Ci.nsIWebProgressListener2) ||
        aIID.equals(Ci.nsISupportsWeakReference) ||
        aIID.equals(Ci.nsISupports))
      return this;
    throw Cr.NS_NOINTERFACE;
  }
}

