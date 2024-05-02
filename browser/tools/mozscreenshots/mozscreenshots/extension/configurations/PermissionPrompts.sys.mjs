/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

// Various parts here are run in the content process.
/* global content */

import { BrowserTestUtils } from "resource://testing-common/BrowserTestUtils.sys.mjs";

const URL =
  "https://test1.example.com/browser/browser/tools/mozscreenshots/mozscreenshots/extension/mozscreenshots/browser/resources/lib/permissionPrompts.html";
let lastTab = null;

export var PermissionPrompts = {
  init() {
    Services.prefs.setBoolPref("media.navigator.permission.fake", true);
    Services.prefs.setBoolPref("extensions.install.requireBuiltInCerts", false);
    Services.prefs.setBoolPref("signon.rememberSignons", true);
  },

  configurations: {
    shareDevices: {
      selectors: ["#notification-popup", "#identity-box"],
      async applyConfig() {
        await closeLastTab();
        await clickOn("#webRTC-shareDevices");
      },
    },

    shareMicrophone: {
      selectors: ["#notification-popup", "#identity-box"],
      async applyConfig() {
        await closeLastTab();
        await clickOn("#webRTC-shareMicrophone");
      },
    },

    shareVideoAndMicrophone: {
      selectors: ["#notification-popup", "#identity-box"],
      async applyConfig() {
        await closeLastTab();
        await clickOn("#webRTC-shareDevices2");
      },
    },

    shareScreen: {
      selectors: ["#notification-popup", "#identity-box"],
      async applyConfig() {
        await closeLastTab();
        await clickOn("#webRTC-shareScreen");
      },
    },

    geo: {
      selectors: ["#notification-popup", "#identity-box"],
      async applyConfig() {
        await closeLastTab();
        await clickOn("#geo");
      },
    },

    persistentStorage: {
      selectors: ["#notification-popup", "#identity-box"],
      async applyConfig() {
        await closeLastTab();
        await clickOn("#persistent-storage");
      },
    },

    loginCapture: {
      selectors: ["#notification-popup", "#identity-box"],
      async applyConfig() {
        await closeLastTab();
        // we need to emulate user input in the form for the save-password prompt to be shown
        await clickOn("#login-capture", function beforeContentFn() {
          const { E10SUtils } = ChromeUtils.importESModule(
            "resource://gre/modules/E10SUtils.sys.mjs"
          );
          E10SUtils.wrapHandlingUserInput(content, true, function () {
            let element = content.document.querySelector(
              "input[type=password]"
            );
            element.setUserInput("123456");
          });
        });
      },
    },

    notifications: {
      selectors: ["#notification-popup", "#identity-box"],
      async applyConfig() {
        await closeLastTab();
        await clickOn("#web-notifications");
      },
    },

    addons: {
      selectors: ["#notification-popup", "#identity-box"],
      async applyConfig() {
        Services.prefs.setBoolPref("xpinstall.whitelist.required", true);

        await closeLastTab();
        await clickOn("#addons");
      },
    },

    addonsNoWhitelist: {
      selectors: ["#notification-popup", "#identity-box"],
      async applyConfig() {
        Services.prefs.setBoolPref("xpinstall.whitelist.required", false);

        let browserWindow =
          Services.wm.getMostRecentWindow("navigator:browser");
        let notification = browserWindow.document.getElementById(
          "addon-webext-permissions-notification"
        );

        await closeLastTab();
        await clickOn("#addons");

        // We want to skip the progress-notification, so we wait for
        // the install-confirmation screen to be "not hidden" = shown.
        return BrowserTestUtils.waitForCondition(
          () => !notification.hidden,
          "addon install confirmation did not show",
          200
        ).catch(msg => {
          return Promise.resolve({ todo: msg });
        });
      },
    },
  },
};

async function closeLastTab() {
  if (!lastTab) {
    return;
  }
  BrowserTestUtils.removeTab(lastTab);
  lastTab = null;
}

async function clickOn(selector, beforeContentFn) {
  let browserWindow = Services.wm.getMostRecentWindow("navigator:browser");

  // Save the tab so we can close it later.
  lastTab = await BrowserTestUtils.openNewForegroundTab(
    browserWindow.gBrowser,
    URL
  );

  let { SpecialPowers } = lastTab.ownerGlobal;
  if (beforeContentFn) {
    await SpecialPowers.spawn(lastTab.linkedBrowser, [], beforeContentFn);
  }

  let popupShownPromise = BrowserTestUtils.waitForEvent(
    browserWindow.PopupNotifications.panel,
    "popupshown"
  );

  await SpecialPowers.spawn(lastTab.linkedBrowser, [selector], arg => {
    /* eslint-env mozilla/chrome-script */
    let element = content.document.querySelector(arg);
    return EventUtils.synthesizeClick(element);
  });

  // Wait for the popup to actually be shown before making the screenshot
  await popupShownPromise;
}
