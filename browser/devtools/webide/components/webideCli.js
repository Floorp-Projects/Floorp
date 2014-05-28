/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

const Ci = Components.interfaces;
const Cu = Components.utils;

Cu.import("resource://gre/modules/XPCOMUtils.jsm");

XPCOMUtils.defineLazyModuleGetter(this, "Services", "resource://gre/modules/Services.jsm");

/**
 * Handles -webide command line option.
 *
 * See webide/content/cli.js for a complete description of the command line.
 */

function webideCli() { }

webideCli.prototype = {
  handle: function(cmdLine) {
    let param;

    try {
      // Returns null if -webide is not present
      // Throws if -webide is present with no params
      param = cmdLine.handleFlagWithParam("webide", false);
      if (!param) {
        return;
      }
    } catch(e) {
      // -webide is present with no params
      cmdLine.handleFlag("webide", false);
    }

    // If -webide is used remotely, we don't want to open
    // a new tab.
    //
    // If -webide is used for a new Firefox instance, we
    // want to open webide only.
    cmdLine.preventDefault = true;

    let win = Services.wm.getMostRecentWindow("devtools:webide");
    if (win) {
      win.focus();
      if (param) {
        win.handleCommandline(param);
      }
      return;
    }

    win = Services.ww.openWindow(null,
                                 "chrome://webide/content/",
                                 "webide",
                                 "chrome,centerscreen,resizable,dialog=no",
                                 null);

    if (param) {
      win.addEventListener("load", function onLoad() {
        win.removeEventListener("load", onLoad, true);
        // next tick
        win.setTimeout(() => win.handleCommandline(param), 0);
      }, true);
    }

    if (cmdLine.state == Ci.nsICommandLine.STATE_INITIAL_LAUNCH) {
      // If this is a new Firefox instance, and because we will only start
      // webide, we need to notify "sessionstore-windows-restored" to trigger
      // addons registration (for simulators and adb helper).
      Services.obs.notifyObservers(null, "sessionstore-windows-restored", "");
    }
  },


  helpInfo : "",

  classID: Components.ID("{79b7b44e-de5e-4e4c-b7a2-044003c615d9}"),
  QueryInterface: XPCOMUtils.generateQI([Ci.nsICommandLineHandler]),
};

this.NSGetFactory = XPCOMUtils.generateNSGetFactory([webideCli]);
