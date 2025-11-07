/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

console.log("Designs override loaded");

export const overrides = [
  () => {
    // Override the default newtab url in tabbar. If pref seted.
    // Experimental feature. Malware can change this pref to redirect user to malware site.
    const newtabOverrideURL = "floorp.newtab.overrides.newtaburl";
    if (Services.prefs.getStringPref(newtabOverrideURL, "") != "") {
      const { AboutNewTab } = ChromeUtils.importESModule(
        "resource:///modules/AboutNewTab.sys.mjs",
      );
      const newTabURL = Services.prefs.getStringPref(newtabOverrideURL);
      AboutNewTab.newTabURL = newTabURL;
    }
  },
];
