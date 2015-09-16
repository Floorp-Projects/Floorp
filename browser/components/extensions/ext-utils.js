XPCOMUtils.defineLazyModuleGetter(this, "PrivateBrowsingUtils",
                                  "resource://gre/modules/PrivateBrowsingUtils.jsm");

Cu.import("resource://gre/modules/ExtensionUtils.jsm");
var {
  EventManager,
} = ExtensionUtils;

// This file provides some useful code for the |tabs| and |windows|
// modules. All of the code is installed on |global|, which is a scope
// shared among the different ext-*.js scripts.

// Manages mapping between XUL tabs and extension tab IDs.
global.TabManager = {
  _tabs: new WeakMap(),
  _nextId: 1,

  getId(tab) {
    if (this._tabs.has(tab)) {
      return this._tabs.get(tab);
    }
    let id = this._nextId++;
    this._tabs.set(tab, id);
    return id;
  },

  getBrowserId(browser) {
    let gBrowser = browser.ownerDocument.defaultView.gBrowser;
    // Some non-browser windows have gBrowser but not
    // getTabForBrowser!
    if (gBrowser && gBrowser.getTabForBrowser) {
      let tab = gBrowser.getTabForBrowser(browser);
      if (tab) {
        return this.getId(tab);
      }
    }
    return -1;
  },

  getTab(tabId) {
    // FIXME: Speed this up without leaking memory somehow.
    let e = Services.wm.getEnumerator("navigator:browser");
    while (e.hasMoreElements()) {
      let window = e.getNext();
      if (!window.gBrowser) {
        continue;
      }
      for (let tab of window.gBrowser.tabs) {
        if (this.getId(tab) == tabId) {
          return tab;
        }
      }
    }
    return null;
  },

  get activeTab() {
    let window = WindowManager.topWindow;
    if (window && window.gBrowser) {
      return window.gBrowser.selectedTab;
    }
    return null;
  },

  getStatus(tab) {
    return tab.getAttribute("busy") == "true" ? "loading" : "complete";
  },

  convert(extension, tab) {
    let window = tab.ownerDocument.defaultView;
    let windowActive = window == WindowManager.topWindow;
    let result = {
      id: this.getId(tab),
      index: tab._tPos,
      windowId: WindowManager.getId(window),
      selected: tab.selected,
      highlighted: tab.selected,
      active: tab.selected,
      pinned: tab.pinned,
      status: this.getStatus(tab),
      incognito: PrivateBrowsingUtils.isBrowserPrivate(tab.linkedBrowser),
      width: tab.linkedBrowser.clientWidth,
      height: tab.linkedBrowser.clientHeight,
    };

    if (extension.hasPermission("tabs")) {
      result.url = tab.linkedBrowser.currentURI.spec;
      if (tab.linkedBrowser.contentTitle) {
        result.title = tab.linkedBrowser.contentTitle;
      }
      let icon = window.gBrowser.getIcon(tab);
      if (icon) {
        result.favIconUrl = icon;
      }
    }

    return result;
  },

  getTabs(extension, window) {
    if (!window.gBrowser) {
      return [];
    }
    return [ for (tab of window.gBrowser.tabs) this.convert(extension, tab) ];
  },
};

// Manages mapping between XUL windows and extension window IDs.
global.WindowManager = {
  _windows: new WeakMap(),
  _nextId: 0,

  WINDOW_ID_NONE: -1,
  WINDOW_ID_CURRENT: -2,

  get topWindow() {
    return Services.wm.getMostRecentWindow("navigator:browser");
  },

  windowType(window) {
    // TODO: Make this work.
    return "normal";
  },

  getId(window) {
    if (this._windows.has(window)) {
      return this._windows.get(window);
    }
    let id = this._nextId++;
    this._windows.set(window, id);
    return id;
  },

  getWindow(id) {
    let e = Services.wm.getEnumerator("navigator:browser");
    while (e.hasMoreElements()) {
      let window = e.getNext();
      if (this.getId(window) == id) {
        return window;
      }
    }
    return null;
  },

  convert(extension, window, getInfo) {
    let result = {
      id: this.getId(window),
      focused: window == WindowManager.topWindow,
      top: window.screenY,
      left: window.screenX,
      width: window.outerWidth,
      height: window.outerHeight,
      incognito: PrivateBrowsingUtils.isWindowPrivate(window),

      // We fudge on these next two.
      type: this.windowType(window),
      state: window.fullScreen ? "fullscreen" : "normal",
    };

    if (getInfo && getInfo.populate) {
      results.tabs = TabManager.getTabs(extension, window);
    }

    return result;
  },
};

