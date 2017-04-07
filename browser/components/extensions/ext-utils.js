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

var {
  ExtensionError,
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
  get(nativeTab) {
    if (!this.tabData.has(nativeTab)) {
      this.tabData.set(nativeTab, this.getDefaults(nativeTab));
    }

    return this.tabData.get(nativeTab);
  },

  clear(nativeTab) {
    this.tabData.delete(nativeTab);
  },

  handleEvent(event) {
    if (event.type == "TabSelect") {
      let nativeTab = event.target;
      this.emit("tab-select", nativeTab);
      this.emit("location-change", nativeTab);
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
      let nativeTab = gBrowser.getTabForBrowser(browser);
      this.emit("location-change", nativeTab, true);
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

/**
 * An event manager API provider which listens for a DOM event in any browser
 * window, and calls the given listener function whenever an event is received.
 * That listener function receives a `fire` object, which it can use to dispatch
 * events to the extension, and a DOM event object.
 *
 * @param {BaseContext} context
 *        The extension context which the event manager belongs to.
 * @param {string} name
 *        The API name of the event manager, e.g.,"runtime.onMessage".
 * @param {string} event
 *        The name of the DOM event to listen for.
 * @param {function} listener
 *        The listener function to call when a DOM event is received.
 */
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

  getId(nativeTab) {
    if (this._tabs.has(nativeTab)) {
      return this._tabs.get(nativeTab);
    }

    this.init();

    let id = this._nextId++;
    this.setId(nativeTab, id);
    return id;
  }

  setId(nativeTab, id) {
    this._tabs.set(nativeTab, id);
    this._tabIds.set(id, nativeTab);
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
          this.adoptedTabs.set(adoptedTab, event.target);

          // This tab is being created to adopt a tab from a different window.
          // Copy the ID from the old tab to the new.
          this.setId(nativeTab, this.getId(adoptedTab));

          adoptedTab.linkedBrowser.messageManager.sendAsyncMessage("Extension:SetTabAndWindowId", {
            windowId: windowTracker.getId(nativeTab.ownerGlobal),
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
          this.setId(adoptedBy, this.getId(nativeTab));

          this.emitDetached(nativeTab, adoptedBy);
        } else {
          this.emitRemoved(nativeTab, false);
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
      let nativeTab = window.arguments[0];
      let adoptedBy = window.gBrowser.tabs[0];

      this.adoptedTabs.set(nativeTab, adoptedBy);
      this.setId(adoptedBy, this.getId(nativeTab));

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
   * @private
   */
  emitCreated(nativeTab) {
    this.emit("tab-created", {nativeTab});
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
      this.emit("tab-removed", {nativeTab, tabId, windowId, isWindowClosing});
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

      let nativeTab = gBrowser.getTabForBrowser(browser);
      if (nativeTab) {
        result.tabId = this.getId(nativeTab);
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
    return this.window.gBrowser.getIcon(this.nativeTab);
  }

  get audible() {
    return this.nativeTab.soundPlaying;
  }

  get browser() {
    return this.nativeTab.linkedBrowser;
  }

  get cookieStoreId() {
    return getCookieStoreIdForTab(this, this.nativeTab);
  }

  get height() {
    return this.browser.clientHeight;
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

  get pinned() {
    return this.nativeTab.pinned;
  }

  get active() {
    return this.nativeTab.selected;
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
    return this.browser.clientWidth;
  }

  get window() {
    return this.nativeTab.ownerGlobal;
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
      let entry = entries[entries.length - 1];
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

    for (let nativeTab of this.window.gBrowser.tabs) {
      yield tabManager.getWrapper(nativeTab);
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
