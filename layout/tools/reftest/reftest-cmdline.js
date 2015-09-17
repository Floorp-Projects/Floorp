/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

Components.utils.import("resource://gre/modules/XPCOMUtils.jsm");

const nsISupports                    = Components.interfaces.nsISupports;
  
const nsICommandLine                 = Components.interfaces.nsICommandLine;
const nsICommandLineHandler          = Components.interfaces.nsICommandLineHandler;
const nsISupportsString              = Components.interfaces.nsISupportsString;
const nsIWindowWatcher               = Components.interfaces.nsIWindowWatcher;

function RefTestCmdLineHandler() {}
RefTestCmdLineHandler.prototype =
{
  classID: Components.ID('{32530271-8c1b-4b7d-a812-218e42c6bb23}'),

  /* nsISupports */
  QueryInterface: XPCOMUtils.generateQI([nsICommandLineHandler]),

  /* nsICommandLineHandler */
  handle : function handler_handle(cmdLine) {
    /* Ignore the platform's online/offline status while running reftests. */
    var ios = Components.classes["@mozilla.org/network/io-service;1"]
              .getService(Components.interfaces.nsIIOService2);
    ios.manageOfflineStatus = false;
    ios.offline = false;

    /**
     * Manipulate preferences by adding to the *default* branch.  Adding
     * to the default branch means the changes we make won't get written
     * back to user preferences.
     *
     * We want to do this here rather than in reftest.js because it's
     * important to force sRGB as an output profile for color management
     * before we load a window.
     */
    var prefs = Components.classes["@mozilla.org/preferences-service;1"].
                getService(Components.interfaces.nsIPrefService);
    var branch = prefs.getDefaultBranch("");

#include reftest-preferences.js

    var wwatch = Components.classes["@mozilla.org/embedcomp/window-watcher;1"]
                           .getService(nsIWindowWatcher);

    function loadReftests() {
      wwatch.openWindow(null, "chrome://reftest/content/reftest.xul", "_blank",
                        "chrome,dialog=no,all", {});
    }

    var remote = false;
    try {
      remote = prefs.getBoolPref("reftest.remote");
    } catch (ex) {
    }

    // If we are running on a remote machine, assume that we can't open another
    // window for transferring focus to when tests don't require focus.
    if (remote) {
      loadReftests();
    }
    else {
      // This dummy window exists solely for enforcing proper focus discipline.
      var dummy = wwatch.openWindow(null, "about:blank", "dummy",
                                    "chrome,dialog=no,left=800,height=200,width=200,all", null);
      dummy.onload = function dummyOnload() {
        dummy.focus();
        loadReftests();
      }
    }

    cmdLine.preventDefault = true;
  },

  helpInfo : "  --reftest <file>   Run layout acceptance tests on given manifest.\n"
};

this.NSGetFactory = XPCOMUtils.generateNSGetFactory([RefTestCmdLineHandler]);
