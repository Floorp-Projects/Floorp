/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

import { FileUtils } from "resource://gre/modules/FileUtils.sys.mjs";

import { MigrationUtils } from "resource:///modules/MigrationUtils.sys.mjs";
import { MigratorBase } from "resource:///modules/MigratorBase.sys.mjs";

const lazy = {};

ChromeUtils.defineESModuleGetters(lazy, {
  FormHistory: "resource://gre/modules/FormHistory.sys.mjs",
  PlacesUtils: "resource://gre/modules/PlacesUtils.sys.mjs",
  PropertyListUtils: "resource://gre/modules/PropertyListUtils.sys.mjs",
});

// NSDate epoch is Jan 1, 2001 UTC
const NS_DATE_EPOCH_MS = new Date("2001-01-01T00:00:00-00:00").getTime();

// Convert NSDate timestamp to UNIX timestamp.
function parseNSDate(cocoaDateStr) {
  let asDouble = parseFloat(cocoaDateStr);
  if (!isNaN(asDouble)) {
    return new Date(NS_DATE_EPOCH_MS + asDouble * 1000);
  }
  return new Date();
}

// Convert UNIX timestamp to NSDate timestamp.
function msToNSDate(ms) {
  return parseFloat(ms - NS_DATE_EPOCH_MS) / 1000;
}

