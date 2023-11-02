/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

import { MigrationUtils } from "resource:///modules/MigrationUtils.sys.mjs";
import { MigratorBase } from "resource:///modules/MigratorBase.sys.mjs";
import { MSMigrationUtils } from "resource:///modules/MSMigrationUtils.sys.mjs";

const EDGE_COOKIE_PATH_OPTIONS = ["", "#!001\\", "#!002\\"];
const EDGE_COOKIES_SUFFIX = "MicrosoftEdge\\Cookies";

const lazy = {};
ChromeUtils.defineESModuleGetters(lazy, {
  ESEDBReader: "resource:///modules/ESEDBReader.sys.mjs",
  PlacesUtils: "resource://gre/modules/PlacesUtils.sys.mjs",
});

const kEdgeRegistryRoot =
  "SOFTWARE\\Classes\\Local Settings\\Software\\" +
  "Microsoft\\Windows\\CurrentVersion\\AppContainer\\Storage\\" +
  "microsoft.microsoftedge_8wekyb3d8bbwe\\MicrosoftEdge";
const kEdgeDatabasePath = "AC\\MicrosoftEdge\\User\\Default\\DataStore\\Data\\";

ChromeUtils.defineLazyGetter(lazy, "gEdgeDatabase", function () {
  let edgeDir = MSMigrationUtils.getEdgeLocalDataFolder();
  if (!edgeDir) {
    return null;
  }
  edgeDir.appendRelativePath(kEdgeDatabasePath);
  if (!edgeDir.exists() || !edgeDir.isReadable() || !edgeDir.isDirectory()) {
    return null;
  }
  let expectedLocation = edgeDir.clone();
  expectedLocation.appendRelativePath(
    "nouser1\\120712-0049\\DBStore\\spartan.edb"
  );
  if (
    expectedLocation.exists() &&
    expectedLocation.isReadable() &&
    expectedLocation.isFile()
  ) {
    expectedLocation.normalize();
    return expectedLocation;
  }
  // We used to recurse into arbitrary subdirectories here, but that code
  // went unused, so it likely isn't necessary, even if we don't understand
  // where the magic folders above come from, they seem to be the same for
  // everyone. Just return null if they're not there:
  return null;
});

/**
 * Get rows from a table in the Edge DB as an array of JS objects.
 *
 * @param {string}            tableName the name of the table to read.
 * @param {string[]|Function} columns   a list of column specifiers
 *                                      (see ESEDBReader.jsm) or a function that
 *                                      generates them based on the database
 *                                      reference once opened.
 * @param {nsIFile}           dbFile    the database file to use. Defaults to
 *                                      the main Edge database.
 * @param {Function}          filterFn  Optional. A function that is called for each row.
 *                                      Only rows for which it returns a truthy
 *                                      value are included in the result.
 * @returns {Array} An array of row objects.
 */
function readTableFromEdgeDB(
  tableName,
  columns,
  dbFile = lazy.gEdgeDatabase,
  filterFn = null
) {
  let database;
  let rows = [];
  try {
    let logFile = dbFile.parent;
    logFile.append("LogFiles");
    database = lazy.ESEDBReader.openDB(dbFile.parent, dbFile, logFile);

    if (typeof columns == "function") {
      columns = columns(database);
    }

    let tableReader = database.tableItems(tableName, columns);
    for (let row of tableReader) {
      if (!filterFn || filterFn(row)) {
        rows.push(row);
      }
    }
  } catch (ex) {
    console.error(
      "Failed to extract items from table ",
      tableName,
      " in Edge database at ",
      dbFile.path,
      " due to the following error: ",
      ex
    );
    // Deliberately make this fail so we expose failure in the UI:
    throw ex;
  } finally {
    if (database) {
      lazy.ESEDBReader.closeDB(database);
    }
  }
  return rows;
}

function EdgeTypedURLMigrator() {}

