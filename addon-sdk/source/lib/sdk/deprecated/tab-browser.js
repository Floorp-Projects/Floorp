/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
'use strict';

// The tab-browser module currently supports only Firefox.
// See: https://bugzilla.mozilla.org/show_bug.cgi?id=560716
module.metadata = {
  'stability': 'deprecated',
  'engines': {
    'Firefox': '*'
  }
};

const {Cc,Ci,Cu} = require('chrome');
var NetUtil = {};
Cu.import('resource://gre/modules/NetUtil.jsm', NetUtil);
NetUtil = NetUtil.NetUtil;
const errors = require('./errors');
const windowUtils = require('./window-utils');
const apiUtils = require('./api-utils');
const collection = require('../util/collection');
const { getMostRecentBrowserWindow } = require('../window/utils');
const { getSelectedTab } = require('../tabs/utils');

function onBrowserLoad(callback, event) {
  if (event.target && event.target.defaultView == this) {
    this.removeEventListener("load", onBrowserLoad, true);
    try {
      require("../timers").setTimeout(function () {
        callback(event);
      }, 10);
    } catch (e) { console.exception(e); }
  }
}

// Utility function to open a new browser window.
function openBrowserWindow(callback, url) {
  let ww = Cc["@mozilla.org/embedcomp/window-watcher;1"].
           getService(Ci.nsIWindowWatcher);
  let urlString = Cc["@mozilla.org/supports-string;1"].
                  createInstance(Ci.nsISupportsString);
  urlString.data = url;
  let window = ww.openWindow(null, "chrome://browser/content/browser.xul",
                             "_blank", "chrome,all,dialog=no", urlString);
  if (callback)
    window.addEventListener("load", onBrowserLoad.bind(window, callback), true);

  return window;
}

// Open a URL in a new tab
exports.addTab = function addTab(url, options) {
  if (!options)
    options = {};
  options.url = url;

  options = apiUtils.validateOptions(options, {
    // TODO: take URL object instead of string (bug 564524)
    url: {
      is: ["string"],
      ok: function (v) !!v,
      msg: "The url parameter must have be a non-empty string."
    },
    inNewWindow: {
      is: ["undefined", "null", "boolean"]
    },
    inBackground: {
      is: ["undefined", "null", "boolean"]
    },
    onLoad: {
      is: ["undefined", "null", "function"]
    },
    isPinned: {
      is: ["undefined", "boolean"]
    }
  });

  var win = getMostRecentBrowserWindow();
  if (!win || options.inNewWindow) {
    openBrowserWindow(function(e) {
      if(options.isPinned) {
        //get the active tab in the recently created window
        let mainWindow = e.target.defaultView;
        mainWindow.gBrowser.pinTab(getSelectedTab(mainWindow));
      }
      require("./errors").catchAndLog(function(e) options.onLoad(e))(e);
    }, options.url);
  }
  else {
    let tab = win.gBrowser.addTab(options.url);
    if (!options.inBackground)
      win.gBrowser.selectedTab = tab;
    if (options.onLoad) {
      let tabBrowser = win.gBrowser.getBrowserForTab(tab);
      tabBrowser.addEventListener("load", function onLoad(e) {
        if (e.target.defaultView.content.location == "about:blank")
          return;
        // remove event handler from addTab - don't want notified
        // for subsequent loads in same tab.
        tabBrowser.removeEventListener("load", onLoad, true);
        require("./errors").catchAndLog(function(e) options.onLoad(e))(e);
      }, true);
    }
  }
}

// Iterate over a window's tabbrowsers
function tabBrowserIterator(window) {
  var browsers = window.document.querySelectorAll("tabbrowser");
  for (var i = 0; i < browsers.length; i++)
    yield browsers[i];
}

// Iterate over a tabbrowser's tabs
function tabIterator(tabbrowser) {
  var tabs = tabbrowser.tabContainer;
  for (var i = 0; i < tabs.children.length; i++) {
    yield tabs.children[i];
  }
}

