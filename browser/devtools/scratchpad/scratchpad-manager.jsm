/* vim:set ts=2 sw=2 sts=2 et tw=80:
 * ***** BEGIN LICENSE BLOCK *****
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
 * The Original Code is Scratchpad
 *
 * The Initial Developer of the Original Code is
 * The Mozilla Foundation.
 * Portions created by the Initial Developer are Copyright (C) 2011
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Heather Arthur <fayearthur@gmail.com> (original author)
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
 * ***** END LICENSE BLOCK *****/

"use strict";

var EXPORTED_SYMBOLS = ["ScratchpadManager"];

const Cc = Components.classes;
const Ci = Components.interfaces;
const Cu = Components.utils;

const SCRATCHPAD_WINDOW_URL = "chrome://browser/content/scratchpad.xul";
const SCRATCHPAD_WINDOW_FEATURES = "chrome,titlebar,toolbar,centerscreen,resizable,dialog=no";

Cu.import("resource://gre/modules/Services.jsm");

/**
 * The ScratchpadManager object opens new Scratchpad windows and manages the state
 * of open scratchpads for session restore. There's only one ScratchpadManager in
 * the life of the browser.
 */
var ScratchpadManager = {

  _scratchpads: [],

  /**
   * Get the saved states of open scratchpad windows. Called by
   * session restore.
   *
   * @return array
   *         The array of scratchpad states.
   */
  getSessionState: function SPM_getSessionState()
  {
    return this._scratchpads;
  },

  /**
   * Restore scratchpad windows from the scratchpad session store file.
   * Called by session restore.
   *
   * @param function aSession
   *        The session object with scratchpad states.
   *
   * @return array
   *         The restored scratchpad windows.
   */
  restoreSession: function SPM_restoreSession(aSession)
  {
    if (!Array.isArray(aSession)) {
      return [];
    }

    let wins = [];
    aSession.forEach(function(state) {
      let win = this.openScratchpad(state);
      wins.push(win);
    }, this);

    return wins;
  },

  /**
   * Iterate through open scratchpad windows and save their states.
   */
  saveOpenWindows: function SPM_saveOpenWindows() {
    this._scratchpads = [];

    let enumerator = Services.wm.getEnumerator("devtools:scratchpad");
    while (enumerator.hasMoreElements()) {
      let win = enumerator.getNext();
      if (!win.closed) {
        this._scratchpads.push(win.Scratchpad.getState());
      }
    }
  },

  /**
   * Open a new scratchpad window with an optional initial state.
   *
   * @param object aState
   *        Optional. The initial state of the scratchpad, an object
   *        with properties filename, text, and executionContext.
   *
   * @return nsIDomWindow
   *         The opened scratchpad window.
   */
  openScratchpad: function SPM_openScratchpad(aState)
  {
    let params = null;
    if (aState) {
      if (typeof aState != 'object') {
        return;
      }
      params = Cc["@mozilla.org/embedcomp/dialogparam;1"]
               .createInstance(Ci.nsIDialogParamBlock);
      params.SetNumberStrings(1);
      params.SetString(0, JSON.stringify(aState));
    }
    let win = Services.ww.openWindow(null, SCRATCHPAD_WINDOW_URL, "_blank",
                                     SCRATCHPAD_WINDOW_FEATURES, params);
    // Only add shutdown observer if we've opened a scratchpad window
    ShutdownObserver.init();

    return win;
  }
};


/**
 * The ShutdownObserver listens for app shutdown and saves the current state
 * of the scratchpads for session restore.
 */
var ShutdownObserver = {
  _initialized: false,

  init: function SDO_init()
  {
    if (this._initialized) {
      return;
    }

    Services.obs.addObserver(this, "quit-application-granted", false);
    this._initialized = true;
  },

  observe: function SDO_observe(aMessage, aTopic, aData)
  {
    if (aTopic == "quit-application-granted") {
      ScratchpadManager.saveOpenWindows();
      this.uninit();
    }
  },

  uninit: function SDO_uninit()
  {
    Services.obs.removeObserver(this, "quit-application-granted");
  }
};
