/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

const { Services } = ChromeUtils.import(
  "resource://gre/modules/Services.jsm"
);

let gTabSleepManager = {
  onLoad() {
    document.getElementById("timeoutMinutes").value =
      Services.prefs.getIntPref("floorp.tabsleep.tabTimeoutMinutes", undefined);

    document.getElementById("excludeHosts").value =
      Services.prefs.getStringPref("floorp.tabsleep.excludeHosts", "")
      .split(",")
      .map(host => host.trim())
      .join("\n");

    let params = window.arguments[0] || {};
    this.init(params);
  },

  init(aParams) {
    document.addEventListener("dialogaccept", () => this.onApplyChanges());
  },

  uninit() {},

  onApplyChanges() {
    let timeoutMinutes = document.getElementById("timeoutMinutes").value;
    let excludeHosts = document.getElementById("excludeHosts").value;

    Services.prefs.setIntPref(
      "floorp.tabsleep.tabTimeoutMinutes",
      Number(timeoutMinutes)
    );

    Services.prefs.setStringPref(
      "floorp.tabsleep.excludeHosts",
      excludeHosts
        .replace("\r\n", "\n")
        .replace("\r", "\n")
        .split("\n")
        .map(host => host.trim())
        .join(",")
    );
  },
};
