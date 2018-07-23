/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/* eslint-env mozilla/browser-window */

/**
 * A set of known icons to use for internal pages. These are hardcoded so we can
 * start loading them faster than ContentLinkHandler would normally find them.
 */
const FAVICON_DEFAULTS = {
  "about:newtab": "chrome://branding/content/icon32.png",
  "about:home": "chrome://branding/content/icon32.png",
  "about:welcome": "chrome://branding/content/icon32.png",
  "about:privatebrowsing": "chrome://browser/skin/privatebrowsing/favicon.svg",
};

window._gBrowser = {
  init() {
    ChromeUtils.defineModuleGetter(this, "AsyncTabSwitcher",
      "resource:///modules/AsyncTabSwitcher.jsm");

    XPCOMUtils.defineLazyServiceGetters(this, {
      _unifiedComplete: ["@mozilla.org/autocomplete/search;1?name=unifiedcomplete", "mozIPlacesAutoComplete"],
      serializationHelper: ["@mozilla.org/network/serialization-helper;1", "nsISerializationHelper"],
      mURIFixup: ["@mozilla.org/docshell/urifixup;1", "nsIURIFixup"],
    });

    Services.obs.addObserver(this, "contextual-identity-updated");

    Services.els.addSystemEventListener(document, "keydown", this, false);
    if (AppConstants.platform == "macosx") {
      Services.els.addSystemEventListener(document, "keypress", this, false);
    }
    window.addEventListener("sizemodechange", this);
    window.addEventListener("occlusionstatechange", this);

    this._setupInitialBrowserAndTab();

    if (Services.prefs.getBoolPref("browser.display.use_system_colors")) {
      this.tabpanels.style.backgroundColor = "-moz-default-background-color";
    } else if (Services.prefs.getIntPref("browser.display.document_color_use") == 2) {
      this.tabpanels.style.backgroundColor =
        Services.prefs.getCharPref("browser.display.background_color");
    }

    let messageManager = window.getGroupMessageManager("browsers");
    if (gMultiProcessBrowser) {
      messageManager.addMessageListener("DOMTitleChanged", this);
      messageManager.addMessageListener("DOMWindowClose", this);
      window.messageManager.addMessageListener("contextmenu", this);
      messageManager.addMessageListener("Browser:Init", this);

      // If this window has remote tabs, switch to our tabpanels fork
      // which does asynchronous tab switching.
      this.tabpanels.classList.add("tabbrowser-tabpanels");
    } else {
      this._outerWindowIDBrowserMap.set(this.selectedBrowser.outerWindowID,
        this.selectedBrowser);
    }
    messageManager.addMessageListener("DOMWindowFocus", this);
    messageManager.addMessageListener("RefreshBlocker:Blocked", this);
    messageManager.addMessageListener("Browser:WindowCreated", this);

    // To correctly handle keypresses for potential FindAsYouType, while
    // the tab's find bar is not yet initialized.
    messageManager.addMessageListener("Findbar:Keypress", this);
    this._setFindbarData();

    XPCOMUtils.defineLazyPreferenceGetter(this, "animationsEnabled",
      "toolkit.cosmeticAnimations.enabled");
    XPCOMUtils.defineLazyPreferenceGetter(this, "schedulePressureDefaultCount",
      "browser.schedulePressure.defaultCount");

    this._setupEventListeners();
  },

  ownerGlobal: window,

  ownerDocument: document,

  closingTabsEnum: { ALL: 0, OTHER: 1, TO_END: 2, MULTI_SELECTED: 3 },

  _visibleTabs: null,

  _lastRelatedTabMap: new WeakMap(),

  mProgressListeners: [],

  mTabsProgressListeners: [],

  _tabListeners: new Map(),

  _tabFilters: new Map(),

  _isBusy: false,

  _outerWindowIDBrowserMap: new Map(),

  arrowKeysShouldWrap: AppConstants == "macosx",

  _autoScrollPopup: null,

  _previewMode: false,

  _lastFindValue: "",

  _contentWaitingCount: 0,

  _tabLayerCache: [],

  tabAnimationsInProgress: 0,

  _XUL_NS: "http://www.mozilla.org/keymaster/gatekeeper/there.is.only.xul",

  /**
   * Binding from browser to tab
   */
  _tabForBrowser: new WeakMap(),

  _preloadedBrowser: null,

  /**
   * `_createLazyBrowser` will define properties on the unbound lazy browser
   * which correspond to properties defined in XBL which will be bound to
   * the browser when it is inserted into the document.  If any of these
   * properties are accessed by consumers, `_insertBrowser` is called and
   * the browser is inserted to ensure that things don't break.  This list
   * provides the names of properties that may be called while the browser
   * is in its unbound (lazy) state.
   */
  _browserBindingProperties: [
    "canGoBack", "canGoForward", "goBack", "goForward", "permitUnload",
    "reload", "reloadWithFlags", "stop", "loadURI",
    "gotoIndex", "currentURI", "documentURI",
    "preferences", "imageDocument", "isRemoteBrowser", "messageManager",
    "getTabBrowser", "finder", "fastFind", "sessionHistory", "contentTitle",
    "characterSet", "fullZoom", "textZoom", "webProgress",
    "addProgressListener", "removeProgressListener", "audioPlaybackStarted",
    "audioPlaybackStopped", "pauseMedia", "stopMedia",
    "resumeMedia", "mute", "unmute", "blockedPopups", "lastURI",
    "purgeSessionHistory", "stopScroll", "startScroll",
    "userTypedValue", "userTypedClear",
    "didStartLoadSinceLastUserTyping", "audioMuted"
  ],

  _removingTabs: [],

  _multiSelectedTabsSet: new WeakSet(),

  _lastMultiSelectedTabRef: null,

  /**
   * Tab close requests are ignored if the window is closing anyway,
   * e.g. when holding Ctrl+W.
   */
  _windowIsClosing: false,

  /**
   * This defines a proxy which allows us to access browsers by
   * index without actually creating a full array of browsers.
   */
  browsers: new Proxy([], {
    has: (target, name) => {
      if (typeof name == "string" && Number.isInteger(parseInt(name))) {
        return (name in gBrowser.tabs);
      }
      return false;
    },
    get: (target, name) => {
      if (name == "length") {
        return gBrowser.tabs.length;
      }
      if (typeof name == "string" && Number.isInteger(parseInt(name))) {
        if (!(name in gBrowser.tabs)) {
          return undefined;
        }
        return gBrowser.tabs[name].linkedBrowser;
      }
      return target[name];
    }
  }),

  /**
   * List of browsers whose docshells must be active in order for print preview
   * to work.
   */
  _printPreviewBrowsers: new Set(),

  _switcher: null,

  _soundPlayingAttrRemovalTimer: 0,

  _hoverTabTimer: null,

  get tabContainer() {
    delete this.tabContainer;
    return this.tabContainer = document.getElementById("tabbrowser-tabs");
  },

  get tabs() {
    delete this.tabs;
    return this.tabs = this.tabContainer.childNodes;
  },

  get tabbox() {
    delete this.tabbox;
    return this.tabbox = document.getElementById("tabbrowser-tabbox");
  },

  get tabpanels() {
    delete this.tabpanels;
    return this.tabpanels = document.getElementById("tabbrowser-tabpanels");
  },

  get addEventListener() {
    delete this.addEventListener;
    return this.addEventListener = this.tabpanels.addEventListener.bind(this.tabpanels);
  },

  get removeEventListener() {
    delete this.removeEventListener;
    return this.removeEventListener = this.tabpanels.removeEventListener.bind(this.tabpanels);
  },

  get dispatchEvent() {
    delete this.dispatchEvent;
    return this.dispatchEvent = this.tabpanels.dispatchEvent.bind(this.tabpanels);
  },

  get visibleTabs() {
    if (!this._visibleTabs)
      this._visibleTabs = Array.filter(this.tabs,
        tab => !tab.hidden && !tab.closing);
    return this._visibleTabs;
  },

  get _numPinnedTabs() {
    for (var i = 0; i < this.tabs.length; i++) {
      if (!this.tabs[i].pinned)
        break;
    }
    return i;
  },

  get popupAnchor() {
    if (this.selectedTab._popupAnchor) {
      return this.selectedTab._popupAnchor;
    }
    let stack = this.selectedBrowser.parentNode;
    // Create an anchor for the popup
    let popupAnchor = document.createElementNS(this._XUL_NS, "hbox");
    popupAnchor.className = "popup-anchor";
    popupAnchor.hidden = true;
    stack.appendChild(popupAnchor);
    return this.selectedTab._popupAnchor = popupAnchor;
  },

  set selectedTab(val) {
    if (gNavToolbox.collapsed && !this._allowTabChange) {
      return this.tabbox.selectedTab;
    }
    // Update the tab
    this.tabbox.selectedTab = val;
    return val;
  },

  get selectedTab() {
    return this._selectedTab;
  },

  get selectedBrowser() {
    return this._selectedBrowser;
  },

  get initialBrowser() {
    delete this.initialBrowser;
    return this.initialBrowser = document.getElementById("tabbrowser-initialBrowser");
  },

  _setupInitialBrowserAndTab() {
    let browser = this.initialBrowser;
    this._selectedBrowser = browser;

    browser.permanentKey = {};
    browser.droppedLinkHandler = handleDroppedLink;

    let autoScrollPopup = browser._createAutoScrollPopup();
    autoScrollPopup.id = "autoscroller";
    document.getElementById("mainPopupSet").appendChild(autoScrollPopup);
    browser.setAttribute("autoscrollpopup", autoScrollPopup.id);

    this._defaultBrowserAttributes = {
      autoscrollpopup: "",
      contextmenu: "",
      datetimepicker: "",
      message: "",
      messagemanagergroup: "",
      selectmenulist: "",
      tooltip: "",
      type: "",
    };
    for (let attribute in this._defaultBrowserAttributes) {
      this._defaultBrowserAttributes[attribute] = browser.getAttribute(attribute);
    }

    let tab = this.tabs[0];
    this._selectedTab = tab;

    let uniqueId = this._generateUniquePanelID();
    this.tabpanels.childNodes[0].id = uniqueId;
    tab.linkedPanel = uniqueId;
    tab.permanentKey = browser.permanentKey;
    tab._tPos = 0;
    tab._fullyOpen = true;
    tab.linkedBrowser = browser;
    this._tabForBrowser.set(browser, tab);

    // Hook the browser up with a progress listener.
    let tabListener = new TabProgressListener(tab, browser, true, false);
    let filter = Cc["@mozilla.org/appshell/component/browser-status-filter;1"]
                  .createInstance(Ci.nsIWebProgress);
    filter.addProgressListener(tabListener, Ci.nsIWebProgress.NOTIFY_ALL);
    this._tabListeners.set(tab, tabListener);
    this._tabFilters.set(tab, filter);
    browser.webProgress.addProgressListener(filter, Ci.nsIWebProgress.NOTIFY_ALL);
  },

  /**
   * BEGIN FORWARDED BROWSER PROPERTIES.  IF YOU ADD A PROPERTY TO THE BROWSER ELEMENT
   * MAKE SURE TO ADD IT HERE AS WELL.
   */
  get canGoBack() {
    return this.selectedBrowser.canGoBack;
  },

  get canGoForward() {
    return this.selectedBrowser.canGoForward;
  },

  goBack() {
    return this.selectedBrowser.goBack();
  },

  goForward() {
    return this.selectedBrowser.goForward();
  },

  reload() {
    return this.selectedBrowser.reload();
  },

  reloadWithFlags(aFlags) {
    return this.selectedBrowser.reloadWithFlags(aFlags);
  },

  stop() {
    return this.selectedBrowser.stop();
  },

  /**
   * throws exception for unknown schemes
   */
  loadURI(aURI, aParams) {
    return this.selectedBrowser.loadURI(aURI, aParams);
  },

  gotoIndex(aIndex) {
    return this.selectedBrowser.gotoIndex(aIndex);
  },

  get currentURI() {
    return this.selectedBrowser.currentURI;
  },

  get finder() {
    return this.selectedBrowser.finder;
  },

  get docShell() {
    return this.selectedBrowser.docShell;
  },

  get webNavigation() {
    return this.selectedBrowser.webNavigation;
  },

  get webProgress() {
    return this.selectedBrowser.webProgress;
  },

  get contentWindow() {
    return this.selectedBrowser.contentWindow;
  },

  get contentWindowAsCPOW() {
    return this.selectedBrowser.contentWindowAsCPOW;
  },

  get sessionHistory() {
    return this.selectedBrowser.sessionHistory;
  },

  get markupDocumentViewer() {
    return this.selectedBrowser.markupDocumentViewer;
  },

  get contentDocument() {
    return this.selectedBrowser.contentDocument;
  },

  get contentDocumentAsCPOW() {
    return this.selectedBrowser.contentDocumentAsCPOW;
  },

  get contentTitle() {
    return this.selectedBrowser.contentTitle;
  },

  get contentPrincipal() {
    return this.selectedBrowser.contentPrincipal;
  },

  get securityUI() {
    return this.selectedBrowser.securityUI;
  },

  set fullZoom(val) {
    this.selectedBrowser.fullZoom = val;
  },

  get fullZoom() {
    return this.selectedBrowser.fullZoom;
  },

  set textZoom(val) {
    this.selectedBrowser.textZoom = val;
  },

  get textZoom() {
    return this.selectedBrowser.textZoom;
  },

  get isSyntheticDocument() {
    return this.selectedBrowser.isSyntheticDocument;
  },

  set userTypedValue(val) {
    return this.selectedBrowser.userTypedValue = val;
  },

  get userTypedValue() {
    return this.selectedBrowser.userTypedValue;
  },

  _setFindbarData() {
    // Ensure we know what the find bar key is in the content process:
    let initialProcessData = Services.ppmm.initialProcessData;
    if (!initialProcessData.findBarShortcutData) {
      let keyEl = document.getElementById("key_find");
      let mods = keyEl.getAttribute("modifiers")
        .replace(/accel/i, AppConstants.platform == "macosx" ? "meta" : "control");
      initialProcessData.findBarShortcutData = {
        key: keyEl.getAttribute("key"),
        modifiers: {
          shiftKey: mods.includes("shift"),
          ctrlKey: mods.includes("control"),
          altKey: mods.includes("alt"),
          metaKey: mods.includes("meta"),
        },
      };
      Services.ppmm.broadcastAsyncMessage("Findbar:ShortcutData",
        initialProcessData.findBarShortcutData);
    }
  },

  isFindBarInitialized(aTab) {
    return (aTab || this.selectedTab)._findBar != undefined;
  },

  /**
   * Get the already constructed findbar
   */
  getCachedFindBar(aTab = this.selectedTab) {
    return aTab._findBar;
  },

  /**
   * Get the findbar, and create it if it doesn't exist.
   * @return the find bar (or null if the window or tab is closed/closing in the interim).
   */
  async getFindBar(aTab = this.selectedTab) {
    let findBar = this.getCachedFindBar(aTab);
    if (findBar) {
      return findBar;
    }

    // Avoid re-entrancy by caching the promise we're about to return.
    if (!aTab._pendingFindBar) {
      aTab._pendingFindBar = this._createFindBar(aTab);
    }
    return aTab._pendingFindBar;
  },

  /**
   * Create a findbar instance.
   * @param aTab the tab to create the find bar for.
   * @return the created findbar, or null if the window or tab is closed/closing.
   */
  async _createFindBar(aTab) {
    let findBar = document.createElementNS(this._XUL_NS, "findbar");
    let browser = this.getBrowserForTab(aTab);
    let browserContainer = this.getBrowserContainer(browser);
    browserContainer.appendChild(findBar);

    await new Promise(r => requestAnimationFrame(r));
    delete aTab._pendingFindBar;
    if (window.closed || aTab.closing) {
      return null;
    }

    findBar.browser = browser;
    findBar._findField.value = this._lastFindValue;

    aTab._findBar = findBar;

    let event = document.createEvent("Events");
    event.initEvent("TabFindInitialized", true, false);
    aTab.dispatchEvent(event);

    return findBar;
  },

  _appendStatusPanel() {
    let browser = this.selectedBrowser;
    let browserContainer = this.getBrowserContainer(browser);
    browserContainer.insertBefore(StatusPanel.panel, browser.parentNode.nextSibling);
  },

  _updateTabBarForPinnedTabs() {
    this.tabContainer._unlockTabSizing();
    this.tabContainer._positionPinnedTabs();
    this.tabContainer._updateCloseButtons();
  },

  _notifyPinnedStatus(aTab) {
    this.getBrowserForTab(aTab).messageManager.sendAsyncMessage("Browser:AppTab", { isAppTab: aTab.pinned });

    let event = document.createEvent("Events");
    event.initEvent(aTab.pinned ? "TabPinned" : "TabUnpinned", true, false);
    aTab.dispatchEvent(event);
  },

  pinTab(aTab) {
    if (aTab.pinned)
      return;

    if (aTab.hidden)
      this.showTab(aTab);

    this.moveTabTo(aTab, this._numPinnedTabs);
    aTab.setAttribute("pinned", "true");
    this._updateTabBarForPinnedTabs();
    this._notifyPinnedStatus(aTab);
  },

  unpinTab(aTab) {
    if (!aTab.pinned)
      return;

    this.moveTabTo(aTab, this._numPinnedTabs - 1);
    aTab.removeAttribute("pinned");
    aTab.style.marginInlineStart = "";
    this._updateTabBarForPinnedTabs();
    this._notifyPinnedStatus(aTab);
  },

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
  },

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
          anim.playState === "running");

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
  },

  getBrowserAtIndex(aIndex) {
    return this.browsers[aIndex];
  },

  getBrowserIndexForDocument(aDocument) {
    var tab = this._getTabForContentWindow(aDocument.defaultView);
    return tab ? tab._tPos : -1;
  },

  getBrowserForDocument(aDocument) {
    var tab = this._getTabForContentWindow(aDocument.defaultView);
    return tab ? tab.linkedBrowser : null;
  },

  getBrowserForContentWindow(aWindow) {
    var tab = this._getTabForContentWindow(aWindow);
    return tab ? tab.linkedBrowser : null;
  },

  getBrowserForOuterWindowID(aID) {
    return this._outerWindowIDBrowserMap.get(aID);
  },

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
  },

  getTabForBrowser(aBrowser) {
    return this._tabForBrowser.get(aBrowser);
  },

  getNotificationBox(aBrowser) {
    return this.getSidebarContainer(aBrowser).parentNode;
  },

  getSidebarContainer(aBrowser) {
    return this.getBrowserContainer(aBrowser).parentNode;
  },

  getBrowserContainer(aBrowser) {
    return (aBrowser || this.selectedBrowser).parentNode.parentNode;
  },

  getTabModalPromptBox(aBrowser) {
    let browser = (aBrowser || this.selectedBrowser);
    if (!browser.tabModalPromptBox) {
      browser.tabModalPromptBox = new TabModalPromptBox(browser);
    }
    return browser.tabModalPromptBox;
  },

  getTabFromAudioEvent(aEvent) {
    if (!Services.prefs.getBoolPref("browser.tabs.showAudioPlayingIcon") ||
        !aEvent.isTrusted) {
      return null;
    }

    var browser = aEvent.originalTarget;
    var tab = this.getTabForBrowser(browser);
    return tab;
  },

  _callProgressListeners(aBrowser, aMethod, aArguments, aCallGlobalListeners = true, aCallTabsListeners = true) {
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

    aBrowser = aBrowser || this.selectedBrowser;

    if (aCallGlobalListeners && aBrowser == this.selectedBrowser) {
      callListeners(this.mProgressListeners, aArguments);
    }

    if (aCallTabsListeners) {
      aArguments.unshift(aBrowser);

      callListeners(this.mTabsProgressListeners, aArguments);
    }

    return rv;
  },

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
  },

  setIcon(aTab, aIconURL = "", aOriginalURL = aIconURL) {
    let makeString = (url) => url instanceof Ci.nsIURI ? url.spec : url;

    aIconURL = makeString(aIconURL);
    aOriginalURL = makeString(aOriginalURL);

    let LOCAL_PROTOCOLS = [
      "chrome:",
      "about:",
      "resource:",
      "data:",
    ];

    if (aIconURL && !LOCAL_PROTOCOLS.some(protocol => aIconURL.startsWith(protocol))) {
      console.error(`Attempt to set a remote URL ${aIconURL} as a tab icon.`);
      return;
    }

    let browser = this.getBrowserForTab(aTab);
    browser.mIconURL = aIconURL;

    if (aIconURL != aTab.getAttribute("image")) {
      if (aIconURL) {
        aTab.setAttribute("image", aIconURL);
      } else {
        aTab.removeAttribute("image");
      }
      this._tabAttrModified(aTab, ["image"]);
    }

    // The aOriginalURL argument is currently only used by tests.
    this._callProgressListeners(browser, "onLinkIconAvailable", [aIconURL, aOriginalURL]);
  },

  getIcon(aTab) {
    let browser = aTab ? this.getBrowserForTab(aTab) : this.selectedBrowser;
    return browser.mIconURL;
  },

  setPageInfo(aURL, aDescription, aPreviewImage) {
    if (aURL) {
      let pageInfo = { url: aURL, description: aDescription, previewImageURL: aPreviewImage };
      PlacesUtils.history.update(pageInfo).catch(Cu.reportError);
    }
  },

  getWindowTitleForBrowser(aBrowser) {
    var newTitle = "";
    var docElement = document.documentElement;
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
  },

  updateTitlebar() {
    document.title = this.getWindowTitleForBrowser(this.selectedBrowser);
  },

  updateCurrentBrowser(aForceUpdate) {
    let newBrowser = this.getBrowserAtIndex(this.tabContainer.selectedIndex);
    if (this.selectedBrowser == newBrowser && !aForceUpdate) {
      return;
    }

    if (!aForceUpdate) {
      document.commandDispatcher.lock();

      TelemetryStopwatch.start("FX_TAB_SWITCH_UPDATE_MS");
    }

    let oldTab = this.selectedTab;

    // Preview mode should not reset the owner
    if (!this._previewMode && !oldTab.selected)
      oldTab.owner = null;

    let lastRelatedTab = this._lastRelatedTabMap.get(oldTab);
    if (lastRelatedTab) {
      if (!lastRelatedTab.selected)
        lastRelatedTab.owner = null;
    }
    this._lastRelatedTabMap = new WeakMap();

    let oldBrowser = this.selectedBrowser;

    if (!gMultiProcessBrowser) {
      oldBrowser.removeAttribute("primary");
      oldBrowser.docShellIsActive = false;
      newBrowser.setAttribute("primary", "true");
      newBrowser.docShellIsActive =
        (window.windowState != window.STATE_MINIMIZED &&
          !window.isFullyOccluded);
    }

    let newTab = this.getTabForBrowser(newBrowser);
    this._selectedBrowser = newBrowser;
    this._selectedTab = newTab;
    this.showTab(newTab);

    gURLBar.setAttribute("switchingtabs", "true");
    window.addEventListener("MozAfterPaint", function() {
      gURLBar.removeAttribute("switchingtabs");
    }, { once: true });

    this._appendStatusPanel();

    if ((oldBrowser.blockedPopups && !newBrowser.blockedPopups) ||
        (!oldBrowser.blockedPopups && newBrowser.blockedPopups)) {
      newBrowser.updateBlockedPopups();
    }

    // Update the URL bar.
    let webProgress = newBrowser.webProgress;
    this._callProgressListeners(null, "onLocationChange",
                                [webProgress, null, newBrowser.currentURI, 0],
                                true, false);

    let securityUI = newBrowser.securityUI;
    if (securityUI) {
      // Include the true final argument to indicate that this event is
      // simulated (instead of being observed by the webProgressListener).
      this._callProgressListeners(null, "onSecurityChange",
                                  [webProgress, null, securityUI.state, true],
                                  true, false);
    }

    let listener = this._tabListeners.get(newTab);
    if (listener && listener.mStateFlags) {
      this._callProgressListeners(null, "onUpdateCurrentBrowser",
                                  [listener.mStateFlags, listener.mStatus,
                                   listener.mMessage, listener.mTotalProgress],
                                  true, false);
    }

    if (!this._previewMode) {
      newTab.updateLastAccessed();
      oldTab.updateLastAccessed();

      let oldFindBar = oldTab._findBar;
      if (oldFindBar &&
          oldFindBar.findMode == oldFindBar.FIND_NORMAL &&
          !oldFindBar.hidden)
        this._lastFindValue = oldFindBar._findField.value;

      this.updateTitlebar();

      newTab.removeAttribute("titlechanged");
      newTab.removeAttribute("attention");

      // The tab has been selected, it's not unselected anymore.
      // (1) Call the current tab's finishUnselectedTabHoverTimer()
      //     to save a telemetry record.
      // (2) Call the current browser's unselectedTabHover() with false
      //     to dispatch an event.
      newTab.finishUnselectedTabHoverTimer();
      newBrowser.unselectedTabHover(false);
    }

    // If the new tab is busy, and our current state is not busy, then
    // we need to fire a start to all progress listeners.
    if (newTab.hasAttribute("busy") && !this._isBusy) {
      this._isBusy = true;
      this._callProgressListeners(null, "onStateChange",
                                  [webProgress, null,
                                   Ci.nsIWebProgressListener.STATE_START |
                                   Ci.nsIWebProgressListener.STATE_IS_NETWORK, 0],
                                  true, false);
    }

    // If the new tab is not busy, and our current state is busy, then
    // we need to fire a stop to all progress listeners.
    if (!newTab.hasAttribute("busy") && this._isBusy) {
      this._isBusy = false;
      this._callProgressListeners(null, "onStateChange",
                                  [webProgress, null,
                                   Ci.nsIWebProgressListener.STATE_STOP |
                                   Ci.nsIWebProgressListener.STATE_IS_NETWORK, 0],
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
      newTab.dispatchEvent(event);
      Services.telemetry.recordEvent("savant", "tab", "select", null, { subcategory: "frame" });

      this._tabAttrModified(oldTab, ["selected"]);
      this._tabAttrModified(newTab, ["selected"]);

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
        this._adjustFocusBeforeTabSwitch(oldTab, newTab);
        this._adjustFocusAfterTabSwitch(newTab);
      }
    }

    updateUserContextUIIndicator();
    gIdentityHandler.updateSharingIndicator();

    // Enable touch events to start a native dragging
    // session to allow the user to easily drag the selected tab.
    // This is currently only supported on Windows.
    oldTab.removeAttribute("touchdownstartsdrag");
    newTab.setAttribute("touchdownstartsdrag", "true");

    if (!gMultiProcessBrowser) {
      this.tabContainer._setPositionalAttributes();

      document.commandDispatcher.unlock();

      let event = new CustomEvent("TabSwitchDone", {
        bubbles: true,
        cancelable: true
      });
      this.dispatchEvent(event);
    }

    if (!aForceUpdate)
      TelemetryStopwatch.finish("FX_TAB_SWITCH_UPDATE_MS");
  },

  _adjustFocusBeforeTabSwitch(oldTab, newTab) {
    if (this._previewMode) {
      return;
    }

    let oldBrowser = oldTab.linkedBrowser;
    let newBrowser = newTab.linkedBrowser;

    oldBrowser._urlbarFocused = (gURLBar && gURLBar.focused);

    if (this.isFindBarInitialized(oldTab)) {
      let findBar = this.getCachedFindBar(oldTab);
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
  },

  _adjustFocusAfterTabSwitch(newTab) {
    // Don't steal focus from the tab bar.
    if (document.activeElement == newTab)
      return;

    let newBrowser = this.getBrowserForTab(newTab);

    // If there's a tabmodal prompt showing, focus it.
    if (newBrowser.hasAttribute("tabmodalPromptShowing")) {
      let prompts = newBrowser.parentNode.getElementsByTagNameNS(this._XUL_NS, "tabmodalprompt");
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
  },

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
  },

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

    if (aBrowser == this.selectedBrowser) {
      gIdentityHandler.updateSharingIndicator();
    }
  },

  getTabSharingState(aTab) {
    // Normalize the state object for consumers (ie.extensions).
    let state = Object.assign({}, aTab._sharingState);
    return {
      camera: !!state.camera,
      microphone: !!state.microphone,
      screen: state.screen && state.screen.replace("Paused", ""),
    };
  },

  setInitialTabTitle(aTab, aTitle, aOptions) {
    if (aTitle) {
      if (!aTab.getAttribute("label")) {
        aTab._labelIsInitialTitle = true;
      }

      this._setTabLabel(aTab, aTitle, aOptions);
    }
  },

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
      // See if we can use the URI as the title.
      if (browser.currentURI.displaySpec) {
        try {
          title = this.mURIFixup.createExposableURI(browser.currentURI).displaySpec;
        } catch (ex) {
          title = browser.currentURI.displaySpec;
        }
      }

      if (title && !isBlankPageURL(title)) {
        // If it's a long data: URI that uses base64 encoding, truncate to a
        // reasonable length rather than trying to display the entire thing,
        // which can be slow.
        // We can't shorten arbitrary URIs like this, as bidi etc might mean
        // we need the trailing characters for display. But a base64-encoded
        // data-URI is plain ASCII, so this is OK for tab-title display.
        // (See bug 1408854.)
        if (title.length > 500 && title.match(/^data:[^,]+;base64,/)) {
          title = title.substring(0, 500) + "\u2026";
        } else {
          // Try to unescape not-ASCII URIs using the current character set.
          try {
            let characterSet = browser.characterSet;
            title = Services.textToSubURI.unEscapeNonAsciiURI(characterSet, title);
          } catch (ex) { /* Do nothing. */ }
        }
      } else {
        // No suitable URI? Fall back to our untitled string.
        title = this.tabContainer.emptyTabTitle;
      }
    }

    return this._setTabLabel(aTab, title, { isContentTitle });
  },

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
  },

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
  },

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

    // When bulk opening tabs, such as from a bookmark folder, we want to insertAfterCurrent
    // if necessary, but we also will set the bulkOrderedOpen flag so that the bookmarks
    // open in the same order they are in the folder.
    if (multiple && aNewIndex < 0 && Services.prefs.getBoolPref("browser.tabs.insertAfterCurrent")) {
      aNewIndex = this.selectedTab._tPos + 1;
    }

    if (aReplace) {
      let browser;
      if (aTargetTab) {
        browser = this.getBrowserForTab(aTargetTab);
        targetTabIndex = aTargetTab._tPos;
      } else {
        browser = this.selectedBrowser;
        targetTabIndex = this.tabContainer.selectedIndex;
      }
      let flags = Ci.nsIWebNavigation.LOAD_FLAGS_NONE;
      if (aAllowThirdPartyFixup) {
        flags |= Ci.nsIWebNavigation.LOAD_FLAGS_ALLOW_THIRD_PARTY_FIXUP |
          Ci.nsIWebNavigation.LOAD_FLAGS_FIXUP_SCHEME_TYPOS;
      }
      try {
        browser.loadURI(aURIs[0], {
          flags,
          postData: aPostDatas[0],
          triggeringPrincipal: aTriggeringPrincipal,
        });
      } catch (e) {
        // Ignore failure in case a URI is wrong, so we can continue
        // opening the next ones.
      }
    } else {
      let params = {
        ownerTab: owner,
        skipAnimation: multiple,
        allowThirdPartyFixup: aAllowThirdPartyFixup,
        postData: aPostDatas[0],
        userContextId: aUserContextId,
        triggeringPrincipal: aTriggeringPrincipal,
        bulkOrderedOpen: multiple,
      };
      if (aNewIndex > -1) {
        params.index = aNewIndex;
      }
      firstTabAdded = this.addTab(aURIs[0], params);
      if (aNewIndex > -1) {
        targetTabIndex = firstTabAdded._tPos;
      }
    }

    let tabNum = targetTabIndex;
    for (let i = 1; i < aURIs.length; ++i) {
      let params = {
        skipAnimation: true,
        allowThirdPartyFixup: aAllowThirdPartyFixup,
        postData: aPostDatas[i],
        userContextId: aUserContextId,
        triggeringPrincipal: aTriggeringPrincipal,
        bulkOrderedOpen: true,
      };
      if (targetTabIndex > -1) {
        params.index = ++tabNum;
      }
      this.addTab(aURIs[i], params);
    }

    if (firstTabAdded && !aLoadInBackground) {
      this.selectedTab = firstTabAdded;
    }
  },

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
    aBrowser.remove();
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
      this.getCachedFindBar(tab).browser = aBrowser;
    }

    tab.linkedBrowser
       .messageManager
       .sendAsyncMessage("Browser:HasSiblings", this.tabs.length > 1);

    evt = document.createEvent("Events");
    evt.initEvent("TabRemotenessChange", true, false);
    tab.dispatchEvent(evt);

    return true;
  },

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
  },

  removePreloadedBrowser() {
    if (!this._isPreloadingEnabled()) {
      return;
    }

    let browser = this._getPreloadedBrowser();

    if (browser) {
      browser.remove();
    }
  },

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
      browser.setAttribute("autocompletepopup", "PopupAutoComplete");
    }

    return browser;
  },

  _isPreloadingEnabled() {
    // Preloading for the newtab page is enabled when the prefs are true
    // and the URL is "about:newtab". We do not support preloading for
    // custom newtab URLs -- only for the default Firefox Home page.
    return Services.prefs.getBoolPref("browser.newtab.preload") &&
      Services.prefs.getBoolPref("browser.newtabpage.enabled") &&
      !aboutNewTabService.overridden;
  },

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
    this.tabpanels.appendChild(notificationbox);

    if (remoteType != E10SUtils.NOT_REMOTE) {
      // For remote browsers, we need to make sure that the webProgress is
      // instantiated, otherwise the parent won't get informed about the state
      // of the preloaded browser until it gets attached to a tab.
      browser.webProgress;
    }

    browser.loadURI(BROWSER_NEW_TAB_URL);
    browser.docShellIsActive = false;
    browser._urlbarFocused = true;

    // Make sure the preloaded browser is loaded with desired zoom level
    let tabURI = Services.io.newURI(BROWSER_NEW_TAB_URL);
    FullZoom.onLocationChange(tabURI, false, browser);
  },

  _createBrowser(aParams) {
    // Supported parameters:
    // userContextId, remote, remoteType, isPreloadBrowser,
    // uriIsAboutBlank, sameProcessAsFrameLoader

    let b = document.createElementNS(this._XUL_NS, "browser");
    b.permanentKey = {};

    for (let attribute in this._defaultBrowserAttributes) {
      b.setAttribute(attribute, this._defaultBrowserAttributes[attribute]);
    }

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

    if (!aParams.isPreloadBrowser) {
      b.setAttribute("autocompletepopup", "PopupAutoComplete");
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
    let stack = document.createElementNS(this._XUL_NS, "stack");
    stack.className = "browserStack";
    stack.appendChild(b);
    stack.setAttribute("flex", "1");

    // Create the browserContainer
    let browserContainer = document.createElementNS(this._XUL_NS, "vbox");
    browserContainer.className = "browserContainer";
    browserContainer.appendChild(stack);
    browserContainer.setAttribute("flex", "10000");

    // Create the sidebar container
    let browserSidebarContainer = document.createElementNS(this._XUL_NS, "hbox");
    browserSidebarContainer.className = "browserSidebarContainer";
    browserSidebarContainer.appendChild(browserContainer);
    browserSidebarContainer.setAttribute("flex", "10000");

    // Add the Message and the Browser to the box
    let notificationbox = document.createElementNS(this._XUL_NS, "notificationbox");
    notificationbox.setAttribute("flex", "1");
    notificationbox.setAttribute("notificationside", "top");
    notificationbox.appendChild(browserSidebarContainer);

    // Prevent the superfluous initial load of a blank document
    // if we're going to load something other than about:blank.
    if (!aParams.uriIsAboutBlank) {
      b.setAttribute("nodefaultsrc", "true");
    }

    return b;
  },

  _createLazyBrowser(aTab) {
    let browser = aTab.linkedBrowser;

    let names = this._browserBindingProperties;

    for (let i = 0; i < names.length; i++) {
      let name = names[i];
      let getter;
      let setter;
      switch (name) {
        case "audioMuted":
          getter = () => aTab.hasAttribute("muted");
          break;
        case "contentTitle":
          getter = () => SessionStore.getLazyTabValue(aTab, "title");
          break;
        case "currentURI":
          getter = () => {
            let url = SessionStore.getLazyTabValue(aTab, "url");
            // Avoid recreating the same nsIURI object over and over again...
            if (browser._cachedCurrentURI) {
              return browser._cachedCurrentURI;
            }
            return browser._cachedCurrentURI = Services.io.newURI(url);
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
        case "userTypedValue":
        case "userTypedClear":
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
  },

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
    delete aTab._cachedCurrentURI;

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
      this.tabpanels.appendChild(notificationbox);
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

    // Most of the time, we start our browser's docShells out as inactive,
    // and then maintain activeness in the tab switcher. Preloaded about:newtab's
    // are already created with their docShell's as inactive, but then explicitly
    // render their layers to ensure that we can switch to them quickly. We avoid
    // setting docShellIsActive to false again in this case, since that'd cause
    // the layers for the preloaded tab to be dropped, and we'd see a flash
    // of empty content instead.
    //
    // So for all browsers except for the preloaded case, we set the browser
    // docShell to inactive.
    if (!usingPreloadedContent) {
      browser.docShellIsActive = false;
    }

    // When addTab() is called with an URL that is not "about:blank" we
    // set the "nodefaultsrc" attribute that prevents a frameLoader
    // from being created as soon as the linked <browser> is inserted
    // into the DOM. We thus have to register the new outerWindowID
    // for non-remote browsers after we have called browser.loadURI().
    if (remoteType == E10SUtils.NOT_REMOTE) {
      this._outerWindowIDBrowserMap.set(browser.outerWindowID, browser);
    }

    // If we transitioned from one browser to two browsers, we need to set
    // hasSiblings=false on both the existing browser and the new browser.
    if (this.tabs.length == 2) {
      window.messageManager
            .broadcastAsyncMessage("Browser:HasSiblings", true);
    } else {
      aTab.linkedBrowser
          .messageManager
          .sendAsyncMessage("Browser:HasSiblings", this.tabs.length > 1);
    }

    var evt = new CustomEvent("TabBrowserInserted", { bubbles: true, detail: { insertedOnTabCreation: aInsertedOnTabCreation } });
    aTab.dispatchEvent(evt);
  },

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

    // Reset webrtc sharing state.
    if (tab._sharingState) {
      this.setBrowserSharing(aBrowser, {});
    }
    webrtcUI.forgetStreamsFromBrowser(aBrowser);

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
    this.getNotificationBox(aBrowser).remove();
    tab.removeAttribute("linkedpanel");

    this._createLazyBrowser(tab);

    let evt = new CustomEvent("TabBrowserDiscarded", { bubbles: true });
    tab.dispatchEvent(evt);
  },

  // eslint-disable-next-line complexity
  addTab(aURI, {
    allowMixedContent,
    allowThirdPartyFixup,
    bulkOrderedOpen,
    charset,
    createLazyBrowser,
    disallowInheritPrincipal,
    eventDetail,
    focusUrlBar,
    forceNotRemote,
    fromExternal,
    index,
    name,
    nextTabParentId,
    noInitialLabel,
    noReferrer,
    opener,
    openerBrowser,
    originPrincipal,
    ownerTab,
    pinned,
    postData,
    preferredRemoteType,
    referrerPolicy,
    referrerURI,
    relatedToCurrent,
    sameProcessAsFrameLoader,
    skipAnimation,
    skipBackgroundNotify,
    triggeringPrincipal,
    userContextId,
  } = {}) {
    // if we're adding tabs, we're past interrupt mode, ditch the owner
    if (this.selectedTab.owner) {
      this.selectedTab.owner = null;
    }

    // Find the tab that opened this one, if any. This is used for
    // determining positioning, and inherited attributes such as the
    // user context ID.
    //
    // If we have a browser opener (which is usually the browser
    // element from a remote window.open() call), use that.
    //
    // Otherwise, if the tab is related to the current tab (e.g.,
    // because it was opened by a link click), use the selected tab as
    // the owner. If referrerURI is set, and we don't have an
    // explicit relatedToCurrent arg, we assume that the tab is
    // related to the current tab, since referrerURI is null or
    // undefined if the tab is opened from an external application or
    // bookmark (i.e. somewhere other than an existing tab).
    if (relatedToCurrent == null) {
      relatedToCurrent = !!referrerURI;
    }
    let openerTab = ((openerBrowser && this.getTabForBrowser(openerBrowser)) ||
      (relatedToCurrent && this.selectedTab));

    var t = document.createElementNS(this._XUL_NS, "tab");

    t.openerTab = openerTab;

    aURI = aURI || "about:blank";
    let aURIObject = null;
    try {
      aURIObject = Services.io.newURI(aURI);
    } catch (ex) { /* we'll try to fix up this URL later */ }

    let lazyBrowserURI;
    if (createLazyBrowser && aURI != "about:blank") {
      lazyBrowserURI = aURIObject;
      aURI = "about:blank";
    }

    var uriIsAboutBlank = aURI == "about:blank";

    if (!noInitialLabel) {
      if (isBlankPageURL(aURI)) {
        t.setAttribute("label", this.tabContainer.emptyTabTitle);
      } else {
        // Set URL as label so that the tab isn't empty initially.
        this.setInitialTabTitle(t, aURI, { beforeTabOpen: true });
      }
    }

    // Related tab inherits current tab's user context unless a different
    // usercontextid is specified
    if (userContextId == null && openerTab) {
      userContextId = openerTab.getAttribute("usercontextid") || 0;
    }

    if (userContextId) {
      t.setAttribute("usercontextid", userContextId);
      ContextualIdentityService.setTabStyle(t);
    }

    if (skipBackgroundNotify) {
      t.setAttribute("skipbackgroundnotify", true);
    }

    if (pinned) {
      t.setAttribute("pinned", "true");
    }

    t.className = "tabbrowser-tab";

    this.tabContainer._unlockTabSizing();

    // When overflowing, new tabs are scrolled into view smoothly, which
    // doesn't go well together with the width transition. So we skip the
    // transition in that case.
    let animate = !skipAnimation && !pinned &&
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

    let usingPreloadedContent = false;
    let b;

    try {
      // If this new tab is owned by another, assert that relationship
      if (ownerTab) {
        t.owner = ownerTab;
      }

      // Ensure we have an index if one was not provided. _insertTabAt
      // will do some additional validation.
      if (typeof index != "number") {
        // Move the new tab after another tab if needed.
        if (!bulkOrderedOpen &&
            ((openerTab &&
              Services.prefs.getBoolPref("browser.tabs.insertRelatedAfterCurrent")) ||
             Services.prefs.getBoolPref("browser.tabs.insertAfterCurrent"))) {

          let lastRelatedTab = openerTab && this._lastRelatedTabMap.get(openerTab);
          index = (lastRelatedTab || openerTab || this.selectedTab)._tPos + 1;

          if (lastRelatedTab) {
            lastRelatedTab.owner = null;
          } else if (openerTab) {
            t.owner = openerTab;
          }
          // Always set related map if opener exists.
          if (openerTab) {
            this._lastRelatedTabMap.set(openerTab, t);
          }
        } else {
          // This is intentionally past bounds, see the comment below on insertBefore.
          index = this.tabs.length;
        }
      }
      // Ensure position respectes tab pinned state.
      if (pinned) {
        index = Math.min(index, this._numPinnedTabs);
      } else {
        index = Math.max(index, this._numPinnedTabs);
      }

      // use .item() instead of [] because dragging to the end of the strip goes out of
      // bounds: .item() returns null (so it acts like appendChild), but [] throws
      let tabAfter = this.tabs.item(index);
      this.tabContainer.insertBefore(t, tabAfter);
      if (tabAfter) {
        this._updateTabsAfterInsert();
      } else {
        t._tPos = index;
      }

      if (pinned) {
        this._updateTabBarForPinnedTabs();
      }
      this.tabContainer._setPositionalAttributes();

      TabBarVisibility.update();

      // If we don't have a preferred remote type, and we have a remote
      // opener, use the opener's remote type.
      if (!preferredRemoteType && openerBrowser) {
        preferredRemoteType = openerBrowser.remoteType;
      }

      // If URI is about:blank and we don't have a preferred remote type,
      // then we need to use the referrer, if we have one, to get the
      // correct remote type for the new tab.
      if (uriIsAboutBlank && !preferredRemoteType && referrerURI) {
        preferredRemoteType =
          E10SUtils.getRemoteTypeForURI(referrerURI.spec, gMultiProcessBrowser);
      }

      let remoteType =
        forceNotRemote ? E10SUtils.NOT_REMOTE :
        E10SUtils.getRemoteTypeForURI(aURI, gMultiProcessBrowser,
          preferredRemoteType);

      // If we open a new tab with the newtab URL in the default
      // userContext, check if there is a preloaded browser ready.
      // Private windows are not included because both the label and the
      // icon for the tab would be set incorrectly (see bug 1195981).
      if (aURI == BROWSER_NEW_TAB_URL &&
          !userContextId &&
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
          userContextId,
          sameProcessAsFrameLoader,
          openerWindow: opener,
          nextTabParentId,
          name,
        });
      }

      t.linkedBrowser = b;

      if (focusUrlBar) {
        b._urlbarFocused = true;
      }

      this._tabForBrowser.set(b, t);
      t.permanentKey = b.permanentKey;
      t._browserParams = {
        uriIsAboutBlank,
        remoteType,
        usingPreloadedContent,
      };

      // If the caller opts in, create a lazy browser.
      if (createLazyBrowser) {
        this._createLazyBrowser(t);

        if (lazyBrowserURI) {
          // Lazy browser must be explicitly registered so tab will appear as
          // a switch-to-tab candidate in autocomplete.
          this._unifiedComplete.registerOpenPage(lazyBrowserURI, userContextId);
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

    // Hack to ensure that the about:newtab, and about:welcome favicon is loaded
    // instantaneously, to avoid flickering and improve perceived performance.
    if (aURI in FAVICON_DEFAULTS) {
      this.setIcon(t, FAVICON_DEFAULTS[aURI]);
    }

    // Dispatch a new tab notification.  We do this once we're
    // entirely done, so that things are in a consistent state
    // even if the event listener opens or closes tabs.
    let evt = new CustomEvent("TabOpen", { bubbles: true, detail: eventDetail || {} });
    t.dispatchEvent(evt);
    Services.telemetry.recordEvent("savant", "tab", "open", null, { subcategory: "frame" });

    if (!usingPreloadedContent && originPrincipal && aURI) {
      let { URI_INHERITS_SECURITY_CONTEXT } = Ci.nsIProtocolHandler;
      // Unless we know for sure we're not inheriting principals,
      // force the about:blank viewer to have the right principal:
      if (!aURIObject ||
          (doGetProtocolFlags(aURIObject) & URI_INHERITS_SECURITY_CONTEXT)) {
        b.createAboutBlankContentViewer(originPrincipal);
      }
    }

    // If we didn't swap docShells with a preloaded browser
    // then let's just continue loading the page normally.
    if (!usingPreloadedContent && (!uriIsAboutBlank || disallowInheritPrincipal)) {
      // pretend the user typed this so it'll be available till
      // the document successfully loads
      if (aURI && !gInitialPages.includes(aURI)) {
        b.userTypedValue = aURI;
      }

      let flags = Ci.nsIWebNavigation.LOAD_FLAGS_NONE;
      if (allowThirdPartyFixup) {
        flags |= Ci.nsIWebNavigation.LOAD_FLAGS_ALLOW_THIRD_PARTY_FIXUP;
        flags |= Ci.nsIWebNavigation.LOAD_FLAGS_FIXUP_SCHEME_TYPOS;
      }
      if (fromExternal) {
        flags |= Ci.nsIWebNavigation.LOAD_FLAGS_FROM_EXTERNAL;
      }
      if (allowMixedContent) {
        flags |= Ci.nsIWebNavigation.LOAD_FLAGS_ALLOW_MIXED_CONTENT;
      }
      if (disallowInheritPrincipal) {
        flags |= Ci.nsIWebNavigation.LOAD_FLAGS_DISALLOW_INHERIT_PRINCIPAL;
      }
      try {
        b.loadURI(aURI, {
          flags,
          triggeringPrincipal,
          referrerURI: noReferrer ? null : referrerURI,
          referrerPolicy,
          charset,
          postData,
        });
      } catch (ex) {
        Cu.reportError(ex);
      }
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

    // Additionally send pinned tab events
    if (pinned) {
      this._notifyPinnedStatus(t);
    }

    return t;
  },

  warnAboutClosingTabs(tabsToClose, aCloseTabs, aOptionalMessage) {
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
    var warningMessage;
    if (aOptionalMessage) {
      warningMessage = aOptionalMessage;
    } else {
      warningMessage =
        PluralForm.get(tabsToClose, gTabBrowserBundle.GetStringFromName("tabs.closeWarningMultiple"))
          .replace("#1", tabsToClose);
    }
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
  },

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
  },

  removeTabsToTheEndFrom(aTab) {
    let tabs = this.getTabsToTheEndFrom(aTab);
    if (!this.warnAboutClosingTabs(tabs.length, this.closingTabsEnum.TO_END)) {
      return;
    }

    this.removeTabs(tabs);
  },

  /**
   * In a multi-select context, all unpinned and unselected tabs are removed.
   * Otherwise all unpinned tabs except aTab are removed.
   */
  removeAllTabsBut(aTab) {
    let tabsToRemove = [];
    if (aTab && aTab.multiselected) {
      tabsToRemove = this.visibleTabs.filter(tab => !tab.multiselected && !tab.pinned);
    } else {
      tabsToRemove = this.visibleTabs.filter(tab => tab != aTab && !tab.pinned);
      this.selectedTab = aTab;
    }

    if (!this.warnAboutClosingTabs(tabsToRemove.length, this.closingTabsEnum.OTHER)) {
      return;
    }

    this.removeTabs(tabsToRemove);
  },

  removeMultiSelectedTabs() {
    let selectedTabs = this.selectedTabs;
    if (!this.warnAboutClosingTabs(selectedTabs.length, this.closingTabsEnum.MULTI_SELECTED)) {
      return;
    }

    this.removeTabs(selectedTabs);
  },

  removeTabs(tabs) {
    let tabsWithBeforeUnload = [];
    let lastToClose;
    let aParams = {animation: true};
    for (let tab of tabs) {
      if (tab.selected)
        lastToClose = tab;
      else if (this._hasBeforeUnload(tab))
        tabsWithBeforeUnload.push(tab);
      else
        this.removeTab(tab, aParams);
    }
    for (let tab of tabsWithBeforeUnload) {
      this.removeTab(tab, aParams);
    }

    // Avoid changing the selected browser several times by removing it,
    // if appropriate, lastly.
    if (lastToClose) {
      this.removeTab(lastToClose, aParams);
    }
  },

  removeCurrentTab(aParams) {
    this.removeTab(this.selectedTab, aParams);
  },

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
    let windowUtils = window.QueryInterface(Ci.nsIInterfaceRequestor)
                            .getInterface(Ci.nsIDOMWindowUtils);
    // We have to sample the tab width now, since _beginRemoveTab might
    // end up modifying the DOM in such a way that aTab gets a new
    // frame created for it (for example, by updating the visually selected
    // state).
    let tabWidth = windowUtils.getBoundsWithoutFlushing(aTab).width;

    if (!this._beginRemoveTab(aTab, null, null, true, skipPermitUnload)) {
      TelemetryStopwatch.cancel("FX_TAB_CLOSE_TIME_ANIM_MS", aTab);
      TelemetryStopwatch.cancel("FX_TAB_CLOSE_TIME_NO_ANIM_MS", aTab);
      return;
    }

    if (!aTab.pinned && !aTab.hidden && aTab._fullyOpen && byMouse)
      this.tabContainer._lockTabSizing(aTab, tabWidth);
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
        console.assert(false, "Giving up waiting for the tab closing animation to finish (bug 608589)");
        tabbrowser._endRemoveTab(tab);
      }
    }, 3000, aTab, this);
  },

  _hasBeforeUnload(aTab) {
    let browser = aTab.linkedBrowser;
    return browser.isRemoteBrowser && browser.frameLoader &&
           browser.frameLoader.tabParent &&
           browser.frameLoader.tabParent.hasBeforeUnload;
  },

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

    // this._switcher would normally cover removing a tab from this
    // cache, but we may not have one at this time.
    let tabCacheIndex = this._tabLayerCache.indexOf(aTab);
    if (tabCacheIndex != -1) {
      this._tabLayerCache.splice(tabCacheIndex, 1);
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
      // Remove the tab's filter and progress listener to avoid leaking.
      if (aTab.linkedPanel) {
        const filter = this._tabFilters.get(aTab);
        browser.webProgress.removeProgressListener(filter);
        const listener = this._tabListeners.get(aTab);
        filter.removeProgressListener(listener);
        listener.destroy();
        this._tabListeners.delete(aTab);
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
      TabBarVisibility.update();

    // We're committed to closing the tab now.
    // Dispatch a notification.
    // We dispatch it before any teardown so that event listeners can
    // inspect the tab that's about to close.
    var evt = new CustomEvent("TabClose", { bubbles: true, detail: { adoptedBy: aAdoptedByTab } });
    aTab.dispatchEvent(evt);
    Services.telemetry.recordEvent("savant", "tab", "close", null, { subcategory: "frame" });

    if (this.tabs.length == 2) {
      // We're closing one of our two open tabs, inform the other tab that its
      // sibling is going away.
      window.messageManager
            .broadcastAsyncMessage("Browser:HasSiblings", false);
    }

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
  },

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
    aTab.remove();

    // Update hashiddentabs if this tab was hidden.
    if (aTab.hidden)
      this.tabContainer._updateHiddenTabsStatus();

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
  },

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
  },

  _blurTab(aTab) {
    this.selectedTab = this._findTabToBlurTo(aTab);
  },

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
        this.setIcon(aOurTab, otherBrowser.mIconURL);
      var isBusy = aOtherTab.hasAttribute("busy");
      if (isBusy) {
        aOurTab.setAttribute("busy", "true");
        modifiedAttrs.push("busy");
        if (aOurTab.selected)
          this._isBusy = true;
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
      let oldValue = otherFindBar._findField.value;
      let wasHidden = otherFindBar.hidden;
      let ourFindBarPromise = this.getFindBar(aOurTab);
      ourFindBarPromise.then(ourFindBar => {
        if (!ourFindBar) {
          return;
        }
        ourFindBar._findField.value = oldValue;
        if (!wasHidden)
          ourFindBar.onFindCommand();
      });
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
  },

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
  },

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
  },

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
  },

  reloadAllTabs() {
    this.reloadTabs(this.visibleTabs);
  },

  reloadMultiSelectedTabs() {
    this.reloadTabs(this.selectedTabs);
  },

  reloadTabs(tabs) {
    for (let tab of tabs) {
      try {
        this.getBrowserForTab(tab).reload();
      } catch (e) {
        // ignore failure to reload so others will be reloaded
      }
    }
  },

  reloadTab(aTab) {
    let browser = this.getBrowserForTab(aTab);
    // Reset temporary permissions on the current tab. This is done here
    // because we only want to reset permissions on user reload.
    SitePermissions.clearTemporaryPermissions(browser);
    browser.reload();
  },

  addProgressListener(aListener) {
    if (arguments.length != 1) {
      Cu.reportError("gBrowser.addProgressListener was " +
        "called with a second argument, " +
        "which is not supported. See bug " +
        "608628. Call stack: " + new Error().stack);
    }

    this.mProgressListeners.push(aListener);
  },

  removeProgressListener(aListener) {
    this.mProgressListeners =
      this.mProgressListeners.filter(l => l != aListener);
  },

  addTabsProgressListener(aListener) {
    this.mTabsProgressListeners.push(aListener);
  },

  removeTabsProgressListener(aListener) {
    this.mTabsProgressListeners =
      this.mTabsProgressListeners.filter(l => l != aListener);
  },

  getBrowserForTab(aTab) {
    return aTab.linkedBrowser;
  },

  showOnlyTheseTabs(aTabs) {
    for (let tab of this.tabs) {
      if (!aTabs.includes(tab))
        this.hideTab(tab);
      else
        this.showTab(tab);
    }

    this.tabContainer._updateHiddenTabsStatus();
    this.tabContainer._handleTabSelect(true);
  },

  showTab(aTab) {
    if (aTab.hidden) {
      aTab.removeAttribute("hidden");
      this._visibleTabs = null; // invalidate cache

      this.tabContainer._updateCloseButtons();
      this.tabContainer._updateHiddenTabsStatus();

      this.tabContainer._setPositionalAttributes();

      let event = document.createEvent("Events");
      event.initEvent("TabShow", true, false);
      aTab.dispatchEvent(event);
      SessionStore.deleteCustomTabValue(aTab, "hiddenBy");
    }
  },

  hideTab(aTab, aSource) {
    if (!aTab.hidden && !aTab.pinned && !aTab.selected &&
        !aTab.closing && !aTab._sharingState) {
      aTab.setAttribute("hidden", "true");
      this._visibleTabs = null; // invalidate cache

      this.tabContainer._updateCloseButtons();
      this.tabContainer._updateHiddenTabsStatus();

      this.tabContainer._setPositionalAttributes();

      let event = document.createEvent("Events");
      event.initEvent("TabHide", true, false);
      aTab.dispatchEvent(event);
      if (aSource) {
        SessionStore.setCustomTabValue(aTab, "hiddenBy", aSource);
      }
    }
  },

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
  },

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
  },

  /**
   * Move contextTab (or selected tabs in a mutli-select context)
   * to a new browser window, unless it is (they are) already the only tab(s)
   * in the current window, in which case this will do nothing.
   */
  replaceTabsWithWindow(contextTab) {
    let tabs;
    if (contextTab.multiselected) {
      tabs = this.selectedTabs;
    } else {
      tabs = [contextTab];
    }

    if (this.tabs.length == tabs.length) {
      return null;
    }

    if (tabs.length == 1) {
      return this.replaceTabWithWindow(tabs[0]);
    }

    // The order of the tabs is reserved.
    // To avoid mutliple tab-switch, the active tab is "moved" lastly, if applicable.
    // If applicable, the active tab remains active in the new window.
    let activeTab = gBrowser.selectedTab;
    let inactiveTabs = tabs.filter(t => t != activeTab);
    let activeTabNewIndex = tabs.indexOf(activeTab);


    // Play the closing animation for all selected tabs to give
    // immediate feedback while waiting for the new window to appear.
    if (this.animationsEnabled) {
      for (let tab of tabs) {
        tab.style.maxWidth = ""; // ensure that fade-out transition happens
        tab.removeAttribute("fadein");
      }
    }

    let win;
    let firstInactiveTab = inactiveTabs[0];
    firstInactiveTab.linkedBrowser.addEventListener("EndSwapDocShells", function() {
      for (let i = 1; i < inactiveTabs.length; i++) {
        win.gBrowser.adoptTab(inactiveTabs[i], i);
      }

      if (activeTabNewIndex > -1) {
        win.gBrowser.adoptTab(activeTab, activeTabNewIndex, true /* aSelectTab */);
      }
    }, { once: true });

    win = this.replaceTabWithWindow(firstInactiveTab);
    return win;
  },

  _updateTabsAfterInsert() {
    for (let i = 0; i < this.tabs.length; i++) {
      this.tabs[i]._tPos = i;
      this.tabs[i]._selected = false;
    }

    // If we're in the midst of an async tab switch while calling
    // moveTabTo, we can get into a case where _visuallySelected
    // is set to true on two different tabs.
    //
    // What we want to do in moveTabTo is to remove logical selection
    // from all tabs, and then re-add logical selection to selectedTab
    // (and visual selection as well if we're not running with e10s, which
    // setting _selected will do automatically).
    //
    // If we're running with e10s, then the visual selection will not
    // be changed, which is fine, since if we weren't in the midst of a
    // tab switch, the previously visually selected tab should still be
    // correct, and if we are in the midst of a tab switch, then the async
    // tab switcher will set the visually selected tab once the tab switch
    // has completed.
    this.selectedTab._selected = true;
  },

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

    let wasFocused = (document.activeElement == this.selectedTab);

    aIndex = aIndex < aTab._tPos ? aIndex : aIndex + 1;

    // invalidate cache
    this._visibleTabs = null;

    // use .item() instead of [] because dragging to the end of the strip goes out of
    // bounds: .item() returns null (so it acts like appendChild), but [] throws
    this.tabContainer.insertBefore(aTab, this.tabs.item(aIndex));
    this._updateTabsAfterInsert();

    if (wasFocused)
      this.selectedTab.focus();

    this.tabContainer._handleTabSelect(true);

    if (aTab.pinned)
      this.tabContainer._positionPinnedTabs();

    this.tabContainer._setPositionalAttributes();

    var evt = document.createEvent("UIEvents");
    evt.initUIEvent("TabMove", true, false, window, oldPosition);
    aTab.dispatchEvent(evt);
  },

  moveTabForward() {
    let nextTab = this.selectedTab.nextSibling;
    while (nextTab && nextTab.hidden)
      nextTab = nextTab.nextSibling;

    if (nextTab)
      this.moveTabTo(this.selectedTab, nextTab._tPos);
    else if (this.arrowKeysShouldWrap)
      this.moveTabToStart();
  },

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
      skipAnimation: true,
      index: aIndex,
    };

    let numPinned = this._numPinnedTabs;
    if (aIndex < numPinned || (aTab.pinned && aIndex == numPinned)) {
      params.pinned = true;
    }

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
  },

  moveTabBackward() {
    let previousTab = this.selectedTab.previousSibling;
    while (previousTab && previousTab.hidden)
      previousTab = previousTab.previousSibling;

    if (previousTab)
      this.moveTabTo(this.selectedTab, previousTab._tPos);
    else if (this.arrowKeysShouldWrap)
      this.moveTabToEnd();
  },

  moveTabToStart() {
    let tabPos = this.selectedTab._tPos;
    if (tabPos > 0)
      this.moveTabTo(this.selectedTab, 0);
  },

  moveTabToEnd() {
    let tabPos = this.selectedTab._tPos;
    if (tabPos < this.browsers.length - 1)
      this.moveTabTo(this.selectedTab, this.browsers.length - 1);
  },

  moveTabOver(aEvent) {
    let direction = window.getComputedStyle(document.documentElement).direction;
    if ((direction == "ltr" && aEvent.keyCode == KeyEvent.DOM_VK_RIGHT) ||
        (direction == "rtl" && aEvent.keyCode == KeyEvent.DOM_VK_LEFT)) {
      this.moveTabForward();
    } else {
      this.moveTabBackward();
    }
  },

  /**
   * @param   aTab
   *          Can be from a different window as well
   * @param   aRestoreTabImmediately
   *          Can defer loading of the tab contents
   */
  duplicateTab(aTab, aRestoreTabImmediately) {
    return SessionStore.duplicateTab(window, aTab, 0, aRestoreTabImmediately);
  },

  addToMultiSelectedTabs(aTab, skipPositionalAttributes) {
    if (aTab.multiselected) {
      return;
    }

    aTab.setAttribute("multiselected", "true");
    this._multiSelectedTabsSet.add(aTab);

    if (!skipPositionalAttributes) {
      this.tabContainer._setPositionalAttributes();
    }
  },

  /**
   * Adds two given tabs and all tabs between them into the (multi) selected tabs collection
   */
  addRangeToMultiSelectedTabs(aTab1, aTab2) {
    if (aTab1 == aTab2) {
      return;
    }

    const tabs = this._visibleTabs;
    const indexOfTab1 = tabs.indexOf(aTab1);
    const indexOfTab2 = tabs.indexOf(aTab2);

    const [lowerIndex, higherIndex] = indexOfTab1 < indexOfTab2 ?
      [indexOfTab1, indexOfTab2] : [indexOfTab2, indexOfTab1];

    for (let i = lowerIndex; i <= higherIndex; i++) {
      this.addToMultiSelectedTabs(tabs[i], true);
    }
    this.tabContainer._setPositionalAttributes();
  },

  removeFromMultiSelectedTabs(aTab) {
    if (!aTab.multiselected) {
      return;
    }
    aTab.removeAttribute("multiselected");
    this.tabContainer._setPositionalAttributes();
    this._multiSelectedTabsSet.delete(aTab);
  },

  clearMultiSelectedTabs(updatePositionalAttributes) {
    for (let tab of this.selectedTabs) {
      tab.removeAttribute("multiselected");
    }
    this._multiSelectedTabsSet = new WeakSet();
    this._lastMultiSelectedTabRef = null;
    if (updatePositionalAttributes) {
      this.tabContainer._setPositionalAttributes();
    }
  },

  /**
   * Remove the active tab from the multiselection if it's the only one left there.
   */
  updateActiveTabMultiSelectState() {
    if (this.selectedTabs.length == 1) {
      this.clearMultiSelectedTabs();
    }
  },

  switchToNextMultiSelectedTab() {
    let lastMultiSelectedTab = gBrowser.lastMultiSelectedTab;
    if (lastMultiSelectedTab != gBrowser.selectedTab) {
      gBrowser.selectedTab = lastMultiSelectedTab;
      return;
    }
    let selectedTabs = ChromeUtils.nondeterministicGetWeakSetKeys(this._multiSelectedTabsSet)
                                  .filter(tab => tab.isConnected && !tab.closing);
    let length = selectedTabs.length;
    gBrowser.selectedTab = selectedTabs[length - 1];
  },

  set selectedTabs(tabs) {
    this.clearMultiSelectedTabs(false);
    this.selectedTab = tabs[0];
    if (tabs.length > 1) {
      for (let tab of tabs) {
        this.addToMultiSelectedTabs(tab, true);
      }
    }
    this.tabContainer._setPositionalAttributes();
  },

  get selectedTabs() {
    let {selectedTab, _multiSelectedTabsSet} = this;
    let tabs = ChromeUtils.nondeterministicGetWeakSetKeys(_multiSelectedTabsSet)
      .filter(tab => tab.isConnected && !tab.closing);
    if (!_multiSelectedTabsSet.has(selectedTab)) {
      tabs.push(selectedTab);
    }
    return tabs.sort((a, b) => a._tPos > b._tPos);
  },

  get multiSelectedTabsCount() {
    return ChromeUtils.nondeterministicGetWeakSetKeys(this._multiSelectedTabsSet)
      .filter(tab => tab.isConnected && !tab.closing)
      .length;
  },

  get lastMultiSelectedTab() {
    let tab = this._lastMultiSelectedTabRef ? this._lastMultiSelectedTabRef.get() : null;
    if (tab && tab.isConnected && this._multiSelectedTabsSet.has(tab)) {
      return tab;
    }
    let selectedTab = gBrowser.selectedTab;
    this.lastMultiSelectedTab = selectedTab;
    return selectedTab;
  },

  set lastMultiSelectedTab(aTab) {
    this._lastMultiSelectedTabRef = Cu.getWeakReference(aTab);
  },

  toggleMuteAudioOnMultiSelectedTabs(aTab) {
    let tabsToToggle;
    if (aTab.activeMediaBlocked) {
      tabsToToggle = this.selectedTabs.filter(tab =>
        tab.activeMediaBlocked || tab.linkedBrowser.audioMuted
      );
    } else {
      let tabMuted = aTab.linkedBrowser.audioMuted;
      tabsToToggle = this.selectedTabs.filter(tab =>
        // When a user is looking to mute selected tabs, then media-blocked tabs
        // should not be toggled. Otherwise those media-blocked tabs are going into a
        // playing and unmuted state.
        tab.linkedBrowser.audioMuted == tabMuted && !tab.activeMediaBlocked ||
        tab.activeMediaBlocked && tabMuted
      );
    }
    for (let tab of tabsToToggle) {
      tab.toggleMuteAudio();
    }
  },

  pinMultiSelectedTabs() {
    for (let tab of this.selectedTabs) {
        this.pinTab(tab);
    }
  },

  unpinMultiSelectedTabs() {
    for (let tab of this.selectedTabs) {
        this.unpinTab(tab);
    }
  },

  activateBrowserForPrintPreview(aBrowser) {
    this._printPreviewBrowsers.add(aBrowser);
    if (this._switcher) {
      this._switcher.activateBrowserForPrintPreview(aBrowser);
    }
    aBrowser.docShellIsActive = true;
  },

  deactivatePrintPreviewBrowsers() {
    let browsers = this._printPreviewBrowsers;
    this._printPreviewBrowsers = new Set();
    for (let browser of browsers) {
      browser.docShellIsActive = this.shouldActivateDocShell(browser);
    }
  },

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
  },

  _getSwitcher() {
    if (!this._switcher) {
      this._switcher = new this.AsyncTabSwitcher(this);
    }
    return this._switcher;
  },

  warmupTab(aTab) {
    if (gMultiProcessBrowser) {
      this._getSwitcher().warmupTab(aTab);
    }
  },

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
          aEvent.keyCode == KeyEvent.DOM_VK_F4) {
        if (gBrowser.multiSelectedTabsCount) {
          gBrowser.removeMultiSelectedTabs();
        } else if (!this.selectedTab.pinned) {
          this.removeCurrentTab({ animate: true });
        }
        aEvent.preventDefault();
      }
    }
  },

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
          if (window.getComputedStyle(document.documentElement).direction == "ltr")
            offset *= -1;
          this.tabContainer.advanceSelectedTab(offset, true);
          aEvent.preventDefault();
      }
    }
  },

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
  },

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
          this.selectedBrowser.preserveLayers(
            window.windowState == window.STATE_MINIMIZED || window.isFullyOccluded);
          this.selectedBrowser.docShellIsActive = this.shouldActivateDocShell(this.selectedBrowser);
        }
        break;
    }
  },

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
        if (!this.isFindBarInitialized(tab)) {
          let fakeEvent = data;
          this.getFindBar(tab).then(findbar => {
            findbar._onBrowserKeypress(fakeEvent);
          });
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
  },

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
    }
  },

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
  },

  destroy() {
    Services.obs.removeObserver(this, "contextual-identity-updated");

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

    Services.els.removeSystemEventListener(document, "keydown", this, false);
    if (AppConstants.platform == "macosx") {
      Services.els.removeSystemEventListener(document, "keypress", this, false);
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
  },

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
        TabCrashHandler.onSelectedBrowserCrash(browser, false);
      } else {
        this.updateBrowserRemoteness(browser, false);
        SessionStore.reviveCrashedTab(tab);
      }

      tab.removeAttribute("soundplaying");
      this.setIcon(tab, icon);
    });

    this.addEventListener("oop-browser-buildid-mismatch", (event) => {
      if (!event.isTrusted)
        return;

      let browser = event.originalTarget;

      if (this.selectedBrowser == browser) {
        TabCrashHandler.onSelectedBrowserCrash(browser, true);
      }
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
  },
};

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
    if (aStateFlags & Ci.nsIWebProgressListener.STATE_STOP &&
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

    if (this.mTotalProgress && this.mTab.hasAttribute("busy")) {
      this.mTab.setAttribute("progress", "true");
      gBrowser._tabAttrModified(this.mTab, ["progress"]);
    }

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

    let location, originalLocation;
    try {
      aRequest.QueryInterface(Ci.nsIChannel);
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
         aStateFlags & Ci.nsIWebProgressListener.STATE_STOP &&
         aStateFlags & Ci.nsIWebProgressListener.STATE_IS_NETWORK) ||
        !ignoreBlank && this.mBlank) {
      this.mBlank = false;
    }

    if (aStateFlags & Ci.nsIWebProgressListener.STATE_START) {
      this.mRequestCount++;
    } else if (aStateFlags & Ci.nsIWebProgressListener.STATE_STOP) {
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

    if (aStateFlags & Ci.nsIWebProgressListener.STATE_START &&
        aStateFlags & Ci.nsIWebProgressListener.STATE_IS_NETWORK) {
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
        if (!(aStateFlags & Ci.nsIWebProgressListener.STATE_RESTORING) &&
            aWebProgress && aWebProgress.isTopLevel) {
          this.mTab.setAttribute("busy", "true");
          gBrowser._tabAttrModified(this.mTab, ["busy"]);
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
          gBrowser._isBusy = true;
        }
      }
    } else if (aStateFlags & Ci.nsIWebProgressListener.STATE_STOP &&
               aStateFlags & Ci.nsIWebProgressListener.STATE_IS_NETWORK) {

      let modifiedAttrs = [];
      if (this.mTab.hasAttribute("busy")) {
        this.mTab.removeAttribute("busy");
        modifiedAttrs.push("busy");
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
      }

      if (this.mTab.hasAttribute("progress")) {
        this.mTab.removeAttribute("progress");
        modifiedAttrs.push("progress");
      }

      if (modifiedAttrs.length) {
        gBrowser._tabAttrModified(this.mTab, modifiedAttrs);
      }

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
      }

      // If we don't already have an icon for this tab then clear the tab's
      // icon. Don't do this on the initial about:blank load to prevent
      // flickering. Don't clear the icon if we already set it from one of the
      // known defaults. Note we use the original URL since about:newtab
      // redirects to a prerendered page.
      if (!this.mBrowser.mIconURL && !ignoreBlank &&
          !(originalLocation.spec in FAVICON_DEFAULTS)) {
        this.mTab.removeAttribute("image");
      }

      // For keyword URIs clear the user typed value since they will be changed into real URIs
      if (location.scheme == "keyword")
        this.mBrowser.userTypedValue = null;

      if (this.mTab.selected)
        gBrowser._isBusy = false;
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

    if (aStateFlags &
        (Ci.nsIWebProgressListener.STATE_START |
         Ci.nsIWebProgressListener.STATE_STOP)) {
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
      // Finally, we do insert the URL if this is a same-document navigation
      // and the user cleared the URL manually.
      if (this.mBrowser.didStartLoadSinceLastUserTyping() ||
          ((aFlags & Ci.nsIWebProgressListener.LOCATION_CHANGE_ERROR_PAGE) &&
            aLocation.spec != "about:blank") ||
          (isSameDocument && this.mBrowser.inLoadURI) ||
          (isSameDocument && !this.mBrowser.userTypedValue)) {
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
        let findBar = gBrowser.getCachedFindBar(this.mTab);

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
        // Removing the tab's image here causes flickering, wait until the load
        // is complete.
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

      if (this.mTab != gBrowser.selectedTab) {
        let tabCacheIndex = gBrowser._tabLayerCache.indexOf(this.mTab);
        if (tabCacheIndex != -1) {
          gBrowser._tabLayerCache.splice(tabCacheIndex, 1);
          gBrowser._getSwitcher().cleanUpTabAfterEviction(this.mTab);
        }
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
}
TabProgressListener.prototype.QueryInterface = ChromeUtils.generateQI(
  ["nsIWebProgressListener",
   "nsIWebProgressListener2",
   "nsISupportsWeakReference"]);

var StatusPanel = {
  get panel() {
    window.addEventListener("resize", this);

    delete this.panel;
    return this.panel = document.getElementById("statuspanel");
  },

  get isVisible() {
    return !this.panel.hasAttribute("inactive");
  },

  update() {
    let text;
    let type;
    let types = ["overLink"];
    if (XULBrowserWindow.busyUI) {
      types.push("status");
    }
    types.push("defaultStatus");
    for (type of types) {
      if ((text = XULBrowserWindow[type])) {
        break;
      }
    }

    if (this._labelElement.value != text ||
        (text && !this.isVisible)) {
      this.panel.setAttribute("previoustype", this.panel.getAttribute("type"));
      this.panel.setAttribute("type", type);
      this._label = text;
      this._labelElement.setAttribute("crop", type == "overLink" ? "center" : "end");
    }
  },

  get _labelElement() {
    delete this._labelElement;
    return this._labelElement = document.getElementById("statuspanel-label");
  },

  set _label(val) {
    if (!this.isVisible) {
      this.panel.removeAttribute("mirror");
      this.panel.removeAttribute("sizelimit");
    }

    if (this.panel.getAttribute("type") == "status" &&
        this.panel.getAttribute("previoustype") == "status") {
      // Before updating the label, set the panel's current width as its
      // min-width to let the panel grow but not shrink and prevent
      // unnecessary flicker while loading pages. We only care about the
      // panel's width once it has been painted, so we can do this
      // without flushing layout.
      this.panel.style.minWidth =
        window.QueryInterface(Ci.nsIInterfaceRequestor)
              .getInterface(Ci.nsIDOMWindowUtils)
              .getBoundsWithoutFlushing(this.panel).width + "px";
    } else {
      this.panel.style.minWidth = "";
    }

    if (val) {
      this._mouseTargetRect = null;
      this._labelElement.value = val;
      MousePosTracker.addListener(this);
      // The inactive state for the panel will be removed in onTrackingStarted,
      // once the initial position of the mouse relative to the StatusPanel
      // is figured out (to avoid both flicker and sync flushing).
    } else {
      this.panel.setAttribute("inactive", "true");
      MousePosTracker.removeListener(this);
    }

    return val;
  },

  onTrackingStarted() {
    this.panel.removeAttribute("inactive");
  },

  getMouseTargetRect() {
    if (!this._mouseTargetRect) {
      this._calcMouseTargetRect();
    }
    return this._mouseTargetRect;
  },

  onMouseEnter() {
    this._mirror();
  },

  onMouseLeave() {
    this._mirror();
  },

  handleEvent(event) {
    if (!this.isVisible) {
      return;
    }
    switch (event.type) {
      case "resize":
        this._mouseTargetRect = null;
        break;
    }
  },

  _calcMouseTargetRect() {
    let container = this.panel.parentNode;
    let alignRight = (getComputedStyle(container).direction == "rtl");
    let panelRect = this.panel.getBoundingClientRect();
    let containerRect = container.getBoundingClientRect();

    this._mouseTargetRect = {
      top:    panelRect.top,
      bottom: panelRect.bottom,
      left:   alignRight ? containerRect.right - panelRect.width : containerRect.left,
      right:  alignRight ? containerRect.right : containerRect.left + panelRect.width
    };
  },

  _mirror() {
    if (this.panel.hasAttribute("mirror")) {
      this.panel.removeAttribute("mirror");
    } else {
      this.panel.setAttribute("mirror", "true");
    }

    if (!this.panel.hasAttribute("sizelimit")) {
      this.panel.setAttribute("sizelimit", "true");
      this._mouseTargetRect = null;
    }
  }
};

var TabBarVisibility = {
  _initialUpdateDone: false,

  update() {
    let toolbar = document.getElementById("TabsToolbar");
    let collapse = false;
    if (!gBrowser /* gBrowser isn't initialized yet */ ||
        gBrowser.tabs.length - gBrowser._removingTabs.length == 1) {
      collapse = !window.toolbar.visible;
    }

    if (collapse == toolbar.collapsed && this._initialUpdateDone) {
      return;
    }
    this._initialUpdateDone = true;

    toolbar.collapsed = collapse;

    document.getElementById("menu_closeWindow").hidden = collapse;
    document.getElementById("menu_close").setAttribute("label",
      gTabBrowserBundle.GetStringFromName(collapse ? "tabs.close" : "tabs.closeTab"));

    TabsInTitlebar.allowedBy("tabs-visible", !collapse);
  }
};

var TabContextMenu = {
  contextTab: null,
  _updateToggleMuteMenuItems(aTab, aConditionFn) {
    ["muted", "soundplaying"].forEach(attr => {
      if (!aConditionFn || aConditionFn(attr)) {
        if (aTab.hasAttribute(attr)) {
          aTab.toggleMuteMenuItem.setAttribute(attr, "true");
          aTab.toggleMultiSelectMuteMenuItem.setAttribute(attr, "true");
        } else {
          aTab.toggleMuteMenuItem.removeAttribute(attr);
          aTab.toggleMultiSelectMuteMenuItem.removeAttribute(attr);
        }
      }
    });
  },
  updateContextMenu(aPopupMenu) {
    this.contextTab = aPopupMenu.triggerNode.localName == "tab" ?
                      aPopupMenu.triggerNode : gBrowser.selectedTab;
    let disabled = gBrowser.tabs.length == 1;
    let multiselectionContext = this.contextTab.multiselected;

    var menuItems = aPopupMenu.getElementsByAttribute("tbattr", "tabbrowser-multiple");
    for (let menuItem of menuItems) {
      menuItem.disabled = disabled;
    }

    if (this.contextTab.hasAttribute("customizemode")) {
      document.getElementById("context_openTabInWindow").disabled = true;
    }

    disabled = gBrowser.visibleTabs.length == 1;
    menuItems = aPopupMenu.getElementsByAttribute("tbattr", "tabbrowser-multiple-visible");
    for (let menuItem of menuItems) {
      menuItem.disabled = disabled;
    }

    // Session store
    document.getElementById("context_undoCloseTab").disabled =
      SessionStore.getClosedTabCount(window) == 0;

    // Only one of Reload_Tab/Reload_Selected_Tabs should be visible.
    document.getElementById("context_reloadTab").hidden = multiselectionContext;
    document.getElementById("context_reloadSelectedTabs").hidden = !multiselectionContext;

    // Only one of pin/unpin/multiselect-pin/multiselect-unpin should be visible
    let contextPinTab = document.getElementById("context_pinTab");
    contextPinTab.hidden = this.contextTab.pinned || multiselectionContext;
    let contextUnpinTab = document.getElementById("context_unpinTab");
    contextUnpinTab.hidden = !this.contextTab.pinned || multiselectionContext;
    let contextPinSelectedTabs = document.getElementById("context_pinSelectedTabs");
    contextPinSelectedTabs.hidden = this.contextTab.pinned || !multiselectionContext;
    let contextUnpinSelectedTabs = document.getElementById("context_unpinSelectedTabs");
    contextUnpinSelectedTabs.hidden = !this.contextTab.pinned || !multiselectionContext;

    // Disable "Close Tabs to the Right" if there are no tabs
    // following it.
    document.getElementById("context_closeTabsToTheEnd").disabled =
      gBrowser.getTabsToTheEndFrom(this.contextTab).length == 0;

    // Disable "Close other Tabs" if there are no unpinned tabs.
    let unpinnedTabsToClose = gBrowser.visibleTabs.length - gBrowser._numPinnedTabs;
    if (!this.contextTab.pinned) {
      unpinnedTabsToClose--;
    }
    document.getElementById("context_closeOtherTabs").disabled = unpinnedTabsToClose < 1;

    // Only one of close_tab/close_selected_tabs should be visible
    document.getElementById("context_closeTab").hidden = multiselectionContext;
    document.getElementById("context_closeSelectedTabs").hidden = !multiselectionContext;

    // Hide "Bookmark All Tabs" for a pinned tab or multiselection.
    // Update its state if visible.
    let bookmarkAllTabs = document.getElementById("context_bookmarkAllTabs");
    bookmarkAllTabs.hidden = this.contextTab.pinned || multiselectionContext;
    if (!bookmarkAllTabs.hidden) {
      PlacesCommandHook.updateBookmarkAllTabsCommand();
    }

    // Show "Bookmark Selected Tabs" in a multiselect context and hide it otherwise.
    let bookmarkMultiSelectedTabs = document.getElementById("context_bookmarkSelectedTabs");
    bookmarkMultiSelectedTabs.hidden = !multiselectionContext;

    let toggleMute = document.getElementById("context_toggleMuteTab");
    let toggleMultiSelectMute = document.getElementById("context_toggleMuteSelectedTabs");

    // Only one of mute_unmute_tab/mute_unmute_selected_tabs should be visible
    toggleMute.hidden = multiselectionContext;
    toggleMultiSelectMute.hidden = !multiselectionContext;

    // Adjust the state of the toggle mute menu item.
    if (this.contextTab.hasAttribute("activemedia-blocked")) {
      toggleMute.label = gNavigatorBundle.getString("playTab.label");
      toggleMute.accessKey = gNavigatorBundle.getString("playTab.accesskey");
    } else if (this.contextTab.hasAttribute("muted")) {
      toggleMute.label = gNavigatorBundle.getString("unmuteTab.label");
      toggleMute.accessKey = gNavigatorBundle.getString("unmuteTab.accesskey");
    } else {
      toggleMute.label = gNavigatorBundle.getString("muteTab.label");
      toggleMute.accessKey = gNavigatorBundle.getString("muteTab.accesskey");
    }

    // Adjust the state of the toggle mute menu item for multi-selected tabs.
    if (this.contextTab.hasAttribute("activemedia-blocked")) {
      toggleMultiSelectMute.label = gNavigatorBundle.getString("playTabs.label");
      toggleMultiSelectMute.accessKey = gNavigatorBundle.getString("playTabs.accesskey");
    } else if (this.contextTab.hasAttribute("muted")) {
      toggleMultiSelectMute.label = gNavigatorBundle.getString("unmuteSelectedTabs.label");
      toggleMultiSelectMute.accessKey = gNavigatorBundle.getString("unmuteSelectedTabs.accesskey");
    } else {
      toggleMultiSelectMute.label = gNavigatorBundle.getString("muteSelectedTabs.label");
      toggleMultiSelectMute.accessKey = gNavigatorBundle.getString("muteSelectedTabs.accesskey");
    }

    this.contextTab.toggleMuteMenuItem = toggleMute;
    this.contextTab.toggleMultiSelectMuteMenuItem = toggleMultiSelectMute;
    this._updateToggleMuteMenuItems(this.contextTab);

    this.contextTab.addEventListener("TabAttrModified", this);
    aPopupMenu.addEventListener("popuphiding", this);

    gSync.updateTabContextMenu(aPopupMenu, this.contextTab);

    document.getElementById("context_reopenInContainer").hidden =
      !Services.prefs.getBoolPref("privacy.userContext.enabled", false) ||
      PrivateBrowsingUtils.isWindowPrivate(window);
  },
  handleEvent(aEvent) {
    switch (aEvent.type) {
      case "popuphiding":
        gBrowser.removeEventListener("TabAttrModified", this);
        aEvent.target.removeEventListener("popuphiding", this);
        break;
      case "TabAttrModified":
        let tab = aEvent.target;
        this._updateToggleMuteMenuItems(tab,
          attr => aEvent.detail.changed.includes(attr));
        break;
    }
  },
  createReopenInContainerMenu(event) {
    createUserContextMenu(event, {
      isContextMenu: true,
      excludeUserContextId: this.contextTab.getAttribute("usercontextid"),
    });
  },
  reopenInContainer(event) {
    let newTab = gBrowser.addTab(this.contextTab.linkedBrowser.currentURI.spec, {
      userContextId: parseInt(event.target.getAttribute("data-usercontextid")),
      pinned: this.contextTab.pinned,
      index: this.contextTab._tPos + 1,
    });

    if (gBrowser.selectedTab == this.contextTab) {
      gBrowser.selectedTab = newTab;
    }
    if (this.contextTab.muted) {
      if (!newTab.muted) {
        newTab.toggleMuteAudio(this.contextTab.muteReason);
      }
    }
  }
};

