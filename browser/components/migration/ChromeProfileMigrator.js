/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*-
 * vim: sw=2 ts=2 sts=2 et */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const { classes: Cc, interfaces: Ci, results: Cr, utils: Cu } = Components;

const FILE_INPUT_STREAM_CID = "@mozilla.org/network/file-input-stream;1";

const S100NS_FROM1601TO1970 = 0x19DB1DED53E8000;
const S100NS_PER_MS = 10;

const AUTH_TYPE = {
  SCHEME_HTML: 0,
  SCHEME_BASIC: 1,
  SCHEME_DIGEST: 2
};

Cu.import("resource://gre/modules/AppConstants.jsm");
Cu.import("resource://gre/modules/XPCOMUtils.jsm");
Cu.import("resource://gre/modules/Services.jsm");
Cu.import("resource://gre/modules/NetUtil.jsm");
Cu.import("resource://gre/modules/FileUtils.jsm");
Cu.import("resource://gre/modules/osfile.jsm"); /* globals OS */
Cu.import("resource://gre/modules/Task.jsm");
Cu.import("resource:///modules/MigrationUtils.jsm"); /* globals MigratorPrototype */

XPCOMUtils.defineLazyModuleGetter(this, "PlacesUtils",
                                  "resource://gre/modules/PlacesUtils.jsm");
XPCOMUtils.defineLazyModuleGetter(this, "OSCrypto",
                                  "resource://gre/modules/OSCrypto.jsm");
/**
 * Get an nsIFile instance representing the expected location of user data
 * for this copy of Chrome/Chromium/Canary on different OSes.
 * @param subfoldersWin {Array} an array of subfolders to use for Windows
 * @param subfoldersOSX {Array} an array of subfolders to use for OS X
 * @param subfoldersUnix {Array} an array of subfolders to use for *nix systems
 * @returns {nsIFile} the place we expect data to live. Might not actually exist!
 */
function getDataFolder(subfoldersWin, subfoldersOSX, subfoldersUnix) {
  let dirServiceID, subfolders;
  if (AppConstants.platform == "win") {
    dirServiceID = "LocalAppData";
    subfolders = subfoldersWin.concat(["User Data"]);
  } else if (AppConstants.platform == "macosx") {
    dirServiceID = "ULibDir";
    subfolders = ["Application Support"].concat(subfoldersOSX);
  } else {
    dirServiceID = "Home";
    subfolders = [".config"].concat(subfoldersUnix);
  }
  return FileUtils.getDir(dirServiceID, subfolders, false);
}

/**
 * Convert Chrome time format to Date object
 *
 * @param   aTime
 *          Chrome time
 * @return  converted Date object
 * @note    Google Chrome uses FILETIME / 10 as time.
 *          FILETIME is based on same structure of Windows.
 */
function chromeTimeToDate(aTime) {
  return new Date((aTime * S100NS_PER_MS - S100NS_FROM1601TO1970) / 10000);
}

/**
 * Insert bookmark items into specific folder.
 *
 * @param   parentGuid
 *          GUID of the folder where items will be inserted
 * @param   items
 *          bookmark items to be inserted
 * @param   errorAccumulator
 *          function that gets called with any errors thrown so we don't drop them on the floor.
 */
function* insertBookmarkItems(parentGuid, items, errorAccumulator) {
  for (let item of items) {
    try {
      if (item.type == "url") {
        if (item.url.trim().startsWith("chrome:")) {
          // Skip invalid chrome URIs. Creating an actual URI always reports
          // messages to the console, so we avoid doing that.
          continue;
        }
        yield MigrationUtils.insertBookmarkWrapper({
          parentGuid, url: item.url, title: item.name
        });
      } else if (item.type == "folder") {
        let newFolderGuid = (yield MigrationUtils.insertBookmarkWrapper({
          parentGuid, type: PlacesUtils.bookmarks.TYPE_FOLDER, title: item.name
        })).guid;

        yield insertBookmarkItems(newFolderGuid, item.children, errorAccumulator);
      }
    } catch (e) {
      Cu.reportError(e);
      errorAccumulator(e);
    }
  }
}

function ChromeProfileMigrator() {
  let chromeUserDataFolder =
    getDataFolder(["Google", "Chrome"], ["Google", "Chrome"], ["google-chrome"]);
  this._chromeUserDataFolder = chromeUserDataFolder.exists() ?
    chromeUserDataFolder : null;
}

ChromeProfileMigrator.prototype = Object.create(MigratorPrototype);

