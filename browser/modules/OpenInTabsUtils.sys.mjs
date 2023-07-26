/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

const lazy = {};

ChromeUtils.defineLazyGetter(lazy, "l10n", () => {
  return new Localization(
    ["browser/tabbrowser.ftl", "branding/brand.ftl"],
    true
  );
});

/**
 * Utility functions that can be used when opening multiple tabs, that can be
 * called without any tabbrowser instance.
 */
export const OpenInTabsUtils = {
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

    const [title, message, button, checkbox] = lazy.l10n.formatMessagesSync([
      { id: "tabbrowser-confirm-open-multiple-tabs-title" },
      {
        id: "tabbrowser-confirm-open-multiple-tabs-message",
        args: { tabCount: numTabsToOpen },
      },
      { id: "tabbrowser-confirm-open-multiple-tabs-button" },
      { id: "tabbrowser-confirm-open-multiple-tabs-checkbox" },
    ]);

    let buttonPressed = Services.prompt.confirmEx(
      aWindow,
      title.value,
      message.value,
      Services.prompt.BUTTON_TITLE_IS_STRING * Services.prompt.BUTTON_POS_0 +
        Services.prompt.BUTTON_TITLE_CANCEL * Services.prompt.BUTTON_POS_1,
      button.value,
      null,
      null,
      checkbox.value,
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
