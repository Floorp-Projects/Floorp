/* -*- Mode: indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set sts=2 sw=2 et tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

// This file provides some useful code for the |tabs| and |windows|
// modules. All of the code is installed on |global|, which is a scope
// shared among the different ext-*.js scripts.

ChromeUtils.defineESModuleGetters(this, {
  AboutReaderParent: "resource:///actors/AboutReaderParent.sys.mjs",
  BrowserWindowTracker: "resource:///modules/BrowserWindowTracker.sys.mjs",
  PrivateBrowsingUtils: "resource://gre/modules/PrivateBrowsingUtils.sys.mjs",
  PromiseUtils: "resource://gre/modules/PromiseUtils.sys.mjs",
});

var { ExtensionError } = ExtensionUtils;

var { defineLazyGetter } = ExtensionCommon;

const READER_MODE_PREFIX = "about:reader";

let tabTracker;
let windowTracker;

function isPrivateTab(nativeTab) {
  return PrivateBrowsingUtils.isBrowserPrivate(nativeTab.linkedBrowser);
}

/* eslint-disable mozilla/balanced-listeners */
extensions.on("uninstalling", (msg, extension) => {
  if (extension.uninstallURL) {
    let browser = windowTracker.topWindow.gBrowser;
    browser.addTab(extension.uninstallURL, {
      relatedToCurrent: true,
      triggeringPrincipal: Services.scriptSecurityManager.createNullPrincipal(
        {}
      ),
    });
  }
});

extensions.on("page-shutdown", (type, context) => {
  if (context.viewType == "tab") {
    if (context.extension.id !== context.xulBrowser.contentPrincipal.addonId) {
      // Only close extension tabs.
      // This check prevents about:addons from closing when it contains a
      // WebExtension as an embedded inline options page.
      return;
    }
    let { gBrowser } = context.xulBrowser.ownerGlobal;
    if (gBrowser && gBrowser.getTabForBrowser) {
      let nativeTab = gBrowser.getTabForBrowser(context.xulBrowser);
      if (nativeTab) {
        gBrowser.removeTab(nativeTab);
      }
    }
  }
});
/* eslint-enable mozilla/balanced-listeners */

global.openOptionsPage = extension => {
  let window = windowTracker.topWindow;
  if (!window) {
    return Promise.reject({ message: "No browser window available" });
  }

  if (extension.manifest.options_ui.open_in_tab) {
    window.switchToTabHavingURI(extension.manifest.options_ui.page, true, {
      triggeringPrincipal: extension.principal,
    });
    return Promise.resolve();
  }

  let viewId = `addons://detail/${encodeURIComponent(
    extension.id
  )}/preferences`;

  return window.BrowserOpenAddonsMgr(viewId);
};

global.makeWidgetId = id => {
  id = id.toLowerCase();
  // FIXME: This allows for collisions.
  return id.replace(/[^a-z0-9_-]/g, "_");
};

global.clickModifiersFromEvent = event => {
  const map = {
    shiftKey: "Shift",
    altKey: "Alt",
    metaKey: "Command",
    ctrlKey: "Ctrl",
  };
  let modifiers = Object.keys(map)
    .filter(key => event[key])
    .map(key => map[key]);

  if (event.ctrlKey && AppConstants.platform === "macosx") {
    modifiers.push("MacCtrl");
  }

  return modifiers;
};

global.waitForTabLoaded = (tab, url) => {
  return new Promise(resolve => {
    windowTracker.addListener("progress", {
      onLocationChange(browser, webProgress, request, locationURI, flags) {
        if (
          webProgress.isTopLevel &&
          browser.ownerGlobal.gBrowser.getTabForBrowser(browser) == tab &&
          (!url || locationURI.spec == url)
        ) {
          windowTracker.removeListener("progress", this);
          resolve();
        }
      },
    });
  });
};

global.replaceUrlInTab = (gBrowser, tab, uri) => {
  let loaded = waitForTabLoaded(tab, uri.spec);
  gBrowser.loadURI(uri, {
    flags: Ci.nsIWebNavigation.LOAD_FLAGS_REPLACE_HISTORY,
    triggeringPrincipal: Services.scriptSecurityManager.getSystemPrincipal(), // This is safe from this functions usage however it would be preferred not to dot his.
  });
  return loaded;
};

/**
 * Manages tab-specific and window-specific context data, and dispatches
 * tab select events across all windows.
 */
