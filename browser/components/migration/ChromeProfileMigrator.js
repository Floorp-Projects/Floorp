/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*-
 * vim: sw=2 ts=2 sts=2 et */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const { classes: Cc, interfaces: Ci, results: Cr, utils: Cu } = Components;

const S100NS_FROM1601TO1970 = 0x19DB1DED53E8000;
const S100NS_PER_MS = 10;

const AUTH_TYPE = {
  SCHEME_HTML: 0,
  SCHEME_BASIC: 1,
  SCHEME_DIGEST: 2,
};

Cu.import("resource://gre/modules/AppConstants.jsm");
Cu.import("resource://gre/modules/NetUtil.jsm");
Cu.import("resource://gre/modules/osfile.jsm");
Cu.import("resource://gre/modules/Services.jsm");
Cu.import("resource://gre/modules/XPCOMUtils.jsm");
Cu.import("resource:///modules/ChromeMigrationUtils.jsm");
Cu.import("resource:///modules/MigrationUtils.jsm");

XPCOMUtils.defineLazyModuleGetter(this, "PlacesUtils",
                                  "resource://gre/modules/PlacesUtils.jsm");
XPCOMUtils.defineLazyModuleGetter(this, "OSCrypto",
                                  "resource://gre/modules/OSCrypto.jsm");

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
 * Convert Date object to Chrome time format
 *
 * @param   aDate
 *          Date object or integer equivalent
 * @return  Chrome time
 * @note    For details on Chrome time, see chromeTimeToDate.
 */
function dateToChromeTime(aDate) {
  return (aDate * 10000 + S100NS_FROM1601TO1970) / S100NS_PER_MS;
}

/**
 * Converts an array of chrome bookmark objects into one our own places code
 * understands.
 *
 * @param   items
 *          bookmark items to be inserted on this parent
 * @param   errorAccumulator
 *          function that gets called with any errors thrown so we don't drop them on the floor.
 */
function convertBookmarks(items, errorAccumulator) {
  let itemsToInsert = [];
  for (let item of items) {
    try {
      if (item.type == "url") {
        if (item.url.trim().startsWith("chrome:")) {
          // Skip invalid chrome URIs. Creating an actual URI always reports
          // messages to the console, so we avoid doing that.
          continue;
        }
        itemsToInsert.push({url: item.url, title: item.name});
      } else if (item.type == "folder") {
        let folderItem = {type: PlacesUtils.bookmarks.TYPE_FOLDER, title: item.name};
        folderItem.children = convertBookmarks(item.children, errorAccumulator);
        itemsToInsert.push(folderItem);
      }
    } catch (ex) {
      Cu.reportError(ex);
      errorAccumulator(ex);
    }
  }
  return itemsToInsert;
}

function ChromeProfileMigrator() {
  this._chromeUserDataPathSuffix = "Chrome";
}

ChromeProfileMigrator.prototype = Object.create(MigratorPrototype);

ChromeProfileMigrator.prototype._getChromeUserDataPathIfExists = async function() {
  if (this._chromeUserDataPath) {
    return this._chromeUserDataPath;
  }
  let path = ChromeMigrationUtils.getDataPath(this._chromeUserDataPathSuffix);
  let exists = await OS.File.exists(path);
  if (exists) {
    this._chromeUserDataPath = path;
  } else {
    this._chromeUserDataPath = null;
  }
  return this._chromeUserDataPath;
};

ChromeProfileMigrator.prototype.getResources =
  async function Chrome_getResources(aProfile) {
    let chromeUserDataPath = await this._getChromeUserDataPathIfExists();
    if (chromeUserDataPath) {
      let profileFolder = OS.Path.join(chromeUserDataPath, aProfile.id);
      if (await OS.File.exists(profileFolder)) {
        let possibleResourcePromises = [
          GetBookmarksResource(profileFolder),
          GetHistoryResource(profileFolder),
          GetCookiesResource(profileFolder),
        ];
        if (AppConstants.platform == "win") {
          possibleResourcePromises.push(GetWindowsPasswordsResource(profileFolder));
        }
        let possibleResources = await Promise.all(possibleResourcePromises);
        return possibleResources.filter(r => r != null);
      }
    }
    return [];
  };

ChromeProfileMigrator.prototype.getLastUsedDate =
  async function Chrome_getLastUsedDate() {
    let sourceProfiles = await this.getSourceProfiles();
    let chromeUserDataPath = await this._getChromeUserDataPathIfExists();
    if (!chromeUserDataPath) {
      return new Date(0);
    }
    let datePromises = sourceProfiles.map(async profile => {
      let basePath = OS.Path.join(chromeUserDataPath, profile.id);
      let fileDatePromises = ["Bookmarks", "History", "Cookies"].map(async leafName => {
        let path = OS.Path.join(basePath, leafName);
        let info = await OS.File.stat(path).catch(() => null);
        return info ? info.lastModificationDate : 0;
      });
      let dates = await Promise.all(fileDatePromises);
      return Math.max(...dates);
    });
    let datesOuter = await Promise.all(datePromises);
    datesOuter.push(0);
    return new Date(Math.max(...datesOuter));
  };

