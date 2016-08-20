/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
"use strict";

this.EXPORTED_SYMBOLS = ["AttributionCode"];

const {classes: Cc, interfaces: Ci, utils: Cu} = Components;

Cu.import("resource://gre/modules/XPCOMUtils.jsm");
XPCOMUtils.defineLazyModuleGetter(this, 'AppConstants',
  'resource://gre/modules/AppConstants.jsm');
XPCOMUtils.defineLazyModuleGetter(this, 'OS',
  'resource://gre/modules/osfile.jsm');
XPCOMUtils.defineLazyModuleGetter(this, 'Services',
  'resource://gre/modules/Services.jsm');
XPCOMUtils.defineLazyModuleGetter(this, 'Task',
  'resource://gre/modules/Task.jsm');

const ATTR_CODE_MAX_LENGTH = 200;
const ATTR_CODE_KEYS_REGEX = /^source|medium|campaign|content$/;
const ATTR_CODE_VALUE_REGEX = /[a-zA-Z0-9_%\\-\\.\\(\\)]*/;
const ATTR_CODE_FIELD_SEPARATOR = "%26"; // URL-encoded &
const ATTR_CODE_KEY_VALUE_SEPARATOR = "%3D"; // URL-encoded =

let gCachedAttrData = null;

/**
 * Returns an nsIFile for the file containing the attribution data.
 */
function getAttributionFile() {
  let file = Services.dirsvc.get("LocalAppData", Ci.nsIFile);
  // appinfo does not exist in xpcshell, so we need defaults.
  file.append(Services.appinfo.vendor || "mozilla");
  file.append(AppConstants.MOZ_APP_NAME);
  file.append("postSigningData");
  return file;
}

/**
 * Returns an object containing a key-value pair for each piece of attribution
 * data included in the passed-in attribution code string.
 * If the string isn't a valid attribution code, returns an empty object.
 */
function parseAttributionCode(code) {
  if (code.length > ATTR_CODE_MAX_LENGTH) {
    return {};
  }

  let isValid = true;
  let parsed = {};
  for (let param of code.split(ATTR_CODE_FIELD_SEPARATOR)) {
    let [key, value] = param.split(ATTR_CODE_KEY_VALUE_SEPARATOR, 2);
    if (key && ATTR_CODE_KEYS_REGEX.test(key)) {
      if (value && ATTR_CODE_VALUE_REGEX.test(value)) {
        parsed[key] = value;
      }
    } else {
      isValid = false;
      break;
    }
  }
  return isValid ? parsed : {};
}

var AttributionCode = {
  /**
   * Reads the attribution code, either from disk or a cached version.
   * Returns a promise that fulfills with an object containing the parsed
   * attribution data if the code could be read and is valid,
   * or an empty object otherwise.
   */
  getAttrDataAsync() {
    return Task.spawn(function*() {
      if (gCachedAttrData != null) {
        return gCachedAttrData;
      }

      let code = "";
      try {
        let bytes = yield OS.File.read(getAttributionFile().path);
        let decoder = new TextDecoder();
        code = decoder.decode(bytes);
      } catch (ex) {
        // The attribution file may already have been deleted,
        // or it may have never been installed at all;
        // failure to open or read it isn't an error.
      }

      gCachedAttrData = parseAttributionCode(code);
      return gCachedAttrData;
    });
  },

  /**
   * Deletes the attribution data file.
   * Returns a promise that resolves when the file is deleted,
   * or if the file couldn't be deleted (the promise is never rejected).
   */
  deleteFileAsync() {
    return Task.spawn(function*() {
      try {
        yield OS.File.remove(getAttributionFile().path);
      } catch (ex) {
        // The attribution file may already have been deleted,
        // or it may have never been installed at all;
        // failure to delete it isn't an error.
      }
    });
  },

  /**
   * Clears the cached attribution code value, if any.
   * Does nothing if called from outside of an xpcshell test.
   */
  _clearCache() {
    let env = Cc["@mozilla.org/process/environment;1"]
              .getService(Ci.nsIEnvironment);
    if (env.exists("XPCSHELL_TEST_PROFILE_DIR")) {
      gCachedAttrData = null;
    }
  },
};