ChromeProfileMigrator.prototype.getResources =
  function Chrome_getResources(aProfile) {
    if (this._chromeUserDataFolder) {
      let profileFolder = this._chromeUserDataFolder.clone();
      profileFolder.append(aProfile.id);
      if (profileFolder.exists()) {
        let possibleResources = [
          GetBookmarksResource(profileFolder),
          GetHistoryResource(profileFolder),
          GetCookiesResource(profileFolder),
        ];
        if (AppConstants.platform == "win") {
          possibleResources.push(GetWindowsPasswordsResource(profileFolder));
        }
        return possibleResources.filter(r => r != null);
      }
    }
    return [];
  };

ChromeProfileMigrator.prototype.getLastUsedDate =
  function Chrome_getLastUsedDate() {
    let datePromises = this.sourceProfiles.map(profile => {
      let basePath = OS.Path.join(this._chromeUserDataFolder.path, profile.id);
      let fileDatePromises = ["Bookmarks", "History", "Cookies"].map(leafName => {
        let path = OS.Path.join(basePath, leafName);
        return OS.File.stat(path).catch(() => null).then(info => {
          return info ? info.lastModificationDate : 0;
        });
      });
      return Promise.all(fileDatePromises).then(dates => {
        return Math.max.apply(Math, dates);
      });
    });
    return Promise.all(datePromises).then(dates => {
      dates.push(0);
      return new Date(Math.max.apply(Math, dates));
    });
  };

Object.defineProperty(ChromeProfileMigrator.prototype, "sourceProfiles", {
  get: function Chrome_sourceProfiles() {
    if ("__sourceProfiles" in this)
      return this.__sourceProfiles;

    if (!this._chromeUserDataFolder)
      return [];

    let profiles = [];
    try {
      // Local State is a JSON file that contains profile info.
      let localState = this._chromeUserDataFolder.clone();
      localState.append("Local State");
      if (!localState.exists())
        throw new Error("Chrome's 'Local State' file does not exist.");
      if (!localState.isReadable())
        throw new Error("Chrome's 'Local State' file could not be read.");

      let fstream = Cc[FILE_INPUT_STREAM_CID].createInstance(Ci.nsIFileInputStream);
      fstream.init(localState, -1, 0, 0);
      let inputStream = NetUtil.readInputStreamToString(fstream, fstream.available(),
                                                        { charset: "UTF-8" });
      let info_cache = JSON.parse(inputStream).profile.info_cache;
      for (let profileFolderName in info_cache) {
        let profileFolder = this._chromeUserDataFolder.clone();
        profileFolder.append(profileFolderName);
        profiles.push({
          id: profileFolderName,
          name: info_cache[profileFolderName].name || profileFolderName,
        });
      }
    } catch (e) {
      Cu.reportError("Error detecting Chrome profiles: " + e);
      // If we weren't able to detect any profiles above, fallback to the Default profile.
      let defaultProfileFolder = this._chromeUserDataFolder.clone();
      defaultProfileFolder.append("Default");
      if (defaultProfileFolder.exists()) {
        profiles = [{
          id: "Default",
          name: "Default",
        }];
      }
    }

    // Only list profiles from which any data can be imported
    this.__sourceProfiles = profiles.filter(function(profile) {
      let resources = this.getResources(profile);
      return resources && resources.length > 0;
    }, this);
    return this.__sourceProfiles;
  }
});

Object.defineProperty(ChromeProfileMigrator.prototype, "sourceHomePageURL", {
  get: function Chrome_sourceHomePageURL() {
    let prefsFile = this._chromeUserDataFolder.clone();
    prefsFile.append("Preferences");
    if (prefsFile.exists()) {
      // XXX reading and parsing JSON is synchronous.
      let fstream = Cc[FILE_INPUT_STREAM_CID].
                    createInstance(Ci.nsIFileInputStream);
      fstream.init(prefsFile, -1, 0, 0);
      try {
        return JSON.parse(
          NetUtil.readInputStreamToString(fstream, fstream.available(),
                                          { charset: "UTF-8" })
            ).homepage;
      } catch (e) {
        Cu.reportError("Error parsing Chrome's preferences file: " + e);
      }
    }
    return "";
  }
});

Object.defineProperty(ChromeProfileMigrator.prototype, "sourceLocked", {
  get: function Chrome_sourceLocked() {
    // There is an exclusive lock on some SQLite databases. Assume they are locked for now.
    return true;
  },
});

