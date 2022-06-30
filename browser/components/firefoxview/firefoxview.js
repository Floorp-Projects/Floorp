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
  AddonManager: "resource://gre/modules/AddonManager.jsm",
});

function titleCase(str) {
  if (str) {
    return str[0].toUpperCase() + str.slice(1);
  }
  return undefined;
}

async function getColorway() {
  const colorwaysCollection = BuiltInThemes.findActiveColorwayCollection();
  if (!colorwaysCollection) {
    return {};
  }
  BuiltInThemes.ensureBuiltInThemes();
  let colorwayProperties = {};
  const colorway = (
    await globalThis.AddonManager.getAddonsByTypes(["theme"])
  ).find(
    theme => theme.isActive && BuiltInThemes.isMonochromaticTheme(theme.id)
  );
  if (colorway) {
    // The colorway.id has the format
    // `{colorName}-{intensitytName}-colorway@mozilla.org`
    // or `{colorName}-colorway@mozilla.org` where intensitytName is optional
    // Bug 1776682 will localize these strings
    const re = /^([a-z\.]+)(-(soft|balanced|bold))?-colorway@mozilla.org$/;
    let [, colorwayName, , intensity] = re.exec(colorway.id);
    // Apply title casing until Bug 1770030 is completed
    colorwayProperties = {
      colorwayName: titleCase(colorwayName),
      intensity: titleCase(intensity),
    };
  }
  const collectionName = await document.l10n.formatValue(
    colorwaysCollection.l10nId.title
  );
  return {
    ...colorwayProperties,
    ...colorwaysCollection,
    collectionName,
  };
}

function showColorway({
  colorwayName,
  intensity,
  collectionName,
  expiry,
  l10nId,
  iconUrl,
}) {
  const el = {
    button: document.getElementById("colorways-button"),
    title: document.getElementById("colorways-collection-title"),
    description: document.getElementById("colorways-collection-description"),
    expiry: document.querySelector("#colorways-collection-expiry-date > span"),
  };
  el.button.addEventListener("click", () => {
    globalThis.ColorwayClosetOpener.openModal();
  });
  document.l10n.setAttributes(
    el.expiry,
    "colorway-collection-expiry-date-span",
    {
      expiryDate: expiry.getTime(),
    }
  );
  if (colorwayName) {
    el.title.textContent = colorwayName;
    if (intensity) {
      document.l10n.setAttributes(
        el.description,
        "firefoxview-colorway-description",
        {
          intensity,
          collection: collectionName,
        }
      );
    } else {
      document.l10n.setAttributes(el.description, l10nId.title);
    }
    document.l10n.setAttributes(
      el.button,
      "firefoxview-change-colorway-button"
    );
  } else {
    if (l10nId.description) {
      document.l10n.setAttributes(el.description, l10nId.description);
    }
    document.l10n.setAttributes(el.title, l10nId.title);
    document.l10n.setAttributes(el.button, "firefoxview-try-colorways-button");
  }
  document.getElementById("colorways-collection-graphic").src = iconUrl || "";
}

const getColorwayPromise = getColorway();

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
    getColorwayPromise.then(showColorway);
  }
});

window.addEventListener("unload", () => {
  tabsSetupFlowManager?.uninit();
  const tabPickupList = document.querySelector("tab-pickup-list");
  if (tabPickupList) {
    tabPickupList.cleanup();
  }
  document.getElementById("recently-closed-tabs-container").cleanup();
});
