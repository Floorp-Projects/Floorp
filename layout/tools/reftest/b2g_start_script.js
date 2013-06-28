args = __marionetteParams;

function setDefaultPrefs() {
    // This code sets the preferences for extension-based reftest; for
    // command-line based reftest they are set in function handler_handle in
    // reftest-cmdline.js.  These two locations should stay in sync.
    //
    // FIXME: These should be in only one place.
    var prefs = Components.classes["@mozilla.org/preferences-service;1"].
                getService(Components.interfaces.nsIPrefService);
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
}

function setPermissions(webserver, port) {
  var perms = Components.classes["@mozilla.org/permissionmanager;1"]
              .getService(Components.interfaces.nsIPermissionManager);
  var ioService = Components.classes["@mozilla.org/network/io-service;1"]
                  .getService(Components.interfaces.nsIIOService);
  var uri = ioService.newURI("http://" + webserver + ":" + port, null, null);
  perms.add(uri, "allowXULXBL", Components.interfaces.nsIPermissionManager.ALLOW_ACTION);
}

// Load into any existing windows
let wm = Components.classes["@mozilla.org/appshell/window-mediator;1"]
                   .getService(Components.interfaces.nsIWindowMediator);
let win = wm.getMostRecentWindow('');

// Set preferences and permissions
setDefaultPrefs();
setPermissions(args[0], args[1]);

// Loading this into the global namespace causes intermittent failures.
// See bug 882888 for more details.
let reftest = {}; Components.utils.import("chrome://reftest/content/reftest.jsm", reftest);

// Start the reftests
reftest.OnRefTestLoad(win);
