/* -*- Mode: indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set sts=2 sw=2 et tw=80: */
"use strict";

// This file provides some useful code for the |tabs| and |windows|
// modules. All of the code is installed on |global|, which is a scope
// shared among the different ext-*.js scripts.

ChromeUtils.defineModuleGetter(this, "PrivateBrowsingUtils",
                               "resource://gre/modules/PrivateBrowsingUtils.jsm");
ChromeUtils.defineModuleGetter(this, "BrowserWindowTracker",
                               "resource:///modules/BrowserWindowTracker.jsm");

var {
  ExtensionError,
} = ExtensionUtils;

var {
  defineLazyGetter,
} = ExtensionCommon;

const READER_MODE_PREFIX = "about:reader";

let tabTracker;
let windowTracker;

// This function is pretty tightly tied to Extension.jsm.
// Its job is to fill in the |tab| property of the sender.
const getSender = (extension, target, sender) => {
  let tabId;
  if ("tabId" in sender) {
    // The message came from a privileged extension page running in a tab. In
    // that case, it should include a tabId property (which is filled in by the
    // page-open listener below).
    tabId = sender.tabId;
    delete sender.tabId;
  } else if (ExtensionCommon.instanceOf(target, "XULElement") ||
             ExtensionCommon.instanceOf(target, "HTMLIFrameElement")) {
    tabId = tabTracker.getBrowserData(target).tabId;
  }

  if (tabId) {
    let tab = extension.tabManager.get(tabId, null);
    if (tab) {
      sender.tab = tab.convert();
    }
  }
};

// Used by Extension.jsm
global.tabGetSender = getSender;

/* eslint-disable mozilla/balanced-listeners */
extensions.on("uninstalling", (msg, extension) => {
  if (extension.uninstallURL) {
    let browser = windowTracker.topWindow.gBrowser;
    browser.addTab(extension.uninstallURL, {relatedToCurrent: true});
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
    let {gBrowser} = context.xulBrowser.ownerGlobal;
    if (gBrowser && gBrowser.getTabForBrowser) {
      let nativeTab = gBrowser.getTabForBrowser(context.xulBrowser);
      if (nativeTab) {
        gBrowser.removeTab(nativeTab);
      }
    }
  }
});
/* eslint-enable mozilla/balanced-listeners */

global.openOptionsPage = (extension) => {
  let window = windowTracker.topWindow;
  if (!window) {
    return Promise.reject({message: "No browser window available"});
  }

  if (extension.manifest.options_ui.open_in_tab) {
    window.switchToTabHavingURI(extension.manifest.options_ui.page, true, {
      triggeringPrincipal: extension.principal,
    });
    return Promise.resolve();
  }

  let viewId = `addons://detail/${encodeURIComponent(extension.id)}/preferences`;

  return window.BrowserOpenAddonsMgr(viewId);
};

global.makeWidgetId = id => {
  id = id.toLowerCase();
  // FIXME: This allows for collisions.
  return id.replace(/[^a-z0-9_-]/g, "_");
};

global.waitForTabLoaded = (tab, url) => {
  return new Promise(resolve => {
    windowTracker.addListener("progress", {
      onLocationChange(browser, webProgress, request, locationURI, flags) {
        if (webProgress.isTopLevel
            && browser.ownerGlobal.gBrowser.getTabForBrowser(browser) == tab
            && (!url || locationURI.spec == url)) {
          windowTracker.removeListener("progress", this);
          resolve();
        }
      },
    });
  });
};

