/* -*- Mode: indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set sts=2 sw=2 et tw=80: */
"use strict";

XPCOMUtils.defineLazyModuleGetter(this, "PrivateBrowsingUtils",
                                  "resource://gre/modules/PrivateBrowsingUtils.jsm");

/* globals TabBase, WindowBase, TabTrackerBase, WindowTrackerBase, TabManagerBase, WindowManagerBase */
Cu.import("resource://gre/modules/ExtensionTabs.jsm");

XPCOMUtils.defineLazyServiceGetter(this, "styleSheetService",
                                   "@mozilla.org/content/style-sheet-service;1",
                                   "nsIStyleSheetService");

Cu.import("resource://gre/modules/ExtensionUtils.jsm");

var {
  ExtensionError,
  SingletonEventManager,
  defineLazyGetter,
} = ExtensionUtils;

let tabTracker;
let windowTracker;

// This file provides some useful code for the |tabs| and |windows|
// modules. All of the code is installed on |global|, which is a scope
// shared among the different ext-*.js scripts.

global.makeWidgetId = id => {
  id = id.toLowerCase();
  // FIXME: This allows for collisions.
  return id.replace(/[^a-z0-9_-]/g, "_");
};

// Manages tab-specific context data, and dispatching tab select events
// across all windows.
global.TabContext = function TabContext(getDefaults, extension) {
  this.extension = extension;
  this.getDefaults = getDefaults;

  this.tabData = new WeakMap();
  this.lastLocation = new WeakMap();

  windowTracker.addListener("progress", this);
  windowTracker.addListener("TabSelect", this);

  EventEmitter.decorate(this);
};

TabContext.prototype = {
  get(tab) {
    if (!this.tabData.has(tab)) {
      this.tabData.set(tab, this.getDefaults(tab));
    }

    return this.tabData.get(tab);
  },

  clear(tab) {
    this.tabData.delete(tab);
  },

  handleEvent(event) {
    if (event.type == "TabSelect") {
      let tab = event.target;
      this.emit("tab-select", tab);
      this.emit("location-change", tab);
    }
  },

  onStateChange(browser, webProgress, request, stateFlags, statusCode) {
    let flags = Ci.nsIWebProgressListener;

    if (!(~stateFlags & (flags.STATE_IS_WINDOW | flags.STATE_START) ||
          this.lastLocation.has(browser))) {
      this.lastLocation.set(browser, request.URI);
    }
  },

  onLocationChange(browser, webProgress, request, locationURI, flags) {
    let gBrowser = browser.ownerGlobal.gBrowser;
    let lastLocation = this.lastLocation.get(browser);
    if (browser === gBrowser.selectedBrowser &&
        !(lastLocation && lastLocation.equalsExceptRef(browser.currentURI))) {
      let tab = gBrowser.getTabForBrowser(browser);
      this.emit("location-change", tab, true);
    }
    this.lastLocation.set(browser, browser.currentURI);
  },

  shutdown() {
    windowTracker.removeListener("progress", this);
    windowTracker.removeListener("TabSelect", this);
  },
};


class WindowTracker extends WindowTrackerBase {
  addProgressListener(window, listener) {
    window.gBrowser.addTabsProgressListener(listener);
  }

  removeProgressListener(window, listener) {
    window.gBrowser.removeTabsProgressListener(listener);
  }
}

global.WindowEventManager = class extends SingletonEventManager {
  constructor(context, name, event, listener) {
    super(context, name, fire => {
      let listener2 = listener.bind(null, fire);

      windowTracker.addListener(event, listener2);
      return () => {
        windowTracker.removeListener(event, listener2);
      };
    });
  }
};

class TabTracker extends TabTrackerBase {
  constructor() {
    super();

    this._tabs = new WeakMap();
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
    windowTracker.addOpenListener(this._handleWindowOpen);
    windowTracker.addCloseListener(this._handleWindowClose);

    /* eslint-disable mozilla/balanced-listeners */
    this.on("tab-detached", this._handleTabDestroyed);
    this.on("tab-removed", this._handleTabDestroyed);
    /* eslint-enable mozilla/balanced-listeners */
  }

  getId(tab) {
    if (this._tabs.has(tab)) {
      return this._tabs.get(tab);
    }

    this.init();

    let id = this._nextId++;
    this.setId(tab, id);
    return id;
  }

  setId(tab, id) {
    this._tabs.set(tab, id);
    this._tabIds.set(id, tab);
  }

