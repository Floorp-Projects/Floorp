/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

var EXPORTED_SYMBOLS = ["BuiltInThemes"];

const { XPCOMUtils } = ChromeUtils.import(
  "resource://gre/modules/XPCOMUtils.jsm"
);

XPCOMUtils.defineLazyModuleGetters(this, {
  AddonManager: "resource://gre/modules/AddonManager.jsm",
  BuiltInThemeConfig: "resource:///modules/BuiltInThemeConfig.jsm",
  Services: "resource://gre/modules/Services.jsm",
});

const kActiveThemePref = "extensions.activeThemeID";
const kRetainedThemesPref = "browser.theme.retainedExpiredThemes";

XPCOMUtils.defineLazyPreferenceGetter(
  this,
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
  builtInThemeMap = BuiltInThemeConfig;

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
      AddonManager.maybeInstallBuiltinAddon(
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
    for (let [id, themeInfo] of this.builtInThemeMap.entries()) {
      if (
        !themeInfo.expiry ||
        retainedThemes.includes(id) ||
        new Date(themeInfo.expiry) > now
      ) {
        installPromises.push(
          AddonManager.maybeInstallBuiltinAddon(
            id,
            themeInfo.version,
            themeInfo.path
          )
        );
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
    return retainedThemes.includes(id) && this.themeIsExpired(id);
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
        !retainedThemes.includes(id) &&
        new Date(themeInfo.expiry) <= now
    );
    for (let [id] of expiredThemes) {
      if (id == activeThemeID) {
        this._retainLimitedTimeTheme(id);
      } else {
        try {
          let addon = await AddonManager.getAddonByID(id);
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
    if (!retainedThemes.includes(id)) {
      retainedThemes.push(id);
      Services.prefs.setStringPref(
        kRetainedThemesPref,
        JSON.stringify(retainedThemes)
      );
    }
  }
}

var BuiltInThemes = new _BuiltInThemes();
