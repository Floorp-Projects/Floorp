/* -*- Mode: indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set sts=2 sw=2 et tw=80: */
"use strict";

XPCOMUtils.defineLazyModuleGetter(this, "PrivateBrowsingUtils",
                                  "resource://gre/modules/PrivateBrowsingUtils.jsm");

Cu.import("resource://gre/modules/ExtensionUtils.jsm");
var {
  EventManager,
} = ExtensionUtils;

// This file provides some useful code for the |tabs| and |windows|
// modules. All of the code is installed on |global|, which is a scope
// shared among the different ext-*.js scripts.


// Manages icon details for toolbar buttons in the |pageAction| and
// |browserAction| APIs.
global.IconDetails = {
  // Accepted icon sizes.
  SIZES: ["19", "38"],

  // Normalizes the various acceptable input formats into an object
  // with two properties, "19" and "38", containing icon URLs.
  normalize(details, extension, context=null, localize=false) {
    let result = {};

    if (details.imageData) {
      let imageData = details.imageData;

      if (imageData instanceof Cu.getGlobalForObject(imageData).ImageData) {
        imageData = {"19": imageData};
      }

      for (let size of this.SIZES) {
        if (size in imageData) {
          result[size] = this.convertImageDataToPNG(imageData[size], context);
        }
      }
    }

    if (details.path) {
      let path = details.path;
      if (typeof path != "object") {
        path = {"19": path};
      }

      let baseURI = context ? context.uri : extension.baseURI;

      for (let size of this.SIZES) {
        if (size in path) {
          let url = path[size];
          if (localize) {
            url = extension.localize(url);
          }

          url = baseURI.resolve(path[size]);

          // The Chrome documentation specifies these parameters as
          // relative paths. We currently accept absolute URLs as well,
          // which means we need to check that the extension is allowed
          // to load them.
          try {
            Services.scriptSecurityManager.checkLoadURIStrWithPrincipal(
              extension.principal, url,
              Services.scriptSecurityManager.DISALLOW_SCRIPT);
          } catch (e if !context) {
            // If there's no context, it's because we're handling this
            // as a manifest directive. Log a warning rather than
            // raising an error, but don't accept the URL in any case.
            extension.manifestError(`Access to URL '${url}' denied`);
            continue;
          }

          result[size] = url;
        }
      }
    }

    return result;
  },

  // Returns the appropriate icon URL for the given icons object and the
  // screen resolution of the given window.
  getURL(icons, window, extension) {
    const DEFAULT = "chrome://browser/content/extension.svg";

    // Use the higher resolution image if we're doing any up-scaling
    // for high resolution monitors.
    let res = window.devicePixelRatio;
    let size = res > 1 ? "38" : "19";

    return icons[size] || icons["19"] || icons["38"] || DEFAULT;
  },

  convertImageDataToPNG(imageData, context) {
    let document = context.contentWindow.document;
    let canvas = document.createElementNS("http://www.w3.org/1999/xhtml", "canvas");
    canvas.width = imageData.width;
    canvas.height = imageData.height;
    canvas.getContext("2d").putImageData(imageData, 0, 0);

    return canvas.toDataURL("image/png");
  }
};

global.makeWidgetId = id => {
  id = id.toLowerCase();
  // FIXME: This allows for collisions.
  return id.replace(/[^a-z0-9_-]/g, "_");
}