// Tracker for all tabbrowsers across all windows,
// or a single tabbrowser if the window is given.
function Tracker(delegate, window) {
  this._delegate = delegate;
  this._browsers = [];
  this._window = window;
  this._windowTracker = windowUtils.WindowTracker(this);

  require("../system/unload").ensure(this);
}
Tracker.prototype = {
  __iterator__: function __iterator__() {
    for (var i = 0; i < this._browsers.length; i++)
      yield this._browsers[i];
  },
  get: function get(index) {
    return this._browsers[index];
  },
  onTrack: function onTrack(window) {
    if (this._window && window != this._window)
      return;

    for (let browser in tabBrowserIterator(window))
      this._browsers.push(browser);
    if (this._delegate)
      for (let browser in tabBrowserIterator(window))
        this._delegate.onTrack(browser);
  },
  onUntrack: function onUntrack(window) {
    if (this._window && window != this._window)
      return;

    for (let browser in tabBrowserIterator(window)) {
      let index = this._browsers.indexOf(browser);
      if (index != -1)
        this._browsers.splice(index, 1);
      else
        console.error("internal error: browser tab not found");
    }
    if (this._delegate)
      for (let browser in tabBrowserIterator(window))
        this._delegate.onUntrack(browser);
  },
  get length() {
    return this._browsers.length;
  },
  unload: function unload() {
    this._windowTracker.unload();
  }
};
exports.Tracker = apiUtils.publicConstructor(Tracker);

// Tracker for all tabs across all windows,
// or a single window if it's given.
function TabTracker(delegate, window) {
  this._delegate = delegate;
  this._tabs = [];
  this._tracker = new Tracker(this, window);
  require("../system/unload").ensure(this);
}
TabTracker.prototype = {
  _TAB_EVENTS: ["TabOpen", "TabClose"],
  _safeTrackTab: function safeTrackTab(tab) {
    this._tabs.push(tab);
    try {
      this._delegate.onTrack(tab);
    } catch (e) {
      console.exception(e);
    }
  },
  _safeUntrackTab: function safeUntrackTab(tab) {
    var index = this._tabs.indexOf(tab);
    if (index == -1)
      console.error("internal error: tab not found");
    this._tabs.splice(index, 1);
    try {
      this._delegate.onUntrack(tab);
    } catch (e) {
      console.exception(e);
    }
  },
  handleEvent: function handleEvent(event) {
    switch (event.type) {
    case "TabOpen":
      this._safeTrackTab(event.target);
      break;
    case "TabClose":
      this._safeUntrackTab(event.target);
      break;
    default:
      throw new Error("internal error: unknown event type: " +
                      event.type);
    }
  },
  onTrack: function onTrack(tabbrowser) {
    for (let tab in tabIterator(tabbrowser))
      this._safeTrackTab(tab);
    var self = this;
    this._TAB_EVENTS.forEach(
      function(eventName) {
        tabbrowser.tabContainer.addEventListener(eventName, self, true);
      });
  },
  onUntrack: function onUntrack(tabbrowser) {
    for (let tab in tabIterator(tabbrowser))
      this._safeUntrackTab(tab);
    var self = this;
    this._TAB_EVENTS.forEach(
      function(eventName) {
        tabbrowser.tabContainer.removeEventListener(eventName, self, true);
      });
  },
  unload: function unload() {
    this._tracker.unload();
  }
};
exports.TabTracker = apiUtils.publicConstructor(TabTracker);

exports.whenContentLoaded = function whenContentLoaded(callback) {
  var cb = require("./errors").catchAndLog(function eventHandler(event) {
    if (event.target && event.target.defaultView)
      callback(event.target.defaultView);
  });

  var tracker = new Tracker({
    onTrack: function(tabBrowser) {
      tabBrowser.addEventListener("DOMContentLoaded", cb, false);
    },
    onUntrack: function(tabBrowser) {
      tabBrowser.removeEventListener("DOMContentLoaded", cb, false);
    }
  });

  return tracker;
};

Object.defineProperty(exports, 'activeTab', {
  get: function() {
    return getSelectedTab(getMostRecentBrowserWindow());
  }
});

/******************* TabModule *********************/

