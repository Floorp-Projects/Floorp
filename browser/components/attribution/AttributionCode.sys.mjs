/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/**
 * This is a policy object used to override behavior for testing.
 */
export const AttributionIOUtils = {
  write: async (path, bytes) => IOUtils.write(path, bytes),
  read: async path => IOUtils.read(path),
  exists: async path => IOUtils.exists(path),
};

import { AppConstants } from "resource://gre/modules/AppConstants.sys.mjs";

const lazy = {};
ChromeUtils.defineESModuleGetters(lazy, {
  MacAttribution: "resource:///modules/MacAttribution.sys.mjs",
});
ChromeUtils.defineLazyGetter(lazy, "log", () => {
  let { ConsoleAPI } = ChromeUtils.importESModule(
    "resource://gre/modules/Console.sys.mjs"
  );
  let consoleOptions = {
    // tip: set maxLogLevel to "debug" and use lazy.log.debug() to create
    // detailed messages during development. See LOG_LEVELS in Console.sys.mjs
    // for details.
    maxLogLevel: "error",
    maxLogLevelPref: "browser.attribution.loglevel",
    prefix: "AttributionCode",
  };
  return new ConsoleAPI(consoleOptions);
});

// This maximum length was originally based on how much space we have in the PE
// file header that we store attribution codes in for full and stub installers.
// Windows Store builds instead use a "Campaign ID" passed through URLs to send
// attribution information, which Microsoft's documentation claims must be no
// longer than 100 characters. In our own testing, we've been able to retrieve
// the first 208 characters of the Campaign ID. Either way, the "max" length
// for Microsoft Store builds is much lower than this limit implies.
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
  "msstoresignedin",
  "dlsource",
];

let gCachedAttrData = null;

