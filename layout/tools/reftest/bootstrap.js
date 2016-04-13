/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

const { utils: Cu, interfaces: Ci, classes: Cc, manager: Cm } = Components;

Cu.import("resource://gre/modules/FileUtils.jsm");
Cu.import("resource://gre/modules/Services.jsm");

var WindowListener = {
  onOpenWindow: function(win) {
    Services.wm.removeListener(WindowListener);

    win = win.QueryInterface(Ci.nsIInterfaceRequestor).getInterface(Ci.nsIDOMWindow);
    win.addEventListener("load", function listener() {
      win.removeEventListener("load", listener, false);

      // Load into any existing windows.
      let windows = Services.wm.getEnumerator("navigator:browser");
      while (windows.hasMoreElements()) {
        win = windows.getNext().QueryInterface(Ci.nsIDOMWindow);
        break;
      }

      Cu.import("chrome://reftest/content/reftest.jsm");
      win.addEventListener("pageshow", function listener() {
        win.removeEventListener("pageshow", listener);
        // Add setTimeout here because windows.innerWidth/Height are not set yet.
        win.setTimeout(function() {OnRefTestLoad(win);}, 0);
      });
    }, false);
  }
};

function startup(data, reason) {
  // b2g is bootstrapped by b2g_start_script.js
  if (Services.appinfo.widgetToolkit == "gonk") {
    return;
  }

  if (Services.appinfo.OS == "Android") {
    Cm.addBootstrappedManifestLocation(data.installPath);
    Services.wm.addListener(WindowListener);
    return;
  }

  let orig = Services.wm.getMostRecentWindow("navigator:browser");

  let ios = Cc["@mozilla.org/network/io-service;1"]
            .getService(Ci.nsIIOService2);
  ios.manageOfflineStatus = false;
  ios.offline = false;

  let wwatch = Cc["@mozilla.org/embedcomp/window-watcher;1"]
                .getService(Ci.nsIWindowWatcher);
  let dummy = wwatch.openWindow(null, "about:blank", "dummy",
                                "chrome,dialog=no,left=800,height=200,width=200,all",null);
  dummy.onload = function() {
    // Close pre-existing window
    orig.close();

    dummy.focus();
    wwatch.openWindow(null, "chrome://reftest/content/reftest.xul", "_blank",
                      "chrome,dialog=no,all", {});
  };
}

function shutdown(data, reason) {
  if (Services.appinfo.widgetToolkit == "gonk") {
    return;
  }

  if (Services.appinfo.OS == "Android") {
    Services.wm.removeListener(WindowListener);
    Cm.removedBootstrappedManifestLocation(data.installPath);
    OnRefTestUnload();
    Cu.unload("chrome://reftest/content/reftest.jsm");
  }
}


function install(data, reason) {}
function uninstall(data, reason) {}
