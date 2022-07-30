/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

var EXPORTED_SYMBOLS = ["BuiltInThemes"];

const { XPCOMUtils } = ChromeUtils.importESModule(
  "resource://gre/modules/XPCOMUtils.sys.mjs"
);

const lazy = {};

XPCOMUtils.defineLazyModuleGetters(lazy, {
  AddonManager: "resource://gre/modules/AddonManager.jsm",
  BuiltInThemeConfig: "resource:///modules/BuiltInThemeConfig.jsm",
});

const ColorwayL10n = new Localization(["browser/colorways.ftl"], true);

const kActiveThemePref = "extensions.activeThemeID";
const kRetainedThemesPref = "browser.theme.retainedExpiredThemes";

const ColorwayIntensityIdPostfixToL10nMap = [
  ["-soft-colorway@mozilla.org", "colorway-intensity-soft"],
  ["-balanced-colorway@mozilla.org", "colorway-intensity-balanced"],
  ["-bold-colorway@mozilla.org", "colorway-intensity-bold"],
];

XPCOMUtils.defineLazyPreferenceGetter(
  lazy,
  "retainedThemes",
  kRetainedThemesPref,
  null,
  null,
  val => {
    if (!val) {
      return [];
    }

    let parsedVal;
    try {
      parsedVal = JSON.parse(val);
    } catch (ex) {
      console.log(`${kRetainedThemesPref} has invalid value.`);
      return [];
    }

    return parsedVal;
  }
);

class _BuiltInThemes {
  /**
   * The list of themes to be installed. This is exposed on the class so tests
   * can set custom config files.
   */
  builtInThemeMap = lazy.BuiltInThemeConfig;

  /**
   * @param {string} id An addon's id string.
   * @returns {string}
   *   If `id` refers to a built-in theme, returns a path pointing to the
   *   theme's preview image. Null otherwise.
   */
  previewForBuiltInThemeId(id) {
    let theme = this.builtInThemeMap.get(id);
    if (theme) {
      return `${theme.path}preview.svg`;
    }

    return null;
  }

  /**
   * @param {string} id An addon's id string.
   * @return {boolean}
   *   True if the theme with id `id` is a monochromatic theme.
   */
  isMonochromaticTheme(id) {
    return id.endsWith("-colorway@mozilla.org");
  }

  /**
   * If the active theme is built-in, this function calls
   * AddonManager.maybeInstallBuiltinAddon for that theme.
   */
  maybeInstallActiveBuiltInTheme() {
    const activeThemeID = Services.prefs.getStringPref(
      kActiveThemePref,
      "default-theme@mozilla.org"
    );
    let activeBuiltInTheme = this.builtInThemeMap.get(activeThemeID);

    if (activeBuiltInTheme) {
      lazy.AddonManager.maybeInstallBuiltinAddon(
        activeThemeID,
        activeBuiltInTheme.version,
        `resource://builtin-themes/${activeBuiltInTheme.path}`
      );
    }
  }

  /**
   * Ensures that all built-in themes are installed and expired themes are
   * uninstalled.
   */
  async ensureBuiltInThemes() {
    let installPromises = [];
    installPromises.push(this._uninstallExpiredThemes());

    const now = new Date();
    this.monochromaticSortIndices = new Map();
    let monochromaticSortIndex = 0;
    for (let [id, themeInfo] of this.builtInThemeMap.entries()) {
      if (
        !themeInfo.expiry ||
        lazy.retainedThemes.includes(id) ||
        new Date(themeInfo.expiry) > now
      ) {
        installPromises.push(
          lazy.AddonManager.maybeInstallBuiltinAddon(
            id,
            themeInfo.version,
            themeInfo.path
          )
        );
        if (this.isMonochromaticTheme(id)) {
          // Monochromatic themes get sorted in the UI according to their
          // position in the config, implied by this loop over
          // builtInThemeMap.entries().
          this.monochromaticSortIndices.set(id, monochromaticSortIndex++);
        }
      }
    }

    await Promise.all(installPromises);
  }

  /**
   * @param {string} id
   *   A theme's ID.
   * @returns {boolean}
   *   Returns true if the theme is expired. False otherwise.
   * @note This looks up the id in a Map rather than accessing a property on
   *   the addon itself. That makes calls to this function O(m) where m is the
   *   total number of built-in themes offered now or in the past. Since we
   *   are using a Map, calls are O(1) in the average case.
   */
  themeIsExpired(id) {
    let themeInfo = this.builtInThemeMap.get(id);
    return themeInfo?.expiry && new Date(themeInfo.expiry) < new Date();
  }

