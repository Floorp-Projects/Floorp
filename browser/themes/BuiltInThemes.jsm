/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

var EXPORTED_SYMBOLS = ["BuiltInThemes"];

const { XPCOMUtils } = ChromeUtils.import(
  "resource://gre/modules/XPCOMUtils.jsm"
);

XPCOMUtils.defineLazyModuleGetters(this, {
  AddonManager: "resource://gre/modules/AddonManager.jsm",
  Services: "resource://gre/modules/Services.jsm",
});

// List of themes built in to the browser. The themes are represented by objects
// containing their id, current version, and path relative to
// resource://builtin-themes/.
const STANDARD_THEMES = new Map([
  [
    "firefox-compact-light@mozilla.org",
    {
      version: "1.2",
      path: "light/",
    },
  ],
  [
    "firefox-compact-dark@mozilla.org",
    {
      version: "1.2",
      path: "dark/",
    },
  ],
  [
    "firefox-alpenglow@mozilla.org",
    {
      version: "1.4",
      path: "alpenglow/",
    },
  ],
]);

const COLORWAY_THEMES = new Map([
  [
    "lush-soft-colorway@mozilla.org",
    {
      version: "1.0",
      path: "monochromatic/lush/soft/",
    },
  ],
  [
    "lush-balanced-colorway@mozilla.org",
    {
      version: "1.0",
      path: "monochromatic/lush/balanced/",
    },
  ],
  [
    "lush-bold-colorway@mozilla.org",
    {
      version: "1.0",
      path: "monochromatic/lush/bold/",
    },
  ],
  [
    "abstract-soft-colorway@mozilla.org",
    {
      version: "1.0",
      path: "monochromatic/abstract/soft/",
    },
  ],
  [
    "abstract-balanced-colorway@mozilla.org",
    {
      version: "1.0",
      path: "monochromatic/abstract/balanced/",
    },
  ],
  [
    "abstract-bold-colorway@mozilla.org",
    {
      version: "1.0",
      path: "monochromatic/abstract/bold/",
    },
  ],
  [
    "elemental-soft-colorway@mozilla.org",
    {
      version: "1.0",
      path: "monochromatic/elemental/soft/",
    },
  ],
  [
    "elemental-balanced-colorway@mozilla.org",
    {
      version: "1.0",
      path: "monochromatic/elemental/balanced/",
    },
  ],
  [
    "elemental-bold-colorway@mozilla.org",
    {
      version: "1.0",
      path: "monochromatic/elemental/bold/",
    },
  ],
  [
    "cheers-soft-colorway@mozilla.org",
    {
      version: "1.0",
      path: "monochromatic/cheers/soft/",
    },
  ],
  [
    "cheers-balanced-colorway@mozilla.org",
    {
      version: "1.0",
      path: "monochromatic/cheers/balanced/",
    },
  ],
  [
    "cheers-bold-colorway@mozilla.org",
    {
      version: "1.0",
      path: "monochromatic/cheers/bold/",
    },
  ],
  [
    "graffiti-soft-colorway@mozilla.org",
    {
      version: "1.0",
      path: "monochromatic/graffiti/soft/",
    },
  ],
  [
    "graffiti-balanced-colorway@mozilla.org",
    {
      version: "1.0",
      path: "monochromatic/graffiti/balanced/",
    },
  ],
  [
    "graffiti-bold-colorway@mozilla.org",
    {
      version: "1.0",
      path: "monochromatic/graffiti/bold/",
    },
  ],
  [
    "foto-soft-colorway@mozilla.org",
    {
      version: "1.0",
      path: "monochromatic/foto/soft/",
    },
  ],
  [
    "foto-balanced-colorway@mozilla.org",
    {
      version: "1.0",
      path: "monochromatic/foto/balanced/",
    },
  ],
  [
    "foto-bold-colorway@mozilla.org",
    {
      version: "1.0",
      path: "monochromatic/foto/bold/",
    },
  ],
]);

class _BuiltInThemes {
  constructor() {
    if (!Services.prefs.getBoolPref("browser.theme.colorways.enabled", true)) {
      // If the colorways pref is false on startup, then we've pushed out a pref
      // change to clients, and should remove the themes.
      this._uninstallColorwayThemes();
    }
  }

  /**
   * @param {string} id An addon's id string.
   * @returns {string}
   *   If `id` refers to a built-in theme, returns a path pointing to the
   *   theme's preview image. Null otherwise.
   */
  previewForBuiltInThemeId(id) {
    if (STANDARD_THEMES.has(id)) {
      return `resource://builtin-themes/${
        STANDARD_THEMES.get(id).path
      }preview.svg`;
    }
    if (
      Services.prefs.getBoolPref("browser.theme.colorways.enabled", true) &&
      COLORWAY_THEMES.has(id)
    ) {
      return `resource://builtin-themes/${
        COLORWAY_THEMES.get(id).path
      }preview.svg`;
    }

    return null;
  }

  /**
   * If the active theme is built-in, this function calls
   * AddonManager.maybeInstallBuiltinAddon for that theme.
   */
  maybeInstallActiveBuiltInTheme() {
    let activeThemeID = Services.prefs.getStringPref(
      "extensions.activeThemeID",
      "default-theme@mozilla.org"
    );
    let activeBuiltInTheme = STANDARD_THEMES.get(activeThemeID);
    if (
      !activeBuiltInTheme &&
      Services.prefs.getBoolPref("browser.theme.colorways.enabled", true)
    ) {
      activeBuiltInTheme = COLORWAY_THEMES.get(activeThemeID);
    }

    if (activeBuiltInTheme) {
      AddonManager.maybeInstallBuiltinAddon(
        activeThemeID,
        activeBuiltInTheme.version,
        `resource://builtin-themes/${activeBuiltInTheme.path}`
      );
    }
  }

  /**
   * Ensures that all built-in themes are installed.
   */
  async ensureBuiltInThemes() {
    let installPromises = [];
    for (let [id, { version, path }] of STANDARD_THEMES.entries()) {
      installPromises.push(
        AddonManager.maybeInstallBuiltinAddon(
          id,
          version,
          `resource://builtin-themes/${path}`
        )
      );
    }

    if (Services.prefs.getBoolPref("browser.theme.colorways.enabled", true)) {
      for (let [id, { version, path }] of COLORWAY_THEMES.entries()) {
        installPromises.push(
          AddonManager.maybeInstallBuiltinAddon(
            id,
            version,
            `resource://builtin-themes/${path}`
          )
        );
      }
    }

    await Promise.all(installPromises);
  }

  async _uninstallColorwayThemes() {
    for (let id of COLORWAY_THEMES.keys()) {
      try {
        let addon = await AddonManager.getAddonByID(id);
        await addon.uninstall();
      } catch (e) {
        Cu.reportError(`Failed to uninstall colorway theme ${id}`);
      }
    }
  }
}

var BuiltInThemes = new _BuiltInThemes();
