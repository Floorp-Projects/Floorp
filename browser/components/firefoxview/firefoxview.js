/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

import { tabsSetupFlowManager } from "./tabs-pickup.js";
import "./recently-closed-tabs.js";

const { XPCOMUtils } = ChromeUtils.import(
  "resource://gre/modules/XPCOMUtils.jsm"
);
const { BuiltInThemes } = ChromeUtils.import(
  "resource:///modules/BuiltInThemes.jsm"
);

XPCOMUtils.defineLazyModuleGetters(globalThis, {
  ColorwayClosetOpener: "resource:///modules/ColorwayClosetOpener.jsm",
});

window.addEventListener("load", () => {
  document.getElementById("colorways-button").addEventListener("click", () => {
    globalThis.ColorwayClosetOpener.openModal();
  });
  tabsSetupFlowManager.initialize(
    document.getElementById("tabs-pickup-container")
  );
  document.getElementById("recently-closed-tabs-container").onLoad();
  const noColorwayCollection =
    BuiltInThemes.findActiveColorwayCollection() === null;
  document
    .getElementById("colorways")
    .classList.toggle("no-collection", noColorwayCollection);
});

window.addEventListener("unload", () => {
  tabsSetupFlowManager?.uninit();
  document.getElementById("recently-closed-tabs-container").cleanup();
});
