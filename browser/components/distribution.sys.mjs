/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

const DISTRIBUTION_CUSTOMIZATION_COMPLETE_TOPIC =
  "distribution-customization-complete";

const PREF_CACHED_FILE_EXISTENCE = "distribution.iniFile.exists.value";
const PREF_CACHED_FILE_APPVERSION = "distribution.iniFile.exists.appversion";

import { AppConstants } from "resource://gre/modules/AppConstants.sys.mjs";

const lazy = {};
ChromeUtils.defineESModuleGetters(lazy, {
  AddonManager: "resource://gre/modules/AddonManager.sys.mjs",
  PlacesUtils: "resource://gre/modules/PlacesUtils.sys.mjs",
});

export function DistributionCustomizer() {}

DistributionCustomizer.prototype = {
  // These prefixes must only contain characters
  // allowed by PlacesUtils.isValidGuid
  BOOKMARK_GUID_PREFIX: "DstB-",
  FOLDER_GUID_PREFIX: "DstF-",

  get _iniFile() {
    // For parallel xpcshell testing purposes allow loading the distribution.ini
    // file from the profile folder through an hidden pref.
    let loadFromProfile = Services.prefs.getBoolPref(
      "distribution.testing.loadFromProfile",
      false
    );

    let iniFile;
    try {
      iniFile = loadFromProfile
        ? Services.dirsvc.get("ProfD", Ci.nsIFile)
        : Services.dirsvc.get("XREAppDist", Ci.nsIFile);
      if (loadFromProfile) {
        iniFile.leafName = "distribution";
      }
      iniFile.append("distribution.ini");
    } catch (ex) {}

    this.__defineGetter__("_iniFile", () => iniFile);
    return iniFile;
  },

  get _hasDistributionIni() {
    if (Services.prefs.prefHasUserValue(PREF_CACHED_FILE_EXISTENCE)) {
      let knownForVersion = Services.prefs.getStringPref(
        PREF_CACHED_FILE_APPVERSION,
        "unknown"
      );
      // StartupCacheInfo isn't available in xpcshell tests.
      if (
        knownForVersion == AppConstants.MOZ_APP_VERSION &&
        (Cu.isInAutomation ||
          Cc["@mozilla.org/startupcacheinfo;1"].getService(
            Ci.nsIStartupCacheInfo
          ).FoundDiskCacheOnInit)
      ) {
        return Services.prefs.getBoolPref(PREF_CACHED_FILE_EXISTENCE);
      }
    }

    let fileExists = this._iniFile.exists();
    Services.prefs.setBoolPref(PREF_CACHED_FILE_EXISTENCE, fileExists);
    Services.prefs.setStringPref(
      PREF_CACHED_FILE_APPVERSION,
      AppConstants.MOZ_APP_VERSION
    );

    this.__defineGetter__("_hasDistributionIni", () => fileExists);
    return fileExists;
  },

  get _ini() {
    let ini = null;
    try {
      if (this._hasDistributionIni) {
        ini = Cc["@mozilla.org/xpcom/ini-parser-factory;1"]
          .getService(Ci.nsIINIParserFactory)
          .createINIParser(this._iniFile);
      }
    } catch (e) {
      if (e.result == Cr.NS_ERROR_FILE_NOT_FOUND) {
        // We probably had cached the file existence as true,
        // but it no longer exists. We could set the new cache
        // value here, but let's just invalidate the cache and
        // let it be cached by a single code path on the next check.
        Services.prefs.clearUserPref(PREF_CACHED_FILE_EXISTENCE);
      } else {
        // Unable to parse INI.
        console.error("Unable to parse distribution.ini");
      }
    }
    this.__defineGetter__("_ini", () => ini);
    return this._ini;
  },

  get _locale() {
    const locale = Services.locale.requestedLocale || "en-US";
    this.__defineGetter__("_locale", () => locale);
    return this._locale;
  },

  get _language() {
    let language = this._locale.split("-")[0];
    this.__defineGetter__("_language", () => language);
    return this._language;
  },

  async _removeDistributionBookmarks() {
    await lazy.PlacesUtils.bookmarks.fetch(
      { guidPrefix: this.BOOKMARK_GUID_PREFIX },
      bookmark => lazy.PlacesUtils.bookmarks.remove(bookmark).catch()
    );
    await lazy.PlacesUtils.bookmarks.fetch(
      { guidPrefix: this.FOLDER_GUID_PREFIX },
      folder => {
        lazy.PlacesUtils.bookmarks.remove(folder).catch();
      }
    );
  },

  async _parseBookmarksSection(parentGuid, section) {
    let keys = Array.from(this._ini.getKeys(section)).sort();
    let re = /^item\.(\d+)\.(\w+)\.?(\w*)/;
    let items = {};
    let defaultIndex = -1;
    let maxIndex = -1;

    for (let key of keys) {
      let m = re.exec(key);
      if (m) {
        let [, itemIndex, iprop, ilocale] = m;
        itemIndex = parseInt(itemIndex);

        if (ilocale) {
          continue;
        }

        if (keys.includes(key + "." + this._locale)) {
          key += "." + this._locale;
        } else if (keys.includes(key + "." + this._language)) {
          key += "." + this._language;
        }

        if (!items[itemIndex]) {
          items[itemIndex] = {};
        }
        items[itemIndex][iprop] = this._ini.getString(section, key);

        if (iprop == "type" && items[itemIndex].type == "default") {
          defaultIndex = itemIndex;
        }

        if (maxIndex < itemIndex) {
          maxIndex = itemIndex;
        }
      } else {
        dump(`Key did not match: ${key}\n`);
      }
    }

    let prependIndex = 0;
    for (let itemIndex = 0; itemIndex <= maxIndex; itemIndex++) {
      if (!items[itemIndex]) {
        continue;
      }

      let index = lazy.PlacesUtils.bookmarks.DEFAULT_INDEX;
      let item = items[itemIndex];

      switch (item.type) {
        case "default":
          break;

        case "folder":
          if (itemIndex < defaultIndex) {
            index = prependIndex++;
          }

          let folder = await lazy.PlacesUtils.bookmarks.insert({
            type: lazy.PlacesUtils.bookmarks.TYPE_FOLDER,
            guid: lazy.PlacesUtils.generateGuidWithPrefix(
              this.FOLDER_GUID_PREFIX
            ),
            parentGuid,
            index,
            title: item.title,
          });

          await this._parseBookmarksSection(
            folder.guid,
            "BookmarksFolder-" + item.folderId
          );
          break;

        case "separator":
          if (itemIndex < defaultIndex) {
            index = prependIndex++;
          }

          await lazy.PlacesUtils.bookmarks.insert({
            type: lazy.PlacesUtils.bookmarks.TYPE_SEPARATOR,
            parentGuid,
            index,
          });
          break;

        case "livemark":
          // Livemarks are no more supported, instead of a livemark we'll insert
          // a bookmark pointing to the site uri, if available.
          if (!item.siteLink) {
            break;
          }
          if (itemIndex < defaultIndex) {
            index = prependIndex++;
          }

          await lazy.PlacesUtils.bookmarks.insert({
            parentGuid,
            index,
            title: item.title,
            url: item.siteLink,
          });
          break;

        case "bookmark":
        default:
          if (itemIndex < defaultIndex) {
            index = prependIndex++;
          }

          await lazy.PlacesUtils.bookmarks.insert({
            guid: lazy.PlacesUtils.generateGuidWithPrefix(
              this.BOOKMARK_GUID_PREFIX
            ),
            parentGuid,
            index,
            title: item.title,
            url: item.link,
          });

          if (item.icon && item.iconData) {
            try {
              let faviconURI = Services.io.newURI(item.icon);
              lazy.PlacesUtils.favicons.replaceFaviconDataFromDataURL(
                faviconURI,
                item.iconData,
                0,
                Services.scriptSecurityManager.getSystemPrincipal()
              );

              lazy.PlacesUtils.favicons.setAndFetchFaviconForPage(
                Services.io.newURI(item.link),
                faviconURI,
                false,
                lazy.PlacesUtils.favicons.FAVICON_LOAD_NON_PRIVATE,
                null,
                Services.scriptSecurityManager.getSystemPrincipal()
              );
            } catch (e) {
              console.error(e);
            }
          }

          break;
      }
    }
  },

  _newProfile: false,
  _customizationsApplied: false,
  applyCustomizations: function DIST_applyCustomizations() {
    this._customizationsApplied = true;

    if (!Services.prefs.prefHasUserValue("browser.migration.version")) {
      this._newProfile = true;
    }

    if (!this._ini) {
      return this._checkCustomizationComplete();
    }

    if (!this._prefDefaultsApplied) {
      this.applyPrefDefaults();
    }
  },

  _bookmarksApplied: false,
  async applyBookmarks() {
    let prefs = Services.prefs
      .getChildList("distribution.yandex")
      .concat(Services.prefs.getChildList("distribution.mailru"))
      .concat(Services.prefs.getChildList("distribution.okru"));
    if (prefs.length) {
      let extensionIDs = [
        "sovetnik-yandex@yandex.ru",
        "vb@yandex.ru",
        "ntp-mail@corp.mail.ru",
        "ntp-okru@corp.mail.ru",
      ];
      for (let extensionID of extensionIDs) {
        let addon = await lazy.AddonManager.getAddonByID(extensionID);
        if (addon) {
          await addon.disable();
        }
      }
      for (let pref of prefs) {
        Services.prefs.clearUserPref(pref);
      }
      await this._removeDistributionBookmarks();
    } else {
      await this._doApplyBookmarks();
    }
    this._bookmarksApplied = true;
    this._checkCustomizationComplete();
  },

  async _doApplyBookmarks() {
    if (!this._ini) {
      return;
    }

    let sections = enumToObject(this._ini.getSections());

    // The global section, and several of its fields, is required
    // (we also check here to be consistent with applyPrefDefaults below)
    if (!sections.Global) {
      return;
    }

    let globalPrefs = enumToObject(this._ini.getKeys("Global"));
    if (!(globalPrefs.id && globalPrefs.version && globalPrefs.about)) {
      return;
    }

    let bmProcessedPref;
    try {
      bmProcessedPref = this._ini.getString(
        "Global",
        "bookmarks.initialized.pref"
      );
    } catch (e) {
      bmProcessedPref =
        "distribution." +
        this._ini.getString("Global", "id") +
        ".bookmarksProcessed";
    }

    if (Services.prefs.getBoolPref(bmProcessedPref, false)) {
      return;
    }

    let { ProfileAge } = ChromeUtils.importESModule(
      "resource://gre/modules/ProfileAge.sys.mjs"
    );
    let profileAge = await ProfileAge();
    let resetDate = await profileAge.reset;

    // If the profile has been reset, don't recreate bookmarks.
    if (!resetDate) {
      if (sections.BookmarksMenu) {
        await this._parseBookmarksSection(
          lazy.PlacesUtils.bookmarks.menuGuid,
          "BookmarksMenu"
        );
      }
      if (sections.BookmarksToolbar) {
        await this._parseBookmarksSection(
          lazy.PlacesUtils.bookmarks.toolbarGuid,
          "BookmarksToolbar"
        );
      }
    }
    Services.prefs.setBoolPref(bmProcessedPref, true);
  },

  _prefDefaultsApplied: false,
  applyPrefDefaults: function DIST_applyPrefDefaults() {
    this._prefDefaultsApplied = true;
    if (!this._ini) {
      return this._checkCustomizationComplete();
    }

    let sections = enumToObject(this._ini.getSections());

    // The global section, and several of its fields, is required
    if (!sections.Global) {
      return this._checkCustomizationComplete();
    }
    let globalPrefs = enumToObject(this._ini.getKeys("Global"));
    if (!(globalPrefs.id && globalPrefs.version)) {
      return this._checkCustomizationComplete();
    }
    let distroID = this._ini.getString("Global", "id");
    if (!globalPrefs.about && !distroID.startsWith("mozilla-")) {
      // About is required unless it is a mozilla distro.
      return this._checkCustomizationComplete();
    }

    let defaults = Services.prefs.getDefaultBranch(null);

    // Global really contains info we set as prefs.  They're only
    // separate because they are "special" (read: required)

    defaults.setStringPref("distribution.id", distroID);

    if (
      distroID.startsWith("yandex") ||
      distroID.startsWith("mailru") ||
      distroID.startsWith("okru")
    ) {
      this.__defineGetter__("_ini", () => null);
      return this._checkCustomizationComplete();
    }

    defaults.setStringPref(
      "distribution.version",
      this._ini.getString("Global", "version")
    );

    let partnerAbout;
    try {
      if (globalPrefs["about." + this._locale]) {
        partnerAbout = this._ini.getString("Global", "about." + this._locale);
      } else if (globalPrefs["about." + this._language]) {
        partnerAbout = this._ini.getString("Global", "about." + this._language);
      } else {
        partnerAbout = this._ini.getString("Global", "about");
      }
      defaults.setStringPref("distribution.about", partnerAbout);
    } catch (e) {
      /* ignore bad prefs due to bug 895473 and move on */
    }

    /* order of precedence is locale->language->default */

    let preferences = new Map();

    if (sections.Preferences) {
      for (let key of this._ini.getKeys("Preferences")) {
        let value = this._ini.getString("Preferences", key);
        if (value) {
          preferences.set(key, value);
        }
      }
    }

    if (sections["Preferences-" + this._language]) {
      for (let key of this._ini.getKeys("Preferences-" + this._language)) {
        let value = this._ini.getString("Preferences-" + this._language, key);
        if (value) {
          preferences.set(key, value);
        } else {
          // If something was set by Preferences, but it's empty in language,
          // it should be removed.
          preferences.delete(key);
        }
      }
    }

    if (sections["Preferences-" + this._locale]) {
      for (let key of this._ini.getKeys("Preferences-" + this._locale)) {
        let value = this._ini.getString("Preferences-" + this._locale, key);
        if (value) {
          preferences.set(key, value);
        } else {
          // If something was set by Preferences, but it's empty in locale,
          // it should be removed.
          preferences.delete(key);
        }
      }
    }

    for (let [prefName, prefValue] of preferences) {
      prefValue = prefValue.replace(/%LOCALE%/g, this._locale);
      prefValue = prefValue.replace(/%LANGUAGE%/g, this._language);
      prefValue = parseValue(prefValue);
      try {
        if (prefName == "general.useragent.locale") {
          defaults.setStringPref("intl.locale.requested", prefValue);
        } else {
          switch (typeof prefValue) {
            case "boolean":
              defaults.setBoolPref(prefName, prefValue);
              break;
            case "number":
              defaults.setIntPref(prefName, prefValue);
              break;
            case "string":
              defaults.setStringPref(prefName, prefValue);
              break;
          }
        }
      } catch (e) {
        /* ignore bad prefs and move on */
      }
    }

    if (this._ini.getString("Global", "id") == "yandex") {
      // All yandex distributions have the same distribution ID,
      // so we're using an internal preference to name them correctly.
      // This is needed for search to work properly.
      try {
        defaults.setStringPref(
          "distribution.id",
          defaults
            .get("extensions.yasearch@yandex.ru.clids.vendor")
            .replace("firefox", "yandex")
        );
      } catch (e) {
        // Just use the default distribution ID.
      }
    }

    let localizedStr = Cc["@mozilla.org/pref-localizedstring;1"].createInstance(
      Ci.nsIPrefLocalizedString
    );

    let localizablePreferences = new Map();

    if (sections.LocalizablePreferences) {
      for (let key of this._ini.getKeys("LocalizablePreferences")) {
        let value = this._ini.getString("LocalizablePreferences", key);
        if (value) {
          localizablePreferences.set(key, value);
        }
      }
    }

    if (sections["LocalizablePreferences-" + this._language]) {
      for (let key of this._ini.getKeys(
        "LocalizablePreferences-" + this._language
      )) {
        let value = this._ini.getString(
          "LocalizablePreferences-" + this._language,
          key
        );
        if (value) {
          localizablePreferences.set(key, value);
        } else {
          // If something was set by Preferences, but it's empty in language,
          // it should be removed.
          localizablePreferences.delete(key);
        }
      }
    }

    if (sections["LocalizablePreferences-" + this._locale]) {
      for (let key of this._ini.getKeys(
        "LocalizablePreferences-" + this._locale
      )) {
        let value = this._ini.getString(
          "LocalizablePreferences-" + this._locale,
          key
        );
        if (value) {
          localizablePreferences.set(key, value);
        } else {
          // If something was set by Preferences, but it's empty in locale,
          // it should be removed.
          localizablePreferences.delete(key);
        }
      }
    }

    for (let [prefName, prefValue] of localizablePreferences) {
      prefValue = parseValue(prefValue);
      prefValue = prefValue.replace(/%LOCALE%/g, this._locale);
      prefValue = prefValue.replace(/%LANGUAGE%/g, this._language);
      localizedStr.data = "data:text/plain," + prefName + "=" + prefValue;
      try {
        defaults.setComplexValue(
          prefName,
          Ci.nsIPrefLocalizedString,
          localizedStr
        );
      } catch (e) {
        /* ignore bad prefs and move on */
      }
    }

    return this._checkCustomizationComplete();
  },

  _checkCustomizationComplete: function DIST__checkCustomizationComplete() {
    const BROWSER_DOCURL = AppConstants.BROWSER_CHROME_URL;

    if (this._newProfile) {
      try {
        var showPersonalToolbar = Services.prefs.getBoolPref(
          "browser.showPersonalToolbar"
        );
        if (showPersonalToolbar) {
          Services.prefs.setCharPref(
            "browser.toolbars.bookmarks.visibility",
            "always"
          );
        }
      } catch (e) {}
      try {
        var showMenubar = Services.prefs.getBoolPref("browser.showMenubar");
        if (showMenubar) {
          Services.xulStore.setValue(
            BROWSER_DOCURL,
            "toolbar-menubar",
            "autohide",
            "false"
          );
        }
      } catch (e) {}
    }

    let prefDefaultsApplied = this._prefDefaultsApplied || !this._ini;
    if (
      this._customizationsApplied &&
      this._bookmarksApplied &&
      prefDefaultsApplied
    ) {
      Services.obs.notifyObservers(
        null,
        DISTRIBUTION_CUSTOMIZATION_COMPLETE_TOPIC
      );
    }
  },
};

function parseValue(value) {
  try {
    value = JSON.parse(value);
  } catch (e) {
    // JSON.parse catches numbers and booleans.
    // Anything else, we assume is a string.
    // Remove the quotes that aren't needed anymore.
    value = value.replace(/^"/, "");
    value = value.replace(/"$/, "");
  }
  return value;
}

function enumToObject(UTF8Enumerator) {
  let ret = {};
  for (let i of UTF8Enumerator) {
    ret[i] = 1;
  }
  return ret;
}