// Manages listeners for window opening and closing. A window is
// considered open when the "load" event fires on it. A window is
// closed when a "domwindowclosed" notification fires for it.
global.WindowListManager = {
  _openListeners: new Set(),
  _closeListeners: new Set(),

  addOpenListener(listener, fireOnExisting = true) {
    if (this._openListeners.length == 0 && this._closeListeners.length == 0) {
      Services.ww.registerNotification(this);
    }
    this._openListeners.add(listener);

    let e = Services.wm.getEnumerator("navigator:browser");
    while (e.hasMoreElements()) {
      let window = e.getNext();
      if (window.document.readyState != "complete") {
        window.addEventListener("load", this);
      } else if (fireOnExisting) {
        listener(window);
      }
    }
  },

  removeOpenListener(listener) {
    this._openListeners.delete(listener);
    if (this._openListeners.length == 0 && this._closeListeners.length == 0) {
      Services.ww.unregisterNotification(this);
    }
  },

  addCloseListener(listener) {
    if (this._openListeners.length == 0 && this._closeListeners.length == 0) {
      Services.ww.registerNotification(this);
    }
    this._closeListeners.add(listener);
  },

  removeCloseListener(listener) {
    this._closeListeners.delete(listener);
    if (this._openListeners.length == 0 && this._closeListeners.length == 0) {
      Services.ww.unregisterNotification(this);
    }
  },

  handleEvent(event) {
    let window = event.target.defaultView;
    window.removeEventListener("load", this.loadListener);
    if (window.document.documentElement.getAttribute("windowtype") != "navigator:browser") {
      return;
    }

    for (let listener of this._openListeners) {
      listener(window);
    }
  },

  queryInterface: XPCOMUtils.generateQI([Ci.nsISupports, Ci.nsIObserver]),

  observe(window, topic, data) {
    if (topic == "domwindowclosed") {
      if (window.document.documentElement.getAttribute("windowtype") != "navigator:browser") {
        return;
      }

      window.removeEventListener("load", this);
      for (let listener of this._closeListeners) {
        listener(window);
      }
    } else {
      window.addEventListener("load", this);
    }
  },
};

// Provides a facility to listen for DOM events across all XUL windows.
global.AllWindowEvents = {
  _listeners: new Map(),

  // If |type| is a normal event type, invoke |listener| each time
  // that event fires in any open window. If |type| is "progress", add
  // a web progress listener that covers all open windows.
  addListener(type, listener) {
    if (type == "domwindowopened") {
      return WindowListManager.addOpenListener(listener);
    } else if (type == "domwindowclosed") {
      return WindowListManager.addCloseListener(listener);
    }

    let needOpenListener = this._listeners.size == 0;

    if (!this._listeners.has(type)) {
      this._listeners.set(type, new Set());
    }
    let list = this._listeners.get(type);
    list.add(listener);

    if (needOpenListener) {
      WindowListManager.addOpenListener(this.openListener);
    }
  },

  removeListener(type, listener) {
    if (type == "domwindowopened") {
      return WindowListManager.removeOpenListener(listener);
    } else if (type == "domwindowclosed") {
      return WindowListManager.removeCloseListener(listener);
    }

    let listeners = this._listeners.get(type);
    listeners.delete(listener);
    if (listeners.length == 0) {
      this._listeners.delete(type);
      if (this._listeners.size == 0) {
        WindowListManager.removeOpenListener(this.openListener);
      }
    }

    let e = Services.wm.getEnumerator("navigator:browser");
    while (e.hasMoreElements()) {
      let window = e.getNext();
      if (type == "progress") {
        window.gBrowser.removeTabsProgressListener(listener);
      } else {
        window.removeEventListener(type, listener);
      }
    }
  },

  // Runs whenever the "load" event fires for a new window.
  openListener(window) {
    for (let [eventType, listeners] of AllWindowEvents._listeners) {
      for (let listener of listeners) {
        if (eventType == "progress") {
          window.gBrowser.addTabsProgressListener(listener);
        } else {
          window.addEventListener(eventType, listener);
        }
      }
    }
  },
};

// Subclass of EventManager where we just need to call
// add/removeEventListener on each XUL window.
global.WindowEventManager = function(context, name, event, listener)
{
  EventManager.call(this, context, name, fire => {
    let listener2 = (...args) => listener(fire, ...args);
    AllWindowEvents.addListener(event, listener2);
    return () => {
      AllWindowEvents.removeListener(event, listener2);
    }
  });
}

WindowEventManager.prototype = Object.create(EventManager.prototype);