global.TabContext = class extends EventEmitter {
  /**
   * @param {Function} getDefaultPrototype
   *        Provides the prototype of the context value for a tab or window when there is none.
   *        Called with a XULElement or ChromeWindow argument.
   *        Should return an object or null.
   */
  constructor(getDefaultPrototype) {
    super();

    this.getDefaultPrototype = getDefaultPrototype;

    this.tabData = new WeakMap();

    windowTracker.addListener("progress", this);
    windowTracker.addListener("TabSelect", this);

    this.tabAdopted = this.tabAdopted.bind(this);
    tabTracker.on("tab-adopted", this.tabAdopted);
  }

  /**
   * Returns the context data associated with `keyObject`.
   *
   * @param {XULElement|ChromeWindow} keyObject
   *        Browser tab or browser chrome window.
   * @returns {object}
   */
  get(keyObject) {
    if (!this.tabData.has(keyObject)) {
      let data = Object.create(this.getDefaultPrototype(keyObject));
      this.tabData.set(keyObject, data);
    }

    return this.tabData.get(keyObject);
  }

  /**
   * Clears the context data associated with `keyObject`.
   *
   * @param {XULElement|ChromeWindow} keyObject
   *        Browser tab or browser chrome window.
   */
  clear(keyObject) {
    this.tabData.delete(keyObject);
  }

  handleEvent(event) {
    if (event.type == "TabSelect") {
      let nativeTab = event.target;
      this.emit("tab-select", nativeTab);
      this.emit("location-change", nativeTab);
    }
  }

  onLocationChange(browser, webProgress, request, locationURI, flags) {
    if (!webProgress.isTopLevel) {
      // Only pageAction and browserAction are consuming the "location-change" event
      // to update their per-tab status, and they should only do so in response of
      // location changes related to the top level frame (See Bug 1493470 for a rationale).
      return;
    }
    let gBrowser = browser.ownerGlobal.gBrowser;
    let tab = gBrowser.getTabForBrowser(browser);
    // fromBrowse will be false in case of e.g. a hash change or history.pushState
    let fromBrowse = !(
      flags & Ci.nsIWebProgressListener.LOCATION_CHANGE_SAME_DOCUMENT
    );
    this.emit("location-change", tab, fromBrowse);
  }

  /**
   * Persists context data when a tab is moved between windows.
   *
   * @param {string} eventType
   *        Event type, should be "tab-adopted".
   * @param {NativeTab} adoptingTab
   *        The tab which is being opened and adopting `adoptedTab`.
   * @param {NativeTab} adoptedTab
   *        The tab which is being closed and adopted by `adoptingTab`.
   */
  tabAdopted(eventType, adoptingTab, adoptedTab) {
    if (!this.tabData.has(adoptedTab)) {
      return;
    }
    // Create a new object (possibly with different inheritance) when a tab is moved
    // into a new window. But then reassign own properties from the old object.
    let newData = this.get(adoptingTab);
    let oldData = this.tabData.get(adoptedTab);
    this.tabData.delete(adoptedTab);
    Object.assign(newData, oldData);
  }

  /**
   * Makes the TabContext instance stop emitting events.
   */
  shutdown() {
    windowTracker.removeListener("progress", this);
    windowTracker.removeListener("TabSelect", this);
    tabTracker.off("tab-adopted", this.tabAdopted);
  }
};

class WindowTracker extends WindowTrackerBase {
  addProgressListener(window, listener) {
    window.gBrowser.addTabsProgressListener(listener);
  }

  removeProgressListener(window, listener) {
    window.gBrowser.removeTabsProgressListener(listener);
  }

  /**
   * @param {BaseContext} context
   *        The extension context
   * @returns {DOMWindow|null} topNormalWindow
   *        The currently active, or topmost, browser window, or null if no
   *        browser window is currently open.
   *        Will return the topmost "normal" (i.e., not popup) window.
   */
  getTopNormalWindow(context) {
    let options = { allowPopups: false };
    if (!context.privateBrowsingAllowed) {
      options.private = false;
    }
    return BrowserWindowTracker.getTopWindow(options);
  }
}

class TabTracker extends TabTrackerBase {
  constructor() {
    super();

    this._tabs = new WeakMap();
    this._browsers = new WeakMap();
    this._tabIds = new Map();
    this._nextId = 1;
    this._deferredTabOpenEvents = new WeakMap();

    this._handleTabDestroyed = this._handleTabDestroyed.bind(this);
  }

