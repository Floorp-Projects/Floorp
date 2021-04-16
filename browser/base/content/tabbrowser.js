/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

{
  // start private scope for gBrowser
  /**
   * A set of known icons to use for internal pages. These are hardcoded so we can
   * start loading them faster than ContentLinkHandler would normally find them.
   */
  const FAVICON_DEFAULTS = {
    "about:newtab": "chrome://branding/content/icon32.png",
    "about:home": "chrome://branding/content/icon32.png",
    "about:welcome": "chrome://branding/content/icon32.png",
    "about:privatebrowsing":
      "chrome://browser/skin/privatebrowsing/favicon.svg",
  };

  window._gBrowser = {
    init() {
      ChromeUtils.defineModuleGetter(
        this,
        "AsyncTabSwitcher",
        "resource:///modules/AsyncTabSwitcher.jsm"
      );
      ChromeUtils.defineModuleGetter(
        this,
        "UrlbarProviderOpenTabs",
        "resource:///modules/UrlbarProviderOpenTabs.jsm"
      );

      if (AppConstants.MOZ_CRASHREPORTER) {
        ChromeUtils.defineModuleGetter(
          this,
          "TabCrashHandler",
          "resource:///modules/ContentCrashHandlers.jsm"
        );
      }

      Services.obs.addObserver(this, "contextual-identity-updated");

      Services.els.addSystemEventListener(document, "keydown", this, false);
      Services.els.addSystemEventListener(document, "keypress", this, false);
      window.addEventListener("sizemodechange", this);
      window.addEventListener("occlusionstatechange", this);
      window.addEventListener("framefocusrequested", this);

      this.tabContainer.init();
      this._setupInitialBrowserAndTab();

      if (Services.prefs.getBoolPref("browser.display.use_system_colors")) {
        this.tabpanels.style.backgroundColor = "-moz-default-background-color";
      } else if (
        Services.prefs.getIntPref("browser.display.document_color_use") == 2
      ) {
        this.tabpanels.style.backgroundColor = Services.prefs.getCharPref(
          "browser.display.background_color"
        );
      }

      this._setFindbarData();

      XPCOMUtils.defineLazyModuleGetters(this, {
        E10SUtils: "resource://gre/modules/E10SUtils.jsm",
      });
      XPCOMUtils.defineLazyServiceGetters(this, {
        MacSharingService: [
          "@mozilla.org/widget/macsharingservice;1",
          "nsIMacSharingService",
        ],
      });

      let tabTooltip = document.getElementById("tabbrowser-tab-tooltip");
      if (gProtonPlacesTooltip) {
        tabTooltip.setAttribute("position", "after_start");
        tabTooltip.setAttribute("anchortoclosest", "tab");
      }

      // We take over setting the document title, so remove the l10n id to
      // avoid it being re-translated and overwriting document content if
      // we ever switch languages at runtime. After a language change, the
      // window title will update at the next tab or location change.
      document.querySelector("title").removeAttribute("data-l10n-id");

      this._setupEventListeners();
      this._initialized = true;
    },

    ownerGlobal: window,

    ownerDocument: document,

    closingTabsEnum: {
      ALL: 0,
      OTHER: 1,
      TO_START: 2,
      TO_END: 3,
      MULTI_SELECTED: 4,
    },

    _visibleTabs: null,

    _tabs: null,

    _lastRelatedTabMap: new WeakMap(),

    mProgressListeners: [],

    mTabsProgressListeners: [],

    _tabListeners: new Map(),

    _tabFilters: new Map(),

    _isBusy: false,

    arrowKeysShouldWrap: AppConstants == "macosx",

    _dateTimePicker: null,

    _previewMode: false,

    _lastFindValue: "",

    _contentWaitingCount: 0,

    _tabLayerCache: [],

    tabAnimationsInProgress: 0,

    /**
     * Binding from browser to tab
     */
    _tabForBrowser: new WeakMap(),

    /**
     * `_createLazyBrowser` will define properties on the unbound lazy browser
     * which correspond to properties defined in MozBrowser which will be bound to
     * the browser when it is inserted into the document.  If any of these
     * properties are accessed by consumers, `_insertBrowser` is called and
     * the browser is inserted to ensure that things don't break.  This list
     * provides the names of properties that may be called while the browser
     * is in its unbound (lazy) state.
     */
    _browserBindingProperties: [
      "canGoBack",
      "canGoForward",
      "goBack",
      "goForward",
      "permitUnload",
      "reload",
      "reloadWithFlags",
      "stop",
      "loadURI",
      "gotoIndex",
      "currentURI",
      "documentURI",
      "remoteType",
      "preferences",
      "imageDocument",
      "isRemoteBrowser",
      "messageManager",
      "getTabBrowser",
      "finder",
      "fastFind",
      "sessionHistory",
      "contentTitle",
      "characterSet",
      "fullZoom",
      "textZoom",
      "tabHasCustomZoom",
      "webProgress",
      "addProgressListener",
      "removeProgressListener",
      "audioPlaybackStarted",
      "audioPlaybackStopped",
      "resumeMedia",
      "mute",
      "unmute",
      "blockedPopups",
      "lastURI",
      "purgeSessionHistory",
      "stopScroll",
      "startScroll",
      "userTypedValue",
      "userTypedClear",
      "didStartLoadSinceLastUserTyping",
      "audioMuted",
    ],

    _removingTabs: [],

    _multiSelectedTabsSet: new WeakSet(),

    _lastMultiSelectedTabRef: null,

    _clearMultiSelectionLocked: false,

    _clearMultiSelectionLockedOnce: false,

    _multiSelectChangeStarted: false,

    _multiSelectChangeAdditions: new Set(),

    _multiSelectChangeRemovals: new Set(),

    _multiSelectChangeSelected: false,

    /**
     * Tab close requests are ignored if the window is closing anyway,
     * e.g. when holding Ctrl+W.
     */
    _windowIsClosing: false,

    preloadedBrowser: null,

    /**
     * This defines a proxy which allows us to access browsers by
     * index without actually creating a full array of browsers.
     */
    browsers: new Proxy([], {
      has: (target, name) => {
        if (typeof name == "string" && Number.isInteger(parseInt(name))) {
          return name in gBrowser.tabs;
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
      },
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
      return (this.tabContainer = document.getElementById("tabbrowser-tabs"));
    },

    get tabs() {
      if (!this._tabs) {
        this._tabs = this.tabContainer.allTabs;
      }
      return this._tabs;
    },

    get tabbox() {
      delete this.tabbox;
      return (this.tabbox = document.getElementById("tabbrowser-tabbox"));
    },

    get tabpanels() {
      delete this.tabpanels;
      return (this.tabpanels = document.getElementById("tabbrowser-tabpanels"));
    },

    addEventListener(...args) {
      this.tabpanels.addEventListener(...args);
    },

    removeEventListener(...args) {
      this.tabpanels.removeEventListener(...args);
    },

    dispatchEvent(...args) {
      return this.tabpanels.dispatchEvent(...args);
    },

    get visibleTabs() {
      if (!this._visibleTabs) {
        this._visibleTabs = Array.prototype.filter.call(
          this.tabs,
          tab => !tab.hidden && !tab.closing
        );
      }
      return this._visibleTabs;
    },

    get _numPinnedTabs() {
      for (var i = 0; i < this.tabs.length; i++) {
        if (!this.tabs[i].pinned) {
          break;
        }
      }
      return i;
    },

    set selectedTab(val) {
      if (
        gSharedTabWarning.willShowSharedTabWarning(val) ||
        document.documentElement.hasAttribute("window-modal-open") ||
        (gNavToolbox.collapsed && !this._allowTabChange)
      ) {
        return;
      }
      // Update the tab
      this.tabbox.selectedTab = val;
    },

    get selectedTab() {
      return this._selectedTab;
    },

    get selectedBrowser() {
      return this._selectedBrowser;
    },

    _setupInitialBrowserAndTab() {
      // See browser.js for the meaning of window.arguments.
      // Bug 1485961 covers making this more sane.
      let userContextId = window.arguments && window.arguments[5];

      let openWindowInfo = window.docShell.treeOwner
        .QueryInterface(Ci.nsIInterfaceRequestor)
        .getInterface(Ci.nsIAppWindow).initialOpenWindowInfo;

      if (!openWindowInfo && window.arguments && window.arguments[11]) {
        openWindowInfo = window.arguments[11];
      }

      let tabArgument = gBrowserInit.getTabToAdopt();

      // If we have a tab argument with browser, we use its remoteType. Otherwise,
      // if e10s is disabled or there's a parent process opener (e.g. parent
      // process about: page) for the content tab, we use a parent
      // process remoteType. Otherwise, we check the URI to determine
      // what to do - if there isn't one, we default to the default remote type.
      //
      // When adopting a tab, we'll also use that tab's browsingContextGroupId,
      // if available, to ensure we don't spawn a new process.
      let remoteType;
      let initialBrowsingContextGroupId;

      if (tabArgument && tabArgument.hasAttribute("usercontextid")) {
        // The window's first argument is a tab if and only if we are swapping tabs.
        // We must set the browser's usercontextid so that the newly created remote
        // tab child has the correct usercontextid.
        userContextId = parseInt(tabArgument.getAttribute("usercontextid"), 10);
      }

      if (tabArgument && tabArgument.linkedBrowser) {
        remoteType = tabArgument.linkedBrowser.remoteType;
        initialBrowsingContextGroupId =
          tabArgument.linkedBrowser.browsingContext?.group.id;
      } else if (openWindowInfo) {
        userContextId = openWindowInfo.originAttributes.userContextId;
        if (openWindowInfo.isRemote) {
          remoteType = E10SUtils.DEFAULT_REMOTE_TYPE;
        } else {
          remoteType = E10SUtils.NOT_REMOTE;
        }
      } else {
        let uriToLoad = gBrowserInit.uriToLoadPromise;
        if (uriToLoad && Array.isArray(uriToLoad)) {
          uriToLoad = uriToLoad[0]; // we only care about the first item
        }

        if (uriToLoad && typeof uriToLoad == "string") {
          let oa = E10SUtils.predictOriginAttributes({
            window,
            userContextId,
          });
          remoteType = E10SUtils.getRemoteTypeForURI(
            uriToLoad,
            gMultiProcessBrowser,
            gFissionBrowser,
            E10SUtils.DEFAULT_REMOTE_TYPE,
            null,
            oa
          );
        } else {
          remoteType = E10SUtils.DEFAULT_REMOTE_TYPE;
        }
      }

      let createOptions = {
        uriIsAboutBlank: false,
        userContextId,
        initialBrowsingContextGroupId,
        remoteType,
        openWindowInfo,
      };
      let browser = this.createBrowser(createOptions);
      browser.setAttribute("primary", "true");
      if (gBrowserAllowScriptsToCloseInitialTabs) {
        browser.setAttribute("allowscriptstoclose", "true");
      }
      browser.droppedLinkHandler = handleDroppedLink;
      browser.loadURI = _loadURI.bind(null, browser);

      let uniqueId = this._generateUniquePanelID();
      let panel = this.getPanel(browser);
      panel.id = uniqueId;
      this.tabpanels.appendChild(panel);

      let tab = this.tabs[0];
      tab.linkedPanel = uniqueId;
      this._selectedTab = tab;
      this._selectedBrowser = browser;
      tab.permanentKey = browser.permanentKey;
      tab._tPos = 0;
      tab._fullyOpen = true;
      tab.linkedBrowser = browser;

      if (userContextId) {
        tab.setAttribute("usercontextid", userContextId);
        ContextualIdentityService.setTabStyle(tab);
      }

      this._tabForBrowser.set(browser, tab);

      this._appendStatusPanel();

      // This is the initial browser, so it's usually active; the default is false
      // so we have to update it:
      browser.docShellIsActive = this.shouldActivateDocShell(browser);

      // Hook the browser up with a progress listener.
      let tabListener = new TabProgressListener(tab, browser, true, false);
      let filter = Cc[
        "@mozilla.org/appshell/component/browser-status-filter;1"
      ].createInstance(Ci.nsIWebProgress);
      filter.addProgressListener(tabListener, Ci.nsIWebProgress.NOTIFY_ALL);
      this._tabListeners.set(tab, tabListener);
      this._tabFilters.set(tab, filter);
      browser.webProgress.addProgressListener(
        filter,
        Ci.nsIWebProgress.NOTIFY_ALL
      );
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

    goBack(requireUserInteraction) {
      return this.selectedBrowser.goBack(requireUserInteraction);
    },

    goForward(requireUserInteraction) {
      return this.selectedBrowser.goForward(requireUserInteraction);
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

    get sessionHistory() {
      return this.selectedBrowser.sessionHistory;
    },

    get markupDocumentViewer() {
      return this.selectedBrowser.markupDocumentViewer;
    },

    get contentDocument() {
      return this.selectedBrowser.contentDocument;
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
      this.selectedBrowser.userTypedValue = val;
    },

    get userTypedValue() {
      return this.selectedBrowser.userTypedValue;
    },

    _invalidateCachedTabs() {
      this._tabs = null;
      this._visibleTabs = null;
    },

    _setFindbarData() {
      // Ensure we know what the find bar key is in the content process:
      let { sharedData } = Services.ppmm;
      if (!sharedData.has("Findbar:Shortcut")) {
        let keyEl = document.getElementById("key_find");
        let mods = keyEl
          .getAttribute("modifiers")
          .replace(
            /accel/i,
            AppConstants.platform == "macosx" ? "meta" : "control"
          );
        sharedData.set("Findbar:Shortcut", {
          key: keyEl.getAttribute("key"),
          shiftKey: mods.includes("shift"),
          ctrlKey: mods.includes("control"),
          altKey: mods.includes("alt"),
          metaKey: mods.includes("meta"),
        });
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
      let findBar = document.createXULElement("findbar");
      let browser = this.getBrowserForTab(aTab);

      // The findbar should be inserted after the browserStack and, if present for
      // this tab, after the StatusPanel as well.
      let insertAfterElement = browser.parentNode;
      if (insertAfterElement.nextElementSibling == StatusPanel.panel) {
        insertAfterElement = StatusPanel.panel;
      }
      insertAfterElement.insertAdjacentElement("afterend", findBar);

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
      this.selectedBrowser.parentNode.insertAdjacentElement(
        "afterend",
        StatusPanel.panel
      );
    },

    _updateTabBarForPinnedTabs() {
      this.tabContainer._unlockTabSizing();
      this.tabContainer._positionPinnedTabs();
      this.tabContainer._setPositionalAttributes();
      this.tabContainer._updateCloseButtons();
    },

    _notifyPinnedStatus(aTab) {
      aTab.linkedBrowser.sendMessageToActor(
        "Browser:AppTab",
        { isAppTab: aTab.pinned },
        "BrowserTab"
      );

      let event = document.createEvent("Events");
      event.initEvent(aTab.pinned ? "TabPinned" : "TabUnpinned", true, false);
      aTab.dispatchEvent(event);
    },

    pinTab(aTab) {
      if (aTab.pinned) {
        return;
      }

      if (aTab.hidden) {
        this.showTab(aTab);
      }

      this.moveTabTo(aTab, this._numPinnedTabs);
      aTab.setAttribute("pinned", "true");
      this._updateTabBarForPinnedTabs();
      this._notifyPinnedStatus(aTab);
    },

    unpinTab(aTab) {
      if (!aTab.pinned) {
        return;
      }

      this.moveTabTo(aTab, this._numPinnedTabs - 1);
      aTab.removeAttribute("pinned");
      aTab.style.marginInlineStart = "";
      aTab._pinnedUnscrollable = false;
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

    _getAndMaybeCreateDateTimePickerPanel() {
      if (!this._dateTimePicker) {
        let wrapper = document.getElementById("dateTimePickerTemplate");
        wrapper.replaceWith(wrapper.content);
        this._dateTimePicker = document.getElementById("DateTimePickerPanel");
      }

      return this._dateTimePicker;
    },

    syncThrobberAnimations(aTab) {
      aTab.ownerGlobal.promiseDocumentFlushed(() => {
        if (!aTab.container) {
          return;
        }

        const animations = Array.from(
          aTab.container.getElementsByTagName("tab")
        )
          .map(tab => {
            const throbber = tab.throbber;
            return throbber ? throbber.getAnimations({ subtree: true }) : [];
          })
          .reduce((a, b) => a.concat(b))
          .filter(
            anim =>
              anim instanceof CSSAnimation &&
              (anim.animationName === "tab-throbber-animation" ||
                anim.animationName === "tab-throbber-animation-rtl") &&
              anim.playState === "running"
          );

        // Synchronize with the oldest running animation, if any.
        const firstStartTime = Math.min(
          ...animations.map(anim =>
            anim.startTime === null ? Infinity : anim.startTime
          )
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

    getBrowserForOuterWindowID(aID) {
      for (let b of this.browsers) {
        if (b.outerWindowID == aID) {
          return b;
        }
      }

      return null;
    },

    getTabForBrowser(aBrowser) {
      return this._tabForBrowser.get(aBrowser);
    },

    getPanel(aBrowser) {
      return this.getBrowserContainer(aBrowser).parentNode;
    },

    getBrowserContainer(aBrowser) {
      return (aBrowser || this.selectedBrowser).parentNode.parentNode;
    },

    getTabNotificationDeck() {
      if (!this._tabNotificationDeck) {
        let template = document.getElementById(
          "tab-notification-deck-template"
        );
        template.replaceWith(template.content);
        this._tabNotificationDeck = document.getElementById(
          "tab-notification-deck"
        );
      }
      return this._tabNotificationDeck;
    },

    _nextNotificationBoxId: 0,
    getNotificationBox(aBrowser) {
      let browser = aBrowser || this.selectedBrowser;
      if (!browser._notificationBox) {
        browser._notificationBox = new MozElements.NotificationBox(element => {
          element.setAttribute("notificationside", "top");
          if (gProton) {
            element.setAttribute(
              "name",
              `tab-notification-box-${this._nextNotificationBoxId++}`
            );
            // With Proton enabled all notification boxes are at the top, built into the browser chrome.
            this.getTabNotificationDeck().append(element);
            if (browser == this.selectedBrowser) {
              this._updateVisibleNotificationBox(browser);
            }
          } else {
            this.getBrowserContainer(browser).prepend(element);
          }
        });
      }
      return browser._notificationBox;
    },

    readNotificationBox(aBrowser) {
      let browser = aBrowser || this.selectedBrowser;
      return browser._notificationBox || null;
    },

    _updateVisibleNotificationBox(aBrowser) {
      if (!this._tabNotificationDeck) {
        // If the deck hasn't been created we don't need to create it here.
        return;
      }
      let notificationBox = this.readNotificationBox(aBrowser);
      this.getTabNotificationDeck().selectedViewName = notificationBox
        ? notificationBox.stack.getAttribute("name")
        : "";
    },

    getTabModalPromptBox(aBrowser) {
      let browser = aBrowser || this.selectedBrowser;
      if (!browser.tabModalPromptBox) {
        browser.tabModalPromptBox = new TabModalPromptBox(browser);
      }
      return browser.tabModalPromptBox;
    },

    getTabDialogBox(aBrowser) {
      let browser = aBrowser || this.selectedBrowser;
      if (!browser.tabDialogBox) {
        browser.tabDialogBox = new TabDialogBox(browser);
      }
      return browser.tabDialogBox;
    },

    getTabFromAudioEvent(aEvent) {
      if (!aEvent.isTrusted) {
        return null;
      }

      var browser = aEvent.originalTarget;
      var tab = this.getTabForBrowser(browser);
      return tab;
    },

    _callProgressListeners(
      aBrowser,
      aMethod,
      aArguments,
      aCallGlobalListeners = true,
      aCallTabsListeners = true
    ) {
      var rv = true;

      function callListeners(listeners, args) {
        for (let p of listeners) {
          if (aMethod in p) {
            try {
              if (!p[aMethod].apply(p, args)) {
                rv = false;
              }
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
     * Sets an icon for the tab if the URI is defined in FAVICON_DEFAULTS.
     */
    setDefaultIcon(aTab, aURI) {
      if (aURI && aURI.spec in FAVICON_DEFAULTS) {
        this.setIcon(aTab, FAVICON_DEFAULTS[aURI.spec]);
      }
    },

    setIcon(
      aTab,
      aIconURL = "",
      aOriginalURL = aIconURL,
      aLoadingPrincipal = null
    ) {
      let makeString = url => (url instanceof Ci.nsIURI ? url.spec : url);

      aIconURL = makeString(aIconURL);
      aOriginalURL = makeString(aOriginalURL);

      let LOCAL_PROTOCOLS = ["chrome:", "about:", "resource:", "data:"];

      if (
        aIconURL &&
        !aLoadingPrincipal &&
        !LOCAL_PROTOCOLS.some(protocol => aIconURL.startsWith(protocol))
      ) {
        console.error(
          `Attempt to set a remote URL ${aIconURL} as a tab icon without a loading principal.`
        );
        return;
      }

      let browser = this.getBrowserForTab(aTab);
      browser.mIconURL = aIconURL;

      if (aIconURL != aTab.getAttribute("image")) {
        if (aIconURL) {
          if (aLoadingPrincipal) {
            aTab.setAttribute("iconloadingprincipal", aLoadingPrincipal);
          } else {
            aTab.removeAttribute("iconloadingprincipal");
          }
          aTab.setAttribute("image", aIconURL);
        } else {
          aTab.removeAttribute("image");
          aTab.removeAttribute("iconloadingprincipal");
        }
        this._tabAttrModified(aTab, ["image"]);
      }

      // The aOriginalURL argument is currently only used by tests.
      this._callProgressListeners(browser, "onLinkIconAvailable", [
        aIconURL,
        aOriginalURL,
      ]);
    },

    getIcon(aTab) {
      let browser = aTab ? this.getBrowserForTab(aTab) : this.selectedBrowser;
      return browser.mIconURL;
    },

    setPageInfo(aURL, aDescription, aPreviewImage) {
      if (aURL) {
        let pageInfo = {
          url: aURL,
          description: aDescription,
          previewImageURL: aPreviewImage,
        };
        PlacesUtils.history.update(pageInfo).catch(Cu.reportError);
      }
    },

    getWindowTitleForBrowser(aBrowser) {
      let docElement = document.documentElement;
      let title = "";

      // If location bar is hidden and the URL type supports a host,
      // add the scheme and host to the title to prevent spoofing.
      // XXX https://bugzilla.mozilla.org/show_bug.cgi?id=22183#c239
      try {
        if (docElement.getAttribute("chromehidden").includes("location")) {
          const uri = Services.io.createExposableURI(aBrowser.currentURI);
          let prefix = uri.prePath;
          if (uri.scheme == "about") {
            prefix = uri.spec;
          } else if (uri.scheme == "moz-extension") {
            const ext = WebExtensionPolicy.getByHostname(uri.host);
            if (ext && ext.name) {
              let extensionLabel = document.getElementById(
                "urlbar-label-extension"
              );
              prefix = `${extensionLabel.value} (${ext.name})`;
            }
          }
          title = prefix + " - ";
        }
      } catch (e) {
        // ignored
      }

      if (docElement.hasAttribute("titlepreface")) {
        title += docElement.getAttribute("titlepreface");
      }

      let tab = this.getTabForBrowser(aBrowser);
      if (tab._labelIsContentTitle) {
        // Strip out any null bytes in the content title, since the
        // underlying widget implementations of nsWindow::SetTitle pass
        // null-terminated strings to system APIs.
        title += tab.getAttribute("label").replace(/\0/g, "");
      }

      let dataSuffix =
        docElement.getAttribute("privatebrowsingmode") == "temporary"
          ? "Private"
          : "Default";
      if (title) {
        // We're using a function rather than just using `title` as the
        // new substring to avoid `$$`, `$'` etc. having a special
        // meaning to `replace`.
        // See https://developer.mozilla.org/en-US/docs/Web/JavaScript/Reference/Global_Objects/String/replace#specifying_a_string_as_a_parameter
        // and the documentation for functions for more info about this.
        return docElement.dataset["contentTitle" + dataSuffix].replace(
          "CONTENTTITLE",
          () => title
        );
      }

      return docElement.dataset["title" + dataSuffix];
    },

    updateTitlebar() {
      document.title = this.getWindowTitleForBrowser(this.selectedBrowser);
    },

    updateCurrentBrowser(aForceUpdate) {
      let newBrowser = this.getBrowserAtIndex(this.tabContainer.selectedIndex);
      if (this.selectedBrowser == newBrowser && !aForceUpdate) {
        return;
      }

      let newTab = this.getTabForBrowser(newBrowser);

      if (!aForceUpdate) {
        TelemetryStopwatch.start("FX_TAB_SWITCH_UPDATE_MS");

        if (gMultiProcessBrowser) {
          this._getSwitcher().requestTab(newTab);
        }

        document.commandDispatcher.lock();
      }

      let oldTab = this.selectedTab;

      // Preview mode should not reset the owner
      if (!this._previewMode && !oldTab.selected) {
        oldTab.owner = null;
      }

      let lastRelatedTab = this._lastRelatedTabMap.get(oldTab);
      if (lastRelatedTab) {
        if (!lastRelatedTab.selected) {
          lastRelatedTab.owner = null;
        }
      }
      this._lastRelatedTabMap = new WeakMap();

      let oldBrowser = this.selectedBrowser;

      if (!gMultiProcessBrowser) {
        oldBrowser.removeAttribute("primary");
        oldBrowser.docShellIsActive = false;
        newBrowser.setAttribute("primary", "true");
        newBrowser.docShellIsActive =
          window.windowState != window.STATE_MINIMIZED &&
          !window.isFullyOccluded;
      }

      this._selectedBrowser = newBrowser;
      this._selectedTab = newTab;
      this.showTab(newTab);

      this._appendStatusPanel();

      if (gProton) {
        this._updateVisibleNotificationBox(newBrowser);
      }

      let oldBrowserPopupsBlocked = oldBrowser.popupBlocker.getBlockedPopupCount();
      let newBrowserPopupsBlocked = newBrowser.popupBlocker.getBlockedPopupCount();
      if (oldBrowserPopupsBlocked != newBrowserPopupsBlocked) {
        newBrowser.popupBlocker.updateBlockedPopupsUI();
      }

      // Update the URL bar.
      let webProgress = newBrowser.webProgress;
      this._callProgressListeners(
        null,
        "onLocationChange",
        [webProgress, null, newBrowser.currentURI, 0, true],
        true,
        false
      );

      let securityUI = newBrowser.securityUI;
      if (securityUI) {
        this._callProgressListeners(
          null,
          "onSecurityChange",
          [webProgress, null, securityUI.state],
          true,
          false
        );
        // Include the true final argument to indicate that this event is
        // simulated (instead of being observed by the webProgressListener).
        this._callProgressListeners(
          null,
          "onContentBlockingEvent",
          [webProgress, null, newBrowser.getContentBlockingEvents(), true],
          true,
          false
        );
      }

      let listener = this._tabListeners.get(newTab);
      if (listener && listener.mStateFlags) {
        this._callProgressListeners(
          null,
          "onUpdateCurrentBrowser",
          [
            listener.mStateFlags,
            listener.mStatus,
            listener.mMessage,
            listener.mTotalProgress,
          ],
          true,
          false
        );
      }

      if (!this._previewMode) {
        newTab.updateLastAccessed();
        oldTab.updateLastAccessed();

        let oldFindBar = oldTab._findBar;
        if (
          oldFindBar &&
          oldFindBar.findMode == oldFindBar.FIND_NORMAL &&
          !oldFindBar.hidden
        ) {
          this._lastFindValue = oldFindBar._findField.value;
        }

        this.updateTitlebar();

        newTab.removeAttribute("titlechanged");
        newTab.removeAttribute("attention");
        this._tabAttrModified(newTab, ["attention"]);

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
        this._callProgressListeners(
          null,
          "onStateChange",
          [
            webProgress,
            null,
            Ci.nsIWebProgressListener.STATE_START |
              Ci.nsIWebProgressListener.STATE_IS_NETWORK,
            0,
          ],
          true,
          false
        );
      }

      // If the new tab is not busy, and our current state is busy, then
      // we need to fire a stop to all progress listeners.
      if (!newTab.hasAttribute("busy") && this._isBusy) {
        this._isBusy = false;
        this._callProgressListeners(
          null,
          "onStateChange",
          [
            webProgress,
            null,
            Ci.nsIWebProgressListener.STATE_STOP |
              Ci.nsIWebProgressListener.STATE_IS_NETWORK,
            0,
          ],
          true,
          false
        );
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
            previousTab: oldTab,
          },
        });
        newTab.dispatchEvent(event);

        this._tabAttrModified(oldTab, ["selected"]);
        this._tabAttrModified(newTab, ["selected"]);

        this._startMultiSelectChange();
        this._multiSelectChangeSelected = true;
        this.clearMultiSelectedTabs();
        if (this._multiSelectChangeAdditions.size) {
          // Some tab has been multiselected just before switching tabs.
          // The tab that was selected at that point should also be multiselected.
          this.addToMultiSelectedTabs(oldTab);
        }

        if (oldBrowser != newBrowser && oldBrowser.getInPermitUnload) {
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
          gURLBar.afterTabSwitchFocusChange();
        }
      }

      updateUserContextUIIndicator();
      gPermissionPanel.updateSharingIndicator();

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
          cancelable: true,
        });
        this.dispatchEvent(event);
      }

      if (!aForceUpdate) {
        TelemetryStopwatch.finish("FX_TAB_SWITCH_UPDATE_MS");
      }
    },

    _adjustFocusBeforeTabSwitch(oldTab, newTab) {
      if (this._previewMode) {
        return;
      }

      let oldBrowser = oldTab.linkedBrowser;
      let newBrowser = newTab.linkedBrowser;

      oldBrowser._urlbarFocused = gURLBar && gURLBar.focused;

      if (this.isFindBarInitialized(oldTab)) {
        let findBar = this.getCachedFindBar(oldTab);
        oldTab._findBarFocused =
          !findBar.hidden &&
          findBar._findField.getAttribute("focused") == "true";
      }

      let activeEl = document.activeElement;
      // If focus is on the old tab, move it to the new tab.
      if (activeEl == oldTab) {
        newTab.focus();
      } else if (
        gMultiProcessBrowser &&
        activeEl != newBrowser &&
        activeEl != newTab
      ) {
        // In e10s, if focus isn't already in the tabstrip or on the new browser,
        // and the new browser's previous focus wasn't in the url bar but focus is
        // there now, we need to adjust focus further.
        let keepFocusOnUrlBar =
          newBrowser && newBrowser._urlbarFocused && gURLBar && gURLBar.focused;
        if (!keepFocusOnUrlBar) {
          // Clear focus so that _adjustFocusAfterTabSwitch can detect if
          // some element has been focused and respect that.
          document.activeElement.blur();
        }
      }
    },

    _adjustFocusAfterTabSwitch(newTab) {
      // Don't steal focus from the tab bar.
      if (document.activeElement == newTab) {
        return;
      }

      let newBrowser = this.getBrowserForTab(newTab);

      if (newBrowser.hasAttribute("tabDialogShowing")) {
        newBrowser.tabDialogBox.focus();
        return;
      }
      if (newBrowser.hasAttribute("tabmodalPromptShowing")) {
        // If there's a tabmodal prompt showing, focus it.
        let prompts = newBrowser.tabModalPromptBox.listPrompts();
        let prompt = prompts[prompts.length - 1];
        // @tabmodalPromptShowing is also set for other tab modal prompts
        // (e.g. the Payment Request dialog) so there may not be a <tabmodalprompt>.
        // Bug 1492814 will implement this for the Payment Request dialog.
        if (prompt) {
          prompt.Dialog.setDefaultFocus();
          return;
        }
      }

      // Focus the location bar if it was previously focused for that tab.
      // In full screen mode, only bother making the location bar visible
      // if the tab is a blank one.
      if (newBrowser._urlbarFocused && gURLBar) {
        // If the user happened to type into the URL bar for this browser
        // by the time we got here, focusing will cause the text to be
        // selected which could cause them to overwrite what they've
        // already typed in.
        if (gURLBar.focused && newBrowser.userTypedValue) {
          return;
        }

        if (!window.fullScreen || newTab.isEmpty) {
          gURLBar.select();
          return;
        }
      }

      // Focus the find bar if it was previously focused for that tab.
      if (
        gFindBarInitialized &&
        !gFindBar.hidden &&
        this.selectedTab._findBarFocused
      ) {
        gFindBar._findField.focus();
        return;
      }

      // Don't focus the content area if something has been focused after the
      // tab switch was initiated.
      if (gMultiProcessBrowser && document.activeElement != document.body) {
        return;
      }

      // We're now committed to focusing the content area.
      let fm = Services.focus;
      let focusFlags = fm.FLAG_NOSCROLL;

      if (!gMultiProcessBrowser) {
        let newFocusedElement = fm.getFocusedElementForWindow(
          window.content,
          true,
          {}
        );

        // for anchors, use FLAG_SHOWRING so that it is clear what link was
        // last clicked when switching back to that tab
        if (
          newFocusedElement &&
          (newFocusedElement instanceof HTMLAnchorElement ||
            newFocusedElement.getAttributeNS(
              "http://www.w3.org/1999/xlink",
              "type"
            ) == "simple")
        ) {
          focusFlags |= fm.FLAG_SHOWRING;
        }
      }

      fm.setFocus(newBrowser, focusFlags);
    },

    _tabAttrModified(aTab, aChanged) {
      if (aTab.closing) {
        return;
      }

      let event = new CustomEvent("TabAttrModified", {
        bubbles: true,
        cancelable: false,
        detail: {
          changed: aChanged,
        },
      });
      aTab.dispatchEvent(event);
    },

    resetBrowserSharing(aBrowser) {
      let tab = this.getTabForBrowser(aBrowser);
      if (!tab) {
        return;
      }
      // If WebRTC was used, leave object to enable tracking of grace periods.
      tab._sharingState = tab._sharingState?.webRTC ? { webRTC: {} } : {};
      tab.removeAttribute("sharing");
      this._tabAttrModified(tab, ["sharing"]);
      if (aBrowser == this.selectedBrowser) {
        gPermissionPanel.updateSharingIndicator();
      }
    },

    updateBrowserSharing(aBrowser, aState) {
      let tab = this.getTabForBrowser(aBrowser);
      if (!tab) {
        return;
      }
      if (tab._sharingState == null) {
        tab._sharingState = {};
      }
      tab._sharingState = Object.assign(tab._sharingState, aState);

      if ("webRTC" in aState) {
        if (tab._sharingState.webRTC?.sharing) {
          if (tab._sharingState.webRTC.paused) {
            tab.removeAttribute("sharing");
          } else {
            tab.setAttribute("sharing", aState.webRTC.sharing);
          }
        } else {
          tab.removeAttribute("sharing");
        }
        this._tabAttrModified(tab, ["sharing"]);
      }

      if (aBrowser == this.selectedBrowser) {
        gPermissionPanel.updateSharingIndicator();
      }
    },

    getTabSharingState(aTab) {
      // Normalize the state object for consumers (ie.extensions).
      let state = Object.assign(
        {},
        aTab._sharingState && aTab._sharingState.webRTC
      );
      return {
        camera: !!state.camera,
        microphone: !!state.microphone,
        screen: state.screen && state.screen.replace("Paused", ""),
      };
    },

    setInitialTabTitle(aTab, aTitle, aOptions = {}) {
      // Convert some non-content title (actually a url) to human readable title
      if (!aOptions.isContentTitle && isBlankPageURL(aTitle)) {
        aTitle = this.tabContainer.emptyTabTitle;
      }

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

      if (aTab.hasAttribute("customizemode")) {
        let brandBundle = document.getElementById("bundle_brand");
        let brandShortName = brandBundle.getString("brandShortName");
        title = gNavigatorBundle.getFormattedString("customizeMode.tabTitle", [
          brandShortName,
        ]);
      }

      // Don't replace an initially set label with the URL while the tab
      // is loading.
      if (aTab._labelIsInitialTitle) {
        if (!title) {
          return false;
        }
        delete aTab._labelIsInitialTitle;
      }

      let isContentTitle = !!title;
      if (!title) {
        // See if we can use the URI as the title.
        if (browser.currentURI.displaySpec) {
          try {
            title = Services.io.createExposableURI(browser.currentURI)
              .displaySpec;
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
              title = Services.textToSubURI.unEscapeNonAsciiURI(
                characterSet,
                title
              );
            } catch (ex) {
              /* Do nothing. */
            }
          }
        } else {
          // No suitable URI? Fall back to our untitled string.
          title = this.tabContainer.emptyTabTitle;
        }
      }

      return this._setTabLabel(aTab, title, { isContentTitle });
    },

    _setTabLabel(aTab, aLabel, { beforeTabOpen, isContentTitle } = {}) {
      if (!aLabel) {
        return false;
      }

      aTab._fullLabel = aLabel;

      if (!isContentTitle) {
        // Remove protocol and "www."
        if (!("_regex_shortenURLForTabLabel" in this)) {
          this._regex_shortenURLForTabLabel = /^[^:]+:\/\/(?:www\.)?/;
        }
        aLabel = aLabel.replace(this._regex_shortenURLForTabLabel, "");
      }

      aTab._labelIsContentTitle = isContentTitle;

      if (aTab.getAttribute("label") == aLabel) {
        return false;
      }

      let dwu = window.windowUtils;
      let isRTL =
        dwu.getDirectionFromText(aLabel) == Ci.nsIDOMWindowUtils.DIRECTION_RTL;

      aTab.setAttribute("label", aLabel);
      aTab.setAttribute("labeldirection", isRTL ? "rtl" : "ltr");

      // Dispatch TabAttrModified event unless we're setting the label
      // before the TabOpen event was dispatched.
      if (!beforeTabOpen) {
        this._tabAttrModified(aTab, ["label"]);
      }

      if (aTab.selected) {
        this.updateTitlebar();
      }

      return true;
    },

    loadOneTab(
      aURI,
      aReferrerInfoOrParams,
      aCharset,
      aPostData,
      aLoadInBackground,
      aAllowThirdPartyFixup
    ) {
      var aTriggeringPrincipal;
      var aReferrerInfo;
      var aFromExternal;
      var aRelatedToCurrent;
      var aAllowInheritPrincipal;
      var aSkipAnimation;
      var aForceNotRemote;
      var aPreferredRemoteType;
      var aUserContextId;
      var aInitialBrowsingContextGroupId;
      var aOriginPrincipal;
      var aOriginStoragePrincipal;
      var aOpenWindowInfo;
      var aOpenerBrowser;
      var aCreateLazyBrowser;
      var aFocusUrlBar;
      var aName;
      var aCsp;
      var aSkipLoad;
      if (
        arguments.length == 2 &&
        typeof arguments[1] == "object" &&
        !(arguments[1] instanceof Ci.nsIURI)
      ) {
        let params = arguments[1];
        aTriggeringPrincipal = params.triggeringPrincipal;
        aReferrerInfo = params.referrerInfo;
        aCharset = params.charset;
        aPostData = params.postData;
        aLoadInBackground = params.inBackground;
        aAllowThirdPartyFixup = params.allowThirdPartyFixup;
        aFromExternal = params.fromExternal;
        aRelatedToCurrent = params.relatedToCurrent;
        aAllowInheritPrincipal = !!params.allowInheritPrincipal;
        aSkipAnimation = params.skipAnimation;
        aForceNotRemote = params.forceNotRemote;
        aPreferredRemoteType = params.preferredRemoteType;
        aUserContextId = params.userContextId;
        aInitialBrowsingContextGroupId = params.initialBrowsingContextGroupId;
        aOriginPrincipal = params.originPrincipal;
        aOriginStoragePrincipal = params.originStoragePrincipal;
        aOpenWindowInfo = params.openWindowInfo;
        aOpenerBrowser = params.openerBrowser;
        aCreateLazyBrowser = params.createLazyBrowser;
        aFocusUrlBar = params.focusUrlBar;
        aName = params.name;
        aCsp = params.csp;
        aSkipLoad = params.skipLoad;
      }

      // all callers of loadOneTab need to pass a valid triggeringPrincipal.
      if (!aTriggeringPrincipal) {
        throw new Error(
          "Required argument triggeringPrincipal missing within loadOneTab"
        );
      }

      var bgLoad =
        aLoadInBackground != null
          ? aLoadInBackground
          : Services.prefs.getBoolPref("browser.tabs.loadInBackground");
      var owner = bgLoad ? null : this.selectedTab;

      var tab = this.addTab(aURI, {
        triggeringPrincipal: aTriggeringPrincipal,
        referrerInfo: aReferrerInfo,
        charset: aCharset,
        postData: aPostData,
        ownerTab: owner,
        allowInheritPrincipal: aAllowInheritPrincipal,
        allowThirdPartyFixup: aAllowThirdPartyFixup,
        fromExternal: aFromExternal,
        relatedToCurrent: aRelatedToCurrent,
        skipAnimation: aSkipAnimation,
        forceNotRemote: aForceNotRemote,
        createLazyBrowser: aCreateLazyBrowser,
        preferredRemoteType: aPreferredRemoteType,
        userContextId: aUserContextId,
        originPrincipal: aOriginPrincipal,
        originStoragePrincipal: aOriginStoragePrincipal,
        initialBrowsingContextGroupId: aInitialBrowsingContextGroupId,
        openWindowInfo: aOpenWindowInfo,
        openerBrowser: aOpenerBrowser,
        focusUrlBar: aFocusUrlBar,
        name: aName,
        csp: aCsp,
        skipLoad: aSkipLoad,
      });
      if (!bgLoad) {
        this.selectedTab = tab;
      }

      return tab;
    },

    loadTabs(
      aURIs,
      {
        allowInheritPrincipal,
        allowThirdPartyFixup,
        inBackground,
        newIndex,
        postDatas,
        replace,
        targetTab,
        triggeringPrincipal,
        csp,
        userContextId,
        fromExternal,
      } = {}
    ) {
      if (!aURIs.length) {
        return;
      }

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
      var owner = multiple || inBackground ? null : this.selectedTab;
      var firstTabAdded = null;
      var targetTabIndex = -1;

      if (typeof newIndex != "number") {
        newIndex = -1;
      }

      // When bulk opening tabs, such as from a bookmark folder, we want to insertAfterCurrent
      // if necessary, but we also will set the bulkOrderedOpen flag so that the bookmarks
      // open in the same order they are in the folder.
      if (
        multiple &&
        newIndex < 0 &&
        Services.prefs.getBoolPref("browser.tabs.insertAfterCurrent")
      ) {
        newIndex = this.selectedTab._tPos + 1;
      }

      if (replace) {
        let browser;
        if (targetTab) {
          browser = this.getBrowserForTab(targetTab);
          targetTabIndex = targetTab._tPos;
        } else {
          browser = this.selectedBrowser;
          targetTabIndex = this.tabContainer.selectedIndex;
        }
        let flags = Ci.nsIWebNavigation.LOAD_FLAGS_NONE;
        if (allowThirdPartyFixup) {
          flags |=
            Ci.nsIWebNavigation.LOAD_FLAGS_ALLOW_THIRD_PARTY_FIXUP |
            Ci.nsIWebNavigation.LOAD_FLAGS_FIXUP_SCHEME_TYPOS;
        }
        if (!allowInheritPrincipal) {
          flags |= Ci.nsIWebNavigation.LOAD_FLAGS_DISALLOW_INHERIT_PRINCIPAL;
        }
        if (fromExternal) {
          flags |= Ci.nsIWebNavigation.LOAD_FLAGS_FROM_EXTERNAL;
        }
        try {
          browser.loadURI(aURIs[0], {
            flags,
            postData: postDatas && postDatas[0],
            triggeringPrincipal,
            csp,
          });
        } catch (e) {
          // Ignore failure in case a URI is wrong, so we can continue
          // opening the next ones.
        }
      } else {
        let params = {
          allowInheritPrincipal,
          ownerTab: owner,
          skipAnimation: multiple,
          allowThirdPartyFixup,
          postData: postDatas && postDatas[0],
          userContextId,
          triggeringPrincipal,
          bulkOrderedOpen: multiple,
          csp,
          fromExternal,
        };
        if (newIndex > -1) {
          params.index = newIndex;
        }
        firstTabAdded = this.addTab(aURIs[0], params);
        if (newIndex > -1) {
          targetTabIndex = firstTabAdded._tPos;
        }
      }

      let tabNum = targetTabIndex;
      for (let i = 1; i < aURIs.length; ++i) {
        let params = {
          allowInheritPrincipal,
          skipAnimation: true,
          allowThirdPartyFixup,
          postData: postDatas && postDatas[i],
          userContextId,
          triggeringPrincipal,
          bulkOrderedOpen: true,
          csp,
          fromExternal,
        };
        if (targetTabIndex > -1) {
          params.index = ++tabNum;
        }
        this.addTab(aURIs[i], params);
      }

      if (firstTabAdded && !inBackground) {
        this.selectedTab = firstTabAdded;
      }
    },

    updateBrowserRemoteness(aBrowser, { newFrameloader, remoteType } = {}) {
      let isRemote = aBrowser.getAttribute("remote") == "true";

      // We have to be careful with this here, as the "no remote type" is null,
      // not a string. Make sure to check only for undefined, since null is
      // allowed.
      if (remoteType === undefined) {
        throw new Error("Remote type must be set!");
      }

      let shouldBeRemote = remoteType !== E10SUtils.NOT_REMOTE;

      if (!gMultiProcessBrowser && shouldBeRemote) {
        throw new Error(
          "Cannot switch to remote browser in a window " +
            "without the remote tabs load context."
        );
      }

      // Abort if we're not going to change anything
      let oldRemoteType = aBrowser.remoteType;
      if (
        isRemote == shouldBeRemote &&
        !newFrameloader &&
        (!isRemote || oldRemoteType == remoteType)
      ) {
        return false;
      }

      let tab = this.getTabForBrowser(aBrowser);
      // aBrowser needs to be inserted now if it hasn't been already.
      this._insertBrowser(tab);

      let evt = document.createEvent("Events");
      evt.initEvent("BeforeTabRemotenessChange", true, false);
      tab.dispatchEvent(evt);

      let wasActive = document.activeElement == aBrowser;

      // Unhook our progress listener.
      let filter = this._tabFilters.get(tab);
      let listener = this._tabListeners.get(tab);
      aBrowser.webProgress.removeProgressListener(filter);
      filter.removeProgressListener(listener);

      // We'll be creating a new listener, so destroy the old one.
      listener.destroy();

      let oldDroppedLinkHandler = aBrowser.droppedLinkHandler;
      let oldUserTypedValue = aBrowser.userTypedValue;
      let hadStartedLoad = aBrowser.didStartLoadSinceLastUserTyping();

      // Change the "remote" attribute.

      // Make sure the browser is destroyed so it unregisters from observer notifications
      aBrowser.destroy();

      if (shouldBeRemote) {
        aBrowser.setAttribute("remote", "true");
        aBrowser.setAttribute("remoteType", remoteType);
      } else {
        aBrowser.setAttribute("remote", "false");
        aBrowser.removeAttribute("remoteType");
      }

      // This call actually switches out our frameloaders. Do this as late as
      // possible before rebuilding the browser, as we'll need the new browser
      // state set up completely first.
      aBrowser.changeRemoteness({
        remoteType,
      });

      // Once we have new frameloaders, this call sets the browser back up.
      aBrowser.construct();

      aBrowser.userTypedValue = oldUserTypedValue;
      if (hadStartedLoad) {
        aBrowser.urlbarChangeTracker.startedLoad();
      }

      aBrowser.droppedLinkHandler = oldDroppedLinkHandler;

      // This shouldn't really be necessary (it should always set the same
      // value as activeness is correctly preserved across remoteness changes).
      // However, this has the side effect of sending MozLayerTreeReady /
      // MozLayerTreeCleared events for remote frames, which the tab switcher
      // depends on.
      aBrowser.docShellIsActive = this.shouldActivateDocShell(aBrowser);

      // Create a new tab progress listener for the new browser we just injected,
      // since tab progress listeners have logic for handling the initial about:blank
      // load
      listener = new TabProgressListener(tab, aBrowser, true, false);
      this._tabListeners.set(tab, listener);
      filter.addProgressListener(listener, Ci.nsIWebProgress.NOTIFY_ALL);

      // Restore the progress listener.
      aBrowser.webProgress.addProgressListener(
        filter,
        Ci.nsIWebProgress.NOTIFY_ALL
      );

      // Restore the securityUI state.
      let securityUI = aBrowser.securityUI;
      let state = securityUI
        ? securityUI.state
        : Ci.nsIWebProgressListener.STATE_IS_INSECURE;
      this._callProgressListeners(
        aBrowser,
        "onSecurityChange",
        [aBrowser.webProgress, null, state],
        true,
        false
      );
      let event = aBrowser.getContentBlockingEvents();
      // Include the true final argument to indicate that this event is
      // simulated (instead of being observed by the webProgressListener).
      this._callProgressListeners(
        aBrowser,
        "onContentBlockingEvent",
        [aBrowser.webProgress, null, event, true],
        true,
        false
      );

      if (shouldBeRemote) {
        // Switching the browser to be remote will connect to a new child
        // process so the browser can no longer be considered to be
        // crashed.
        tab.removeAttribute("crashed");
      } else {
        aBrowser.sendMessageToActor(
          "Browser:AppTab",
          { isAppTab: tab.pinned },
          "BrowserTab"
        );
      }

      if (wasActive) {
        aBrowser.focus();
      }

      // If the findbar has been initialised, reset its browser reference.
      if (this.isFindBarInitialized(tab)) {
        this.getCachedFindBar(tab).browser = aBrowser;
      }

      tab.linkedBrowser.sendMessageToActor(
        "Browser:HasSiblings",
        this.tabs.length > 1,
        "BrowserTab"
      );

      evt = document.createEvent("Events");
      evt.initEvent("TabRemotenessChange", true, false);
      tab.dispatchEvent(evt);

      return true;
    },

    updateBrowserRemotenessByURL(aBrowser, aURL, aOptions = {}) {
      if (!gMultiProcessBrowser) {
        return this.updateBrowserRemoteness(aBrowser, {
          remoteType: E10SUtils.NOT_REMOTE,
        });
      }

      let oldRemoteType = aBrowser.remoteType;

      let oa = E10SUtils.predictOriginAttributes({ browser: aBrowser });

      aOptions.remoteType = E10SUtils.getRemoteTypeForURI(
        aURL,
        gMultiProcessBrowser,
        gFissionBrowser,
        oldRemoteType,
        aBrowser.currentURI,
        oa
      );

      // If this URL can't load in the current browser then flip it to the
      // correct type.
      if (oldRemoteType != aOptions.remoteType || aOptions.newFrameloader) {
        return this.updateBrowserRemoteness(aBrowser, aOptions);
      }

      return false;
    },

    createBrowser({
      isPreloadBrowser,
      name,
      openWindowInfo,
      remoteType,
      initialBrowsingContextGroupId,
      uriIsAboutBlank,
      userContextId,
      skipLoad,
      initiallyActive,
    } = {}) {
      let b = document.createXULElement("browser");
      // Use the JSM global to create the permanentKey, so that if the
      // permanentKey is held by something after this window closes, it
      // doesn't keep the window alive.
      b.permanentKey = new (Cu.getGlobalForObject(Services).Object)();

      // Ensure that SessionStore has flushed any session history state from the
      // content process before we this browser's remoteness.
      if (!Services.appinfo.sessionHistoryInParent) {
        b.prepareToChangeRemoteness = () =>
          SessionStore.prepareToChangeRemoteness(b);
        b.afterChangeRemoteness = switchId => {
          let tab = this.getTabForBrowser(b);
          SessionStore.finishTabRemotenessChange(tab, switchId);
          return true;
        };
      }

      const defaultBrowserAttributes = {
        contextmenu: "contentAreaContextMenu",
        maychangeremoteness: "true",
        message: "true",
        messagemanagergroup: "browsers",
        selectmenulist: "ContentSelectDropdown",
        tooltip: "aHTMLTooltip",
        type: "content",
      };
      for (let attribute in defaultBrowserAttributes) {
        b.setAttribute(attribute, defaultBrowserAttributes[attribute]);
      }

      if (!initiallyActive) {
        b.setAttribute("initiallyactive", "false");
      }

      if (userContextId) {
        b.setAttribute("usercontextid", userContextId);
      }

      if (remoteType) {
        b.setAttribute("remoteType", remoteType);
        b.setAttribute("remote", "true");
      }

      if (!isPreloadBrowser) {
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
      if (isPreloadBrowser) {
        b.setAttribute("preloadedState", "preloaded");
      }

      // Ensure that the browser will be created in a specific initial
      // BrowsingContextGroup. This may change the process selection behaviour
      // of the newly created browser, and is often used in combination with
      // "remoteType" to ensure that the initial about:blank load occurs
      // within the same process as another window.
      if (initialBrowsingContextGroupId) {
        b.setAttribute(
          "initialBrowsingContextGroupId",
          initialBrowsingContextGroupId
        );
      }

      // Propagate information about the opening content window to the browser.
      if (openWindowInfo) {
        b.openWindowInfo = openWindowInfo;
      }

      // This will be used by gecko to control the name of the opened
      // window.
      if (name) {
        // XXX: The `name` property is special in HTML and XUL. Should
        // we use a different attribute name for this?
        b.setAttribute("name", name);
      }

      let notificationbox = document.createXULElement("notificationbox");
      notificationbox.setAttribute("notificationside", "top");

      // We set large flex on both containers to allow the devtools toolbox to
      // set a flex attribute. We don't want the toolbox to actually take up free
      // space, but we do want it to collapse when the window shrinks, and with
      // flex=0 it can't. When the toolbox is on the bottom it's a sibling of
      // browserStack, and when it's on the side it's a sibling of
      // browserContainer.
      let stack = document.createXULElement("stack");
      stack.className = "browserStack";
      stack.appendChild(b);
      stack.setAttribute("flex", "10000");

      let browserContainer = document.createXULElement("vbox");
      browserContainer.className = "browserContainer";
      browserContainer.appendChild(notificationbox);
      browserContainer.appendChild(stack);
      browserContainer.setAttribute("flex", "10000");

      let browserSidebarContainer = document.createXULElement("hbox");
      browserSidebarContainer.className = "browserSidebarContainer";
      browserSidebarContainer.appendChild(browserContainer);

      // Prevent the superfluous initial load of a blank document
      // if we're going to load something other than about:blank.
      if (!uriIsAboutBlank || skipLoad) {
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
              return (browser._cachedCurrentURI = Services.io.newURI(url));
            };
            break;
          case "didStartLoadSinceLastUserTyping":
            getter = () => () => false;
            break;
          case "fullZoom":
          case "textZoom":
            getter = () => 1;
            break;
          case "tabHasCustomZoom":
            getter = () => false;
            break;
          case "getTabBrowser":
            getter = () => () => this;
            break;
          case "isRemoteBrowser":
            getter = () => browser.getAttribute("remote") == "true";
            break;
          case "permitUnload":
            getter = () => () => ({ permitUnload: true });
            break;
          case "reload":
          case "reloadWithFlags":
            getter = () => params => {
              // Wait for load handler to be instantiated before
              // initializing the reload.
              aTab.addEventListener(
                "SSTabRestoring",
                () => {
                  browser[name](params);
                },
                { once: true }
              );
              gBrowser._insertBrowser(aTab);
            };
            break;
          case "remoteType":
            getter = () => {
              let url = SessionStore.getLazyTabValue(aTab, "url");
              // Avoid recreating the same nsIURI object over and over again...
              let uri;
              if (browser._cachedCurrentURI) {
                uri = browser._cachedCurrentURI;
              } else {
                uri = browser._cachedCurrentURI = Services.io.newURI(url);
              }
              let oa = E10SUtils.predictOriginAttributes({
                browser,
                userContextId: aTab.getAttribute("usercontextid"),
              });
              return E10SUtils.getRemoteTypeForURI(
                url,
                gMultiProcessBrowser,
                gFissionBrowser,
                undefined,
                uri,
                oa
              );
            };
            break;
          case "userTypedValue":
          case "userTypedClear":
            getter = () => SessionStore.getLazyTabValue(aTab, name);
            break;
          default:
            getter = () => {
              if (AppConstants.NIGHTLY_BUILD) {
                let message = `[bug 1345098] Lazy browser prematurely inserted via '${name}' property access:\n`;
                console.log(message + new Error().stack);
              }
              this._insertBrowser(aTab);
              return browser[name];
            };
            setter = value => {
              if (AppConstants.NIGHTLY_BUILD) {
                let message = `[bug 1345098] Lazy browser prematurely inserted via '${name}' property access:\n`;
                console.log(message + new Error().stack);
              }
              this._insertBrowser(aTab);
              return (browser[name] = value);
            };
        }
        Object.defineProperty(browser, name, {
          get: getter,
          set: setter,
          configurable: true,
          enumerable: true,
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

      let { uriIsAboutBlank, usingPreloadedContent } = aTab._browserParams;
      delete aTab._browserParams;
      delete browser._cachedCurrentURI;

      let panel = this.getPanel(browser);
      let uniqueId = this._generateUniquePanelID();
      panel.id = uniqueId;
      aTab.linkedPanel = uniqueId;

      // Inject the <browser> into the DOM if necessary.
      if (!panel.parentNode) {
        // NB: this appendChild call causes us to run constructors for the
        // browser element, which fires off a bunch of notifications. Some
        // of those notifications can cause code to run that inspects our
        // state, so it is important that the tab element is fully
        // initialized by this point.
        this.tabpanels.appendChild(panel);
      }

      // wire up a progress listener for the new browser object.
      let tabListener = new TabProgressListener(
        aTab,
        browser,
        uriIsAboutBlank,
        usingPreloadedContent
      );
      const filter = Cc[
        "@mozilla.org/appshell/component/browser-status-filter;1"
      ].createInstance(Ci.nsIWebProgress);
      filter.addProgressListener(tabListener, Ci.nsIWebProgress.NOTIFY_ALL);
      browser.webProgress.addProgressListener(
        filter,
        Ci.nsIWebProgress.NOTIFY_ALL
      );
      this._tabListeners.set(aTab, tabListener);
      this._tabFilters.set(aTab, filter);

      browser.droppedLinkHandler = handleDroppedLink;
      browser.loadURI = _loadURI.bind(null, browser);

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

      // If we transitioned from one browser to two browsers, we need to set
      // hasSiblings=false on both the existing browser and the new browser.
      if (this.tabs.length == 2) {
        this.tabs[0].linkedBrowser.sendMessageToActor(
          "Browser:HasSiblings",
          true,
          "BrowserTab"
        );
        this.tabs[1].linkedBrowser.sendMessageToActor(
          "Browser:HasSiblings",
          true,
          "BrowserTab"
        );
      } else {
        aTab.linkedBrowser.sendMessageToActor(
          "Browser:HasSiblings",
          this.tabs.length > 1,
          "BrowserTab"
        );
      }

      // Only fire this event if the tab is already in the DOM
      // and will be handled by a listener.
      if (aTab.isConnected) {
        var evt = new CustomEvent("TabBrowserInserted", {
          bubbles: true,
          detail: { insertedOnTabCreation: aInsertedOnTabCreation },
        });
        aTab.dispatchEvent(evt);
      }
    },

    _mayDiscardBrowser(aTab, aForceDiscard) {
      let browser = aTab.linkedBrowser;
      let action = aForceDiscard ? "unload" : "dontUnload";

      if (
        !aTab ||
        aTab.selected ||
        aTab.closing ||
        this._windowIsClosing ||
        !browser.isConnected ||
        !browser.isRemoteBrowser ||
        !browser.permitUnload(action).permitUnload
      ) {
        return false;
      }

      return true;
    },

    discardBrowser(aTab, aForceDiscard) {
      "use strict";
      let browser = aTab.linkedBrowser;

      if (!this._mayDiscardBrowser(aTab, aForceDiscard)) {
        return false;
      }

      // Reset sharing state.
      if (aTab._sharingState) {
        this.resetBrowserSharing(browser);
      }
      webrtcUI.forgetStreamsFromBrowserContext(browser.browsingContext);

      // Set browser parameters for when browser is restored.  Also remove
      // listeners and set up lazy restore data in SessionStore. This must
      // be done before browser is destroyed and removed from the document.
      aTab._browserParams = {
        uriIsAboutBlank: browser.currentURI.spec == "about:blank",
        remoteType: browser.remoteType,
        usingPreloadedContent: false,
      };

      SessionStore.resetBrowserToLazyState(aTab);

      // Remove the tab's filter and progress listener.
      let filter = this._tabFilters.get(aTab);
      let listener = this._tabListeners.get(aTab);
      browser.webProgress.removeProgressListener(filter);
      filter.removeProgressListener(listener);
      listener.destroy();

      this._tabListeners.delete(aTab);
      this._tabFilters.delete(aTab);

      // Reset the findbar and remove it if it is attached to the tab.
      if (aTab._findBar) {
        aTab._findBar.close(true);
        aTab._findBar.remove();
        delete aTab._findBar;
      }

      // Remove potentially stale attributes.
      let attributesToRemove = [
        "activemedia-blocked",
        "busy",
        "pendingicon",
        "progress",
        "soundplaying",
      ];
      let removedAttributes = [];
      for (let attr of attributesToRemove) {
        if (aTab.hasAttribute(attr)) {
          removedAttributes.push(attr);
          aTab.removeAttribute(attr);
        }
      }
      if (removedAttributes.length) {
        this._tabAttrModified(aTab, removedAttributes);
      }

      browser.destroy();
      this.getPanel(browser).remove();
      aTab.removeAttribute("linkedpanel");

      this._createLazyBrowser(aTab);

      let evt = new CustomEvent("TabBrowserDiscarded", { bubbles: true });
      aTab.dispatchEvent(evt);
      return true;
    },

    /**
     * Loads a tab with a default null principal unless specified
     */
    addWebTab(aURI, params = {}) {
      if (!params.triggeringPrincipal) {
        params.triggeringPrincipal = Services.scriptSecurityManager.createNullPrincipal(
          {
            userContextId: params.userContextId,
          }
        );
      }
      if (params.triggeringPrincipal.isSystemPrincipal) {
        throw new Error(
          "System principal should never be passed into addWebTab()"
        );
      }
      return this.addTab(aURI, params);
    },

    addAdjacentNewTab(tab) {
      Services.obs.notifyObservers(
        {
          wrappedJSObject: new Promise(resolve => {
            this.selectedTab = this.addTrustedTab(BROWSER_NEW_TAB_URL, {
              index: tab._tPos + 1,
              userContextId: tab.userContextId,
            });
            resolve(this.selectedBrowser);
          }),
        },
        "browser-open-newtab-start"
      );
    },

    /**
     * Must only be used sparingly for content that came from Chrome context
     * If in doubt use addWebTab
     */
    addTrustedTab(aURI, params = {}) {
      params.triggeringPrincipal = Services.scriptSecurityManager.getSystemPrincipal();
      return this.addTab(aURI, params);
    },

    // eslint-disable-next-line complexity
    addTab(
      aURI,
      {
        allowInheritPrincipal,
        allowThirdPartyFixup,
        bulkOrderedOpen,
        charset,
        createLazyBrowser,
        disableTRR,
        eventDetail,
        focusUrlBar,
        forceNotRemote,
        fromExternal,
        index,
        lazyTabTitle,
        name,
        noInitialLabel,
        openWindowInfo,
        openerBrowser,
        originPrincipal,
        originStoragePrincipal,
        ownerTab,
        pinned,
        postData,
        preferredRemoteType,
        referrerInfo,
        relatedToCurrent,
        initialBrowsingContextGroupId,
        skipAnimation,
        skipBackgroundNotify,
        triggeringPrincipal,
        userContextId,
        csp,
        skipLoad,
        batchInsertingTabs,
      } = {}
    ) {
      // all callers of addTab that pass a params object need to pass
      // a valid triggeringPrincipal.
      if (!triggeringPrincipal) {
        throw new Error(
          "Required argument triggeringPrincipal missing within addTab"
        );
      }

      if (!UserInteraction.running("browser.tabs.opening", window)) {
        UserInteraction.start("browser.tabs.opening", "initting", window);
      }

      // Don't use document.l10n.setAttributes because the FTL file is loaded
      // lazily and we won't be able to resolve the string.
      document
        .getElementById("History:UndoCloseTab")
        .setAttribute("data-l10n-args", JSON.stringify({ tabCount: 1 }));
      SessionStore.setLastClosedTabCount(window, 1);

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
      // the owner. If referrerInfo is set, and we don't have an
      // explicit relatedToCurrent arg, we assume that the tab is
      // related to the current tab, since referrerURI is null or
      // undefined if the tab is opened from an external application or
      // bookmark (i.e. somewhere other than an existing tab).
      if (relatedToCurrent == null) {
        relatedToCurrent = !!(referrerInfo && referrerInfo.originalReferrer);
      }
      let openerTab =
        (openerBrowser && this.getTabForBrowser(openerBrowser)) ||
        (relatedToCurrent && this.selectedTab);

      var t = document.createXULElement("tab", { is: "tabbrowser-tab" });
      // Tag the tab as being created so extension code can ignore events
      // prior to TabOpen.
      t.initializingTab = true;
      t.openerTab = openerTab;

      aURI = aURI || "about:blank";
      let aURIObject = null;
      try {
        aURIObject = Services.io.newURI(aURI);
      } catch (ex) {
        /* we'll try to fix up this URL later */
      }

      let lazyBrowserURI;
      if (createLazyBrowser && aURI != "about:blank") {
        lazyBrowserURI = aURIObject;
        aURI = "about:blank";
      }

      var uriIsAboutBlank = aURI == "about:blank";

      // When overflowing, new tabs are scrolled into view smoothly, which
      // doesn't go well together with the width transition. So we skip the
      // transition in that case.
      let animate =
        !skipAnimation &&
        !pinned &&
        this.tabContainer.getAttribute("overflow") != "true" &&
        !gReduceMotion;

      // Related tab inherits current tab's user context unless a different
      // usercontextid is specified
      if (userContextId == null && openerTab) {
        userContextId = openerTab.getAttribute("usercontextid") || 0;
      }

      if (!noInitialLabel) {
        if (isBlankPageURL(aURI)) {
          t.setAttribute("label", this.tabContainer.emptyTabTitle);
        } else {
          // Set URL as label so that the tab isn't empty initially.
          this.setInitialTabTitle(t, aURI, { beforeTabOpen: true });
        }
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

      t.classList.add("tabbrowser-tab");

      this.tabContainer._unlockTabSizing();

      if (!animate) {
        UserInteraction.update("browser.tabs.opening", "not-animated", window);
        t.setAttribute("fadein", "true");

        // Call _handleNewTab asynchronously as it needs to know if the
        // new tab is selected.
        setTimeout(
          function(tabContainer) {
            tabContainer._handleNewTab(t);
          },
          0,
          this.tabContainer
        );
      } else {
        UserInteraction.update("browser.tabs.opening", "animated", window);
      }

      let usingPreloadedContent = false;
      let b;

      try {
        if (!batchInsertingTabs) {
          // When we are not restoring a session, we need to know
          // insert the tab into the tab container in the correct position
          this._insertTabAtIndex(t, {
            index,
            ownerTab,
            openerTab,
            pinned,
            bulkOrderedOpen,
          });
        }

        // If we don't have a preferred remote type, and we have a remote
        // opener, use the opener's remote type.
        if (!preferredRemoteType && openerBrowser) {
          preferredRemoteType = openerBrowser.remoteType;
        }

        var oa = E10SUtils.predictOriginAttributes({ window, userContextId });

        // If URI is about:blank and we don't have a preferred remote type,
        // then we need to use the referrer, if we have one, to get the
        // correct remote type for the new tab.
        if (
          uriIsAboutBlank &&
          !preferredRemoteType &&
          referrerInfo &&
          referrerInfo.originalReferrer
        ) {
          preferredRemoteType = E10SUtils.getRemoteTypeForURI(
            referrerInfo.originalReferrer.spec,
            gMultiProcessBrowser,
            gFissionBrowser,
            E10SUtils.DEFAULT_REMOTE_TYPE,
            null,
            oa
          );
        }

        let remoteType = forceNotRemote
          ? E10SUtils.NOT_REMOTE
          : E10SUtils.getRemoteTypeForURI(
              aURI,
              gMultiProcessBrowser,
              gFissionBrowser,
              preferredRemoteType,
              null,
              oa
            );

        // If we open a new tab with the newtab URL in the default
        // userContext, check if there is a preloaded browser ready.
        if (aURI == BROWSER_NEW_TAB_URL && !userContextId) {
          b = NewTabPagePreloading.getPreloadedBrowser(window);
          if (b) {
            usingPreloadedContent = true;
          }
        }

        if (!b) {
          // No preloaded browser found, create one.
          b = this.createBrowser({
            remoteType,
            uriIsAboutBlank,
            userContextId,
            initialBrowsingContextGroupId,
            openWindowInfo,
            name,
            skipLoad,
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
            this.UrlbarProviderOpenTabs.registerOpenTab(
              lazyBrowserURI.spec,
              userContextId
            );
            b.registeredOpenURI = lazyBrowserURI;
          }
          SessionStore.setTabState(t, {
            entries: [
              {
                url: lazyBrowserURI ? lazyBrowserURI.spec : "about:blank",
                title: lazyTabTitle,
                triggeringPrincipal_base64: E10SUtils.serializePrincipal(
                  triggeringPrincipal
                ),
              },
            ],
          });
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
          this.getPanel(t.linkedBrowser).remove();
        }
        throw e;
      }

      // Hack to ensure that the about:newtab, and about:welcome favicon is loaded
      // instantaneously, to avoid flickering and improve perceived performance.
      this.setDefaultIcon(t, aURIObject);

      if (!batchInsertingTabs) {
        // Fire a TabOpen event
        this._fireTabOpen(t, eventDetail);

        if (
          !usingPreloadedContent &&
          originPrincipal &&
          originStoragePrincipal &&
          aURI
        ) {
          let { URI_INHERITS_SECURITY_CONTEXT } = Ci.nsIProtocolHandler;
          // Unless we know for sure we're not inheriting principals,
          // force the about:blank viewer to have the right principal:
          if (
            !aURIObject ||
            doGetProtocolFlags(aURIObject) & URI_INHERITS_SECURITY_CONTEXT
          ) {
            b.createAboutBlankContentViewer(
              originPrincipal,
              originStoragePrincipal
            );
          }
        }

        // If we didn't swap docShells with a preloaded browser
        // then let's just continue loading the page normally.
        if (
          !usingPreloadedContent &&
          (!uriIsAboutBlank || !allowInheritPrincipal) &&
          !skipLoad
        ) {
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
          } else if (!triggeringPrincipal.isSystemPrincipal) {
            // XXX this code must be reviewed and changed when bug 1616353
            // lands.
            flags |= Ci.nsIWebNavigation.LOAD_FLAGS_FIRST_LOAD;
          }
          if (!allowInheritPrincipal) {
            flags |= Ci.nsIWebNavigation.LOAD_FLAGS_DISALLOW_INHERIT_PRINCIPAL;
          }
          if (disableTRR) {
            flags |= Ci.nsIWebNavigation.LOAD_FLAGS_DISABLE_TRR;
          }
          try {
            b.loadURI(aURI, {
              flags,
              triggeringPrincipal,
              referrerInfo,
              charset,
              postData,
              csp,
            });
          } catch (ex) {
            Cu.reportError(ex);
          }
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

      gSharedTabWarning.tabAdded(t);

      return t;
    },

    addMultipleTabs(restoreTabsLazily, selectTab, aPropertiesTabs) {
      let tabs = [];
      let tabsFragment = document.createDocumentFragment();
      let tabToSelect = null;
      let hiddenTabs = new Map();
      let shouldUpdateForPinnedTabs = false;

      // We create each tab and browser, but only insert them
      // into a document fragment so that we can insert them all
      // together. This prevents synch reflow for each tab
      // insertion.
      for (var i = 0; i < aPropertiesTabs.length; i++) {
        let tabData = aPropertiesTabs[i];

        let userContextId = tabData.userContextId;
        let select = i == selectTab - 1;
        let tab;
        let tabWasReused = false;

        // Re-use existing selected tab if possible to avoid the overhead of
        // selecting a new tab.
        if (
          select &&
          this.selectedTab.userContextId == userContextId &&
          !SessionStore.isTabRestoring(this.selectedTab)
        ) {
          tabWasReused = true;
          tab = this.selectedTab;
          if (!tabData.pinned) {
            this.unpinTab(tab);
          } else {
            this.pinTab(tab);
          }
        }

        // Add a new tab if needed.
        if (!tab) {
          let createLazyBrowser =
            restoreTabsLazily && !select && !tabData.pinned;

          let url = "about:blank";
          if (createLazyBrowser && tabData.entries && tabData.entries.length) {
            // Let tabbrowser know the future URI because progress listeners won't
            // get onLocationChange notification before the browser is inserted.
            let activeIndex = (tabData.index || tabData.entries.length) - 1;
            // Ensure the index is in bounds.
            activeIndex = Math.min(activeIndex, tabData.entries.length - 1);
            activeIndex = Math.max(activeIndex, 0);
            url = tabData.entries[activeIndex].url;
          }

          // Setting noInitialLabel is a perf optimization. Rendering tab labels
          // would make resizing the tabs more expensive as we're adding them.
          // Each tab will get its initial label set in restoreTab.
          tab = this.addTrustedTab(url, {
            createLazyBrowser,
            skipAnimation: true,
            allowInheritPrincipal: true,
            noInitialLabel: true,
            userContextId,
            skipBackgroundNotify: true,
            bulkOrderedOpen: true,
            batchInsertingTabs: true,
          });

          if (select) {
            tabToSelect = tab;
          }
        }

        tabs.push(tab);

        if (tabData.pinned) {
          // Calling `pinTab` calls `moveTabTo`, which assumes the tab is
          // inserted in the DOM. If the tab is not yet in the DOM,
          // just insert it in the right place from the start.
          if (!tab.parentNode) {
            tab._tPos = this._numPinnedTabs;
            this.tabContainer.insertBefore(tab, this.tabs[this._numPinnedTabs]);
            tab.setAttribute("pinned", "true");
            this._invalidateCachedTabs();
            // Then ensure all the tab open/pinning information is sent.
            this._fireTabOpen(tab, {});
            this._notifyPinnedStatus(tab);
            // Once we're done adding all tabs, _updateTabBarForPinnedTabs
            // needs calling:
            shouldUpdateForPinnedTabs = true;
          }
        } else {
          if (tab.hidden) {
            tab.hidden = true;
            hiddenTabs.set(tab, tabData.extData && tabData.extData.hiddenBy);
          }

          tabsFragment.appendChild(tab);
          if (tabWasReused) {
            this._invalidateCachedTabs();
          }
        }

        tab.initialize();
      }

      // inject the new DOM nodes
      this.tabContainer.appendChild(tabsFragment);

      for (let [tab, hiddenBy] of hiddenTabs) {
        let event = document.createEvent("Events");
        event.initEvent("TabHide", true, false);
        tab.dispatchEvent(event);
        if (hiddenBy) {
          SessionStore.setCustomTabValue(tab, "hiddenBy", hiddenBy);
        }
      }

      this._invalidateCachedTabs();
      if (shouldUpdateForPinnedTabs) {
        this._updateTabBarForPinnedTabs();
      }

      // We need to wait until after all tabs have been appended to the DOM
      // to remove the old selected tab.
      if (tabToSelect) {
        let leftoverTab = this.selectedTab;
        this.selectedTab = tabToSelect;
        this.removeTab(leftoverTab);
      }

      if (tabs.length > 1 || !tabs[0].selected) {
        this._updateTabsAfterInsert();
        this.tabContainer._setPositionalAttributes();
        TabBarVisibility.update();

        for (let tab of tabs) {
          // If tabToSelect is a tab, we didn't reuse the selected tab.
          if (tabToSelect || !tab.selected) {
            // Fire a TabOpen event for all unpinned tabs, except reused selected
            // tabs.
            if (!tab.pinned) {
              this._fireTabOpen(tab, {});
            }

            // Fire a TabBrowserInserted event on all tabs that have a connected,
            // real browser, except for reused selected tabs.
            if (tab.linkedPanel) {
              var evt = new CustomEvent("TabBrowserInserted", {
                bubbles: true,
                detail: { insertedOnTabCreation: true },
              });
              tab.dispatchEvent(evt);
            }
          }
        }
      }

      return tabs;
    },

    moveTabsToStart(contextTab) {
      let tabs = contextTab.multiselected ? this.selectedTabs : [contextTab];
      // Walk the array in reverse order so the tabs are kept in order.
      for (let i = tabs.length - 1; i >= 0; i--) {
        let tab = tabs[i];
        if (tab._tPos > 0) {
          this.moveTabTo(tab, 0);
        }
      }
    },

    moveTabsToEnd(contextTab) {
      let tabs = contextTab.multiselected ? this.selectedTabs : [contextTab];
      for (let tab of tabs) {
        if (tab._tPos < this.tabs.length - 1) {
          this.moveTabTo(tab, this.tabs.length - 1);
        }
      }
    },

    warnAboutClosingTabs(tabsToClose, aCloseTabs) {
      if (tabsToClose <= 1) {
        return true;
      }

      const pref =
        aCloseTabs == this.closingTabsEnum.ALL
          ? "browser.tabs.warnOnClose"
          : "browser.tabs.warnOnCloseOtherTabs";
      var shouldPrompt = Services.prefs.getBoolPref(pref);
      if (!shouldPrompt) {
        return true;
      }

      const maxTabsUndo = Services.prefs.getIntPref(
        "browser.sessionstore.max_tabs_undo"
      );
      if (
        aCloseTabs != this.closingTabsEnum.ALL &&
        tabsToClose <= maxTabsUndo
      ) {
        return true;
      }

      var ps = Services.prompt;

      // default to true: if it were false, we wouldn't get this far
      var warnOnClose = { value: true };

      // focus the window before prompting.
      // this will raise any minimized window, which will
      // make it obvious which window the prompt is for and will
      // solve the problem of windows "obscuring" the prompt.
      // see bug #350299 for more details
      window.focus();
      let warningMessage = gTabBrowserBundle.GetStringFromName(
        "tabs.closeWarningMultipleTabs"
      );
      warningMessage = PluralForm.get(tabsToClose, warningMessage).replace(
        "#1",
        tabsToClose
      );
      let flags =
        ps.BUTTON_TITLE_IS_STRING * ps.BUTTON_POS_0 +
        ps.BUTTON_TITLE_CANCEL * ps.BUTTON_POS_1;
      let checkboxLabel =
        aCloseTabs == this.closingTabsEnum.ALL
          ? gTabBrowserBundle.GetStringFromName("tabs.closeWarningPrompt")
          : null;
      var buttonPressed = ps.confirmEx(
        window,
        gTabBrowserBundle.GetStringFromName("tabs.closeTitleTabs"),
        warningMessage,
        flags,
        gTabBrowserBundle.GetStringFromName("tabs.closeButtonMultiple"),
        null,
        null,
        checkboxLabel,
        warnOnClose
      );
      var reallyClose = buttonPressed == 0;

      // don't set the pref unless they press OK and it's false
      if (
        aCloseTabs == this.closingTabsEnum.ALL &&
        reallyClose &&
        !warnOnClose.value
      ) {
        Services.prefs.setBoolPref(pref, false);
      }

      return reallyClose;
    },

    /**
     * This determines where the tab should be inserted within the tabContainer
     */
    _insertTabAtIndex(
      tab,
      { index, ownerTab, openerTab, pinned, bulkOrderedOpen } = {}
    ) {
      // If this new tab is owned by another, assert that relationship
      if (ownerTab) {
        tab.owner = ownerTab;
      }

      // Ensure we have an index if one was not provided.
      if (typeof index != "number") {
        // Move the new tab after another tab if needed.
        if (
          !bulkOrderedOpen &&
          ((openerTab &&
            Services.prefs.getBoolPref(
              "browser.tabs.insertRelatedAfterCurrent"
            )) ||
            Services.prefs.getBoolPref("browser.tabs.insertAfterCurrent"))
        ) {
          let lastRelatedTab =
            openerTab && this._lastRelatedTabMap.get(openerTab);
          let previousTab = lastRelatedTab || openerTab || this.selectedTab;
          if (previousTab.multiselected) {
            index = this.selectedTabs[this.selectedTabs.length - 1]._tPos + 1;
          } else {
            index = previousTab._tPos + 1;
          }

          if (lastRelatedTab) {
            lastRelatedTab.owner = null;
          } else if (openerTab) {
            tab.owner = openerTab;
          }
          // Always set related map if opener exists.
          if (openerTab) {
            this._lastRelatedTabMap.set(openerTab, tab);
          }
        } else {
          index = Infinity;
        }
      }
      // Ensure index is within bounds.
      if (pinned) {
        index = Math.max(index, 0);
        index = Math.min(index, this._numPinnedTabs);
      } else {
        index = Math.max(index, this._numPinnedTabs);
        index = Math.min(index, this.tabs.length);
      }

      let tabAfter = this.tabs[index] || null;
      this._invalidateCachedTabs();
      // Prevent a flash of unstyled content by setting up the tab content
      // and inherited attributes before appending it (see Bug 1592054):
      tab.initialize();
      this.tabContainer.insertBefore(tab, tabAfter);
      if (tabAfter) {
        this._updateTabsAfterInsert();
      } else {
        tab._tPos = index;
      }

      if (pinned) {
        this._updateTabBarForPinnedTabs();
      }
      this.tabContainer._setPositionalAttributes();

      TabBarVisibility.update();
    },

    /**
     * Dispatch a new tab event. This should be called when things are in a
     * consistent state, such that listeners of this event can again open
     * or close tabs.
     */
    _fireTabOpen(tab, eventDetail) {
      delete tab.initializingTab;
      let evt = new CustomEvent("TabOpen", {
        bubbles: true,
        detail: eventDetail || {},
      });
      tab.dispatchEvent(evt);
    },

    getTabsToTheStartFrom(aTab) {
      let tabsToStart = [];
      let tabs = this.visibleTabs;
      for (let i = 0; i < tabs.length; ++i) {
        if (tabs[i] == aTab) {
          break;
        }
        // Ignore pinned tabs.
        if (tabs[i].pinned) {
          continue;
        }
        // In a multi-select context, select all unselected tabs
        // starting from the context tab.
        if (aTab.multiselected && tabs[i].multiselected) {
          continue;
        }
        tabsToStart.push(tabs[i]);
      }
      return tabsToStart;
    },

    getTabsToTheEndFrom(aTab) {
      let tabsToEnd = [];
      let tabs = this.visibleTabs;
      for (let i = tabs.length - 1; i >= 0; --i) {
        if (tabs[i] == aTab) {
          break;
        }
        // Ignore pinned tabs.
        if (tabs[i].pinned) {
          continue;
        }
        // In a multi-select context, select all unselected tabs
        // starting from the context tab.
        if (aTab.multiselected && tabs[i].multiselected) {
          continue;
        }
        tabsToEnd.push(tabs[i]);
      }
      return tabsToEnd;
    },

    /**
     * In a multi-select context, the tabs (except pinned tabs) that are located to the
     * left of the leftmost selected tab will be removed.
     */
    removeTabsToTheStartFrom(aTab) {
      let tabs = this.getTabsToTheStartFrom(aTab);
      if (
        !this.warnAboutClosingTabs(tabs.length, this.closingTabsEnum.TO_START)
      ) {
        return;
      }

      this.removeTabs(tabs);
    },

    /**
     * In a multi-select context, the tabs (except pinned tabs) that are located to the
     * right of the rightmost selected tab will be removed.
     */
    removeTabsToTheEndFrom(aTab) {
      let tabs = this.getTabsToTheEndFrom(aTab);
      if (
        !this.warnAboutClosingTabs(tabs.length, this.closingTabsEnum.TO_END)
      ) {
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
        tabsToRemove = this.visibleTabs.filter(
          tab => !tab.multiselected && !tab.pinned
        );
      } else {
        tabsToRemove = this.visibleTabs.filter(
          tab => tab != aTab && !tab.pinned
        );
      }

      if (
        !this.warnAboutClosingTabs(
          tabsToRemove.length,
          this.closingTabsEnum.OTHER
        )
      ) {
        return;
      }

      this.removeTabs(tabsToRemove);
    },

    removeMultiSelectedTabs() {
      let selectedTabs = this.selectedTabs;
      if (
        !this.warnAboutClosingTabs(
          selectedTabs.length,
          this.closingTabsEnum.MULTI_SELECTED
        )
      ) {
        return;
      }

      this.removeTabs(selectedTabs);
    },

    removeTabs(
      tabs,
      { animate = true, suppressWarnAboutClosingWindow = false } = {}
    ) {
      // When 'closeWindowWithLastTab' pref is enabled, closing all tabs
      // can be considered equivalent to closing the window.
      if (
        this.tabs.length == tabs.length &&
        Services.prefs.getBoolPref("browser.tabs.closeWindowWithLastTab")
      ) {
        window.closeWindow(
          true,
          suppressWarnAboutClosingWindow ? null : window.warnAboutClosingWindow
        );
        return;
      }

      let initialTabCount = tabs.length;
      this._clearMultiSelectionLocked = true;

      // Guarantee that _clearMultiSelectionLocked lock gets released.
      try {
        let tabsWithBeforeUnloadPrompt = [];
        let tabsWithoutBeforeUnload = [];
        let beforeUnloadPromises = [];
        let lastToClose;
        let aParams = { animate, prewarmed: true };

        for (let tab of tabs) {
          if (tab.selected) {
            lastToClose = tab;
            let toBlurTo = this._findTabToBlurTo(lastToClose, tabs);
            if (toBlurTo) {
              this._getSwitcher().warmupTab(toBlurTo);
            }
          } else if (this._hasBeforeUnload(tab)) {
            TelemetryStopwatch.start("FX_TAB_CLOSE_PERMIT_UNLOAD_TIME_MS", tab);
            // We need to block while calling permitUnload() because it
            // processes the event queue and may lead to another removeTab()
            // call before permitUnload() returns.
            tab._pendingPermitUnload = true;
            beforeUnloadPromises.push(
              // To save time, we first run the beforeunload event listeners in all
              // content processes in parallel. Tabs that would have shown a prompt
              // will be handled again later.
              tab.linkedBrowser.asyncPermitUnload("dontUnload").then(
                ({ permitUnload }) => {
                  tab._pendingPermitUnload = false;
                  TelemetryStopwatch.finish(
                    "FX_TAB_CLOSE_PERMIT_UNLOAD_TIME_MS",
                    tab
                  );
                  if (tab.closing) {
                    // The tab was closed by the user while we were in permitUnload, don't
                    // attempt to close it a second time.
                  } else if (permitUnload) {
                    // OK to close without prompting, do it immediately.
                    this.removeTab(tab, {
                      animate,
                      prewarmed: true,
                      skipPermitUnload: true,
                    });
                  } else {
                    // We will need to prompt, queue it so it happens sequentially.
                    tabsWithBeforeUnloadPrompt.push(tab);
                  }
                },
                err => {
                  console.log("error while calling asyncPermitUnload", err);
                  tab._pendingPermitUnload = false;
                  TelemetryStopwatch.finish(
                    "FX_TAB_CLOSE_PERMIT_UNLOAD_TIME_MS",
                    tab
                  );
                }
              )
            );
          } else {
            tabsWithoutBeforeUnload.push(tab);
          }
        }
        // Now that all the beforeunload IPCs have been sent to content processes,
        // we can queue unload messages for all the tabs without beforeunload listeners.
        // Doing this first would cause content process main threads to be busy and delay
        // beforeunload responses, which would be user-visible.
        for (let tab of tabsWithoutBeforeUnload) {
          this.removeTab(tab, aParams);
        }

        // Wait for all the beforeunload events to have been processed by content processes.
        // The permitUnload() promise will, alas, not call its resolution
        // callbacks after the browser window the promise lives in has closed,
        // so we have to check for that case explicitly.
        let done = false;
        Promise.all(beforeUnloadPromises).then(() => {
          done = true;
        });
        Services.tm.spinEventLoopUntilOrQuit(
          "tabbrowser.js:removeTabs",
          () => done || window.closed
        );
        if (!done) {
          return;
        }

        // Now run again sequentially the beforeunload listeners that will result in a prompt.
        for (let tab of tabsWithBeforeUnloadPrompt) {
          this.removeTab(tab, aParams);
        }

        // Avoid changing the selected browser several times by removing it,
        // if appropriate, lastly.
        if (lastToClose) {
          this.removeTab(lastToClose, aParams);
        }
      } catch (e) {
        Cu.reportError(e);
      }

      this._clearMultiSelectionLocked = false;
      this.avoidSingleSelectedTab();
      let closedTabsCount =
        initialTabCount - tabs.filter(t => t.isConnected && !t.closing).length;
      // Don't use document.l10n.setAttributes because the FTL file is loaded
      // lazily and we won't be able to resolve the string.
      document
        .getElementById("History:UndoCloseTab")
        .setAttribute(
          "data-l10n-args",
          JSON.stringify({ tabCount: closedTabsCount })
        );
      SessionStore.setLastClosedTabCount(window, closedTabsCount);
    },

    removeCurrentTab(aParams) {
      this.removeTab(this.selectedTab, aParams);
    },

    removeTab(
      aTab,
      {
        animate,
        byMouse,
        skipPermitUnload,
        closeWindowWithLastTab,
        prewarmed,
      } = {}
    ) {
      if (UserInteraction.running("browser.tabs.opening", window)) {
        UserInteraction.finish("browser.tabs.opening", window);
      }

      // Telemetry stopwatches may already be running if removeTab gets
      // called again for an already closing tab.
      if (
        !TelemetryStopwatch.running("FX_TAB_CLOSE_TIME_ANIM_MS", aTab) &&
        !TelemetryStopwatch.running("FX_TAB_CLOSE_TIME_NO_ANIM_MS", aTab)
      ) {
        // Speculatevely start both stopwatches now. We'll cancel one of
        // the two later depending on whether we're animating.
        TelemetryStopwatch.start("FX_TAB_CLOSE_TIME_ANIM_MS", aTab);
        TelemetryStopwatch.start("FX_TAB_CLOSE_TIME_NO_ANIM_MS", aTab);
      }

      // Handle requests for synchronously removing an already
      // asynchronously closing tab.
      if (!animate && aTab.closing) {
        this._endRemoveTab(aTab);
        return;
      }

      var isLastTab = this.tabs.length - this._removingTabs.length == 1;
      let windowUtils = window.windowUtils;
      // We have to sample the tab width now, since _beginRemoveTab might
      // end up modifying the DOM in such a way that aTab gets a new
      // frame created for it (for example, by updating the visually selected
      // state).
      let tabWidth = windowUtils.getBoundsWithoutFlushing(aTab).width;

      if (
        !this._beginRemoveTab(aTab, {
          closeWindowFastpath: true,
          skipPermitUnload,
          closeWindowWithLastTab,
          prewarmed,
        })
      ) {
        TelemetryStopwatch.cancel("FX_TAB_CLOSE_TIME_ANIM_MS", aTab);
        TelemetryStopwatch.cancel("FX_TAB_CLOSE_TIME_NO_ANIM_MS", aTab);
        return;
      }

      if (!aTab.pinned && !aTab.hidden && aTab._fullyOpen && byMouse) {
        this.tabContainer._lockTabSizing(aTab, tabWidth);
      } else {
        this.tabContainer._unlockTabSizing();
      }

      if (
        !animate /* the caller didn't opt in */ ||
        gReduceMotion ||
        isLastTab ||
        aTab.pinned ||
        aTab.hidden ||
        this._removingTabs.length >
          3 /* don't want lots of concurrent animations */ ||
        aTab.getAttribute("fadein") !=
          "true" /* fade-in transition hasn't been triggered yet */ ||
        window.getComputedStyle(aTab).maxWidth ==
          "0.1px" /* fade-in transition hasn't moved yet */
      ) {
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

      setTimeout(
        function(tab, tabbrowser) {
          if (
            tab.container &&
            window.getComputedStyle(tab).maxWidth == "0.1px"
          ) {
            console.assert(
              false,
              "Giving up waiting for the tab closing animation to finish (bug 608589)"
            );
            tabbrowser._endRemoveTab(tab);
          }
        },
        3000,
        aTab,
        this
      );
    },

    _hasBeforeUnload(aTab) {
      let browser = aTab.linkedBrowser;
      if (browser.isRemoteBrowser && browser.frameLoader) {
        return browser.hasBeforeUnload;
      }
      return false;
    },

    _beginRemoveTab(
      aTab,
      {
        adoptedByTab,
        closeWindowWithLastTab,
        closeWindowFastpath,
        skipPermitUnload,
        prewarmed,
      } = {}
    ) {
      if (aTab.closing || this._windowIsClosing) {
        return false;
      }

      var browser = this.getBrowserForTab(aTab);
      if (
        !skipPermitUnload &&
        !adoptedByTab &&
        aTab.linkedPanel &&
        !aTab._pendingPermitUnload &&
        (!browser.isRemoteBrowser || this._hasBeforeUnload(aTab))
      ) {
        if (!prewarmed) {
          let blurTab = this._findTabToBlurTo(aTab);
          if (blurTab) {
            this.warmupTab(blurTab);
          }
        }

        TelemetryStopwatch.start("FX_TAB_CLOSE_PERMIT_UNLOAD_TIME_MS", aTab);

        // We need to block while calling permitUnload() because it
        // processes the event queue and may lead to another removeTab()
        // call before permitUnload() returns.
        aTab._pendingPermitUnload = true;
        let { permitUnload } = browser.permitUnload();
        aTab._pendingPermitUnload = false;

        TelemetryStopwatch.finish("FX_TAB_CLOSE_PERMIT_UNLOAD_TIME_MS", aTab);

        // If we were closed during onbeforeunload, we return false now
        // so we don't (try to) close the same tab again. Of course, we
        // also stop if the unload was cancelled by the user:
        if (aTab.closing || !permitUnload) {
          return false;
        }
      }

      // this._switcher would normally cover removing a tab from this
      // cache, but we may not have one at this time.
      let tabCacheIndex = this._tabLayerCache.indexOf(aTab);
      if (tabCacheIndex != -1) {
        this._tabLayerCache.splice(tabCacheIndex, 1);
      }

      // Delay hiding the the active tab if we're screen sharing.
      // See Bug 1642747.
      let screenShareInActiveTab =
        aTab == this.selectedTab && aTab._sharingState?.webRTC?.screen;

      if (!screenShareInActiveTab) {
        this._blurTab(aTab);
      }

      var closeWindow = false;
      var newTab = false;
      if (this.tabs.length - this._removingTabs.length == 1) {
        closeWindow =
          closeWindowWithLastTab != null
            ? closeWindowWithLastTab
            : !window.toolbar.visible ||
              Services.prefs.getBoolPref("browser.tabs.closeWindowWithLastTab");

        if (closeWindow) {
          // We've already called beforeunload on all the relevant tabs if we get here,
          // so avoid calling it again:
          window.skipNextCanClose = true;
        }

        // Closing the tab and replacing it with a blank one is notably slower
        // than closing the window right away. If the caller opts in, take
        // the fast path.
        if (closeWindow && closeWindowFastpath && !this._removingTabs.length) {
          // This call actually closes the window, unless the user
          // cancels the operation.  We are finished here in both cases.
          this._windowIsClosing = window.closeWindow(
            true,
            window.warnAboutClosingWindow
          );
          return false;
        }

        newTab = true;
      }
      aTab._endRemoveArgs = [closeWindow, newTab];

      // swapBrowsersAndCloseOther will take care of closing the window without animation.
      if (closeWindow && adoptedByTab) {
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
      if (!adoptedByTab && aTab.hasAttribute("soundplaying")) {
        // Don't persist the muted state as this wasn't a user action.
        // This lets undo-close-tab return it to an unmuted state.
        aTab.linkedBrowser.mute(true);
      }

      aTab.closing = true;
      this._removingTabs.push(aTab);
      this._invalidateCachedTabs();

      // Invalidate hovered tab state tracking for this closing tab.
      if (this.tabContainer._hoveredTab == aTab) {
        aTab._mouseleave();
      }

      if (newTab) {
        this.addTrustedTab(BROWSER_NEW_TAB_URL, {
          skipAnimation: true,
        });
      } else {
        TabBarVisibility.update();
      }

      // Splice this tab out of any lines of succession before any events are
      // dispatched.
      this.replaceInSuccession(aTab, aTab.successor);
      this.setSuccessor(aTab, null);

      // We're committed to closing the tab now.
      // Dispatch a notification.
      // We dispatch it before any teardown so that event listeners can
      // inspect the tab that's about to close.
      let evt = new CustomEvent("TabClose", {
        bubbles: true,
        detail: { adoptedBy: adoptedByTab },
      });
      aTab.dispatchEvent(evt);

      if (this.tabs.length == 2) {
        // We're closing one of our two open tabs, inform the other tab that its
        // sibling is going away.
        this.tabs[0].linkedBrowser.sendMessageToActor(
          "Browser:HasSiblings",
          false,
          "BrowserTab"
        );
        this.tabs[1].linkedBrowser.sendMessageToActor(
          "Browser:HasSiblings",
          false,
          "BrowserTab"
        );
      }

      let notificationBox = this.readNotificationBox(browser);
      notificationBox?._stack?.remove();

      if (aTab.linkedPanel) {
        if (!adoptedByTab && !gMultiProcessBrowser) {
          // Prevent this tab from showing further dialogs, since we're closing it
          browser.contentWindow.windowUtils.disableDialogs();
        }

        // Remove the tab's filter and progress listener.
        const filter = this._tabFilters.get(aTab);

        browser.webProgress.removeProgressListener(filter);

        const listener = this._tabListeners.get(aTab);
        filter.removeProgressListener(listener);
        listener.destroy();
      }

      if (browser.registeredOpenURI && !adoptedByTab) {
        let userContextId = browser.getAttribute("usercontextid") || 0;
        this.UrlbarProviderOpenTabs.unregisterOpenTab(
          browser.registeredOpenURI.spec,
          userContextId
        );
        delete browser.registeredOpenURI;
      }

      // We are no longer the primary content area.
      browser.removeAttribute("primary");

      // Remove this tab as the owner of any other tabs, since it's going away.
      for (let tab of this.tabs) {
        if ("owner" in tab && tab.owner == aTab) {
          // |tab| is a child of the tab we're removing, make it an orphan
          tab.owner = null;
        }
      }

      return true;
    },

    _endRemoveTab(aTab) {
      if (!aTab || !aTab._endRemoveArgs) {
        return;
      }

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
        while (this._removingTabs.length) {
          this._endRemoveTab(this._removingTabs[0]);
        }
      } else if (!this._windowIsClosing) {
        if (aNewTab) {
          gURLBar.select();
        }

        // workaround for bug 345399
        this.tabContainer.arrowScrollbox._updateScrollButtonsDisabledState();
      }

      // We're going to remove the tab and the browser now.
      this._tabFilters.delete(aTab);
      this._tabListeners.delete(aTab);

      var browser = this.getBrowserForTab(aTab);

      if (aTab.linkedPanel) {
        // Because of the fact that we are setting JS properties on
        // the browser elements, and we have code in place
        // to preserve the JS objects for any elements that have
        // JS properties set on them, the browser element won't be
        // destroyed until the document goes away.  So we force a
        // cleanup ourselves.
        // This has to happen before we remove the child since functions
        // like `getBrowserContainer` expect the browser to be parented.
        browser.destroy();
      }

      var wasPinned = aTab.pinned;

      // Remove the tab ...
      aTab.remove();
      this._invalidateCachedTabs();

      // Update hashiddentabs if this tab was hidden.
      if (aTab.hidden) {
        this.tabContainer._updateHiddenTabsStatus();
      }

      // ... and fix up the _tPos properties immediately.
      for (let i = aTab._tPos; i < this.tabs.length; i++) {
        this.tabs[i]._tPos = i;
      }

      if (!this._windowIsClosing) {
        if (wasPinned) {
          this.tabContainer._positionPinnedTabs();
        }

        // update tab close buttons state
        this.tabContainer._updateCloseButtons();

        setTimeout(
          function(tabs) {
            tabs._lastTabClosedByMouse = false;
          },
          0,
          this.tabContainer
        );
      }

      // update tab positional properties and attributes
      this.selectedTab._selected = true;
      this.tabContainer._setPositionalAttributes();

      // Removing the panel requires fixing up selectedPanel immediately
      // (see below), which would be hindered by the potentially expensive
      // browser removal. So we remove the browser and the panel in two
      // steps.

      var panel = this.getPanel(browser);

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
      TelemetryStopwatch.finish(
        "FX_TAB_CLOSE_TIME_ANIM_MS",
        aTab,
        true /* aCanceledOkay */
      );
      TelemetryStopwatch.finish(
        "FX_TAB_CLOSE_TIME_NO_ANIM_MS",
        aTab,
        true /* aCanceledOkay */
      );

      if (aCloseWindow) {
        this._windowIsClosing = closeWindow(
          true,
          window.warnAboutClosingWindow
        );
      }
    },

    /**
     * Finds the tab that we will blur to if we blur aTab.
     * @param   aTab
     *          The tab we would blur
     * @param   aExcludeTabs
     *          Tabs to exclude from our search (i.e., because they are being
     *          closed along with aTab)
     */
    _findTabToBlurTo(aTab, aExcludeTabs = []) {
      if (!aTab.selected) {
        return null;
      }

      let excludeTabs = new Set(aExcludeTabs);

      // If this tab has a successor, it should be selectable, since
      // hiding or closing a tab removes that tab as a successor.
      if (aTab.successor && !excludeTabs.has(aTab.successor)) {
        return aTab.successor;
      }

      if (
        aTab.owner &&
        !aTab.owner.hidden &&
        !aTab.owner.closing &&
        !excludeTabs.has(aTab.owner) &&
        Services.prefs.getBoolPref("browser.tabs.selectOwnerOnClose")
      ) {
        return aTab.owner;
      }

      // Switch to a visible tab unless there aren't any others remaining
      let remainingTabs = this.visibleTabs;
      let numTabs = remainingTabs.length;
      if (numTabs == 0 || (numTabs == 1 && remainingTabs[0] == aTab)) {
        remainingTabs = Array.prototype.filter.call(
          this.tabs,
          tab => !tab.closing && !excludeTabs.has(tab)
        );
      }

      // Try to find a remaining tab that comes after the given tab
      let tab = this.tabContainer.findNextTab(aTab, {
        direction: 1,
        filter: _tab => remainingTabs.includes(_tab),
      });

      if (!tab) {
        tab = this.tabContainer.findNextTab(aTab, {
          direction: -1,
          filter: _tab => remainingTabs.includes(_tab),
        });
      }

      return tab;
    },

    _blurTab(aTab) {
      this.selectedTab = this._findTabToBlurTo(aTab);
    },

    /**
     * @returns {boolean}
     *   False if swapping isn't permitted, true otherwise.
     */
    swapBrowsersAndCloseOther(aOurTab, aOtherTab) {
      // Do not allow transfering a private tab to a non-private window
      // and vice versa.
      if (
        PrivateBrowsingUtils.isWindowPrivate(window) !=
        PrivateBrowsingUtils.isWindowPrivate(aOtherTab.ownerGlobal)
      ) {
        return false;
      }

      // Do not allow transfering a useRemoteSubframes tab to a
      // non-useRemoteSubframes window and vice versa.
      if (gFissionBrowser != aOtherTab.ownerGlobal.gFissionBrowser) {
        return false;
      }

      let ourBrowser = this.getBrowserForTab(aOurTab);
      let otherBrowser = aOtherTab.linkedBrowser;

      // Can't swap between chrome and content processes.
      if (ourBrowser.isRemoteBrowser != otherBrowser.isRemoteBrowser) {
        return false;
      }

      // Keep the userContextId if set on other browser
      if (otherBrowser.hasAttribute("usercontextid")) {
        ourBrowser.setAttribute(
          "usercontextid",
          otherBrowser.getAttribute("usercontextid")
        );
      }

      // That's gBrowser for the other window, not the tab's browser!
      var remoteBrowser = aOtherTab.ownerGlobal.gBrowser;
      var isPending = aOtherTab.hasAttribute("pending");

      let otherTabListener = remoteBrowser._tabListeners.get(aOtherTab);
      let stateFlags = 0;
      if (otherTabListener) {
        stateFlags = otherTabListener.mStateFlags;
      }

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
      if (
        !remoteBrowser._beginRemoveTab(aOtherTab, {
          adoptedByTab: aOurTab,
          closeWindowWithLastTab: true,
        })
      ) {
        return false;
      }

      // If this is the last tab of the window, hide the window
      // immediately without animation before the docshell swap, to avoid
      // about:blank being painted.
      let [closeWindow] = aOtherTab._endRemoveArgs;
      if (closeWindow) {
        let win = aOtherTab.ownerGlobal;
        win.windowUtils.suppressAnimation(true);
        // Only suppressing window animations isn't enough to avoid
        // an empty content area being painted.
        let baseWin = win.docShell.treeOwner.QueryInterface(Ci.nsIBaseWindow);
        baseWin.visibility = false;
      }

      let modifiedAttrs = [];
      if (aOtherTab.hasAttribute("muted")) {
        aOurTab.setAttribute("muted", "true");
        aOurTab.muteReason = aOtherTab.muteReason;
        // For non-lazy tabs, mute() must be called.
        if (aOurTab.linkedPanel) {
          ourBrowser.mute();
        }
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
      if (aOtherTab.hasAttribute("pictureinpicture")) {
        aOurTab.setAttribute("pictureinpicture", true);
        modifiedAttrs.push("pictureinpicture");

        let event = new CustomEvent("TabSwapPictureInPicture", {
          detail: aOurTab,
        });
        aOtherTab.dispatchEvent(event);
      }

      SitePermissions.copyTemporaryPermissions(otherBrowser, ourBrowser);

      // If the other tab is pending (i.e. has not been restored, yet)
      // then do not switch docShells but retrieve the other tab's state
      // and apply it to our tab.
      if (isPending) {
        // Tag tab so that the extension framework can ignore tab events that
        // are triggered amidst the tab/browser restoration process
        // (TabHide, TabPinned, TabUnpinned, "muted" attribute changes, etc.).
        aOurTab.initializingTab = true;
        delete ourBrowser._cachedCurrentURI;
        SessionStore.setTabState(aOurTab, SessionStore.getTabState(aOtherTab));
        delete aOurTab.initializingTab;

        // Make sure to unregister any open URIs.
        this._swapRegisteredOpenURIs(ourBrowser, otherBrowser);
      } else {
        // Workarounds for bug 458697
        // Icon might have been set on DOMLinkAdded, don't override that.
        if (!ourBrowser.mIconURL && otherBrowser.mIconURL) {
          this.setIcon(aOurTab, otherBrowser.mIconURL);
        }
        var isBusy = aOtherTab.hasAttribute("busy");
        if (isBusy) {
          aOurTab.setAttribute("busy", "true");
          modifiedAttrs.push("busy");
          if (aOurTab.selected) {
            this._isBusy = true;
          }
        }

        this._swapBrowserDocShells(aOurTab, otherBrowser, stateFlags);
      }

      // Unregister the previously opened URI
      if (otherBrowser.registeredOpenURI) {
        let userContextId = otherBrowser.getAttribute("usercontextid") || 0;
        this.UrlbarProviderOpenTabs.unregisterOpenTab(
          otherBrowser.registeredOpenURI.spec,
          userContextId
        );
        delete otherBrowser.registeredOpenURI;
      }

      // Handle findbar data (if any)
      let otherFindBar = aOtherTab._findBar;
      if (otherFindBar && otherFindBar.findMode == otherFindBar.FIND_NORMAL) {
        let oldValue = otherFindBar._findField.value;
        let wasHidden = otherFindBar.hidden;
        let ourFindBarPromise = this.getFindBar(aOurTab);
        ourFindBarPromise.then(ourFindBar => {
          if (!ourFindBar) {
            return;
          }
          ourFindBar._findField.value = oldValue;
          if (!wasHidden) {
            ourFindBar.onFindCommand();
          }
        });
      }

      // Finish tearing down the tab that's going away.
      if (closeWindow) {
        aOtherTab.ownerGlobal.close();
      } else {
        remoteBrowser._endRemoveTab(aOtherTab);
      }

      this.setTabTitle(aOurTab);

      // If the tab was already selected (this happens in the scenario
      // of replaceTabWithWindow), notify onLocationChange, etc.
      if (aOurTab.selected) {
        this.updateCurrentBrowser(true);
      }

      if (modifiedAttrs.length) {
        this._tabAttrModified(aOurTab, modifiedAttrs);
      }

      return true;
    },

    swapBrowsers(aOurTab, aOtherTab) {
      let otherBrowser = aOtherTab.linkedBrowser;
      let otherTabBrowser = otherBrowser.getTabBrowser();

      // We aren't closing the other tab so, we also need to swap its tablisteners.
      let filter = otherTabBrowser._tabFilters.get(aOtherTab);
      let tabListener = otherTabBrowser._tabListeners.get(aOtherTab);
      otherBrowser.webProgress.removeProgressListener(filter);
      filter.removeProgressListener(tabListener);

      // Perform the docshell swap through the common mechanism.
      this._swapBrowserDocShells(aOurTab, otherBrowser);

      // Restore the listeners for the swapped in tab.
      tabListener = new otherTabBrowser.ownerGlobal.TabProgressListener(
        aOtherTab,
        otherBrowser,
        false,
        false
      );
      otherTabBrowser._tabListeners.set(aOtherTab, tabListener);

      const notifyAll = Ci.nsIWebProgress.NOTIFY_ALL;
      filter.addProgressListener(tabListener, notifyAll);
      otherBrowser.webProgress.addProgressListener(filter, notifyAll);
    },

    _swapBrowserDocShells(aOurTab, aOtherBrowser, aStateFlags) {
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

      let remoteBrowser = aOtherBrowser.ownerGlobal.gBrowser;

      // If switcher is active, it will intercept swap events and
      // react as needed.
      if (!this._switcher) {
        aOtherBrowser.docShellIsActive = this.shouldActivateDocShell(
          ourBrowser
        );
      }

      // Swap the docshells
      ourBrowser.swapDocShells(aOtherBrowser);

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

      // Restore the progress listener
      tabListener = new TabProgressListener(
        aOurTab,
        ourBrowser,
        false,
        false,
        aStateFlags
      );
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

    announceWindowCreated(browser, userContextId) {
      let tab = this.getTabForBrowser(browser);
      if (tab) {
        if (userContextId) {
          ContextualIdentityService.telemetry(userContextId);
          tab.setUserContextId(userContextId);
        }

        browser.sendMessageToActor(
          "Browser:AppTab",
          { isAppTab: tab.pinned },
          "BrowserTab"
        );
      }

      // We don't want to update the container icon and identifier if
      // this is not the selected browser.
      if (browser == gBrowser.selectedBrowser) {
        updateUserContextUIIndicator();
      }
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
      SitePermissions.clearTemporaryBlockPermissions(browser);
      // Also reset DOS mitigations for the basic auth prompt on reload.
      delete browser.authPromptAbuseCounter;
      gIdentityHandler.hidePopup();
      gPermissionPanel.hidePopup();
      browser.reload();
    },

    addProgressListener(aListener) {
      if (arguments.length != 1) {
        Cu.reportError(
          "gBrowser.addProgressListener was " +
            "called with a second argument, " +
            "which is not supported. See bug " +
            "608628. Call stack: " +
            new Error().stack
        );
      }

      this.mProgressListeners.push(aListener);
    },

    removeProgressListener(aListener) {
      this.mProgressListeners = this.mProgressListeners.filter(
        l => l != aListener
      );
    },

    addTabsProgressListener(aListener) {
      this.mTabsProgressListeners.push(aListener);
    },

    removeTabsProgressListener(aListener) {
      this.mTabsProgressListeners = this.mTabsProgressListeners.filter(
        l => l != aListener
      );
    },

    getBrowserForTab(aTab) {
      return aTab.linkedBrowser;
    },

    showOnlyTheseTabs(aTabs) {
      for (let tab of this.tabs) {
        if (!aTabs.includes(tab)) {
          this.hideTab(tab);
        } else {
          this.showTab(tab);
        }
      }

      this.tabContainer._updateHiddenTabsStatus();
      this.tabContainer._handleTabSelect(true);
    },

    showTab(aTab) {
      if (aTab.hidden) {
        aTab.removeAttribute("hidden");
        this._invalidateCachedTabs();

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
      if (
        aTab.hidden ||
        aTab.pinned ||
        aTab.selected ||
        aTab.closing ||
        // Tabs that are sharing the screen, microphone or camera cannot be hidden.
        aTab._sharingState?.webRTC?.sharing
      ) {
        return;
      }
      aTab.setAttribute("hidden", "true");
      this._invalidateCachedTabs();

      this.tabContainer._updateCloseButtons();
      this.tabContainer._updateHiddenTabsStatus();

      this.tabContainer._setPositionalAttributes();

      // Splice this tab out of any lines of succession before any events are
      // dispatched.
      this.replaceInSuccession(aTab, aTab.successor);
      this.setSuccessor(aTab, null);

      let event = document.createEvent("Events");
      event.initEvent("TabHide", true, false);
      aTab.dispatchEvent(event);
      if (aSource) {
        SessionStore.setCustomTabValue(aTab, "hiddenBy", aSource);
      }
    },

    selectTabAtIndex(aIndex, aEvent) {
      let tabs = this.visibleTabs;

      // count backwards for aIndex < 0
      if (aIndex < 0) {
        aIndex += tabs.length;
        // clamp at index 0 if still negative.
        if (aIndex < 0) {
          aIndex = 0;
        }
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
      if (this.tabs.length == 1) {
        return null;
      }

      var options = "chrome,dialog=no,all";
      for (var name in aOptions) {
        options += "," + name + "=" + aOptions[name];
      }

      // Play the tab closing animation to give immediate feedback while
      // waiting for the new window to appear.
      // content area when the docshells are swapped.
      if (!gReduceMotion) {
        aTab.style.maxWidth = ""; // ensure that fade-out transition happens
        aTab.removeAttribute("fadein");
      }

      // tell a new window to take the "dropped" tab
      return window.openDialog(
        AppConstants.BROWSER_CHROME_URL,
        "_blank",
        options,
        aTab
      );
    },

    /**
     * Move contextTab (or selected tabs in a mutli-select context)
     * to a new browser window, unless it is (they are) already the only tab(s)
     * in the current window, in which case this will do nothing.
     */
    replaceTabsWithWindow(contextTab, aOptions) {
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
        return this.replaceTabWithWindow(tabs[0], aOptions);
      }

      // Play the closing animation for all selected tabs to give
      // immediate feedback while waiting for the new window to appear.
      if (!gReduceMotion) {
        for (let tab of tabs) {
          tab.style.maxWidth = ""; // ensure that fade-out transition happens
          tab.removeAttribute("fadein");
        }
      }

      // Create a new window and make it adopt the tabs, preserving their relative order.
      // The initial tab of the new window will be selected, so it should adopt the
      // selected tab of the original window, if applicable, or else the first moving tab.
      // This avoids tab-switches in the new window, preserving tab laziness.
      // However, to avoid multiple tab-switches in the original window, the other tabs
      // should be adopted before the selected one.
      let selectedTabIndex = Math.max(0, tabs.indexOf(gBrowser.selectedTab));
      let selectedTab = tabs[selectedTabIndex];
      let win = this.replaceTabWithWindow(selectedTab, aOptions);
      win.addEventListener(
        "before-initial-tab-adopted",
        () => {
          for (let i = 0; i < tabs.length; ++i) {
            if (i != selectedTabIndex) {
              win.gBrowser.adoptTab(tabs[i], i);
            }
          }
          // Restore tab selection
          let winVisibleTabs = win.gBrowser.visibleTabs;
          let winTabLength = winVisibleTabs.length;
          win.gBrowser.addRangeToMultiSelectedTabs(
            winVisibleTabs[0],
            winVisibleTabs[winTabLength - 1]
          );
          win.gBrowser.lockClearMultiSelectionOnce();
        },
        { once: true }
      );
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
      if (oldPosition == aIndex) {
        return;
      }

      // Don't allow mixing pinned and unpinned tabs.
      if (aTab.pinned) {
        aIndex = Math.min(aIndex, this._numPinnedTabs - 1);
      } else {
        aIndex = Math.max(aIndex, this._numPinnedTabs);
      }
      if (oldPosition == aIndex) {
        return;
      }

      if (!aKeepRelatedTabs) {
        this._lastRelatedTabMap = new WeakMap();
      }

      let wasFocused = document.activeElement == this.selectedTab;

      aIndex = aIndex < aTab._tPos ? aIndex : aIndex + 1;

      let neighbor = this.tabs[aIndex] || null;
      this._invalidateCachedTabs();
      this.tabContainer.insertBefore(aTab, neighbor);
      this._updateTabsAfterInsert();

      if (wasFocused) {
        this.selectedTab.focus();
      }

      this.tabContainer._handleTabSelect(true);

      if (aTab.pinned) {
        this.tabContainer._positionPinnedTabs();
      }

      this.tabContainer._setPositionalAttributes();

      var evt = document.createEvent("UIEvents");
      evt.initUIEvent("TabMove", true, false, window, oldPosition);
      aTab.dispatchEvent(evt);
    },

    moveTabForward() {
      let nextTab = this.tabContainer.findNextTab(this.selectedTab, {
        direction: 1,
        filter: tab => !tab.hidden,
      });

      if (nextTab) {
        this.moveTabTo(this.selectedTab, nextTab._tPos);
      } else if (this.arrowKeysShouldWrap) {
        this.moveTabToStart();
      }
    },

    /**
     * Adopts a tab from another browser window, and inserts it at aIndex
     *
     * @returns {object}
     *    The new tab in the current window, null if the tab couldn't be adopted.
     */
    adoptTab(aTab, aIndex, aSelectTab) {
      // Swap the dropped tab with a new one we create and then close
      // it in the other window (making it seem to have moved between
      // windows). We also ensure that the tab we create to swap into has
      // the same remote type and process as the one we're swapping in.
      // This makes sure we don't get a short-lived process for the new tab.
      let linkedBrowser = aTab.linkedBrowser;
      let createLazyBrowser = !aTab.linkedPanel;
      let params = {
        eventDetail: { adoptedTab: aTab },
        preferredRemoteType: linkedBrowser.remoteType,
        initialBrowsingContextGroupId: linkedBrowser.browsingContext?.group.id,
        skipAnimation: true,
        index: aIndex,
        createLazyBrowser,
        allowInheritPrincipal: createLazyBrowser,
      };

      let numPinned = this._numPinnedTabs;
      if (aIndex < numPinned || (aTab.pinned && aIndex == numPinned)) {
        params.pinned = true;
      }

      if (aTab.hasAttribute("usercontextid")) {
        // new tab must have the same usercontextid as the old one
        params.userContextId = aTab.getAttribute("usercontextid");
      }
      let newTab = this.addWebTab("about:blank", params);
      let newBrowser = this.getBrowserForTab(newTab);

      aTab.container._finishAnimateTabMove();

      if (!createLazyBrowser) {
        // Stop the about:blank load.
        newBrowser.stop();
        // Make sure it has a docshell.
        newBrowser.docShell;
      }

      if (!this.swapBrowsersAndCloseOther(newTab, aTab)) {
        // Swapping wasn't permitted. Bail out.
        this.removeTab(newTab);
        return null;
      }

      if (aSelectTab) {
        this.selectedTab = newTab;
      }

      return newTab;
    },

    moveTabBackward() {
      let previousTab = this.tabContainer.findNextTab(this.selectedTab, {
        direction: -1,
        filter: tab => !tab.hidden,
      });

      if (previousTab) {
        this.moveTabTo(this.selectedTab, previousTab._tPos);
      } else if (this.arrowKeysShouldWrap) {
        this.moveTabToEnd();
      }
    },

    moveTabToStart() {
      let tabPos = this.selectedTab._tPos;
      if (tabPos > 0) {
        this.moveTabTo(this.selectedTab, 0);
      }
    },

    moveTabToEnd() {
      let tabPos = this.selectedTab._tPos;
      if (tabPos < this.browsers.length - 1) {
        this.moveTabTo(this.selectedTab, this.browsers.length - 1);
      }
    },

    moveTabOver(aEvent) {
      if (
        (!RTL_UI && aEvent.keyCode == KeyEvent.DOM_VK_RIGHT) ||
        (RTL_UI && aEvent.keyCode == KeyEvent.DOM_VK_LEFT)
      ) {
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
     * @param   aOptions
     *          The new index of the tab
     */
    duplicateTab(aTab, aRestoreTabImmediately, aOptions) {
      return SessionStore.duplicateTab(
        window,
        aTab,
        0,
        aRestoreTabImmediately,
        aOptions
      );
    },

    addToMultiSelectedTabs(aTab) {
      if (aTab.multiselected) {
        return;
      }

      aTab.setAttribute("multiselected", "true");
      aTab.setAttribute("aria-selected", "true");
      this._multiSelectedTabsSet.add(aTab);
      this._startMultiSelectChange();
      if (this._multiSelectChangeRemovals.has(aTab)) {
        this._multiSelectChangeRemovals.delete(aTab);
      } else {
        this._multiSelectChangeAdditions.add(aTab);
      }
    },

    /**
     * Adds two given tabs and all tabs between them into the (multi) selected tabs collection
     */
    addRangeToMultiSelectedTabs(aTab1, aTab2) {
      if (aTab1 == aTab2) {
        return;
      }

      const tabs = this.visibleTabs;
      const indexOfTab1 = tabs.indexOf(aTab1);
      const indexOfTab2 = tabs.indexOf(aTab2);

      const [lowerIndex, higherIndex] =
        indexOfTab1 < indexOfTab2
          ? [indexOfTab1, indexOfTab2]
          : [indexOfTab2, indexOfTab1];

      for (let i = lowerIndex; i <= higherIndex; i++) {
        this.addToMultiSelectedTabs(tabs[i]);
      }
    },

    removeFromMultiSelectedTabs(aTab) {
      if (!aTab.multiselected) {
        return;
      }
      aTab.removeAttribute("multiselected");
      aTab.removeAttribute("aria-selected");
      this._multiSelectedTabsSet.delete(aTab);
      this._startMultiSelectChange();
      if (this._multiSelectChangeAdditions.has(aTab)) {
        this._multiSelectChangeAdditions.delete(aTab);
      } else {
        this._multiSelectChangeRemovals.add(aTab);
      }
    },

    clearMultiSelectedTabs() {
      if (this._clearMultiSelectionLocked) {
        if (this._clearMultiSelectionLockedOnce) {
          this._clearMultiSelectionLockedOnce = false;
          this._clearMultiSelectionLocked = false;
        }
        return;
      }

      if (this.multiSelectedTabsCount < 1) {
        return;
      }

      for (let tab of this.selectedTabs) {
        this.removeFromMultiSelectedTabs(tab);
      }
      this._lastMultiSelectedTabRef = null;
    },

    selectAllTabs() {
      let visibleTabs = this.visibleTabs;
      gBrowser.addRangeToMultiSelectedTabs(
        visibleTabs[0],
        visibleTabs[visibleTabs.length - 1]
      );
    },

    allTabsSelected() {
      return (
        this.visibleTabs.length == 1 ||
        this.visibleTabs.every(t => t.multiselected)
      );
    },

    lockClearMultiSelectionOnce() {
      this._clearMultiSelectionLockedOnce = true;
      this._clearMultiSelectionLocked = true;
    },

    unlockClearMultiSelection() {
      this._clearMultiSelectionLockedOnce = false;
      this._clearMultiSelectionLocked = false;
    },

    /**
     * Remove a tab from the multiselection if it's the only one left there.
     *
     * In fact, some scenario may lead to only one single tab multi-selected,
     * this is something to avoid (Chrome does the same)
     * Consider 4 tabs A,B,C,D with A having the focus
     * 1. select C with Ctrl
     * 2. Right-click on B and "Close Tabs to The Right"
     *
     * Expected result
     * C and D closing
     * A being the only multi-selected tab, selection should be cleared
     *
     *
     * Single selected tab could even happen with a none-focused tab.
     * For exemple with the menu "Close other tabs", it could happen
     * with a multi-selected pinned tab.
     * For illustration, consider 4 tabs A,B,C,D with B active
     * 1. pin A and Ctrl-select it
     * 2. Ctrl-select C
     * 3. right-click on D and click "Close Other Tabs"
     *
     * Expected result
     * B and C closing
     * A[pinned] being the only multi-selected tab, selection should be cleared.
     */
    avoidSingleSelectedTab() {
      if (this.multiSelectedTabsCount == 1) {
        this.clearMultiSelectedTabs();
      }
    },

    switchToNextMultiSelectedTab() {
      this._clearMultiSelectionLocked = true;

      // Guarantee that _clearMultiSelectionLocked lock gets released.
      try {
        let lastMultiSelectedTab = gBrowser.lastMultiSelectedTab;
        if (lastMultiSelectedTab != gBrowser.selectedTab) {
          gBrowser.selectedTab = lastMultiSelectedTab;
        } else {
          let selectedTabs = ChromeUtils.nondeterministicGetWeakSetKeys(
            this._multiSelectedTabsSet
          ).filter(tab => tab.isConnected && !tab.closing);
          let length = selectedTabs.length;
          gBrowser.selectedTab = selectedTabs[length - 1];
        }
      } catch (e) {
        Cu.reportError(e);
      }

      this._clearMultiSelectionLocked = false;
    },

    set selectedTabs(tabs) {
      this.clearMultiSelectedTabs();
      this.selectedTab = tabs[0];
      if (tabs.length > 1) {
        for (let tab of tabs) {
          this.addToMultiSelectedTabs(tab);
        }
      }
    },

    get selectedTabs() {
      let { selectedTab, _multiSelectedTabsSet } = this;
      let tabs = ChromeUtils.nondeterministicGetWeakSetKeys(
        _multiSelectedTabsSet
      ).filter(tab => tab.isConnected && !tab.closing);
      if (!_multiSelectedTabsSet.has(selectedTab)) {
        tabs.push(selectedTab);
      }
      return tabs.sort((a, b) => a._tPos > b._tPos);
    },

    get multiSelectedTabsCount() {
      return ChromeUtils.nondeterministicGetWeakSetKeys(
        this._multiSelectedTabsSet
      ).filter(tab => tab.isConnected && !tab.closing).length;
    },

    get lastMultiSelectedTab() {
      let tab = this._lastMultiSelectedTabRef
        ? this._lastMultiSelectedTabRef.get()
        : null;
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

    _startMultiSelectChange() {
      if (!this._multiSelectChangeStarted) {
        this._multiSelectChangeStarted = true;
        Promise.resolve().then(() => this._endMultiSelectChange());
      }
    },

    _endMultiSelectChange() {
      let noticeable = false;
      let { selectedTab } = this;
      if (this._multiSelectChangeAdditions.size) {
        if (!selectedTab.multiselected) {
          this.addToMultiSelectedTabs(selectedTab);
        }
        noticeable = true;
      }
      if (this._multiSelectChangeRemovals.size) {
        if (this._multiSelectChangeRemovals.has(selectedTab)) {
          this.switchToNextMultiSelectedTab();
        }
        this.avoidSingleSelectedTab();
        noticeable = true;
      }
      this._multiSelectChangeStarted = false;
      if (noticeable || this._multiSelectChangeSelected) {
        this._multiSelectChangeSelected = false;
        this._multiSelectChangeAdditions.clear();
        this._multiSelectChangeRemovals.clear();
        if (noticeable) {
          this.tabContainer._setPositionalAttributes();
        }
        this.dispatchEvent(
          new CustomEvent("TabMultiSelect", { bubbles: true })
        );
      }
    },

    toggleMuteAudioOnMultiSelectedTabs(aTab) {
      let tabsToToggle;
      if (aTab.activeMediaBlocked) {
        tabsToToggle = this.selectedTabs.filter(
          tab => tab.activeMediaBlocked || tab.linkedBrowser.audioMuted
        );
      } else {
        let tabMuted = aTab.linkedBrowser.audioMuted;
        tabsToToggle = this.selectedTabs.filter(
          tab =>
            // When a user is looking to mute selected tabs, then media-blocked tabs
            // should not be toggled. Otherwise those media-blocked tabs are going into a
            // playing and unmuted state.
            (tab.linkedBrowser.audioMuted == tabMuted &&
              !tab.activeMediaBlocked) ||
            (tab.activeMediaBlocked && tabMuted)
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
      // The selectedTabs getter returns the tabs
      // in visual order. We need to unpin in reverse
      // order to maintain visual order.
      let selectedTabs = this.selectedTabs;
      for (let i = selectedTabs.length - 1; i >= 0; i--) {
        let tab = selectedTabs[i];
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
      return (
        (aBrowser == this.selectedBrowser &&
          window.windowState != window.STATE_MINIMIZED &&
          !window.isFullyOccluded) ||
        this._printPreviewBrowsers.has(aBrowser)
      );
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

    /**
     * _maybeRequestReplyFromRemoteContent may call
     * aEvent.requestReplyFromRemoteContent if necessary.
     *
     * @param aEvent    The handling event.
     * @return          true if the handler should wait a reply event.
     *                  false if the handle can handle the immediately.
     */
    _maybeRequestReplyFromRemoteContent(aEvent) {
      if (aEvent.defaultPrevented) {
        return false;
      }
      // If the event target is a remote browser, and the event has not been
      // handled by the remote content yet, we should wait a reply event
      // from the content.
      if (aEvent.isWaitingReplyFromRemoteContent) {
        return true; // Somebody called requestReplyFromRemoteContent already.
      }
      if (
        !aEvent.isReplyEventFromRemoteContent &&
        aEvent.target?.isRemoteBrowser === true
      ) {
        aEvent.requestReplyFromRemoteContent();
        return true;
      }
      return false;
    },

    _handleKeyDownEvent(aEvent) {
      if (!aEvent.isTrusted) {
        // Don't let untrusted events mess with tabs.
        return;
      }

      // Skip this only if something has explicitly cancelled it.
      if (aEvent.defaultCancelled) {
        return;
      }

      // Don't check if the event was already consumed because tab
      // navigation should always work for better user experience.

      switch (ShortcutUtils.getSystemActionForEvent(aEvent)) {
        case ShortcutUtils.TOGGLE_CARET_BROWSING:
          this._maybeRequestReplyFromRemoteContent(aEvent);
          return;
        case ShortcutUtils.MOVE_TAB_BACKWARD:
          this.moveTabBackward();
          aEvent.preventDefault();
          return;
        case ShortcutUtils.MOVE_TAB_FORWARD:
          this.moveTabForward();
          aEvent.preventDefault();
          return;
        case ShortcutUtils.CLOSE_TAB:
          if (gBrowser.multiSelectedTabsCount) {
            gBrowser.removeMultiSelectedTabs();
          } else if (!this.selectedTab.pinned) {
            this.removeCurrentTab({ animate: true });
          }
          aEvent.preventDefault();
      }
    },

    toggleCaretBrowsing() {
      const kPrefShortcutEnabled =
        "accessibility.browsewithcaret_shortcut.enabled";
      const kPrefWarnOnEnable = "accessibility.warn_on_browsewithcaret";
      const kPrefCaretBrowsingOn = "accessibility.browsewithcaret";

      var isEnabled = Services.prefs.getBoolPref(kPrefShortcutEnabled);
      if (!isEnabled) {
        return;
      }

      // Toggle browse with caret mode
      var browseWithCaretOn = Services.prefs.getBoolPref(
        kPrefCaretBrowsingOn,
        false
      );
      var warn = Services.prefs.getBoolPref(kPrefWarnOnEnable, true);
      if (warn && !browseWithCaretOn) {
        var checkValue = { value: false };
        var promptService = Services.prompt;

        var buttonPressed = promptService.confirmEx(
          window,
          gTabBrowserBundle.GetStringFromName(
            "browsewithcaret.checkWindowTitle"
          ),
          gTabBrowserBundle.GetStringFromName("browsewithcaret.checkLabel"),
          // Make "No" the default:
          promptService.STD_YES_NO_BUTTONS | promptService.BUTTON_POS_1_DEFAULT,
          null,
          null,
          null,
          gTabBrowserBundle.GetStringFromName("browsewithcaret.checkMsg"),
          checkValue
        );
        if (buttonPressed != 0) {
          if (checkValue.value) {
            try {
              Services.prefs.setBoolPref(kPrefShortcutEnabled, false);
            } catch (ex) {}
          }
          return;
        }
        if (checkValue.value) {
          try {
            Services.prefs.setBoolPref(kPrefWarnOnEnable, false);
          } catch (ex) {}
        }
      }

      // Toggle the pref
      try {
        Services.prefs.setBoolPref(kPrefCaretBrowsingOn, !browseWithCaretOn);
      } catch (ex) {}
    },

    _handleKeyPressEvent(aEvent) {
      if (!aEvent.isTrusted) {
        // Don't let untrusted events mess with tabs.
        return;
      }

      // Skip this only if something has explicitly cancelled it.
      if (aEvent.defaultCancelled) {
        return;
      }

      switch (ShortcutUtils.getSystemActionForEvent(aEvent, { rtl: RTL_UI })) {
        case ShortcutUtils.TOGGLE_CARET_BROWSING:
          if (
            aEvent.defaultPrevented ||
            this._maybeRequestReplyFromRemoteContent(aEvent)
          ) {
            break;
          }
          this.toggleCaretBrowsing();
          break;

        case ShortcutUtils.NEXT_TAB:
          if (AppConstants.platform == "macosx") {
            this.tabContainer.advanceSelectedTab(1, true);
            aEvent.preventDefault();
          }
          break;
        case ShortcutUtils.PREVIOUS_TAB:
          if (AppConstants.platform == "macosx") {
            this.tabContainer.advanceSelectedTab(-1, true);
            aEvent.preventDefault();
          }
          break;
      }
    },

    getTabTooltip(tab, includeLabel = true) {
      let label = "";
      if (includeLabel) {
        label = tab._fullLabel || tab.getAttribute("label");
      }
      if (
        Services.prefs.getBoolPref(
          "browser.tabs.tooltipsShowPidAndActiveness",
          false
        )
      ) {
        if (tab.linkedBrowser) {
          // When enabled, show the PID of the content process, and if
          // we're running with fission enabled, try to include PIDs for
          // every remote subframe.
          let [contentPid, ...framePids] = E10SUtils.getBrowserPids(
            tab.linkedBrowser,
            gFissionBrowser
          );
          if (contentPid) {
            label += " (pid " + contentPid + ")";
            if (gFissionBrowser) {
              label += " [F";
              if (framePids.length) {
                label += " " + framePids.join(", ");
              }
              label += "]";
            }
          }
          if (tab.linkedBrowser.docShellIsActive) {
            label += " [A]";
          }
        }
      }
      if (tab.userContextId) {
        label = gTabBrowserBundle.formatStringFromName(
          "tabs.containers.tooltip",
          [
            label,
            ContextualIdentityService.getUserContextLabel(tab.userContextId),
          ]
        );
      }
      return label;
    },

    createTooltip(event) {
      event.stopPropagation();
      let tab = document.tooltipNode
        ? document.tooltipNode.closest("tab")
        : null;
      if (!tab) {
        event.preventDefault();
        return;
      }

      let stringWithShortcut = (stringId, keyElemId, pluralCount) => {
        let keyElem = document.getElementById(keyElemId);
        let shortcut = ShortcutUtils.prettifyShortcut(keyElem);
        return PluralForm.get(
          pluralCount,
          gTabBrowserBundle.GetStringFromName(stringId)
        )
          .replace("%S", shortcut)
          .replace("#1", pluralCount);
      };

      let label;
      const selectedTabs = this.selectedTabs;
      const contextTabInSelection = selectedTabs.includes(tab);
      const affectedTabsLength = contextTabInSelection
        ? selectedTabs.length
        : 1;
      if (tab.mOverCloseButton) {
        label = tab.selected
          ? stringWithShortcut(
              "tabs.closeTabs.tooltip",
              "key_close",
              affectedTabsLength
            )
          : PluralForm.get(
              affectedTabsLength,
              gTabBrowserBundle.GetStringFromName("tabs.closeTabs.tooltip")
            ).replace("#1", affectedTabsLength);
      }
      // When Picture-in-Picture is open, we repurpose '.tab-icon-sound' as
      // an inert Picture-in-Picture indicator, so we should display
      // the default tooltip
      else if (tab._overPlayingIcon && !tab.pictureinpicture) {
        let stringID;
        if (tab.selected) {
          stringID = tab.linkedBrowser.audioMuted
            ? "tabs.unmuteAudio2.tooltip"
            : "tabs.muteAudio2.tooltip";
          label = stringWithShortcut(
            stringID,
            "key_toggleMute",
            affectedTabsLength
          );
        } else {
          if (tab.hasAttribute("activemedia-blocked")) {
            stringID = "tabs.unblockAudio2.tooltip";
          } else {
            stringID = tab.linkedBrowser.audioMuted
              ? "tabs.unmuteAudio2.background.tooltip"
              : "tabs.muteAudio2.background.tooltip";
          }

          label = PluralForm.get(
            affectedTabsLength,
            gTabBrowserBundle.GetStringFromName(stringID)
          ).replace("#1", affectedTabsLength);
        }
      } else {
        label = this.getTabTooltip(tab);
      }

      if (!gProtonPlacesTooltip) {
        event.target.setAttribute("label", label);
        return;
      }

      let title = event.target.querySelector(".places-tooltip-title");
      title.textContent = label;
      let url = event.target.querySelector(".places-tooltip-uri");
      url.value = tab.linkedBrowser?.currentURI?.spec.replace(
        /^https:\/\//,
        ""
      );
      let icon = event.target.querySelector("#places-tooltip-insecure-icon");
      icon.hidden = !url.value.startsWith("http://");
    },

    handleEvent(aEvent) {
      switch (aEvent.type) {
        case "keydown":
          this._handleKeyDownEvent(aEvent);
          break;
        case "keypress":
          this._handleKeyPressEvent(aEvent);
          break;
        case "framefocusrequested": {
          let tab = this.getTabForBrowser(aEvent.target);
          if (!tab || tab == this.selectedTab) {
            // Let the focus manager try to do its thing by not calling
            // preventDefault(). It will still raise the window if appropriate.
            break;
          }
          this.selectedTab = tab;
          window.focus();
          aEvent.preventDefault();
          break;
        }
        case "sizemodechange":
        case "occlusionstatechange":
          if (aEvent.target == window && !this._switcher) {
            this.selectedBrowser.preserveLayers(
              window.windowState == window.STATE_MINIMIZED ||
                window.isFullyOccluded
            );
            this.selectedBrowser.docShellIsActive = this.shouldActivateDocShell(
              this.selectedBrowser
            );
          }
          break;
      }
    },

    observe(aSubject, aTopic, aData) {
      switch (aTopic) {
        case "contextual-identity-updated": {
          let identity = aSubject.wrappedJSObject;
          for (let tab of this.tabs) {
            if (tab.getAttribute("usercontextid") == identity.userContextId) {
              ContextualIdentityService.setTabStyle(tab);
            }
          }
          break;
        }
      }
    },

    refreshBlocked(actor, browser, data) {
      // The data object is expected to contain the following properties:
      //  - URI (string)
      //     The URI that a page is attempting to refresh or redirect to.
      //  - delay (int)
      //     The delay (in milliseconds) before the page was going to
      //     reload or redirect.
      //  - sameURI (bool)
      //     true if we're refreshing the page. false if we're redirecting.

      let brandBundle = document.getElementById("bundle_brand");
      let brandShortName = brandBundle.getString("brandShortName");
      let message = gNavigatorBundle.getFormattedString(
        "refreshBlocked." + (data.sameURI ? "refreshLabel" : "redirectLabel"),
        [brandShortName]
      );

      let notificationBox = this.getNotificationBox(browser);
      let notification = notificationBox.getNotificationWithValue(
        "refresh-blocked"
      );

      if (notification) {
        notification.label = message;
      } else {
        let refreshButtonText = gNavigatorBundle.getString(
          "refreshBlocked.goButton"
        );
        let refreshButtonAccesskey = gNavigatorBundle.getString(
          "refreshBlocked.goButton.accesskey"
        );

        let buttons = [
          {
            label: refreshButtonText,
            accessKey: refreshButtonAccesskey,
            callback() {
              actor.sendAsyncMessage("RefreshBlocker:Refresh", data);
            },
          },
        ];

        notificationBox.appendNotification(
          message,
          "refresh-blocked",
          "chrome://browser/skin/notification-icons/popup.svg",
          notificationBox.PRIORITY_INFO_MEDIUM,
          buttons
        );
      }
    },

    _generateUniquePanelID() {
      if (!this._uniquePanelIDCounter) {
        this._uniquePanelIDCounter = 0;
      }

      let outerID = window.docShell.outerWindowID;

      // We want panel IDs to be globally unique, that's why we include the
      // window ID. We switched to a monotonic counter as Date.now() lead
      // to random failures because of colliding IDs.
      return "panel-" + outerID + "-" + ++this._uniquePanelIDCounter;
    },

    destroy() {
      this.tabContainer.destroy();
      Services.obs.removeObserver(this, "contextual-identity-updated");

      for (let tab of this.tabs) {
        let browser = tab.linkedBrowser;
        if (browser.registeredOpenURI) {
          let userContextId = browser.getAttribute("usercontextid") || 0;
          this.UrlbarProviderOpenTabs.unregisterOpenTab(
            browser.registeredOpenURI.spec,
            userContextId
          );
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
        Services.els.removeSystemEventListener(
          document,
          "keypress",
          this,
          false
        );
      }
      window.removeEventListener("sizemodechange", this);
      window.removeEventListener("occlusionstatechange", this);
      window.removeEventListener("framefocusrequested", this);

      if (gMultiProcessBrowser) {
        if (this._switcher) {
          this._switcher.destroy();
        }
      }
    },

    _setupEventListeners() {
      this.tabpanels.addEventListener("select", event => {
        if (event.target == this.tabpanels) {
          this.updateCurrentBrowser();
        }
      });

      this.addEventListener("DOMWindowClose", event => {
        let browser = event.target;
        if (!browser.isRemoteBrowser) {
          if (!event.isTrusted) {
            // If the browser is not remote, then we expect the event to be trusted.
            // In the remote case, the DOMWindowClose event is captured in content,
            // a message is sent to the parent, and another DOMWindowClose event
            // is re-dispatched on the actual browser node. In that case, the event
            // won't  be marked as trusted, since it's synthesized by JavaScript.
            return;
          }
          // In the parent-process browser case, it's possible that the browser
          // that fired DOMWindowClose is actually a child of another browser. We
          // want to find the top-most browser to determine whether or not this is
          // for a tab or not. The chromeEventHandler will be the top-most browser.
          browser = event.target.docShell.chromeEventHandler;
        }

        if (this.tabs.length == 1) {
          // We already did PermitUnload in the content process
          // for this tab (the only one in the window). So we don't
          // need to do it again for any tabs.
          window.skipNextCanClose = true;
          // In the parent-process browser case, the nsCloseEvent will actually take
          // care of tearing down the window, but we need to do this ourselves in the
          // content-process browser case. Doing so in both cases doesn't appear to
          // hurt.
          window.close();
          return;
        }

        let tab = this.getTabForBrowser(browser);
        if (tab) {
          // Skip running PermitUnload since it already happened in
          // the content process.
          this.removeTab(tab, { skipPermitUnload: true });
          // If we don't preventDefault on the DOMWindowClose event, then
          // in the parent-process browser case, we're telling the platform
          // to close the entire window. Calling preventDefault is our way of
          // saying we took care of this close request by closing the tab.
          event.preventDefault();
        }
      });

      this.addEventListener("pagetitlechanged", event => {
        let browser = event.target;
        let tab = this.getTabForBrowser(browser);
        if (!tab || tab.hasAttribute("pending")) {
          return;
        }

        // Ignore empty title changes on internal pages. This prevents the title
        // from changing while Fluent is populating the (initially-empty) title
        // element.
        if (
          !browser.contentTitle &&
          browser.contentPrincipal.isSystemPrincipal
        ) {
          return;
        }

        let titleChanged = this.setTabTitle(tab);
        if (titleChanged && !tab.selected && !tab.hasAttribute("busy")) {
          tab.setAttribute("titlechanged", "true");
        }
      });

      this.addEventListener(
        "DOMWillOpenModalDialog",
        event => {
          if (!event.isTrusted) {
            return;
          }

          let targetIsWindow = event.target instanceof Window;

          // We're about to open a modal dialog, so figure out for which tab:
          // If this is a same-process modal dialog, then we're given its DOM
          // window as the event's target. For remote dialogs, we're given the
          // browser, but that's in the originalTarget and not the target,
          // because it's across the tabbrowser's XBL boundary.
          let tabForEvent = targetIsWindow
            ? this.getTabForBrowser(event.target.docShell.chromeEventHandler)
            : this.getTabForBrowser(event.originalTarget);

          // Focus window for beforeunload dialog so it is seen but don't
          // steal focus from other applications.
          if (
            event.detail &&
            event.detail.tabPrompt &&
            event.detail.inPermitUnload &&
            Services.focus.activeWindow
          ) {
            window.focus();
          }

          // Don't need to act if the tab is already selected or if there isn't
          // a tab for the event (e.g. for the webextensions options_ui remote
          // browsers embedded in the "about:addons" page):
          if (!tabForEvent || tabForEvent.selected) {
            return;
          }

          // We always switch tabs for beforeunload tab-modal prompts.
          if (
            event.detail &&
            event.detail.tabPrompt &&
            !event.detail.inPermitUnload
          ) {
            let docPrincipal = targetIsWindow
              ? event.target.document.nodePrincipal
              : null;
            // At least one of these should/will be non-null:
            let promptPrincipal =
              event.detail.promptPrincipal ||
              docPrincipal ||
              tabForEvent.linkedBrowser.contentPrincipal;

            // For null principals, we bail immediately and don't show the checkbox:
            if (!promptPrincipal || promptPrincipal.isNullPrincipal) {
              tabForEvent.setAttribute("attention", "true");
              this._tabAttrModified(tabForEvent, ["attention"]);
              return;
            }

            // For non-system/expanded principals without permission, we bail and show the checkbox.
            if (promptPrincipal.URI && !promptPrincipal.isSystemPrincipal) {
              let permission = Services.perms.testPermissionFromPrincipal(
                promptPrincipal,
                "focus-tab-by-prompt"
              );
              if (permission != Services.perms.ALLOW_ACTION) {
                // Tell the prompt box we want to show the user a checkbox:
                let tabPrompt = Services.prefs.getBoolPref(
                  "prompts.contentPromptSubDialog"
                )
                  ? this.getTabDialogBox(tabForEvent.linkedBrowser)
                  : this.getTabModalPromptBox(tabForEvent.linkedBrowser);

                tabPrompt.onNextPromptShowAllowFocusCheckboxFor(
                  promptPrincipal
                );
                tabForEvent.setAttribute("attention", "true");
                this._tabAttrModified(tabForEvent, ["attention"]);
                return;
              }
            }
            // ... so system and expanded principals, as well as permitted "normal"
            // URI-based principals, always get to steal focus for the tab when prompting.
          }

          // If permissions/origins dictate so, bring tab to the front.
          this.selectedTab = tabForEvent;
        },
        true
      );

      // When cancelling beforeunload tabmodal dialogs, reset the URL bar to
      // avoid spoofing risks.
      this.addEventListener(
        "DOMModalDialogClosed",
        event => {
          if (
            !event.detail?.wasPermitUnload ||
            event.detail.areLeaving ||
            event.target.nodeName != "browser"
          ) {
            return;
          }
          event.target.userTypedValue = null;
          if (event.target == this.selectedBrowser) {
            gURLBar.setURI();
          }
        },
        true
      );

      let onTabCrashed = event => {
        if (!event.isTrusted) {
          return;
        }

        let browser = event.originalTarget;

        if (!event.isTopFrame) {
          TabCrashHandler.onSubFrameCrash(browser, event.childID);
          return;
        }

        // Preloaded browsers do not actually have any tabs. If one crashes,
        // it should be released and removed.
        if (browser === this.preloadedBrowser) {
          NewTabPagePreloading.removePreloadedBrowser(window);
          return;
        }

        let isRestartRequiredCrash =
          event.type == "oop-browser-buildid-mismatch";

        let icon = browser.mIconURL;
        let tab = this.getTabForBrowser(browser);

        if (this.selectedBrowser == browser) {
          TabCrashHandler.onSelectedBrowserCrash(
            browser,
            isRestartRequiredCrash
          );
        } else {
          TabCrashHandler.onBackgroundBrowserCrash(
            browser,
            isRestartRequiredCrash
          );
        }

        tab.removeAttribute("soundplaying");
        this.setIcon(tab, icon);
      };

      this.addEventListener("oop-browser-crashed", onTabCrashed);
      this.addEventListener("oop-browser-buildid-mismatch", onTabCrashed);

      this.addEventListener("DOMAudioPlaybackStarted", event => {
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

      this.addEventListener("DOMAudioPlaybackStopped", event => {
        var tab = this.getTabFromAudioEvent(event);
        if (!tab) {
          return;
        }

        if (tab.hasAttribute("soundplaying")) {
          let removalDelay = Services.prefs.getIntPref(
            "browser.tabs.delayHidingAudioPlayingIconMS"
          );

          tab.style.setProperty(
            "--soundplaying-removal-delay",
            `${removalDelay - 300}ms`
          );
          tab.setAttribute("soundplaying-scheduledremoval", "true");
          this._tabAttrModified(tab, ["soundplaying-scheduledremoval"]);

          tab._soundPlayingAttrRemovalTimer = setTimeout(() => {
            tab.removeAttribute("soundplaying-scheduledremoval");
            tab.removeAttribute("soundplaying");
            this._tabAttrModified(tab, [
              "soundplaying",
              "soundplaying-scheduledremoval",
            ]);
          }, removalDelay);
        }
      });

      this.addEventListener("DOMAudioPlaybackBlockStarted", event => {
        var tab = this.getTabFromAudioEvent(event);
        if (!tab) {
          return;
        }

        if (!tab.hasAttribute("activemedia-blocked")) {
          tab.setAttribute("activemedia-blocked", true);
          this._tabAttrModified(tab, ["activemedia-blocked"]);
        }
      });

      this.addEventListener("DOMAudioPlaybackBlockStopped", event => {
        var tab = this.getTabFromAudioEvent(event);
        if (!tab) {
          return;
        }

        if (tab.hasAttribute("activemedia-blocked")) {
          tab.removeAttribute("activemedia-blocked");
          this._tabAttrModified(tab, ["activemedia-blocked"]);
          let hist = Services.telemetry.getHistogramById(
            "TAB_AUDIO_INDICATOR_USED"
          );
          hist.add(2 /* unblockByVisitingTab */);
        }
      });

      this.addEventListener("GloballyAutoplayBlocked", event => {
        let browser = event.originalTarget;
        let tab = this.getTabForBrowser(browser);
        if (!tab) {
          return;
        }

        SitePermissions.setForPrincipal(
          browser.contentPrincipal,
          "autoplay-media",
          SitePermissions.BLOCK,
          SitePermissions.SCOPE_GLOBAL,
          browser
        );
      });

      let tabContextFTLInserter = () => {
        MozXULElement.insertFTLIfNeeded("browser/tabContextMenu.ftl");
        // Un-lazify the l10n-ids now that the FTL file has been inserted.
        document
          .getElementById("tabContextMenu")
          .querySelectorAll("[data-lazy-l10n-id]")
          .forEach(el => {
            el.setAttribute(
              "data-l10n-id",
              el.getAttribute("data-lazy-l10n-id")
            );
            el.removeAttribute("data-lazy-l10n-id");
          });
        this.tabContainer.removeEventListener(
          "contextmenu",
          tabContextFTLInserter,
          true
        );
        this.tabContainer.removeEventListener(
          "mouseover",
          tabContextFTLInserter
        );
        this.tabContainer.removeEventListener(
          "focus",
          tabContextFTLInserter,
          true
        );
      };
      this.tabContainer.addEventListener(
        "contextmenu",
        tabContextFTLInserter,
        true
      );
      this.tabContainer.addEventListener("mouseover", tabContextFTLInserter);
      this.tabContainer.addEventListener("focus", tabContextFTLInserter, true);

      // Fired when Gecko has decided a <browser> element will change
      // remoteness. This allows persisting some state on this element across
      // process switches.
      this.addEventListener("WillChangeBrowserRemoteness", event => {
        let browser = event.originalTarget;
        let tab = this.getTabForBrowser(browser);
        if (!tab) {
          return;
        }

        // Dispatch the `BeforeTabRemotenessChange` event, allowing other code
        // to react to this tab's process switch.
        let evt = document.createEvent("Events");
        evt.initEvent("BeforeTabRemotenessChange", true, false);
        tab.dispatchEvent(evt);

        let wasActive = document.activeElement == browser;

        // Unhook our progress listener.
        let filter = this._tabFilters.get(tab);
        let oldListener = this._tabListeners.get(tab);
        browser.webProgress.removeProgressListener(filter);
        filter.removeProgressListener(oldListener);
        let stateFlags = oldListener.mStateFlags;
        let requestCount = oldListener.mRequestCount;

        // We'll be creating a new listener, so destroy the old one.
        oldListener.destroy();

        let oldDroppedLinkHandler = browser.droppedLinkHandler;
        let oldUserTypedValue = browser.userTypedValue;
        let hadStartedLoad = browser.didStartLoadSinceLastUserTyping();

        let didChange = didChangeEvent => {
          browser.userTypedValue = oldUserTypedValue;
          if (hadStartedLoad) {
            browser.urlbarChangeTracker.startedLoad();
          }

          browser.droppedLinkHandler = oldDroppedLinkHandler;

          // This shouldn't really be necessary (it should always set the same
          // value as activeness is correctly preserved across remoteness changes).
          // However, this has the side effect of sending MozLayerTreeReady /
          // MozLayerTreeCleared events for remote frames, which the tab switcher
          // depends on.
          browser.docShellIsActive = this.shouldActivateDocShell(browser);

          // Create a new tab progress listener for the new browser we just
          // injected, since tab progress listeners have logic for handling the
          // initial about:blank load
          let listener = new TabProgressListener(
            tab,
            browser,
            false,
            false,
            stateFlags,
            requestCount
          );
          this._tabListeners.set(tab, listener);
          filter.addProgressListener(listener, Ci.nsIWebProgress.NOTIFY_ALL);

          // Restore the progress listener.
          browser.webProgress.addProgressListener(
            filter,
            Ci.nsIWebProgress.NOTIFY_ALL
          );

          let cbEvent = browser.getContentBlockingEvents();
          // Include the true final argument to indicate that this event is
          // simulated (instead of being observed by the webProgressListener).
          this._callProgressListeners(
            browser,
            "onContentBlockingEvent",
            [browser.webProgress, null, cbEvent, true],
            true,
            false
          );

          if (browser.isRemoteBrowser) {
            // Switching the browser to be remote will connect to a new child
            // process so the browser can no longer be considered to be
            // crashed.
            tab.removeAttribute("crashed");
          } else {
            browser.sendMessageToActor(
              "Browser:AppTab",
              { isAppTab: tab.pinned },
              "BrowserTab"
            );
          }

          if (wasActive) {
            browser.focus();
          }

          if (this.isFindBarInitialized(tab)) {
            this.getCachedFindBar(tab).browser = browser;
          }

          browser.sendMessageToActor(
            "Browser:HasSiblings",
            this.tabs.length > 1,
            "BrowserTab"
          );

          evt = document.createEvent("Events");
          evt.initEvent("TabRemotenessChange", true, false);
          tab.dispatchEvent(evt);
        };
        browser.addEventListener("DidChangeBrowserRemoteness", didChange, {
          once: true,
        });
      });
    },

    setSuccessor(aTab, successorTab) {
      if (aTab.ownerGlobal != window) {
        throw new Error("Cannot set the successor of another window's tab");
      }
      if (successorTab == aTab) {
        successorTab = null;
      }
      if (successorTab && successorTab.ownerGlobal != window) {
        throw new Error("Cannot set the successor to another window's tab");
      }
      if (aTab.successor) {
        aTab.successor.predecessors.delete(aTab);
      }
      aTab.successor = successorTab;
      if (successorTab) {
        if (!successorTab.predecessors) {
          successorTab.predecessors = new Set();
        }
        successorTab.predecessors.add(aTab);
      }
    },

    /**
     * For all tabs with aTab as a successor, set the successor to aOtherTab
     * instead.
     */
    replaceInSuccession(aTab, aOtherTab) {
      if (aTab.predecessors) {
        for (const predecessor of Array.from(aTab.predecessors)) {
          this.setSuccessor(predecessor, aOtherTab);
        }
      }
    },
  };

  /**
   * A web progress listener object definition for a given tab.
   */
  class TabProgressListener {
    constructor(
      aTab,
      aBrowser,
      aStartsBlank,
      aWasPreloadedBrowser,
      aOrigStateFlags,
      aOrigRequestCount
    ) {
      let stateFlags = aOrigStateFlags || 0;
      // Initialize mStateFlags to non-zero e.g. when creating a progress
      // listener for preloaded browsers as there was no progress listener
      // around when the content started loading. If the content didn't
      // quite finish loading yet, mStateFlags will very soon be overridden
      // with the correct value and end up at STATE_STOP again.
      if (aWasPreloadedBrowser) {
        stateFlags =
          Ci.nsIWebProgressListener.STATE_STOP |
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
      this.mRequestCount = aOrigRequestCount || 0;
    }

    destroy() {
      delete this.mTab;
      delete this.mBrowser;
    }

    _callProgressListeners(...args) {
      args.unshift(this.mBrowser);
      return gBrowser._callProgressListeners.apply(gBrowser, args);
    }

    _shouldShowProgress(aRequest) {
      if (this.mBlank) {
        return false;
      }

      // Don't show progress indicators in tabs for about: URIs
      // pointing to local resources.
      if (
        aRequest instanceof Ci.nsIChannel &&
        aRequest.originalURI.schemeIs("about")
      ) {
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
      if (
        aStateFlags & Ci.nsIWebProgressListener.STATE_STOP &&
        this.mRequestCount == 0 &&
        !aLocation
      ) {
        return true;
      }

      let location = aLocation ? aLocation.spec : "";
      return location == "about:blank";
    }

    onProgressChange(
      aWebProgress,
      aRequest,
      aCurSelfProgress,
      aMaxSelfProgress,
      aCurTotalProgress,
      aMaxTotalProgress
    ) {
      this.mTotalProgress = aMaxTotalProgress
        ? aCurTotalProgress / aMaxTotalProgress
        : 0;

      if (!this._shouldShowProgress(aRequest)) {
        return;
      }

      if (this.mTotalProgress && this.mTab.hasAttribute("busy")) {
        this.mTab.setAttribute("progress", "true");
        gBrowser._tabAttrModified(this.mTab, ["progress"]);
      }

      this._callProgressListeners("onProgressChange", [
        aWebProgress,
        aRequest,
        aCurSelfProgress,
        aMaxSelfProgress,
        aCurTotalProgress,
        aMaxTotalProgress,
      ]);
    }

    onProgressChange64(
      aWebProgress,
      aRequest,
      aCurSelfProgress,
      aMaxSelfProgress,
      aCurTotalProgress,
      aMaxTotalProgress
    ) {
      return this.onProgressChange(
        aWebProgress,
        aRequest,
        aCurSelfProgress,
        aMaxSelfProgress,
        aCurTotalProgress,
        aMaxTotalProgress
      );
    }

    /* eslint-disable complexity */
    onStateChange(aWebProgress, aRequest, aStateFlags, aStatus) {
      if (!aRequest) {
        return;
      }

      let location, originalLocation;
      try {
        aRequest.QueryInterface(Ci.nsIChannel);
        location = aRequest.URI;
        originalLocation = aRequest.originalURI;
      } catch (ex) {}

      let ignoreBlank = this._isForInitialAboutBlank(
        aWebProgress,
        aStateFlags,
        location
      );

      const {
        STATE_START,
        STATE_STOP,
        STATE_IS_NETWORK,
      } = Ci.nsIWebProgressListener;

      // If we were ignoring some messages about the initial about:blank, and we
      // got the STATE_STOP for it, we'll want to pay attention to those messages
      // from here forward. Similarly, if we conclude that this state change
      // is one that we shouldn't be ignoring, then stop ignoring.
      if (
        (ignoreBlank &&
          aStateFlags & STATE_STOP &&
          aStateFlags & STATE_IS_NETWORK) ||
        (!ignoreBlank && this.mBlank)
      ) {
        this.mBlank = false;
      }

      if (aStateFlags & STATE_START && aStateFlags & STATE_IS_NETWORK) {
        this.mRequestCount++;

        if (aWebProgress.isTopLevel) {
          // Need to use originalLocation rather than location because things
          // like about:home and about:privatebrowsing arrive with nsIRequest
          // pointing to their resolved jar: or file: URIs.
          if (
            !(
              originalLocation &&
              gInitialPages.includes(originalLocation.spec) &&
              originalLocation != "about:blank" &&
              this.mBrowser.initialPageLoadedFromUserAction !=
                originalLocation.spec &&
              this.mBrowser.currentURI &&
              this.mBrowser.currentURI.spec == "about:blank"
            )
          ) {
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
          delete this.mBrowser.initialPageLoadedFromUserAction;
          // If the browser is loading it must not be crashed anymore
          this.mTab.removeAttribute("crashed");
        }

        if (this._shouldShowProgress(aRequest)) {
          if (
            !(aStateFlags & Ci.nsIWebProgressListener.STATE_RESTORING) &&
            aWebProgress &&
            aWebProgress.isTopLevel
          ) {
            this.mTab.setAttribute("busy", "true");
            gBrowser._tabAttrModified(this.mTab, ["busy"]);
            this.mTab._notselectedsinceload = !this.mTab.selected;
            gBrowser.syncThrobberAnimations(this.mTab);
          }

          if (this.mTab.selected) {
            gBrowser._isBusy = true;
          }
        }
      } else if (aStateFlags & STATE_STOP && aStateFlags & STATE_IS_NETWORK) {
        // since we (try to) only handle STATE_STOP of the last request,
        // the count of open requests should now be 0
        this.mRequestCount = 0;

        let modifiedAttrs = [];
        if (this.mTab.hasAttribute("busy")) {
          this.mTab.removeAttribute("busy");
          modifiedAttrs.push("busy");

          // Only animate the "burst" indicating the page has loaded if
          // the top-level page is the one that finished loading.
          if (
            aWebProgress.isTopLevel &&
            !aWebProgress.isLoadingDocument &&
            Components.isSuccessCode(aStatus) &&
            !gBrowser.tabAnimationsInProgress &&
            !gReduceMotion
          ) {
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
          if (!isSuccessful && !this.mTab.isEmpty) {
            // Restore the current document's location in case the
            // request was stopped (possibly from a content script)
            // before the location changed.

            this.mBrowser.userTypedValue = null;

            let isNavigating = this.mBrowser.isNavigating;
            if (this.mTab.selected && !isNavigating) {
              gURLBar.setURI();
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
        if (
          !this.mBrowser.mIconURL &&
          !ignoreBlank &&
          !(originalLocation.spec in FAVICON_DEFAULTS)
        ) {
          this.mTab.removeAttribute("image");
        }

        // For keyword URIs clear the user typed value since they will be changed into real URIs
        if (location.scheme == "keyword") {
          this.mBrowser.userTypedValue = null;
        }

        if (this.mTab.selected) {
          gBrowser._isBusy = false;
        }
      }

      if (ignoreBlank) {
        this._callProgressListeners(
          "onUpdateCurrentBrowser",
          [aStateFlags, aStatus, "", 0],
          true,
          false
        );
      } else {
        this._callProgressListeners(
          "onStateChange",
          [aWebProgress, aRequest, aStateFlags, aStatus],
          true,
          false
        );
      }

      this._callProgressListeners(
        "onStateChange",
        [aWebProgress, aRequest, aStateFlags, aStatus],
        false
      );

      if (aStateFlags & (STATE_START | STATE_STOP)) {
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

      let isSameDocument = !!(
        aFlags & Ci.nsIWebProgressListener.LOCATION_CHANGE_SAME_DOCUMENT
      );
      if (topLevel) {
        let isReload = !!(
          aFlags & Ci.nsIWebProgressListener.LOCATION_CHANGE_RELOAD
        );
        let isErrorPage = !!(
          aFlags & Ci.nsIWebProgressListener.LOCATION_CHANGE_ERROR_PAGE
        );

        // We need to clear the typed value
        // if the document failed to load, to make sure the urlbar reflects the
        // failed URI (particularly for SSL errors). However, don't clear the value
        // if the error page's URI is about:blank, because that causes complete
        // loss of urlbar contents for invalid URI errors (see bug 867957).
        // Another reason to clear the userTypedValue is if this was an anchor
        // navigation initiated by the user.
        // Finally, we do insert the URL if this is a same-document navigation
        // and the user cleared the URL manually.
        if (
          this.mBrowser.didStartLoadSinceLastUserTyping() ||
          (isErrorPage && aLocation.spec != "about:blank") ||
          (isSameDocument && this.mBrowser.isNavigating) ||
          (isSameDocument && !this.mBrowser.userTypedValue)
        ) {
          this.mBrowser.userTypedValue = null;
        }

        // If the tab has been set to "busy" outside the stateChange
        // handler below (e.g. by sessionStore.navigateAndRestore), and
        // the load results in an error page, it's possible that there
        // isn't any (STATE_IS_NETWORK & STATE_STOP) state to cause busy
        // attribute being removed. In this case we should remove the
        // attribute here.
        if (isErrorPage && this.mTab.hasAttribute("busy")) {
          this.mTab.removeAttribute("busy");
          gBrowser._tabAttrModified(this.mTab, ["busy"]);
        }

        if (!isSameDocument) {
          // If the browser was playing audio, we should remove the playing state.
          if (this.mTab.hasAttribute("soundplaying")) {
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

          // Note that we're not updating for same-document loads, despite
          // the `title` argument to `history.pushState/replaceState`. For
          // context, see https://bugzilla.mozilla.org/show_bug.cgi?id=585653
          // and https://github.com/whatwg/html/issues/2174
          if (!isReload) {
            gBrowser.setTabTitle(this.mTab);
          }

          // Don't clear the favicon if this tab is in the pending
          // state, as SessionStore will have set the icon for us even
          // though we're pointed at an about:blank. Also don't clear it
          // if the tab is in customize mode, to keep the one set by
          // gCustomizeMode.setTab (bug 1551239). Also don't clear it
          // if onLocationChange was triggered by a pushState or a
          // replaceState (bug 550565) or a hash change (bug 408415).
          if (
            !this.mTab.hasAttribute("pending") &&
            !this.mTab.hasAttribute("customizemode") &&
            aWebProgress.isLoadingDocument
          ) {
            // Removing the tab's image here causes flickering, wait until the
            // load is complete.
            this.mBrowser.mIconURL = null;
          }
        }

        let userContextId = this.mBrowser.getAttribute("usercontextid") || 0;
        if (this.mBrowser.registeredOpenURI) {
          let uri = this.mBrowser.registeredOpenURI;
          gBrowser.UrlbarProviderOpenTabs.unregisterOpenTab(
            uri.spec,
            userContextId
          );
          delete this.mBrowser.registeredOpenURI;
        }
        // Tabs in private windows aren't registered as "Open" so
        // that they don't appear as switch-to-tab candidates.
        if (
          !isBlankPageURL(aLocation.spec) &&
          (!PrivateBrowsingUtils.isWindowPrivate(window) ||
            PrivateBrowsingUtils.permanentPrivateBrowsing)
        ) {
          gBrowser.UrlbarProviderOpenTabs.registerOpenTab(
            aLocation.spec,
            userContextId
          );
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

      if (!this.mBlank || this.mBrowser.hasContentOpener) {
        this._callProgressListeners("onLocationChange", [
          aWebProgress,
          aRequest,
          aLocation,
          aFlags,
        ]);
        if (topLevel && !isSameDocument) {
          // Include the true final argument to indicate that this event is
          // simulated (instead of being observed by the webProgressListener).
          this._callProgressListeners("onContentBlockingEvent", [
            aWebProgress,
            null,
            0,
            true,
          ]);
        }
      }

      if (topLevel) {
        this.mBrowser.lastURI = aLocation;
        this.mBrowser.lastLocationChange = Date.now();
      }
    }

    onStatusChange(aWebProgress, aRequest, aStatus, aMessage) {
      if (this.mBlank) {
        return;
      }

      this._callProgressListeners("onStatusChange", [
        aWebProgress,
        aRequest,
        aStatus,
        aMessage,
      ]);

      this.mMessage = aMessage;
    }

    onSecurityChange(aWebProgress, aRequest, aState) {
      this._callProgressListeners("onSecurityChange", [
        aWebProgress,
        aRequest,
        aState,
      ]);
    }

    onContentBlockingEvent(aWebProgress, aRequest, aEvent) {
      this._callProgressListeners("onContentBlockingEvent", [
        aWebProgress,
        aRequest,
        aEvent,
      ]);
    }

    onRefreshAttempted(aWebProgress, aURI, aDelay, aSameURI) {
      return this._callProgressListeners("onRefreshAttempted", [
        aWebProgress,
        aURI,
        aDelay,
        aSameURI,
      ]);
    }
  }
  TabProgressListener.prototype.QueryInterface = ChromeUtils.generateQI([
    "nsIWebProgressListener",
    "nsIWebProgressListener2",
    "nsISupportsWeakReference",
  ]);
} // end private scope for gBrowser

var StatusPanel = {
  get panel() {
    delete this.panel;
    return (this.panel = document.getElementById("statuspanel"));
  },

  get isVisible() {
    return !this.panel.hasAttribute("inactive");
  },

  update() {
    if (BrowserHandler.kiosk) {
      return;
    }
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

    // If it's a long data: URI that uses base64 encoding, truncate to
    // a reasonable length rather than trying to display the entire thing.
    // We can't shorten arbitrary URIs like this, as bidi etc might mean
    // we need the trailing characters for display. But a base64-encoded
    // data-URI is plain ASCII, so this is OK for status panel display.
    // (See bug 1484071.)
    let textCropped = false;
    if (text.length > 500 && text.match(/^data:[^,]+;base64,/)) {
      text = text.substring(0, 500) + "\u2026";
      textCropped = true;
    }

    if (this._labelElement.value != text || (text && !this.isVisible)) {
      this.panel.setAttribute("previoustype", this.panel.getAttribute("type"));
      this.panel.setAttribute("type", type);
      this._label = text;
      this._labelElement.setAttribute(
        "crop",
        type == "overLink" && !textCropped ? "center" : "end"
      );
    }
  },

  get _labelElement() {
    delete this._labelElement;
    return (this._labelElement = document.getElementById("statuspanel-label"));
  },

  set _label(val) {
    if (!this.isVisible) {
      this.panel.removeAttribute("mirror");
      this.panel.removeAttribute("sizelimit");
    }

    if (
      this.panel.getAttribute("type") == "status" &&
      this.panel.getAttribute("previoustype") == "status"
    ) {
      // Before updating the label, set the panel's current width as its
      // min-width to let the panel grow but not shrink and prevent
      // unnecessary flicker while loading pages. We only care about the
      // panel's width once it has been painted, so we can do this
      // without flushing layout.
      this.panel.style.minWidth =
        window.windowUtils.getBoundsWithoutFlushing(this.panel).width + "px";
    } else {
      this.panel.style.minWidth = "";
    }

    if (val) {
      this._labelElement.value = val;
      this.panel.removeAttribute("inactive");
      MousePosTracker.addListener(this);
    } else {
      this.panel.setAttribute("inactive", "true");
      MousePosTracker.removeListener(this);
    }
  },

  getMouseTargetRect() {
    let container = this.panel.parentNode;
    let panelRect = window.windowUtils.getBoundsWithoutFlushing(this.panel);
    let containerRect = window.windowUtils.getBoundsWithoutFlushing(container);

    return {
      top: panelRect.top,
      bottom: panelRect.bottom,
      left: RTL_UI ? containerRect.right - panelRect.width : containerRect.left,
      right: RTL_UI
        ? containerRect.right
        : containerRect.left + panelRect.width,
    };
  },

  onMouseEnter() {
    this._mirror();
  },

  onMouseLeave() {
    this._mirror();
  },

  _mirror() {
    if (this.panel.hasAttribute("mirror")) {
      this.panel.removeAttribute("mirror");
    } else {
      this.panel.setAttribute("mirror", "true");
    }

    if (!this.panel.hasAttribute("sizelimit")) {
      this.panel.setAttribute("sizelimit", "true");
    }
  },
};

var TabBarVisibility = {
  _initialUpdateDone: false,

  update() {
    let toolbar = document.getElementById("TabsToolbar");
    let collapse = false;
    if (
      !gBrowser /* gBrowser isn't initialized yet */ ||
      gBrowser.tabs.length - gBrowser._removingTabs.length == 1
    ) {
      collapse = !window.toolbar.visible;
    }

    if (collapse == toolbar.collapsed && this._initialUpdateDone) {
      return;
    }
    this._initialUpdateDone = true;

    toolbar.collapsed = collapse;
    let navbar = document.getElementById("nav-bar");
    navbar.setAttribute("tabs-hidden", collapse);

    document.getElementById("menu_closeWindow").hidden = collapse;
    document
      .getElementById("menu_close")
      .setAttribute(
        "label",
        gTabBrowserBundle.GetStringFromName(
          collapse ? "tabs.close" : "tabs.closeTab"
        )
      );

    TabsInTitlebar.allowedBy("tabs-visible", !collapse);
  },
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
    let tab =
      aPopupMenu.triggerNode &&
      (aPopupMenu.triggerNode.tab || aPopupMenu.triggerNode.closest("tab"));

    this.contextTab = tab || gBrowser.selectedTab;

    let disabled = gBrowser.tabs.length == 1;
    let multiselectionContext = this.contextTab.multiselected;
    let tabCountInfo = JSON.stringify({
      tabCount: (multiselectionContext && gBrowser.multiSelectedTabsCount) || 1,
    });

    var menuItems = aPopupMenu.getElementsByAttribute(
      "tbattr",
      "tabbrowser-multiple"
    );
    for (let menuItem of menuItems) {
      menuItem.disabled = disabled;
    }

    if (this.contextTab.hasAttribute("customizemode")) {
      document.getElementById("context_openTabInWindow").disabled = true;
    }

    disabled = gBrowser.visibleTabs.length == 1;
    menuItems = aPopupMenu.getElementsByAttribute(
      "tbattr",
      "tabbrowser-multiple-visible"
    );
    for (let menuItem of menuItems) {
      menuItem.disabled = disabled;
    }

    // Session store
    document.getElementById("context_undoCloseTab").disabled =
      SessionStore.getClosedTabCount(window) == 0;

    // Only one of Reload_Tab/Reload_Selected_Tabs should be visible.
    document.getElementById("context_reloadTab").hidden = multiselectionContext;
    document.getElementById(
      "context_reloadSelectedTabs"
    ).hidden = !multiselectionContext;

    // Only one of pin/unpin/multiselect-pin/multiselect-unpin should be visible
    let contextPinTab = document.getElementById("context_pinTab");
    contextPinTab.hidden = this.contextTab.pinned || multiselectionContext;
    let contextUnpinTab = document.getElementById("context_unpinTab");
    contextUnpinTab.hidden = !this.contextTab.pinned || multiselectionContext;
    let contextPinSelectedTabs = document.getElementById(
      "context_pinSelectedTabs"
    );
    contextPinSelectedTabs.hidden =
      this.contextTab.pinned || !multiselectionContext;
    let contextUnpinSelectedTabs = document.getElementById(
      "context_unpinSelectedTabs"
    );
    contextUnpinSelectedTabs.hidden =
      !this.contextTab.pinned || !multiselectionContext;

    let contextMoveTabOptions = document.getElementById(
      "context_moveTabOptions"
    );
    contextMoveTabOptions.setAttribute("data-l10n-args", tabCountInfo);
    contextMoveTabOptions.disabled = gBrowser.allTabsSelected();
    let selectedTabs = gBrowser.selectedTabs;
    let contextMoveTabToEnd = document.getElementById("context_moveToEnd");
    let allSelectedTabsAdjacent = selectedTabs.every(
      (element, index, array) => {
        return array.length > index + 1
          ? element._tPos + 1 == array[index + 1]._tPos
          : true;
      }
    );
    let contextTabIsSelected = this.contextTab.multiselected;
    let visibleTabs = gBrowser.visibleTabs;
    let lastVisibleTab = visibleTabs[visibleTabs.length - 1];
    let tabsToMove = contextTabIsSelected ? selectedTabs : [this.contextTab];
    let lastTabToMove = tabsToMove[tabsToMove.length - 1];

    let isLastPinnedTab = false;
    if (lastTabToMove.pinned) {
      let sibling = gBrowser.tabContainer.findNextTab(lastTabToMove);
      isLastPinnedTab = !sibling || !sibling.pinned;
    }
    contextMoveTabToEnd.disabled =
      (lastTabToMove == lastVisibleTab || isLastPinnedTab) &&
      allSelectedTabsAdjacent;
    let contextMoveTabToStart = document.getElementById("context_moveToStart");
    let isFirstTab =
      tabsToMove[0] == visibleTabs[0] ||
      tabsToMove[0] == visibleTabs[gBrowser._numPinnedTabs];
    contextMoveTabToStart.disabled = isFirstTab && allSelectedTabsAdjacent;

    // Only one of "Duplicate Tab"/"Duplicate Tabs" should be visible.
    document.getElementById(
      "context_duplicateTab"
    ).hidden = multiselectionContext;
    document.getElementById(
      "context_duplicateTabs"
    ).hidden = !multiselectionContext;

    // Disable "Close Tabs to the Left/Right" if there are no tabs
    // preceding/following it.
    let closeTabsToTheStartItem = document.getElementById(
      "context_closeTabsToTheStart"
    );
    let noTabsToStart = !gBrowser.getTabsToTheStartFrom(this.contextTab).length;
    closeTabsToTheStartItem.disabled = noTabsToStart;
    let closeTabsToTheEndItem = document.getElementById(
      "context_closeTabsToTheEnd"
    );
    let noTabsToEnd = !gBrowser.getTabsToTheEndFrom(this.contextTab).length;
    closeTabsToTheEndItem.disabled = noTabsToEnd;

    // Disable "Close other Tabs" if there are no unpinned tabs.
    let unpinnedTabsToClose = multiselectionContext
      ? gBrowser.visibleTabs.filter(t => !t.multiselected && !t.pinned).length
      : gBrowser.visibleTabs.filter(t => t != this.contextTab && !t.pinned)
          .length;
    let closeOtherTabsItem = document.getElementById("context_closeOtherTabs");
    closeOtherTabsItem.disabled = unpinnedTabsToClose < 1;

    // Update the close item with how many tabs will close.
    document
      .getElementById("context_closeTab")
      .setAttribute("data-l10n-args", tabCountInfo);

    // Disable "Close Multiple Tabs" if all sub menuitems are disabled
    document.getElementById("context_closeTabOptions").disabled =
      closeTabsToTheStartItem.disabled &&
      closeTabsToTheEndItem.disabled &&
      closeOtherTabsItem.disabled;

    // Hide "Bookmark Tab" for multiselection.
    // Update its state if visible.
    let bookmarkTab = document.getElementById("context_bookmarkTab");
    bookmarkTab.hidden = multiselectionContext;

    // Show "Bookmark Selected Tabs" in a multiselect context and hide it otherwise.
    let bookmarkMultiSelectedTabs = document.getElementById(
      "context_bookmarkSelectedTabs"
    );
    bookmarkMultiSelectedTabs.hidden = !multiselectionContext;

    let toggleMute = document.getElementById("context_toggleMuteTab");
    let toggleMultiSelectMute = document.getElementById(
      "context_toggleMuteSelectedTabs"
    );

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
      toggleMultiSelectMute.label = gNavigatorBundle.getString(
        "playTabs.label"
      );
      toggleMultiSelectMute.accessKey = gNavigatorBundle.getString(
        "playTabs.accesskey"
      );
    } else if (this.contextTab.hasAttribute("muted")) {
      toggleMultiSelectMute.label = gNavigatorBundle.getString(
        "unmuteSelectedTabs2.label"
      );
      toggleMultiSelectMute.accessKey = gNavigatorBundle.getString(
        "unmuteSelectedTabs2.accesskey"
      );
    } else {
      toggleMultiSelectMute.label = gNavigatorBundle.getString(
        "muteSelectedTabs2.label"
      );
      toggleMultiSelectMute.accessKey = gNavigatorBundle.getString(
        "muteSelectedTabs2.accesskey"
      );
    }

    this.contextTab.toggleMuteMenuItem = toggleMute;
    this.contextTab.toggleMultiSelectMuteMenuItem = toggleMultiSelectMute;
    this._updateToggleMuteMenuItems(this.contextTab);

    let selectAllTabs = document.getElementById("context_selectAllTabs");
    selectAllTabs.disabled = gBrowser.allTabsSelected();

    this.contextTab.addEventListener("TabAttrModified", this);
    aPopupMenu.addEventListener("popuphiding", this);

    gSync.updateTabContextMenu(aPopupMenu, this.contextTab);

    document.getElementById("context_reopenInContainer").hidden =
      !Services.prefs.getBoolPref("privacy.userContext.enabled", false) ||
      PrivateBrowsingUtils.isWindowPrivate(window);

    this.updateShareURLMenuItem();
  },

  handleEvent(aEvent) {
    switch (aEvent.type) {
      case "command":
        this.onShareURLCommand(aEvent);
        break;
      case "popuphiding":
        this.onPopupHiding(aEvent);
        break;
      case "popupshowing":
        this.onPopupShowing(aEvent);
        break;
      case "TabAttrModified":
        let tab = aEvent.target;
        this._updateToggleMuteMenuItems(tab, attr =>
          aEvent.detail.changed.includes(attr)
        );
        break;
    }
  },

  onPopupHiding(event) {
    // We don't want to rebuild the contents of the "Share" menupopup if only its submenu is
    // hidden. So only clear its "data-initialized" attribute when the main context menu is
    // hidden.
    if (event.target.id === "tabContextMenu") {
      let menupopup = document.getElementById("context_shareTabURL_popup");
      menupopup?.removeAttribute("data-initialized");
    }

    gBrowser.removeEventListener("TabAttrModified", this);
    event.target.removeEventListener("popuphiding", this);
  },

  onPopupShowing(event) {
    let menupopup = document.getElementById("context_shareTabURL_popup");
    if (
      event.target === menupopup &&
      !menupopup.hasAttribute("data-initialized")
    ) {
      this.initializeShareURLPopup();
    }
  },

  createReopenInContainerMenu(event) {
    createUserContextMenu(event, {
      isContextMenu: true,
      excludeUserContextId: this.contextTab.getAttribute("usercontextid"),
    });
  },
  duplicateSelectedTabs() {
    let tabsToDuplicate = gBrowser.selectedTabs;
    let newIndex = tabsToDuplicate[tabsToDuplicate.length - 1]._tPos + 1;
    for (let tab of tabsToDuplicate) {
      let newTab = SessionStore.duplicateTab(window, tab);
      gBrowser.moveTabTo(newTab, newIndex++);
    }
  },
  reopenInContainer(event) {
    let userContextId = parseInt(
      event.target.getAttribute("data-usercontextid")
    );
    let reopenedTabs = this.contextTab.multiselected
      ? gBrowser.selectedTabs
      : [this.contextTab];

    for (let tab of reopenedTabs) {
      if (tab.getAttribute("usercontextid") == userContextId) {
        continue;
      }

      /* Create a triggering principal that is able to load the new tab
         For content principals that are about: chrome: or resource: we need system to load them.
         Anything other than system principal needs to have the new userContextId.
      */
      let triggeringPrincipal;

      if (tab.linkedPanel) {
        triggeringPrincipal = tab.linkedBrowser.contentPrincipal;
      } else {
        // For lazy tab browsers, get the original principal
        // from SessionStore
        let tabState = JSON.parse(SessionStore.getTabState(tab));
        try {
          triggeringPrincipal = E10SUtils.deserializePrincipal(
            tabState.triggeringPrincipal_base64
          );
        } catch (ex) {
          continue;
        }
      }

      if (!triggeringPrincipal || triggeringPrincipal.isNullPrincipal) {
        // Ensure that we have a null principal if we couldn't
        // deserialize it (for lazy tab browsers) ...
        // This won't always work however is safe to use.
        triggeringPrincipal = Services.scriptSecurityManager.createNullPrincipal(
          { userContextId }
        );
      } else if (triggeringPrincipal.isContentPrincipal) {
        triggeringPrincipal = Services.scriptSecurityManager.principalWithOA(
          triggeringPrincipal,
          {
            userContextId,
          }
        );
      }

      let newTab = gBrowser.addTab(tab.linkedBrowser.currentURI.spec, {
        userContextId,
        pinned: tab.pinned,
        index: tab._tPos + 1,
        triggeringPrincipal,
      });

      if (gBrowser.selectedTab == tab) {
        gBrowser.selectedTab = newTab;
      }
      if (tab.muted && !newTab.muted) {
        newTab.toggleMuteAudio(tab.muteReason);
      }
    }
  },

  closeContextTabs(event) {
    if (this.contextTab.multiselected) {
      gBrowser.removeMultiSelectedTabs();
    } else {
      gBrowser.removeTab(this.contextTab, { animate: true });
    }
  },

  updateShareURLMenuItem() {
    // We only support "share URL" on macOS and on Windows 10:
    if (
      !gProton ||
      !(
        AppConstants.platform == "macosx" ||
        AppConstants.isPlatformAndVersionAtLeast("win", "6.4")
      )
    ) {
      return;
    }

    let shareURL = document.getElementById("context_shareTabURL");

    if (!shareURL) {
      shareURL = this.createShareURLMenuItem();
    }

    // Don't show the menuitem on non-shareable URLs.
    let browser = this.contextTab.linkedBrowser;
    shareURL.hidden = !BrowserUtils.isShareableURL(browser.currentURI);
  },

  /**
   * Creates and returns the "Share" menu item.
   */
  createShareURLMenuItem() {
    let menu = document.getElementById("tabContextMenu");
    let shareURL = null;

    if (AppConstants.platform == "win") {
      shareURL = this.buildShareURLItem();
    } else if (AppConstants.platform == "macosx") {
      shareURL = this.buildShareURLMenu();
    }

    let sendTabContext = document.getElementById("context_sendTabToDevice");
    menu.insertBefore(shareURL, sendTabContext.nextSibling);
    return shareURL;
  },

  /**
   * Returns a menu item specifically for accessing Windows sharing services.
   */
  buildShareURLItem() {
    let shareURLMenuItem = document.createXULElement("menuitem");
    shareURLMenuItem.setAttribute("id", "context_shareTabURL");
    document.l10n.setAttributes(shareURLMenuItem, "tab-context-share-url");

    shareURLMenuItem.addEventListener("command", this);
    return shareURLMenuItem;
  },

  /**
   * Returns a menu specifically for accessing macOSx sharing services .
   */
  buildShareURLMenu() {
    let menu = document.createXULElement("menu");
    menu.id = "context_shareTabURL";
    document.l10n.setAttributes(menu, "tab-context-share-url");

    let menuPopup = document.createXULElement("menupopup");
    menuPopup.id = "context_shareTabURL_popup";
    menuPopup.addEventListener("popupshowing", this);
    menu.appendChild(menuPopup);

    return menu;
  },

  /**
   * Populates the "Share" menupopup on macOSx.
   */
  initializeShareURLPopup() {
    if (AppConstants.platform !== "macosx") {
      return;
    }

    let menuPopup = document.getElementById("context_shareTabURL_popup");

    // Empty menupopup
    while (menuPopup.firstChild) {
      menuPopup.firstChild.remove();
    }

    let url = this.contextTab.linkedBrowser.currentURI;
    let sharingService = gBrowser.MacSharingService;
    let currentURI = gURLBar.makeURIReadable(url).displaySpec;
    let services = sharingService.getSharingProviders(currentURI);

    services.forEach(share => {
      let item = document.createXULElement("menuitem");
      item.classList.add("menuitem-iconic");
      item.setAttribute("label", share.menuItemTitle);
      item.setAttribute("share-name", share.name);
      item.setAttribute("image", share.image);
      menuPopup.appendChild(item);
    });
    let moreItem = document.createXULElement("menuitem");
    document.l10n.setAttributes(moreItem, "tab-context-share-more");
    moreItem.classList.add("menuitem-iconic", "share-more-button");
    menuPopup.appendChild(moreItem);

    menuPopup.addEventListener("command", this);
    menuPopup.setAttribute("data-initialized", true);
  },

  onShareURLCommand(event) {
    // Only call sharing services for the "Share" context menu item. These
    // services are accessed from a submenu popup for MacOS or the "Share"
    // menu item for Windows.
    if (
      event.target.parentNode.id !== "context_shareTabURL_popup" &&
      event.target.id !== "context_shareTabURL"
    ) {
      return;
    }

    let browser = this.contextTab.linkedBrowser;
    let currentURI = gURLBar.makeURIReadable(browser.currentURI).displaySpec;

    if (AppConstants.platform == "win") {
      WindowsUIUtils.shareUrl(currentURI, browser.contentTitle);
      return;
    }

    // On macOSX platforms
    let sharingService = gBrowser.MacSharingService;
    let shareName = event.target.getAttribute("share-name");

    if (shareName) {
      sharingService.shareUrl(shareName, currentURI, browser.contentTitle);
    } else if (event.target.classList.contains("share-more-button")) {
      sharingService.openSharingPreferences();
    }
  },
};
