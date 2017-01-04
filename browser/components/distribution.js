/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

this.EXPORTED_SYMBOLS = [ "DistributionCustomizer" ];

var Ci = Components.interfaces;
var Cc = Components.classes;
var Cr = Components.results;
var Cu = Components.utils;

const DISTRIBUTION_CUSTOMIZATION_COMPLETE_TOPIC =
  "distribution-customization-complete";

Cu.import("resource://gre/modules/XPCOMUtils.jsm");
Cu.import("resource://gre/modules/Services.jsm");
Cu.import("resource://gre/modules/Task.jsm");
Cu.import("resource://gre/modules/Preferences.jsm");
XPCOMUtils.defineLazyModuleGetter(this, "PlacesUtils",
                                  "resource://gre/modules/PlacesUtils.jsm");

this.DistributionCustomizer = function DistributionCustomizer() {
  // For parallel xpcshell testing purposes allow loading the distribution.ini
  // file from the profile folder through an hidden pref.
  let loadFromProfile = false;
  try {
    loadFromProfile = Services.prefs.getBoolPref("distribution.testing.loadFromProfile");
  } catch (ex) {}
  let dirSvc = Cc["@mozilla.org/file/directory_service;1"].
               getService(Ci.nsIProperties);
  try {
    let iniFile = loadFromProfile ? dirSvc.get("ProfD", Ci.nsIFile)
                                  : dirSvc.get("XREAppDist", Ci.nsIFile);
    if (loadFromProfile) {
      iniFile.leafName = "distribution";
    }
    iniFile.append("distribution.ini");
    if (iniFile.exists())
      this._iniFile = iniFile;
  } catch (ex) {}
}