// Supported tab events
const events = [
  "onActivate",
  "onDeactivate",
  "onOpen",
  "onClose",
  "onReady",
  "onLoad",
  "onPaint"
];
exports.tabEvents = events;

/**
 * TabModule
 *
 * Constructor for a module that implements the tabs API
 */
let TabModule = exports.TabModule = function TabModule(window) {
  let self = this;
  /**
   * Tab
   *
   * Safe object representing a tab.
   */
  let tabConstructor = apiUtils.publicConstructor(function(element) {
    if (!element)
      throw new Error("no tab element.");
    let win = element.ownerDocument.defaultView;
    if (!win)
      throw new Error("element has no window.");
    if (window && win != window)
      throw new Error("module's window and element's window don't match.");
    let browser = win.gBrowser.getBrowserForTab(element);

    this.__defineGetter__("title", function() browser.contentDocument.title);
    this.__defineGetter__("location", function() browser.contentDocument.location);
    this.__defineSetter__("location", function(val) browser.contentDocument.location = val);
    this.__defineGetter__("contentWindow", function() browser.contentWindow);
    this.__defineGetter__("contentDocument", function() browser.contentDocument);
    this.__defineGetter__("favicon", function() {
      let pageURI = NetUtil.newURI(browser.contentDocument.location);
      let fs = Cc["@mozilla.org/browser/favicon-service;1"].
               getService(Ci.nsIFaviconService);
      let faviconURL;
      try {
        let faviconURI = fs.getFaviconForPage(pageURI);
        faviconURL = fs.getFaviconDataAsDataURL(faviconURI);
      } catch(ex) {
        let data = getChromeURLContents("chrome://mozapps/skin/places/defaultFavicon.png");
        let encoded = browser.contentWindow.btoa(data);
        faviconURL = "data:image/png;base64," + encoded;
      }
      return faviconURL;
    });
    this.__defineGetter__("style", function() null); // TODO
    this.__defineGetter__("index", function() win.gBrowser.getBrowserIndexForDocument(browser.contentDocument));
    this.__defineGetter__("thumbnail", function() getThumbnailCanvasForTab(element, browser.contentWindow));

    this.close = function() win.gBrowser.removeTab(element);
    this.move = function(index) {
      win.gBrowser.moveTabTo(element, index);
    };

    this.__defineGetter__("isPinned", function() element.pinned);
    this.pin = function() win.gBrowser.pinTab(element);
    this.unpin = function() win.gBrowser.unpinTab(element);

    // Set up the event handlers
    let tab = this;
    events.filter(function(e) e != "onOpen").forEach(function(e) {
      // create a collection for each event
      collection.addCollectionProperty(tab, e);
      // make tabs setter for each event, for adding via property assignment
      tab.__defineSetter__(e, function(val) tab[e].add(val));
    });

    // listen for events, filtered on this tab
    eventsTabDelegate.addTabDelegate(this);
  });

  /**
   * tabs.activeTab
   */
  this.__defineGetter__("activeTab", function() {
    try {
      return window ? tabConstructor(getSelectedTab(window))
                    : tabConstructor(exports.activeTab);
    }
    catch (e) { }
    return null;
  });
  this.__defineSetter__("activeTab", function(tab) {
    let [tabElement, win] = getElementAndWindowForTab(tab, window);
    if (tabElement) {
      // set as active tab
      win.gBrowser.selectedTab = tabElement;
      // focus the window
      win.focus();
    }
  });

  this.open = function TM_open(options) {
    open(options, tabConstructor, window);
  }

  // Set up the event handlers
  events.forEach(function(eventHandler) {
    // create a collection for each event
    collection.addCollectionProperty(self, eventHandler);
    // make tabs setter for each event, for adding via property assignment
    self.__defineSetter__(eventHandler, function(val) self[eventHandler].add(val));
  });

  // Tracker that listens for tab events, and proxies
  // them to registered event listeners.
  let eventsTabDelegate = {
    selectedTab: null,
    tabs: [],
    addTabDelegate: function TETT_addTabDelegate(tabObj) {
      this.tabs.push(tabObj);
    },
    pushTabEvent: function TETT_pushTabEvent(event, tab) {
      for (let callback in self[event]) {
        require("./errors").catchAndLog(function(tab) {
          callback(new tabConstructor(tab));
        })(tab);
      }

      if (event != "onOpen") {
        this.tabs.forEach(function(tabObj) {
          if (tabObj[event].length) {
            let [tabEl,] = getElementAndWindowForTab(tabObj, window);
            if (tabEl == tab) {
              for (let callback in tabObj[event])
                require("./errors").catchAndLog(function() callback())();
            }
          }
          // if being closed, remove the tab object from the cache
          // of tabs to notify about events.
          if (event == "onClose")
            this.tabs.splice(this.tabs.indexOf(tabObj), 1);
        }, this);
      }
    },
    unload: function() {
      this.selectedTab = null;
      this.tabs.splice(0);
    }
  };
  require("../system/unload").ensure(eventsTabDelegate);

  let eventsTabTracker = new ModuleTabTracker({
    onTrack: function TETT_onTrack(tab) {
      eventsTabDelegate.pushTabEvent("onOpen", tab);
    },
    onUntrack: function TETT_onUntrack(tab) {
      eventsTabDelegate.pushTabEvent("onClose", tab);
    },
    onSelect: function TETT_onSelect(tab) {
      if (eventsTabDelegate.selectedTab)
        eventsTabDelegate.pushTabEvent("onDeactivate", tab);

      eventsTabDelegate.selectedTab = new tabConstructor(tab);

      eventsTabDelegate.pushTabEvent("onActivate", tab);
    },
    onReady: function TETT_onReady(tab) {
      eventsTabDelegate.pushTabEvent("onReady", tab);
    },
    onLoad: function TETT_onLoad(tab) {
      eventsTabDelegate.pushTabEvent("onLoad", tab);
    },
    onPaint: function TETT_onPaint(tab) {
      eventsTabDelegate.pushTabEvent("onPaint", tab);
    }
  }, window);
  require("../system/unload").ensure(eventsTabTracker);

  // Iterator for all tabs
  this.__iterator__ = function tabsIterator() {
    for (let i = 0; i < eventsTabTracker._tabs.length; i++)
      yield tabConstructor(eventsTabTracker._tabs[i]);
  }

  this.__defineGetter__("length", function() eventsTabTracker._tabs.length);

  // Cleanup when unloaded
  this.unload = function TM_unload() {
    // Unregister tabs event listeners
    events.forEach(function(e) self[e] = []);
  }
  require("../system/unload").ensure(this);

} // End of TabModule constructor