function Bookmarks(aBookmarksFile) {
  this._file = aBookmarksFile;
}
Bookmarks.prototype = {
  type: MigrationUtils.resourceTypes.BOOKMARKS,

  migrate: function B_migrate(aCallback) {
    return (async () => {
      let dict = await new Promise(resolve =>
        lazy.PropertyListUtils.read(this._file, resolve)
      );
      if (!dict) {
        throw new Error("Could not read Bookmarks.plist");
      }
      let children = dict.get("Children");
      if (!children) {
        throw new Error("Invalid Bookmarks.plist format");
      }

      let collection =
        dict.get("Title") == "com.apple.ReadingList"
          ? this.READING_LIST_COLLECTION
          : this.ROOT_COLLECTION;
      await this._migrateRootCollection(children, collection);
    })().then(
      () => aCallback(true),
      e => {
        console.error(e);
        aCallback(false);
      }
    );
  },

  // Bookmarks collections in Safari.  Constants for migrateCollection.
  ROOT_COLLECTION: 0,
  MENU_COLLECTION: 1,
  TOOLBAR_COLLECTION: 2,
  READING_LIST_COLLECTION: 3,

  /**
   * Start the migration of a Safari collection of bookmarks by retrieving favicon data.
   *
   * @param {object[]} aEntries
   *   The collection's children
   * @param {number} aCollection
   *   One of the _COLLECTION values above.
   */
  async _migrateRootCollection(aEntries, aCollection) {
    // First, try to get the favicon data of a user's bookmarks.
    // In Safari, Favicons are stored as files with a unique name:
    // the MD5 hash of the UUID of an SQLite entry in favicons.db.
    // Thus, we must create a map from bookmark URLs -> their favicon entry's UUID.
    let bookmarkURLToUUIDMap = new Map();

    const faviconFolder = FileUtils.getDir("ULibDir", [
      "Safari",
      "Favicon Cache",
    ]).path;
    let dbPath = PathUtils.join(faviconFolder, "favicons.db");

    try {
      // If there is an error getting favicon data, we catch the error and move on.
      // In this case, the bookmarkURLToUUIDMap will be left empty.
      let rows = await MigrationUtils.getRowsFromDBWithoutLocks(
        dbPath,
        "Safari favicons",
        `SELECT I.uuid, I.url AS favicon_url, P.url 
        FROM icon_info I
        INNER JOIN page_url P ON I.uuid = P.uuid;`
      );

      if (rows) {
        // Convert the rows from our SQLite database into a map from bookmark url to uuid
        for (let row of rows) {
          let uniqueURL = Services.io.newURI(row.getResultByName("url")).spec;

          // Normalize the URL by removing any trailing slashes. We'll make sure to do
          // the same when doing look-ups during a migration.
          if (uniqueURL.endsWith("/")) {
            uniqueURL = uniqueURL.replace(/\/+$/, "");
          }
          bookmarkURLToUUIDMap.set(uniqueURL, row.getResultByName("uuid"));
        }
      }
    } catch (ex) {
      console.error(ex);
    }

    await this._migrateCollection(aEntries, aCollection, bookmarkURLToUUIDMap);
  },

  /**
   * Recursively migrate a Safari collection of bookmarks.
   *
   * @param {object[]} aEntries
   *   The collection's children
   * @param {number} aCollection
   *   One of the _COLLECTION values above
   * @param {Map} bookmarkURLToUUIDMap
   *   A map from a bookmark's URL to the UUID of its entry in the favicons.db database
   * @returns {Promise<undefined>}
   *   Resolves after the bookmarks and favicons have been inserted into the
   *   appropriate databases.
   */
  async _migrateCollection(aEntries, aCollection, bookmarkURLToUUIDMap) {
    // A collection of bookmarks in Safari resembles places roots.  In the
    // property list files (Bookmarks.plist, ReadingList.plist) they are
    // stored as regular bookmarks folders, and thus can only be distinguished
    // from by their names and places in the hierarchy.

    let entriesFiltered = [];
    if (aCollection == this.ROOT_COLLECTION) {
      for (let entry of aEntries) {
        let type = entry.get("WebBookmarkType");
        if (type == "WebBookmarkTypeList" && entry.has("Children")) {
          let title = entry.get("Title");
          let children = entry.get("Children");
          if (title == "BookmarksBar") {
            await this._migrateCollection(
              children,
              this.TOOLBAR_COLLECTION,
              bookmarkURLToUUIDMap
            );
          } else if (title == "BookmarksMenu") {
            await this._migrateCollection(
              children,
              this.MENU_COLLECTION,
              bookmarkURLToUUIDMap
            );
          } else if (title == "com.apple.ReadingList") {
            await this._migrateCollection(
              children,
              this.READING_LIST_COLLECTION,
              bookmarkURLToUUIDMap
            );
          } else if (entry.get("ShouldOmitFromUI") !== true) {
            entriesFiltered.push(entry);
          }
        } else if (type == "WebBookmarkTypeLeaf") {
          entriesFiltered.push(entry);
        }
      }
    } else {
      entriesFiltered = aEntries;
    }

    if (!entriesFiltered.length) {
      return;
    }

    let folderGuid = -1;
    switch (aCollection) {
      case this.ROOT_COLLECTION: {
        // In Safari, it is possible (though quite cumbersome) to move
        // bookmarks to the bookmarks root, which is the parent folder of
        // all bookmarks "collections".  That is somewhat in parallel with
        // both the places root and the unfiled-bookmarks root.
        // Because the former is only an implementation detail in our UI,
        // the unfiled root seems to be the best choice.
        folderGuid = lazy.PlacesUtils.bookmarks.unfiledGuid;
        break;
      }
      case this.MENU_COLLECTION: {
        folderGuid = lazy.PlacesUtils.bookmarks.menuGuid;
        break;
      }
      case this.TOOLBAR_COLLECTION: {
        folderGuid = lazy.PlacesUtils.bookmarks.toolbarGuid;
        break;
      }
      case this.READING_LIST_COLLECTION: {
        // Reading list items are imported as regular bookmarks.
        // They are imported under their own folder, created either under the
        // bookmarks menu (in the case of startup migration).
        let readingListTitle = await MigrationUtils.getLocalizedString(
          "migration-imported-safari-reading-list"
        );
        folderGuid = (
          await MigrationUtils.insertBookmarkWrapper({
            parentGuid: lazy.PlacesUtils.bookmarks.menuGuid,
            type: lazy.PlacesUtils.bookmarks.TYPE_FOLDER,
            title: readingListTitle,
          })
        ).guid;
        break;
      }
      default:
        throw new Error("Unexpected value for aCollection!");
    }
    if (folderGuid == -1) {
      throw new Error("Invalid folder GUID");
    }

    await this._migrateEntries(
      entriesFiltered,
      folderGuid,
      bookmarkURLToUUIDMap
    );
  },

  /**
   * Migrates bookmarks and favicons from Safari to Firefox.
   *
   * @param {object[]} entries
   *   The Safari collection's children
   * @param {number} parentGuid
   *   GUID of the collection folder
   * @param {Map} bookmarkURLToUUIDMap
   *   A map from a bookmark's URL to the UUID of its entry in the favicons.db database
   */
  async _migrateEntries(entries, parentGuid, bookmarkURLToUUIDMap) {
    let { convertedEntries, favicons } = await this._convertEntries(
      entries,
      bookmarkURLToUUIDMap
    );

    await MigrationUtils.insertManyBookmarksWrapper(
      convertedEntries,
      parentGuid
    );

    MigrationUtils.insertManyFavicons(favicons);
  },

  /**
   * Converts Safari collection entries into a suitable format for
   * inserting bookmarks and favicons.
   *
   * @param {object[]} entries
   *   The collection's children
   * @param {Map} bookmarkURLToUUIDMap
   *   A map from a bookmark's URL to the UUID of its entry in the favicons.db database
   * @returns {object[]}
   *   Returns an object with an array of converted bookmark entries and favicons
   */
  async _convertEntries(entries, bookmarkURLToUUIDMap) {
    let favicons = [];
    let convertedEntries = [];

    const faviconFolder = FileUtils.getDir("ULibDir", [
      "Safari",
      "Favicon Cache",
    ]).path;

    for (const entry of entries) {
      let type = entry.get("WebBookmarkType");
      if (type == "WebBookmarkTypeList" && entry.has("Children")) {
        let convertedChildren = await this._convertEntries(
          entry.get("Children"),
          bookmarkURLToUUIDMap
        );
        favicons.push(...convertedChildren.favicons);
        convertedEntries.push({
          title: entry.get("Title"),
          type: lazy.PlacesUtils.bookmarks.TYPE_FOLDER,
          children: convertedChildren.convertedEntries,
        });
      } else if (type == "WebBookmarkTypeLeaf" && entry.has("URLString")) {
        // Check we understand this URL before adding it:
        let url = entry.get("URLString");
        try {
          new URL(url);
        } catch (ex) {
          console.error(
            `Ignoring ${url} when importing from Safari because of exception:`,
            ex
          );
          continue;
        }
        let title;
        if (entry.has("URIDictionary")) {
          title = entry.get("URIDictionary").get("title");
        }
        convertedEntries.push({ url, title });

        try {
          // Try to get the favicon data for each bookmark we have.
          // We use uri.spec as our unique identifier since bookmark links
          // don't completely match up in the Safari data.
          let uri = Services.io.newURI(url);
          let uriSpec = uri.spec;

          // Safari's favicon database doesn't include forward slashes for
          // the page URLs, despite adding them in the Bookmarks.plist file.
          // We'll strip any off here for our favicon lookup.
          if (uriSpec.endsWith("/")) {
            uriSpec = uriSpec.replace(/\/+$/, "");
          }

          let uuid = bookmarkURLToUUIDMap.get(uriSpec);
          if (uuid) {
            // Hash the UUID with md5 to give us the favicon file name.
            let hashedUUID = lazy.PlacesUtils.md5(uuid, {
              format: "hex",
            }).toUpperCase();
            let faviconFile = PathUtils.join(
              faviconFolder,
              "favicons",
              hashedUUID
            );
            let faviconData = await IOUtils.read(faviconFile);
            favicons.push({ faviconData, uri });
          }
        } catch (error) {
          // Even if we fail, still continue the import process
          // since favicons aren't as essential as the bookmarks themselves.
          console.error(error);
        }
      }
    }

    return { convertedEntries, favicons };
  },
};

