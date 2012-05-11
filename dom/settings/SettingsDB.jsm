/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

let EXPORTED_SYMBOLS = ["SettingsDB", "SETTINGSDB_NAME", "SETTINGSSTORE_NAME"];

/* static functions */
let DEBUG = 0;
if (DEBUG) {
  debug = function (s) { dump("-*- SettingsDB: " + s + "\n"); }
} else {
  debug = function (s) {}
}

const SETTINGSDB_NAME = "settings";
const SETTINGSDB_VERSION = 1;
const SETTINGSSTORE_NAME = "settings";

Components.utils.import("resource://gre/modules/IndexedDBHelper.jsm");

function SettingsDB() {}

SettingsDB.prototype = {

  __proto__: IndexedDBHelper.prototype,

  upgradeSchema: function upgradeSchema(aTransaction, aDb, aOldVersion, aNewVersion) {
    let objectStore = aDb.createObjectStore(SETTINGSSTORE_NAME, { keyPath: "settingName" });
    objectStore.createIndex("settingValue", "settingValue", { unique: false });
    debug("Created object stores and indexes");
  },

  init: function init(aGlobal) {
      this.initDBHelper(SETTINGSDB_NAME, SETTINGSDB_VERSION, SETTINGSSTORE_NAME, aGlobal);
  }
}