  init() {
    if (this.initialized) {
      return;
    }
    this.initialized = true;

    this.adoptedTabs = new WeakSet();

    this._handleWindowOpen = this._handleWindowOpen.bind(this);
    this._handleWindowClose = this._handleWindowClose.bind(this);

    windowTracker.addListener("TabClose", this);
    windowTracker.addListener("TabOpen", this);
    windowTracker.addListener("TabSelect", this);
    windowTracker.addListener("TabMultiSelect", this);
    windowTracker.addOpenListener(this._handleWindowOpen);
    windowTracker.addCloseListener(this._handleWindowClose);

    AboutReaderParent.addMessageListener("Reader:UpdateReaderButton", this);

    /* eslint-disable mozilla/balanced-listeners */
    this.on("tab-detached", this._handleTabDestroyed);
    this.on("tab-removed", this._handleTabDestroyed);
    /* eslint-enable mozilla/balanced-listeners */
  }

  getId(nativeTab) {
    let id = this._tabs.get(nativeTab);
    if (id) {
      return id;
    }

    this.init();

    id = this._nextId++;
    this.setId(nativeTab, id);
    return id;
  }

  getBrowserTabId(browser) {
    let id = this._browsers.get(browser);
    if (id) {
      return id;
    }

    let tab = browser.ownerGlobal.gBrowser.getTabForBrowser(browser);
    if (tab) {
      id = this.getId(tab);
      this._browsers.set(browser, id);
      return id;
    }
    return -1;
  }

  setId(nativeTab, id) {
    if (!nativeTab.parentNode) {
      throw new Error("Cannot attach ID to a destroyed tab.");
    }
    if (nativeTab.ownerGlobal.closed) {
      throw new Error("Cannot attach ID to a tab in a closed window.");
    }

    this._tabs.set(nativeTab, id);
    if (nativeTab.linkedBrowser) {
      this._browsers.set(nativeTab.linkedBrowser, id);
    }
    this._tabIds.set(id, nativeTab);
  }

  /**
   * Handles tab adoption when a tab is moved between windows.
   * Ensures the new tab will have the same ID as the old one, and
   * emits "tab-adopted", "tab-detached" and "tab-attached" events.
   *
   * @param {NativeTab} adoptingTab
   *        The tab which is being opened and adopting `adoptedTab`.
   * @param {NativeTab} adoptedTab
   *        The tab which is being closed and adopted by `adoptingTab`.
   */
  adopt(adoptingTab, adoptedTab) {
    if (this.adoptedTabs.has(adoptedTab)) {
      // The adoption has already been handled.
      return;
    }
    this.adoptedTabs.add(adoptedTab);
    let tabId = this.getId(adoptedTab);
    this.setId(adoptingTab, tabId);
    this.emit("tab-adopted", adoptingTab, adoptedTab);
    if (this.has("tab-detached")) {
      let nativeTab = adoptedTab;
      let adoptedBy = adoptingTab;
      let oldWindowId = windowTracker.getId(nativeTab.ownerGlobal);
      let oldPosition = nativeTab._tPos;
      this.emit("tab-detached", {
        nativeTab,
        adoptedBy,
        tabId,
        oldWindowId,
        oldPosition,
      });
    }
    if (this.has("tab-attached")) {
      let nativeTab = adoptingTab;
      let newWindowId = windowTracker.getId(nativeTab.ownerGlobal);
      let newPosition = nativeTab._tPos;
      this.emit("tab-attached", {
        nativeTab,
        tabId,
        newWindowId,
        newPosition,
      });
    }
  }

  _handleTabDestroyed(event, { nativeTab }) {
    let id = this._tabs.get(nativeTab);
    if (id) {
      this._tabs.delete(nativeTab);
      if (this._tabIds.get(id) === nativeTab) {
        this._tabIds.delete(id);
      }
    }
  }

  /**
   * Returns the XUL <tab> element associated with the given tab ID. If no tab
   * with the given ID exists, and no default value is provided, an error is
   * raised, belonging to the scope of the given context.
   *
   * @param {integer} tabId
   *        The ID of the tab to retrieve.
   * @param {*} default_
   *        The value to return if no tab exists with the given ID.
   * @returns {Element<tab>}
   *        A XUL <tab> element.
   */
  getTab(tabId, default_ = undefined) {
    let nativeTab = this._tabIds.get(tabId);
    if (nativeTab) {
      return nativeTab;
    }
    if (default_ !== undefined) {
      return default_;
    }
    throw new ExtensionError(`Invalid tab ID: ${tabId}`);
  }

