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

const INTENSITY_SOFT = "soft";
const INTENSITY_BALANCED = "balanced";
const INTENSITY_BOLD = "bold";
const ID_SUFFIX_COLORWAY = "-colorway@mozilla.org";
const ID_SUFFIX_PRIMARY_INTENSITY = `-${INTENSITY_BALANCED}${ID_SUFFIX_COLORWAY}`;
const ID_SUFFIX_DARK_COLORWAY = `-${INTENSITY_BOLD}${ID_SUFFIX_COLORWAY}`;
const ID_SUFFIXES_FOR_SECONDARY_INTENSITIES = new RegExp(
  `-(${INTENSITY_SOFT}|${INTENSITY_BOLD})-colorway@mozilla\\.org$`
);
const MATCH_INTENSITY_FROM_ID = new RegExp(
  `-(${INTENSITY_SOFT}|${INTENSITY_BALANCED}|${INTENSITY_BOLD})-colorway@mozilla\\.org$`
);

/**
 * Helper for colorway closet related telemetry probes.
 */
const ColorwaysTelemetry = {
  init() {
    Services.telemetry.setEventRecordingEnabled("colorways_modal", true);
  },

  recordEvent(telemetryEventData) {
    if (!telemetryEventData || !Object.entries(telemetryEventData).length) {
      console.error("Unable to record event due to missing telemetry data");
      return;
    }

    if (telemetryEventData.source && ColorwayCloset.previousTheme?.id) {
      telemetryEventData.method = BuiltInThemes.isColorwayFromCurrentCollection(
        ColorwayCloset.previousTheme.id
      )
        ? "change_colorway"
        : "try_colorways";
      telemetryEventData.object = telemetryEventData.source;
    }
    Services.telemetry.recordEvent(
      "colorways_modal",
      telemetryEventData.method,
      telemetryEventData.object,
      telemetryEventData.value || null,
      telemetryEventData.extraKeys || null
    );
  },
};

