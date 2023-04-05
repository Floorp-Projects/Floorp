/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*-
 * vim: sw=2 ts=2 sts=2 et */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

const AUTH_TYPE = {
  SCHEME_HTML: 0,
  SCHEME_BASIC: 1,
  SCHEME_DIGEST: 2,
};

import { XPCOMUtils } from "resource://gre/modules/XPCOMUtils.sys.mjs";
import { AppConstants } from "resource://gre/modules/AppConstants.sys.mjs";
import { MigrationUtils } from "resource:///modules/MigrationUtils.sys.mjs";
import { MigratorBase } from "resource:///modules/MigratorBase.sys.mjs";

const lazy = {};

ChromeUtils.defineESModuleGetters(lazy, {
  ChromeMigrationUtils: "resource:///modules/ChromeMigrationUtils.sys.mjs",
  PlacesUtils: "resource://gre/modules/PlacesUtils.sys.mjs",
  Qihoo360seMigrationUtils: "resource:///modules/360seMigrationUtils.sys.mjs",
});

XPCOMUtils.defineLazyModuleGetters(lazy, {
  NetUtil: "resource://gre/modules/NetUtil.jsm",
});

/**
 * Converts an array of chrome bookmark objects into one our own places code
 * understands.
 *
 * @param {object[]} items Chrome Bookmark items to be inserted on this parent
 * @param {set} bookmarkURLAccumulator Accumulate all imported bookmark urls to be used for importing favicons
 * @param {Function} errorAccumulator function that gets called with any errors
 *   thrown so we don't drop them on the floor.
 * @returns {object[]}
 */
function convertBookmarks(items, bookmarkURLAccumulator, errorAccumulator) {
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
        bookmarkURLAccumulator.add({ url: item.url });
      } else if (item.type == "folder") {
        let folderItem = {
          type: lazy.PlacesUtils.bookmarks.TYPE_FOLDER,
          title: item.name,
        };
        folderItem.children = convertBookmarks(
          item.children,
          bookmarkURLAccumulator,
          errorAccumulator
        );
        itemsToInsert.push(folderItem);
      }
    } catch (ex) {
      console.error(ex);
      errorAccumulator(ex);
    }
  }
  return itemsToInsert;
}

/**
 * Chrome profile migrator. This can also be used as a parent class for
 * migrators for browsers that are variants of Chrome.
 */
export class ChromeProfileMigrator extends MigratorBase {
  static get key() {
    return "chrome";
  }

  static get displayNameL10nID() {
    return "migration-wizard-migrator-display-name-chrome";
  }

  static get brandImage() {
    return "chrome://browser/content/migration/brands/chrome.png";
  }

  get _chromeUserDataPathSuffix() {
    return "Chrome";
  }

  _keychainServiceName = "Chrome Safe Storage";

  _keychainAccountName = "Chrome";

  async _getChromeUserDataPathIfExists() {
    if (this._chromeUserDataPath) {
      return this._chromeUserDataPath;
    }
    let path = await lazy.ChromeMigrationUtils.getDataPath(
      this._chromeUserDataPathSuffix
    );
    let exists = await IOUtils.exists(path);
    if (exists) {
      this._chromeUserDataPath = path;
    } else {
      this._chromeUserDataPath = null;
    }
    return this._chromeUserDataPath;
  }

