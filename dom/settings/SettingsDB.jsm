/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

let Cc = Components.classes;
let Ci = Components.interfaces;
let Cu = Components.utils;

this.EXPORTED_SYMBOLS = ["SettingsDB", "SETTINGSDB_NAME", "SETTINGSSTORE_NAME"];

const DEBUG = false;
function debug(s) {
  if (DEBUG) dump("-*- SettingsDB: " + s + "\n");
}

this.SETTINGSDB_NAME = "settings";
this.SETTINGSDB_VERSION = 2;
this.SETTINGSSTORE_NAME = "settings";

Cu.import("resource://gre/modules/IndexedDBHelper.jsm");
Cu.import("resource://gre/modules/FileUtils.jsm");
Cu.import("resource://gre/modules/NetUtil.jsm");

this.SettingsDB = function SettingsDB() {}

SettingsDB.prototype = {

  __proto__: IndexedDBHelper.prototype,

  upgradeSchema: function upgradeSchema(aTransaction, aDb, aOldVersion, aNewVersion) {
    let objectStore;
    if (aOldVersion == 0) {
      objectStore = aDb.createObjectStore(SETTINGSSTORE_NAME, { keyPath: "settingName" });
      if (DEBUG) debug("Created object stores");
    } else if (aOldVersion == 1) {
      if (DEBUG) debug("Get object store for upgrade and remove old index");
      objectStore = aTransaction.objectStore(SETTINGSSTORE_NAME);
      objectStore.deleteIndex("settingValue");
    } else {
      if (DEBUG) debug("Get object store for upgrade");
      objectStore = aTransaction.objectStore(SETTINGSSTORE_NAME);
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
    stream.close();

    objectStore.openCursor().onsuccess = function(event) {
      let cursor = event.target.result;
      if (cursor) {
        let value = cursor.value;
        if (value.settingName in settings) {
          if (DEBUG) debug("Upgrade " +settings[value.settingName]);
          value.defaultValue = settings[value.settingName];
          delete settings[value.settingName];
          if ("settingValue" in value) {
            value.userValue = value.settingValue;
            delete value.settingValue;
          }
          cursor.update(value);
        } else if ("userValue" in value || "settingValue" in value) {
          value.defaultValue = undefined;
          if (aOldVersion == 1 && value.settingValue) {
            value.userValue = value.settingValue;
            delete value.settingValue;
          }
          cursor.update(value);
        } else {
          cursor.delete();
        }
        cursor.continue();
      } else {
        for (let name in settings) {
          if (DEBUG) debug("Set new:" + name +", " + settings[name]);
          objectStore.add({ settingName: name, defaultValue: settings[name], userValue: undefined });
        }
      }
    };
  },

  init: function init(aGlobal) {
    this.initDBHelper(SETTINGSDB_NAME, SETTINGSDB_VERSION,
                      [SETTINGSSTORE_NAME], aGlobal);
  }
}
