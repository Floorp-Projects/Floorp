/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

function setDefaultPrefs() {
    // This code sets the preferences for extension-based reftest; for
    // command-line based reftest they are set in function handler_handle in
    // reftest-cmdline.js.  These two locations should stay in sync.
    //
    // FIXME: These should be in only one place.
    var prefs = Cc["@mozilla.org/preferences-service;1"].
                getService(Ci.nsIPrefService);
    var branch = prefs.getDefaultBranch("");
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
    // Disable the fade out (over time) of overlay scrollbars, since we
    // can't guarantee taking both reftest snapshots at the same point
    // during the fade.
    branch.setBoolPref("layout.testing.overlay-scrollbars.always-visible", true);
}

function setPermissions() {
  if (__marionetteParams.length < 2) {
    return;
  }

  let serverAddr = __marionetteParams[0];
  let serverPort = __marionetteParams[1];
  let perms = Cc["@mozilla.org/permissionmanager;1"]
              .getService(Ci.nsIPermissionManager);
  let ioService = Cc["@mozilla.org/network/io-service;1"]
                  .getService(Ci.nsIIOService);
  let uri = ioService.newURI("http://" + serverAddr + ":" + serverPort, null, null);
  perms.add(uri, "allowXULXBL", Ci.nsIPermissionManager.ALLOW_ACTION);
}

// Load into any existing windows
let wm = Cc["@mozilla.org/appshell/window-mediator;1"]
            .getService(Ci.nsIWindowMediator);
let win = wm.getMostRecentWindow('');

// Set preferences and permissions
setDefaultPrefs();
setPermissions();

// Loading this into the global namespace causes intermittent failures.
// See bug 882888 for more details.
let reftest = {};
Cu.import("chrome://reftest/content/reftest.jsm", reftest);

// Start the reftests
reftest.OnRefTestLoad(win);
