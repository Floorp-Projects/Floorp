/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const Cu = Components.utils;

Cu.import("resource://gre/modules/Services.jsm");
Cu.import("resource://gre/modules/commonjs/promise/core.js");
Cu.import("resource:///modules/devtools/EventEmitter.jsm");

this.EXPORTED_SYMBOLS = [ "Hosts" ];

/**
 * A toolbox host represents an object that contains a toolbox (e.g. the
 * sidebar or a separate window). Any host object should implement the
 * following functions:
 *
 * open() - create the UI and emit a 'ready' event when the UI is ready to use
 * destroy() - destroy the host's UI
 */

this.Hosts = {
  "bottom": BottomHost,
  "side": SidebarHost,
  "window": WindowHost
}

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
  open: function BH_open() {
    let deferred = Promise.defer();

    let gBrowser = this.hostTab.ownerDocument.defaultView.gBrowser;
    let ownerDocument = gBrowser.ownerDocument;

    this._splitter = ownerDocument.createElement("splitter");
    this._splitter.setAttribute("class", "devtools-horizontal-splitter");

    this.frame = ownerDocument.createElement("iframe");
    this.frame.id = "devtools-toolbox-bottom-iframe";
    this.frame.height = Services.prefs.getIntPref(this.heightPref);

    this._nbox = gBrowser.getNotificationBox(this.hostTab.linkedBrowser);
    this._nbox.appendChild(this._splitter);
    this._nbox.appendChild(this.frame);

    let frameLoad = function() {
      this.frame.removeEventListener("DOMContentLoaded", frameLoad, true);
      this.emit("ready", this.frame);

      deferred.resolve(this.frame);
    }.bind(this);

    this.frame.addEventListener("DOMContentLoaded", frameLoad, true);

    // we have to load something so we can switch documents if we have to
    this.frame.setAttribute("src", "about:blank");

    focusTab(this.hostTab);

    return deferred.promise;
  },

  /**
   * Destroy the bottom dock.
   */
  destroy: function BH_destroy() {
    if (!this._destroyed) {
      this._destroyed = true;

      Services.prefs.setIntPref(this.heightPref, this.frame.height);
      this._nbox.removeChild(this._splitter);
      this._nbox.removeChild(this.frame);
    }

    return Promise.resolve(null);
  }
}


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
  open: function RH_open() {
    let deferred = Promise.defer();

    let gBrowser = this.hostTab.ownerDocument.defaultView.gBrowser;
    let ownerDocument = gBrowser.ownerDocument;

    this._splitter = ownerDocument.createElement("splitter");
    this._splitter.setAttribute("class", "devtools-side-splitter");

    this.frame = ownerDocument.createElement("iframe");
    this.frame.id = "devtools-toolbox-side-iframe";
    this.frame.width = Services.prefs.getIntPref(this.widthPref);

    this._sidebar = gBrowser.getSidebarContainer(this.hostTab.linkedBrowser);
    this._sidebar.appendChild(this._splitter);
    this._sidebar.appendChild(this.frame);

    let frameLoad = function() {
      this.frame.removeEventListener("DOMContentLoaded", frameLoad, true);
      this.emit("ready", this.frame);

      deferred.resolve(this.frame);
    }.bind(this);

    this.frame.addEventListener("DOMContentLoaded", frameLoad, true);
    this.frame.setAttribute("src", "about:blank");

    focusTab(this.hostTab);

    return deferred.promise;
  },

  /**
   * Destroy the sidebar.
   */
  destroy: function RH_destroy() {
    if (!this._destroyed) {
      this._destroyed = true;

      Services.prefs.setIntPref(this.widthPref, this.frame.width);
      this._sidebar.removeChild(this._splitter);
      this._sidebar.removeChild(this.frame);
    }

    return Promise.resolve(null);
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

  WINDOW_URL: "chrome://browser/content/devtools/framework/toolbox-window.xul",

  /**
   * Create a new xul window to contain the toolbox.
   */
  open: function WH_open() {
    let deferred = Promise.defer();

    let flags = "chrome,centerscreen,resizable,dialog=no";
    let win = Services.ww.openWindow(null, this.WINDOW_URL, "_blank",
                                     flags, null);

    let frameLoad = function(event) {
      win.removeEventListener("load", frameLoad, true);
      this.frame = win.document.getElementById("toolbox-iframe");
      this.emit("ready", this.frame);

      deferred.resolve(this.frame);
    }.bind(this);

    win.addEventListener("load", frameLoad, true);
    win.addEventListener("unload", this._boundUnload);

    win.focus();

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
   * Destroy the window.
   */
  destroy: function WH_destroy() {
    if (!this._destroyed) {
      this._destroyed = true;

      this._window.removeEventListener("unload", this._boundUnload);
      this._window.close();
    }

    return Promise.resolve(null);
  }
}

/**
 *  Switch to the given tab in a browser and focus the browser window
 */
function focusTab(tab) {
  let browserWindow = tab.ownerDocument.defaultView;
  browserWindow.focus();
  browserWindow.gBrowser.selectedTab = tab;
}
