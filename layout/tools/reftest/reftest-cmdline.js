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
    var args = { };
    args.wrappedJSObject = args;
    try {
      var uristr = cmdLine.handleFlagWithParam("reftest", false);
      if (uristr == null)
        return;
      try {
        args.uri = cmdLine.resolveURI(uristr).spec;
      }
      catch (e) {
        return;
      }
    }
    catch (e) {
      cmdLine.handleFlag("reftest", true);
    }

    try {
      var nocache = cmdLine.handleFlag("reftestnocache", false);
      args.nocache = nocache;
    }
    catch (e) {
    }

    try {
      var skipslowtests = cmdLine.handleFlag("reftestskipslowtests", false);
      args.skipslowtests = skipslowtests;
    }
    catch (e) {
    }

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
     *
     * If you change these, please adjust them in the bootstrap.js function 
     * setDefaultPrefs().  These are duplicated there so we can have a 
     * restartless addon for reftest on native Android.
     *
     * FIXME: These should be in only one place. 
     */
    var prefs = Components.classes["@mozilla.org/preferences-service;1"].
                getService(Components.interfaces.nsIPrefService);
    var branch = prefs.getDefaultBranch("");
    branch.setBoolPref("gfx.color_management.force_srgb", true);
    branch.setBoolPref("browser.dom.window.dump.enabled", true);
    branch.setIntPref("ui.caretBlinkTime", -1);
    branch.setBoolPref("dom.send_after_paint_to_content", true);
    // no slow script dialogs
    branch.setIntPref("dom.max_script_run_time", 0);
    branch.setIntPref("dom.max_chrome_script_run_time", 0);
    branch.setIntPref("hangmonitor.timeout", 0);
    // Ensure autoplay is enabled for all platforms.
    branch.setBoolPref("media.autoplay.enabled", true);
    // Disable updates
    branch.setBoolPref("app.update.enabled", false);

    var wwatch = Components.classes["@mozilla.org/embedcomp/window-watcher;1"]
                           .getService(nsIWindowWatcher);
    wwatch.openWindow(null, "chrome://reftest/content/reftest.xul", "_blank",
                      "chrome,dialog=no,all", args);
    cmdLine.preventDefault = true;
  },

  helpInfo : "  -reftest <file>    Run layout acceptance tests on given manifest.\n"
};

var NSGetFactory = XPCOMUtils.generateNSGetFactory([RefTestCmdLineHandler]);