global.replaceUrlInTab = (gBrowser, tab, url) => {
  let loaded = waitForTabLoaded(tab, url);
  gBrowser.loadURI(url, {
    flags: Ci.nsIWebNavigation.LOAD_FLAGS_REPLACE_HISTORY,
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
   * @returns {Object}
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
    let gBrowser = browser.ownerGlobal.gBrowser;
    let tab = gBrowser.getTabForBrowser(browser);
    // fromBrowse will be false in case of e.g. a hash change or history.pushState
    let fromBrowse = !(flags & Ci.nsIWebProgressListener.LOCATION_CHANGE_SAME_DOCUMENT);
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
   * @property {DOMWindow|null} topNormalWindow
   *        The currently active, or topmost, browser window, or null if no
   *        browser window is currently open.
   *        Will return the topmost "normal" (i.e., not popup) window.
   *        @readonly
   */
  get topNormalWindow() {
    return BrowserWindowTracker.getTopWindow({allowPopups: false});
  }
}

class TabTracker extends TabTrackerBase {
  constructor() {
    super();

    this._tabs = new WeakMap();
    this._browsers = new WeakMap();
    this._tabIds = new Map();
    this._nextId = 1;

    this._handleTabDestroyed = this._handleTabDestroyed.bind(this);
  }

  init() {
    if (this.initialized) {
      return;
    }
    this.initialized = true;

    this.adoptedTabs = new WeakMap();

    this._handleWindowOpen = this._handleWindowOpen.bind(this);
    this._handleWindowClose = this._handleWindowClose.bind(this);

    windowTracker.addListener("TabClose", this);
    windowTracker.addListener("TabOpen", this);
    windowTracker.addListener("TabSelect", this);
    windowTracker.addOpenListener(this._handleWindowOpen);
    windowTracker.addCloseListener(this._handleWindowClose);

    Services.mm.addMessageListener("Reader:UpdateReaderButton", this);

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
    this._tabs.set(nativeTab, id);
    if (nativeTab.linkedBrowser) {
      this._browsers.set(nativeTab.linkedBrowser, id);
    }
    this._tabIds.set(id, nativeTab);
  }

  /**
   * Handles tab adoption when a tab is moved between windows.
   * Ensures the new tab will have the same ID as the old one,
   * and emits a "tab-adopted" event.
   *
   * @param {NativeTab} adoptingTab
   *        The tab which is being opened and adopting `adoptedTab`.
   * @param {NativeTab} adoptedTab
   *        The tab which is being closed and adopted by `adoptingTab`.
   */
  adopt(adoptingTab, adoptedTab) {
    if (!this.adoptedTabs.has(adoptedTab)) {
      this.adoptedTabs.set(adoptedTab, adoptingTab);
      this.setId(adoptingTab, this.getId(adoptedTab));
      this.emit("tab-adopted", adoptingTab, adoptedTab);
    }
  }

  _handleTabDestroyed(event, {nativeTab}) {
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

  /**
   * @param {Event} event
   *        The DOM Event to handle.
   * @private
   */
  handleEvent(event) {
    let nativeTab = event.target;

    switch (event.type) {
      case "TabOpen":
        let {adoptedTab} = event.detail;
        if (adoptedTab) {
          // This tab is being created to adopt a tab from a different window.
          // Handle the adoption.
          this.adopt(nativeTab, adoptedTab);

          adoptedTab.linkedBrowser.messageManager.sendAsyncMessage("Extension:SetFrameData", {
            windowId: windowTracker.getId(nativeTab.ownerGlobal),
          });
        }

        // Save the current tab, since the newly-created tab will likely be
        // active by the time the promise below resolves and the event is
        // dispatched.
        let currentTab = nativeTab.ownerGlobal.gBrowser.selectedTab;

        // We need to delay sending this event until the next tick, since the
        // tab does not have its final index when the TabOpen event is dispatched.
        Promise.resolve().then(() => {
          if (event.detail.adoptedTab) {
            this.emitAttached(event.originalTarget);
          } else {
            this.emitCreated(event.originalTarget, currentTab);
          }
        });
        break;

      case "TabClose":
        let {adoptedBy} = event.detail;
        if (adoptedBy) {
          // This tab is being closed because it was adopted by a new window.
          // Handle the adoption in case it was created as the first tab of a
          // new window, and did not have an `adoptedTab` detail when it was
          // opened.
          this.adopt(adoptedBy, nativeTab);

          this.emitDetached(nativeTab, adoptedBy);
        } else {
          this.emitRemoved(nativeTab, false);
        }
        break;

      case "TabSelect":
        // Because we are delaying calling emitCreated above, we also need to
        // delay sending this event because it shouldn't fire before onCreated.
        Promise.resolve().then(() => {
          this.emitActivated(nativeTab);
        });
        break;
    }
  }

  /**
   * @param {Object} message
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
      let nativeTab = tabToAdopt;
      let adoptedBy = window.gBrowser.tabs[0];
      this.adopt(adoptedBy, nativeTab);

      // We need to be sure to fire this event after the onDetached event
      // for the original tab.
      let listener = (event, details) => {
        if (details.nativeTab === nativeTab) {
          this.off("tab-detached", listener);

          Promise.resolve().then(() => {
            this.emitAttached(details.adoptedBy);
          });
        }
      };

      this.on("tab-detached", listener);
    } else {
      for (let nativeTab of window.gBrowser.tabs) {
        this.emitCreated(nativeTab);
      }

      // emitActivated to trigger tab.onActivated/tab.onHighlighted for a newly opened window.
      this.emitActivated(window.gBrowser.tabs[0]);
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
      if (this.adoptedTabs.has(nativeTab)) {
        this.emitDetached(nativeTab, this.adoptedTabs.get(nativeTab));
      } else {
        this.emitRemoved(nativeTab, true);
      }
    }
  }

  /**
   * Emits a "tab-activated" event for the given tab element.
   *
   * @param {NativeTab} nativeTab
   *        The tab element which has been activated.
   * @private
   */
  emitActivated(nativeTab) {
    this.emit("tab-activated", {
      tabId: this.getId(nativeTab),
      windowId: windowTracker.getId(nativeTab.ownerGlobal)});
  }

  /**
   * Emits a "tab-attached" event for the given tab element.
   *
   * @param {NativeTab} nativeTab
   *        The tab element in the window to which the tab is being attached.
   * @private
   */
  emitAttached(nativeTab) {
    let newWindowId = windowTracker.getId(nativeTab.ownerGlobal);
    let tabId = this.getId(nativeTab);

    this.emit("tab-attached", {nativeTab, tabId, newWindowId, newPosition: nativeTab._tPos});
  }

  /**
   * Emits a "tab-detached" event for the given tab element.
   *
   * @param {NativeTab} nativeTab
   *        The tab element in the window from which the tab is being detached.
   * @param {NativeTab} adoptedBy
   *        The tab element in the window to which detached tab is being moved,
   *        and will adopt this tab's contents.
   * @private
   */
  emitDetached(nativeTab, adoptedBy) {
    let oldWindowId = windowTracker.getId(nativeTab.ownerGlobal);
    let tabId = this.getId(nativeTab);

    this.emit("tab-detached", {nativeTab, adoptedBy, tabId, oldWindowId, oldPosition: nativeTab._tPos});
  }

  /**
   * Emits a "tab-created" event for the given tab element.
   *
   * @param {NativeTab} nativeTab
   *        The tab element which is being created.
   * @param {NativeTab} [currentTab]
   *        The tab element for the currently active tab.
   * @private
   */
  emitCreated(nativeTab, currentTab) {
    this.emit("tab-created", {nativeTab, currentTab});
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

    this.emit("tab-removed", {nativeTab, tabId, windowId, isWindowClosing});
  }

  getBrowserData(browser) {
    let {gBrowser} = browser.ownerGlobal;
    // Some non-browser windows have gBrowser but not getTabForBrowser!
    if (!gBrowser || !gBrowser.getTabForBrowser) {
      if (browser.ownerDocument.documentURI === "about:addons") {
        // When we're loaded into a <browser> inside about:addons, we need to go up
        // one more level.
        browser = browser.ownerDocument.docShell.chromeEventHandler;

        ({gBrowser} = browser.ownerGlobal);
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

Object.assign(global, {tabTracker, windowTracker});

class Tab extends TabBase {
  get _favIconUrl() {
    return this.window.gBrowser.getIcon(this.nativeTab);
  }

  get audible() {
    return this.nativeTab.soundPlaying;
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
    return super.frameLoader || {lazyWidth: 0, lazyHeight: 0};
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
    if (opener && opener.parentNode && opener.ownerDocument == this.nativeTab.ownerDocument) {
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
    let {nativeTab} = this;

    let mutedInfo = {muted: nativeTab.muted};
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
    let {selected, multiselected} = this.nativeTab;
    return selected || multiselected;
  }

  get selected() {
    return this.nativeTab.selected;
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

  /**
   * Converts session store data to an object compatible with the return value
   * of the convert() method, representing that data.
   *
   * @param {Extension} extension
   *        The extension for which to convert the data.
   * @param {Object} tabData
   *        Session store data for a closed tab, as returned by
   *        `SessionStore.getClosedTabData()`.
   * @param {DOMWindow} [window = null]
   *        The browser window which the tab belonged to before it was closed.
   *        May be null if the window the tab belonged to no longer exists.
   *
   * @returns {Object}
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
      lastAccessed: tabData.state ? tabData.state.lastAccessed : tabData.lastAccessed,
    };

    // tabData is a representation of a tab, as stored in the session data,
    // and given that is not a real nativeTab, we only need to check if the extension
    // has the "tabs" permission (because tabData represents a closed tab, and so we
    // already know that it can't be the activeTab).
    if (extension.hasPermission("tabs")) {
      let entries = tabData.state ? tabData.state.entries : tabData.entries;
      let lastTabIndex = tabData.state ? tabData.state.index : tabData.index;
      // We need to take lastTabIndex - 1 because the index in the tab data is
      // 1-based rather than 0-based.
      let entry = entries[lastTabIndex - 1];
      result.url = entry.url;
      result.title = entry.title;
      if (tabData.image) {
        result.favIconUrl = tabData.image;
      }
    }

    return result;
  }
}

class Window extends WindowBase {
  /**
   * Update the geometry of the browser window.
   *
   * @param {Object} options
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
    let {window} = this;

    if (options.left !== null || options.top !== null) {
      let left = options.left !== null ? options.left : window.screenX;
      let top = options.top !== null ? options.top : window.screenY;
      window.moveTo(left, top);
    }

    if (options.width !== null || options.height !== null) {
      let width = options.width !== null ? options.width : window.outerWidth;
      let height = options.height !== null ? options.height : window.outerHeight;
      window.resizeTo(width, height);
    }
  }

  get _title() {
    return this.window.document.title;
  }

  setTitlePreface(titlePreface) {
    this.window.document.documentElement.setAttribute("titlepreface", titlePreface);
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
    return this.xulWindow.zLevel >= Ci.nsIXULWindow.raisedZ;
  }

  get isLastFocused() {
    return this.window === windowTracker.topWindow;
  }

  static getState(window) {
    const STATES = {
      [window.STATE_MAXIMIZED]: "maximized",
      [window.STATE_MINIMIZED]: "minimized",
      [window.STATE_NORMAL]: "normal",
    };
    let state = STATES[window.windowState];
    if (window.fullScreen) {
      state = "fullscreen";
    }
    return state;
  }

  get state() {
    return Window.getState(this.window);
  }

  set state(state) {
    let {window} = this;
    if (state !== "fullscreen" && window.fullScreen) {
      window.fullScreen = false;
    }

    switch (state) {
      case "maximized":
        window.maximize();
        break;

      case "minimized":
      case "docked":
        window.minimize();
        break;

      case "normal":
        // Restore sometimes returns the window to its previous state, rather
        // than to the "normal" state, so it may need to be called anywhere from
        // zero to two times.
        window.restore();
        if (window.windowState !== window.STATE_NORMAL) {
          window.restore();
        }
        if (window.windowState !== window.STATE_NORMAL) {
          // And on OS-X, where normal vs. maximized is basically a heuristic,
          // we need to cheat.
          window.sizeToContent();
        }
        break;

      case "fullscreen":
        window.fullScreen = true;
        break;

      default:
        throw new Error(`Unexpected window state: ${state}`);
    }
  }

  * getTabs() {
    // A new window is being opened and it is adopting an existing tab, we return
    // an empty iterator here because there should be no other tabs to return during
    // that duration (See Bug 1458918 for a rationale).
    if (this.window.gBrowserInit.isAdoptingTab()) {
      return;
    }

    let {tabManager} = this.extension;

    for (let nativeTab of this.window.gBrowser.tabs) {
      yield tabManager.getWrapper(nativeTab);
    }
  }

  * getHighlightedTabs() {
    let {tabManager} = this.extension;
    for (let tab of this.window.gBrowser.selectedTabs) {
      yield tabManager.getWrapper(tab);
    }
  }

  get activeTab() {
    let {tabManager} = this.extension;

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
   * @param {Object} windowData
   *        Session store data for a closed window, as returned by
   *        `SessionStore.getClosedWindowData()`.
   *
   * @returns {Object}
   * @static
   */
  static convertFromSessionStoreClosedData(extension, windowData) {
    let result = {
      sessionId: String(windowData.closedId),
      focused: false,
      incognito: false,
      type: "normal", // this is always "normal" for a closed window
      // Surely this does not actually work?
      state: this.getState(windowData),
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

Object.assign(global, {Tab, Window});

class TabManager extends TabManagerBase {
  get(tabId, default_ = undefined) {
    let nativeTab = tabTracker.getTab(tabId, default_);

    if (nativeTab) {
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

  wrapTab(nativeTab) {
    return new Tab(this.extension, nativeTab, tabTracker.getId(nativeTab));
  }
}

class WindowManager extends WindowManagerBase {
  get(windowId, context) {
    let window = windowTracker.getWindow(windowId, context);

    return this.getWrapper(window);
  }

  * getAll() {
    for (let window of windowTracker.browserWindows()) {
      yield this.getWrapper(window);
    }
  }

  wrapWindow(window) {
    return new Window(this.extension, window, windowTracker.getId(window));
  }
}


extensions.on("startup", (type, extension) => { // eslint-disable-line mozilla/balanced-listeners
  defineLazyGetter(extension, "tabManager",
                   () => new TabManager(extension));
  defineLazyGetter(extension, "windowManager",
                   () => new WindowManager(extension));
});
