/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

this.EXPORTED_SYMBOLS = ["Preferences"];

const {classes: Cc, interfaces: Ci, utils: Cu} = Components;

Cu.import("resource://gre/modules/Services.jsm");
Cu.import("resource://gre/modules/Timer.jsm");

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
      let configName = primary + ("-" + advanced || "");
      this.configurations[configName] = {};
      this.configurations[configName].applyConfig = prefHelper.bind(null, primary, advanced);
    }
  },

  configurations: {},
};

function prefHelper(primary, advanced) {
  return new Promise((resolve) => {
    let browserWindow = Services.wm.getMostRecentWindow("navigator:browser");
    if (primary == "paneAdvanced") {
      browserWindow.openAdvancedPreferences(advanced);
    } else {
      browserWindow.openPreferences(primary);
    }
    setTimeout(resolve, 50);
  });
}
