/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

this.EXPORTED_SYMBOLS = ["Preferences"];

const {classes: Cc, interfaces: Ci, utils: Cu} = Components;

Cu.import("resource://gre/modules/Services.jsm");
Cu.import("resource://gre/modules/Task.jsm");
Cu.import("resource://testing-common/TestUtils.jsm");
Cu.import("resource://testing-common/ContentTask.jsm");

this.Preferences = {

  init(libDir) {
    let panes = [
      ["paneGeneral", null],
      ["paneSearch", null],
      ["paneContent", null],
      ["paneApplications", null],
      ["panePrivacy", null],
      ["panePrivacy", null, DNTDialog],
      ["paneSecurity", null],
      ["paneSync", null],
      ["paneAdvanced", "generalTab"],
      ["paneAdvanced", "dataChoicesTab"],
      ["paneAdvanced", "networkTab"],
      ["paneAdvanced", "networkTab", connectionDialog],
      ["paneAdvanced", "updateTab"],
      ["paneAdvanced", "encryptionTab"],
      ["paneAdvanced", "encryptionTab", certManager],
      ["paneAdvanced", "encryptionTab", deviceManager],
    ];
    for (let [primary, advanced, customFn] of panes) {
      let configName = primary.replace(/^pane/, "prefs") + (advanced ? "-" + advanced : "");
      if (customFn) {
        configName += "-" + customFn.name;
      }
      this.configurations[configName] = {};
      this.configurations[configName].applyConfig = prefHelper.bind(null, primary, advanced, customFn);
    }
  },

  configurations: {},
};

let prefHelper = Task.async(function*(primary, advanced = null, customFn = null) {
  let browserWindow = Services.wm.getMostRecentWindow("navigator:browser");
  let selectedBrowser = browserWindow.gBrowser.selectedBrowser;

  // close any dialog that might still be open
  yield ContentTask.spawn(selectedBrowser, null, function*() {
    if (!content.window.gSubDialog) {
      return;
    }
    content.window.gSubDialog.close();
  });

  let readyPromise = null;
  if (selectedBrowser.currentURI.specIgnoringRef == "about:preferences") {
    if (selectedBrowser.currentURI.spec == "about:preferences#" + primary.replace(/^pane/, "")) {
      // We're already on the correct pane.
      readyPromise = Promise.resolve();
    } else {
      readyPromise = paintPromise(browserWindow);
    }
  } else {
    readyPromise = TestUtils.topicObserved("advanced-pane-loaded");
  }

  if (primary == "paneAdvanced") {
    browserWindow.openAdvancedPreferences(advanced);
  } else {
    browserWindow.openPreferences(primary);
  }

  yield readyPromise;

  if (customFn) {
    let customPaintPromise = paintPromise(browserWindow);
    yield* customFn(selectedBrowser);
    yield customPaintPromise;
  }
});

function paintPromise(browserWindow) {
  return new Promise((resolve) => {
    browserWindow.addEventListener("MozAfterPaint", function onPaint() {
      browserWindow.removeEventListener("MozAfterPaint", onPaint);
      resolve();
    });
  });
}

function* DNTDialog(aBrowser) {
  yield ContentTask.spawn(aBrowser, null, function* () {
    content.document.getElementById("doNotTrackSettings").click();
  });
}

function* connectionDialog(aBrowser) {
  yield ContentTask.spawn(aBrowser, null, function* () {
    content.document.getElementById("connectionSettings").click();
  });
}

function* certManager(aBrowser) {
  yield ContentTask.spawn(aBrowser, null, function* () {
    content.document.getElementById("viewCertificatesButton").click();
  });
}

function* deviceManager(aBrowser) {
  yield ContentTask.spawn(aBrowser, null, function* () {
    content.document.getElementById("viewSecurityDevicesButton").click();
  });
}
