Components.utils.import("resource://gre/modules/FileUtils.jsm");        

function loadIntoWindow(window) {}
function unloadFromWindow(window) {}

function setDefaultPrefs() {
    // This code sets the preferences for extension-based reftest.
    var prefs = Components.classes["@mozilla.org/preferences-service;1"].
                getService(Components.interfaces.nsIPrefService);
    var branch = prefs.getDefaultBranch("");

#include reftest-preferences.js
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

