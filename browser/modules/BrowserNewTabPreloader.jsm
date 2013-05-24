/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

this.EXPORTED_SYMBOLS = ["BrowserNewTabPreloader"];

const Cu = Components.utils;
const Cc = Components.classes;
const Ci = Components.interfaces;

Cu.import("resource://gre/modules/Services.jsm");
Cu.import("resource://gre/modules/XPCOMUtils.jsm");

const HTML_NS = "http://www.w3.org/1999/xhtml";
const XUL_NS = "http://www.mozilla.org/keymaster/gatekeeper/there.is.only.xul";
const XUL_PAGE = "data:application/vnd.mozilla.xul+xml;charset=utf-8,<window id='win'/>";
const PREF_BRANCH = "browser.newtab.";

// The interval between swapping in a preload docShell and kicking off the
// next preload in the background.
const PRELOADER_INTERVAL_MS = 3000;
// The initial delay before we start preloading our first new tab page. The
// timer is started after the first 'browser-delayed-startup' has been sent.
const PRELOADER_INIT_DELAY_MS = 5000;
// The number of miliseconds we'll wait after we received a notification that
// causes us to update our list of browsers and tabbrowser sizes. This acts as
// kind of a damper when too many events are occuring in quick succession.
const PRELOADER_UPDATE_DELAY_MS = 3000;

const TOPIC_TIMER_CALLBACK = "timer-callback";
const TOPIC_DELAYED_STARTUP = "browser-delayed-startup-finished";
const TOPIC_XUL_WINDOW_CLOSED = "xul-window-destroyed";

function createTimer(obj, delay) {
  let timer = Cc["@mozilla.org/timer;1"].createInstance(Ci.nsITimer);
  timer.init(obj, delay, Ci.nsITimer.TYPE_ONE_SHOT);
  return timer;
}

function clearTimer(timer) {
  if (timer) {
    timer.cancel();
  }
  return null;
}

this.BrowserNewTabPreloader = {
  init: function Preloader_init() {
    Initializer.start();
  },

  uninit: function Preloader_uninit() {
    Initializer.stop();
    HostFrame.destroy();
    Preferences.uninit();
    HiddenBrowsers.uninit();
  },

  newTab: function Preloader_newTab(aTab) {
    let win = aTab.ownerDocument.defaultView;
    if (win.gBrowser) {
      let {boxObject: {width, height}} = win.gBrowser;
      let hiddenBrowser = HiddenBrowsers.get(width, height)
      if (hiddenBrowser) {
        return hiddenBrowser.swapWithNewTab(aTab);
      }
    }

    return false;
  }
};

Object.freeze(BrowserNewTabPreloader);

let Initializer = {
  _timer: null,
  _observing: false,

  start: function Initializer_start() {
    Services.obs.addObserver(this, TOPIC_DELAYED_STARTUP, false);
    this._observing = true;
  },

  stop: function Initializer_stop() {
    this._timer = clearTimer(this._timer);

    if (this._observing) {
      Services.obs.removeObserver(this, TOPIC_DELAYED_STARTUP);
      this._observing = false;
    }
  },

  observe: function Initializer_observe(aSubject, aTopic, aData) {
    if (aTopic == TOPIC_DELAYED_STARTUP) {
      Services.obs.removeObserver(this, TOPIC_DELAYED_STARTUP);
      this._observing = false;
      this._startTimer();
    } else if (aTopic == TOPIC_TIMER_CALLBACK) {
      this._timer = null;
      this._startPreloader();
    }
  },

  _startTimer: function Initializer_startTimer() {
    this._timer = createTimer(this, PRELOADER_INIT_DELAY_MS);
  },

  _startPreloader: function Initializer_startPreloader() {
    Preferences.init();
    if (Preferences.enabled) {
      HiddenBrowsers.init();
    }
  }
};

