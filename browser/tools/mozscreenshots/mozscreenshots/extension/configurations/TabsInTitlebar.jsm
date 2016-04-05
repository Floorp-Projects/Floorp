/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

this.EXPORTED_SYMBOLS = ["TabsInTitlebar"];

const {classes: Cc, interfaces: Ci, utils: Cu} = Components;

const PREF_TABS_IN_TITLEBAR = "browser.tabs.drawInTitlebar";

Cu.import("resource://gre/modules/Services.jsm");
Cu.import("resource://gre/modules/Task.jsm");

this.TabsInTitlebar = {

  init(libDir) {},

  configurations: {
    tabsInTitlebar: {
      applyConfig: Task.async(function*() {
        if (Services.appinfo.OS == "Linux") {
          return Promise.reject("TabsInTitlebar isn't supported on Linux");
        }
        Services.prefs.setBoolPref(PREF_TABS_IN_TITLEBAR, true);
        return undefined;
      }),
    },

    tabsOutsideTitlebar: {
      applyConfig: Task.async(function*() {
        Services.prefs.setBoolPref(PREF_TABS_IN_TITLEBAR, false);
      }),
    },

  },
};