async function GetHistoryResource() {
  let dbPath = FileUtils.getDir("ULibDir", ["Safari", "History.db"]).path;
  let maxAge = msToNSDate(
    Date.now() - MigrationUtils.HISTORY_MAX_AGE_IN_MILLISECONDS
  );

  // If we have read access to the Safari profile directory, check to
  // see if there's any history to import. If we can't access the profile
  // directory, let's assume that there's history to import and give the
  // user the option to migrate it.
  let canReadHistory = false;
  try {
    // 'stat' is always allowed, but reading is somehow not, if the user hasn't
    // allowed it:
    await IOUtils.read(dbPath, { maxBytes: 1 });
    canReadHistory = true;
  } catch (ex) {
    console.error(
      "Cannot yet read from Safari profile directory. Will presume history exists for import."
    );
  }

  if (canReadHistory) {
    let countQuery = `
      SELECT COUNT(*)
      FROM history_items LEFT JOIN history_visits
      ON history_items.id = history_visits.history_item
      WHERE history_visits.visit_time > ${maxAge}
      LIMIT 1;`;

    let countResult = await MigrationUtils.getRowsFromDBWithoutLocks(
      dbPath,
      "Safari history",
      countQuery
    );

    if (!countResult[0].getResultByName("COUNT(*)")) {
      return null;
    }
  }

  let selectQuery = `
    SELECT
      history_items.url as history_url,
      history_visits.title as history_title,
      history_visits.visit_time as history_time
    FROM history_items LEFT JOIN history_visits
    ON history_items.id = history_visits.history_item
    WHERE history_visits.visit_time > ${maxAge};`;

  return {
    type: MigrationUtils.resourceTypes.HISTORY,

    async migrate(callback) {
      callback(await this._migrate());
    },

    async _migrate() {
      let historyRows;

      try {
        historyRows = await MigrationUtils.getRowsFromDBWithoutLocks(
          dbPath,
          "Safari history",
          selectQuery
        );

        if (!historyRows.length) {
          console.log("No history found");
          return false;
        }
      } catch (ex) {
        console.error(ex);
        return false;
      }

      let pageInfos = [];
      for (let row of historyRows) {
        pageInfos.push({
          title: row.getResultByName("history_title"),
          url: new URL(row.getResultByName("history_url")),
          visits: [
            {
              transition: lazy.PlacesUtils.history.TRANSITIONS.TYPED,
              date: parseNSDate(row.getResultByName("history_time")),
            },
          ],
        });
      }
      await MigrationUtils.insertVisitsWrapper(pageInfos);

      return true;
    },
  };
}

