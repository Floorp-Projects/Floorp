/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

this.EXPORTED_SYMBOLS = ["DevEdition"];

const {classes: Cc, interfaces: Ci, utils: Cu} = Components;
const THEME_ID = "firefox-devedition@mozilla.org";

Cu.import("resource://gre/modules/LightweightThemeManager.jsm");
Cu.import("resource://gre/modules/Services.jsm");
Cu.import("resource://gre/modules/Task.jsm");

this.DevEdition = {
  init(libDir) {},

  configurations: {
    devEditionLight: {
      applyConfig: Task.async(() => {
        Services.prefs.setCharPref("devtools.theme", "light");
        LightweightThemeManager.currentTheme = LightweightThemeManager.getUsedTheme(THEME_ID);
        Services.prefs.setBoolPref("browser.devedition.theme.showCustomizeButton", true);
      }),
    },
    devEditionDark: {
      applyConfig: Task.async(() => {
        Services.prefs.setCharPref("devtools.theme", "dark");
        LightweightThemeManager.currentTheme = LightweightThemeManager.getUsedTheme(THEME_ID);
        Services.prefs.setBoolPref("browser.devedition.theme.showCustomizeButton", true);
      }),
    },
    devEditionOff: {
      applyConfig: Task.async(() => {
        Services.prefs.clearUserPref("devtools.theme");
        LightweightThemeManager.currentTheme = null;
        Services.prefs.clearUserPref("browser.devedition.theme.showCustomizeButton");
      }),
    },
  },
};