/**
 * tabs.open - open a URL in a new tab
 */
function open(options, tabConstructor, window) {
  if (typeof options === "string")
    options = { url: options };

  options = apiUtils.validateOptions(options, {
    url: {
      is: ["string"]
    },
    inNewWindow: {
      is: ["undefined", "boolean"]
    },
    inBackground: {
      is: ["undefined", "boolean"]
    },
    isPinned: {
      is: ["undefined", "boolean"]
    },
    onOpen: {
      is: ["undefined", "function"]
    }
  });

  if (window)
    options.inNewWindow = false;

  let win = window || require("./window-utils").activeBrowserWindow;

  if (!win || options.inNewWindow)
    openURLInNewWindow(options, tabConstructor);
  else
    openURLInNewTab(options, win, tabConstructor);
}

function openURLInNewWindow(options, tabConstructor) {
  let addTabOptions = {
    inNewWindow: true
  };
  if (options.onOpen) {
    addTabOptions.onLoad = function(e) {
      let win = e.target.defaultView;
      let tabEl = win.gBrowser.tabContainer.childNodes[0];
      let tabBrowser = win.gBrowser.getBrowserForTab(tabEl);
      tabBrowser.addEventListener("load", function onLoad(e) {
        tabBrowser.removeEventListener("load", onLoad, true);
        let tab = tabConstructor(tabEl);
        require("./errors").catchAndLog(function(e) options.onOpen(e))(tab);
      }, true);
    };
  }
  if (options.isPinned) {
    addTabOptions.isPinned = true;
  }
  exports.addTab(options.url.toString(), addTabOptions);
}

