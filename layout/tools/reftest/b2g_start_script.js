function setDefaultPrefs() {
    // This code sets the preferences for extension-based reftest; for
    // command-line based reftest they are set in function handler_handle in
    // reftest-cmdline.js.  These two locations should stay in sync.
    //
    // FIXME: These should be in only one place.
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
}

// Load into any existing windows
let wm = Components.classes["@mozilla.org/appshell/window-mediator;1"]
                   .getService(Components.interfaces.nsIWindowMediator);
let win = wm.getMostRecentWindow('');
setDefaultPrefs();
Components.utils.import("chrome://reftest/content/reftest.jsm");
OnRefTestLoad(win);
