/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
"use strict";

var EXPORTED_SYMBOLS = ["MacAttribution"];

const { XPCOMUtils } = ChromeUtils.importESModule(
  "resource://gre/modules/XPCOMUtils.sys.mjs"
);
const lazy = {};
XPCOMUtils.defineLazyGetter(lazy, "log", () => {
  let { ConsoleAPI } = ChromeUtils.import("resource://gre/modules/Console.jsm");
  let consoleOptions = {
    // tip: set maxLogLevel to "debug" and use lazy.log.debug() to create
    // detailed messages during development. See LOG_LEVELS in Console.jsm for
    // details.
    maxLogLevel: "error",
    maxLogLevelPref: "browser.attribution.mac.loglevel",
    prefix: "MacAttribution",
  };
  return new ConsoleAPI(consoleOptions);
});

ChromeUtils.defineModuleGetter(
  lazy,
  "Subprocess",
  "resource://gre/modules/Subprocess.jsm"
);

/**
 * Get the location of the user's macOS quarantine database.
 * @return {String} path.
 */
function getQuarantineDatabasePath() {
  let file = Services.dirsvc.get("Home", Ci.nsIFile);
  file.append("Library");
  file.append("Preferences");
  file.append("com.apple.LaunchServices.QuarantineEventsV2");
  return file.path;
}

/**
 * Query given path for quarantine extended attributes.
 * @param {String} path of the file to query.
 * @return {[String, String]} pair of the quarantine data GUID and remaining
 *                            quarantine data (usually, Gatekeeper flags).
 * @throws NS_ERROR_NOT_AVAILABLE if there is no quarantine GUID for the given path.
 * @throws NS_ERROR_UNEXPECTED if there is a quarantine GUID, but it is malformed.
 */
async function getQuarantineAttributes(path) {
  let bytes = await IOUtils.getMacXAttr(path, "com.apple.quarantine");
  if (!bytes) {
    throw new Components.Exception(
      `No macOS quarantine xattrs found for ${path}`,
      Cr.NS_ERROR_NOT_AVAILABLE
    );
  }

  let string = new TextDecoder("utf-8").decode(bytes);
  let parts = string.split(";");
  if (!parts.length) {
    throw new Components.Exception(
      `macOS quarantine data is not ; separated`,
      Cr.NS_ERROR_UNEXPECTED
    );
  }
  let guid = parts[parts.length - 1];
  if (guid.length != 36) {
    // Like "12345678-90AB-CDEF-1234-567890ABCDEF".
    throw new Components.Exception(
      `macOS quarantine data guid is not length 36: ${guid.length}`,
      Cr.NS_ERROR_UNEXPECTED
    );
  }

  return { guid, parts };
}

/**
 * Invoke system SQLite binary to extract the referrer URL corresponding to
 * the given GUID from the given macOS quarantine database.
 * @param {String} path of the user's macOS quarantine database.
 * @param {String} guid to query.
 * @return {String} referrer URL.
 */
async function queryQuarantineDatabase(
  guid,
  path = getQuarantineDatabasePath()
) {
  let query = `SELECT COUNT(*), LSQuarantineOriginURLString
       FROM LSQuarantineEvent
       WHERE LSQuarantineEventIdentifier = '${guid}'
       ORDER BY LSQuarantineTimeStamp DESC LIMIT 1`;

  let proc = await lazy.Subprocess.call({
    command: "/usr/bin/sqlite3",
    arguments: [path, query],
    environment: {},
    stderr: "stdout",
  });

  let stdout = await proc.stdout.readString();

  let { exitCode } = await proc.wait();
  if (exitCode != 0) {
    throw new Components.Exception(
      "Failed to run sqlite3",
      Cr.NS_ERROR_UNEXPECTED
    );
  }

  // Output is like "integer|url".
  let parts = stdout.split("|", 2);
  if (parts.length != 2) {
    throw new Components.Exception(
      "Failed to parse sqlite3 output",
      Cr.NS_ERROR_UNEXPECTED
    );
  }

  if (parts[0].trim() == "0") {
    throw new Components.Exception(
      `Quarantine database does not contain URL for guid ${guid}`,
      Cr.NS_ERROR_UNEXPECTED
    );
  }

  return parts[1].trim();
}

var MacAttribution = {
  /**
   * The file path to the `.app` directory.
   */
  get applicationPath() {
    // On macOS, `GreD` is like "App.app/Contents/macOS".  Return "App.app".
    return Services.dirsvc.get("GreD", Ci.nsIFile).parent.parent.path;
  },

  /**
   * Used by the Attributions system to get the download referrer.
   *
   * @param {String} path to get the quarantine data from.
   *             Usually this is a `.app` directory but can be any
   *             (existing) file or directory.  Default: `this.applicationPath`.
   * @return {String} referrer URL.
   * @throws NS_ERROR_NOT_AVAILABLE if there is no quarantine GUID for the given path.
   * @throws NS_ERROR_UNEXPECTED if there is a quarantine GUID, but no corresponding referrer URL is known.
   */
  async getReferrerUrl(path = this.applicationPath) {
    lazy.log.debug(`getReferrerUrl(${JSON.stringify(path)})`);

    // First, determine the quarantine GUID assigned by macOS to the given path.
    let guid;
    try {
      guid = (await getQuarantineAttributes(path)).guid;
    } catch (ex) {
      throw new Components.Exception(
        `No macOS quarantine GUID found for ${path}`,
        Cr.NS_ERROR_NOT_AVAILABLE
      );
    }
    lazy.log.debug(`getReferrerUrl: guid: ${guid}`);

    // Second, fish the relevant record from the quarantine database.
    let url = "";
    try {
      url = await queryQuarantineDatabase(guid);
      lazy.log.debug(`getReferrerUrl: url: ${url}`);
    } catch (ex) {
      // This path is known to macOS but we failed to extract a referrer -- be noisy.
      throw new Components.Exception(
        `No macOS quarantine referrer URL found for ${path} with GUID ${guid}`,
        Cr.NS_ERROR_UNEXPECTED
      );
    }

    return url;
  },
};
