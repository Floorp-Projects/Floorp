/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*-
 * vim: sw=2 ts=2 sts=2 et */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const AUTH_TYPE = {
  SCHEME_HTML: 0,
  SCHEME_BASIC: 1,
  SCHEME_DIGEST: 2,
};

const { XPCOMUtils } = ChromeUtils.importESModule(
  "resource://gre/modules/XPCOMUtils.sys.mjs"
);
const { AppConstants } = ChromeUtils.import(
  "resource://gre/modules/AppConstants.jsm"
);
const { MigratorPrototype, MigrationUtils } = ChromeUtils.import(
  "resource:///modules/MigrationUtils.jsm"
);

const lazy = {};

ChromeUtils.defineESModuleGetters(lazy, {
  PlacesUIUtils: "resource:///modules/PlacesUIUtils.sys.mjs",
  PlacesUtils: "resource://gre/modules/PlacesUtils.sys.mjs",
});

XPCOMUtils.defineLazyModuleGetters(lazy, {
  ChromeMigrationUtils: "resource:///modules/ChromeMigrationUtils.jsm",
  NetUtil: "resource://gre/modules/NetUtil.jsm",
  OS: "resource://gre/modules/osfile.jsm",
  Qihoo360seMigrationUtils: "resource:///modules/360seMigrationUtils.jsm",
});

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
          // Skip invalid internal URIs. Creating an actual URI always reports
          // messages to the console because Gecko has its own concept of how
          // chrome:// URIs should be formed, so we avoid doing that.
          continue;
        }
        if (item.url.trim().startsWith("edge:")) {
          // Don't import internal Microsoft Edge URIs as they won't resolve within Firefox.
          continue;
        }
        itemsToInsert.push({ url: item.url, title: item.name });
      } else if (item.type == "folder") {
        let folderItem = {
          type: lazy.PlacesUtils.bookmarks.TYPE_FOLDER,
          title: item.name,
        };
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

ChromeProfileMigrator.prototype._keychainServiceName = "Chrome Safe Storage";
ChromeProfileMigrator.prototype._keychainAccountName = "Chrome";

ChromeProfileMigrator.prototype._getChromeUserDataPathIfExists = async function() {
  if (this._chromeUserDataPath) {
    return this._chromeUserDataPath;
  }
  let path = lazy.ChromeMigrationUtils.getDataPath(
    this._chromeUserDataPathSuffix
  );
  let exists = await lazy.OS.File.exists(path);
  if (exists) {
    this._chromeUserDataPath = path;
  } else {
    this._chromeUserDataPath = null;
  }
  return this._chromeUserDataPath;
};

ChromeProfileMigrator.prototype.getResources = async function Chrome_getResources(
  aProfile
) {
  let chromeUserDataPath = await this._getChromeUserDataPathIfExists();
  if (chromeUserDataPath) {
    let profileFolder = lazy.OS.Path.join(chromeUserDataPath, aProfile.id);
    if (await lazy.OS.File.exists(profileFolder)) {
      let possibleResourcePromises = [
        GetBookmarksResource(profileFolder, this.getBrowserKey()),
        GetHistoryResource(profileFolder),
        GetCookiesResource(profileFolder),
      ];
      if (lazy.ChromeMigrationUtils.supportsLoginsForPlatform) {
        possibleResourcePromises.push(
          this._GetPasswordsResource(profileFolder)
        );
      }
      let possibleResources = await Promise.all(possibleResourcePromises);
      return possibleResources.filter(r => r != null);
    }
  }
  return [];
};

ChromeProfileMigrator.prototype.getLastUsedDate = async function Chrome_getLastUsedDate() {
  let sourceProfiles = await this.getSourceProfiles();
  let chromeUserDataPath = await this._getChromeUserDataPathIfExists();
  if (!chromeUserDataPath) {
    return new Date(0);
  }
  let datePromises = sourceProfiles.map(async profile => {
    let basePath = lazy.OS.Path.join(chromeUserDataPath, profile.id);
    let fileDatePromises = ["Bookmarks", "History", "Cookies"].map(
      async leafName => {
        let path = lazy.OS.Path.join(basePath, leafName);
        let info = await lazy.OS.File.stat(path).catch(() => null);
        return info ? info.lastModificationDate : 0;
      }
    );
    let dates = await Promise.all(fileDatePromises);
    return Math.max(...dates);
  });
  let datesOuter = await Promise.all(datePromises);
  datesOuter.push(0);
  return new Date(Math.max(...datesOuter));
};

ChromeProfileMigrator.prototype.getSourceProfiles = async function Chrome_getSourceProfiles() {
  if ("__sourceProfiles" in this) {
    return this.__sourceProfiles;
  }

  let chromeUserDataPath = await this._getChromeUserDataPathIfExists();
  if (!chromeUserDataPath) {
    return [];
  }

  let localState;
  let profiles = [];
  try {
    localState = await lazy.ChromeMigrationUtils.getLocalState(
      this._chromeUserDataPathSuffix
    );
    let info_cache = localState.profile.info_cache;
    for (let profileFolderName in info_cache) {
      profiles.push({
        id: profileFolderName,
        name: info_cache[profileFolderName].name || profileFolderName,
      });
    }
  } catch (e) {
    // Avoid reporting NotFoundErrors from trying to get local state.
    if (localState || e.name != "NotFoundError") {
      Cu.reportError("Error detecting Chrome profiles: " + e);
    }
    // If we weren't able to detect any profiles above, fallback to the Default profile.
    let defaultProfilePath = PathUtils.join(chromeUserDataPath, "Default");
    if (await IOUtils.exists(defaultProfilePath)) {
      profiles = [
        {
          id: "Default",
          name: "Default",
        },
      ];
    }
  }

  let profileResources = await Promise.all(
    profiles.map(async profile => ({
      profile,
      resources: await this.getResources(profile),
    }))
  );

  // Only list profiles from which any data can be imported
  this.__sourceProfiles = profileResources
    .filter(({ resources }) => {
      return resources && !!resources.length;
    }, this)
    .map(({ profile }) => profile);
  return this.__sourceProfiles;
};

Object.defineProperty(ChromeProfileMigrator.prototype, "sourceLocked", {
  get: function Chrome_sourceLocked() {
    // There is an exclusive lock on some SQLite databases. Assume they are locked for now.
    return true;
  },
});

async function GetBookmarksResource(aProfileFolder, aBrowserKey) {
  let bookmarksPath = lazy.OS.Path.join(aProfileFolder, "Bookmarks");

  if (aBrowserKey === "chromium-360se") {
    let localState = {};
    try {
      localState = await lazy.ChromeMigrationUtils.getLocalState("360 SE");
    } catch (ex) {
      Cu.reportError(ex);
    }

    let alternativeBookmarks = await lazy.Qihoo360seMigrationUtils.getAlternativeBookmarks(
      { bookmarksPath, localState }
    );
    if (alternativeBookmarks.resource) {
      return alternativeBookmarks.resource;
    }

    bookmarksPath = alternativeBookmarks.path;
  }

  if (!(await lazy.OS.File.exists(bookmarksPath))) {
    return null;
  }

  return {
    type: MigrationUtils.resourceTypes.BOOKMARKS,

    migrate(aCallback) {
      return (async function() {
        let gotErrors = false;
        let errorGatherer = function() {
          gotErrors = true;
        };
        // Parse Chrome bookmark file that is JSON format
        let bookmarkJSON = await lazy.OS.File.read(bookmarksPath, {
          encoding: "UTF-8",
        });
        let roots = JSON.parse(bookmarkJSON).roots;

        // Importing bookmark bar items
        if (roots.bookmark_bar.children && roots.bookmark_bar.children.length) {
          // Toolbar
          let parentGuid = lazy.PlacesUtils.bookmarks.toolbarGuid;
          let bookmarks = convertBookmarks(
            roots.bookmark_bar.children,
            errorGatherer
          );
          await MigrationUtils.insertManyBookmarksWrapper(
            bookmarks,
            parentGuid
          );
          lazy.PlacesUIUtils.maybeToggleBookmarkToolbarVisibilityAfterMigration();
        }

        // Importing bookmark menu items
        if (roots.other.children && roots.other.children.length) {
          // Bookmark menu
          let parentGuid = lazy.PlacesUtils.bookmarks.menuGuid;
          let bookmarks = convertBookmarks(roots.other.children, errorGatherer);
          await MigrationUtils.insertManyBookmarksWrapper(
            bookmarks,
            parentGuid
          );
        }
        if (gotErrors) {
          throw new Error("The migration included errors.");
        }
      })().then(
        () => aCallback(true),
        () => aCallback(false)
      );
    },
  };
}

async function GetHistoryResource(aProfileFolder) {
  let historyPath = lazy.OS.Path.join(aProfileFolder, "History");
  if (!(await lazy.OS.File.exists(historyPath))) {
    return null;
  }

  return {
    type: MigrationUtils.resourceTypes.HISTORY,

    migrate(aCallback) {
      (async function() {
        const MAX_AGE_IN_DAYS = Services.prefs.getIntPref(
          "browser.migrate.chrome.history.maxAgeInDays"
        );
        const LIMIT = Services.prefs.getIntPref(
          "browser.migrate.chrome.history.limit"
        );

        let query =
          "SELECT url, title, last_visit_time, typed_count FROM urls WHERE hidden = 0";
        if (MAX_AGE_IN_DAYS) {
          let maxAge = lazy.ChromeMigrationUtils.dateToChromeTime(
            Date.now() - MAX_AGE_IN_DAYS * 24 * 60 * 60 * 1000
          );
          query += " AND last_visit_time > " + maxAge;
        }
        if (LIMIT) {
          query += " ORDER BY last_visit_time DESC LIMIT " + LIMIT;
        }

        let rows = await MigrationUtils.getRowsFromDBWithoutLocks(
          historyPath,
          "Chrome history",
          query
        );
        let pageInfos = [];
        let fallbackVisitDate = new Date();
        for (let row of rows) {
          try {
            // if having typed_count, we changes transition type to typed.
            let transition = lazy.PlacesUtils.history.TRANSITIONS.LINK;
            if (row.getResultByName("typed_count") > 0) {
              transition = lazy.PlacesUtils.history.TRANSITIONS.TYPED;
            }

            pageInfos.push({
              title: row.getResultByName("title"),
              url: new URL(row.getResultByName("url")),
              visits: [
                {
                  transition,
                  date: lazy.ChromeMigrationUtils.chromeTimeToDate(
                    row.getResultByName("last_visit_time"),
                    fallbackVisitDate
                  ),
                },
              ],
            });
          } catch (e) {
            Cu.reportError(e);
          }
        }

        if (pageInfos.length) {
          await MigrationUtils.insertVisitsWrapper(pageInfos);
        }
      })().then(
        () => {
          aCallback(true);
        },
        ex => {
          Cu.reportError(ex);
          aCallback(false);
        }
      );
    },
  };
}

async function GetCookiesResource(aProfileFolder) {
  let cookiesPath = lazy.OS.Path.join(aProfileFolder, "Cookies");
  if (!(await lazy.OS.File.exists(cookiesPath))) {
    return null;
  }

  return {
    type: MigrationUtils.resourceTypes.COOKIES,

    async migrate(aCallback) {
      // Get columns names and set is_sceure, is_httponly fields accordingly.
      let columns = await MigrationUtils.getRowsFromDBWithoutLocks(
        cookiesPath,
        "Chrome cookies",
        `PRAGMA table_info(cookies)`
      ).catch(ex => {
        Cu.reportError(ex);
        aCallback(false);
      });
      // If the promise was rejected we will have already called aCallback,
      // so we can just return here.
      if (!columns) {
        return;
      }
      columns = columns.map(c => c.getResultByName("name"));
      let isHttponly = columns.includes("is_httponly")
        ? "is_httponly"
        : "httponly";
      let isSecure = columns.includes("is_secure") ? "is_secure" : "secure";

      let source_scheme = columns.includes("source_scheme")
        ? "source_scheme"
        : `"${Ci.nsICookie.SCHEME_UNSET}" as source_scheme`;

      // We don't support decrypting cookies yet so only import plaintext ones.
      let rows = await MigrationUtils.getRowsFromDBWithoutLocks(
        cookiesPath,
        "Chrome cookies",
        `SELECT host_key, name, value, path, expires_utc, ${isSecure}, ${isHttponly}, encrypted_value, ${source_scheme}
        FROM cookies
        WHERE length(encrypted_value) = 0`
      ).catch(ex => {
        Cu.reportError(ex);
        aCallback(false);
      });

      // If the promise was rejected we will have already called aCallback,
      // so we can just return here.
      if (!rows) {
        return;
      }

      let fallbackExpiryDate = 0;
      for (let row of rows) {
        let host_key = row.getResultByName("host_key");
        if (host_key.match(/^\./)) {
          // 1st character of host_key may be ".", so we have to remove it
          host_key = host_key.substr(1);
        }

        let schemeType = Ci.nsICookie.SCHEME_UNSET;
        switch (row.getResultByName("source_scheme")) {
          case 1:
            schemeType = Ci.nsICookie.SCHEME_HTTP;
            break;
          case 2:
            schemeType = Ci.nsICookie.SCHEME_HTTPS;
            break;
        }

        try {
          let expiresUtc =
            lazy.ChromeMigrationUtils.chromeTimeToDate(
              row.getResultByName("expires_utc"),
              fallbackExpiryDate
            ) / 1000;
          // No point adding cookies that don't have a valid expiry.
          if (!expiresUtc) {
            continue;
          }

          Services.cookies.add(
            host_key,
            row.getResultByName("path"),
            row.getResultByName("name"),
            row.getResultByName("value"),
            row.getResultByName(isSecure),
            row.getResultByName(isHttponly),
            false,
            parseInt(expiresUtc),
            {},
            Ci.nsICookie.SAMESITE_NONE,
            schemeType
          );
        } catch (e) {
          Cu.reportError(e);
        }
      }
      aCallback(true);
    },
  };
}

ChromeProfileMigrator.prototype._GetPasswordsResource = async function(
  aProfileFolder
) {
  let loginPath = lazy.OS.Path.join(aProfileFolder, "Login Data");
  if (!(await lazy.OS.File.exists(loginPath))) {
    return null;
  }

  let {
    _chromeUserDataPathSuffix,
    _keychainServiceName,
    _keychainAccountName,
    _keychainMockPassphrase = null,
  } = this;

  return {
    type: MigrationUtils.resourceTypes.PASSWORDS,

    async migrate(aCallback) {
      let rows = await MigrationUtils.getRowsFromDBWithoutLocks(
        loginPath,
        "Chrome passwords",
        `SELECT origin_url, action_url, username_element, username_value,
        password_element, password_value, signon_realm, scheme, date_created,
        times_used FROM logins WHERE blacklisted_by_user = 0`
      ).catch(ex => {
        Cu.reportError(ex);
        aCallback(false);
      });
      // If the promise was rejected we will have already called aCallback,
      // so we can just return here.
      if (!rows) {
        return;
      }

      // If there are no relevant rows, return before initializing crypto and
      // thus prompting for Keychain access on macOS.
      if (!rows.length) {
        aCallback(true);
        return;
      }

      let crypto;
      try {
        if (AppConstants.platform == "win") {
          let { ChromeWindowsLoginCrypto } = ChromeUtils.import(
            "resource:///modules/ChromeWindowsLoginCrypto.jsm"
          );
          crypto = new ChromeWindowsLoginCrypto(_chromeUserDataPathSuffix);
        } else if (AppConstants.platform == "macosx") {
          let { ChromeMacOSLoginCrypto } = ChromeUtils.import(
            "resource:///modules/ChromeMacOSLoginCrypto.jsm"
          );
          crypto = new ChromeMacOSLoginCrypto(
            _keychainServiceName,
            _keychainAccountName,
            _keychainMockPassphrase
          );
        } else {
          aCallback(false);
          return;
        }
      } catch (ex) {
        // Handle the user canceling Keychain access or other OSCrypto errors.
        Cu.reportError(ex);
        aCallback(false);
        return;
      }

      let logins = [];
      let fallbackCreationDate = new Date();
      for (let row of rows) {
        try {
          let origin_url = lazy.NetUtil.newURI(
            row.getResultByName("origin_url")
          );
          // Ignore entries for non-http(s)/ftp URLs because we likely can't
          // use them anyway.
          const kValidSchemes = new Set(["https", "http", "ftp"]);
          if (!kValidSchemes.has(origin_url.scheme)) {
            continue;
          }
          let loginInfo = {
            username: row.getResultByName("username_value"),
            password: await crypto.decryptData(
              row.getResultByName("password_value"),
              null
            ),
            origin: origin_url.prePath,
            formActionOrigin: null,
            httpRealm: null,
            usernameElement: row.getResultByName("username_element"),
            passwordElement: row.getResultByName("password_element"),
            timeCreated: lazy.ChromeMigrationUtils.chromeTimeToDate(
              row.getResultByName("date_created") + 0,
              fallbackCreationDate
            ).getTime(),
            timesUsed: row.getResultByName("times_used") + 0,
          };

          switch (row.getResultByName("scheme")) {
            case AUTH_TYPE.SCHEME_HTML:
              let action_url = row.getResultByName("action_url");
              if (!action_url) {
                // If there is no action_url, store the wildcard "" value.
                // See the `formActionOrigin` IDL comments.
                loginInfo.formActionOrigin = "";
                break;
              }
              let action_uri = lazy.NetUtil.newURI(action_url);
              if (!kValidSchemes.has(action_uri.scheme)) {
                continue; // This continues the outer for loop.
              }
              loginInfo.formActionOrigin = action_uri.prePath;
              break;
            case AUTH_TYPE.SCHEME_BASIC:
            case AUTH_TYPE.SCHEME_DIGEST:
              // signon_realm format is URIrealm, so we need remove URI
              loginInfo.httpRealm = row
                .getResultByName("signon_realm")
                .substring(loginInfo.origin.length + 1);
              break;
            default:
              throw new Error(
                "Login data scheme type not supported: " +
                  row.getResultByName("scheme")
              );
          }
          logins.push(loginInfo);
        } catch (e) {
          Cu.reportError(e);
        }
      }
      try {
        if (logins.length) {
          await MigrationUtils.insertLoginsWrapper(logins);
        }
      } catch (e) {
        Cu.reportError(e);
      }
      if (crypto.finalize) {
        crypto.finalize();
      }
      aCallback(true);
    },
  };
};

ChromeProfileMigrator.prototype.classDescription = "Chrome Profile Migrator";
ChromeProfileMigrator.prototype.contractID =
  "@mozilla.org/profile/migrator;1?app=browser&type=chrome";
ChromeProfileMigrator.prototype.classID = Components.ID(
  "{4cec1de4-1671-4fc3-a53e-6c539dc77a26}"
);

/**
 *  Chromium migration
 **/
function ChromiumProfileMigrator() {
  this._chromeUserDataPathSuffix = "Chromium";
  this._keychainServiceName = "Chromium Safe Storage";
  this._keychainAccountName = "Chromium";
}

ChromiumProfileMigrator.prototype = Object.create(
  ChromeProfileMigrator.prototype
);
ChromiumProfileMigrator.prototype.classDescription =
  "Chromium Profile Migrator";
ChromiumProfileMigrator.prototype.contractID =
  "@mozilla.org/profile/migrator;1?app=browser&type=chromium";
ChromiumProfileMigrator.prototype.classID = Components.ID(
  "{8cece922-9720-42de-b7db-7cef88cb07ca}"
);

var EXPORTED_SYMBOLS = [
  "ChromeProfileMigrator",
  "ChromiumProfileMigrator",
  "BraveProfileMigrator",
];

/**
 * Chrome Canary
 * Not available on Linux
 **/
function CanaryProfileMigrator() {
  this._chromeUserDataPathSuffix = "Canary";
}
CanaryProfileMigrator.prototype = Object.create(
  ChromeProfileMigrator.prototype
);
CanaryProfileMigrator.prototype.classDescription =
  "Chrome Canary Profile Migrator";
CanaryProfileMigrator.prototype.contractID =
  "@mozilla.org/profile/migrator;1?app=browser&type=canary";
CanaryProfileMigrator.prototype.classID = Components.ID(
  "{4bf85aa5-4e21-46ca-825f-f9c51a5e8c76}"
);

if (AppConstants.platform == "win" || AppConstants.platform == "macosx") {
  EXPORTED_SYMBOLS.push("CanaryProfileMigrator");
}

/**
 * Chrome Dev - Linux only (not available in Mac and Windows)
 */
function ChromeDevMigrator() {
  this._chromeUserDataPathSuffix = "Chrome Dev";
}
ChromeDevMigrator.prototype = Object.create(ChromeProfileMigrator.prototype);
ChromeDevMigrator.prototype.classDescription = "Chrome Dev Profile Migrator";
ChromeDevMigrator.prototype.contractID =
  "@mozilla.org/profile/migrator;1?app=browser&type=chrome-dev";
ChromeDevMigrator.prototype.classID = Components.ID(
  "{7370a02a-4886-42c3-a4ec-d48c726ec30a}"
);

if (AppConstants.platform != "win" && AppConstants.platform != "macosx") {
  EXPORTED_SYMBOLS.push("ChromeDevMigrator");
}

function ChromeBetaMigrator() {
  this._chromeUserDataPathSuffix = "Chrome Beta";
}
ChromeBetaMigrator.prototype = Object.create(ChromeProfileMigrator.prototype);
ChromeBetaMigrator.prototype.classDescription = "Chrome Beta Profile Migrator";
ChromeBetaMigrator.prototype.contractID =
  "@mozilla.org/profile/migrator;1?app=browser&type=chrome-beta";
ChromeBetaMigrator.prototype.classID = Components.ID(
  "{47f75963-840b-4950-a1f0-d9c1864f8b8e}"
);

if (AppConstants.platform != "macosx") {
  EXPORTED_SYMBOLS.push("ChromeBetaMigrator");
}

function BraveProfileMigrator() {
  this._chromeUserDataPathSuffix = "Brave";
  this._keychainServiceName = "Brave Browser Safe Storage";
  this._keychainAccountName = "Brave Browser";
}
BraveProfileMigrator.prototype = Object.create(ChromeProfileMigrator.prototype);
BraveProfileMigrator.prototype.classDescription = "Brave Browser Migrator";
BraveProfileMigrator.prototype.contractID =
  "@mozilla.org/profile/migrator;1?app=browser&type=brave";
BraveProfileMigrator.prototype.classID = Components.ID(
  "{4071880a-69e4-4c83-88b4-6c589a62801d}"
);

function ChromiumEdgeMigrator() {
  this._chromeUserDataPathSuffix = "Edge";
  this._keychainServiceName = "Microsoft Edge Safe Storage";
  this._keychainAccountName = "Microsoft Edge";
}
ChromiumEdgeMigrator.prototype = Object.create(ChromeProfileMigrator.prototype);
ChromiumEdgeMigrator.prototype.classDescription =
  "Chromium Edge Profile Migrator";
ChromiumEdgeMigrator.prototype.contractID =
  "@mozilla.org/profile/migrator;1?app=browser&type=chromium-edge";
ChromiumEdgeMigrator.prototype.classID = Components.ID(
  "{3c7f6b7c-baa9-4338-acfa-04bf79f1dcf1}"
);

function ChromiumEdgeBetaMigrator() {
  this._chromeUserDataPathSuffix = "Edge Beta";
  this._keychainServiceName = "Microsoft Edge Safe Storage";
  this._keychainAccountName = "Microsoft Edge";
}
ChromiumEdgeBetaMigrator.prototype = Object.create(
  ChromiumEdgeMigrator.prototype
);
ChromiumEdgeBetaMigrator.prototype.classDescription =
  "Chromium Edge Beta Profile Migrator";
ChromiumEdgeBetaMigrator.prototype.contractID =
  "@mozilla.org/profile/migrator;1?app=browser&type=chromium-edge-beta";
ChromiumEdgeBetaMigrator.prototype.classID = Components.ID(
  "{0fc3d48a-c1c3-4871-b58f-a8b47d1555fb}"
);

if (AppConstants.platform == "macosx" || AppConstants.platform == "win") {
  EXPORTED_SYMBOLS.push("ChromiumEdgeMigrator", "ChromiumEdgeBetaMigrator");
}

function Chromium360seMigrator() {
  this._chromeUserDataPathSuffix = "360 SE";
}
Chromium360seMigrator.prototype = Object.create(
  ChromeProfileMigrator.prototype
);
Chromium360seMigrator.prototype.classDescription =
  "Chromium 360 Secure Browser Profile Migrator";
Chromium360seMigrator.prototype.contractID =
  "@mozilla.org/profile/migrator;1?app=browser&type=chromium-360se";
Chromium360seMigrator.prototype.classID = Components.ID(
  "{2e1a182e-ce4f-4dc9-a22c-d4125b931552}"
);

if (AppConstants.platform == "win") {
  EXPORTED_SYMBOLS.push("Chromium360seMigrator");
}