function openURLInNewTab(options, window, tabConstructor) {
  window.focus();
  let tabEl = window.gBrowser.addTab(options.url.toString());
  if (!options.inBackground)
    window.gBrowser.selectedTab = tabEl;
  if (options.isPinned)
    window.gBrowser.pinTab(tabEl);
  if (options.onOpen) {
    let tabBrowser = window.gBrowser.getBrowserForTab(tabEl);
    tabBrowser.addEventListener("load", function onLoad(e) {
      // remove event handler from addTab - don't want to be notified
      // for subsequent loads in same tab.
      tabBrowser.removeEventListener("load", onLoad, true);
      let tab = tabConstructor(tabEl);
      require("../timers").setTimeout(function() {
        require("./errors").catchAndLog(function(tab) options.onOpen(tab))(tab);
      }, 10);
    }, true);
  }
}

function getElementAndWindowForTab(tabObj, window) {
  // iterate over open windows, or use single window if provided
  let windowIterator = window ? function() { yield window; }
                              : require("./window-utils").windowIterator;
  for (let win in windowIterator()) {
    if (win.gBrowser) {
      // find the tab element at tab.index
      let index = win.gBrowser.getBrowserIndexForDocument(tabObj.contentDocument);
      if (index > -1)
        return [win.gBrowser.tabContainer.getItemAtIndex(index), win];
    }
  }
  return [null, null];
}

