/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

this.EXPORTED_SYMBOLS = ["PermissionPrompts"];

const {classes: Cc, interfaces: Ci, utils: Cu} = Components;

Cu.import("resource://gre/modules/Services.jsm");
Cu.import("resource:///modules/E10SUtils.jsm");
Cu.import("resource://testing-common/ContentTask.jsm");
Cu.import("resource://testing-common/BrowserTestUtils.jsm");

const URL = "https://test1.example.com/browser/browser/tools/mozscreenshots/mozscreenshots/extension/mozscreenshots/browser/chrome/mozscreenshots/lib/permissionPrompts.html";
let lastTab = null;

this.PermissionPrompts = {
  init(libDir) {
    Services.prefs.setBoolPref("browser.storageManager.enabled", true);
    Services.prefs.setBoolPref("media.navigator.permission.fake", true);
    Services.prefs.setCharPref("media.getusermedia.screensharing.allowed_domains",
                               "test1.example.com");
    Services.prefs.setBoolPref("extensions.install.requireBuiltInCerts", false);
    Services.prefs.setBoolPref("signon.rememberSignons", true);
  },

  configurations: {
    shareDevices: {
      async applyConfig() {
        await closeLastTab();
        await clickOn("#webRTC-shareDevices");
      },
    },

    shareMicrophone: {
      async applyConfig() {
        await closeLastTab();
        await clickOn("#webRTC-shareMicrophone");
      },
    },

    shareVideoAndMicrophone: {
      async applyConfig() {
        await closeLastTab();
        await clickOn("#webRTC-shareDevices2");
      },
    },

    shareScreen: {
      async applyConfig() {
        await closeLastTab();
        await clickOn("#webRTC-shareScreen");
      },
    },

    geo: {
      async applyConfig() {
        await closeLastTab();
        await clickOn("#geo");
      },
    },

    persistentStorage: {
      async applyConfig() {
        await closeLastTab();
        await clickOn("#persistent-storage");
      },
    },

    loginCapture: {
      async applyConfig() {
        await closeLastTab();
        await clickOn("#login-capture");
      },
    },

    notifications: {
      async applyConfig() {
        await closeLastTab();
        await clickOn("#web-notifications");
      },
    },

    addons: {
      async applyConfig() {
        Services.prefs.setBoolPref("xpinstall.whitelist.required", true);

        await closeLastTab();
        await clickOn("#addons");
      },
    },

    addonsNoWhitelist: {
      async applyConfig() {
        Services.prefs.setBoolPref("xpinstall.whitelist.required", false);

        let browserWindow = Services.wm.getMostRecentWindow("navigator:browser");
        let notification = browserWindow.document.getElementById("addon-install-confirmation-notification");

        await closeLastTab();
        await clickOn("#addons");

        // We want to skip the progress-notification, so we wait for
        // the install-confirmation screen to be "not hidden" = shown.
        await BrowserTestUtils.waitForCondition(() => !notification.hasAttribute("hidden"),
                                                "addon install confirmation did not show", 200);
      },
    },
  },
};

async function closeLastTab() {
  if (!lastTab) {
    return;
  }
  await BrowserTestUtils.removeTab(lastTab);
  lastTab = null;
}

async function clickOn(selector) {
  let browserWindow = Services.wm.getMostRecentWindow("navigator:browser");

  // Save the tab so we can close it later.
  lastTab = await BrowserTestUtils.openNewForegroundTab(browserWindow.gBrowser, URL);

  await ContentTask.spawn(lastTab.linkedBrowser, selector, async function(arg) {
    E10SUtils.wrapHandlingUserInput(content, true, function() {
      let element = content.document.querySelector(arg);
      element.click();
    });
  });

  // Wait for the popup to actually be shown before making the screenshot
  await BrowserTestUtils.waitForEvent(browserWindow.PopupNotifications.panel, "popupshown");
}
