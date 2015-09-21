/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
/* globals DOMHelpers, Services */

"use strict";

const {Cu} = require("chrome");
const EventEmitter = require("devtools/toolkit/event-emitter");
const promise = require("promise");
Cu.import("resource://gre/modules/Services.jsm");
Cu.import("resource:///modules/devtools/DOMHelpers.jsm");

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

exports.Hosts = {
  "bottom": BottomHost,
  "side": SidebarHost,
  "window": WindowHost,
  "custom": CustomHost
};

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
  create: function() {
    let deferred = promise.defer();

    let gBrowser = this.hostTab.ownerDocument.defaultView.gBrowser;
    let ownerDocument = gBrowser.ownerDocument;
    this._nbox = gBrowser.getNotificationBox(this.hostTab.linkedBrowser);

    this._splitter = ownerDocument.createElement("splitter");
    this._splitter.setAttribute("class", "devtools-horizontal-splitter");

    this.frame = ownerDocument.createElement("iframe");
    this.frame.className = "devtools-toolbox-bottom-iframe";
    this.frame.height = Math.min(
      Services.prefs.getIntPref(this.heightPref),
      this._nbox.clientHeight - MIN_PAGE_SIZE
    );

    this._nbox.appendChild(this._splitter);
    this._nbox.appendChild(this.frame);

    let frameLoad = () => {
      this.emit("ready", this.frame);
      deferred.resolve(this.frame);
    };

    this.frame.tooltip = "aHTMLTooltip";

    // we have to load something so we can switch documents if we have to
    this.frame.setAttribute("src", "about:blank");

    let domHelper = new DOMHelpers(this.frame.contentWindow);
    domHelper.onceDOMReady(frameLoad);

    focusTab(this.hostTab);

    return deferred.promise;
  },

  /**
   * Raise the host.
   */
  raise: function() {
    focusTab(this.hostTab);
  },

  /**
   * Minimize this host so that only the toolbox tabbar remains visible.
   * @param {Number} height The height to minimize to. Defaults to 0, which
   * means that the toolbox won't be visible at all once minimized.
   */
  minimize: function(height=0) {
    if (this.isMinimized) {
      return;
    }
    this.isMinimized = true;

    let onTransitionEnd = event => {
      if (event.propertyName !== "margin-bottom") {
        // Ignore transitionend on unrelated properties.
        return;
      }

      this.frame.removeEventListener("transitionend", onTransitionEnd);
      this.emit("minimized");
    };
    this.frame.addEventListener("transitionend", onTransitionEnd);
    this.frame.style.marginBottom = -this.frame.height + height + "px";
    this._splitter.classList.add("disabled");
  },

  /**
   * If the host was minimized before, maximize it again (the host will be
   * maximized to the height it previously had).
   */
  maximize: function() {
    if (!this.isMinimized) {
      return;
    }
    this.isMinimized = false;

    let onTransitionEnd = event => {
      if (event.propertyName !== "margin-bottom") {
        // Ignore transitionend on unrelated properties.
        return;
      }

      this.frame.removeEventListener("transitionend", onTransitionEnd);
      this.emit("maximized");
    };
    this.frame.addEventListener("transitionend", onTransitionEnd);
    this.frame.style.marginBottom = "0";
    this._splitter.classList.remove("disabled");
  },

  /**
   * Toggle the minimize mode.
   * @param {Number} minHeight The height to minimize to.
   */
  toggleMinimizeMode: function(minHeight) {
    this.isMinimized ? this.maximize() : this.minimize(minHeight);
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
    }

    return promise.resolve(null);
  }
};

/**
 * Host object for the in-browser sidebar
 */
function SidebarHost(hostTab) {
  this.hostTab = hostTab;

  EventEmitter.decorate(this);
}

SidebarHost.prototype = {
  type: "side",

  widthPref: "devtools.toolbox.sidebar.width",

  /**
   * Create a box in the sidebar of the host tab.
   */
  create: function() {
    let deferred = promise.defer();

    let gBrowser = this.hostTab.ownerDocument.defaultView.gBrowser;
    let ownerDocument = gBrowser.ownerDocument;
    this._sidebar = gBrowser.getSidebarContainer(this.hostTab.linkedBrowser);

    this._splitter = ownerDocument.createElement("splitter");
    this._splitter.setAttribute("class", "devtools-side-splitter");

    this.frame = ownerDocument.createElement("iframe");
    this.frame.className = "devtools-toolbox-side-iframe";

    this.frame.width = Math.min(
      Services.prefs.getIntPref(this.widthPref),
      this._sidebar.clientWidth - MIN_PAGE_SIZE
    );

    this._sidebar.appendChild(this._splitter);
    this._sidebar.appendChild(this.frame);

    let frameLoad = () => {
      this.emit("ready", this.frame);
      deferred.resolve(this.frame);
    };

    this.frame.tooltip = "aHTMLTooltip";
    this.frame.setAttribute("src", "about:blank");

    let domHelper = new DOMHelpers(this.frame.contentWindow);
    domHelper.onceDOMReady(frameLoad);

    focusTab(this.hostTab);

    return deferred.promise;
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
   * Destroy the sidebar.
   */
  destroy: function() {
    if (!this._destroyed) {
      this._destroyed = true;

      Services.prefs.setIntPref(this.widthPref, this.frame.width);
      this._sidebar.removeChild(this._splitter);
      this._sidebar.removeChild(this.frame);
    }

    return promise.resolve(null);
  }
};

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
    let deferred = promise.defer();

    let flags = "chrome,centerscreen,resizable,dialog=no";
    let win = Services.ww.openWindow(null, this.WINDOW_URL, "_blank",
                                     flags, null);

    let frameLoad = () => {
      win.removeEventListener("load", frameLoad, true);
      win.focus();
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
    let topWindow = this.frame.ownerDocument.defaultView;
    if (!topWindow) {
      return;
    }
    let json = {name: "toolbox-" + msg, uid: this.uid};
    if (data) {
      json.data = data;
    }
    topWindow.postMessage(JSON.stringify(json), "*");
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
  let browserWindow = tab.ownerDocument.defaultView;
  browserWindow.focus();
  browserWindow.gBrowser.selectedTab = tab;
}