  /**
   * Sets the opener of `tab` to the ID `openerTab`. Both tabs must be in the
   * same window, or this function will throw a type error.
   *
   * @param {Element} tab The tab for which to set the owner.
   * @param {Element} openerTab The opener of <tab>.
   */
  setOpener(tab, openerTab) {
    if (tab.ownerDocument !== openerTab.ownerDocument) {
      throw new Error("Tab must be in the same window as its opener");
    }
    tab.openerTab = openerTab;
  }

  deferredForTabOpen(nativeTab) {
    let deferred = this._deferredTabOpenEvents.get(nativeTab);
    if (!deferred) {
      deferred = PromiseUtils.defer();
      this._deferredTabOpenEvents.set(nativeTab, deferred);
      deferred.promise.then(() => {
        this._deferredTabOpenEvents.delete(nativeTab);
      });
    }
    return deferred;
  }

  async maybeWaitForTabOpen(nativeTab) {
    let deferred = this._deferredTabOpenEvents.get(nativeTab);
    return deferred && deferred.promise;
  }

  /**
   * @param {Event} event
   *        The DOM Event to handle.
   * @private
   */
  handleEvent(event) {
    let nativeTab = event.target;

    switch (event.type) {
      case "TabOpen":
        let { adoptedTab } = event.detail;
        if (adoptedTab) {
          // This tab is being created to adopt a tab from a different window.
          // Handle the adoption.
          this.adopt(nativeTab, adoptedTab);
        } else {
          // Save the size of the current tab, since the newly-created tab will
          // likely be active by the time the promise below resolves and the
          // event is dispatched.
          const currentTab = nativeTab.ownerGlobal.gBrowser.selectedTab;
          const { frameLoader } = currentTab.linkedBrowser;
          const currentTabSize = {
            width: frameLoader.lazyWidth,
            height: frameLoader.lazyHeight,
          };

          // We need to delay sending this event until the next tick, since the
          // tab can become selected immediately after "TabOpen", then onCreated
          // should be fired with `active: true`.
          let deferred = this.deferredForTabOpen(event.originalTarget);
          Promise.resolve().then(() => {
            deferred.resolve();
            if (!event.originalTarget.parentNode) {
              // If the tab is already be destroyed, do nothing.
              return;
            }
            this.emitCreated(event.originalTarget, currentTabSize);
          });
        }
        break;

      case "TabClose":
        let { adoptedBy } = event.detail;
        if (adoptedBy) {
          // This tab is being closed because it was adopted by a new window.
          // Handle the adoption in case it was created as the first tab of a
          // new window, and did not have an `adoptedTab` detail when it was
          // opened.
          this.adopt(adoptedBy, nativeTab);
        } else {
          this.emitRemoved(nativeTab, false);
        }
        break;

      case "TabSelect":
        // Because we are delaying calling emitCreated above, we also need to
        // delay sending this event because it shouldn't fire before onCreated.
        this.maybeWaitForTabOpen(nativeTab).then(() => {
          if (!nativeTab.parentNode) {
            // If the tab is already be destroyed, do nothing.
            return;
          }
          this.emitActivated(nativeTab, event.detail.previousTab);
        });
        break;

      case "TabMultiSelect":
        if (this.has("tabs-highlighted")) {
          // Because we are delaying calling emitCreated above, we also need to
          // delay sending this event because it shouldn't fire before onCreated.
          // event.target is gBrowser, so we don't use maybeWaitForTabOpen.
          Promise.resolve().then(() => {
            this.emitHighlighted(event.target.ownerGlobal);
          });
        }
        break;
    }
  }

  /**
   * @param {object} message
   *        The message to handle.
   * @private
   */
  receiveMessage(message) {
    switch (message.name) {
      case "Reader:UpdateReaderButton":
        if (message.data && message.data.isArticle !== undefined) {
          this.emit("tab-isarticle", message);
        }
        break;
    }
  }

  /**
   * A private method which is called whenever a new browser window is opened,
   * and dispatches the necessary events for it.
   *
   * @param {DOMWindow} window
   *        The window being opened.
   * @private
   */
  _handleWindowOpen(window) {
    const tabToAdopt = window.gBrowserInit.getTabToAdopt();
    if (tabToAdopt) {
      // Note that this event handler depends on running before the
      // delayed startup code in browser.js, which is currently triggered
      // by the first MozAfterPaint event. That code handles finally
      // adopting the tab, and clears it from the arguments list in the
      // process, so if we run later than it, we're too late.
      let adoptedBy = window.gBrowser.tabs[0];
      this.adopt(adoptedBy, tabToAdopt);
    } else {
      for (let nativeTab of window.gBrowser.tabs) {
        this.emitCreated(nativeTab);
      }

      // emitActivated to trigger tab.onActivated/tab.onHighlighted for a newly opened window.
      this.emitActivated(window.gBrowser.tabs[0]);
      if (this.has("tabs-highlighted")) {
        this.emitHighlighted(window);
      }
    }
  }

