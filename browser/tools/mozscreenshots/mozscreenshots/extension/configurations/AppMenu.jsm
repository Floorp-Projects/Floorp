/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

this.EXPORTED_SYMBOLS = ["AppMenu"];

const {classes: Cc, interfaces: Ci, utils: Cu} = Components;

Cu.import("resource://gre/modules/Services.jsm");

this.AppMenu = {

  init(libDir) {},

  configurations: {
    appMenuClosed: {
      selectors: ["#appMenu-popup"],
      async applyConfig() {
        let browserWindow = Services.wm.getMostRecentWindow("navigator:browser");
        browserWindow.PanelUI.hide();
      },
    },

    appMenuMainView: {
      selectors: ["#appMenu-popup"],
      applyConfig() {
        let browserWindow = Services.wm.getMostRecentWindow("navigator:browser");
        let promise = browserWindow.PanelUI.show();
        browserWindow.PanelUI.showMainView();
        return promise;
      },
    },

    appMenuHistorySubview: {
      selectors: ["#appMenu-popup"],
      async applyConfig() {
        // History has a footer
        if (isCustomizing()) {
          return "Can't show subviews while customizing";
        }
        let browserWindow = Services.wm.getMostRecentWindow("navigator:browser");
        await browserWindow.PanelUI.show();
        browserWindow.PanelUI.showMainView();
        browserWindow.document.getElementById("history-panelmenu").click();

        return undefined;
      },

      verifyConfig: verifyConfigHelper,
    },

    appMenuHelpSubview: {
      selectors: ["#appMenu-popup"],
      async applyConfig() {
        if (isCustomizing()) {
          return "Can't show subviews while customizing";
        }
        let browserWindow = Services.wm.getMostRecentWindow("navigator:browser");
        await browserWindow.PanelUI.show();
        browserWindow.PanelUI.showMainView();
        browserWindow.document.getElementById("PanelUI-help").click();

        return undefined;
      },

      verifyConfig: verifyConfigHelper,
    },

  },
};

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