  /**
   * @param {string} id
   *   The theme's id.
   * @return {boolean}
   *   True if the theme with id `id` is both expired and retained. That is,
   *   the user has the ability to use it after its expiry date.
   */
  isRetainedExpiredTheme(id) {
    return lazy.retainedThemes.includes(id) && this.themeIsExpired(id);
  }

  /**
   * Uninstalls themes after they expire. If the expired theme is active, then
   * it is not uninstalled. Instead, it is saved so that the user can use it
   * indefinitely.
   */
  async _uninstallExpiredThemes() {
    const activeThemeID = Services.prefs.getStringPref(
      kActiveThemePref,
      "default-theme@mozilla.org"
    );
    const now = new Date();
    const expiredThemes = Array.from(this.builtInThemeMap.entries()).filter(
      ([id, themeInfo]) =>
        !!themeInfo.expiry &&
        !lazy.retainedThemes.includes(id) &&
        new Date(themeInfo.expiry) <= now
    );
    for (let [id] of expiredThemes) {
      if (id == activeThemeID) {
        this._retainLimitedTimeTheme(id);
      } else {
        try {
          let addon = await lazy.AddonManager.getAddonByID(id);
          if (addon) {
            await addon.uninstall();
          }
        } catch (e) {
          Cu.reportError(`Failed to uninstall expired theme ${id}`);
        }
      }
    }
  }

  /**
   * Set a pref to ensure that the user can continue to use a specified theme
   * past its expiry date.
   * @param {string} id
   *   The ID of the theme to retain.
   */
  _retainLimitedTimeTheme(id) {
    if (!lazy.retainedThemes.includes(id)) {
      lazy.retainedThemes.push(id);
      Services.prefs.setStringPref(
        kRetainedThemesPref,
        JSON.stringify(lazy.retainedThemes)
      );
    }
  }

  /**
   * Finds the active colorway collection.
   * @return {object}
   *   Colorway Collection
   */
  findActiveColorwayCollection() {
    return this.builtInThemeMap.findActiveColorwayCollection();
  }

  /**
   * @return {boolean}
   *   Whether a specific theme is part of the currently active colorway
   *   collection.
   */
  isColorwayFromCurrentCollection(id) {
    let collection = this.findActiveColorwayCollection();
    return (
      collection && this.builtInThemeMap.get(id)?.collection == collection.id
    );
  }

  /**
   * Colorway collections are usually divided into and presented as "groups".
   * A group either contains closely related colorways, e.g. stemming from the
   * same base color but with different intensities (soft, balanced, and bold),
   * or if the current collection doesn't have intensities, each colorway is
   * their own group. Group name localization is optional.
   * @param {string} id
   *   The ID of the colorway add-on.
   * @return {string}
   *   Localized colorway group name. null if there's no such name, in which
   *   case the caller should fall back on getting a name from the add-on API.
   */
  getLocalizedColorwayGroupName(colorwayId) {
    return this._getColorwayString(colorwayId, "groupName");
  }

  /**
   * @param {string} id
   *   The ID of the colorway add-on.
   * @return {string}
   *   L10nId for intensity value of the colorway with the provided id, null if
   *   there's none.
   */
  getColorwayIntensityL10nId(colorwayId) {
    const result = ColorwayIntensityIdPostfixToL10nMap.find(
      ([postfix, l10nId]) => colorwayId.endsWith(postfix)
    );
    return result ? result[1] : null;
  }

  /**
   * @param {string} id
   *   The ID of the colorway add-on.
   * @return {string}
   *   Localized description of the colorway with the provided id, null if
   *   there's none.
   */
  getLocalizedColorwayDescription(colorwayId) {
    return this._getColorwayString(colorwayId, "description");
  }

  _getColorwayString(colorwayId, stringType) {
    let l10nId = this.builtInThemeMap.get(colorwayId)?.l10nId?.[stringType];
    let s;
    if (l10nId) {
      [s] = ColorwayL10n.formatMessagesSync([
        {
          id: l10nId,
        },
      ]);
    }
    return s?.value || null;
  }
}

var BuiltInThemes = new _BuiltInThemes();
