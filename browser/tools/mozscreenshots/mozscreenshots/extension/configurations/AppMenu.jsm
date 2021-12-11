/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

var EXPORTED_SYMBOLS = ["AppMenu"];

const { Services } = ChromeUtils.import("resource://gre/modules/Services.jsm");
const { BrowserTestUtils } = ChromeUtils.import(
  "resource://testing-common/BrowserTestUtils.jsm"
);

var AppMenu = {
  init(libDir) {},

  configurations: {
    appMenuMainView: {
      selectors: ["#appMenu-popup"],
      async applyConfig() {
        let browserWindow = Services.wm.getMostRecentWindow(
          "navigator:browser"
        );
        await reopenAppMenu(browserWindow);
      },
    },

    appMenuHistorySubview: {
      selectors: ["#appMenu-popup"],
      async applyConfig() {
        let browserWindow = Services.wm.getMostRecentWindow(
          "navigator:browser"
        );
        await reopenAppMenu(browserWindow);

        browserWindow.document.getElementById("appMenu-library-button").click();
        let view = browserWindow.document.getElementById("appMenu-libraryView");
        let promiseViewShown = BrowserTestUtils.waitForEvent(view, "ViewShown");
        await promiseViewShown;
      },

      verifyConfig: verifyConfigHelper,
    },

    appMenuHelpSubview: {
      selectors: ["#appMenu-popup"],
      async applyConfig() {
        let browserWindow = Services.wm.getMostRecentWindow(
          "navigator:browser"
        );
        await reopenAppMenu(browserWindow);

        browserWindow.document.getElementById("appMenu-help-button2").click();
        let view = browserWindow.document.getElementById("PanelUI-helpView");
        let promiseViewShown = BrowserTestUtils.waitForEvent(view, "ViewShown");
        await promiseViewShown;
      },

      verifyConfig: verifyConfigHelper,
    },
  },
};

async function reopenAppMenu(browserWindow) {
  browserWindow.PanelUI.hide();
  let promiseViewShown = BrowserTestUtils.waitForEvent(
    PanelUI.panel,
    "ViewShown"
  );
  browserWindow.PanelUI.show();
  await promiseViewShown;
}

function verifyConfigHelper() {
  if (isCustomizing()) {
    return "navigator:browser has the customizing attribute";
  }
  return undefined;
}

function isCustomizing() {
  let browserWindow = Services.wm.getMostRecentWindow("navigator:browser");
  if (browserWindow.document.documentElement.hasAttribute("customizing")) {
    return true;
  }
  return false;
}
