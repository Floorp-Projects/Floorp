/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

import { tabsSetupFlowManager } from "./tabs-pickup.js";

const { XPCOMUtils } = ChromeUtils.import(
  "resource://gre/modules/XPCOMUtils.jsm"
);

XPCOMUtils.defineLazyModuleGetters(globalThis, {
  ColorwayClosetOpener: "resource:///modules/ColorwayClosetOpener.jsm",
});

window.addEventListener("load", () => {
  document
    .getElementById("colorway-closet-button")
    .addEventListener("click", () => {
      globalThis.ColorwayClosetOpener.openModal();
    });
  tabsSetupFlowManager.initialize(
    document.getElementById("tabs-pickup-container")
  );
});