/**
 * Safari's preferences property list is independently used for three purposes:
 * (a) importation of preferences
 * (b) importation of search strings
 * (c) retrieving the home page.
 *
 * So, rather than reading it three times, it's cached and managed here.
 *
 * @param {nsIFile} aPreferencesFile
 *   The .plist file to be read.
 */
function MainPreferencesPropertyList(aPreferencesFile) {
  this._file = aPreferencesFile;
  this._callbacks = [];
}
MainPreferencesPropertyList.prototype = {
  /**
   * @see PropertyListUtils.read
   * @param {Function} aCallback
   *   A callback called with an Object representing the key-value pairs
   *   read out of the .plist file.
   */
  read: function MPPL_read(aCallback) {
    if ("_dict" in this) {
      aCallback(this._dict);
      return;
    }

    let alreadyReading = !!this._callbacks.length;
    this._callbacks.push(aCallback);
    if (!alreadyReading) {
      lazy.PropertyListUtils.read(this._file, aDict => {
        this._dict = aDict;
        for (let callback of this._callbacks) {
          try {
            callback(aDict);
          } catch (ex) {
            console.error(ex);
          }
        }
        this._callbacks.splice(0);
      });
    }
  },
};

function SearchStrings(aMainPreferencesPropertyListInstance) {
  this._mainPreferencesPropertyList = aMainPreferencesPropertyListInstance;
}
SearchStrings.prototype = {
  type: MigrationUtils.resourceTypes.OTHERDATA,

  migrate: function SS_migrate(aCallback) {
    this._mainPreferencesPropertyList.read(
      MigrationUtils.wrapMigrateFunction(function migrateSearchStrings(aDict) {
        if (!aDict) {
          throw new Error("Could not get preferences dictionary");
        }

        if (aDict.has("RecentSearchStrings")) {
          let recentSearchStrings = aDict.get("RecentSearchStrings");
          if (recentSearchStrings && recentSearchStrings.length) {
            let changes = recentSearchStrings.map(searchString => ({
              op: "add",
              fieldname: "searchbar-history",
              value: searchString,
            }));
            lazy.FormHistory.update(changes);
          }
        }
      }, aCallback)
    );
  },
};

/**
 * Safari migrator
 */
export class SafariProfileMigrator extends MigratorBase {
  static get key() {
    return "safari";
  }

  static get displayNameL10nID() {
    return "migration-wizard-migrator-display-name-safari";
  }

  static get brandImage() {
    return "chrome://browser/content/migration/brands/safari.png";
  }

