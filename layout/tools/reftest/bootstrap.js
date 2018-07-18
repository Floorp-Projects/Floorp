/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

const Cm = Components.manager;

ChromeUtils.import("resource://gre/modules/Services.jsm");

function processTerminated() {
  return new Promise(resolve => {
    Services.obs.addObserver(function observe(subject, topic) {
      if (topic == "ipc:content-shutdown") {
        Services.obs.removeObserver(observe, topic);
        resolve();
      }
    }, "ipc:content-shutdown");
  });
}

var WindowListener = {
  onOpenWindow: function(win) {
    Services.wm.removeListener(WindowListener);

    win = win.QueryInterface(Ci.nsIInterfaceRequestor).getInterface(Ci.nsIDOMWindow);
    win.addEventListener("load", function listener() {
      // Load into any existing windows.
      let windows = Services.wm.getEnumerator("navigator:browser");
      while (windows.hasMoreElements()) {
        win = windows.getNext().QueryInterface(Ci.nsIDOMWindow);
        break;
      }

      ChromeUtils.import("chrome://reftest/content/reftest.jsm");
      win.addEventListener("pageshow", function() {
        // Add setTimeout here because windows.innerWidth/Height are not set yet.
        win.setTimeout(function() {OnRefTestLoad(win);}, 0);
      }, {once: true});
    }, {once: true});
  }
};

function startup(data, reason) {
  if (Services.appinfo.OS == "Android") {
    Cm.addBootstrappedManifestLocation(data.installPath);
    Services.wm.addListener(WindowListener);
    return;
  }

  let orig = Services.wm.getMostRecentWindow("navigator:browser");

  let ios = Cc["@mozilla.org/network/io-service;1"]
              .getService(Ci.nsIIOService);
  ios.manageOfflineStatus = false;
  ios.offline = false;

  let wwatch = Cc["@mozilla.org/embedcomp/window-watcher;1"]
                .getService(Ci.nsIWindowWatcher);
  let dummy = wwatch.openWindow(null, "about:blank", "dummy",
                                "chrome,dialog=no,left=800,height=200,width=200,all",null);
  dummy.onload = async function() {
    // Close pre-existing window
    orig.close();

    ChromeUtils.import("chrome://reftest/content/PerTestCoverageUtils.jsm");
    if (PerTestCoverageUtils.enabled) {
      // In PerTestCoverage mode, wait for the process belonging to the window we just closed
      // to be terminated, to avoid its shutdown interfering when we reset the counters.
      await processTerminated();
    }

    dummy.focus();
    wwatch.openWindow(null, "chrome://reftest/content/reftest.xul", "_blank",
                      "chrome,dialog=no,all", {});
  };
}

function shutdown(data, reason) {
  if (Services.appinfo.OS == "Android") {
    Services.wm.removeListener(WindowListener);
    Cm.removedBootstrappedManifestLocation(data.installPath);
    OnRefTestUnload();
    Cu.unload("chrome://reftest/content/reftest.jsm");
  }
}


function install(data, reason) {}
function uninstall(data, reason) {}