  /**
   * A private method which is called whenever a browser window is closed,
   * and dispatches the necessary events for it.
   *
   * @param {DOMWindow} window
   *        The window being closed.
   * @private
   */
  _handleWindowClose(window) {
    for (let nativeTab of window.gBrowser.tabs) {
      if (!this.adoptedTabs.has(nativeTab)) {
        this.emitRemoved(nativeTab, true);
      }
    }
  }

  /**
   * Emits a "tab-activated" event for the given tab element.
   *
   * @param {NativeTab} nativeTab
   *        The tab element which has been activated.
   * @param {NativeTab} previousTab
   *        The tab element which was previously activated.
   * @private
   */
  emitActivated(nativeTab, previousTab = undefined) {
    let previousTabIsPrivate, previousTabId;
    if (previousTab && !previousTab.closing) {
      previousTabId = this.getId(previousTab);
      previousTabIsPrivate = isPrivateTab(previousTab);
    }
    this.emit("tab-activated", {
      tabId: this.getId(nativeTab),
      previousTabId,
      previousTabIsPrivate,
      windowId: windowTracker.getId(nativeTab.ownerGlobal),
      nativeTab,
    });
  }

  /**
   * Emits a "tabs-highlighted" event for the given tab element.
   *
   * @param {ChromeWindow} window
   *        The window in which the active tab or the set of multiselected tabs changed.
   * @private
   */
  emitHighlighted(window) {
    let tabIds = window.gBrowser.selectedTabs.map(tab => this.getId(tab));
    let windowId = windowTracker.getId(window);
    this.emit("tabs-highlighted", {
      tabIds,
      windowId,
    });
  }

  /**
   * Emits a "tab-created" event for the given tab element.
   *
   * @param {NativeTab} nativeTab
   *        The tab element which is being created.
   * @param {object} [currentTabSize]
   *        The size of the tab element for the currently active tab.
   * @private
   */
  emitCreated(nativeTab, currentTabSize) {
    this.emit("tab-created", {
      nativeTab,
      currentTabSize,
    });
  }

  /**
   * Emits a "tab-removed" event for the given tab element.
   *
   * @param {NativeTab} nativeTab
   *        The tab element which is being removed.
   * @param {boolean} isWindowClosing
   *        True if the tab is being removed because the browser window is
   *        closing.
   * @private
   */
  emitRemoved(nativeTab, isWindowClosing) {
    let windowId = windowTracker.getId(nativeTab.ownerGlobal);
    let tabId = this.getId(nativeTab);

    this.emit("tab-removed", {
      nativeTab,
      tabId,
      windowId,
      isWindowClosing,
    });
  }

  getBrowserData(browser) {
    let window = browser.ownerGlobal;
    if (!window) {
      return {
        tabId: -1,
        windowId: -1,
      };
    }
    let { gBrowser } = window;
    // Some non-browser windows have gBrowser but not getTabForBrowser!
    if (!gBrowser || !gBrowser.getTabForBrowser) {
      if (window.top.document.documentURI === "about:addons") {
        // When we're loaded into a <browser> inside about:addons, we need to go up
        // one more level.
        browser = window.docShell.chromeEventHandler;

        ({ gBrowser } = browser.ownerGlobal);
      } else {
        return {
          tabId: -1,
          windowId: -1,
        };
      }
    }

    return {
      tabId: this.getBrowserTabId(browser),
      windowId: windowTracker.getId(browser.ownerGlobal),
    };
  }

  get activeTab() {
    let window = windowTracker.topWindow;
    if (window && window.gBrowser) {
      return window.gBrowser.selectedTab;
    }
    return null;
  }
}

windowTracker = new WindowTracker();
tabTracker = new TabTracker();

Object.assign(global, { tabTracker, windowTracker });

class Tab extends TabBase {
  get _favIconUrl() {
    return this.window.gBrowser.getIcon(this.nativeTab);
  }

  get attention() {
    return this.nativeTab.hasAttribute("attention");
  }

  get audible() {
    return this.nativeTab.soundPlaying;
  }