EdgeTypedURLMigrator.prototype = {
  type: MigrationUtils.resourceTypes.HISTORY,

  get _typedURLs() {
    if (!this.__typedURLs) {
      this.__typedURLs = MSMigrationUtils.getTypedURLs(kEdgeRegistryRoot);
    }
    return this.__typedURLs;
  },

  get exists() {
    return this._typedURLs.size > 0;
  },

  migrate(aCallback) {
    let typedURLs = this._typedURLs;
    let pageInfos = [];
    let now = new Date();
    let maxDate = new Date(
      Date.now() - MigrationUtils.HISTORY_MAX_AGE_IN_MILLISECONDS
    );

    for (let [urlString, time] of typedURLs) {
      let visitDate = time ? lazy.PlacesUtils.toDate(time) : now;
      if (time && visitDate < maxDate) {
        continue;
      }

      let url;
      try {
        url = new URL(urlString);
        if (!["http:", "https:", "ftp:"].includes(url.protocol)) {
          continue;
        }
      } catch (ex) {
        console.error(ex);
        continue;
      }

      pageInfos.push({
        url,
        visits: [
          {
            transition: lazy.PlacesUtils.history.TRANSITIONS.TYPED,
            date: time ? lazy.PlacesUtils.toDate(time) : new Date(),
          },
        ],
      });
    }

    if (!pageInfos.length) {
      aCallback(typedURLs.size == 0);
      return;
    }

    MigrationUtils.insertVisitsWrapper(pageInfos).then(
      () => aCallback(true),
      () => aCallback(false)
    );
  },
};

function EdgeTypedURLDBMigrator(dbOverride) {
  this.dbOverride = dbOverride;
}

EdgeTypedURLDBMigrator.prototype = {
  type: MigrationUtils.resourceTypes.HISTORY,

  get db() {
    return this.dbOverride || lazy.gEdgeDatabase;
  },

  get exists() {
    return !!this.db;
  },

  migrate(callback) {
    this._migrateTypedURLsFromDB().then(
      () => callback(true),
      ex => {
        console.error(ex);
        callback(false);
      }
    );
  },

  async _migrateTypedURLsFromDB() {
    if (await lazy.ESEDBReader.dbLocked(this.db)) {
      throw new Error("Edge seems to be running - its database is locked.");
    }
    let columns = [
      { name: "URL", type: "string" },
      { name: "AccessDateTimeUTC", type: "date" },
    ];

    let typedUrls = [];
    try {
      typedUrls = readTableFromEdgeDB("TypedUrls", columns, this.db);
    } catch (ex) {
      // Maybe the table doesn't exist (older versions of Win10).
      // Just fall through and we'll return because there's no data.
      // The `readTableFromEdgeDB` helper will report errors to the
      // console anyway.
    }
    if (!typedUrls.length) {
      return;
    }

    let pageInfos = [];

    const kDateCutOff = new Date(
      Date.now() - MigrationUtils.HISTORY_MAX_AGE_IN_MILLISECONDS
    );
    for (let typedUrlInfo of typedUrls) {
      try {
        let date = typedUrlInfo.AccessDateTimeUTC;
        if (!date) {
          date = kDateCutOff;
        } else if (date < kDateCutOff) {
          continue;
        }

        let url = new URL(typedUrlInfo.URL);
        if (!["http:", "https:", "ftp:"].includes(url.protocol)) {
          continue;
        }

        pageInfos.push({
          url,
          visits: [
            {
              transition: lazy.PlacesUtils.history.TRANSITIONS.TYPED,
              date,
            },
          ],
        });
      } catch (ex) {
        console.error(ex);
      }
    }
    await MigrationUtils.insertVisitsWrapper(pageInfos);
  },
};

function EdgeReadingListMigrator(dbOverride) {
  this.dbOverride = dbOverride;
}