function GetBookmarksResource(aProfileFolder) {
  let bookmarksFile = aProfileFolder.clone();
  bookmarksFile.append("Bookmarks");
  if (!bookmarksFile.exists())
    return null;

  return {
    type: MigrationUtils.resourceTypes.BOOKMARKS,

    migrate(aCallback) {
      return Task.spawn(function* () {
        let gotErrors = false;
        let errorGatherer = function() { gotErrors = true };
        let jsonStream = yield new Promise((resolve, reject) => {
          let options = {
            uri: NetUtil.newURI(bookmarksFile),
            loadUsingSystemPrincipal: true
          };
          NetUtil.asyncFetch(options, (inputStream, resultCode) => {
            if (Components.isSuccessCode(resultCode)) {
              resolve(inputStream);
            } else {
              reject(new Error("Could not read Bookmarks file"));
            }
          });
        });

        // Parse Chrome bookmark file that is JSON format
        let bookmarkJSON = NetUtil.readInputStreamToString(
          jsonStream, jsonStream.available(), { charset : "UTF-8" });
        let roots = JSON.parse(bookmarkJSON).roots;

        // Importing bookmark bar items
        if (roots.bookmark_bar.children &&
            roots.bookmark_bar.children.length > 0) {
          // Toolbar
          let parentGuid = PlacesUtils.bookmarks.toolbarGuid;
          if (!MigrationUtils.isStartupMigration) {
            parentGuid =
              yield MigrationUtils.createImportedBookmarksFolder("Chrome", parentGuid);
          }
          yield insertBookmarkItems(parentGuid, roots.bookmark_bar.children, errorGatherer);
        }

        // Importing bookmark menu items
        if (roots.other.children &&
            roots.other.children.length > 0) {
          // Bookmark menu
          let parentGuid = PlacesUtils.bookmarks.menuGuid;
          if (!MigrationUtils.isStartupMigration) {
            parentGuid =
              yield MigrationUtils.createImportedBookmarksFolder("Chrome", parentGuid);
          }
          yield insertBookmarkItems(parentGuid, roots.other.children, errorGatherer);
        }
        if (gotErrors) {
          throw new Error("The migration included errors.");
        }
      }).then(() => aCallback(true),
              () => aCallback(false));
    }
  };
}

function GetHistoryResource(aProfileFolder) {
  let historyFile = aProfileFolder.clone();
  historyFile.append("History");
  if (!historyFile.exists())
    return null;

  return {
    type: MigrationUtils.resourceTypes.HISTORY,

    migrate(aCallback) {
      Task.spawn(function* () {
        let rows = yield MigrationUtils.getRowsFromDBWithoutLocks(historyFile.path, "Chrome history",
          `SELECT url, title, last_visit_time, typed_count FROM urls WHERE hidden = 0`);
        let places = [];
        for (let row of rows) {
          try {
            // if having typed_count, we changes transition type to typed.
            let transType = PlacesUtils.history.TRANSITION_LINK;
            if (row.getResultByName("typed_count") > 0)
              transType = PlacesUtils.history.TRANSITION_TYPED;

            places.push({
              uri: NetUtil.newURI(row.getResultByName("url")),
              title: row.getResultByName("title"),
              visits: [{
                transitionType: transType,
                visitDate: chromeTimeToDate(
                             row.getResultByName(
                               "last_visit_time")) * 1000,
              }],
            });
          } catch (e) {
            Cu.reportError(e);
          }
        }

        if (places.length > 0) {
          yield new Promise((resolve, reject) => {
            MigrationUtils.insertVisitsWrapper(places, {
              _success: false,
              handleResult() {
                // Importing any entry is considered a successful import.
                this._success = true;
              },
              handleError() {},
              handleCompletion() {
                if (this._success) {
                  resolve();
                } else {
                  reject(new Error("Couldn't add visits"));
                }
              }
            });
          });
        }
      }).then(() => { aCallback(true) },
              ex => {
                Cu.reportError(ex);
                aCallback(false);
              });
    }
  };
}

function GetCookiesResource(aProfileFolder) {
  let cookiesFile = aProfileFolder.clone();
  cookiesFile.append("Cookies");
  if (!cookiesFile.exists())
    return null;

  return {
    type: MigrationUtils.resourceTypes.COOKIES,

    migrate: Task.async(function* (aCallback) {
      // We don't support decrypting cookies yet so only import plaintext ones.
      let rows = yield MigrationUtils.getRowsFromDBWithoutLocks(cookiesFile.path, "Chrome cookies",
       `SELECT host_key, name, value, path, expires_utc, secure, httponly, encrypted_value
        FROM cookies
        WHERE length(encrypted_value) = 0`).catch(ex => {
          Cu.reportError(ex);
          aCallback(false);
        });
      // If the promise was rejected we will have already called aCallback,
      // so we can just return here.
      if (!rows) {
        return;
      }

      for (let row of rows) {
        let host_key = row.getResultByName("host_key");
        if (host_key.match(/^\./)) {
          // 1st character of host_key may be ".", so we have to remove it
          host_key = host_key.substr(1);
        }

        try {
          let expiresUtc =
            chromeTimeToDate(row.getResultByName("expires_utc")) / 1000;
          Services.cookies.add(host_key,
                               row.getResultByName("path"),
                               row.getResultByName("name"),
                               row.getResultByName("value"),
                               row.getResultByName("secure"),
                               row.getResultByName("httponly"),
                               false,
                               parseInt(expiresUtc),
                               {});
        } catch (e) {
          Cu.reportError(e);
        }
      }
      aCallback(true);
    }),
  };
}