  get autoDiscardable() {
    return !this.nativeTab.undiscardable;
  }

  get browser() {
    return this.nativeTab.linkedBrowser;
  }

  get discarded() {
    return !this.nativeTab.linkedPanel;
  }

  get frameLoader() {
    // If we don't have a frameLoader yet, just return a dummy with no width and
    // height.
    return super.frameLoader || { lazyWidth: 0, lazyHeight: 0 };
  }

  get hidden() {
    return this.nativeTab.hidden;
  }

  get sharingState() {
    return this.window.gBrowser.getTabSharingState(this.nativeTab);
  }

  get cookieStoreId() {
    return getCookieStoreIdForTab(this, this.nativeTab);
  }

  get openerTabId() {
    let opener = this.nativeTab.openerTab;
    if (
      opener &&
      opener.parentNode &&
      opener.ownerDocument == this.nativeTab.ownerDocument
    ) {
      return tabTracker.getId(opener);
    }
    return null;
  }

  get height() {
    return this.frameLoader.lazyHeight;
  }

  get index() {
    return this.nativeTab._tPos;
  }

  get mutedInfo() {
    let { nativeTab } = this;

    let mutedInfo = { muted: nativeTab.muted };
    if (nativeTab.muteReason === null) {
      mutedInfo.reason = "user";
    } else if (nativeTab.muteReason) {
      mutedInfo.reason = "extension";
      mutedInfo.extensionId = nativeTab.muteReason;
    }

    return mutedInfo;
  }

  get lastAccessed() {
    return this.nativeTab.lastAccessed;
  }

  get pinned() {
    return this.nativeTab.pinned;
  }

  get active() {
    return this.nativeTab.selected;
  }

  get highlighted() {
    let { selected, multiselected } = this.nativeTab;
    return selected || multiselected;
  }

  get status() {
    if (this.nativeTab.getAttribute("busy") === "true") {
      return "loading";
    }
    return "complete";
  }

  get width() {
    return this.frameLoader.lazyWidth;
  }

  get window() {
    return this.nativeTab.ownerGlobal;
  }

  get windowId() {
    return windowTracker.getId(this.window);
  }

  get isArticle() {
    return this.nativeTab.linkedBrowser.isArticle;
  }

  get isInReaderMode() {
    return this.url && this.url.startsWith(READER_MODE_PREFIX);
  }

  get successorTabId() {
    const { successor } = this.nativeTab;
    return successor ? tabTracker.getId(successor) : -1;
  }

  /**
   * Converts session store data to an object compatible with the return value
   * of the convert() method, representing that data.
   *
   * @param {Extension} extension
   *        The extension for which to convert the data.
   * @param {object} tabData
   *        Session store data for a closed tab, as returned by
   *        `SessionStore.getClosedTabData()`.
   * @param {DOMWindow} [window = null]
   *        The browser window which the tab belonged to before it was closed.
   *        May be null if the window the tab belonged to no longer exists.
   *
   * @returns {object}
   * @static
   */
  static convertFromSessionStoreClosedData(extension, tabData, window = null) {
    let result = {
      sessionId: String(tabData.closedId),
      index: tabData.pos ? tabData.pos : 0,
      windowId: window && windowTracker.getId(window),
      highlighted: false,
      active: false,
      pinned: false,
      hidden: tabData.state ? tabData.state.hidden : tabData.hidden,
      incognito: Boolean(tabData.state && tabData.state.isPrivate),
      lastAccessed: tabData.state
        ? tabData.state.lastAccessed
        : tabData.lastAccessed,
    };

    let entries = tabData.state ? tabData.state.entries : tabData.entries;
    let lastTabIndex = tabData.state ? tabData.state.index : tabData.index;

    // Tab may have empty history.
    if (entries.length) {
      // We need to take lastTabIndex - 1 because the index in the tab data is
      // 1-based rather than 0-based.
      let entry = entries[lastTabIndex - 1];

      // tabData is a representation of a tab, as stored in the session data,
      // and given that is not a real nativeTab, we only need to check if the extension
      // has the "tabs" or host permission (because tabData represents a closed tab,
      // and so we already know that it can't be the activeTab).
      if (
        extension.hasPermission("tabs") ||
        extension.allowedOrigins.matches(entry.url)
      ) {
        result.url = entry.url;
        result.title = entry.title;
        if (tabData.image) {
          result.favIconUrl = tabData.image;
        }
      }
    }

    return result;
  }
}