const ColorwayCloset = {
  // This is essentially an instant-apply dialog, but we make an effort to only
  // keep the theme change if the user hits the "Set colorway" button, and
  // otherwise revert to the previous theme upon closing the modal. However,
  // this doesn't cover the application quitting while the modal is open, in
  // which case the theme change will be kept.
  revertToPreviousTheme: true,

  el: {
    colorwayRadios: document.getElementById("colorway-selector"),
    intensityContainer: document.getElementById("colorway-intensities"),
    colorwayFigure: document.getElementById("colorway-figure"),
    colorwayName: document.getElementById("colorway-name"),
    collectionTitle: document.getElementById("collection-title"),
    colorwayDescription: document.getElementById("colorway-description"),
    expiryDateSpan: document.querySelector("#collection-expiry-date > span"),
    setColorwayButton: document.getElementById("set-colorway"),
    cancelButton: document.getElementById("cancel"),
    homepageResetContainer: document.getElementById("homepage-reset-container"),
  },

  init() {
    window.addEventListener("unload", this);

    ColorwaysTelemetry.init();

    this._displayCollectionData();

    AddonManager.addAddonListener(this);
    this._initColorwayRadios().then(() => {
      this.el.colorwayRadios.addEventListener("change", this);
      this.el.intensityContainer.addEventListener("change", this);

      let args = window?.arguments?.[0];
      // Record telemetry probes that are called upon opening the modal.
      ColorwaysTelemetry.recordEvent(args);

      this.el.setColorwayButton.onclick = () => {
        this.revertToPreviousTheme = false;
        window.close();
      };
    });

    this.el.cancelButton.onclick = () => {
      window.close();
    };

    this._displayHomepageResetOption();
  },

  async _initColorwayRadios() {
    BuiltInThemes.ensureBuiltInThemes();
    let themes = await AddonManager.getAddonsByTypes(["theme"]);
    this.previousTheme = themes.find(theme => theme.isActive);
    this.colorways = themes.filter(theme =>
      BuiltInThemes.isColorwayFromCurrentCollection(theme.id)
    );

    // The radio buttons represent colorway "groups". A group is a colorway
    // from the current collection to represent related colorways with another
    // intensity. If the current collection doesn't have intensities, each
    // colorway is their own group.
    this.colorwayGroups = this.colorways.filter(
      colorway => !ID_SUFFIXES_FOR_SECONDARY_INTENSITIES.test(colorway.id)
    );

    for (const addon of this.colorwayGroups) {
      let input = document.createElement("input");
      input.type = "radio";
      input.name = "colorway";
      input.value = addon.id;
      input.setAttribute("title", this._getColorwayGroupName(addon));
      if (addon.iconURL) {
        input.style.setProperty("--colorway-icon", `url(${addon.iconURL})`);
      }
      this.el.colorwayRadios.appendChild(input);
    }

    // If the current active theme is part of our collection, make the UI reflect
    // that. Otherwise go ahead and enable the first colorway in our list.
    this.selectedColorway = this.colorways.find(colorway => colorway.isActive);
    if (this.selectedColorway) {
      this.refresh();
    } else {
      let colorwayToEnable = this.colorwayGroups[0];
      // If the user has been using a theme with a dark color scheme, make an
      // effort to default to a colorway with a dark color scheme as well.
      if (window.matchMedia("(prefers-color-scheme: dark)").matches) {
        let firstDarkColorway = this.colorways.find(colorway =>
          colorway.id.endsWith(ID_SUFFIX_DARK_COLORWAY)
        );
        colorwayToEnable = firstDarkColorway || colorwayToEnable;
      }
      colorwayToEnable.enable();
    }
  },

  _displayHomepageResetOption() {
    const { HomePage } = ChromeUtils.import("resource:///modules/HomePage.jsm");
    this.el.homepageResetContainer.hidden = HomePage.isDefault;
    if (!HomePage.isDefault) {
      let homeState;
      this.el.homepageResetContainer
        .querySelector("#homepage-reset-prompt > button")
        .addEventListener("click", () => {
          ColorwaysTelemetry.recordEvent({
            method: "homepage_reset",
            object: "modal",
          });
          homeState = HomePage.get();
          HomePage.reset();
          this.el.homepageResetContainer.classList.add("success");
        });
      this.el.homepageResetContainer
        .querySelector("#homepage-reset-success > button")
        .addEventListener("click", () => {
          ColorwaysTelemetry.recordEvent({
            method: "homepage_reset_undo",
            object: "modal",
          });
          HomePage.set(homeState);
          this.el.homepageResetContainer.classList.remove("success");
        });
    }
  },

  _displayCollectionData() {
    const collection = BuiltInThemes.findActiveColorwayCollection();
    if (!collection) {
      // Whoops. There should be no entry point to this UI without an active
      // collection.
      window.close();
    }
    document.l10n.setAttributes(
      this.el.collectionTitle,
      collection.l10nId.title
    );
    document.l10n.setAttributes(
      this.el.expiryDateSpan,
      "colorway-collection-expiry-label",
      {
        expiryDate: collection.expiry.getTime(),
      }
    );
  },

  _getFigureUrl() {
    return BuiltInThemes.builtInThemeMap.get(this.selectedColorway.id)
      .figureUrl;
  },

  _displayColorwayData() {
    this.el.colorwayName.innerText = this._getColorwayGroupName(
      this.selectedColorway
    );
    this.el.colorwayDescription.innerText = this.selectedColorway.description;
    this.el.colorwayFigure.src = this._getFigureUrl();

    this.el.intensityContainer.hidden = !this.hasIntensities;
    if (this.hasIntensities) {
      let selectedIntensity = this.selectedColorway.id.match(
        MATCH_INTENSITY_FROM_ID
      )[1];
      for (let radio of document.querySelectorAll(
        ".colorway-intensity-radio"
      )) {
        let intensity = radio.getAttribute("data-intensity");
        radio.value = this._changeIntensity(
          this.selectedColorway.id,
          intensity
        );
        if (intensity == selectedIntensity) {
          radio.checked = true;
        }
      }
    }
  },

  _getColorwayGroupId(colorwayId) {
    let groupId = colorwayId.replace(
      ID_SUFFIXES_FOR_SECONDARY_INTENSITIES,
      ID_SUFFIX_PRIMARY_INTENSITY
    );
    return this.colorwayGroups.map(addon => addon.id).includes(groupId)
      ? groupId
      : null;
  },

  _changeIntensity(colorwayId, intensity) {
    return colorwayId.replace(
      MATCH_INTENSITY_FROM_ID,
      `-${intensity}${ID_SUFFIX_COLORWAY}`
    );
  },

  _getColorwayGroupName(addon) {
    return BuiltInThemes.getLocalizedColorwayGroupName(addon.id) || addon.name;
  },

  handleEvent(e) {
    switch (e.type) {
      case "change":
        let newId = e.target.value;
        // Persist the selected intensity when toggling between colorway radios.
        if (e.currentTarget == this.el.colorwayRadios && this.hasIntensities) {
          let selectedIntensity = document
            .querySelector(".colorway-intensity-radio:checked")
            .getAttribute("data-intensity");
          newId = this._changeIntensity(newId, selectedIntensity);
        }
        this.colorways.find(colorway => colorway.id == newId).enable();
        break;
      case "unload":
        AddonManager.removeAddonListener(this);
        if (this.revertToPreviousTheme) {
          this.previousTheme.enable();
          ColorwaysTelemetry.recordEvent({
            method: "cancel",
            object: "modal",
          });
        } else {
          ColorwaysTelemetry.recordEvent({
            method: "set_colorway",
            object: "modal",
            value: null,
            extraKeys: { colorway_id: this.selectedColorway.id },
          });
        }
        break;
    }
  },

  onEnabled(addon) {
    if (addon.type == "theme") {
      if (!this.colorways.find(colorway => colorway.id == addon.id)) {
        // The selected theme changed to a non-colorway from outside the modal.
        // This UI can't represent that state, so bail out.
        this.revertToPreviousTheme = false;
        window.close();
        return;
      }
      this.selectedColorway = addon;
      this.refresh();
    }
  },

  refresh() {
    this.groupIdForSelectedColorway = this._getColorwayGroupId(
      this.selectedColorway.id
    );
    this.hasIntensities = this.groupIdForSelectedColorway.endsWith(
      ID_SUFFIX_PRIMARY_INTENSITY
    );
    for (let input of this.el.colorwayRadios.children) {
      if (input.value == this.groupIdForSelectedColorway) {
        input.checked = true;
        this._displayColorwayData();
        break;
      }
    }
    this.el.setColorwayButton.disabled =
      this.previousTheme.id == this.selectedColorway.id;
  },
};

ColorwayCloset.init();
