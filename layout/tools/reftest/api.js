/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

const Cm = Components.manager;

ChromeUtils.import("resource://gre/modules/Services.jsm");
ChromeUtils.import("resource://gre/modules/XPCOMUtils.jsm");

XPCOMUtils.defineLazyServiceGetter(this, "resProto",
                                   "@mozilla.org/network/protocol;1?name=resource",
                                   "nsISubstitutingProtocolHandler");

XPCOMUtils.defineLazyServiceGetter(this, "aomStartup",
                                   "@mozilla.org/addons/addon-manager-startup;1",
                                   "amIAddonManagerStartup");

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

function startAndroid(win) {
  // Add setTimeout here because windows.innerWidth/Height are not set yet.
  win.setTimeout(function() {OnRefTestLoad(win);}, 0);
}

var WindowListener = {
  onOpenWindow: function(xulWin) {
    Services.wm.removeListener(WindowListener);

    let win = xulWin.docShell.domWindow;
    win.addEventListener("load", function listener() {
      // Load into any existing windows.
      for (win of Services.wm.getEnumerator("navigator:browser")) {
        break;
      }

      win.addEventListener("pageshow", function() {
        startAndroid(win);
      }, {once: true});
    }, {once: true});
  }
};

this.reftest = class extends ExtensionAPI {
  onStartup() {
    let uri = Services.io.newURI("chrome/reftest/res/", null, this.extension.rootURI);
    resProto.setSubstitutionWithFlags("reftest", uri, resProto.ALLOW_CONTENT_ACCESS);

    const manifestURI = Services.io.newURI("manifest.json", null, this.extension.rootURI);
    this.chromeHandle = aomStartup.registerChrome(manifestURI, [
      ["content", "reftest", "chrome/reftest/content/", "contentaccessible=yes"],
    ]);

    // Starting tests is handled quite differently on android and desktop.
    // On Android, OnRefTestLoad() takes over the main browser window so
    // we just need to call it as soon as the browser window is available.
    // On desktop, a separate window (dummy) is created and explicitly given
    // focus (see bug 859339 for details), then tests are launched in a new
    // top-level window.
    let win = Services.wm.getMostRecentWindow("navigator:browser");

    if (Services.appinfo.OS == "Android") {
      ChromeUtils.import("resource://reftest/reftest.jsm");
      if (win) {
        startAndroid(win);
      } else {
        Services.wm.addListener(WindowListener);
      }
      return;
    }

    Services.io.manageOfflineStatus = false;
    Services.io.offline = false;

    let dummy = Services.ww.openWindow(null, "about:blank", "dummy",
                                       "chrome,dialog=no,left=800,height=200,width=200,all",null);
    dummy.onload = async function() {
      // Close pre-existing window
      win.close();

      ChromeUtils.import("resource://reftest/PerTestCoverageUtils.jsm");
      if (PerTestCoverageUtils.enabled) {
        // In PerTestCoverage mode, wait for the process belonging to the window we just closed
        // to be terminated, to avoid its shutdown interfering when we reset the counters.
        await processTerminated();
      }

      dummy.focus();
      Services.ww.openWindow(null, "chrome://reftest/content/reftest.xul",
                             "_blank", "chrome,dialog=no,all", {});
    };
  }

  onShutdown() {
    resProto.setSubstitution("reftest", null);

    this.chromeHandle.destruct();
    this.chromeHandle = null;

    if (Services.appinfo.OS == "Android") {
      Services.wm.removeListener(WindowListener);
      OnRefTestUnload();
      Cu.unload("resource://reftest/reftest.jsm");
    }
  }
};