// Tracker for all tabs across all windows
// This is tab-browser.TabTracker, but with
// support for additional events added.
function ModuleTabTracker(delegate, window) {
  this._delegate = delegate;
  this._tabs = [];
  this._tracker = new Tracker(this, window);
  require("../system/unload").ensure(this);
}
ModuleTabTracker.prototype = {
  _TAB_EVENTS: ["TabOpen", "TabClose", "TabSelect", "DOMContentLoaded",
                "load", "MozAfterPaint"],
  _safeTrackTab: function safeTrackTab(tab) {
    tab.linkedBrowser.addEventListener("load", this, true);
    tab.linkedBrowser.addEventListener("MozAfterPaint", this, false);
    this._tabs.push(tab);
    try {
      this._delegate.onTrack(tab);
    } catch (e) {
      console.exception(e);
    }
  },
  _safeUntrackTab: function safeUntrackTab(tab) {
    tab.linkedBrowser.removeEventListener("load", this, true);
    tab.linkedBrowser.removeEventListener("MozAfterPaint", this, false);
    var index = this._tabs.indexOf(tab);
    if (index == -1)
      throw new Error("internal error: tab not found");
    this._tabs.splice(index, 1);
    try {
      this._delegate.onUntrack(tab);
    } catch (e) {
      console.exception(e);
    }
  },
  _safeSelectTab: function safeSelectTab(tab) {
    var index = this._tabs.indexOf(tab);
    if (index == -1)
      console.error("internal error: tab not found");
    try {
      if (this._delegate.onSelect)
        this._delegate.onSelect(tab);
    } catch (e) {
      console.exception(e);
    }
  },
  _safeDOMContentLoaded: function safeDOMContentLoaded(event) {
    let tabBrowser = event.currentTarget;
    let tabBrowserIndex = tabBrowser.getBrowserIndexForDocument(event.target);
    // TODO: I'm seeing this when loading data url images
    if (tabBrowserIndex == -1)
      return;
    let tab = tabBrowser.tabContainer.getItemAtIndex(tabBrowserIndex);
    let index = this._tabs.indexOf(tab);
    if (index == -1)
      console.error("internal error: tab not found");
    try {
      if (this._delegate.onReady)
        this._delegate.onReady(tab);
    } catch (e) {
      console.exception(e);
    }
  },
  _safeLoad: function safeLoad(event) {
    let win = event.currentTarget.ownerDocument.defaultView;
    let tabIndex = win.gBrowser.getBrowserIndexForDocument(event.target);
    if (tabIndex == -1)
      return;
    let tab = win.gBrowser.tabContainer.getItemAtIndex(tabIndex);
    let index = this._tabs.indexOf(tab);
    if (index == -1)
      console.error("internal error: tab not found");
    try {
      if (this._delegate.onLoad)
        this._delegate.onLoad(tab);
    } catch (e) {
      console.exception(e);
    }
  },
  _safeMozAfterPaint: function safeMozAfterPaint(event) {
    let win = event.currentTarget.ownerDocument.defaultView;
    let tabIndex = win.gBrowser.getBrowserIndexForDocument(event.target.document);
    if (tabIndex == -1)
      return;
    let tab = win.gBrowser.tabContainer.getItemAtIndex(tabIndex);
    let index = this._tabs.indexOf(tab);
    if (index == -1)
      console.error("internal error: tab not found");
    try {
      if (this._delegate.onPaint)
        this._delegate.onPaint(tab);
    } catch (e) {
      console.exception(e);
    }
  },
  handleEvent: function handleEvent(event) {
    switch (event.type) {
    case "TabOpen":
      this._safeTrackTab(event.target);
      break;
    case "TabClose":
      this._safeUntrackTab(event.target);
      break;
    case "TabSelect":
      this._safeSelectTab(event.target);
      break;
    case "DOMContentLoaded":
      this._safeDOMContentLoaded(event);
      break;
    case "load":
      this._safeLoad(event);
      break;
    case "MozAfterPaint":
      this._safeMozAfterPaint(event);
      break;
    default:
      throw new Error("internal error: unknown event type: " +
                      event.type);
    }
  },
  onTrack: function onTrack(tabbrowser) {
    for (let tab in tabIterator(tabbrowser))
      this._safeTrackTab(tab);
    tabbrowser.tabContainer.addEventListener("TabOpen", this, false);
    tabbrowser.tabContainer.addEventListener("TabClose", this, false);
    tabbrowser.tabContainer.addEventListener("TabSelect", this, false);
    tabbrowser.ownerDocument.defaultView.gBrowser.addEventListener("DOMContentLoaded", this, false);
  },
  onUntrack: function onUntrack(tabbrowser) {
    for (let tab in tabIterator(tabbrowser))
      this._safeUntrackTab(tab);
    tabbrowser.tabContainer.removeEventListener("TabOpen", this, false);
    tabbrowser.tabContainer.removeEventListener("TabClose", this, false);
    tabbrowser.tabContainer.removeEventListener("TabSelect", this, false);
    tabbrowser.ownerDocument.defaultView.gBrowser.removeEventListener("DOMContentLoaded", this, false);
  },
  unload: function unload() {
    this._tracker.unload();
  }
};

// Utility to get a thumbnail canvas from a tab object
function getThumbnailCanvasForTab(tabEl, window) {
  var thumbnail = window.document.createElementNS("http://www.w3.org/1999/xhtml", "canvas");
  thumbnail.mozOpaque = true;
  window = tabEl.linkedBrowser.contentWindow;
  thumbnail.width = Math.ceil(window.screen.availWidth / 5.75);
  var aspectRatio = 0.5625; // 16:9
  thumbnail.height = Math.round(thumbnail.width * aspectRatio);
  var ctx = thumbnail.getContext("2d");
  var snippetWidth = window.innerWidth * .6;
  var scale = thumbnail.width / snippetWidth;
  ctx.scale(scale, scale);
  ctx.drawWindow(window, window.scrollX, window.scrollY, snippetWidth, snippetWidth * aspectRatio, "rgb(255,255,255)");
  return thumbnail;
}

// Utility to return the contents of the target of a chrome URL
function getChromeURLContents(chromeURL) {
  let io = Cc["@mozilla.org/network/io-service;1"].
           getService(Ci.nsIIOService);
  let channel = io.newChannel(chromeURL, null, null);
  let input = channel.open();
  let stream = Cc["@mozilla.org/binaryinputstream;1"].
               createInstance(Ci.nsIBinaryInputStream);
  stream.setInputStream(input);
  let str = stream.readBytes(input.available());
  stream.close();
  input.close();
  return str;
}
