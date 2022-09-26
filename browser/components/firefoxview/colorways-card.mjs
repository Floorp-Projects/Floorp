/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const { BuiltInThemes } = ChromeUtils.import(
  "resource:///modules/BuiltInThemes.jsm"
);
const { AddonManager } = ChromeUtils.import(
  "resource://gre/modules/AddonManager.jsm"
);
const { NimbusFeatures } = ChromeUtils.import(
  "resource://nimbus/ExperimentAPI.jsm"
);

class ColorwaysCard extends HTMLElement {
  constructor() {
    super();
    this._selectedColorwayId = null;
    this._colorwayCollectionName = "";
    this._initPromise = Promise.all([
      this._getSelectedColorway(),
      this._getLocalizedStrings(),
    ]);
  }

  onEnabled(addon) {
    if (addon.type == "theme") {
      this._selectedColorwayId = BuiltInThemes.isColorwayFromCurrentCollection(
        addon.id
      )
        ? addon.id
        : null;
      this._render();
    }
  }

  connectedCallback() {
    this.container = this.querySelector("#colorways");
    this.activeCollectionTemplate = this.querySelector(
      "#colorways-active-collection-template"
    );
    this.noCollectionTemplate = this.querySelector(
      "#colorways-no-collection-template"
    );
    const colorwaysCollection =
      NimbusFeatures.majorRelease2022.getVariable("colorwayCloset") &&
      BuiltInThemes.findActiveColorwayCollection();
    this.container.classList.toggle("no-collection", !colorwaysCollection);
    this.container.classList.toggle("content-container", colorwaysCollection);
    const template = colorwaysCollection
      ? this.activeCollectionTemplate
      : this.noCollectionTemplate;
    if (this.container.firstChild) {
      this.container.firstChild.remove();
    }
    this.container.append(document.importNode(template.content, true));
    this.button = this.querySelector("#colorways-button");
    this.collection_title = this.querySelector("#colorways-collection-title");
    this.description = this.querySelector("#colorways-collection-description");
    this.expiry = this.querySelector("#colorways-collection-expiry-date");
    this.figure = this.querySelector("#colorways-collection-figure");
    if (colorwaysCollection) {
      this.button.addEventListener("click", () => {
        const { ColorwayClosetOpener } = ChromeUtils.import(
          "resource:///modules/ColorwayClosetOpener.jsm"
        );
        ColorwayClosetOpener.openModal({
          source: "firefoxview",
        });
      });
      this._initPromise.then(() => this._render());
      AddonManager.addAddonListener(this);
      window.addEventListener("unload", () => this.cleanup());
    }
  }

  cleanup() {
    AddonManager.removeAddonListener(this);
  }

  disconnectedCallback() {
    this.cleanup();
  }

  async _getSelectedColorway() {
    await BuiltInThemes.ensureBuiltInThemes();
    this._selectedColorwayId =
      (await AddonManager.getAddonsByTypes(["theme"])).find(
        theme =>
          theme.isActive &&
          BuiltInThemes.isColorwayFromCurrentCollection(theme.id)
      )?.id || null;
  }

  async _getLocalizedStrings() {
    let l10nIds = [
      "colorway-intensity-soft",
      "colorway-intensity-balanced",
      "colorway-intensity-bold",
    ];
    const collection = BuiltInThemes.findActiveColorwayCollection();
    if (collection) {
      l10nIds.push(collection.l10nId.title);
    }
    let l10nValues = await document.l10n.formatValues(l10nIds);
    if (collection) {
      this._colorwayCollectionName = l10nValues.pop();
    }
    this._intensityL10nValue = new Map(
      l10nValues.map((string, index) => [l10nIds[index], string])
    );
  }

  _render() {
    this._showData(this._getData());
  }

  _getData() {
    let collection = BuiltInThemes.findActiveColorwayCollection();
    if (!collection) {
      return {};
    }
    let colorway = null;
    if (this._selectedColorwayId) {
      colorway = {
        name: BuiltInThemes.getLocalizedColorwayGroupName(
          this._selectedColorwayId
        ),
        figureUrl: BuiltInThemes.builtInThemeMap.get(this._selectedColorwayId)
          .figureUrl,
        intensity: this._intensityL10nValue.get(
          BuiltInThemes.getColorwayIntensityL10nId(this._selectedColorwayId)
        ),
      };
    }
    return {
      collection,
      colorway,
      figureUrl: colorway?.figureUrl || collection.figureUrl,
    };
  }

  _showData({ collection, colorway, figureUrl }) {
    if (colorway) {
      this.expiry.hidden = true;
      this.collection_title.removeAttribute("data-l10n-id");
      this.collection_title.textContent = colorway.name;
      if (colorway.intensity) {
        document.l10n.setAttributes(
          this.description,
          "firefoxview-colorway-description",
          {
            intensity: colorway.intensity,
            collection: this._colorwayCollectionName,
          }
        );
      } else {
        document.l10n.setAttributes(this.description, collection.l10nId.title);
      }
      document.l10n.setAttributes(
        this.button,
        "firefoxview-change-colorway-button"
      );
    } else {
      this.expiry.hidden = false;
      document.l10n.setAttributes(
        this.expiry.firstElementChild,
        "colorway-collection-expiry-label",
        {
          expiryDate: collection.expiry.getTime(),
        }
      );
      if (collection.l10nId.description) {
        document.l10n.setAttributes(
          this.description,
          collection.l10nId.description
        );
      }
      document.l10n.setAttributes(
        this.collection_title,
        collection.l10nId.title
      );
      document.l10n.setAttributes(
        this.button,
        "firefoxview-try-colorways-button"
      );
    }
    this.figure.src = figureUrl || "";
  }
}

customElements.define("colorways-card", ColorwaysCard);