ChromeProfileMigrator.prototype.getSourceProfiles =
  async function Chrome_getSourceProfiles() {
    if ("__sourceProfiles" in this)
      return this.__sourceProfiles;

    let chromeUserDataPath = await this._getChromeUserDataPathIfExists();
    if (!chromeUserDataPath)
      return [];

    let profiles = [];
    try {
      let localState = await ChromeMigrationUtils.getLocalState();
      let info_cache = localState.profile.info_cache;
      for (let profileFolderName in info_cache) {
        profiles.push({
          id: profileFolderName,
          name: info_cache[profileFolderName].name || profileFolderName,
        });
      }
    } catch (e) {
      Cu.reportError("Error detecting Chrome profiles: " + e);
      // If we weren't able to detect any profiles above, fallback to the Default profile.
      let defaultProfilePath = OS.Path.join(chromeUserDataPath, "Default");
      if (await OS.File.exists(defaultProfilePath)) {
        profiles = [{
          id: "Default",
          name: "Default",
        }];
      }
    }

    let profileResources = await Promise.all(profiles.map(async profile => ({
      profile,
      resources: await this.getResources(profile),
    })));

    // Only list profiles from which any data can be imported
    this.__sourceProfiles = profileResources.filter(({resources}) => {
      return resources && resources.length > 0;
    }, this).map(({profile}) => profile);
    return this.__sourceProfiles;
  };

ChromeProfileMigrator.prototype.getSourceHomePageURL =
  async function Chrome_getSourceHomePageURL() {
    let chromeUserDataPath = await this._getChromeUserDataPathIfExists();
    if (!chromeUserDataPath)
      return "";
    let prefsPath = OS.Path.join(chromeUserDataPath, "Preferences");
    if (await OS.File.exists(prefsPath)) {
      try {
        let json = await OS.File.read(prefsPath, {encoding: "UTF-8"});
        return JSON.parse(json).homepage;
      } catch (e) {
        Cu.reportError("Error parsing Chrome's preferences file: " + e);
      }
    }
    return "";
  };

Object.defineProperty(ChromeProfileMigrator.prototype, "sourceLocked", {
  get: function Chrome_sourceLocked() {
    // There is an exclusive lock on some SQLite databases. Assume they are locked for now.
    return true;
  },
});

async function GetBookmarksResource(aProfileFolder) {
  let bookmarksPath = OS.Path.join(aProfileFolder, "Bookmarks");
  if (!(await OS.File.exists(bookmarksPath)))
    return null;

  return {
    type: MigrationUtils.resourceTypes.BOOKMARKS,

    migrate(aCallback) {
      return (async function() {
        let gotErrors = false;
        let errorGatherer = function() { gotErrors = true; };
        // Parse Chrome bookmark file that is JSON format
        let bookmarkJSON = await OS.File.read(bookmarksPath, {encoding: "UTF-8"});
        let roots = JSON.parse(bookmarkJSON).roots;

        // Importing bookmark bar items
        if (roots.bookmark_bar.children &&
            roots.bookmark_bar.children.length > 0) {
          // Toolbar
          let parentGuid = PlacesUtils.bookmarks.toolbarGuid;
          let bookmarks = convertBookmarks(roots.bookmark_bar.children, errorGatherer);
          if (!MigrationUtils.isStartupMigration) {
            parentGuid =
              await MigrationUtils.createImportedBookmarksFolder("Chrome", parentGuid);
          }
          await MigrationUtils.insertManyBookmarksWrapper(bookmarks, parentGuid);
        }

        // Importing bookmark menu items
        if (roots.other.children &&
            roots.other.children.length > 0) {
          // Bookmark menu
          let parentGuid = PlacesUtils.bookmarks.menuGuid;
          let bookmarks = convertBookmarks(roots.other.children, errorGatherer);
          if (!MigrationUtils.isStartupMigration) {
            parentGuid
              = await MigrationUtils.createImportedBookmarksFolder("Chrome", parentGuid);
          }
          await MigrationUtils.insertManyBookmarksWrapper(bookmarks, parentGuid);
        }
        if (gotErrors) {
          throw new Error("The migration included errors.");
        }
      })().then(() => aCallback(true),
              () => aCallback(false));
    },
  };
}

