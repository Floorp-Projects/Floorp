/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
"use strict";

var EXPORTED_SYMBOLS = ["ChromeMigrationUtils"];

const { XPCOMUtils } = ChromeUtils.importESModule(
  "resource://gre/modules/XPCOMUtils.sys.mjs"
);
const { AppConstants } = ChromeUtils.import(
  "resource://gre/modules/AppConstants.jsm"
);
const lazy = {};
XPCOMUtils.defineLazyModuleGetters(lazy, {
  LoginHelper: "resource://gre/modules/LoginHelper.jsm",
  MigrationUtils: "resource:///modules/MigrationUtils.jsm",
  OS: "resource://gre/modules/osfile.jsm",
});

const S100NS_FROM1601TO1970 = 0x19db1ded53e8000;
const S100NS_PER_MS = 10;

var ChromeMigrationUtils = {
  // Supported browsers with importable logins.
  CONTEXTUAL_LOGIN_IMPORT_BROWSERS: ["chrome", "chromium-edge", "chromium"],

  _extensionVersionDirectoryNames: {},

  // The cache for the locale strings.
  // For example, the data could be:
  // {
  //   "profile-id-1": {
  //     "extension-id-1": {
  //       "name": {
  //         "message": "Fake App 1"
  //       }
  //   },
  // }
  _extensionLocaleStrings: {},

  get supportsLoginsForPlatform() {
    return ["macosx", "win"].includes(AppConstants.platform);
  },

  /**
   * Get all extensions installed in a specific profile.
   * @param {String} profileId - A Chrome user profile ID. For example, "Profile 1".
   * @returns {Array} All installed Chrome extensions information.
   */
  async getExtensionList(profileId) {
    if (profileId === undefined) {
      profileId = await this.getLastUsedProfileId();
    }
    let path = this.getExtensionPath(profileId);
    let iterator = new lazy.OS.File.DirectoryIterator(path);
    let extensionList = [];
    await iterator
      .forEach(async entry => {
        if (entry.isDir) {
          let extensionInformation = await this.getExtensionInformation(
            entry.name,
            profileId
          );
          if (extensionInformation) {
            extensionList.push(extensionInformation);
          }
        }
      })
      .catch(ex => Cu.reportError(ex));
    return extensionList;
  },

  /**
   * Get information of a specific Chrome extension.
   * @param {String} extensionId - The extension ID.
   * @param {String} profileId - The user profile's ID.
   * @retruns {Object} The Chrome extension information.
   */
  async getExtensionInformation(extensionId, profileId) {
    if (profileId === undefined) {
      profileId = await this.getLastUsedProfileId();
    }
    let extensionInformation = null;
    try {
      let manifestPath = this.getExtensionPath(profileId);
      manifestPath = lazy.OS.Path.join(manifestPath, extensionId);
      // If there are multiple sub-directories in the extension directory,
      // read the files in the latest directory.
      let directories = await this._getSortedByVersionSubDirectoryNames(
        manifestPath
      );
      if (!directories[0]) {
        return null;
      }

      manifestPath = lazy.OS.Path.join(
        manifestPath,
        directories[0],
        "manifest.json"
      );
      let manifest = await lazy.OS.File.read(manifestPath, {
        encoding: "utf-8",
      });
      manifest = JSON.parse(manifest);
      // No app attribute means this is a Chrome extension not a Chrome app.
      if (!manifest.app) {
        const DEFAULT_LOCALE = manifest.default_locale;
        let name = await this._getLocaleString(
          manifest.name,
          DEFAULT_LOCALE,
          extensionId,
          profileId
        );
        let description = await this._getLocaleString(
          manifest.description,
          DEFAULT_LOCALE,
          extensionId,
          profileId
        );
        if (name) {
          extensionInformation = {
            id: extensionId,
            name,
            description,
          };
        } else {
          throw new Error("Cannot read the Chrome extension's name property.");
        }
      }
    } catch (ex) {
      Cu.reportError(ex);
    }
    return extensionInformation;
  },

  /**
   * Get the manifest's locale string.
   * @param {String} key - The key of a locale string, for example __MSG_name__.
   * @param {String} locale - The specific language of locale string.
   * @param {String} extensionId - The extension ID.
   * @param {String} profileId - The user profile's ID.
   * @retruns {String} The locale string.
   */
  async _getLocaleString(key, locale, extensionId, profileId) {
    // Return the key string if it is not a locale key.
    // The key string starts with "__MSG_" and ends with "__".
    // For example, "__MSG_name__".
    // https://developer.chrome.com/apps/i18n
    if (!key.startsWith("__MSG_") || !key.endsWith("__")) {
      return key;
    }

    let localeString = null;
    try {
      let localeFile;
      if (
        this._extensionLocaleStrings[profileId] &&
        this._extensionLocaleStrings[profileId][extensionId]
      ) {
        localeFile = this._extensionLocaleStrings[profileId][extensionId];
      } else {
        if (!this._extensionLocaleStrings[profileId]) {
          this._extensionLocaleStrings[profileId] = {};
        }
        let localeFilePath = this.getExtensionPath(profileId);
        localeFilePath = lazy.OS.Path.join(localeFilePath, extensionId);
        let directories = await this._getSortedByVersionSubDirectoryNames(
          localeFilePath
        );
        // If there are multiple sub-directories in the extension directory,
        // read the files in the latest directory.
        localeFilePath = lazy.OS.Path.join(
          localeFilePath,
          directories[0],
          "_locales",
          locale,
          "messages.json"
        );
        localeFile = await lazy.OS.File.read(localeFilePath, {
          encoding: "utf-8",
        });
        localeFile = JSON.parse(localeFile);
        this._extensionLocaleStrings[profileId][extensionId] = localeFile;
      }
      const PREFIX_LENGTH = 6;
      const SUFFIX_LENGTH = 2;
      // Get the locale key from the string with locale prefix and suffix.
      // For example, it will get the "name" sub-string from the "__MSG_name__" string.
      key = key.substring(PREFIX_LENGTH, key.length - SUFFIX_LENGTH);
      if (localeFile[key] && localeFile[key].message) {
        localeString = localeFile[key].message;
      }
    } catch (ex) {
      Cu.reportError(ex);
    }
    return localeString;
  },

  /**
   * Check that a specific extension is installed or not.
   * @param {String} extensionId - The extension ID.
   * @param {String} profileId - The user profile's ID.
   * @returns {Boolean} Return true if the extension is installed otherwise return false.
   */
  async isExtensionInstalled(extensionId, profileId) {
    if (profileId === undefined) {
      profileId = await this.getLastUsedProfileId();
    }
    let extensionPath = this.getExtensionPath(profileId);
    let isInstalled = await lazy.OS.File.exists(
      lazy.OS.Path.join(extensionPath, extensionId)
    );
    return isInstalled;
  },

  /**
   * Get the last used user profile's ID.
   * @returns {String} The last used user profile's ID.
   */
  async getLastUsedProfileId() {
    let localState = await this.getLocalState();
    return localState ? localState.profile.last_used : "Default";
  },

  /**
   * Get the local state file content.
   * @param {String} dataPath the type of Chrome data we're looking for (Chromium, Canary, etc.)
   * @returns {Object} The JSON-based content.
   */
  async getLocalState(dataPath = "Chrome") {
    let localState = null;
    try {
      let localStatePath = PathUtils.join(
        this.getDataPath(dataPath),
        "Local State"
      );
      localState = JSON.parse(await IOUtils.readUTF8(localStatePath));
    } catch (ex) {
      // Don't report the error if it's just a file not existing.
      if (ex.name != "NotFoundError") {
        Cu.reportError(ex);
      }
      throw ex;
    }
    return localState;
  },

  /**
   * Get the path of Chrome extension directory.
   * @param {String} profileId - The user profile's ID.
   * @returns {String} The path of Chrome extension directory.
   */
  getExtensionPath(profileId) {
    return PathUtils.join(this.getDataPath(), profileId, "Extensions");
  },

  /**
   * Get the path of an application data directory.
   * @param {String} chromeProjectName - The Chrome project name, e.g. "Chrome", "Canary", etc.
   *                                     Defaults to "Chrome".
   * @returns {String} The path of application data directory.
   */
  getDataPath(chromeProjectName = "Chrome") {
    const SUB_DIRECTORIES = {
      win: {
        Brave: ["BraveSoftware", "Brave-Browser"],
        Chrome: ["Google", "Chrome"],
        "Chrome Beta": ["Google", "Chrome Beta"],
        Chromium: ["Chromium"],
        Canary: ["Google", "Chrome SxS"],
        Edge: ["Microsoft", "Edge"],
        "Edge Beta": ["Microsoft", "Edge Beta"],
        "360 SE": ["360se6"],
      },
      macosx: {
        Brave: ["BraveSoftware", "Brave-Browser"],
        Chrome: ["Google", "Chrome"],
        Chromium: ["Chromium"],
        Canary: ["Google", "Chrome Canary"],
        Edge: ["Microsoft Edge"],
        "Edge Beta": ["Microsoft Edge Beta"],
      },
      linux: {
        Brave: ["BraveSoftware", "Brave-Browser"],
        Chrome: ["google-chrome"],
        "Chrome Beta": ["google-chrome-beta"],
        "Chrome Dev": ["google-chrome-unstable"],
        Chromium: ["chromium"],
        // Canary is not available on Linux.
        // Edge is not available on Linux.
      },
    };
    let subfolders = SUB_DIRECTORIES[AppConstants.platform][chromeProjectName];
    if (!subfolders) {
      return null;
    }

    let rootDir;
    if (AppConstants.platform == "win") {
      rootDir = chromeProjectName === "360 SE" ? "AppData" : "LocalAppData";
      subfolders = subfolders.concat(["User Data"]);
    } else if (AppConstants.platform == "macosx") {
      rootDir = "ULibDir";
      subfolders = ["Application Support"].concat(subfolders);
    } else {
      rootDir = "Home";
      subfolders = [".config"].concat(subfolders);
    }
    try {
      let target = Services.dirsvc.get(rootDir, Ci.nsIFile);
      for (let subfolder of subfolders) {
        target.append(subfolder);
      }
      return target.path;
    } catch (ex) {
      // The path logic here shouldn't error, so log it:
      Cu.reportError(ex);
    }
    return null;
  },

  /**
   * Get the directory objects sorted by version number.
   * @param {String} path - The path to the extension directory.
   * otherwise return all file/directory object.
   * @returns {Array} The file/directory object array.
   */
  async _getSortedByVersionSubDirectoryNames(path) {
    if (this._extensionVersionDirectoryNames[path]) {
      return this._extensionVersionDirectoryNames[path];
    }

    let iterator = new lazy.OS.File.DirectoryIterator(path);
    let entries = [];
    await iterator
      .forEach(async entry => {
        if (entry.isDir) {
          entries.push(entry.name);
        }
      })
      .catch(ex => {
        Cu.reportError(ex);
        entries = [];
      });
    // The directory name is the version number string of the extension.
    // For example, it could be "1.0_0", "1.0_1", "1.0_2", 1.1_0, 1.1_1, or 1.1_2.
    // The "1.0_1" strings mean that the "1.0_0" directory is existed and you install the version 1.0 again.
    // https://chromium.googlesource.com/chromium/src/+/0b58a813992b539a6b555ad7959adfad744b095a/chrome/common/extensions/extension_file_util_unittest.cc
    entries.sort((a, b) => Services.vc.compare(b, a));

    this._extensionVersionDirectoryNames[path] = entries;
    return entries;
  },

  /**
   * Convert Chrome time format to Date object
   *
   * @param   aTime
   *          Chrome time
   * @param   aFallbackValue
   *          a date or timestamp (valid argument for the Date constructor)
   *          that will be used if the chrometime value passed is invalid.
   * @return  converted Date object
   * @note    Google Chrome uses FILETIME / 10 as time.
   *          FILETIME is based on same structure of Windows.
   */
  chromeTimeToDate(aTime, aFallbackValue) {
    // The date value may be 0 in some cases. Because of the subtraction below,
    // that'd generate a date before the unix epoch, which can upset consumers
    // due to the unix timestamp then being negative. Catch this case:
    if (!aTime) {
      return new Date(aFallbackValue);
    }
    return new Date((aTime * S100NS_PER_MS - S100NS_FROM1601TO1970) / 10000);
  },

  /**
   * Convert Date object to Chrome time format
   *
   * @param   aDate
   *          Date object or integer equivalent
   * @return  Chrome time
   * @note    For details on Chrome time, see chromeTimeToDate.
   */
  dateToChromeTime(aDate) {
    return (aDate * 10000 + S100NS_FROM1601TO1970) / S100NS_PER_MS;
  },

  /**
   * Returns an array of chromium browser ids that have importable logins.
   */
  _importableLoginsCache: null,
  async getImportableLogins(formOrigin) {
    // Only provide importable if we actually support importing.
    if (!this.supportsLoginsForPlatform) {
      return undefined;
    }

    // Lazily fill the cache with all importable login browsers.
    if (!this._importableLoginsCache) {
      this._importableLoginsCache = new Map();

      // Just handle these chromium-based browsers for now.
      for (const browserId of this.CONTEXTUAL_LOGIN_IMPORT_BROWSERS) {
        // Skip if there's no profile data.
        const migrator = await lazy.MigrationUtils.getMigrator(browserId);
        if (!migrator) {
          continue;
        }

        // Check each profile for logins.
        const dataPath = await migrator.wrappedJSObject._getChromeUserDataPathIfExists();
        for (const profile of await migrator.getSourceProfiles()) {
          const path = lazy.OS.Path.join(dataPath, profile.id, "Login Data");
          // Skip if login data is missing.
          if (!(await lazy.OS.File.exists(path))) {
            Cu.reportError(`Missing file at ${path}`);
            continue;
          }

          try {
            for (const row of await lazy.MigrationUtils.getRowsFromDBWithoutLocks(
              path,
              `Importable ${browserId} logins`,
              `SELECT origin_url
               FROM logins
               WHERE blacklisted_by_user = 0`
            )) {
              const url = row.getString(0);
              try {
                // Initialize an array if it doesn't exist for the origin yet.
                const origin = lazy.LoginHelper.getLoginOrigin(url);
                const entries = this._importableLoginsCache.get(origin) || [];
                if (!entries.length) {
                  this._importableLoginsCache.set(origin, entries);
                }

                // Add the browser if it doesn't exist yet.
                if (!entries.includes(browserId)) {
                  entries.push(browserId);
                }
              } catch (ex) {
                Cu.reportError(
                  `Failed to process importable url ${url} from ${browserId} ${ex}`
                );
              }
            }
          } catch (ex) {
            Cu.reportError(
              `Failed to get importable logins from ${browserId} ${ex}`
            );
          }
        }
      }
    }
    return this._importableLoginsCache.get(formOrigin);
  },
};
