/* -*- Mode: javascript; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ft=javascript ts=2 et sw=2 tw=80: */
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
 * The Original Code is Mozilla Debugger UI code.
 *
 * The Initial Developer of the Original Code is
 *   Mozilla Foundation
 * Portions created by the Initial Developer are Copyright (C) 2011
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Dave Camp <dcamp@mozilla.com> (original author)
 *   Panos Astithas <past@mozilla.com>
 *   Victor Porof <vporof@mozilla.com>
 *   Mihai Sucan <mihai.sucan@gmail.com>
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
"use strict";

const Cc = Components.classes;
const Ci = Components.interfaces;
const Cu = Components.utils;

Cu.import("resource://gre/modules/Services.jsm");

let EXPORTED_SYMBOLS = ["DebuggerUI"];

/**
 * Provides a simple mechanism of managing debugger instances per tab.
 *
 * @param nsIDOMWindow aWindow
 *        The chrome window for which the DebuggerUI instance is created.
 */
function DebuggerUI(aWindow) {
  this.chromeWindow = aWindow;
}

DebuggerUI.prototype = {
  /**
   * Called by the DebuggerPane to update the Debugger toggle switches with the
   * debugger state.
   */
  refreshCommand: function DUI_refreshCommand() {
    let selectedTab = this.chromeWindow.getBrowser().selectedTab;
    let command = this.chromeWindow.document.getElementById("Tools:Debugger");

    if (this.getDebugger(selectedTab) != null) {
      command.setAttribute("checked", "true");
    } else {
      command.removeAttribute("checked");
    }
  },

  /**
   * Starts a debugger for the current tab, or stops it if already started.
   * @return DebuggerPane if the debugger is started, null if it's stopped.
   */
  toggleDebugger: function DUI_toggleDebugger() {
    let tab = this.chromeWindow.gBrowser.selectedTab;

    if (tab._scriptDebugger) {
      tab._scriptDebugger.close();
      return null;
    }
    return new DebuggerPane(this, tab);
  },

  /**
   * Get the debugger for a specified tab.
   * @return DebuggerPane if a debugger exists for the tab, null otherwise
   */
  getDebugger: function DUI_getDebugger(aTab) {
    return '_scriptDebugger' in aTab ? aTab._scriptDebugger : null;
  },

  /**
   * Get the preferences associated with the debugger frontend.
   * @return object
   */
  get preferences() {
    return DebuggerUIPreferences;
  }
};

/**
 * Creates a pane that will host the debugger.
 *
 * @param XULElement aTab
 *        The tab in which to create the debugger.
 */
function DebuggerPane(aDebuggerUI, aTab) {
  this._globalUI = aDebuggerUI;
  this._tab = aTab;
  this._create();
}

DebuggerPane.prototype = {

  /**
   * Creates and initializes the widgets containing the debugger UI.
   */
  _create: function DP__create() {
    this._tab._scriptDebugger = this;

    let gBrowser = this._tab.linkedBrowser.getTabBrowser();
    let ownerDocument = gBrowser.parentNode.ownerDocument;

    this._splitter = ownerDocument.createElement("splitter");
    this._splitter.setAttribute("class", "hud-splitter");

    this._frame = ownerDocument.createElement("iframe");
    this._frame.height = DebuggerUIPreferences.height;

    this._nbox = gBrowser.getNotificationBox(this._tab.linkedBrowser);
    this._nbox.appendChild(this._splitter);
    this._nbox.appendChild(this._frame);

    this.close = this.close.bind(this);
    let self = this;

    this._frame.addEventListener("Debugger:Loaded", function dbgLoaded() {
      self._frame.removeEventListener("Debugger:Loaded", dbgLoaded, true);
      self._frame.addEventListener("Debugger:Close", self.close, true);
      self._frame.addEventListener("unload", self.close, true);

      // Bind shortcuts for accessing the breakpoint methods in the debugger.
      let bkp = self.debuggerWindow.DebuggerController.Breakpoints;
      self.addBreakpoint = bkp.addBreakpoint;
      self.removeBreakpoint = bkp.removeBreakpoint;
      self.getBreakpoint = bkp.getBreakpoint;
    }, true);

    this._frame.setAttribute("src", "chrome://browser/content/debugger.xul");

    this._globalUI.refreshCommand();
  },

  /**
   * Closes the debugger, removing child nodes and event listeners.
   */
  close: function DP_close() {
    if (!this._tab) {
      return;
    }
    this._tab._scriptDebugger = null;
    this._tab = null;

    DebuggerUIPreferences.height = this._frame.height;
    this._frame.removeEventListener("Debugger:Close", this.close, true);
    this._frame.removeEventListener("unload", this.close, true);

    this._nbox.removeChild(this._splitter);
    this._nbox.removeChild(this._frame);

    this._splitter = null;
    this._frame = null;
    this._nbox = null;

    this._globalUI.refreshCommand();
  },

  /**
   * Gets the debugger content window.
   * @return nsIDOMWindow if a debugger window exists, null otherwise
   */
  get debuggerWindow() {
    return this._frame ? this._frame.contentWindow : null;
  },

  /**
   * Shortcut for accessing the list of breakpoints in the debugger.
   * @return object if a debugger window exists, null otherwise
   */
  get breakpoints() {
    let debuggerWindow = this.debuggerWindow;
    if (debuggerWindow) {
      return debuggerWindow.DebuggerController.Breakpoints.store;
    }
    return null;
  }
};

/**
 * Various debugger UI preferences (currently just the pane height).
 */
let DebuggerUIPreferences = {

  /**
   * Gets the preferred height of the debugger pane.
   * @return number
   */
  get height() {
    if (this._height === undefined) {
      this._height = Services.prefs.getIntPref("devtools.debugger.ui.height");
    }
    return this._height;
  },

  /**
   * Sets the preferred height of the debugger pane.
   * @param number value
   */
  set height(value) {
    Services.prefs.setIntPref("devtools.debugger.ui.height", value);
    this._height = value;
  }
};
