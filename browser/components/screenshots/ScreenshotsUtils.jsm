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

    // init UI as a tab dialog box
    let dialogBox = gBrowser.getTabDialogBox(browser);

    Services.obs.notifyObservers(subj, "toggle-screenshot-disable", "true");

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
