/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const { classes: Cc, interfaces: Ci, utils: Cu } = Components;

this.EXPORTED_SYMBOLS = [ "switchToFloatingScrollbars", "switchToNativeScrollbars" ];

Cu.import("resource://gre/modules/Services.jsm");

let URL = Services.io.newURI("chrome://browser/skin/devtools/floating-scrollbars.css", null, null);

let trackedTabs = new WeakMap();

/**
 * Switch to floating scrollbars, à la mobile.
 *
 * @param aTab the targeted tab.
 *
 */
this.switchToFloatingScrollbars = function switchToFloatingScrollbars(aTab) {
  let mgr = trackedTabs.get(aTab);
  if (!mgr) {
    mgr = new ScrollbarManager(aTab);
  }
  mgr.switchToFloating();
}

/**
 * Switch to original native scrollbars.
 *
 * @param aTab the targeted tab.
 *
 */
this.switchToNativeScrollbars = function switchToNativeScrollbars(aTab) {
  let mgr = trackedTabs.get(aTab);
  if (mgr) {
    mgr.reset();
  }
}

function ScrollbarManager(aTab) {
  trackedTabs.set(aTab, this);

  this.attachedTab = aTab;
  this.attachedBrowser = aTab.linkedBrowser;

  this.reset = this.reset.bind(this);
  this.switchToFloating = this.switchToFloating.bind(this);

  this.attachedTab.addEventListener("TabClose", this.reset, true);
  this.attachedBrowser.addEventListener("DOMContentLoaded", this.switchToFloating, true);
}

ScrollbarManager.prototype = {
  get win() {
    return this.attachedBrowser.contentWindow;
  },

  /*
   * Change the look of the scrollbars.
   */
  switchToFloating: function() {
    let windows = this.getInnerWindows(this.win);
    windows.forEach(this.injectStyleSheet);
    this.forceStyle();
  },


  /*
   * Reset the look of the scrollbars.
   */
  reset: function() {
    let windows = this.getInnerWindows(this.win);
    windows.forEach(this.removeStyleSheet);
    this.forceStyle(this.attachedBrowser);
    this.attachedBrowser.removeEventListener("DOMContentLoaded", this.switchToFloating, true);
    this.attachedTab.removeEventListener("TabClose", this.reset, true);
    trackedTabs.delete(this.attachedTab);
  },

  /*
   * Toggle the display property of the window to force the style to be applied.
   */
  forceStyle: function() {
    let parentWindow = this.attachedBrowser.ownerDocument.defaultView;
    let display = parentWindow.getComputedStyle(this.attachedBrowser).display; // Save display value
    this.attachedBrowser.style.display = "none";
    parentWindow.getComputedStyle(this.attachedBrowser).display; // Flush
    this.attachedBrowser.style.display = display; // Restore
  },

  /*
   * return all the window objects present in the hiearchy of a window.
   */
  getInnerWindows: function(win) {
    let iframes = win.document.querySelectorAll("iframe");
    let innerWindows = [];
    for (let iframe of iframes) {
      innerWindows = innerWindows.concat(this.getInnerWindows(iframe.contentWindow));
    }
    return [win].concat(innerWindows);
  },

  /*
   * Append the new scrollbar style.
   */
  injectStyleSheet: function(win) {
    let winUtils = win.QueryInterface(Ci.nsIInterfaceRequestor).getInterface(Ci.nsIDOMWindowUtils);
    try {
      winUtils.loadSheet(URL, win.AGENT_SHEET);
    }catch(e) {}
  },

  /*
   * Remove the injected stylesheet.
   */
  removeStyleSheet: function(win) {
    let winUtils = win.QueryInterface(Ci.nsIInterfaceRequestor).getInterface(Ci.nsIDOMWindowUtils);
    try {
      winUtils.removeSheet(URL, win.AGENT_SHEET);
    }catch(e) {}
  },
}
