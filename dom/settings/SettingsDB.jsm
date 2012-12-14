/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

let Cc = Components.classes;
let Ci = Components.interfaces;
let Cu = Components.utils;

this.EXPORTED_SYMBOLS = ["SettingsDB", "SETTINGSDB_NAME", "SETTINGSSTORE_NAME"];

const DEBUG = false;
function debug(s) {
  if (DEBUG) dump("-*- SettingsDB: " + s + "\n");
}

this.SETTINGSDB_NAME = "settings";
this.SETTINGSDB_VERSION = 1;
this.SETTINGSSTORE_NAME = "settings";

Cu.import("resource://gre/modules/IndexedDBHelper.jsm");
Cu.import("resource://gre/modules/FileUtils.jsm");
Cu.import("resource://gre/modules/NetUtil.jsm");

this.SettingsDB = function SettingsDB() {}

SettingsDB.prototype = {

  __proto__: IndexedDBHelper.prototype,

  upgradeSchema: function upgradeSchema(aTransaction, aDb, aOldVersion, aNewVersion) {
    let objectStore = aDb.createObjectStore(SETTINGSSTORE_NAME,
                                            { keyPath: "settingName" });
    objectStore.createIndex("settingValue", "settingValue", { unique: false });
    if (DEBUG) debug("Created object stores and indexes");

    if (aOldVersion != 0) {
      return;
    }

    // Loading resource://app/defaults/settings.json doesn't work because
    // settings.json is not in the omnijar.
    // So we look for the app dir instead and go from here...
    let settingsFile = FileUtils.getFile("DefRt", ["settings.json"], false);
    if (!settingsFile || (settingsFile && !settingsFile.exists())) {
      // On b2g desktop builds the settings.json file is moved in the
      // profile directory by the build system.
      settingsFile = FileUtils.getFile("ProfD", ["settings.json"], false);
      if (!settingsFile || (settingsFile && !settingsFile.exists())) {
        return;
      }
    }

    let chan = NetUtil.newChannel(settingsFile);
    let stream = chan.open();
    // Obtain a converter to read from a UTF-8 encoded input stream.
    let converter = Cc["@mozilla.org/intl/scriptableunicodeconverter"]
                    .createInstance(Ci.nsIScriptableUnicodeConverter);
    converter.charset = "UTF-8";
    let rawstr = converter.ConvertToUnicode(NetUtil.readInputStreamToString(
                                            stream,
                                            stream.available()) || "");
    let settings;
    try {
      settings = JSON.parse(rawstr);
    } catch(e) {
      if (DEBUG) debug("Error parsing " + settingsFile.path + " : " + e);
      return;
    }

    for (let setting in settings) {
      if (DEBUG) debug("Adding setting " + setting);
      objectStore.put({ settingName: setting,
                        settingValue: settings[setting] });
    }
  },

  init: function init(aGlobal) {
    this.initDBHelper(SETTINGSDB_NAME, SETTINGSDB_VERSION,
                      SETTINGSSTORE_NAME, aGlobal);
  }
}
