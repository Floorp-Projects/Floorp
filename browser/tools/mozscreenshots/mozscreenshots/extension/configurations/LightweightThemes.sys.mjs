/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

import { AddonManager } from "resource://gre/modules/AddonManager.sys.mjs";

export var LightweightThemes = {
  init() {},

  configurations: {
    noLWT: {
      selectors: [],
      async applyConfig() {
        let addon = await AddonManager.getAddonByID(
          "default-theme@mozilla.org"
        );
        await addon.enable();
      },
    },

    compactLight: {
      selectors: [],
      async applyConfig() {
        let addon = await AddonManager.getAddonByID(
          "firefox-compact-light@mozilla.org"
        );
        await addon.enable();
      },
    },

    compactDark: {
      selectors: [],
      async applyConfig() {
        let addon = await AddonManager.getAddonByID(
          "firefox-compact-dark@mozilla.org"
        );
        await addon.enable();
      },
    },

    alpenGlow: {
      selectors: [],
      async applyConfig() {
        let addon = await AddonManager.getAddonByID(
          "firefox-alpenglow@mozilla.org"
        );
        await addon.enable();
      },
    },
  },
};