DistributionCustomizer.prototype = {
  _iniFile: null,

  get _ini() {
    let ini = null;
    try {
      if (this._iniFile) {
        ini = Cc["@mozilla.org/xpcom/ini-parser-factory;1"].
              getService(Ci.nsIINIParserFactory).
              createINIParser(this._iniFile);
      }
    } catch (e) {
      // Unable to parse INI.
      Cu.reportError("Unable to parse distribution.ini");
    }
    this.__defineGetter__("_ini", () => ini);
    return this._ini;
  },

  get _locale() {
    let locale;
    try {
      locale = this._prefs.getCharPref("general.useragent.locale");
    } catch (e) {
      locale = "en-US";
    }
    this.__defineGetter__("_locale", () => locale);
    return this._locale;
  },

  get _language() {
    let language = this._locale.split("-")[0];
    this.__defineGetter__("_language", () => language);
    return this._language;
  },

  get _prefSvc() {
    let svc = Cc["@mozilla.org/preferences-service;1"].
              getService(Ci.nsIPrefService);
    this.__defineGetter__("_prefSvc", () => svc);
    return this._prefSvc;
  },

  get _prefs() {
    let branch = this._prefSvc.getBranch(null);
    this.__defineGetter__("_prefs", () => branch);
    return this._prefs;
  },

  get _ioSvc() {
    let svc = Cc["@mozilla.org/network/io-service;1"].
              getService(Ci.nsIIOService);
    this.__defineGetter__("_ioSvc", () => svc);
    return this._ioSvc;
  },

  _makeURI: function DIST__makeURI(spec) {
    return this._ioSvc.newURI(spec, null, null);
  },

  _parseBookmarksSection: Task.async(function* (parentGuid, section) {
    let keys = Array.from(enumerate(this._ini.getKeys(section))).sort();
    let re = /^item\.(\d+)\.(\w+)\.?(\w*)/;
    let items = {};
    let defaultIndex = -1;
    let maxIndex = -1;

    for (let key of keys) {
      let m = re.exec(key);
      if (m) {
        let [, itemIndex, iprop, ilocale] = m;
        itemIndex = parseInt(itemIndex);

        if (ilocale)
          continue;

        if (keys.indexOf(key + "." + this._locale) >= 0) {
          key += "." + this._locale;
        } else if (keys.indexOf(key + "." + this._language) >= 0) {
          key += "." + this._language;
        }

        if (!items[itemIndex])
          items[itemIndex] = {};
        items[itemIndex][iprop] = this._ini.getString(section, key);

        if (iprop == "type" && items[itemIndex]["type"] == "default")
          defaultIndex = itemIndex;

        if (maxIndex < itemIndex)
          maxIndex = itemIndex;
      } else {
        dump(`Key did not match: ${key}\n`);
      }
    }

    let prependIndex = 0;
    for (let itemIndex = 0; itemIndex <= maxIndex; itemIndex++) {
      if (!items[itemIndex])
        continue;

      let index = PlacesUtils.bookmarks.DEFAULT_INDEX;
      let item = items[itemIndex];

      switch (item.type) {
      case "default":
        break;

      case "folder":
        if (itemIndex < defaultIndex)
          index = prependIndex++;

        let folder = yield PlacesUtils.bookmarks.insert({
          type: PlacesUtils.bookmarks.TYPE_FOLDER,
          parentGuid, index, title: item.title
        });

        yield this._parseBookmarksSection(folder.guid,
                                          "BookmarksFolder-" + item.folderId);

        if (item.description) {
          let folderId = yield PlacesUtils.promiseItemId(folder.guid);
          PlacesUtils.annotations.setItemAnnotation(folderId,
                                                    "bookmarkProperties/description",
                                                    item.description, 0,
                                                    PlacesUtils.annotations.EXPIRE_NEVER);
        }

        break;

      case "separator":
        if (itemIndex < defaultIndex)
          index = prependIndex++;

        yield PlacesUtils.bookmarks.insert({
          type: PlacesUtils.bookmarks.TYPE_SEPARATOR,
          parentGuid, index
        });
        break;

      case "livemark":
        if (itemIndex < defaultIndex)
          index = prependIndex++;

        // Don't bother updating the livemark contents on creation.
        let parentId = yield PlacesUtils.promiseItemId(parentGuid);
        yield PlacesUtils.livemarks.addLivemark({
          feedURI: this._makeURI(item.feedLink),
          siteURI: this._makeURI(item.siteLink),
          parentId, index, title: item.title
        });
        break;

      case "bookmark":
      default:
        if (itemIndex < defaultIndex)
          index = prependIndex++;

        let bm = yield PlacesUtils.bookmarks.insert({
          parentGuid, index, title: item.title, url: item.link
        });

        if (item.description) {
          let bmId = yield PlacesUtils.promiseItemId(bm.guid);
          PlacesUtils.annotations.setItemAnnotation(bmId,
                                                    "bookmarkProperties/description",
                                                    item.description, 0,
                                                    PlacesUtils.annotations.EXPIRE_NEVER);
        }

        if (item.icon && item.iconData) {
          try {
            let faviconURI = this._makeURI(item.icon);
            PlacesUtils.favicons.replaceFaviconDataFromDataURL(
              faviconURI, item.iconData, 0,
              Services.scriptSecurityManager.getSystemPrincipal());

            PlacesUtils.favicons.setAndFetchFaviconForPage(
              this._makeURI(item.link), faviconURI, false,
              PlacesUtils.favicons.FAVICON_LOAD_NON_PRIVATE, null,
              Services.scriptSecurityManager.getSystemPrincipal());
          } catch (e) {
            Cu.reportError(e);
          }
        }

        if (item.keyword) {
          try {
            yield PlacesUtils.keywords.insert({ keyword: item.keyword,
                                                url: item.link });
          } catch (e) {
            Cu.reportError(e);
          }
        }

        break;
      }
    }
  }),

  _newProfile: false,
  _customizationsApplied: false,
  applyCustomizations: function DIST_applyCustomizations() {
    this._customizationsApplied = true;

    if (!Services.prefs.prefHasUserValue("browser.migration.version"))
      this._newProfile = true;

    if (!this._ini)
      return this._checkCustomizationComplete();

    // nsPrefService loads very early.  Reload prefs so we can set
    // distribution defaults during the prefservice:after-app-defaults
    // notification (see applyPrefDefaults below)
    this._prefSvc.QueryInterface(Ci.nsIObserver);
    this._prefSvc.observe(null, "reload-default-prefs", null);
  },

  _bookmarksApplied: false,
  applyBookmarks: Task.async(function* () {
    yield this._doApplyBookmarks();
    this._bookmarksApplied = true;
    this._checkCustomizationComplete();
  }),

  _doApplyBookmarks: Task.async(function* () {
    if (!this._ini)
      return;

    let sections = enumToObject(this._ini.getSections());

    // The global section, and several of its fields, is required
    // (we also check here to be consistent with applyPrefDefaults below)
    if (!sections["Global"])
      return;

    let globalPrefs = enumToObject(this._ini.getKeys("Global"));
    if (!(globalPrefs["id"] && globalPrefs["version"] && globalPrefs["about"]))
      return;

    let bmProcessedPref;
    try {
      bmProcessedPref = this._ini.getString("Global",
                                            "bookmarks.initialized.pref");
    } catch (e) {
      bmProcessedPref = "distribution." +
        this._ini.getString("Global", "id") + ".bookmarksProcessed";
    }

    let bmProcessed = false;
    try {
      bmProcessed = this._prefs.getBoolPref(bmProcessedPref);
    } catch (e) {}

    if (!bmProcessed) {
      if (sections["BookmarksMenu"])
        yield this._parseBookmarksSection(PlacesUtils.bookmarks.menuGuid,
                                          "BookmarksMenu");
      if (sections["BookmarksToolbar"])
        yield this._parseBookmarksSection(PlacesUtils.bookmarks.toolbarGuid,
                                          "BookmarksToolbar");
      this._prefs.setBoolPref(bmProcessedPref, true);
    }
  }),

  _prefDefaultsApplied: false,
  applyPrefDefaults: function DIST_applyPrefDefaults() {
    this._prefDefaultsApplied = true;
    if (!this._ini)
      return this._checkCustomizationComplete();

    let sections = enumToObject(this._ini.getSections());

    // The global section, and several of its fields, is required
    if (!sections["Global"])
      return this._checkCustomizationComplete();
    let globalPrefs = enumToObject(this._ini.getKeys("Global"));
    if (!(globalPrefs["id"] && globalPrefs["version"] && globalPrefs["about"]))
      return this._checkCustomizationComplete();

    let defaults = new Preferences({defaultBranch: true});

    // Global really contains info we set as prefs.  They're only
    // separate because they are "special" (read: required)

    defaults.set("distribution.id", this._ini.getString("Global", "id"));
    defaults.set("distribution.version", this._ini.getString("Global", "version"));

    let partnerAbout;
    try {
      if (globalPrefs["about." + this._locale]) {
        partnerAbout = this._ini.getString("Global", "about." + this._locale);
      } else if (globalPrefs["about." + this._language]) {
        partnerAbout = this._ini.getString("Global", "about." + this._language);
      } else {
        partnerAbout = this._ini.getString("Global", "about");
      }
      defaults.set("distribution.about", partnerAbout);
    } catch (e) {
      /* ignore bad prefs due to bug 895473 and move on */
      Cu.reportError(e);
    }

    var usedPreferences = [];

    if (sections["Preferences-" + this._locale]) {
      for (let key of enumerate(this._ini.getKeys("Preferences-" + this._locale))) {
        try {
          let value = this._ini.getString("Preferences-" + this._locale, key);
          if (value) {
            defaults.set(key, parseValue(value));
          }
          usedPreferences.push(key);
        } catch (e) { /* ignore bad prefs and move on */ }
      }
    }

    if (sections["Preferences-" + this._language]) {
      for (let key of enumerate(this._ini.getKeys("Preferences-" + this._language))) {
        if (usedPreferences.indexOf(key) > -1) {
          continue;
        }
        try {
          let value = this._ini.getString("Preferences-" + this._language, key);
          if (value) {
            defaults.set(key, parseValue(value));
          }
          usedPreferences.push(key);
        } catch (e) { /* ignore bad prefs and move on */ }
      }
    }

    if (sections["Preferences"]) {
      for (let key of enumerate(this._ini.getKeys("Preferences"))) {
        if (usedPreferences.indexOf(key) > -1) {
          continue;
        }
        try {
          let value = this._ini.getString("Preferences", key);
          if (value) {
            value = value.replace(/%LOCALE%/g, this._locale);
            value = value.replace(/%LANGUAGE%/g, this._language);
            defaults.set(key, parseValue(value));
          }
        } catch (e) { /* ignore bad prefs and move on */ }
      }
    }

    let localizedStr = Cc["@mozilla.org/pref-localizedstring;1"].
      createInstance(Ci.nsIPrefLocalizedString);

    var usedLocalizablePreferences = [];

    if (sections["LocalizablePreferences-" + this._locale]) {
      for (let key of enumerate(this._ini.getKeys("LocalizablePreferences-" + this._locale))) {
        try {
          let value = this._ini.getString("LocalizablePreferences-" + this._locale, key);
          if (value) {
            value = parseValue(value);
            localizedStr.data = "data:text/plain," + key + "=" + value;
            defaults._prefBranch.setComplexValue(key, Ci.nsIPrefLocalizedString, localizedStr);
          }
          usedLocalizablePreferences.push(key);
        } catch (e) { /* ignore bad prefs and move on */ }
      }
    }

    if (sections["LocalizablePreferences-" + this._language]) {
      for (let key of enumerate(this._ini.getKeys("LocalizablePreferences-" + this._language))) {
        if (usedLocalizablePreferences.indexOf(key) > -1) {
          continue;
        }
        try {
          let value = this._ini.getString("LocalizablePreferences-" + this._language, key);
          if (value) {
            value = parseValue(value);
            localizedStr.data = "data:text/plain," + key + "=" + value;
            defaults._prefBranch.setComplexValue(key, Ci.nsIPrefLocalizedString, localizedStr);
          }
          usedLocalizablePreferences.push(key);
        } catch (e) { /* ignore bad prefs and move on */ }
      }
    }

    if (sections["LocalizablePreferences"]) {
      for (let key of enumerate(this._ini.getKeys("LocalizablePreferences"))) {
        if (usedLocalizablePreferences.indexOf(key) > -1) {
          continue;
        }
        try {
          let value = this._ini.getString("LocalizablePreferences", key);
          if (value) {
            value = parseValue(value);
            value = value.replace(/%LOCALE%/g, this._locale);
            value = value.replace(/%LANGUAGE%/g, this._language);
            localizedStr.data = "data:text/plain," + key + "=" + value;
          }
          defaults._prefBranch.setComplexValue(key, Ci.nsIPrefLocalizedString, localizedStr);
        } catch (e) { /* ignore bad prefs and move on */ }
      }
    }

    return this._checkCustomizationComplete();
  },

  _checkCustomizationComplete: function DIST__checkCustomizationComplete() {
    const BROWSER_DOCURL = "chrome://browser/content/browser.xul";

    if (this._newProfile) {
      let xulStore = Cc["@mozilla.org/xul/xulstore;1"].getService(Ci.nsIXULStore);

      try {
        var showPersonalToolbar = Services.prefs.getBoolPref("browser.showPersonalToolbar");
        if (showPersonalToolbar) {
          xulStore.setValue(BROWSER_DOCURL, "PersonalToolbar", "collapsed", "false");
        }
      } catch (e) {}
      try {
        var showMenubar = Services.prefs.getBoolPref("browser.showMenubar");
        if (showMenubar) {
          xulStore.setValue(BROWSER_DOCURL, "toolbar-menubar", "autohide", "false");
        }
      } catch (e) {}
    }

    let prefDefaultsApplied = this._prefDefaultsApplied || !this._ini;
    if (this._customizationsApplied && this._bookmarksApplied &&
        prefDefaultsApplied) {
      let os = Cc["@mozilla.org/observer-service;1"].
               getService(Ci.nsIObserverService);
      os.notifyObservers(null, DISTRIBUTION_CUSTOMIZATION_COMPLETE_TOPIC, null);
    }
  }
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

function* enumerate(UTF8Enumerator) {
  while (UTF8Enumerator.hasMore())
    yield UTF8Enumerator.getNext();
}

function enumToObject(UTF8Enumerator) {
  let ret = {};
  for (let i of enumerate(UTF8Enumerator))
    ret[i] = 1;
  return ret;
}