EdgeReadingListMigrator.prototype = {
  type: MigrationUtils.resourceTypes.BOOKMARKS,

  get db() {
    return this.dbOverride || lazy.gEdgeDatabase;
  },

  get exists() {
    return !!this.db;
  },

  migrate(callback) {
    this._migrateReadingList(lazy.PlacesUtils.bookmarks.menuGuid).then(
      () => callback(true),
      ex => {
        console.error(ex);
        callback(false);
      }
    );
  },

  async _migrateReadingList(parentGuid) {
    if (await lazy.ESEDBReader.dbLocked(this.db)) {
      throw new Error("Edge seems to be running - its database is locked.");
    }
    let columnFn = db => {
      let columns = [
        { name: "URL", type: "string" },
        { name: "Title", type: "string" },
        { name: "AddedDate", type: "date" },
      ];

      // Later versions have an IsDeleted column:
      let isDeletedColumn = db.checkForColumn("ReadingList", "IsDeleted");
      if (
        isDeletedColumn &&
        isDeletedColumn.dbType == lazy.ESEDBReader.COLUMN_TYPES.JET_coltypBit
      ) {
        columns.push({ name: "IsDeleted", type: "boolean" });
      }
      return columns;
    };

    let filterFn = row => {
      return !row.IsDeleted;
    };

    let readingListItems = readTableFromEdgeDB(
      "ReadingList",
      columnFn,
      this.db,
      filterFn
    );
    if (!readingListItems.length) {
      return;
    }

    let destFolderGuid = await this._ensureReadingListFolder(parentGuid);
    let bookmarks = [];
    for (let item of readingListItems) {
      let dateAdded = item.AddedDate || new Date();
      // Avoid including broken URLs:
      try {
        new URL(item.URL);
      } catch (ex) {
        continue;
      }
      bookmarks.push({ url: item.URL, title: item.Title, dateAdded });
    }
    await MigrationUtils.insertManyBookmarksWrapper(bookmarks, destFolderGuid);
  },

  async _ensureReadingListFolder(parentGuid) {
    if (!this.__readingListFolderGuid) {
      let folderTitle = await MigrationUtils.getLocalizedString(
        "migration-imported-edge-reading-list"
      );
      let folderSpec = {
        type: lazy.PlacesUtils.bookmarks.TYPE_FOLDER,
        parentGuid,
        title: folderTitle,
      };
      this.__readingListFolderGuid = (
        await MigrationUtils.insertBookmarkWrapper(folderSpec)
      ).guid;
    }
    return this.__readingListFolderGuid;
  },
};

function EdgeBookmarksMigrator(dbOverride) {
  this.dbOverride = dbOverride;
}

EdgeBookmarksMigrator.prototype = {
  type: MigrationUtils.resourceTypes.BOOKMARKS,

  get db() {
    return this.dbOverride || lazy.gEdgeDatabase;
  },

  get TABLE_NAME() {
    return "Favorites";
  },

  get exists() {
    if (!("_exists" in this)) {
      this._exists = !!this.db;
    }
    return this._exists;
  },

  migrate(callback) {
    this._migrateBookmarks().then(
      () => callback(true),
      ex => {
        console.error(ex);
        callback(false);
      }
    );
  },

  async _migrateBookmarks() {
    if (await lazy.ESEDBReader.dbLocked(this.db)) {
      throw new Error("Edge seems to be running - its database is locked.");
    }
    let { toplevelBMs, toolbarBMs } = this._fetchBookmarksFromDB();
    if (toplevelBMs.length) {
      let parentGuid = lazy.PlacesUtils.bookmarks.menuGuid;
      await MigrationUtils.insertManyBookmarksWrapper(toplevelBMs, parentGuid);
    }
    if (toolbarBMs.length) {
      let parentGuid = lazy.PlacesUtils.bookmarks.toolbarGuid;
      await MigrationUtils.insertManyBookmarksWrapper(toolbarBMs, parentGuid);
    }
  },

  _fetchBookmarksFromDB() {
    let folderMap = new Map();
    let columns = [
      { name: "URL", type: "string" },
      { name: "Title", type: "string" },
      { name: "DateUpdated", type: "date" },
      { name: "IsFolder", type: "boolean" },
      { name: "IsDeleted", type: "boolean" },
      { name: "ParentId", type: "guid" },
      { name: "ItemId", type: "guid" },
    ];
    let filterFn = row => {
      if (row.IsDeleted) {
        return false;
      }
      if (row.IsFolder) {
        folderMap.set(row.ItemId, row);
      }
      return true;
    };
    let bookmarks = readTableFromEdgeDB(
      this.TABLE_NAME,
      columns,
      this.db,
      filterFn
    );
    let toplevelBMs = [],
      toolbarBMs = [];
    for (let bookmark of bookmarks) {
      let bmToInsert;
      // Ignore invalid URLs:
      if (!bookmark.IsFolder) {
        try {
          new URL(bookmark.URL);
        } catch (ex) {
          console.error(
            `Ignoring ${bookmark.URL} when importing from Edge because of exception: ${ex}`
          );
          continue;
        }
        bmToInsert = {
          dateAdded: bookmark.DateUpdated || new Date(),
          title: bookmark.Title,
          url: bookmark.URL,
        };
      } /* bookmark.IsFolder */ else {
        // Ignore the favorites bar bookmark itself.
        if (bookmark.Title == "_Favorites_Bar_") {
          continue;
        }
        if (!bookmark._childrenRef) {
          bookmark._childrenRef = [];
        }
        bmToInsert = {
          title: bookmark.Title,
          type: lazy.PlacesUtils.bookmarks.TYPE_FOLDER,
          dateAdded: bookmark.DateUpdated || new Date(),
          children: bookmark._childrenRef,
        };
      }

      if (!folderMap.has(bookmark.ParentId)) {
        toplevelBMs.push(bmToInsert);
      } else {
        let parent = folderMap.get(bookmark.ParentId);
        if (parent.Title == "_Favorites_Bar_") {
          toolbarBMs.push(bmToInsert);
          continue;
        }
        if (!parent._childrenRef) {
          parent._childrenRef = [];
        }
        parent._childrenRef.push(bmToInsert);
      }
    }
    return { toplevelBMs, toolbarBMs };
  },
};

