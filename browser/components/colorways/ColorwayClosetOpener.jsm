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
    let { gBrowser } = BrowserWindowTracker.getTopWindow();
    let dialogBox = gBrowser.getTabDialogBox(gBrowser.selectedBrowser);
    let rv = dialogBox.open(
      "chrome://browser/content/colorways/colorwaycloset.html",
      {
        features: "resizable=no",
      }
    );
    let { dialog } = rv;
    dialog._dialogReady.then(() => {
      // The modal document had a width set so `SubDialog` could use it to
      // determine the initial frame size. We'll now remove that width because
      // the modal document's layout is responsive and we don't want to scroll
      // horizontally when resizing the browser window such that the frame
      // becomes narrower than its initial width.
      dialog._frame.contentDocument.documentElement.style.removeProperty(
        "width"
      );
    });
    return rv;
  },
};