  async getResources(aProfile) {
    let chromeUserDataPath = await this._getChromeUserDataPathIfExists();
    if (chromeUserDataPath) {
      let profileFolder = chromeUserDataPath;
      if (aProfile) {
        profileFolder = PathUtils.join(chromeUserDataPath, aProfile.id);
      }
      if (await IOUtils.exists(profileFolder)) {
        let possibleResourcePromises = [
          GetBookmarksResource(profileFolder, this.constructor.key),
          GetHistoryResource(profileFolder),
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
  }

  async getLastUsedDate() {
    let sourceProfiles = await this.getSourceProfiles();
    if (!sourceProfiles) {
      return new Date(0);
    }
    let chromeUserDataPath = await this._getChromeUserDataPathIfExists();
    if (!chromeUserDataPath) {
      return new Date(0);
    }
    let datePromises = sourceProfiles.map(async profile => {
      let basePath = PathUtils.join(chromeUserDataPath, profile.id);
      let fileDatePromises = ["Bookmarks", "History", "Cookies"].map(
        async leafName => {
          let path = PathUtils.join(basePath, leafName);
          let info = await IOUtils.stat(path).catch(() => null);
          return info ? info.lastModified : 0;
        }
      );
      let dates = await Promise.all(fileDatePromises);
      return Math.max(...dates);
    });
    let datesOuter = await Promise.all(datePromises);
    datesOuter.push(0);
    return new Date(Math.max(...datesOuter));
  }

  async getSourceProfiles() {
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
        console.error("Error detecting Chrome profiles: ", e);
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
  }

  async _GetPasswordsResource(aProfileFolder) {
    let loginPath = PathUtils.join(aProfileFolder, "Login Data");
    if (!(await IOUtils.exists(loginPath))) {
      return null;
    }

    let {
      _chromeUserDataPathSuffix,
      _keychainServiceName,
      _keychainAccountName,
      _keychainMockPassphrase = null,
    } = this;

    let countQuery = `SELECT COUNT(*) FROM logins WHERE blacklisted_by_user = 0`;

    let countRows = await MigrationUtils.getRowsFromDBWithoutLocks(
      loginPath,
      "Chrome passwords",
      countQuery
    );

    if (!countRows[0].getResultByName("COUNT(*)")) {
      return null;
    }

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
          console.error(ex);
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
            let { ChromeWindowsLoginCrypto } = ChromeUtils.importESModule(
              "resource:///modules/ChromeWindowsLoginCrypto.sys.mjs"
            );
            crypto = new ChromeWindowsLoginCrypto(_chromeUserDataPathSuffix);
          } else if (AppConstants.platform == "macosx") {
            let { ChromeMacOSLoginCrypto } = ChromeUtils.importESModule(
              "resource:///modules/ChromeMacOSLoginCrypto.sys.mjs"
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
          console.error(ex);
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
            console.error(e);
          }
        }
        try {
          if (logins.length) {
            await MigrationUtils.insertLoginsWrapper(logins);
          }
        } catch (e) {
          console.error(e);
        }
        if (crypto.finalize) {
          crypto.finalize();
        }
        aCallback(true);
      },
    };
  }
}

