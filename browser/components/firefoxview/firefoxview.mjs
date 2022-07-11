/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

import { tabsSetupFlowManager } from "./tabs-pickup.mjs";

const { XPCOMUtils } = ChromeUtils.import(
  "resource://gre/modules/XPCOMUtils.jsm"
);
const { BuiltInThemes } = ChromeUtils.import(
  "resource:///modules/BuiltInThemes.jsm"
);

let lazy = {};
XPCOMUtils.defineLazyModuleGetters(lazy, {
  ColorwayClosetOpener: "resource:///modules/ColorwayClosetOpener.jsm",
  AddonManager: "resource://gre/modules/AddonManager.jsm",
});

async function getColorway() {
  const collection = BuiltInThemes.findActiveColorwayCollection();
  if (!collection) {
    return {};
  }
  await BuiltInThemes.ensureBuiltInThemes();
  let colorway = null;
  let l10nIds = [collection.l10nId.title];
  const colorwayId = (await lazy.AddonManager.getAddonsByTypes(["theme"])).find(
    theme => theme.isActive && BuiltInThemes.isMonochromaticTheme(theme.id)
  )?.id;
  if (colorwayId) {
    l10nIds.push(BuiltInThemes.getColorwayIntensityL10nId(colorwayId));
    colorway = {
      name: BuiltInThemes.getLocalizedColorwayGroupName(colorwayId),
      figureUrl: BuiltInThemes.builtInThemeMap.get(colorwayId).figureUrl,
    };
  }
  const l10nValues = await document.l10n.formatValues(l10nIds);
  collection.name = l10nValues[0];
  if (colorway) {
    colorway.intensity = l10nValues[1];
  }
  return {
    collection,
    colorway,
    figureUrl: colorway?.figureUrl || collection.figureUrl,
  };
}

function showColorway({ collection, colorway, figureUrl }) {
  const el = {
    button: document.getElementById("colorways-button"),
    title: document.getElementById("colorways-collection-title"),
    description: document.getElementById("colorways-collection-description"),
    expiry: document.querySelector("#colorways-collection-expiry-date > span"),
  };
  el.button.addEventListener("click", () => {
    lazy.ColorwayClosetOpener.openModal();
  });
  document.l10n.setAttributes(
    el.expiry,
    "colorway-collection-expiry-date-span",
    {
      expiryDate: collection.expiry.getTime(),
    }
  );
  if (colorway) {
    el.title.textContent = colorway.name;
    if (colorway.intensity) {
      document.l10n.setAttributes(
        el.description,
        "firefoxview-colorway-description",
        {
          intensity: colorway.intensity,
          collection: collection.name,
        }
      );
    } else {
      document.l10n.setAttributes(el.description, collection.l10nId.title);
    }
    document.l10n.setAttributes(
      el.button,
      "firefoxview-change-colorway-button"
    );
  } else {
    if (collection.l10nId.description) {
      document.l10n.setAttributes(
        el.description,
        collection.l10nId.description
      );
    }
    document.l10n.setAttributes(el.title, collection.l10nId.title);
    document.l10n.setAttributes(el.button, "firefoxview-try-colorways-button");
  }
  document.getElementById("colorways-collection-graphic").src = figureUrl || "";
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
