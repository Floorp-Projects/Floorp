/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/*
 * This module adjusts network priority for tabs in a way that gives 'important'
 * tabs a higher priority. There are 3 levels of priority. Each is listed below
 * with the priority adjustment used.
 *
 * Highest (-10): Selected tab in the focused window.
 * Medium (0):    Background tabs in the focused window.
 *                Selected tab in background windows.
 * Lowest (+10):  Background tabs in background windows.
 */

this.EXPORTED_SYMBOLS = ["trackBrowserWindow"];

const Ci = Components.interfaces;

Components.utils.import("resource://gre/modules/XPCOMUtils.jsm");


// Lazy getters
XPCOMUtils.defineLazyServiceGetter(this, "_focusManager",
                                   "@mozilla.org/focus-manager;1",
                                   "nsIFocusManager");


// Constants
const TAB_EVENTS = ["TabOpen", "TabSelect"];
const WINDOW_EVENTS = ["activate", "unload"];
// PRIORITY DELTA is -10 because lower priority value is actually a higher priority
const PRIORITY_DELTA = -10;


// Variables
let _lastFocusedWindow = null;
let _windows = [];


// Exported symbol
this.trackBrowserWindow = function trackBrowserWindow(aWindow) {
  WindowHelper.addWindow(aWindow);
}


// Global methods
function _handleEvent(aEvent) {
  switch (aEvent.type) {
    case "TabOpen":
      BrowserHelper.onOpen(aEvent.target.linkedBrowser);
      break;
    case "TabSelect":
      BrowserHelper.onSelect(aEvent.target.linkedBrowser);
      break;
    case "activate":
      WindowHelper.onActivate(aEvent.target);
      break;
    case "unload":
      WindowHelper.removeWindow(aEvent.currentTarget);
      break;
  }
}


// Methods that impact a browser. Put into single object for organization.
let BrowserHelper = {
  onOpen: function NP_BH_onOpen(aBrowser) {
    // If the tab is in the focused window, leave priority as it is
    if (aBrowser.ownerDocument.defaultView != _lastFocusedWindow)
      this.decreasePriority(aBrowser);
  },

  onSelect: function NP_BH_onSelect(aBrowser) {
    let windowEntry = WindowHelper.getEntry(aBrowser.ownerDocument.defaultView);
    if (windowEntry.lastSelectedBrowser)
      this.decreasePriority(windowEntry.lastSelectedBrowser);
    this.increasePriority(aBrowser);

    windowEntry.lastSelectedBrowser = aBrowser;
  },

  increasePriority: function NP_BH_increasePriority(aBrowser) {
    aBrowser.adjustPriority(PRIORITY_DELTA);
  },

  decreasePriority: function NP_BH_decreasePriority(aBrowser) {
    aBrowser.adjustPriority(PRIORITY_DELTA * -1);
  }
};


// Methods that impact a window. Put into single object for organization.
let WindowHelper = {
  addWindow: function NP_WH_addWindow(aWindow) {
    // Build internal data object
    _windows.push({ window: aWindow, lastSelectedBrowser: null });

    // Add event listeners
    TAB_EVENTS.forEach(function(event) {
      aWindow.gBrowser.tabContainer.addEventListener(event, _handleEvent, false);
    });
    WINDOW_EVENTS.forEach(function(event) {
      aWindow.addEventListener(event, _handleEvent, false);
    });

    // This gets called AFTER activate event, so if this is the focused window
    // we want to activate it. Otherwise, deprioritize it.
    if (aWindow == _focusManager.activeWindow)
      this.handleFocusedWindow(aWindow);
    else
      this.decreasePriority(aWindow);

    // Select the selected tab
    BrowserHelper.onSelect(aWindow.gBrowser.selectedBrowser);
  },

  removeWindow: function NP_WH_removeWindow(aWindow) {
    if (aWindow == _lastFocusedWindow)
      _lastFocusedWindow = null;

    // Delete this window from our tracking
    _windows.splice(this.getEntryIndex(aWindow), 1);

    // Remove the event listeners
    TAB_EVENTS.forEach(function(event) {
      aWindow.gBrowser.tabContainer.removeEventListener(event, _handleEvent, false);
    });
    WINDOW_EVENTS.forEach(function(event) {
      aWindow.removeEventListener(event, _handleEvent, false);
    });
  },

  onActivate: function NP_WH_onActivate(aWindow, aHasFocus) {
    // If this window was the last focused window, we don't need to do anything
    if (aWindow == _lastFocusedWindow)
      return;

    // handleFocusedWindow will deprioritize the current window
    this.handleFocusedWindow(aWindow);

    // Lastly we should increase priority for this window
    this.increasePriority(aWindow);
  },

  handleFocusedWindow: function NP_WH_handleFocusedWindow(aWindow) {
    // If we have a last focused window, we need to deprioritize it first
    if (_lastFocusedWindow)
      this.decreasePriority(_lastFocusedWindow);

    // aWindow is now focused
    _lastFocusedWindow = aWindow;
  },

  // Auxiliary methods
  increasePriority: function NP_WH_increasePriority(aWindow) {
    aWindow.gBrowser.browsers.forEach(function(aBrowser) {
      BrowserHelper.increasePriority(aBrowser);
    });
  },

  decreasePriority: function NP_WH_decreasePriority(aWindow) {
    aWindow.gBrowser.browsers.forEach(function(aBrowser) {
      BrowserHelper.decreasePriority(aBrowser);
    });
  },

  getEntry: function NP_WH_getEntry(aWindow) {
    return _windows[this.getEntryIndex(aWindow)];
  },

  getEntryIndex: function NP_WH_getEntryAtIndex(aWindow) {
    // Assumes that every object has a unique window & it's in the array
    for (let i = 0; i < _windows.length; i++)
      if (_windows[i].window == aWindow)
        return i;
  }
};