export var AttributionCode = {
  /**
   * Wrapper to pull campaign IDs from MSIX builds.
   * This function solely exists to make it easy to mock out for tests.
   */
  async msixCampaignId() {
    const windowsPackageManager = Cc[
      "@mozilla.org/windows-package-manager;1"
    ].createInstance(Ci.nsIWindowsPackageManager);

    return windowsPackageManager.campaignId();
  },

  /**
   * Returns a platform-specific nsIFile for the file containing the attribution
   * data, or null if the current platform does not support (caching)
   * attribution data.
   */
  get attributionFile() {
    if (AppConstants.platform == "win") {
      let file = Services.dirsvc.get("GreD", Ci.nsIFile);
      file.append("postSigningData");
      return file;
    }

    return null;
  },

  /**
   * Write the given attribution code to the attribution file.
   * @param {String} code to write.
   */
  async writeAttributionFile(code) {
    // Writing attribution files is only used as part of test code
    // so bailing here for MSIX builds is no big deal.
    if (
      AppConstants.platform === "win" &&
      Services.sysinfo.getProperty("hasWinPackageId")
    ) {
      Services.console.logStringMessage(
        "Attribution code cannot be written for MSIX builds, aborting."
      );
      return;
    }
    let file = AttributionCode.attributionFile;
    await IOUtils.makeDirectory(file.parent.path);
    let bytes = new TextEncoder().encode(code);
    await AttributionIOUtils.write(file.path, bytes);
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
          if (key === "msstoresignedin") {
            if (value === "true") {
              parsed[key] = true;
            } else if (value === "false") {
              parsed[key] = false;
            } else {
              throw new Error("Couldn't parse msstoresignedin");
            }
          } else {
            parsed[key] = value;
          }
        }
      } else {
        lazy.log.debug(
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

  async _getMacAttrDataAsync() {
    // On macOS, we fish the attribution data from an extended attribute on
    // the .app bundle directory.
    try {
      let attrStr = await lazy.MacAttribution.getAttributionString();
      lazy.log.debug(
        `_getMacAttrDataAsync: getAttributionString: "${attrStr}"`
      );

      gCachedAttrData = this.parseAttributionCode(attrStr);
    } catch (ex) {
      // Avoid partial attribution data.
      gCachedAttrData = {};

      // No attributions.  Just `warn` 'cuz this isn't necessarily an error.
      lazy.log.warn("Caught exception fetching macOS attribution codes!", ex);

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

    lazy.log.debug(
      `macOS attribution data is ${JSON.stringify(gCachedAttrData)}`
    );

    return gCachedAttrData;
  },

  async _getWindowsNSISAttrDataAsync() {
    return AttributionIOUtils.read(this.attributionFile.path);
  },

  async _getWindowsMSIXAttrDataAsync() {
    // This comes out of windows-package-manager _not_ URL encoded or in an ArrayBuffer,
    // but the parsing code wants it that way. It's easier to just provide that
    // than have the parsing code support both.
    lazy.log.debug(
      `winPackageFamilyName is: ${Services.sysinfo.getProperty(
        "winPackageFamilyName"
      )}`
    );
    let encoder = new TextEncoder();
    return encoder.encode(encodeURIComponent(await this.msixCampaignId()));
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
    if (AppConstants.platform != "win" && AppConstants.platform != "macosx") {
      // This platform doesn't support attribution.
      return gCachedAttrData;
    }
    if (gCachedAttrData != null) {
      lazy.log.debug(
        `getAttrDataAsync: attribution is cached: ${JSON.stringify(
          gCachedAttrData
        )}`
      );
      return gCachedAttrData;
    }

    gCachedAttrData = {};

    if (AppConstants.platform == "macosx") {
      lazy.log.debug(`getAttrDataAsync: macOS`);
      return this._getMacAttrDataAsync();
    }

    lazy.log.debug("getAttrDataAsync: !macOS");

    let attributionFile = this.attributionFile;
    let bytes;
    try {
      if (
        AppConstants.platform === "win" &&
        Services.sysinfo.getProperty("hasWinPackageId")
      ) {
        lazy.log.debug("getAttrDataAsync: MSIX");
        bytes = await this._getWindowsMSIXAttrDataAsync();
      } else {
        lazy.log.debug("getAttrDataAsync: NSIS");
        bytes = await this._getWindowsNSISAttrDataAsync();
      }
    } catch (ex) {
      if (DOMException.isInstance(ex) && ex.name == "NotFoundError") {
        lazy.log.debug(
          `getAttrDataAsync: !exists("${
            attributionFile.path
          }"), returning ${JSON.stringify(gCachedAttrData)}`
        );
        return gCachedAttrData;
      }
      lazy.log.debug(
        `other error trying to read attribution data:
          attributionFile.path is: ${attributionFile.path}`
      );
      lazy.log.debug("Full exception is:");
      lazy.log.debug(ex);

      Services.telemetry
        .getHistogramById("BROWSER_ATTRIBUTION_ERRORS")
        .add("read_error");
    }
    if (bytes) {
      try {
        let decoder = new TextDecoder();
        let code = decoder.decode(bytes);
        lazy.log.debug(
          `getAttrDataAsync: attribution bytes deserializes to ${code}`
        );
        if (AppConstants.platform == "macosx" && !code) {
          // On macOS, an empty attribution code is fine.  (On Windows, that
          // means the stub/full installer has been incorrectly attributed,
          // which is an error.)
          return gCachedAttrData;
        }

        gCachedAttrData = this.parseAttributionCode(code);
        lazy.log.debug(
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
    // There is no cache file on macOS
    if (AppConstants.platform == "win") {
      try {
        await IOUtils.remove(this.attributionFile.path);
      } catch (ex) {
        // The attribution file may already have been deleted,
        // or it may have never been installed at all;
        // failure to delete it isn't an error.
      }
    }
  },

  /**
   * Clears the cached attribution code value, if any.
   * Does nothing if called from outside of an xpcshell test.
   */
  _clearCache() {
    if (Services.env.exists("XPCSHELL_TEST_PROFILE_DIR")) {
      gCachedAttrData = null;
    }
  },
};