class Window extends WindowBase {
  /**
   * Update the geometry of the browser window.
   *
   * @param {object} options
   *        An object containing new values for the window's geometry.
   * @param {integer} [options.left]
   *        The new pixel distance of the left side of the browser window from
   *        the left of the screen.
   * @param {integer} [options.top]
   *        The new pixel distance of the top side of the browser window from
   *        the top of the screen.
   * @param {integer} [options.width]
   *        The new pixel width of the window.
   * @param {integer} [options.height]
   *        The new pixel height of the window.
   */
  updateGeometry(options) {
    let { window } = this;

    if (options.left !== null || options.top !== null) {
      let left = options.left !== null ? options.left : window.screenX;
      let top = options.top !== null ? options.top : window.screenY;
      window.moveTo(left, top);
    }

    if (options.width !== null || options.height !== null) {
      let width = options.width !== null ? options.width : window.outerWidth;
      let height =
        options.height !== null ? options.height : window.outerHeight;
      window.resizeTo(width, height);
    }
  }

  get _title() {
    return this.window.document.title;
  }

  setTitlePreface(titlePreface) {
    this.window.document.documentElement.setAttribute(
      "titlepreface",
      titlePreface
    );
  }

  get focused() {
    return this.window.document.hasFocus();
  }

  get top() {
    return this.window.screenY;
  }

  get left() {
    return this.window.screenX;
  }

  get width() {
    return this.window.outerWidth;
  }

  get height() {
    return this.window.outerHeight;
  }

  get incognito() {
    return PrivateBrowsingUtils.isWindowPrivate(this.window);
  }

  get alwaysOnTop() {
    return this.appWindow.zLevel >= Ci.nsIAppWindow.raisedZ;
  }

  get isLastFocused() {
    return this.window === windowTracker.topWindow;
  }

  static getState(window) {
    const STATES = {
      [window.STATE_MAXIMIZED]: "maximized",
      [window.STATE_MINIMIZED]: "minimized",
      [window.STATE_FULLSCREEN]: "fullscreen",
      [window.STATE_NORMAL]: "normal",
    };
    return STATES[window.windowState];
  }

  get state() {
    return Window.getState(this.window);
  }

  async setState(state) {
    let { window } = this;

    const expectedState = (function () {
      switch (state) {
        case "maximized":
          return window.STATE_MAXIMIZED;
        case "minimized":
        case "docked":
          return window.STATE_MINIMIZED;
        case "normal":
          return window.STATE_NORMAL;
        case "fullscreen":
          return window.STATE_FULLSCREEN;
      }
      throw new Error(`Unexpected window state: ${state}`);
    })();

    const initialState = window.windowState;
    if (expectedState == initialState) {
      return;
    }

    // We check for window.fullScreen here to make sure to exit fullscreen even
    // if DOM and widget disagree on what the state is. This is a speculative
    // fix for bug 1780876, ideally it should not be needed.
    if (initialState == window.STATE_FULLSCREEN || window.fullScreen) {
      window.fullScreen = false;
    }

    switch (expectedState) {
      case window.STATE_MAXIMIZED:
        window.maximize();
        break;
      case window.STATE_MINIMIZED:
        window.minimize();
        break;

      case window.STATE_NORMAL:
        // Restore sometimes returns the window to its previous state, rather
        // than to the "normal" state, so it may need to be called anywhere from
        // zero to two times.
        window.restore();
        if (window.windowState !== window.STATE_NORMAL) {
          window.restore();
        }
        break;

      case window.STATE_FULLSCREEN:
        window.fullScreen = true;
        break;

      default:
        throw new Error(`Unexpected window state: ${state}`);
    }

    if (window.windowState != expectedState) {
      // On Linux, sizemode changes are asynchronous. Some of them might not
      // even happen if the window manager doesn't want to, so wait for a bit
      // instead of forever for a sizemode change that might not ever happen.
      const noWindowManagerTimeout = 2000;

      let onSizeModeChange;
      const promiseExpectedSizeMode = new Promise(resolve => {
        onSizeModeChange = function () {
          if (window.windowState == expectedState) {
            resolve();
          }
        };
        window.addEventListener("sizemodechange", onSizeModeChange);
      });

      await Promise.any([
        promiseExpectedSizeMode,
        new Promise(resolve => setTimeout(resolve, noWindowManagerTimeout)),
      ]);

      window.removeEventListener("sizemodechange", onSizeModeChange);
    }
  }

