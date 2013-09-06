Components.utils.import("resource://gre/modules/FileUtils.jsm");        

function loadIntoWindow(window) {}
function unloadFromWindow(window) {}

function setDefaultPrefs() {
    // This code sets the preferences for extension-based reftest; for
    // command-line based reftest they are set in function handler_handle in
    // reftest-cmdline.js.  These two locations should stay in sync.
    //
    // FIXME: These should be in only one place.
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
}

var windowListener = {
    onOpenWindow: function(aWindow) {
        let domWindow = aWindow.QueryInterface(Components.interfaces.nsIInterfaceRequestor).getInterface(Components.interfaces.nsIDOMWindowInternal || Components.interfaces.nsIDOMWindow);
        domWindow.addEventListener("load", function() {
            domWindow.removeEventListener("load", arguments.callee, false);

            let wm = Components.classes["@mozilla.org/appshell/window-mediator;1"].getService(Components.interfaces.nsIWindowMediator);

            // Load into any existing windows
            let enumerator = wm.getEnumerator("navigator:browser");
            while (enumerator.hasMoreElements()) {
                let win = enumerator.getNext().QueryInterface(Components.interfaces.nsIDOMWindow);
                setDefaultPrefs();
                Components.utils.import("chrome://reftest/content/reftest.jsm");
                win.addEventListener("pageshow", function() {
                    win.removeEventListener("pageshow", arguments.callee); 
                    // We add a setTimeout here because windows.innerWidth/Height are not set yet;
                    win.setTimeout(function () {OnRefTestLoad(win);}, 0);
                });
                break;
            }
        }, false);
   },
   onCloseWindow: function(aWindow){ },
   onWindowTitleChange: function(){ },
};

function startup(aData, aReason) {
    let wm = Components.classes["@mozilla.org/appshell/window-mediator;1"].
             getService (Components.interfaces.nsIWindowMediator);

    Components.manager.addBootstrappedManifestLocation(aData.installPath);

    // Load into any new windows
    wm.addListener(windowListener);
}

function shutdown(aData, aReason) {
    // When the application is shutting down we normally don't have to clean up any UI changes
    if (aReason == APP_SHUTDOWN)
        return;

    let wm = Components.classes["@mozilla.org/appshell/window-mediator;1"].
             getService(Components.interfaces.nsIWindowMediator);

    // Stop watching for new windows
    wm.removeListener(windowListener);

    // Unload from any existing windows
    let enumerator = wm.getEnumerator("navigator:browser");
    while (enumerator.hasMoreElements()) {
        let win = enumerator.getNext().QueryInterface(Components.interfaces.nsIDOMWindow);
        unloadFromWindow(win);
    }
}

function install(aData, aReason) { }
function uninstall(aData, aReason) { }

