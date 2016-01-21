/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

this.EXPORTED_SYMBOLS = ["AppMenu"];

const {classes: Cc, interfaces: Ci, utils: Cu} = Components;

Cu.import("resource://gre/modules/Services.jsm");
Cu.import("resource://gre/modules/Task.jsm");

this.AppMenu = {

  init(libDir) {},

  configurations: {
    appMenuClosed: {
      applyConfig: Task.async(function*() {
        let browserWindow = Services.wm.getMostRecentWindow("navigator:browser");
        browserWindow.PanelUI.hide();
      }),
    },

    appMenuMainView: {
      applyConfig() {
        let browserWindow = Services.wm.getMostRecentWindow("navigator:browser");
        let promise = browserWindow.PanelUI.show();
        browserWindow.PanelUI.showMainView();
        return promise;
      },
    },

    appMenuHistorySubview: {
      applyConfig() {
        // History has a footer
        if (isCustomizing()) {
          return Promise.reject("Can't show subviews while customizing");
        }
        let browserWindow = Services.wm.getMostRecentWindow("navigator:browser");
        let promise = browserWindow.PanelUI.show();
        return promise.then(() => {
          browserWindow.PanelUI.showMainView();
          browserWindow.document.getElementById("history-panelmenu").click();
        });
      },

      verifyConfig: verifyConfigHelper,
    },

    appMenuHelpSubview: {
      applyConfig() {
        if (isCustomizing()) {
          return Promise.reject("Can't show subviews while customizing");
        }
        let browserWindow = Services.wm.getMostRecentWindow("navigator:browser");
        let promise = browserWindow.PanelUI.show();
        return promise.then(() => {
          browserWindow.PanelUI.showMainView();
          browserWindow.document.getElementById("PanelUI-help").click();
        });
      },

      verifyConfig: verifyConfigHelper,
    },

  },
};

function verifyConfigHelper() {
  if (isCustomizing()) {
    return Promise.reject("AppMenu verifyConfigHelper");
  }
  return Promise.resolve("AppMenu verifyConfigHelper");
}

function isCustomizing() {
  let browserWindow = Services.wm.getMostRecentWindow("navigator:browser");
  if (browserWindow.document.documentElement.hasAttribute("customizing")) {
    return true;
  }
  return false;
}
