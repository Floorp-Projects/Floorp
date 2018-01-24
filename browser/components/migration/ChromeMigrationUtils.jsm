/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
"use strict";

this.EXPORTED_SYMBOLS = ["ChromeMigrationUtils"];

const { classes: Cc, interfaces: Ci, results: Cr, utils: Cu } = Components;

Cu.import("resource://gre/modules/AppConstants.jsm");
Cu.import("resource://gre/modules/NetUtil.jsm");
Cu.import("resource://gre/modules/osfile.jsm");
Cu.import("resource://gre/modules/Services.jsm");
Cu.import("resource://gre/modules/XPCOMUtils.jsm");

this.ChromeMigrationUtils = {
  _chromeUserDataPath: null,

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
    let iterator = new OS.File.DirectoryIterator(path);
    let extensionList = [];
    await iterator.forEach(async entry => {
      if (entry.isDir) {
        let extensionInformation = await this.getExtensionInformation(entry.name, profileId);
        if (extensionInformation) {
          extensionList.push(extensionInformation);
        }
      }
    }).catch(ex => Cu.reportError(ex));
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
      manifestPath = OS.Path.join(manifestPath, extensionId);
      // If there are multiple sub-directories in the extension directory,
      // read the files in the latest directory.
      let directories = await this._getSortedByVersionSubDirectoryNames(manifestPath);
      if (!directories[0]) {
        return null;
      }

      manifestPath = OS.Path.join(manifestPath, directories[0], "manifest.json");
      let manifest = await OS.File.read(manifestPath, { encoding: "utf-8" });
      manifest = JSON.parse(manifest);
      // No app attribute means this is a Chrome extension not a Chrome app.
      if (!manifest.app) {
        const DEFAULT_LOCALE = manifest.default_locale;
        let name = await this._getLocaleString(manifest.name, DEFAULT_LOCALE, extensionId, profileId);
        let description = await this._getLocaleString(manifest.description, DEFAULT_LOCALE, extensionId, profileId);
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
      if (this._extensionLocaleStrings[profileId] &&
          this._extensionLocaleStrings[profileId][extensionId]) {
        localeFile = this._extensionLocaleStrings[profileId][extensionId];
      } else {
        if (!this._extensionLocaleStrings[profileId]) {
          this._extensionLocaleStrings[profileId] = {};
        }
        let localeFilePath = this.getExtensionPath(profileId);
        localeFilePath = OS.Path.join(localeFilePath, extensionId);
        let directories = await this._getSortedByVersionSubDirectoryNames(localeFilePath);
        // If there are multiple sub-directories in the extension directory,
        // read the files in the latest directory.
        localeFilePath = OS.Path.join(localeFilePath, directories[0], "_locales", locale, "messages.json");
        localeFile = await OS.File.read(localeFilePath, { encoding: "utf-8" });
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
    let isInstalled = await OS.File.exists(OS.Path.join(extensionPath, extensionId));
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
   * @returns {Object} The JSON-based content.
   */
  async getLocalState() {
    let localState = null;
    try {
      let localStatePath = OS.Path.join(this.getChromeUserDataPath(), "Local State");
      let localStateJson = await OS.File.read(localStatePath, { encoding: "utf-8" });
      localState = JSON.parse(localStateJson);
    } catch (ex) {
      Cu.reportError(ex);
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
    return OS.Path.join(this.getChromeUserDataPath(), profileId, "Extensions");
  },

  /**
   * Get the path of the Chrome user data directory.
   * @returns {String} The path of the Chrome user data directory.
   */
  getChromeUserDataPath() {
    if (!this._chromeUserDataPath) {
      this._chromeUserDataPath = this.getDataPath("Chrome");
    }
    return this._chromeUserDataPath;
  },

  /**
   * Get the path of an application data directory.
   * @param {String} chromeProjectName - The Chrome project name, e.g. "Chrome", "Chromium" or "Canary".
   * @returns {String} The path of application data directory.
   */
  getDataPath(chromeProjectName) {
    const SUB_DIRECTORIES = {
      win: {
        Chrome: ["Google", "Chrome"],
        Chromium: ["Chromium"],
        Canary: ["Google", "Chrome SxS"],
      },
      macosx: {
        Chrome: ["Google", "Chrome"],
        Chromium: ["Chromium"],
        Canary: ["Google", "Chrome Canary"],
      },
      linux: {
        Chrome: ["google-chrome"],
        Chromium: ["chromium"],
        // Canary is not available on Linux.
      },
    };
    let dirKey, subfolders;
    subfolders = SUB_DIRECTORIES[AppConstants.platform][chromeProjectName];
    if (!subfolders) {
      return null;
    }

    if (AppConstants.platform == "win") {
      dirKey = "winLocalAppDataDir";
      subfolders = subfolders.concat(["User Data"]);
    } else if (AppConstants.platform == "macosx") {
      dirKey = "macUserLibDir";
      subfolders = ["Application Support"].concat(subfolders);
    } else {
      dirKey = "homeDir";
      subfolders = [".config"].concat(subfolders);
    }
    subfolders.unshift(OS.Constants.Path[dirKey]);
    return OS.Path.join(...subfolders);
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

    let iterator = new OS.File.DirectoryIterator(path);
    let entries = [];
    await iterator.forEach(async entry => {
      if (entry.isDir) {
        entries.push(entry.name);
      }
    }).catch(ex => {
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
};
