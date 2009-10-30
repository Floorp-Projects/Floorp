/* ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is Mozilla Corporation.
 * Portions created by the Initial Developer are Copyright (C) 2009
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Paul O’Shannessy <paul@oshannessy.com> (original author)
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

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

let EXPORTED_SYMBOLS = ["trackBrowserWindow"];

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
function trackBrowserWindow(aWindow) {
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
  onOpen: function(aBrowser) {
    // If the tab is in the focused window, leave priority as it is
    if (aBrowser.ownerDocument.defaultView != _lastFocusedWindow)
      this.decreasePriority(aBrowser);
  },

  onSelect: function(aBrowser) {
    let windowEntry = WindowHelper.getEntry(aBrowser.ownerDocument.defaultView);
    if (windowEntry.lastSelectedBrowser)
      this.decreasePriority(windowEntry.lastSelectedBrowser);
    this.increasePriority(aBrowser);

    windowEntry.lastSelectedBrowser = aBrowser;
  },

  // Auxiliary methods
  getLoadgroup: function(aBrowser) {
    return aBrowser.webNavigation.QueryInterface(Ci.nsIDocumentLoader)
                   .loadGroup.QueryInterface(Ci.nsISupportsPriority);
  },

  increasePriority: function(aBrowser) {
    this.getLoadgroup(aBrowser).adjustPriority(PRIORITY_DELTA);
  },

  decreasePriority: function(aBrowser) {
    this.getLoadgroup(aBrowser).adjustPriority(PRIORITY_DELTA * -1);
  }
};


// Methods that impact a window. Put into single object for organization.
let WindowHelper = {
  addWindow: function(aWindow) {
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

  removeWindow: function(aWindow) {
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

  onActivate: function(aWindow, aHasFocus) {
    // If this window was the last focused window, we don't need to do anything
    if (aWindow == _lastFocusedWindow)
      return;

    // handleFocusedWindow will deprioritize the current window
    this.handleFocusedWindow(aWindow);

    // Lastly we should increase priority for this window
    this.increasePriority(aWindow);
  },

  handleFocusedWindow: function(aWindow) {
    // If we have a last focused window, we need to deprioritize it first
    if (_lastFocusedWindow)
      this.decreasePriority(_lastFocusedWindow);

    // aWindow is now focused
    _lastFocusedWindow = aWindow;
  },

  // Auxiliary methods
  increasePriority: function(aWindow) {
    aWindow.gBrowser.browsers.forEach(function(aBrowser) {
      BrowserHelper.increasePriority(aBrowser);
    });
  },

  decreasePriority: function(aWindow) {
    aWindow.gBrowser.browsers.forEach(function(aBrowser) {
      BrowserHelper.decreasePriority(aBrowser);
    });
  },

  getEntry: function(aWindow) {
    return _windows[this.getEntryIndex(aWindow)];
  },

  getEntryIndex: function(aWindow) {
    // Assumes that every object has a unique window & it's in the array
    for (let i = 0; i < _windows.length; i++)
      if (_windows[i].window == aWindow)
        return i;
  }
};

