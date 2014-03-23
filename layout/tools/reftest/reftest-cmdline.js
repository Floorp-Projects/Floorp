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
    // For mochitests, we're more interested in testing the behavior of in-
    // content XBL bindings, so we set this pref to true. In reftests, we're
    // more interested in testing the behavior of XBL as it works in chrome,
    // so we want this pref to be false.
    branch.setBoolPref("dom.use_xbl_scopes_for_remote_xul", false);
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
    // Disable addon updates and prefetching so we don't leak them
    branch.setBoolPref("extensions.update.enabled", false);
    branch.setBoolPref("extensions.getAddons.cache.enabled", false);
    // Disable blocklist updates so we don't have them reported as leaks
    branch.setBoolPref("extensions.blocklist.enabled", false);
    // Make url-classifier updates so rare that they won't affect tests
    branch.setIntPref("urlclassifier.updateinterval", 172800);
    // Disable high-quality downscaling, since it makes reftests more difficult.
    branch.setBoolPref("image.high_quality_downscaling.enabled", false);
    // Checking whether two files are the same is slow on Windows.
    // Setting this pref makes tests run much faster there.
    branch.setBoolPref("security.fileuri.strict_origin_policy", false);
    // Disable the thumbnailing service
    branch.setBoolPref("browser.pagethumbnails.capturing_disabled", true);
    // Enable APZC so we can test it
    branch.setBoolPref("layers.async-pan-zoom.enabled", true);
    // Since our tests are 800px wide, set the assume-designed-for width of all
    // pages to be 800px (instead of the default of 980px). This ensures that
    // in our 800px window we don't zoom out by default to try to fit the
    // assumed 980px content.
    branch.setIntPref("browser.viewport.desktopWidth", 800);
    // Disable the fade out (over time) of overlay scrollbars, since we
    // can't guarantee taking both reftest snapshots at the same point
    // during the fade.
    branch.setBoolPref("layout.testing.overlay-scrollbars.always-visible", true);

    var wwatch = Components.classes["@mozilla.org/embedcomp/window-watcher;1"]
                           .getService(nsIWindowWatcher);

    function loadReftests() {
      wwatch.openWindow(null, "chrome://reftest/content/reftest.xul", "_blank",
                        "chrome,dialog=no,all", args);
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

  helpInfo : "  -reftest <file>    Run layout acceptance tests on given manifest.\n"
};

this.NSGetFactory = XPCOMUtils.generateNSGetFactory([RefTestCmdLineHandler]);
