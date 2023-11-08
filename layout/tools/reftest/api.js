/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

// These globals are available to extensions using privileged APIs.
/* globals XPCOMUtils, ExtensionAPI */

const Cm = Components.manager;

var OnRefTestLoad, OnRefTestUnload;

XPCOMUtils.defineLazyServiceGetter(
  this,
  "resProto",
  "@mozilla.org/network/protocol;1?name=resource",
  "nsISubstitutingProtocolHandler"
);

XPCOMUtils.defineLazyServiceGetter(
  this,
  "aomStartup",
  "@mozilla.org/addons/addon-manager-startup;1",
  "amIAddonManagerStartup"
);

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
  win.setTimeout(function () {
    OnRefTestLoad(win);
  }, 0);
}

function GetMainWindow() {
  let win = Services.wm.getMostRecentWindow("navigator:browser");
  if (!win) {
    // There is no navigator:browser in the geckoview TestRunnerActivity;
    // try navigator.geckoview instead.
    win = Services.wm.getMostRecentWindow("navigator:geckoview");
  }
  return win;
}

this.reftest = class extends ExtensionAPI {
  onStartup() {
    let uri = Services.io.newURI(
      "chrome/reftest/res/",
      null,
      this.extension.rootURI
    );
    resProto.setSubstitutionWithFlags(
      "reftest",
      uri,
      resProto.ALLOW_CONTENT_ACCESS
    );

    const manifestURI = Services.io.newURI(
      "manifest.json",
      null,
      this.extension.rootURI
    );

    let manifestDirectives = [
      [
        "content",
        "reftest",
        "chrome/reftest/content/",
        "contentaccessible=yes",
      ],
    ];
    if (Services.appinfo.OS == "Android") {
      manifestDirectives.push([
        "override",
        "chrome://global/skin/global.css",
        "chrome://reftest/content/fake-global.css",
      ]);
    }
    this.chromeHandle = aomStartup.registerChrome(
      manifestURI,
      manifestDirectives
    );

    // Starting tests is handled quite differently on android and desktop.
    // On Android, OnRefTestLoad() takes over the main browser window so
    // we just need to call it as soon as the browser window is available.
    // On desktop, a separate window (dummy) is created and explicitly given
    // focus (see bug 859339 for details), then tests are launched in a new
    // top-level window.
    let win = GetMainWindow();
    if (Services.appinfo.OS == "Android") {
      ({ OnRefTestLoad, OnRefTestUnload } = ChromeUtils.importESModule(
        "resource://reftest/reftest.sys.mjs"
      ));
      if (win) {
        startAndroid(win);
      } else {
        // The window type parameter is only available once the window's document
        // element has been created. The main window has already been created
        // however and it is in an in-between state which means that you can't
        // find it by its type nor will domwindowcreated be fired.
        // So we listen to either initial-document-element-inserted which
        // indicates when it's okay to search for the main window by type again.
        Services.obs.addObserver(function observer(aSubject, aTopic, aData) {
          Services.obs.removeObserver(observer, aTopic);
          startAndroid(GetMainWindow());
        }, "initial-document-element-inserted");
      }
      return;
    }

    Services.io.manageOfflineStatus = false;
    Services.io.offline = false;

    let dummy = Services.ww.openWindow(
      null,
      "about:blank",
      "dummy",
      "chrome,dialog=no,left=800,height=200,width=200,all",
      null
    );
    dummy.onload = async function () {
      // Close pre-existing window
      win.close();

      const { PerTestCoverageUtils } = ChromeUtils.importESModule(
        "resource://reftest/PerTestCoverageUtils.sys.mjs"
      );
      if (PerTestCoverageUtils.enabled) {
        // In PerTestCoverage mode, wait for the process belonging to the window we just closed
        // to be terminated, to avoid its shutdown interfering when we reset the counters.
        await processTerminated();
      }

      dummy.focus();
      Services.ww.openWindow(
        null,
        "chrome://reftest/content/reftest.xhtml",
        "_blank",
        "chrome,dialog=no,all",
        {}
      );
    };
  }

  onShutdown() {
    resProto.setSubstitution("reftest", null);

    this.chromeHandle.destruct();
    this.chromeHandle = null;

    if (Services.appinfo.OS == "Android") {
      OnRefTestUnload();
    }
  }
};
