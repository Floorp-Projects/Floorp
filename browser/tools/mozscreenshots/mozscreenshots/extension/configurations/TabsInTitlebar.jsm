/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

var EXPORTED_SYMBOLS = ["TabsInTitlebar"];

const PREF_TABS_IN_TITLEBAR = "browser.tabs.inTitlebar";

var TabsInTitlebar = {
  init(libDir) {},

  configurations: {
    tabsInTitlebar: {
      selectors: ["#navigator-toolbox"],
      async applyConfig() {
        Services.prefs.setIntPref(PREF_TABS_IN_TITLEBAR, 1);
        return undefined;
      },
    },

    tabsOutsideTitlebar: {
      selectors: ["#navigator-toolbox"].concat(
        Services.appinfo.OS == "Linux" ? [] : ["#titlebar"]
      ),
      async applyConfig() {
        Services.prefs.setIntPref(PREF_TABS_IN_TITLEBAR, 0);
      },
    },
  },
};
