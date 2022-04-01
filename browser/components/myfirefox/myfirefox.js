/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const { XPCOMUtils } = ChromeUtils.import(
  "resource://gre/modules/XPCOMUtils.jsm"
);

XPCOMUtils.defineLazyModuleGetters(this, {
  ColorwayClosetOpener: "resource:///modules/ColorwayClosetOpener.jsm",
});

window.addEventListener("load", () => {
  document
    .getElementById("colorway-closet-button")
    .addEventListener("click", () => {
      ColorwayClosetOpener.openModal();
    });
});