function getCookiesPaths() {
  let folders = [];
  let edgeDir = MSMigrationUtils.getEdgeLocalDataFolder();
  if (edgeDir) {
    edgeDir.append("AC");
    for (let path of EDGE_COOKIE_PATH_OPTIONS) {
      let folder = edgeDir.clone();
      let fullPath = path + EDGE_COOKIES_SUFFIX;
      folder.appendRelativePath(fullPath);
      if (folder.exists() && folder.isReadable() && folder.isDirectory()) {
        folders.push(fullPath);
      }
    }
  }
  return folders;
}

/**
 * Edge (EdgeHTML) profile migrator
 */
export class EdgeProfileMigrator extends MigratorBase {
  static get key() {
    return "edge";
  }

  static get displayNameL10nID() {
    return "migration-wizard-migrator-display-name-edge-legacy";
  }

  static get brandImage() {
    return "chrome://browser/content/migration/brands/edge.png";
  }

  getBookmarksMigratorForTesting(dbOverride) {
    return new EdgeBookmarksMigrator(dbOverride);
  }

  getReadingListMigratorForTesting(dbOverride) {
    return new EdgeReadingListMigrator(dbOverride);
  }

  getHistoryDBMigratorForTesting(dbOverride) {
    return new EdgeTypedURLDBMigrator(dbOverride);
  }

  getHistoryRegistryMigratorForTesting() {
    return new EdgeTypedURLMigrator();
  }

  getResources() {
    let resources = [
      new EdgeBookmarksMigrator(),
      new EdgeTypedURLMigrator(),
      new EdgeTypedURLDBMigrator(),
      new EdgeReadingListMigrator(),
    ];
    let windowsVaultFormPasswordsMigrator =
      MSMigrationUtils.getWindowsVaultFormPasswordsMigrator();
    windowsVaultFormPasswordsMigrator.name = "EdgeVaultFormPasswords";
    resources.push(windowsVaultFormPasswordsMigrator);
    return resources.filter(r => r.exists);
  }

  async getLastUsedDate() {
    // Don't do this if we don't have a single profile (see the comment for
    // sourceProfiles) or if we can't find the database file:
    let sourceProfiles = await this.getSourceProfiles();
    if (sourceProfiles !== null || !lazy.gEdgeDatabase) {
      return Promise.resolve(new Date(0));
    }
    let logFilePath = PathUtils.join(
      lazy.gEdgeDatabase.parent.path,
      "LogFiles",
      "edb.log"
    );
    let dbPath = lazy.gEdgeDatabase.path;
    let datePromises = [logFilePath, dbPath, ...getCookiesPaths()].map(path => {
      return IOUtils.stat(path)
        .then(info => info.lastModified)
        .catch(() => 0);
    });
    datePromises.push(
      new Promise(resolve => {
        let typedURLs = new Map();
        try {
          typedURLs = MSMigrationUtils.getTypedURLs(kEdgeRegistryRoot);
        } catch (ex) {}
        let times = [0, ...typedURLs.values()];
        // dates is an array of PRTimes, which are in microseconds - convert to milliseconds
        resolve(Math.max.apply(Math, times) / 1000);
      })
    );
    return Promise.all(datePromises).then(dates => {
      return new Date(Math.max.apply(Math, dates));
    });
  }

  /**
   * @returns {Array|null}
   *   Somewhat counterintuitively, this returns
   *   |null| to indicate "There is only 1 (default) profile".
   *   See MigrationUtils.sys.mjs for slightly more info on how sourceProfiles is used.
   */
  getSourceProfiles() {
    return null;
  }
}
