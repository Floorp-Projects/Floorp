/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

this.EXPORTED_SYMBOLS = ["PermissionPrompts"];

const {classes: Cc, interfaces: Ci, utils: Cu} = Components;

Cu.import("resource://gre/modules/Services.jsm");
Cu.import("resource://gre/modules/Task.jsm");
Cu.import("resource:///modules/E10SUtils.jsm");
Cu.import("resource://testing-common/ContentTask.jsm");
Cu.import("resource://testing-common/BrowserTestUtils.jsm");

const URL = "https://test1.example.com/extensions/mozscreenshots/browser/chrome/mozscreenshots/lib/permissionPrompts.html";
let lastTab = null;

this.PermissionPrompts = {
  init(libDir) {
    Services.prefs.setBoolPref("media.navigator.permission.fake", true);
    Services.prefs.setBoolPref("media.getusermedia.screensharing.allow_on_old_platforms", true);
    Services.prefs.setCharPref("media.getusermedia.screensharing.allowed_domains",
                               "test1.example.com");
    Services.prefs.setBoolPref("extensions.install.requireBuiltInCerts", false);
  },

  configurations: {
    shareDevices: {
      applyConfig: Task.async(function*() {
        yield closeLastTab();
        yield clickOn("#webRTC-shareDevices");
      }),
    },

    shareMicrophone: {
      applyConfig: Task.async(function*() {
        yield closeLastTab();
        yield clickOn("#webRTC-shareMicrophone");
      }),
    },

    shareVideoAndMicrophone: {
      applyConfig: Task.async(function*() {
        yield closeLastTab();
        yield clickOn("#webRTC-shareDevices2");
      }),
    },

    shareScreen: {
      applyConfig: Task.async(function*() {
        yield closeLastTab();
        yield clickOn("#webRTC-shareScreen");
      }),
    },

    geo: {
      applyConfig: Task.async(function*() {
        yield closeLastTab();
        yield clickOn("#geo");
      }),
    },

    loginCapture: {
      applyConfig: Task.async(function*() {
        yield closeLastTab();
        yield clickOn("#login-capture", URL);
      }),
    },

    notifications: {
      applyConfig: Task.async(function*() {
        yield closeLastTab();
        yield clickOn("#web-notifications", URL);
      }),
    },

    addons: {
      applyConfig: Task.async(function*() {
        Services.prefs.setBoolPref("xpinstall.whitelist.required", true);

        yield closeLastTab();
        yield clickOn("#addons", URL);
      }),
    },

    addonsNoWhitelist: {
      applyConfig: Task.async(function*() {
        Services.prefs.setBoolPref("xpinstall.whitelist.required", false);

        let browserWindow = Services.wm.getMostRecentWindow("navigator:browser");
        let notification = browserWindow.document.getElementById("addon-install-confirmation-notification");

        yield closeLastTab();
        yield clickOn("#addons", URL);

        // We want to skip the progress-notification, so we wait for
        // the install-confirmation screen to be "not hidden" = shown.
        yield BrowserTestUtils.waitForCondition(() => !notification.hasAttribute("hidden"),
                                                "addon install confirmation did not show", 200);
      }),
    },
  },
};

function* closeLastTab(selector) {
  if (!lastTab) {
    return;
  }
  let browserWindow = Services.wm.getMostRecentWindow("navigator:browser");
  yield BrowserTestUtils.removeTab(lastTab);
  lastTab = null;
}

function* clickOn(selector) {
  let browserWindow = Services.wm.getMostRecentWindow("navigator:browser");

  // Save the tab so we can close it later.
  lastTab = yield BrowserTestUtils.openNewForegroundTab(browserWindow.gBrowser, URL);

  yield ContentTask.spawn(lastTab.linkedBrowser, selector, function* (arg) {
    E10SUtils.wrapHandlingUserInput(content, true, function() {
      let element = content.document.querySelector(arg);
      element.click();
    });
  });

  // Wait for the popup to actually be shown before making the screenshot
  yield BrowserTestUtils.waitForEvent(browserWindow.PopupNotifications.panel, "popupshown");
}
