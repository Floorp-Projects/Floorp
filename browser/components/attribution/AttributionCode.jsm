/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
"use strict";

var EXPORTED_SYMBOLS = ["AttributionCode"];

const { XPCOMUtils } = ChromeUtils.import(
  "resource://gre/modules/XPCOMUtils.jsm"
);
ChromeUtils.defineModuleGetter(
  this,
  "AppConstants",
  "resource://gre/modules/AppConstants.jsm"
);
ChromeUtils.defineModuleGetter(this, "OS", "resource://gre/modules/osfile.jsm");
ChromeUtils.defineModuleGetter(
  this,
  "Services",
  "resource://gre/modules/Services.jsm"
);
XPCOMUtils.defineLazyGlobalGetters(this, ["URL"]);

const ATTR_CODE_MAX_LENGTH = 1010;
const ATTR_CODE_VALUE_REGEX = /[a-zA-Z0-9_%\\-\\.\\(\\)]*/;
const ATTR_CODE_FIELD_SEPARATOR = "%26"; // URL-encoded &
const ATTR_CODE_KEY_VALUE_SEPARATOR = "%3D"; // URL-encoded =
const ATTR_CODE_KEYS = [
  "source",
  "medium",
  "campaign",
  "content",
  "experiment",
  "variation",
  "ua",
];

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

var AttributionCode = {
  /**
   * Returns an array of allowed attribution code keys.
   */
  get allowedCodeKeys() {
    return [...ATTR_CODE_KEYS];
  },

  /**
   * Returns an object containing a key-value pair for each piece of attribution
   * data included in the passed-in attribution code string.
   * If the string isn't a valid attribution code, returns an empty object.
   */
  parseAttributionCode(code) {
    if (code.length > ATTR_CODE_MAX_LENGTH) {
      return {};
    }

    let isValid = true;
    let parsed = {};
    for (let param of code.split(ATTR_CODE_FIELD_SEPARATOR)) {
      let [key, value] = param.split(ATTR_CODE_KEY_VALUE_SEPARATOR, 2);
      if (key && ATTR_CODE_KEYS.includes(key)) {
        if (value && ATTR_CODE_VALUE_REGEX.test(value)) {
          parsed[key] = value;
        }
      } else {
        isValid = false;
        break;
      }
    }

    if (isValid) {
      return parsed;
    }

    Services.telemetry
      .getHistogramById("BROWSER_ATTRIBUTION_ERRORS")
      .add("decode_error");

    return {};
  },

  /**
   * Reads the attribution code, either from disk or a cached version.
   * Returns a promise that fulfills with an object containing the parsed
   * attribution data if the code could be read and is valid,
   * or an empty object otherwise.
   *
   * On windows the attribution service converts utm_* keys, removing "utm_".
   * On OSX the attributions are set directly on download and retain "utm_".  We
   * strip "utm_" while retrieving the params.
   */
  async getAttrDataAsync() {
    if (gCachedAttrData != null) {
      return gCachedAttrData;
    }

    gCachedAttrData = {};
    if (AppConstants.platform == "win") {
      let bytes;
      try {
        bytes = await OS.File.read(getAttributionFile().path);
      } catch (ex) {
        if (ex instanceof OS.File.Error && ex.becauseNoSuchFile) {
          return gCachedAttrData;
        }
        Services.telemetry
          .getHistogramById("BROWSER_ATTRIBUTION_ERRORS")
          .add("read_error");
      }
      if (bytes) {
        try {
          let decoder = new TextDecoder();
          let code = decoder.decode(bytes);
          gCachedAttrData = this.parseAttributionCode(code);
        } catch (ex) {
          // TextDecoder can throw an error
          Services.telemetry
            .getHistogramById("BROWSER_ATTRIBUTION_ERRORS")
            .add("decode_error");
        }
      }
    } else if (AppConstants.platform == "macosx") {
      try {
        let appPath = Services.dirsvc.get("GreD", Ci.nsIFile).parent.parent
          .path;
        let attributionSvc = Cc["@mozilla.org/mac-attribution;1"].getService(
          Ci.nsIMacAttributionService
        );
        let referrer = attributionSvc.getReferrerUrl(appPath);
        let params = new URL(referrer).searchParams;
        for (let key of ATTR_CODE_KEYS) {
          // We support the key prefixed with utm_ or not, but intentionally
          // choose non-utm params over utm params.
          for (let paramKey of [`utm_${key}`, `funnel_${key}`, key]) {
            if (params.has(paramKey)) {
              let value = params.get(paramKey);
              if (value && ATTR_CODE_VALUE_REGEX.test(value)) {
                gCachedAttrData[key] = value;
              }
            }
          }
        }
      } catch (ex) {
        // No attributions
      }
    }
    return gCachedAttrData;
  },

  /**
   * Return the cached attribution data synchronously without hitting
   * the disk.
   * @returns A dictionary with the attribution data if it's available,
   *          null otherwise.
   */
  getCachedAttributionData() {
    return gCachedAttrData;
  },

  /**
   * Deletes the attribution data file.
   * Returns a promise that resolves when the file is deleted,
   * or if the file couldn't be deleted (the promise is never rejected).
   */
  async deleteFileAsync() {
    try {
      await OS.File.remove(getAttributionFile().path);
    } catch (ex) {
      // The attribution file may already have been deleted,
      // or it may have never been installed at all;
      // failure to delete it isn't an error.
    }
  },

  /**
   * Clears the cached attribution code value, if any.
   * Does nothing if called from outside of an xpcshell test.
   */
  _clearCache() {
    let env = Cc["@mozilla.org/process/environment;1"].getService(
      Ci.nsIEnvironment
    );
    if (env.exists("XPCSHELL_TEST_PROFILE_DIR")) {
      gCachedAttrData = null;
    }
  },
};