// Open a panel anchored to the given node, containing a browser opened
// to the given URL, owned by the given extension. If |popupURL| is not
// an absolute URL, it is resolved relative to the given extension's
// base URL.
global.openPanel = (node, popupURL, extension) => {
  let document = node.ownerDocument;

  let popupURI = Services.io.newURI(popupURL, null, extension.baseURI);

  Services.scriptSecurityManager.checkLoadURIWithPrincipal(
    extension.principal, popupURI,
    Services.scriptSecurityManager.DISALLOW_SCRIPT);

  let panel = document.createElement("panel");
  panel.setAttribute("id", makeWidgetId(extension.id) + "-panel");
  panel.setAttribute("class", "browser-extension-panel");
  panel.setAttribute("type", "arrow");
  panel.setAttribute("role", "group");

  let anchor;
  if (node.localName == "toolbarbutton") {
    // Toolbar buttons are a special case. The panel becomes a child of
    // the button, and is anchored to the button's icon.
    node.appendChild(panel);
    anchor = document.getAnonymousElementByAttribute(node, "class", "toolbarbutton-icon");
  } else {
    // In all other cases, the panel is anchored to the target node
    // itself, and is a child of a popupset node.
    document.getElementById("mainPopupSet").appendChild(panel);
    anchor = node;
  }

  const XUL_NS = "http://www.mozilla.org/keymaster/gatekeeper/there.is.only.xul";
  let browser = document.createElementNS(XUL_NS, "browser");
  browser.setAttribute("type", "content");
  browser.setAttribute("disableglobalhistory", "true");
  panel.appendChild(browser);

  let titleChangedListener = () => {
    panel.setAttribute("aria-label", browser.contentTitle);
  }

  let context;
  panel.addEventListener("popuphidden", () => {
    browser.removeEventListener("DOMTitleChanged", titleChangedListener, true);
    context.unload();
    panel.remove();
  });

  let loadListener = () => {
    panel.removeEventListener("load", loadListener);

    context = new ExtensionPage(extension, {
      type: "popup",
      contentWindow: browser.contentWindow,
      uri: popupURI,
      docShell: browser.docShell,
    });
    GlobalManager.injectInDocShell(browser.docShell, extension, context);
    browser.setAttribute("src", context.uri.spec);

    let contentLoadListener = () => {
      browser.removeEventListener("load", contentLoadListener, true);

      let contentViewer = browser.docShell.contentViewer;
      let width = {}, height = {};
      try {
        contentViewer.getContentSize(width, height);
        [width, height] = [width.value, height.value];
      } catch (e) {
        // getContentSize can throw
        [width, height] = [400, 400];
      }

      let window = document.defaultView;
      width /= window.devicePixelRatio;
      height /= window.devicePixelRatio;
      width = Math.min(width, 800);
      height = Math.min(height, 800);

      browser.setAttribute("width", width);
      browser.setAttribute("height", height);

      panel.openPopup(anchor, "bottomcenter topright", 0, 0, false, false);
    };
    browser.addEventListener("load", contentLoadListener, true);

    browser.addEventListener("DOMTitleChanged", titleChangedListener, true);
  };
  panel.addEventListener("load", loadListener);

  return panel;
}

// Manages tab-specific context data, and dispatching tab select events
// across all windows.
global.TabContext = function TabContext(getDefaults, extension) {
  this.extension = extension;
  this.getDefaults = getDefaults;

  this.tabData = new WeakMap();

  AllWindowEvents.addListener("progress", this);
  AllWindowEvents.addListener("TabSelect", this);

  EventEmitter.decorate(this);
}

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

  onLocationChange(browser, webProgress, request, locationURI, flags) {
    let gBrowser = browser.ownerDocument.defaultView.gBrowser;
    if (browser === gBrowser.selectedBrowser) {
      let tab = gBrowser.getTabForBrowser(browser);
      this.emit("location-change", tab, true);
    }
  },

  shutdown() {
    AllWindowEvents.removeListener("progress", this);
    AllWindowEvents.removeListener("TabSelect", this);
  },
};

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
    for (let window of WindowListManager.browserWindows()) {
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
    for (let window of WindowListManager.browserWindows(true)) {
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

  // Returns an iterator for all browser windows. Unless |includeIncomplete| is
  // true, only fully-loaded windows are returned.
  *browserWindows(includeIncomplete = false) {
    let e = Services.wm.getEnumerator("navigator:browser");
    while (e.hasMoreElements()) {
      let window = e.getNext();
      if (includeIncomplete || window.document.readyState == "complete") {
        yield window;
      }
    }
  },

  addOpenListener(listener, fireOnExisting = true) {
    if (this._openListeners.length == 0 && this._closeListeners.length == 0) {
      Services.ww.registerNotification(this);
    }
    this._openListeners.add(listener);

    for (let window of this.browserWindows(true)) {
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
      WindowListManager.addOpenListener(this.openListener, false);
    }

    for (let window of WindowListManager.browserWindows()) {
      this.addWindowListener(window, type, listener);
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

    for (let window of WindowListManager.browserWindows()) {
      if (type == "progress") {
        window.gBrowser.removeTabsProgressListener(listener);
      } else {
        window.removeEventListener(type, listener);
      }
    }
  },

  addWindowListener(window, eventType, listener) {
    if (eventType == "progress") {
      window.gBrowser.addTabsProgressListener(listener);
    } else {
      window.addEventListener(eventType, listener);
    }
  },

  // Runs whenever the "load" event fires for a new window.
  openListener(window) {
    for (let [eventType, listeners] of AllWindowEvents._listeners) {
      for (let listener of listeners) {
        this.addWindowListener(window, eventType, listener);
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