let Preferences = {
  _enabled: null,
  _branch: null,
  _url: null,

  get enabled() {
    if (this._enabled === null) {
      this._enabled = this._branch.getBoolPref("preload") &&
                      !this._branch.prefHasUserValue("url") &&
                      this.url && this.url != "about:blank";
    }

    return this._enabled;
  },

  get url() {
    if (this._url === null) {
      this._url = this._branch.getCharPref("url");
    }

    return this._url;
  },

  init: function Preferences_init() {
    this._branch = Services.prefs.getBranch(PREF_BRANCH);
    this._branch.addObserver("", this, false);
  },

  uninit: function Preferences_uninit() {
    if (this._branch) {
      this._branch.removeObserver("", this);
      this._branch = null;
    }
  },

  observe: function Preferences_observe(aSubject, aTopic, aData) {
    let {url: prevURL, enabled: prevEnabled} = this;
    this._url = this._enabled = null;

    if (prevEnabled && !this.enabled) {
      HiddenBrowsers.uninit();
    } else if (!prevEnabled && this.enabled) {
      HiddenBrowsers.init();
    } else if (this._browser && prevURL != this.url) {
      HiddenBrowsers.updateBrowserURLs(this.url);
    }
  },
};

let HiddenBrowsers = {
  _browsers: null,
  _updateTimer: null,

  _topics: [
    TOPIC_DELAYED_STARTUP,
    TOPIC_XUL_WINDOW_CLOSED
  ],

  init: function () {
    this._browsers = new Map();
    this._updateBrowserSizes();
    this._topics.forEach(t => Services.obs.addObserver(this, t, false));
  },

  uninit: function () {
    this._topics.forEach(t => Services.obs.removeObserver(this, t, false));
    this._updateTimer = clearTimer(this._updateTimer);

    if (this._browsers) {
      for (let [key, browser] of this._browsers) {
        browser.destroy();
      }
      this._browsers = null;
    }
  },

  get: function (width, height) {
    // We haven't been initialized, yet.
    if (!this._browsers) {
      return null;
    }

    let key = width + "x" + height;
    if (!this._browsers.has(key)) {
      // Update all browsers' sizes if we can't find a matching one.
      this._updateBrowserSizes();
    }

    // We should now have a matching browser.
    if (this._browsers.has(key)) {
      return this._browsers.get(key);
    }

    // We should never be here. Return the first browser we find.
    Cu.reportError("NewTabPreloader: no matching browser found after updating");
    for (let [size, browser] of this._browsers) {
      return browser;
    }

    // We should really never be here.
    Cu.reportError("NewTabPreloader: not even a single browser was found?");
    return null;
  },

  observe: function (subject, topic, data) {
    if (topic === TOPIC_TIMER_CALLBACK) {
      this._updateTimer = null;
      this._updateBrowserSizes();
    } else {
      this._updateTimer = clearTimer(this._updateTimer);
      this._updateTimer = createTimer(this, PRELOADER_UPDATE_DELAY_MS);
    }
  },

  updateBrowserURLs: function (url) {
    for (let [key, browser] of this._browsers) {
      browser.update(url);
    }
  },

  _updateBrowserSizes: function () {
    let sizes = this._collectTabBrowserSizes();
    let toRemove = [];

    // Iterate all browsers and check that they
    // each can be assigned to one of the sizes.
    for (let [key, browser] of this._browsers) {
      if (sizes.has(key)) {
        // We already have a browser for that size, great!
        sizes.delete(key);
      } else {
        // This browser is superfluous or needs to be resized.
        toRemove.push(browser);
        this._browsers.delete(key);
      }
    }

    // Iterate all sizes that we couldn't find a browser for.
    for (let [key, {width, height}] of sizes) {
      let browser;
      if (toRemove.length) {
        // Let's just resize one of the superfluous
        // browsers and put it back into the map.
        browser = toRemove.shift();
        browser.resize(width, height);
      } else {
        // No more browsers to reuse, create a new one.
        browser = new HiddenBrowser(width, height);
      }

      this._browsers.set(key, browser);
    }

    // Finally, remove all browsers we don't need anymore.
    toRemove.forEach(b => b.destroy());
  },

  _collectTabBrowserSizes: function () {
    let sizes = new Map();

    function tabBrowsers() {
      let wins = Services.ww.getWindowEnumerator("navigator:browser");
      while (wins.hasMoreElements()) {
        let win = wins.getNext();
        if (win.gBrowser) {
          yield win.gBrowser;
        }
      }
    }

    // Collect the sizes of all <tabbrowser>s out there.
    for (let {boxObject: {width, height}} of tabBrowsers()) {
      if (width > 0 && height > 0) {
        let key = width + "x" + height;
        if (!sizes.has(key)) {
          sizes.set(key, {width: width, height: height});
        }
      }
    }

    return sizes;
  }
};

