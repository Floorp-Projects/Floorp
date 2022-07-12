/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const EXPORTED_SYMBOLS = ["OpenInTabsUtils"];

const { XPCOMUtils } = ChromeUtils.importESModule(
  "resource://gre/modules/XPCOMUtils.sys.mjs"
);

const lazy = {};

XPCOMUtils.defineLazyGetter(lazy, "bundle", function() {
  return Services.strings.createBundle(
    "chrome://browser/locale/tabbrowser.properties"
  );
});

/**
 * Utility functions that can be used when opening multiple tabs, that can be
 * called without any tabbrowser instance.
 */
const OpenInTabsUtils = {
  getString(key) {
    return lazy.bundle.GetStringFromName(key);
  },

  getFormattedString(key, params) {
    return lazy.bundle.formatStringFromName(key, params);
  },

  /**
   * Gives the user a chance to cancel loading lots of tabs at once.
   */
  confirmOpenInTabs(numTabsToOpen, aWindow) {
    const WARN_ON_OPEN_PREF = "browser.tabs.warnOnOpen";
    const MAX_OPNE_PREF = "browser.tabs.maxOpenBeforeWarn";
    if (!Services.prefs.getBoolPref(WARN_ON_OPEN_PREF)) {
      return true;
    }
    if (numTabsToOpen < Services.prefs.getIntPref(MAX_OPNE_PREF)) {
      return true;
    }

    // default to true: if it were false, we wouldn't get this far
    let warnOnOpen = { value: true };

    const messageKey = "tabs.openWarningMultipleBranded";
    const openKey = "tabs.openButtonMultiple";
    const BRANDING_BUNDLE_URI = "chrome://branding/locale/brand.properties";
    let brandShortName = Services.strings
      .createBundle(BRANDING_BUNDLE_URI)
      .GetStringFromName("brandShortName");

    let buttonPressed = Services.prompt.confirmEx(
      aWindow,
      this.getString("tabs.openWarningTitle"),
      this.getFormattedString(messageKey, [numTabsToOpen, brandShortName]),
      Services.prompt.BUTTON_TITLE_IS_STRING * Services.prompt.BUTTON_POS_0 +
        Services.prompt.BUTTON_TITLE_CANCEL * Services.prompt.BUTTON_POS_1,
      this.getString(openKey),
      null,
      null,
      this.getFormattedString("tabs.openWarningPromptMeBranded", [
        brandShortName,
      ]),
      warnOnOpen
    );

    let reallyOpen = buttonPressed == 0;
    // don't set the pref unless they press OK and it's false
    if (reallyOpen && !warnOnOpen.value) {
      Services.prefs.setBoolPref(WARN_ON_OPEN_PREF, false);
    }

    return reallyOpen;
  },

  /*
   * Async version of confirmOpenInTabs.
   */
  promiseConfirmOpenInTabs(numTabsToOpen, aWindow) {
    return new Promise(resolve => {
      Services.tm.dispatchToMainThread(() => {
        resolve(this.confirmOpenInTabs(numTabsToOpen, aWindow));
      });
    });
  },
};