function GetWindowsPasswordsResource(aProfileFolder) {
  let loginFile = aProfileFolder.clone();
  loginFile.append("Login Data");
  if (!loginFile.exists())
    return null;

  return {
    type: MigrationUtils.resourceTypes.PASSWORDS,

    migrate: Task.async(function* (aCallback) {
      let rows = yield MigrationUtils.getRowsFromDBWithoutLocks(loginFile.path, "Chrome passwords",
       `SELECT origin_url, action_url, username_element, username_value,
        password_element, password_value, signon_realm, scheme, date_created,
        times_used FROM logins WHERE blacklisted_by_user = 0`).catch(ex => {
          Cu.reportError(ex);
          aCallback(false);
        });
      // If the promise was rejected we will have already called aCallback,
      // so we can just return here.
      if (!rows) {
        return;
      }
      let crypto = new OSCrypto();

      for (let row of rows) {
        let loginInfo = {
          username: row.getResultByName("username_value"),
          password: crypto.
                    decryptData(crypto.arrayToString(row.getResultByName("password_value")),
                                                     null),
          hostname: NetUtil.newURI(row.getResultByName("origin_url")).prePath,
          formSubmitURL: null,
          httpRealm: null,
          usernameElement: row.getResultByName("username_element"),
          passwordElement: row.getResultByName("password_element"),
          timeCreated: chromeTimeToDate(row.getResultByName("date_created") + 0).getTime(),
          timesUsed: row.getResultByName("times_used") + 0,
        };

        try {
          switch (row.getResultByName("scheme")) {
            case AUTH_TYPE.SCHEME_HTML:
              loginInfo.formSubmitURL = NetUtil.newURI(row.getResultByName("action_url")).prePath;
              break;
            case AUTH_TYPE.SCHEME_BASIC:
            case AUTH_TYPE.SCHEME_DIGEST:
              // signon_realm format is URIrealm, so we need remove URI
              loginInfo.httpRealm = row.getResultByName("signon_realm")
                                       .substring(loginInfo.hostname.length + 1);
              break;
            default:
              throw new Error("Login data scheme type not supported: " +
                              row.getResultByName("scheme"));
          }
          MigrationUtils.insertLoginWrapper(loginInfo);
        } catch (e) {
          Cu.reportError(e);
        }
      }
      crypto.finalize();
      aCallback(true);
    }),
  };
}

ChromeProfileMigrator.prototype.classDescription = "Chrome Profile Migrator";
ChromeProfileMigrator.prototype.contractID = "@mozilla.org/profile/migrator;1?app=browser&type=chrome";
ChromeProfileMigrator.prototype.classID = Components.ID("{4cec1de4-1671-4fc3-a53e-6c539dc77a26}");


/**
 *  Chromium migration
 **/
function ChromiumProfileMigrator() {
  let chromiumUserDataFolder = getDataFolder(["Chromium"], ["Chromium"], ["chromium"]);
  this._chromeUserDataFolder = chromiumUserDataFolder.exists() ? chromiumUserDataFolder : null;
}

ChromiumProfileMigrator.prototype = Object.create(ChromeProfileMigrator.prototype);
ChromiumProfileMigrator.prototype.classDescription = "Chromium Profile Migrator";
ChromiumProfileMigrator.prototype.contractID = "@mozilla.org/profile/migrator;1?app=browser&type=chromium";
ChromiumProfileMigrator.prototype.classID = Components.ID("{8cece922-9720-42de-b7db-7cef88cb07ca}");

var componentsArray = [ChromeProfileMigrator, ChromiumProfileMigrator];

/**
 * Chrome Canary
 * Not available on Linux
 **/
function CanaryProfileMigrator() {
  let chromeUserDataFolder = getDataFolder(["Google", "Chrome SxS"], ["Google", "Chrome Canary"]);
  this._chromeUserDataFolder = chromeUserDataFolder.exists() ? chromeUserDataFolder : null;
}
CanaryProfileMigrator.prototype = Object.create(ChromeProfileMigrator.prototype);
CanaryProfileMigrator.prototype.classDescription = "Chrome Canary Profile Migrator";
CanaryProfileMigrator.prototype.contractID = "@mozilla.org/profile/migrator;1?app=browser&type=canary";
CanaryProfileMigrator.prototype.classID = Components.ID("{4bf85aa5-4e21-46ca-825f-f9c51a5e8c76}");

if (AppConstants.platform == "win" || AppConstants.platform == "macosx") {
  componentsArray.push(CanaryProfileMigrator);
}

this.NSGetFactory = XPCOMUtils.generateNSGetFactory(componentsArray);
