/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

this.EXPORTED_SYMBOLS = [ "DistributionCustomizer" ];

const Ci = Components.interfaces;
const Cc = Components.classes;
const Cr = Components.results;
const Cu = Components.utils;

const DISTRIBUTION_CUSTOMIZATION_COMPLETE_TOPIC =
  "distribution-customization-complete";

Cu.import("resource://gre/modules/XPCOMUtils.jsm");
Cu.import("resource://gre/modules/Services.jsm");
XPCOMUtils.defineLazyModuleGetter(this, "PlacesUtils",
                                  "resource://gre/modules/PlacesUtils.jsm");

this.DistributionCustomizer = function DistributionCustomizer() {
  // For parallel xpcshell testing purposes allow loading the distribution.ini
  // file from the profile folder through an hidden pref.
  let loadFromProfile = false;
  try {
    loadFromProfile = Services.prefs.getBoolPref("distribution.testing.loadFromProfile");
  } catch(ex) {}
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
  } catch(ex) {}
}

DistributionCustomizer.prototype = {
  _iniFile: null,

  get _ini() {
    let ini = Cc["@mozilla.org/xpcom/ini-parser-factory;1"].
              getService(Ci.nsIINIParserFactory).
              createINIParser(this._iniFile);
    this.__defineGetter__("_ini", function() ini);
    return this._ini;
  },

  get _locale() {
    let locale;
    try {
      locale = this._prefs.getCharPref("general.useragent.locale");
    }
    catch (e) {
      locale = "en-US";
    }
    this.__defineGetter__("_locale", function() locale);
    return this._locale;
  },

  get _prefSvc() {
    let svc = Cc["@mozilla.org/preferences-service;1"].
              getService(Ci.nsIPrefService);
    this.__defineGetter__("_prefSvc", function() svc);
    return this._prefSvc;
  },

  get _prefs() {
    let branch = this._prefSvc.getBranch(null);
    this.__defineGetter__("_prefs", function() branch);
    return this._prefs;
  },

  get _ioSvc() {
    let svc = Cc["@mozilla.org/network/io-service;1"].
              getService(Ci.nsIIOService);
    this.__defineGetter__("_ioSvc", function() svc);
    return this._ioSvc;
  },

  _makeURI: function DIST__makeURI(spec) {
    return this._ioSvc.newURI(spec, null, null);
  },

  _parseBookmarksSection:
  function DIST_parseBookmarksSection(parentId, section) {
    let keys = [];
    for (let i in enumerate(this._ini.getKeys(section)))
      keys.push(i);
    keys.sort();

    let items = {};
    let defaultItemId = -1;
    let maxItemId = -1;

    for (let i = 0; i < keys.length; i++) {
      let m = /^item\.(\d+)\.(\w+)\.?(\w*)/.exec(keys[i]);
      if (m) {
        let [foo, iid, iprop, ilocale] = m;
        iid = parseInt(iid);

        if (ilocale)
          continue;

        if (!items[iid])
          items[iid] = {};
        if (keys.indexOf(keys[i] + "." + this._locale) >= 0) {
          items[iid][iprop] = this._ini.getString(section, keys[i] + "." +
                                                  this._locale);
        } else {
          items[iid][iprop] = this._ini.getString(section, keys[i]);
        }

        if (iprop == "type" && items[iid]["type"] == "default")
          defaultItemId = iid;

        if (maxItemId < iid)
          maxItemId = iid;
      } else {
        dump("Key did not match: " + keys[i] + "\n");
      }
    }

    let prependIndex = 0;
    for (let iid = 0; iid <= maxItemId; iid++) {
      if (!items[iid])
        continue;

      let index = PlacesUtils.bookmarks.DEFAULT_INDEX;
      let newId;

      switch (items[iid]["type"]) {
      case "default":
        break;

      case "folder":
        if (iid < defaultItemId)
          index = prependIndex++;

        newId = PlacesUtils.bookmarks.createFolder(parentId,
                                                   items[iid]["title"],
                                                   index);

        this._parseBookmarksSection(newId, "BookmarksFolder-" +
                                    items[iid]["folderId"]);

        if (items[iid]["description"])
          PlacesUtils.annotations.setItemAnnotation(newId,
                                                    "bookmarkProperties/description",
                                                    items[iid]["description"], 0,
                                                    PlacesUtils.annotations.EXPIRE_NEVER);

        break;

      case "separator":
        if (iid < defaultItemId)
          index = prependIndex++;
        PlacesUtils.bookmarks.insertSeparator(parentId, index);
        break;

      case "livemark":
        if (iid < defaultItemId)
          index = prependIndex++;

        // Don't bother updating the livemark contents on creation.
        PlacesUtils.livemarks.addLivemark({ title: items[iid]["title"]
                                          , parentId: parentId
                                          , index: index
                                          , feedURI: this._makeURI(items[iid]["feedLink"])
                                          , siteURI: this._makeURI(items[iid]["siteLink"])
                                          }).then(null, Cu.reportError);
        break;

      case "bookmark":
      default:
        if (iid < defaultItemId)
          index = prependIndex++;

        newId = PlacesUtils.bookmarks.insertBookmark(parentId,
                                                     this._makeURI(items[iid]["link"]),
                                                     index, items[iid]["title"]);

        if (items[iid]["description"])
          PlacesUtils.annotations.setItemAnnotation(newId,
                                                    "bookmarkProperties/description",
                                                    items[iid]["description"], 0,
                                                    PlacesUtils.annotations.EXPIRE_NEVER);

        break;
      }
    }
  },

  _customizationsApplied: false,
  applyCustomizations: function DIST_applyCustomizations() {
    this._customizationsApplied = true;
    if (!this._iniFile)
      return this._checkCustomizationComplete();

    // nsPrefService loads very early.  Reload prefs so we can set
    // distribution defaults during the prefservice:after-app-defaults
    // notification (see applyPrefDefaults below)
    this._prefSvc.QueryInterface(Ci.nsIObserver);
    this._prefSvc.observe(null, "reload-default-prefs", null);
  },

  _bookmarksApplied: false,
  applyBookmarks: function DIST_applyBookmarks() {
    this._bookmarksApplied = true;
    if (!this._iniFile)
      return this._checkCustomizationComplete();

    let sections = enumToObject(this._ini.getSections());

    // The global section, and several of its fields, is required
    // (we also check here to be consistent with applyPrefDefaults below)
    if (!sections["Global"])
      return this._checkCustomizationComplete();
    let globalPrefs = enumToObject(this._ini.getKeys("Global"));
    if (!(globalPrefs["id"] && globalPrefs["version"] && globalPrefs["about"]))
      return this._checkCustomizationComplete();

    let bmProcessedPref;
    try {
      bmProcessedPref = this._ini.getString("Global",
                                            "bookmarks.initialized.pref");
    }
    catch (e) {
      bmProcessedPref = "distribution." +
        this._ini.getString("Global", "id") + ".bookmarksProcessed";
    }

    let bmProcessed = false;
    try {
      bmProcessed = this._prefs.getBoolPref(bmProcessedPref);
    }
    catch (e) {}

    if (!bmProcessed) {
      if (sections["BookmarksMenu"])
        this._parseBookmarksSection(PlacesUtils.bookmarksMenuFolderId,
                                    "BookmarksMenu");
      if (sections["BookmarksToolbar"])
        this._parseBookmarksSection(PlacesUtils.toolbarFolderId,
                                    "BookmarksToolbar");
      this._prefs.setBoolPref(bmProcessedPref, true);
    }
    return this._checkCustomizationComplete();
  },

  _prefDefaultsApplied: false,
  applyPrefDefaults: function DIST_applyPrefDefaults() {
    this._prefDefaultsApplied = true;
    if (!this._iniFile)
      return this._checkCustomizationComplete();

    let sections = enumToObject(this._ini.getSections());

    // The global section, and several of its fields, is required
    if (!sections["Global"])
      return this._checkCustomizationComplete();
    let globalPrefs = enumToObject(this._ini.getKeys("Global"));
    if (!(globalPrefs["id"] && globalPrefs["version"] && globalPrefs["about"]))
      return this._checkCustomizationComplete();

    let defaults = this._prefSvc.getDefaultBranch(null);

    // Global really contains info we set as prefs.  They're only
    // separate because they are "special" (read: required)

    defaults.setCharPref("distribution.id", this._ini.getString("Global", "id"));
    defaults.setCharPref("distribution.version",
                         this._ini.getString("Global", "version"));

    let partnerAbout = Cc["@mozilla.org/supports-string;1"].
      createInstance(Ci.nsISupportsString);
    try {
      if (globalPrefs["about." + this._locale]) {
        partnerAbout.data = this._ini.getString("Global", "about." + this._locale);
      } else {
        partnerAbout.data = this._ini.getString("Global", "about");
      }
      defaults.setComplexValue("distribution.about",
                               Ci.nsISupportsString, partnerAbout);
    } catch (e) {
      /* ignore bad prefs due to bug 895473 and move on */
      Cu.reportError(e);
    }

    if (sections["Preferences"]) {
      for (let key in enumerate(this._ini.getKeys("Preferences"))) {
        try {
          let value = eval(this._ini.getString("Preferences", key));
          switch (typeof value) {
          case "boolean":
            defaults.setBoolPref(key, value);
            break;
          case "number":
            defaults.setIntPref(key, value);
            break;
          case "string":
            defaults.setCharPref(key, value);
            break;
          case "undefined":
            defaults.setCharPref(key, value);
            break;
          }
        } catch (e) { /* ignore bad prefs and move on */ }
      }
    }

    // We eval() the localizable prefs as well (even though they'll
    // always get set as a string) to keep the INI format consistent:
    // string prefs always need to be in quotes

    let localizedStr = Cc["@mozilla.org/pref-localizedstring;1"].
      createInstance(Ci.nsIPrefLocalizedString);

    if (sections["LocalizablePreferences"]) {
      for (let key in enumerate(this._ini.getKeys("LocalizablePreferences"))) {
        try {
          let value = eval(this._ini.getString("LocalizablePreferences", key));
          value = value.replace("%LOCALE%", this._locale, "g");
          localizedStr.data = "data:text/plain," + key + "=" + value;
          defaults.setComplexValue(key, Ci.nsIPrefLocalizedString, localizedStr);
        } catch (e) { /* ignore bad prefs and move on */ }
      }
    }

    if (sections["LocalizablePreferences-" + this._locale]) {
      for (let key in enumerate(this._ini.getKeys("LocalizablePreferences-" + this._locale))) {
        try {
          let value = eval(this._ini.getString("LocalizablePreferences-" + this._locale, key));
          localizedStr.data = "data:text/plain," + key + "=" + value;
          defaults.setComplexValue(key, Ci.nsIPrefLocalizedString, localizedStr);
        } catch (e) { /* ignore bad prefs and move on */ }
      }
    }

    return this._checkCustomizationComplete();
  },

  _checkCustomizationComplete: function DIST__checkCustomizationComplete() {
    let prefDefaultsApplied = this._prefDefaultsApplied || !this._iniFile;
    if (this._customizationsApplied && this._bookmarksApplied &&
        prefDefaultsApplied) {
      let os = Cc["@mozilla.org/observer-service;1"].
               getService(Ci.nsIObserverService);
      os.notifyObservers(null, DISTRIBUTION_CUSTOMIZATION_COMPLETE_TOPIC, null);
    }
  }
};

function enumerate(UTF8Enumerator) {
  while (UTF8Enumerator.hasMore())
    yield UTF8Enumerator.getNext();
}

function enumToObject(UTF8Enumerator) {
  let ret = {};
  for (let i in enumerate(UTF8Enumerator))
    ret[i] = 1;
  return ret;
}