async function GetBookmarksResource(aProfileFolder, aBrowserKey) {
  let bookmarksPath = PathUtils.join(aProfileFolder, "Bookmarks");
  let faviconsPath = PathUtils.join(aProfileFolder, "Favicons");

  if (aBrowserKey === "chromium-360se") {
    let localState = {};
    try {
      localState = await lazy.ChromeMigrationUtils.getLocalState("360 SE");
    } catch (ex) {
      console.error(ex);
    }

    let alternativeBookmarks = await lazy.Qihoo360seMigrationUtils.getAlternativeBookmarks(
      { bookmarksPath, localState }
    );
    if (alternativeBookmarks.resource) {
      return alternativeBookmarks.resource;
    }

    bookmarksPath = alternativeBookmarks.path;
  }

  if (!(await IOUtils.exists(bookmarksPath))) {
    return null;
  }
  // check to read JSON bookmarks structure and see if any bookmarks exist else return null
  // Parse Chrome bookmark file that is JSON format
  let bookmarkJSON = await IOUtils.readJSON(bookmarksPath);
  let other = bookmarkJSON.roots.other.children.length;
  let bookmarkBar = bookmarkJSON.roots.bookmark_bar.children.length;
  let synced = bookmarkJSON.roots.synced.children.length;

  if (!other && !bookmarkBar && !synced) {
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

        let faviconRows = [];
        try {
          faviconRows = await MigrationUtils.getRowsFromDBWithoutLocks(
            faviconsPath,
            "Chrome Bookmark Favicons",
            `select fav.id, fav.url, map.page_url, bit.image_data FROM favicons as fav 
              INNER JOIN favicon_bitmaps bit ON (fav.id = bit.icon_id) 
              INNER JOIN icon_mapping map ON (map.icon_id = bit.icon_id)`
          );
        } catch (ex) {
          console.error(ex);
        }
        // Create Hashmap for favicons
        let faviconMap = new Map();
        for (let faviconRow of faviconRows) {
          // First, try to normalize the URI:
          try {
            let uri = lazy.NetUtil.newURI(
              faviconRow.getResultByName("page_url")
            );
            faviconMap.set(uri.spec, {
              faviconData: faviconRow.getResultByName("image_data"),
              uri,
            });
          } catch (e) {
            // Couldn't parse the URI, so just skip it.
            continue;
          }
        }

        let roots = bookmarkJSON.roots;
        let bookmarkURLAccumulator = new Set();

        // Importing bookmark bar items
        if (roots.bookmark_bar.children && roots.bookmark_bar.children.length) {
          // Toolbar
          let parentGuid = lazy.PlacesUtils.bookmarks.toolbarGuid;
          let bookmarks = convertBookmarks(
            roots.bookmark_bar.children,
            bookmarkURLAccumulator,
            errorGatherer
          );
          await MigrationUtils.insertManyBookmarksWrapper(
            bookmarks,
            parentGuid
          );
        }

        // Importing Other Bookmarks items
        if (roots.other.children && roots.other.children.length) {
          // Other Bookmarks
          let parentGuid = lazy.PlacesUtils.bookmarks.unfiledGuid;
          let bookmarks = convertBookmarks(
            roots.other.children,
            bookmarkURLAccumulator,
            errorGatherer
          );
          await MigrationUtils.insertManyBookmarksWrapper(
            bookmarks,
            parentGuid
          );
        }

        // Importing synced Bookmarks items
        if (roots.synced.children && roots.synced.children.length) {
          // Synced  Bookmarks
          let parentGuid = lazy.PlacesUtils.bookmarks.unfiledGuid;
          let bookmarks = convertBookmarks(
            roots.synced.children,
            bookmarkURLAccumulator,
            errorGatherer
          );
          await MigrationUtils.insertManyBookmarksWrapper(
            bookmarks,
            parentGuid
          );
        }

        // Find all favicons with associated bookmarks
        let favicons = [];
        for (let bookmark of bookmarkURLAccumulator) {
          try {
            let uri = lazy.NetUtil.newURI(bookmark.url);
            let favicon = faviconMap.get(uri.spec);
            if (favicon) {
              favicons.push(favicon);
            }
          } catch (e) {
            // Couldn't parse the bookmark URI, so just skip
            continue;
          }
        }

        // Import Bookmark Favicons
        MigrationUtils.insertManyFavicons(favicons);
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
  let historyPath = PathUtils.join(aProfileFolder, "History");
  if (!(await IOUtils.exists(historyPath))) {
    return null;
  }
  let countQuery = "SELECT COUNT(*) FROM urls WHERE hidden = 0";

  let countRows = await MigrationUtils.getRowsFromDBWithoutLocks(
    historyPath,
    "Chrome history",
    countQuery
  );
  if (!countRows[0].getResultByName("COUNT(*)")) {
    return null;
  }
  return {
    type: MigrationUtils.resourceTypes.HISTORY,

    migrate(aCallback) {
      (async function() {
        const LIMIT = Services.prefs.getIntPref(
          "browser.migrate.chrome.history.limit"
        );

        let query =
          "SELECT url, title, last_visit_time, typed_count FROM urls WHERE hidden = 0";
        let maxAge = lazy.ChromeMigrationUtils.dateToChromeTime(
          Date.now() - MigrationUtils.HISTORY_MAX_AGE_IN_MILLISECONDS
        );
        query += " AND last_visit_time > " + maxAge;

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
            console.error(e);
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
          console.error(ex);
          aCallback(false);
        }
      );
    },
  };
}

/**
 * Chromium migrator
 */
export class ChromiumProfileMigrator extends ChromeProfileMigrator {
  static get key() {
    return "chromium";
  }

  static get displayNameL10nID() {
    return "migration-wizard-migrator-display-name-chromium";
  }

  static get brandImage() {
    return "chrome://browser/content/migration/brands/chromium.png";
  }

  _chromeUserDataPathSuffix = "Chromium";
  _keychainServiceName = "Chromium Safe Storage";
  _keychainAccountName = "Chromium";
}

/**
 * Chrome Canary
 * Not available on Linux
 */
export class CanaryProfileMigrator extends ChromeProfileMigrator {
  static get key() {
    return "canary";
  }

  static get displayNameL10nID() {
    return "migration-wizard-migrator-display-name-canary";
  }

  static get brandImage() {
    return "chrome://browser/content/migration/brands/canary.png";
  }

  get _chromeUserDataPathSuffix() {
    return "Canary";
  }

  get _keychainServiceName() {
    return "Chromium Safe Storage";
  }

  get _keychainAccountName() {
    return "Chromium";
  }
}

/**
 * Chrome Dev - Linux only (not available in Mac and Windows)
 */
export class ChromeDevMigrator extends ChromeProfileMigrator {
  static get key() {
    return "chrome-dev";
  }

  static get displayNameL10nID() {
    return "migration-wizard-migrator-display-name-chrome-dev";
  }

  _chromeUserDataPathSuffix = "Chrome Dev";
  _keychainServiceName = "Chromium Safe Storage";
  _keychainAccountName = "Chromium";
}

/**
 * Chrome Beta migrator
 */
export class ChromeBetaMigrator extends ChromeProfileMigrator {
  static get key() {
    return "chrome-beta";
  }

  static get displayNameL10nID() {
    return "migration-wizard-migrator-display-name-chrome-beta";
  }

  _chromeUserDataPathSuffix = "Chrome Beta";
  _keychainServiceName = "Chromium Safe Storage";
  _keychainAccountName = "Chromium";
}

/**
 * Brave migrator
 */
export class BraveProfileMigrator extends ChromeProfileMigrator {
  static get key() {
    return "brave";
  }

  static get displayNameL10nID() {
    return "migration-wizard-migrator-display-name-brave";
  }

  static get brandImage() {
    return "chrome://browser/content/migration/brands/brave.png";
  }

  _chromeUserDataPathSuffix = "Brave";
  _keychainServiceName = "Brave Browser Safe Storage";
  _keychainAccountName = "Brave Browser";
}

/**
 * Edge (Chromium-based) migrator
 */
export class ChromiumEdgeMigrator extends ChromeProfileMigrator {
  static get key() {
    return "chromium-edge";
  }

  static get displayNameL10nID() {
    return "migration-wizard-migrator-display-name-chromium-edge";
  }

  static get brandImage() {
    return "chrome://browser/content/migration/brands/edge.png";
  }

  _chromeUserDataPathSuffix = "Edge";
  _keychainServiceName = "Microsoft Edge Safe Storage";
  _keychainAccountName = "Microsoft Edge";
}

/**
 * Edge Beta (Chromium-based) migrator
 */
export class ChromiumEdgeBetaMigrator extends ChromeProfileMigrator {
  static get key() {
    return "chromium-edge-beta";
  }

  static get displayNameL10nID() {
    return "migration-wizard-migrator-display-name-chromium-edge-beta";
  }

  static get brandImage() {
    return "chrome://browser/content/migration/brands/edgebeta.png";
  }

  _chromeUserDataPathSuffix = "Edge Beta";
  _keychainServiceName = "Microsoft Edge Safe Storage";
  _keychainAccountName = "Microsoft Edge";
}

/**
 * Chromium 360 migrator
 */
export class Chromium360seMigrator extends ChromeProfileMigrator {
  static get key() {
    return "chromium-360se";
  }

  static get displayNameL10nID() {
    return "migration-wizard-migrator-display-name-chromium-360se";
  }

  static get brandImage() {
    return "chrome://browser/content/migration/brands/360.png";
  }

  _chromeUserDataPathSuffix = "360 SE";
  _keychainServiceName = "Microsoft Edge Safe Storage";
  _keychainAccountName = "Microsoft Edge";
}

/**
 * Opera migrator
 */
export class OperaProfileMigrator extends ChromeProfileMigrator {
  static get key() {
    return "opera";
  }

  static get displayNameL10nID() {
    return "migration-wizard-migrator-display-name-opera";
  }

  static get brandImage() {
    return "chrome://browser/content/migration/brands/opera.png";
  }

  _chromeUserDataPathSuffix = "Opera";
  _keychainServiceName = "Opera Safe Storage";
  _keychainAccountName = "Opera";

  getSourceProfiles() {
    return null;
  }
}

/**
 * Opera GX migrator
 */
export class OperaGXProfileMigrator extends ChromeProfileMigrator {
  static get key() {
    return "opera-gx";
  }

  static get displayNameL10nID() {
    return "migration-wizard-migrator-display-name-opera-gx";
  }

  static get brandImage() {
    return "chrome://browser/content/migration/brands/operagx.png";
  }

  _chromeUserDataPathSuffix = "Opera GX";
  _keychainServiceName = "Opera Safe Storage";
  _keychainAccountName = "Opera";

  getSourceProfiles() {
    return null;
  }
}

/**
 * Vivaldi migrator
 */
export class VivaldiProfileMigrator extends ChromeProfileMigrator {
  static get key() {
    return "vivaldi";
  }

  static get displayNameL10nID() {
    return "migration-wizard-migrator-display-name-vivaldi";
  }

  static get brandImage() {
    return "chrome://browser/content/migration/brands/vivaldi.png";
  }

  _chromeUserDataPathSuffix = "Vivaldi";
  _keychainServiceName = "Vivaldi Safe Storage";
  _keychainAccountName = "Vivaldi";
}
