/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

let Cc = Components.classes;
let Ci = Components.interfaces;
let Cu = Components.utils;

Cu.import("resource://gre/modules/Services.jsm");
Cu.import("resource://gre/modules/ObjectWrapper.jsm");

this.EXPORTED_SYMBOLS = ["SettingsDB", "SETTINGSDB_NAME", "SETTINGSSTORE_NAME"];

const DEBUG = false;
function debug(s) {
  if (DEBUG) dump("-*- SettingsDB: " + s + "\n");
}

this.SETTINGSDB_NAME = "settings";
this.SETTINGSDB_VERSION = 3;
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
          value.defaultValue = this.prepareValue(settings[value.settingName]);
          delete settings[value.settingName];
          if ("settingValue" in value) {
            value.userValue = this.prepareValue(value.settingValue);
            delete value.settingValue;
          }
          cursor.update(value);
        } else if ("userValue" in value || "settingValue" in value) {
          value.defaultValue = undefined;
          if (aOldVersion == 1 && value.settingValue) {
            value.userValue = this.prepareValue(value.settingValue);
            delete value.settingValue;
          }
          cursor.update(value);
        } else {
          cursor.delete();
        }
        cursor.continue();
      } else {
        for (let name in settings) {
          let value = this.prepareValue(settings[name]);
          if (DEBUG) debug("Set new:" + name +", " + value);
          objectStore.add({ settingName: name, defaultValue: value, userValue: undefined });
        }
      }
    }.bind(this);
  },

  // If the value is a data: uri, convert it to a Blob.
  convertDataURIToBlob: function(aValue) {
    /* base64 to ArrayBuffer decoding, from
       https://developer.mozilla.org/en-US/docs/Web/JavaScript/Base64_encoding_and_decoding
    */
    function b64ToUint6 (nChr) {
      return nChr > 64 && nChr < 91 ?
          nChr - 65
        : nChr > 96 && nChr < 123 ?
          nChr - 71
        : nChr > 47 && nChr < 58 ?
          nChr + 4
        : nChr === 43 ?
          62
        : nChr === 47 ?
          63
        :
          0;
    }

    function base64DecToArr(sBase64, nBlocksSize) {
      let sB64Enc = sBase64.replace(/[^A-Za-z0-9\+\/]/g, ""),
          nInLen = sB64Enc.length,
          nOutLen = nBlocksSize ? Math.ceil((nInLen * 3 + 1 >> 2) / nBlocksSize) * nBlocksSize
                                : nInLen * 3 + 1 >> 2,
          taBytes = new Uint8Array(nOutLen);

      for (let nMod3, nMod4, nUint24 = 0, nOutIdx = 0, nInIdx = 0; nInIdx < nInLen; nInIdx++) {
        nMod4 = nInIdx & 3;
        nUint24 |= b64ToUint6(sB64Enc.charCodeAt(nInIdx)) << 18 - 6 * nMod4;
        if (nMod4 === 3 || nInLen - nInIdx === 1) {
          for (nMod3 = 0; nMod3 < 3 && nOutIdx < nOutLen; nMod3++, nOutIdx++) {
            taBytes[nOutIdx] = nUint24 >>> (16 >>> nMod3 & 24) & 255;
          }
          nUint24 = 0;
        }
      }
      return taBytes;
    }

    // Check if we have a data: uri, and if it's base64 encoded.
    // data:image/jpeg;base64,/9j/4AAQSkZJRgABAQEA...
    if (typeof aValue == "string" && aValue.startsWith("data:")) {
      try {
        let uri = Services.io.newURI(aValue, null, null);
        // XXX: that would be nice to reuse the c++ bits of the data:
        // protocol handler instead.
        let mimeType = "application/octet-stream";
        let mimeDelim = aValue.indexOf(";");
        if (mimeDelim !== -1) {
          mimeType = aValue.substring(5, mimeDelim);
        }
        let start = aValue.indexOf(",") + 1;
        let isBase64 = ((aValue.indexOf("base64") + 7) == start);
        let payload = aValue.substring(start);

        return new Blob([isBase64 ? base64DecToArr(payload) : payload],
                        { type: mimeType });
      } catch(e) {
        dump(e);
      }
    }
    return aValue
  },

  // Makes sure any property that is a data: uri gets converted to a Blob.
  prepareValue: function(aObject) {
    let kind = ObjectWrapper.getObjectKind(aObject);
    if (kind == "array") {
      let res = [];
      aObject.forEach(function(aObj) {
        res.push(this.prepareValue(aObj));
      }, this);
      return res;
    } else if (kind == "file" || kind == "blob" || kind == "date") {
      return aObject;
    } else if (kind == "primitive") {
      return this.convertDataURIToBlob(aObject);
    }

    // Fall-through, we now have a dictionary object.
    for (let prop in aObject) {
      aObject[prop] = this.prepareValue(aObject[prop]);
    }
    return aObject;
  },

  init: function init() {
    this.initDBHelper(SETTINGSDB_NAME, SETTINGSDB_VERSION,
                      [SETTINGSSTORE_NAME]);
  }
}