  async getResources() {
    let profileDir = FileUtils.getDir("ULibDir", ["Safari"]);
    if (!profileDir.exists()) {
      return null;
    }

    let resources = [];
    let pushProfileFileResource = function (aFileName, aConstructor) {
      let file = profileDir.clone();
      file.append(aFileName);
      if (file.exists()) {
        resources.push(new aConstructor(file));
      }
    };

    pushProfileFileResource("Bookmarks.plist", Bookmarks);

    // The Reading List feature was introduced at the same time in Windows and
    // Mac versions of Safari.  Not surprisingly, they are stored in the same
    // format in both versions.  Surpsingly, only on Windows there is a
    // separate property list for it.  This code is used on mac too, because
    // Apple may fix this at some point.
    pushProfileFileResource("ReadingList.plist", Bookmarks);

    let prefs = this.mainPreferencesPropertyList;
    if (prefs) {
      resources.push(new SearchStrings(prefs));
    }

    resources.push(GetHistoryResource());

    resources = await Promise.all(resources);

    return resources.filter(r => r != null);
  }

  async getLastUsedDate() {
    const profileDir = FileUtils.getDir("ULibDir", ["Safari"]);
    const dates = await Promise.all(
      ["Bookmarks.plist", "History.db"].map(file => {
        const path = PathUtils.join(profileDir.path, file);
        return IOUtils.stat(path)
          .then(info => info.lastModified)
          .catch(() => 0);
      })
    );

    return new Date(Math.max(...dates));
  }

  async hasPermissions() {
    if (this._hasPermissions) {
      return true;
    }
    // Check if we have access to some key files, but only if they exist.
    let historyTarget = FileUtils.getDir("ULibDir", ["Safari", "History.db"]);
    let bookmarkTarget = FileUtils.getDir("ULibDir", [
      "Safari",
      "Bookmarks.plist",
    ]);
    let faviconTarget = FileUtils.getDir("ULibDir", [
      "Safari",
      "Favicon Cache",
      "favicons.db",
    ]);
    try {
      let historyExists = await IOUtils.exists(historyTarget.path);
      let bookmarksExists = await IOUtils.exists(bookmarkTarget.path);
      let faviconsExists = await IOUtils.exists(faviconTarget.path);
      // We now know which files exist, which is always allowed.
      // To determine if we have read permissions, try to read a single byte
      // from each file that exists, which will throw if we need permissions.
      if (historyExists) {
        await IOUtils.read(historyTarget.path, { maxBytes: 1 });
      }
      if (bookmarksExists) {
        await IOUtils.read(bookmarkTarget.path, { maxBytes: 1 });
      }
      if (faviconsExists) {
        await IOUtils.read(faviconTarget.path, { maxBytes: 1 });
      }
      this._hasPermissions = true;
      return true;
    } catch (ex) {
      return false;
    }
  }

  async getPermissions(win) {
    // Keep prompting the user until they pick something that grants us access
    // to Safari's bookmarks and favicons or they cancel out of the file open panel.
    while (!(await this.hasPermissions())) {
      let fp = Cc["@mozilla.org/filepicker;1"].createInstance(Ci.nsIFilePicker);
      // The title (second arg) is not displayed on macOS, so leave it blank.
      fp.init(win, "", Ci.nsIFilePicker.modeGetFolder);
      fp.filterIndex = 1;
      fp.displayDirectory = FileUtils.getDir("ULibDir", [""]);
      // Now wait for the filepicker to open and close. If the user picks
      // the Safari folder, macOS will grant us read access to everything
      // inside, so we don't need to check or do anything else with what's
      // returned by the filepicker.
      let result = await new Promise(resolve => fp.open(resolve));
      // Bail if the user cancels the dialog:
      if (result == Ci.nsIFilePicker.returnCancel) {
        return false;
      }
    }
    return true;
  }

  async canGetPermissions() {
    if (await MigrationUtils.canGetPermissionsOnPlatform()) {
      const profileDir = FileUtils.getDir("ULibDir", ["Safari"]);
      if (await IOUtils.exists(profileDir.path)) {
        return profileDir.path;
      }
    }
    return false;
  }

  get mainPreferencesPropertyList() {
    if (this._mainPreferencesPropertyList === undefined) {
      let file = FileUtils.getDir("UsrPrfs", []);
      if (file.exists()) {
        file.append("com.apple.Safari.plist");
        if (file.exists()) {
          this._mainPreferencesPropertyList = new MainPreferencesPropertyList(
            file
          );
          return this._mainPreferencesPropertyList;
        }
      }
      this._mainPreferencesPropertyList = null;
      return this._mainPreferencesPropertyList;
    }
    return this._mainPreferencesPropertyList;
  }
}