async function GetHistoryResource(aProfileFolder) {
  let historyPath = OS.Path.join(aProfileFolder, "History");
  if (!(await OS.File.exists(historyPath)))
    return null;

  return {
    type: MigrationUtils.resourceTypes.HISTORY,

    migrate(aCallback) {
      (async function() {
        const MAX_AGE_IN_DAYS = Services.prefs.getIntPref("browser.migrate.chrome.history.maxAgeInDays");
        const LIMIT = Services.prefs.getIntPref("browser.migrate.chrome.history.limit");

        let query = "SELECT url, title, last_visit_time, typed_count FROM urls WHERE hidden = 0";
        if (MAX_AGE_IN_DAYS) {
          let maxAge = dateToChromeTime(Date.now() - MAX_AGE_IN_DAYS * 24 * 60 * 60 * 1000);
          query += " AND last_visit_time > " + maxAge;
        }
        if (LIMIT) {
          query += " ORDER BY last_visit_time DESC LIMIT " + LIMIT;
        }

        let rows =
          await MigrationUtils.getRowsFromDBWithoutLocks(historyPath, "Chrome history", query);
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
          await new Promise((resolve, reject) => {
            MigrationUtils.insertVisitsWrapper(places, {
              ignoreErrors: true,
              ignoreResults: true,
              handleCompletion(updatedCount) {
                if (updatedCount > 0) {
                  resolve();
                } else {
                  reject(new Error("Couldn't add visits"));
                }
              },
            });
          });
        }
      })().then(() => { aCallback(true); },
              ex => {
                Cu.reportError(ex);
                aCallback(false);
              });
    },
  };
}

async function GetCookiesResource(aProfileFolder) {
  let cookiesPath = OS.Path.join(aProfileFolder, "Cookies");
  if (!(await OS.File.exists(cookiesPath)))
    return null;

  return {
    type: MigrationUtils.resourceTypes.COOKIES,

    async migrate(aCallback) {
      // We don't support decrypting cookies yet so only import plaintext ones.
      let rows = await MigrationUtils.getRowsFromDBWithoutLocks(cookiesPath, "Chrome cookies",
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
    },
  };
}

async function GetWindowsPasswordsResource(aProfileFolder) {
  let loginPath = OS.Path.join(aProfileFolder, "Login Data");
  if (!(await OS.File.exists(loginPath)))
    return null;

  return {
    type: MigrationUtils.resourceTypes.PASSWORDS,

    async migrate(aCallback) {
      let rows = await MigrationUtils.getRowsFromDBWithoutLocks(loginPath, "Chrome passwords",
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
        try {
          let origin_url = NetUtil.newURI(row.getResultByName("origin_url"));
          // Ignore entries for non-http(s)/ftp URLs because we likely can't
          // use them anyway.
          const kValidSchemes = new Set(["https", "http", "ftp"]);
          if (!kValidSchemes.has(origin_url.scheme)) {
            continue;
          }
          let loginInfo = {
            username: row.getResultByName("username_value"),
            password: crypto.
                      decryptData(crypto.arrayToString(row.getResultByName("password_value")),
                                                       null),
            hostname: origin_url.prePath,
            formSubmitURL: null,
            httpRealm: null,
            usernameElement: row.getResultByName("username_element"),
            passwordElement: row.getResultByName("password_element"),
            timeCreated: chromeTimeToDate(row.getResultByName("date_created") + 0).getTime(),
            timesUsed: row.getResultByName("times_used") + 0,
          };

          switch (row.getResultByName("scheme")) {
            case AUTH_TYPE.SCHEME_HTML:
              let action_url = NetUtil.newURI(row.getResultByName("action_url"));
              if (!kValidSchemes.has(action_url.scheme)) {
                continue; // This continues the outer for loop.
              }
              loginInfo.formSubmitURL = action_url.prePath;
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
    },
  };
}

ChromeProfileMigrator.prototype.classDescription = "Chrome Profile Migrator";
ChromeProfileMigrator.prototype.contractID = "@mozilla.org/profile/migrator;1?app=browser&type=chrome";
ChromeProfileMigrator.prototype.classID = Components.ID("{4cec1de4-1671-4fc3-a53e-6c539dc77a26}");


/**
 *  Chromium migration
 **/
function ChromiumProfileMigrator() {
  this._chromeUserDataPathSuffix = "Chromium";
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
  this._chromeUserDataPathSuffix = "Canary";
}
CanaryProfileMigrator.prototype = Object.create(ChromeProfileMigrator.prototype);
CanaryProfileMigrator.prototype.classDescription = "Chrome Canary Profile Migrator";
CanaryProfileMigrator.prototype.contractID = "@mozilla.org/profile/migrator;1?app=browser&type=canary";
CanaryProfileMigrator.prototype.classID = Components.ID("{4bf85aa5-4e21-46ca-825f-f9c51a5e8c76}");

if (AppConstants.platform == "win" || AppConstants.platform == "macosx") {
  componentsArray.push(CanaryProfileMigrator);
}

this.NSGetFactory = XPCOMUtils.generateNSGetFactory(componentsArray);