function HiddenBrowser(width, height) {
  this.resize(width, height);

  HostFrame.get(aFrame => {
    let doc = aFrame.document;
    this._browser = doc.createElementNS(XUL_NS, "browser");
    this._browser.setAttribute("type", "content");
    doc.getElementById("win").appendChild(this._browser);
    this.preload();
  });
}

HiddenBrowser.prototype = {
  _width: null,
  _height: null,
  _timer: null,

  get isPreloaded() {
    return this._browser &&
           this._browser.contentDocument &&
           this._browser.contentDocument.readyState == "complete" &&
           this._browser.currentURI.spec == Preferences.url;
  },

  swapWithNewTab: function (aTab) {
    if (!this.isPreloaded || this._timer) {
      return false;
    }

    let tabbrowser = aTab.ownerDocument.defaultView.gBrowser;
    if (!tabbrowser) {
      return false;
    }

    // Swap docShells.
    tabbrowser.swapNewTabWithBrowser(aTab, this._browser);

    // Start a timer that will kick off preloading the next newtab page.
    this._timer = createTimer(this, PRELOADER_INTERVAL_MS);

    // Signal that we swapped docShells.
    return true;
  },

  observe: function () {
    this._timer = null;
    this.preload();
  },

  preload: function () {
    if (!this._browser) {
      return;
    }

    // Make sure the browser has the right size.
    this.resize(this._width, this._height);

    // Start pre-loading the new tab page.
    this._browser.loadURI(Preferences.url);
  },

  resize: function (width, height) {
    if (this._browser) {
      this._browser.style.width = width + "px";
      this._browser.style.height = height + "px";
    } else {
      this._width = width;
      this._height = height;
    }
  },

  update: function (aURL) {
    this._browser.setAttribute("src", aURL);
  },

  destroy: function () {
    if (this._browser) {
      this._browser.remove();
      this._browser = null;
    }

    this._timer = clearTimer(this._timer);
  }
};

let HostFrame = {
  _frame: null,
  _loading: false,

  get hiddenDOMDocument() {
    return Services.appShell.hiddenDOMWindow.document;
  },

  get isReady() {
    return this.hiddenDOMDocument.readyState === "complete";
  },

  get: function (callback) {
    if (this._frame) {
      callback(this._frame);
    } else if (this.isReady && !this._loading) {
      this._create(callback);
      this._loading = true;
    } else {
      Services.tm.currentThread.dispatch(() => HostFrame.get(callback),
                                         Ci.nsIThread.DISPATCH_NORMAL);
    }
  },

  destroy: function () {
    this._frame = null;
  },

  _create: function (callback) {
    let doc = this.hiddenDOMDocument;
    let iframe = doc.createElementNS(HTML_NS, "iframe");
    doc.documentElement.appendChild(iframe);

    let frame = iframe.contentWindow;
    let docShell = frame.QueryInterface(Ci.nsIInterfaceRequestor)
                        .getInterface(Ci.nsIDocShell);

    docShell.createAboutBlankContentViewer(null);
    frame.location = XUL_PAGE;

    let eventHandler = docShell.chromeEventHandler;
    eventHandler.addEventListener("DOMContentLoaded", function onLoad() {
      eventHandler.removeEventListener("DOMContentLoaded", onLoad, false);
      callback(HostFrame._frame = frame);
    }, false);
  }
};
