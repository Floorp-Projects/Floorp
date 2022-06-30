/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

var EXPORTED_SYMBOLS = ["ColorwayClosetOpener"];

const { BrowserWindowTracker } = ChromeUtils.import(
  "resource:///modules/BrowserWindowTracker.jsm"
);

let ColorwayClosetOpener = {
  openModal: () => {
    let win = BrowserWindowTracker.getTopWindow();
    let dialogBox = win.gBrowser.getTabDialogBox(win.gBrowser.selectedBrowser);
    return dialogBox.open("chrome://browser/content/colorwaycloset.html", {
      features: "resizable=no",
      sizeTo: "available",
    });
  },
};
