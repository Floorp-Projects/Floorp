/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

ChromeUtils.import("resource://gre/modules/AppConstants.jsm");
ChromeUtils.import("resource://gre/modules/osfile.jsm");
ChromeUtils.import("resource://gre/modules/Services.jsm");
ChromeUtils.import("resource://gre/modules/XPCOMUtils.jsm");
ChromeUtils.import("resource:///modules/MigrationUtils.jsm");
ChromeUtils.import("resource:///modules/MSMigrationUtils.jsm");
ChromeUtils.defineModuleGetter(this, "PlacesUtils",
                               "resource://gre/modules/PlacesUtils.jsm");
ChromeUtils.defineModuleGetter(this, "ESEDBReader",
                               "resource:///modules/ESEDBReader.jsm");

Cu.importGlobalProperties(["URL"]);

const kEdgeRegistryRoot = "SOFTWARE\\Classes\\Local Settings\\Software\\" +
  "Microsoft\\Windows\\CurrentVersion\\AppContainer\\Storage\\" +
  "microsoft.microsoftedge_8wekyb3d8bbwe\\MicrosoftEdge";
const kEdgeDatabasePath = "AC\\MicrosoftEdge\\User\\Default\\DataStore\\Data\\";

XPCOMUtils.defineLazyGetter(this, "gEdgeDatabase", function() {
  let edgeDir = MSMigrationUtils.getEdgeLocalDataFolder();
  if (!edgeDir) {
    return null;
  }
  edgeDir.appendRelativePath(kEdgeDatabasePath);
  if (!edgeDir.exists() || !edgeDir.isReadable() || !edgeDir.isDirectory()) {
    return null;
  }
  let expectedLocation = edgeDir.clone();
  expectedLocation.appendRelativePath("nouser1\\120712-0049\\DBStore\\spartan.edb");
  if (expectedLocation.exists() && expectedLocation.isReadable() && expectedLocation.isFile()) {
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
 * @param {String}            tableName the name of the table to read.
 * @param {String[]|function} columns   a list of column specifiers
 *                                      (see ESEDBReader.jsm) or a function that
 *                                      generates them based on the database
 *                                      reference once opened.
 * @param {nsIFile}           dbFile    the database file to use. Defaults to
 *                                      the main Edge database.
 * @param {function}          filterFn  Optional. A function that is called for each row.
 *                                      Only rows for which it returns a truthy
 *                                      value are included in the result.
 * @returns {Array} An array of row objects.
 */
function readTableFromEdgeDB(tableName, columns, dbFile = gEdgeDatabase, filterFn = null) {
  let database;
  let rows = [];
  try {
    let logFile = dbFile.parent;
    logFile.append("LogFiles");
    database = ESEDBReader.openDB(dbFile.parent, dbFile, logFile);

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
    Cu.reportError("Failed to extract items from table " + tableName + " in Edge database at " +
                   dbFile.path + " due to the following error: " + ex);
    // Deliberately make this fail so we expose failure in the UI:
    throw ex;
  } finally {
    if (database) {
      ESEDBReader.closeDB(database);
    }
  }
  return rows;
}

function EdgeTypedURLMigrator() {
}

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
    for (let [urlString, time] of typedURLs) {
      let url;
      try {
        url = new URL(urlString);
        if (!["http:", "https:", "ftp:"].includes(url.protocol)) {
          continue;
        }
      } catch (ex) {
        Cu.reportError(ex);
        continue;
      }

      pageInfos.push({
        url,
        visits: [{
          transition: PlacesUtils.history.TRANSITIONS.TYPED,
          date: time ? PlacesUtils.toDate(time) : new Date(),
        }],
      });
    }

    if (pageInfos.length == 0) {
      aCallback(typedURLs.size == 0);
      return;
    }

    MigrationUtils.insertVisitsWrapper(pageInfos).then(
      () => aCallback(true),
      () => aCallback(false));
  },
};

function EdgeTypedURLDBMigrator() {
}

EdgeTypedURLDBMigrator.prototype = {
  type: MigrationUtils.resourceTypes.HISTORY,

  get db() { return gEdgeDatabase; },

  get exists() {
    return !!this.db;
  },

  migrate(callback) {
    this._migrateTypedURLsFromDB().then(
      () => callback(true),
      ex => {
        Cu.reportError(ex);
        callback(false);
      }
    );
  },

  async _migrateTypedURLsFromDB() {
    if (await ESEDBReader.dbLocked(this.db)) {
      throw new Error("Edge seems to be running - its database is locked.");
    }
    let columns = [
      {name: "URL", type: "string"},
      {name: "AccessDateTimeUTC", type: "date"},
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
    // Sometimes the values are bogus (e.g. 0 becomes some date in 1600),
    // and places will throw *everything* away, not just the bogus ones,
    // so deal with that by having a cutoff date. Also, there's not much
    // point importing really old entries. The cut-off date is related to
    // Edge's launch date.
    const kDateCutOff = new Date("2016", 0, 1);
    for (let typedUrlInfo of typedUrls) {
      try {
        let url = new URL(typedUrlInfo.URL);
        if (!["http:", "https:", "ftp:"].includes(url.protocol)) {
          continue;
        }

        let date = typedUrlInfo.AccessDateTimeUTC;
        if (!date) {
          date = kDateCutOff;
        } else if (date < kDateCutOff) {
          continue;
        }

        pageInfos.push({
          url,
          visits: [{
            transition: PlacesUtils.history.TRANSITIONS.TYPED,
            date,
          }],
        });
      } catch (ex) {
        Cu.reportError(ex);
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

  get db() { return this.dbOverride || gEdgeDatabase; },

  get exists() {
    return !!this.db;
  },

  migrate(callback) {
    this._migrateReadingList(PlacesUtils.bookmarks.menuGuid).then(
      () => callback(true),
      ex => {
        Cu.reportError(ex);
        callback(false);
      }
    );
  },

  async _migrateReadingList(parentGuid) {
    if (await ESEDBReader.dbLocked(this.db)) {
      throw new Error("Edge seems to be running - its database is locked.");
    }
    let columnFn = db => {
      let columns = [
        {name: "URL", type: "string"},
        {name: "Title", type: "string"},
        {name: "AddedDate", type: "date"},
      ];

      // Later versions have an IsDeleted column:
      let isDeletedColumn = db.checkForColumn("ReadingList", "IsDeleted");
      if (isDeletedColumn && isDeletedColumn.dbType == ESEDBReader.COLUMN_TYPES.JET_coltypBit) {
        columns.push({name: "IsDeleted", type: "boolean"});
      }
      return columns;
    };

    let filterFn = row => {
      return !row.IsDeleted;
    };

    let readingListItems = readTableFromEdgeDB("ReadingList", columnFn, this.db, filterFn);
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
      let folderTitle = MigrationUtils.getLocalizedString("importedEdgeReadingList");
      let folderSpec = {type: PlacesUtils.bookmarks.TYPE_FOLDER, parentGuid, title: folderTitle};
      this.__readingListFolderGuid = (await MigrationUtils.insertBookmarkWrapper(folderSpec)).guid;
    }
    return this.__readingListFolderGuid;
  },
};

function EdgeBookmarksMigrator(dbOverride) {
  this.dbOverride = dbOverride;
}

EdgeBookmarksMigrator.prototype = {
  type: MigrationUtils.resourceTypes.BOOKMARKS,

  get db() { return this.dbOverride || gEdgeDatabase; },

  get TABLE_NAME() { return "Favorites"; },

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
        Cu.reportError(ex);
        callback(false);
      }
    );
  },

  async _migrateBookmarks() {
    if (await ESEDBReader.dbLocked(this.db)) {
      throw new Error("Edge seems to be running - its database is locked.");
    }
    let {toplevelBMs, toolbarBMs} = this._fetchBookmarksFromDB();
    if (toplevelBMs.length) {
      let parentGuid = PlacesUtils.bookmarks.menuGuid;
      if (!MigrationUtils.isStartupMigration) {
        parentGuid = await MigrationUtils.createImportedBookmarksFolder("Edge", parentGuid);
      }
      await MigrationUtils.insertManyBookmarksWrapper(toplevelBMs, parentGuid);
    }
    if (toolbarBMs.length) {
      let parentGuid = PlacesUtils.bookmarks.toolbarGuid;
      if (!MigrationUtils.isStartupMigration) {
        parentGuid = await MigrationUtils.createImportedBookmarksFolder("Edge", parentGuid);
      }
      await MigrationUtils.insertManyBookmarksWrapper(toolbarBMs, parentGuid);
    }
  },

  _fetchBookmarksFromDB() {
    let folderMap = new Map();
    let columns = [
      {name: "URL", type: "string"},
      {name: "Title", type: "string"},
      {name: "DateUpdated", type: "date"},
      {name: "IsFolder", type: "boolean"},
      {name: "IsDeleted", type: "boolean"},
      {name: "ParentId", type: "guid"},
      {name: "ItemId", type: "guid"},
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
    let bookmarks = readTableFromEdgeDB(this.TABLE_NAME, columns, this.db, filterFn);
    let toplevelBMs = [], toolbarBMs = [];
    for (let bookmark of bookmarks) {
      let bmToInsert;
      // Ignore invalid URLs:
      if (!bookmark.IsFolder) {
        try {
          new URL(bookmark.URL);
        } catch (ex) {
          Cu.reportError(`Ignoring ${bookmark.URL} when importing from Edge because of exception: ${ex}`);
          continue;
        }
        bmToInsert = {
          dateAdded: bookmark.DateUpdated || new Date(),
          title: bookmark.Title,
          url: bookmark.URL,
        };
      } else /* bookmark.IsFolder */ {
        // Ignore the favorites bar bookmark itself.
        if (bookmark.Title == "_Favorites_Bar_") {
          continue;
        }
        if (!bookmark._childrenRef) {
          bookmark._childrenRef = [];
        }
        bmToInsert = {
          title: bookmark.Title,
          type: PlacesUtils.bookmarks.TYPE_FOLDER,
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
    return {toplevelBMs, toolbarBMs};
  },
};

function EdgeProfileMigrator() {
  this.wrappedJSObject = this;
}

EdgeProfileMigrator.prototype = Object.create(MigratorPrototype);

EdgeProfileMigrator.prototype.getBookmarksMigratorForTesting = function(dbOverride) {
  return new EdgeBookmarksMigrator(dbOverride);
};

EdgeProfileMigrator.prototype.getReadingListMigratorForTesting = function(dbOverride) {
  return new EdgeReadingListMigrator(dbOverride);
};

EdgeProfileMigrator.prototype.getResources = function() {
  let resources = [
    new EdgeBookmarksMigrator(),
    MSMigrationUtils.getCookiesMigrator(MSMigrationUtils.MIGRATION_TYPE_EDGE),
    new EdgeTypedURLMigrator(),
    new EdgeTypedURLDBMigrator(),
    new EdgeReadingListMigrator(),
  ];
  let windowsVaultFormPasswordsMigrator =
    MSMigrationUtils.getWindowsVaultFormPasswordsMigrator();
  windowsVaultFormPasswordsMigrator.name = "EdgeVaultFormPasswords";
  resources.push(windowsVaultFormPasswordsMigrator);
  return resources.filter(r => r.exists);
};

EdgeProfileMigrator.prototype.getLastUsedDate = async function() {
  // Don't do this if we don't have a single profile (see the comment for
  // sourceProfiles) or if we can't find the database file:
  let sourceProfiles = await this.getSourceProfiles();
  if (sourceProfiles !== null || !gEdgeDatabase) {
    return Promise.resolve(new Date(0));
  }
  let logFilePath = OS.Path.join(gEdgeDatabase.parent.path, "LogFiles", "edb.log");
  let dbPath = gEdgeDatabase.path;
  let cookieMigrator = MSMigrationUtils.getCookiesMigrator(MSMigrationUtils.MIGRATION_TYPE_EDGE);
  let cookiePaths = cookieMigrator._cookiesFolders.map(f => f.path);
  let datePromises = [logFilePath, dbPath, ...cookiePaths].map(path => {
    return OS.File.stat(path).catch(() => null).then(info => {
      return info ? info.lastModificationDate : 0;
    });
  });
  datePromises.push(new Promise(resolve => {
    let typedURLs = new Map();
    try {
      typedURLs = MSMigrationUtils.getTypedURLs(kEdgeRegistryRoot);
    } catch (ex) {}
    let times = [0, ...typedURLs.values()];
    // dates is an array of PRTimes, which are in microseconds - convert to milliseconds
    resolve(Math.max.apply(Math, times) / 1000);
  }));
  return Promise.all(datePromises).then(dates => {
    return new Date(Math.max.apply(Math, dates));
  });
};

/* Somewhat counterintuitively, this returns:
 * - |null| to indicate "There is only 1 (default) profile" (on win10+)
 * - |[]| to indicate "There are no profiles" (on <=win8.1) which will avoid using this migrator.
 * See MigrationUtils.jsm for slightly more info on how sourceProfiles is used.
 */
EdgeProfileMigrator.prototype.getSourceProfiles = function() {
  let isWin10OrHigher = AppConstants.isPlatformAndVersionAtLeast("win", "10");
  return isWin10OrHigher ? null : [];
};

EdgeProfileMigrator.prototype.__defineGetter__("sourceLocked", function() {
  // There is an exclusive lock on some databases. Assume they are locked for now.
  return true;
});


EdgeProfileMigrator.prototype.classDescription = "Edge Profile Migrator";
EdgeProfileMigrator.prototype.contractID = "@mozilla.org/profile/migrator;1?app=browser&type=edge";
EdgeProfileMigrator.prototype.classID = Components.ID("{62e8834b-2d17-49f5-96ff-56344903a2ae}");

this.NSGetFactory = XPCOMUtils.generateNSGetFactory([EdgeProfileMigrator]);
