/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set ft=javascript ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const EventEmitter = require("devtools/shared/event-emitter");
const promise = require("promise");
const defer = require("devtools/shared/defer");
const Services = require("Services");
const {DOMHelpers} = require("resource://devtools/client/shared/DOMHelpers.jsm");

loader.lazyRequireGetter(this, "AppConstants", "resource://gre/modules/AppConstants.jsm", true);
loader.lazyRequireGetter(this, "gDevToolsBrowser", "devtools/client/framework/devtools-browser", true);

/* A host should always allow this much space for the page to be displayed.
 * There is also a min-height on the browser, but we still don't want to set
 * frame.height to be larger than that, since it can cause problems with
 * resizing the toolbox and panel layout. */
const MIN_PAGE_SIZE = 25;

/**
 * A toolbox host represents an object that contains a toolbox (e.g. the
 * sidebar or a separate window). Any host object should implement the
 * following functions:
 *
 * create() - create the UI and emit a 'ready' event when the UI is ready to use
 * destroy() - destroy the host's UI
 */

/**
 * Host object for the dock on the bottom of the browser
 */
function BottomHost(hostTab) {
  this.hostTab = hostTab;

  EventEmitter.decorate(this);
}

BottomHost.prototype = {
  type: "bottom",

  heightPref: "devtools.toolbox.footer.height",

  /**
   * Create a box at the bottom of the host tab.
   */
  create: async function() {
    await gDevToolsBrowser.loadBrowserStyleSheet(this.hostTab.ownerGlobal);

    const gBrowser = this.hostTab.ownerDocument.defaultView.gBrowser;
    const ownerDocument = gBrowser.ownerDocument;
    this._nbox = gBrowser.getNotificationBox(this.hostTab.linkedBrowser);

    this._splitter = ownerDocument.createElement("splitter");
    this._splitter.setAttribute("class", "devtools-horizontal-splitter");
    // Avoid resizing notification containers
    this._splitter.setAttribute("resizebefore", "flex");

    this.frame = ownerDocument.createElement("iframe");
    this.frame.flex = 1; // Required to be able to shrink when the window shrinks
    this.frame.className = "devtools-toolbox-bottom-iframe";
    this.frame.height = Math.min(
      Services.prefs.getIntPref(this.heightPref),
      this._nbox.clientHeight - MIN_PAGE_SIZE
    );

    this._nbox.appendChild(this._splitter);
    this._nbox.appendChild(this.frame);

    this.frame.tooltip = "aHTMLTooltip";

    // we have to load something so we can switch documents if we have to
    this.frame.setAttribute("src", "about:blank");

    const frame = await new Promise(resolve => {
      const domHelper = new DOMHelpers(this.frame.contentWindow);
      const frameLoad = () => {
        this.emit("ready", this.frame);
        resolve(this.frame);
      };
      domHelper.onceDOMReady(frameLoad);
      focusTab(this.hostTab);
    });

    return frame;
  },

  /**
   * Raise the host.
   */
  raise: function() {
    focusTab(this.hostTab);
  },

  /**
   * Set the toolbox title.
   * Nothing to do for this host type.
   */
  setTitle: function() {},

  /**
   * Destroy the bottom dock.
   */
  destroy: function() {
    if (!this._destroyed) {
      this._destroyed = true;

      Services.prefs.setIntPref(this.heightPref, this.frame.height);
      this._nbox.removeChild(this._splitter);
      this._nbox.removeChild(this.frame);
      this.frame = null;
      this._nbox = null;
      this._splitter = null;
    }

    return promise.resolve(null);
  }
};

/**
 * Base Host object for the in-browser sidebar
 */
class SidebarHost {
  constructor(hostTab, type) {
    this.hostTab = hostTab;
    this.type = type;
    this.widthPref = "devtools.toolbox.sidebar.width";

    EventEmitter.decorate(this);
  }

  /**
   * Create a box in the sidebar of the host tab.
   */
  async create() {
    await gDevToolsBrowser.loadBrowserStyleSheet(this.hostTab.ownerGlobal);
    const gBrowser = this.hostTab.ownerDocument.defaultView.gBrowser;
    const ownerDocument = gBrowser.ownerDocument;
    this._browser = gBrowser.getBrowserContainer(this.hostTab.linkedBrowser);
    this._sidebar = gBrowser.getSidebarContainer(this.hostTab.linkedBrowser);

    this._splitter = ownerDocument.createElement("splitter");
    this._splitter.setAttribute("class", "devtools-side-splitter");

    this.frame = ownerDocument.createElement("iframe");
    this.frame.flex = 1; // Required to be able to shrink when the window shrinks
    this.frame.className = "devtools-toolbox-side-iframe";

    this.frame.width = Math.min(
      Services.prefs.getIntPref(this.widthPref),
      this._sidebar.clientWidth - MIN_PAGE_SIZE
    );

    if (this.type == "right") {
      this._sidebar.appendChild(this._splitter);
      this._sidebar.appendChild(this.frame);
    } else {
      this._sidebar.insertBefore(this.frame, this._browser);
      this._sidebar.insertBefore(this._splitter, this._browser);
    }

    this.frame.tooltip = "aHTMLTooltip";
    this.frame.setAttribute("src", "about:blank");

    const frame = await new Promise(resolve => {
      const domHelper = new DOMHelpers(this.frame.contentWindow);
      const frameLoad = () => {
        this.emit("ready", this.frame);
        resolve(this.frame);
      };
      domHelper.onceDOMReady(frameLoad);
      focusTab(this.hostTab);
    });

    return frame;
  }

