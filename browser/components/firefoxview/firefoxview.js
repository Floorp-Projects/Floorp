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
  tabsSetupFlowManager.initialize(
    document.getElementById("tabs-pickup-container")
  );
  document.getElementById("recently-closed-tabs-container").onLoad();
  const colorwaysCollection = BuiltInThemes.findActiveColorwayCollection();
  const colorwaysContainer = document.getElementById("colorways");
  colorwaysContainer.classList.toggle("no-collection", !colorwaysCollection);
  colorwaysContainer.classList.toggle("content-container", colorwaysCollection);
  const templateId = colorwaysCollection
    ? "colorways-active-collection-template"
    : "colorways-no-collection-template";
  if (colorwaysContainer.firstChild) {
    colorwaysContainer.firstChild.remove();
  }
  colorwaysContainer.append(
    document.importNode(document.getElementById(templateId).content, true)
  );
  if (colorwaysCollection) {
    const { expiry, l10nId } = colorwaysCollection;
    const colorwayButton = document.getElementById("colorways-button");
    document.l10n.setAttributes(
      colorwayButton,
      "colorway-fx-home-try-colorways-button"
    );
    colorwayButton.addEventListener("click", () => {
      globalThis.ColorwayClosetOpener.openModal();
    });
    const collectionTitle = document.getElementById(
      "colorways-collection-title"
    );
    document.l10n.setAttributes(collectionTitle, l10nId.title);
    const colorwayExpiryDateSpan = document.querySelector(
      "#colorways-collection-expiry-date > span"
    );
    document.l10n.setAttributes(
      colorwayExpiryDateSpan,
      "colorway-collection-expiry-date-span",
      {
        expiryDate: expiry.getTime(),
      }
    );
    if (l10nId.description) {
      const message = document.getElementById(
        "colorways-collection-description"
      );
      document.l10n.setAttributes(message, l10nId.description);
    }
    // TODO: Uncomment when graphic is finalized document.getElementById("colorways-collection-graphic").src = "chrome://browser/content/colorway-try-colorways.svg";
  }
});

window.addEventListener("unload", () => {
  tabsSetupFlowManager?.uninit();
  document.querySelector("tab-pickup-list").cleanup();
  document.getElementById("recently-closed-tabs-container").cleanup();
});
