/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

this.EXPORTED_SYMBOLS = ["Preferences"];

const {classes: Cc, interfaces: Ci, utils: Cu} = Components;

Cu.import("resource://gre/modules/Services.jsm");
Cu.import("resource://gre/modules/Task.jsm");
Cu.import("resource://gre/modules/Timer.jsm");
Cu.import("resource://testing-common/TestUtils.jsm");
Cu.import("resource://testing-common/ContentTask.jsm");

this.Preferences = {

  init(libDir) {
    Services.prefs.setBoolPref("browser.preferences.inContent", true);

    let panes = [
      ["paneGeneral", null],
      ["paneSearch", null],
      ["paneContent", null],
      ["paneApplications", null],
      ["panePrivacy", null],
      ["paneSecurity", null],
      ["paneSync", null],
      ["paneAdvanced", "generalTab"],
      ["paneAdvanced", "dataChoicesTab"],
      ["paneAdvanced", "networkTab"],
      ["paneAdvanced", "updateTab"],
      ["paneAdvanced", "encryptionTab"],
    ];
    for (let [primary, advanced] of panes) {
      let configName = primary.replace(/^pane/, "prefs") + (advanced ? "-" + advanced : "");
      this.configurations[configName] = {};
      this.configurations[configName].applyConfig = prefHelper.bind(null, primary, advanced);
    }
  },

  configurations: {
    "panePrivacy-DNTDialog": {
      applyConfig: Task.async(function*() {
        let browserWindow = Services.wm.getMostRecentWindow("navigator:browser");
        yield prefHelper("panePrivacy", null);

        yield ContentTask.spawn(browserWindow.gBrowser.selectedBrowser, null, function* () {
          content.document.getElementById("doNotTrackSettings").click();
        });
      }),
    },
  },
};

let prefHelper = Task.async(function*(primary, advanced) {
  let browserWindow = Services.wm.getMostRecentWindow("navigator:browser");
  let selectedBrowser = browserWindow.gBrowser;
  let readyPromise = null;
  if (selectedBrowser.currentURI.specIgnoringRef == "about:preferences") {
    readyPromise = new Promise((resolve) => {
      browserWindow.addEventListener("MozAfterPaint", function paneSwitch() {
        browserWindow.removeEventListener("MozAfterPaint", paneSwitch);
        resolve();
      });
    });

  } else {
    readyPromise = TestUtils.topicObserved("advanced-pane-loaded");
  }

  if (primary == "paneAdvanced") {
    browserWindow.openAdvancedPreferences(advanced);
  } else {
    browserWindow.openPreferences(primary);
  }

  yield readyPromise;

  // close any dialog that might still be open
  yield ContentTask.spawn(selectedBrowser.selectedBrowser, null, function*() {
    content.window.gSubDialog.close();
  });
});
