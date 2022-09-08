/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

var EXPORTED_SYMBOLS = ["ColorwayClosetOpener"];

const { BrowserWindowTracker } = ChromeUtils.import(
  "resource:///modules/BrowserWindowTracker.jsm"
);

let ColorwayClosetOpener = {
  /**
   * Opens the colorway closet modal. "source" indicates from where the modal was opened,
   * and it is used for existing telemetry probes.
   * Valid "source" types include: "aboutaddons", "firefoxview" and "unknown" (default).
   * @See Events.yaml for existing colorway closet probes
   */
  openModal: ({ source = "unknown" } = {}) => {
    let { gBrowser } = BrowserWindowTracker.getTopWindow();
    let dialogBox = gBrowser.getTabDialogBox(gBrowser.selectedBrowser);
    return dialogBox.open(
      "chrome://browser/content/colorways/colorwaycloset.html",
      {
        features: "resizable=no",
        sizeTo: "available",
      },
      { source }
    );
  },
};
