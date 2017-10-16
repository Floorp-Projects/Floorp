/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/*
 * This module tracks each browser window and informs network module
 * the current selected tab's content outer window ID.
 */

this.EXPORTED_SYMBOLS = ["trackBrowserWindow"];

const Ci = Components.interfaces;

Components.utils.import("resource://gre/modules/XPCOMUtils.jsm");
Components.utils.import("resource://gre/modules/Services.jsm");

// Lazy getters
XPCOMUtils.defineLazyServiceGetter(this, "_focusManager",
                                   "@mozilla.org/focus-manager;1",
                                   "nsIFocusManager");

// Constants
const TAB_EVENTS = ["TabBrowserInserted", "TabSelect"];
const WINDOW_EVENTS = ["activate", "unload"];
const DEBUG = false;

// Variables
var _lastFocusedWindow = null;
var _lastTopLevelWindowID = 0;

// Exported symbol
this.trackBrowserWindow = function trackBrowserWindow(aWindow) {
  WindowHelper.addWindow(aWindow);
};

// Global methods
function debug(s) {
  if (DEBUG) {
    dump("-*- UpdateTopLevelContentWindowIDHelper: " + s + "\n");
  }
}

function _updateCurrentContentOuterWindowID(aBrowser) {
  if (!aBrowser.outerWindowID ||
      aBrowser.outerWindowID === _lastTopLevelWindowID) {
    return;
  }

  debug("Current window uri=" + aBrowser.currentURI.spec +
        " id=" + aBrowser.outerWindowID);

  _lastTopLevelWindowID = aBrowser.outerWindowID;
  let windowIDWrapper = Components.classes["@mozilla.org/supports-PRUint64;1"]
                          .createInstance(Ci.nsISupportsPRUint64);
  windowIDWrapper.data = _lastTopLevelWindowID;
  Services.obs.notifyObservers(windowIDWrapper,
                               "net:current-toplevel-outer-content-windowid");
}

function _handleEvent(aEvent) {
  switch (aEvent.type) {
    case "TabBrowserInserted":
      if (aEvent.target.ownerGlobal.gBrowser.selectedBrowser === aEvent.target.linkedBrowser) {
        _updateCurrentContentOuterWindowID(aEvent.target.linkedBrowser);
      }
      break;
    case "TabSelect":
      _updateCurrentContentOuterWindowID(aEvent.target.linkedBrowser);
      break;
    case "activate":
      WindowHelper.onActivate(aEvent.target);
      break;
    case "unload":
      WindowHelper.removeWindow(aEvent.currentTarget);
      break;
  }
}

function _handleMessage(aMessage) {
  let browser = aMessage.target;
  if (aMessage.name === "Browser:Init" &&
      browser === browser.ownerGlobal.gBrowser.selectedBrowser) {
    _updateCurrentContentOuterWindowID(browser);
  }
}

// Methods that impact a window. Put into single object for organization.
var WindowHelper = {
  addWindow: function NP_WH_addWindow(aWindow) {
    // Add event listeners
    TAB_EVENTS.forEach(function(event) {
      aWindow.gBrowser.tabContainer.addEventListener(event, _handleEvent);
    });
    WINDOW_EVENTS.forEach(function(event) {
      aWindow.addEventListener(event, _handleEvent);
    });

    let messageManager = aWindow.getGroupMessageManager("browsers");
    messageManager.addMessageListener("Browser:Init", _handleMessage);

    // This gets called AFTER activate event, so if this is the focused window
    // we want to activate it.
    if (aWindow == _focusManager.activeWindow)
      this.handleFocusedWindow(aWindow);

    // Update the selected tab's content outer window ID.
    _updateCurrentContentOuterWindowID(aWindow.gBrowser.selectedBrowser);
  },

  removeWindow: function NP_WH_removeWindow(aWindow) {
    if (aWindow == _lastFocusedWindow)
      _lastFocusedWindow = null;

    // Remove the event listeners
    TAB_EVENTS.forEach(function(event) {
      aWindow.gBrowser.tabContainer.removeEventListener(event, _handleEvent);
    });
    WINDOW_EVENTS.forEach(function(event) {
      aWindow.removeEventListener(event, _handleEvent);
    });

    let messageManager = aWindow.getGroupMessageManager("browsers");
    messageManager.removeMessageListener("Browser:Init", _handleMessage);
  },

  onActivate: function NP_WH_onActivate(aWindow, aHasFocus) {
    // If this window was the last focused window, we don't need to do anything
    if (aWindow == _lastFocusedWindow)
      return;

    this.handleFocusedWindow(aWindow);

    _updateCurrentContentOuterWindowID(aWindow.gBrowser.selectedBrowser);
  },

  handleFocusedWindow: function NP_WH_handleFocusedWindow(aWindow) {
    // aWindow is now focused
    _lastFocusedWindow = aWindow;
  },
};
