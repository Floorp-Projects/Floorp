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
ChromeUtils.defineModuleGetter(
  this,
  "MacAttribution",
  "resource:///modules/MacAttribution.jsm"
);
XPCOMUtils.defineLazyGetter(this, "log", () => {
  let ConsoleAPI = ChromeUtils.import("resource://gre/modules/Console.jsm", {})
    .ConsoleAPI;
  let consoleOptions = {
    // tip: set maxLogLevel to "debug" and use log.debug() to create detailed
    // messages during development. See LOG_LEVELS in Console.jsm for details.
    maxLogLevel: "error",
    maxLogLevelPref: "browser.attribution.loglevel",
    prefix: "AttributionCode",
  };
  return new ConsoleAPI(consoleOptions);
});
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
  "dltoken",
];

let gCachedAttrData = null;

var AttributionCode = {
  /**
   * Returns a platform-specific nsIFile for the file containing the attribution
   * data, or null if the current platform does not support (caching)
   * attribution data.
   */
  get attributionFile() {
    if (AppConstants.platform == "win") {
      let file = Services.dirsvc.get("LocalAppData", Ci.nsIFile);
      // appinfo does not exist in xpcshell, so we need defaults.
      file.append(Services.appinfo.vendor || "mozilla");
      file.append(AppConstants.MOZ_APP_NAME);
      file.append("postSigningData");
      return file;
    } else if (AppConstants.platform == "macosx") {
      // There's no `UpdRootD` in xpcshell tests.  Some existing tests override
      // it, which is onerous and difficult to share across tests.  When testing,
      // if it's not defined, fallback to a nested subdirectory of the xpcshell
      // temp directory.  Nesting more closely replicates the situation where the
      // update directory does not (yet) exist, testing a scenario witnessed in
      // development.
      let file;
      try {
        file = Services.dirsvc.get("UpdRootD", Ci.nsIFile);
      } catch (ex) {
        let env = Cc["@mozilla.org/process/environment;1"].getService(
          Ci.nsIEnvironment
        );
        // It's most common to test for the profile dir, even though we actually
        // are using the temp dir.
        if (
          ex instanceof Ci.nsIException &&
          ex.result == Cr.NS_ERROR_FAILURE &&
          env.exists("XPCSHELL_TEST_PROFILE_DIR")
        ) {
          let path = env.get("XPCSHELL_TEST_TEMP_DIR");
          file = Cc["@mozilla.org/file/local;1"].createInstance(Ci.nsIFile);
          file.initWithPath(path);
          file.append("nested_UpdRootD_1");
          file.append("nested_UpdRootD_2");
        } else {
          throw ex;
        }
      }
      file.append("macAttributionData");
      return file;
    }

    return null;
  },

  /**
   * Write the given attribution code to the attribution file.
   * @param {String} code to write.
   */
  async writeAttributionFile(code) {
    let file = AttributionCode.attributionFile;
    let dir = file.parent;
    try {
      // This is a simple way to create the entire directory tree.
      // `OS.File.makeDir` has an awkward API for our situation.
      dir.create(Ci.nsIFile.DIRECTORY_TYPE, 0o755);
    } catch (ex) {
      if (ex.result != Cr.NS_ERROR_FILE_ALREADY_EXISTS) {
        throw ex;
      }
      // Ignore the exception due to a directory that already exists.
    }
    let bytes = new TextEncoder().encode(code);
    await OS.File.writeAtomic(file.path, bytes);
  },

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
        log.debug(
          `parseAttributionCode: "${code}" => isValid = false: "${key}", "${value}"`
        );
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
   * Returns an object containing a key-value pair for each piece of attribution
   * data included in the passed-in URL containing a query string encoding an
   * attribution code.
   *
   * We have less control of the attribution codes on macOS so we accept more
   * URLs than we accept attribution codes on Windows.
   *
   * If the URL is empty, returns an empty object.
   *
   * If the URL doesn't parse, throws.
   */
  parseAttributionCodeFromUrl(url) {
    if (!url) {
      return {};
    }

    let parsed = {};

    let params = new URL(url).searchParams;
    for (let key of ATTR_CODE_KEYS) {
      // We support the key prefixed with utm_ or not, but intentionally
      // choose non-utm params over utm params.
      for (let paramKey of [`utm_${key}`, `funnel_${key}`, key]) {
        if (params.has(paramKey)) {
          // We expect URI-encoded components in our attribution codes.
          let value = encodeURIComponent(params.get(paramKey));
          if (value && ATTR_CODE_VALUE_REGEX.test(value)) {
            parsed[key] = value;
          }
        }
      }
    }

    return parsed;
  },

  /**
   * Returns a string serializing the given attribution data.
   *
   * It is expected that the given values are already URL-encoded.
   */
  serializeAttributionData(data) {
    // Iterating in this way makes the order deterministic.
    let s = "";
    for (let key of ATTR_CODE_KEYS) {
      if (key in data) {
        let value = data[key];
        if (s) {
          s += ATTR_CODE_FIELD_SEPARATOR; // URL-encoded &
        }
        s += `${key}${ATTR_CODE_KEY_VALUE_SEPARATOR}${value}`; // URL-encoded =
      }
    }
    return s;
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
      log.debug(
        `getAttrDataAsync: attribution is cached: ${JSON.stringify(
          gCachedAttrData
        )}`
      );
      return gCachedAttrData;
    }

    gCachedAttrData = {};
    let attributionFile = this.attributionFile;
    if (!attributionFile) {
      // This platform doesn't support attribution.
      log.debug(`getAttrDataAsync: no attribution (attributionFile is null)`);
      return gCachedAttrData;
    }

    if (
      AppConstants.platform == "macosx" &&
      !(await OS.File.exists(attributionFile.path))
    ) {
      log.debug(
        `getAttrDataAsync: macOS && !exists("${attributionFile.path}")`
      );

      // On macOS, we fish the attribution data from the system quarantine DB.
      try {
        let referrer = await MacAttribution.getReferrerUrl();
        log.debug(
          `getAttrDataAsync: macOS attribution getReferrerUrl: "${referrer}"`
        );

        gCachedAttrData = this.parseAttributionCodeFromUrl(referrer);
      } catch (ex) {
        // Avoid partial attribution data.
        gCachedAttrData = {};

        // No attributions.  Just `warn` 'cuz this isn't necessarily an error.
        log.warn("Caught exception fetching macOS attribution codes!", ex);

        if (
          ex instanceof Ci.nsIException &&
          ex.result == Cr.NS_ERROR_UNEXPECTED
        ) {
          // Bad quarantine data.
          Services.telemetry
            .getHistogramById("BROWSER_ATTRIBUTION_ERRORS")
            .add("quarantine_error");
        }
      }

      log.debug(`macOS attribution data is ${JSON.stringify(gCachedAttrData)}`);

      // We only want to try to fetch the referrer from the quarantine
      // database once on macOS.
      try {
        let code = this.serializeAttributionData(gCachedAttrData);
        log.debug(`macOS attribution data serializes as "${code}"`);
        await this.writeAttributionFile(code);
      } catch (ex) {
        log.debug(`Caught exception writing "${attributionFile.path}"`, ex);
        Services.telemetry
          .getHistogramById("BROWSER_ATTRIBUTION_ERRORS")
          .add("write_error");
        return gCachedAttrData;
      }

      log.debug(
        `Returning after successfully writing "${attributionFile.path}"`
      );
      return gCachedAttrData;
    }

    log.debug(`getAttrDataAsync: !macOS || !exists("${attributionFile.path}")`);

    let bytes;
    try {
      bytes = await OS.File.read(attributionFile.path);
    } catch (ex) {
      if (ex instanceof OS.File.Error && ex.becauseNoSuchFile) {
        log.debug(
          `getAttrDataAsync: !exists("${
            attributionFile.path
          }"), returning ${JSON.stringify(gCachedAttrData)}`
        );
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
        log.debug(
          `getAttrDataAsync: ${attributionFile.path} deserializes to ${code}`
        );
        if (AppConstants.platform == "macosx" && !code) {
          // On macOS, an empty attribution code is fine.  (On Windows, that
          // means the stub/full installer has been incorrectly attributed,
          // which is an error.)
          return gCachedAttrData;
        }

        gCachedAttrData = this.parseAttributionCode(code);
        log.debug(
          `getAttrDataAsync: ${code} parses to ${JSON.stringify(
            gCachedAttrData
          )}`
        );
      } catch (ex) {
        // TextDecoder can throw an error
        Services.telemetry
          .getHistogramById("BROWSER_ATTRIBUTION_ERRORS")
          .add("decode_error");
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
      await OS.File.remove(this.attributionFile.path);
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