  /**
   * Raise the host.
   */
  raise() {
    focusTab(this.hostTab);
  }

  /**
   * Set the toolbox title.
   * Nothing to do for this host type.
   */
  setTitle() {}

  /**
   * Destroy the sidebar.
   */
  destroy() {
    if (!this._destroyed) {
      this._destroyed = true;

      Services.prefs.setIntPref(this.widthPref, this.frame.width);
      this._sidebar.removeChild(this._splitter);
      this._sidebar.removeChild(this.frame);
    }

    return promise.resolve(null);
  }
}

/**
 * Host object for the in-browser left sidebar
 */
class LeftHost extends SidebarHost {
  constructor(hostTab) {
    super(hostTab, "left");
  }
}

/**
 * Host object for the in-browser right sidebar
 */
class RightHost extends SidebarHost {
  constructor(hostTab) {
    super(hostTab, "right");
  }
}

/**
 * Host object for the toolbox in a separate window
 */
function WindowHost() {
  this._boundUnload = this._boundUnload.bind(this);

  EventEmitter.decorate(this);
}

WindowHost.prototype = {
  type: "window",

  WINDOW_URL: "chrome://devtools/content/framework/toolbox-window.xul",

  /**
   * Create a new xul window to contain the toolbox.
   */
  create: function() {
    const deferred = defer();

    const flags = "chrome,centerscreen,resizable,dialog=no";
    const win = Services.ww.openWindow(null, this.WINDOW_URL, "_blank",
                                     flags, null);

    const frameLoad = () => {
      win.removeEventListener("load", frameLoad, true);
      win.focus();

      let key;
      if (AppConstants.platform === "macosx") {
        key = win.document.getElementById("toolbox-key-toggle-osx");
      } else {
        key = win.document.getElementById("toolbox-key-toggle");
      }
      key.removeAttribute("disabled");

      this.frame = win.document.getElementById("toolbox-iframe");
      this.emit("ready", this.frame);

      deferred.resolve(this.frame);
    };

    win.addEventListener("load", frameLoad, true);
    win.addEventListener("unload", this._boundUnload);

    this._window = win;

    return deferred.promise;
  },

  /**
   * Catch the user closing the window.
   */
  _boundUnload: function(event) {
    if (event.target.location != this.WINDOW_URL) {
      return;
    }
    this._window.removeEventListener("unload", this._boundUnload);

    this.emit("window-closed");
  },

  /**
   * Raise the host.
   */
  raise: function() {
    this._window.focus();
  },

  /**
   * Set the toolbox title.
   */
  setTitle: function(title) {
    this._window.document.title = title;
  },

  /**
   * Destroy the window.
   */
  destroy: function() {
    if (!this._destroyed) {
      this._destroyed = true;

      this._window.removeEventListener("unload", this._boundUnload);
      this._window.close();
    }

    return promise.resolve(null);
  }
};

/**
 * Host object for the toolbox in its own tab
 */
function CustomHost(hostTab, options) {
  this.frame = options.customIframe;
  this.uid = options.uid;
  EventEmitter.decorate(this);
}

CustomHost.prototype = {
  type: "custom",

  _sendMessageToTopWindow: function(msg, data) {
    // It's up to the custom frame owner (parent window) to honor
    // "close" or "raise" instructions.
    const topWindow = this.frame.ownerDocument.defaultView;
    if (!topWindow) {
      return;
    }
    const message = {
      name: "toolbox-" + msg,
      uid: this.uid,
      data,
    };
    topWindow.postMessage(message, "*");
  },

  /**
   * Create a new xul window to contain the toolbox.
   */
  create: function() {
    return promise.resolve(this.frame);
  },

  /**
   * Raise the host.
   */
  raise: function() {
    this._sendMessageToTopWindow("raise");
  },

  /**
   * Set the toolbox title.
   */
  setTitle: function(title) {
    this._sendMessageToTopWindow("title", { value: title });
  },

  /**
   * Destroy the window.
   */
  destroy: function() {
    if (!this._destroyed) {
      this._destroyed = true;
      this._sendMessageToTopWindow("close");
    }
    return promise.resolve(null);
  }
};

/**
 *  Switch to the given tab in a browser and focus the browser window
 */
function focusTab(tab) {
  const browserWindow = tab.ownerDocument.defaultView;
  browserWindow.focus();
  browserWindow.gBrowser.selectedTab = tab;
}

exports.Hosts = {
  "bottom": BottomHost,
  "left": LeftHost,
  "right": RightHost,
  "window": WindowHost,
  "custom": CustomHost
};

