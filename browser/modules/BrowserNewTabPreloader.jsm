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
const TOPIC_DELAYED_STARTUP = "browser-delayed-startup-finished";
const PRELOADER_INIT_DELAY_MS = 5000;

this.BrowserNewTabPreloader = {
  init: function Preloader_init() {
    Initializer.start();
  },

  uninit: function Preloader_uninit() {
    Initializer.stop();
    HostFrame.destroy();
    Preferences.uninit();
    HiddenBrowser.destroy();
  },

  newTab: function Preloader_newTab(aTab) {
    HiddenBrowser.swapWithNewTab(aTab);
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
    if (this._timer) {
      this._timer.cancel();
      this._timer = null;
    }

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
    } else if (aTopic == "timer-callback") {
      this._timer = null;
      this._startPreloader();
    }
  },

  _startTimer: function Initializer_startTimer() {
    this._timer = Cc["@mozilla.org/timer;1"].createInstance(Ci.nsITimer);
    this._timer.init(this, PRELOADER_INIT_DELAY_MS, Ci.nsITimer.TYPE_ONE_SHOT);
  },

  _startPreloader: function Initializer_startPreloader() {
    Preferences.init();
    if (Preferences.enabled) {
      HiddenBrowser.create();
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
    let {url, enabled} = this;
    this._url = this._enabled = null;

    if (enabled && !this.enabled) {
      HiddenBrowser.destroy();
    } else if (!enabled && this.enabled) {
      HiddenBrowser.create();
    } else if (this._browser && url != this.url) {
      HiddenBrowser.update(this.url);
    }
  },
};

let HiddenBrowser = {
  get isPreloaded() {
    return this._browser &&
           this._browser.contentDocument &&
           this._browser.contentDocument.readyState == "complete" &&
           this._browser.currentURI.spec == Preferences.url;
  },

  swapWithNewTab: function HiddenBrowser_swapWithNewTab(aTab) {
    if (this.isPreloaded) {
      let tabbrowser = aTab.ownerDocument.defaultView.gBrowser;
      if (tabbrowser) {
        tabbrowser.swapNewTabWithBrowser(aTab, this._browser);
      }
    }
  },

  create: function HiddenBrowser_create() {
    HostFrame.get(aFrame => {
      let doc = aFrame.document;
      this._browser = doc.createElementNS(XUL_NS, "browser");
      this._browser.setAttribute("type", "content");
      this._browser.setAttribute("src", Preferences.url);
      doc.getElementById("win").appendChild(this._browser);
    });
  },

  update: function HiddenBrowser_update(aURL) {
    this._browser.setAttribute("src", aURL);
  },

  destroy: function HiddenBrowser_destroy() {
    if (this._browser) {
      this._browser.parentNode.removeChild(this._browser);
      this._browser = null;
    }
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
