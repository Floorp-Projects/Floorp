/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

const Ci = Components.interfaces;
const Cu = Components.utils;

Cu.import("resource://gre/modules/XPCOMUtils.jsm");

XPCOMUtils.defineLazyModuleGetter(this, "Services", "resource://gre/modules/Services.jsm");

/**
 * Handles --webide command line option.
 */

function webideCli() { }

webideCli.prototype = {
  handle: function(cmdLine) {
    let param;

    if (!cmdLine.handleFlag("webide", false)) {
      return;
    }

    // If --webide is used remotely, we don't want to open
    // a new tab.
    //
    // If --webide is used for a new Firefox instance, we
    // want to open webide only.
    cmdLine.preventDefault = true;

    let win = Services.wm.getMostRecentWindow("devtools:webide");
    if (win) {
      win.focus();
    } else {
      win = Services.ww.openWindow(null,
                                   "chrome://webide/content/",
                                   "webide",
                                   "chrome,centerscreen,resizable,dialog=no",
                                   null);
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
