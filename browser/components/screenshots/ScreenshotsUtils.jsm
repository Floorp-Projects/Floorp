/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

var EXPORTED_SYMBOLS = ["ScreenshotsUtils"];

const { Services } = ChromeUtils.import("resource://gre/modules/Services.jsm");

var ScreenshotsUtils = {
  initialize() {
    Services.obs.addObserver(this, "menuitem-screenshot");
  },
  observe(subj, topic, data) {
    let { gBrowser } = subj;
    let browser = gBrowser.selectedBrowser;

    let currDialogBox = browser.tabDialogBox;

    // if dialog box exists and if it does then close current tab dialog box
    if (currDialogBox) {
      let manager = currDialogBox.getTabDialogManager();
      let dialogs = manager.hasDialogs && manager.dialogs;
      if (dialogs.length) {
        for (let dialog of dialogs) {
          if (
            dialog._openedURL.endsWith(
              `browsingContextId=${browser.browsingContext.id}`
            ) &&
            dialog._openedURL.includes("screenshots.html")
          ) {
            dialog.close();
            return null;
          }
        }
      }
    }

    // init UI as a tab dialog box
    let dialogBox = gBrowser.getTabDialogBox(browser);

    return dialogBox.open(
      `chrome://browser/content/screenshots/screenshots.html?browsingContextId=${browser.browsingContext.id}`,
      {
        features: "resizable=no",
        sizeTo: "available",
        allowDuplicateDialogs: false,
      }
    );
  },
  notify(window, type) {
    if (Services.prefs.getBoolPref("screenshots.browser.component.enabled")) {
      Services.obs.notifyObservers(
        window.event.currentTarget.ownerGlobal,
        "menuitem-screenshot"
      );
    } else {
      Services.obs.notifyObservers(null, "menuitem-screenshot-extension", type);
    }
  },
};