  _handleTabDestroyed(event, {tab}) {
    let id = this._tabs.get(tab);
    if (id) {
      this._tabs.delete(tab);
      if (this._tabIds.get(id) === tab) {
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
    let tab = this._tabIds.get(tabId);
    if (tab) {
      return tab;
    }
    if (default_ !== undefined) {
      return default_;
    }
    throw new ExtensionError(`Invalid tab ID: ${tabId}`);
  }

  /**
   * @param {Event} event
   *        The DOM Event to handle.
   * @private
   */
  handleEvent(event) {
    let tab = event.target;

    switch (event.type) {
      case "TabOpen":
        let {adoptedTab} = event.detail;
        if (adoptedTab) {
          this.adoptedTabs.set(adoptedTab, event.target);

          // This tab is being created to adopt a tab from a different window.
          // Copy the ID from the old tab to the new.
          this.setId(tab, this.getId(adoptedTab));

          adoptedTab.linkedBrowser.messageManager.sendAsyncMessage("Extension:SetTabAndWindowId", {
            windowId: windowTracker.getId(tab.ownerGlobal),
          });
        }

        // We need to delay sending this event until the next tick, since the
        // tab does not have its final index when the TabOpen event is dispatched.
        Promise.resolve().then(() => {
          if (event.detail.adoptedTab) {
            this.emitAttached(event.originalTarget);
          } else {
            this.emitCreated(event.originalTarget);
          }
        });
        break;

      case "TabClose":
        let {adoptedBy} = event.detail;
        if (adoptedBy) {
          // This tab is being closed because it was adopted by a new window.
          // Copy its ID to the new tab, in case it was created as the first tab
          // of a new window, and did not have an `adoptedTab` detail when it was
          // opened.
          this.setId(adoptedBy, this.getId(tab));

          this.emitDetached(tab, adoptedBy);
        } else {
          this.emitRemoved(tab, false);
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
    if (window.arguments && window.arguments[0] instanceof window.XULElement) {
      // If the first window argument is a XUL element, it means the
      // window is about to adopt a tab from another window to replace its
      // initial tab.
      //
      // Note that this event handler depends on running before the
      // delayed startup code in browser.js, which is currently triggered
      // by the first MozAfterPaint event. That code handles finally
      // adopting the tab, and clears it from the arguments list in the
      // process, so if we run later than it, we're too late.
      let tab = window.arguments[0];
      let adoptedBy = window.gBrowser.tabs[0];

      this.adoptedTabs.set(tab, adoptedBy);
      this.setId(adoptedBy, this.getId(tab));

      // We need to be sure to fire this event after the onDetached event
      // for the original tab.
      let listener = (event, details) => {
        if (details.tab === tab) {
          this.off("tab-detached", listener);

          Promise.resolve().then(() => {
            this.emitAttached(details.adoptedBy);
          });
        }
      };

      this.on("tab-detached", listener);
    } else {
      for (let tab of window.gBrowser.tabs) {
        this.emitCreated(tab);
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
    for (let tab of window.gBrowser.tabs) {
      if (this.adoptedTabs.has(tab)) {
        this.emitDetached(tab, this.adoptedTabs.get(tab));
      } else {
        this.emitRemoved(tab, true);
      }
    }
  }

  /**
   * Emits a "tab-attached" event for the given tab element.
   *
   * @param {NativeTab} tab
   *        The tab element in the window to which the tab is being attached.
   * @private
   */
  emitAttached(tab) {
    let newWindowId = windowTracker.getId(tab.ownerGlobal);
    let tabId = this.getId(tab);

    this.emit("tab-attached", {tab, tabId, newWindowId, newPosition: tab._tPos});
  }

  /**
   * Emits a "tab-detached" event for the given tab element.
   *
   * @param {NativeTab} tab
   *        The tab element in the window from which the tab is being detached.
   * @param {NativeTab} adoptedBy
   *        The tab element in the window to which detached tab is being moved,
   *        and will adopt this tab's contents.
   * @private
   */
  emitDetached(tab, adoptedBy) {
    let oldWindowId = windowTracker.getId(tab.ownerGlobal);
    let tabId = this.getId(tab);

    this.emit("tab-detached", {tab, adoptedBy, tabId, oldWindowId, oldPosition: tab._tPos});
  }

  /**
   * Emits a "tab-created" event for the given tab element.
   *
   * @param {NativeTab} tab
   *        The tab element which is being created.
   * @private
   */
  emitCreated(tab) {
    this.emit("tab-created", {tab});
  }

  /**
   * Emits a "tab-removed" event for the given tab element.
   *
   * @param {NativeTab} tab
   *        The tab element which is being removed.
   * @param {boolean} isWindowClosing
   *        True if the tab is being removed because the browser window is
   *        closing.
   * @private
   */
  emitRemoved(tab, isWindowClosing) {
    let windowId = windowTracker.getId(tab.ownerGlobal);
    let tabId = this.getId(tab);

    // When addons run in-process, `window.close()` is synchronous. Most other
    // addon-invoked calls are asynchronous since they go through a proxy
    // context via the message manager. This includes event registrations such
    // as `tabs.onRemoved.addListener`.
    //
    // So, even if `window.close()` were to be called (in-process) after calling
    // `tabs.onRemoved.addListener`, then the tab would be closed before the
    // event listener is registered. To make sure that the event listener is
    // notified, we dispatch `tabs.onRemoved` asynchronously.
    Services.tm.mainThread.dispatch(() => {
      this.emit("tab-removed", {tab, tabId, windowId, isWindowClosing});
    }, Ci.nsIThread.DISPATCH_NORMAL);
  }

  getBrowserData(browser) {
    if (browser.ownerGlobal.location.href === "about:addons") {
      // When we're loaded into a <browser> inside about:addons, we need to go up
      // one more level.
      browser = browser.ownerGlobal.QueryInterface(Ci.nsIInterfaceRequestor)
                       .getInterface(Ci.nsIDocShell)
                       .chromeEventHandler;
    }

    let result = {
      tabId: -1,
      windowId: -1,
    };

    let {gBrowser} = browser.ownerGlobal;
    // Some non-browser windows have gBrowser but not
    // getTabForBrowser!
    if (gBrowser && gBrowser.getTabForBrowser) {
      result.windowId = windowTracker.getId(browser.ownerGlobal);

      let tab = gBrowser.getTabForBrowser(browser);
      if (tab) {
        result.tabId = this.getId(tab);
      }
    }

    return result;
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
    return this.window.gBrowser.getIcon(this.tab);
  }

  get audible() {
    return this.tab.soundPlaying;
  }

  get browser() {
    return this.tab.linkedBrowser;
  }

  get cookieStoreId() {
    return getCookieStoreIdForTab(this, this.tab);
  }

  get height() {
    return this.browser.clientHeight;
  }

  get index() {
    return this.tab._tPos;
  }

  get mutedInfo() {
    let tab = this.tab;

    let mutedInfo = {muted: tab.muted};
    if (tab.muteReason === null) {
      mutedInfo.reason = "user";
    } else if (tab.muteReason) {
      mutedInfo.reason = "extension";
      mutedInfo.extensionId = tab.muteReason;
    }

    return mutedInfo;
  }

  get pinned() {
    return this.tab.pinned;
  }

  get active() {
    return this.tab.selected;
  }

  get selected() {
    return this.tab.selected;
  }

  get status() {
    if (this.tab.getAttribute("busy") === "true") {
      return "loading";
    }
    return "complete";
  }

  get width() {
    return this.browser.clientWidth;
  }

  get window() {
    return this.tab.ownerGlobal;
  }

  get windowId() {
    return windowTracker.getId(this.window);
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
      selected: false,
      highlighted: false,
      active: false,
      pinned: false,
      incognito: Boolean(tabData.state && tabData.state.isPrivate),
    };

    if (extension.tabManager.hasTabPermission(tabData)) {
      let entries = tabData.state ? tabData.state.entries : tabData.entries;
      result.url = entries[0].url;
      result.title = entries[0].title;
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
    let {tabManager} = this.extension;

    for (let tab of this.window.gBrowser.tabs) {
      yield tabManager.getWrapper(tab);
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
      result.tabs = windowData.tabs.map(tab => {
        return Tab.convertFromSessionStoreClosedData(extension, tab);
      });
    }

    return result;
  }
}

Object.assign(global, {Tab, Window});

class TabManager extends TabManagerBase {
  get(tabId, default_ = undefined) {
    let tab = tabTracker.getTab(tabId, default_);

    if (tab) {
      return this.getWrapper(tab);
    }
    return default_;
  }

  addActiveTabPermission(tab = tabTracker.activeTab) {
    return super.addActiveTabPermission(tab);
  }

  revokeActiveTabPermission(tab = tabTracker.activeTab) {
    return super.revokeActiveTabPermission(tab);
  }

  wrapTab(tab) {
    return new Tab(this.extension, tab, tabTracker.getId(tab));
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