  *getTabs() {
    // A new window is being opened and it is adopting an existing tab, we return
    // an empty iterator here because there should be no other tabs to return during
    // that duration (See Bug 1458918 for a rationale).
    if (this.window.gBrowserInit.isAdoptingTab()) {
      return;
    }

    let { tabManager } = this.extension;

    for (let nativeTab of this.window.gBrowser.tabs) {
      let tab = tabManager.getWrapper(nativeTab);
      if (tab) {
        yield tab;
      }
    }
  }

  *getHighlightedTabs() {
    let { tabManager } = this.extension;
    for (let nativeTab of this.window.gBrowser.selectedTabs) {
      let tab = tabManager.getWrapper(nativeTab);
      if (tab) {
        yield tab;
      }
    }
  }

  get activeTab() {
    let { tabManager } = this.extension;

    // A new window is being opened and it is adopting a tab, and we do not create
    // a TabWrapper for the tab being adopted because it will go away once the tab
    // adoption has been completed (See Bug 1458918 for rationale).
    if (this.window.gBrowserInit.isAdoptingTab()) {
      return null;
    }

    return tabManager.getWrapper(this.window.gBrowser.selectedTab);
  }

  getTabAtIndex(index) {
    let nativeTab = this.window.gBrowser.tabs[index];
    if (nativeTab) {
      return this.extension.tabManager.getWrapper(nativeTab);
    }
  }

  /**
   * Converts session store data to an object compatible with the return value
   * of the convert() method, representing that data.
   *
   * @param {Extension} extension
   *        The extension for which to convert the data.
   * @param {object} windowData
   *        Session store data for a closed window, as returned by
   *        `SessionStore.getClosedWindowData()`.
   *
   * @returns {object}
   * @static
   */
  static convertFromSessionStoreClosedData(extension, windowData) {
    let result = {
      sessionId: String(windowData.closedId),
      focused: false,
      incognito: false,
      type: "normal", // this is always "normal" for a closed window
      // Bug 1781226: we assert "state" is "normal" in tests, but we could use
      // the "sizemode" property if we wanted.
      state: "normal",
      alwaysOnTop: false,
    };

    if (windowData.tabs.length) {
      result.tabs = windowData.tabs.map(tabData => {
        return Tab.convertFromSessionStoreClosedData(extension, tabData);
      });
    }

    return result;
  }
}

Object.assign(global, { Tab, Window });

class TabManager extends TabManagerBase {
  get(tabId, default_ = undefined) {
    let nativeTab = tabTracker.getTab(tabId, default_);

    if (nativeTab) {
      if (!this.canAccessTab(nativeTab)) {
        throw new ExtensionError(`Invalid tab ID: ${tabId}`);
      }
      return this.getWrapper(nativeTab);
    }
    return default_;
  }

  addActiveTabPermission(nativeTab = tabTracker.activeTab) {
    return super.addActiveTabPermission(nativeTab);
  }

  revokeActiveTabPermission(nativeTab = tabTracker.activeTab) {
    return super.revokeActiveTabPermission(nativeTab);
  }

  canAccessTab(nativeTab) {
    // Check private browsing access at browser window level.
    if (!this.extension.canAccessWindow(nativeTab.ownerGlobal)) {
      return false;
    }
    if (
      this.extension.userContextIsolation &&
      !this.extension.canAccessContainer(nativeTab.userContextId)
    ) {
      return false;
    }
    return true;
  }

  wrapTab(nativeTab) {
    return new Tab(this.extension, nativeTab, tabTracker.getId(nativeTab));
  }

  getWrapper(nativeTab) {
    if (!nativeTab.ownerGlobal.gBrowserInit.isAdoptingTab()) {
      return super.getWrapper(nativeTab);
    }
  }
}

class WindowManager extends WindowManagerBase {
  get(windowId, context) {
    let window = windowTracker.getWindow(windowId, context);

    return this.getWrapper(window);
  }

  *getAll(context) {
    for (let window of windowTracker.browserWindows()) {
      if (!this.canAccessWindow(window, context)) {
        continue;
      }
      let wrapped = this.getWrapper(window);
      if (wrapped) {
        yield wrapped;
      }
    }
  }

  wrapWindow(window) {
    return new Window(this.extension, window, windowTracker.getId(window));
  }
}

// eslint-disable-next-line mozilla/balanced-listeners
extensions.on("startup", (type, extension) => {
  defineLazyGetter(extension, "tabManager", () => new TabManager(extension));
  defineLazyGetter(
    extension,
    "windowManager",
    () => new WindowManager(extension)
  );
});
