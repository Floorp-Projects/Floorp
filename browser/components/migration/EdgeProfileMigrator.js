/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const { classes: Cc, interfaces: Ci, utils: Cu, results: Cr } = Components;

Cu.import("resource://gre/modules/AppConstants.jsm");
Cu.import("resource://gre/modules/osfile.jsm"); /* globals OS */
Cu.import("resource://gre/modules/Services.jsm");
Cu.import("resource://gre/modules/Task.jsm");
Cu.import("resource://gre/modules/XPCOMUtils.jsm");
Cu.import("resource:///modules/MigrationUtils.jsm"); /* globals MigratorPrototype */
Cu.import("resource:///modules/MSMigrationUtils.jsm");
XPCOMUtils.defineLazyModuleGetter(this, "PlacesUtils",
                                  "resource://gre/modules/PlacesUtils.jsm");
XPCOMUtils.defineLazyModuleGetter(this, "ESEDBReader",
                                  "resource:///modules/ESEDBReader.jsm");

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
 * @param {function}          filterFn  a function that is called for each row.
 *                                      Only rows for which it returns a truthy
 *                                      value are included in the result.
 * @param {nsIFile}           dbFile    the database file to use. Defaults to
 *                                      the main Edge database.
 * @returns {Array} An array of row objects.
 */
function readTableFromEdgeDB(tableName, columns, filterFn, dbFile = gEdgeDatabase) {
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
      if (filterFn(row)) {
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
    let places = [];
    for (let [urlString, time] of typedURLs) {
      let uri;
      try {
        uri = Services.io.newURI(urlString);
        if (["http", "https", "ftp"].indexOf(uri.scheme) == -1) {
          continue;
        }
      } catch (ex) {
        Cu.reportError(ex);
        continue;
      }

      // Note that the time will be in microseconds (PRTime),
      // and Date.now() returns milliseconds. Places expects PRTime,
      // so we multiply the Date.now return value to make up the difference.
      let visitDate = time || (Date.now() * 1000);
      places.push({
        uri,
        visits: [{ transitionType: Ci.nsINavHistoryService.TRANSITION_TYPED,
                   visitDate}]
      });
    }

    if (places.length == 0) {
      aCallback(typedURLs.size == 0);
      return;
    }

    MigrationUtils.insertVisitsWrapper(places, {
      _success: false,
      handleResult() {
        // Importing any entry is considered a successful import.
        this._success = true;
      },
      handleError() {},
      handleCompletion() {
        aCallback(this._success);
      }
    });
  },
};

function EdgeReadingListMigrator() {
}

EdgeReadingListMigrator.prototype = {
  type: MigrationUtils.resourceTypes.BOOKMARKS,

  get exists() {
    return !!gEdgeDatabase;
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

  _migrateReadingList: Task.async(function*(parentGuid) {
    let columnFn = db => {
      let columns = [
        {name: "URL", type: "string"},
        {name: "Title", type: "string"},
        {name: "AddedDate", type: "date"}
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

    let readingListItems = readTableFromEdgeDB("ReadingList", columnFn, filterFn);
    if (!readingListItems.length) {
      return;
    }

    let destFolderGuid = yield this._ensureReadingListFolder(parentGuid);
    let exceptionThrown;
    for (let item of readingListItems) {
      let dateAdded = item.AddedDate || new Date();
      yield MigrationUtils.insertBookmarkWrapper({
        parentGuid: destFolderGuid, url: item.URL, title: item.Title, dateAdded
      }).catch(ex => {
        if (!exceptionThrown) {
          exceptionThrown = ex;
        }
        Cu.reportError(ex);
      });
    }
    if (exceptionThrown) {
      throw exceptionThrown;
    }
  }),

  _ensureReadingListFolder: Task.async(function*(parentGuid) {
    if (!this.__readingListFolderGuid) {
      let folderTitle = MigrationUtils.getLocalizedString("importedEdgeReadingList");
      let folderSpec = {type: PlacesUtils.bookmarks.TYPE_FOLDER, parentGuid, title: folderTitle};
      this.__readingListFolderGuid = (yield MigrationUtils.insertBookmarkWrapper(folderSpec)).guid;
    }
    return this.__readingListFolderGuid;
  }),
};

function EdgeBookmarksMigrator(dbOverride) {
  this.dbOverride = dbOverride;
}

EdgeBookmarksMigrator.prototype = {
  type: MigrationUtils.resourceTypes.BOOKMARKS,

  get db() { return this.dbOverride || gEdgeDatabase },

  get TABLE_NAME() { return "Favorites" },

  get exists() {
    if (!("_exists" in this)) {
      this._exists = !!this.db && this._checkTableExists();
    }
    return this._exists;
  },

  _checkTableExists() {
    let database;
    let rv;
    try {
      let logFile = this.db.parent;
      logFile.append("LogFiles");
      database = ESEDBReader.openDB(this.db.parent, this.db, logFile);

      rv = database.tableExists(this.TABLE_NAME);
    } catch (ex) {
      Cu.reportError("Failed to check for table " + this.TABLE_NAME + " in Edge database at " +
                     this.db.path + " due to the following error: " + ex);
      return false;
    } finally {
      if (database) {
        ESEDBReader.closeDB(database);
      }
    }
    return rv;
  },

  migrate(callback) {
    this._migrateBookmarks(PlacesUtils.bookmarks.menuGuid).then(
      () => callback(true),
      ex => {
        Cu.reportError(ex);
        callback(false);
      }
    );
  },

  _migrateBookmarks: Task.async(function*(rootGuid) {
    let {bookmarks, folderMap} = this._fetchBookmarksFromDB();
    if (!bookmarks.length) {
      return;
    }
    yield this._importBookmarks(bookmarks, folderMap, rootGuid);
  }),

  _importBookmarks: Task.async(function*(bookmarks, folderMap, rootGuid) {
    if (!MigrationUtils.isStartupMigration) {
      rootGuid =
        yield MigrationUtils.createImportedBookmarksFolder("Edge", rootGuid);
    }

    let exceptionThrown;
    for (let bookmark of bookmarks) {
      // If this is a folder, we might have created it already to put other bookmarks in.
      if (bookmark.IsFolder && bookmark._guid) {
        continue;
      }

      // If this is a folder, just create folders up to and including that folder.
      // Otherwise, create folders until we have a parent for this bookmark.
      // This avoids duplicating logic for the bookmarks bar.
      let folderId = bookmark.IsFolder ? bookmark.ItemId : bookmark.ParentId;
      let parentGuid = yield this._getGuidForFolder(folderId, folderMap, rootGuid).catch(ex => {
        if (!exceptionThrown) {
          exceptionThrown = ex;
        }
        Cu.reportError(ex);
      });

      // If this was a folder, we're done with this item
      if (bookmark.IsFolder) {
        continue;
      }

      if (!parentGuid) {
        // If we couldn't sort out a parent, fall back to importing on the root:
        parentGuid = rootGuid;
      }
      let placesInfo = {
        parentGuid,
        url: bookmark.URL,
        dateAdded: bookmark.DateUpdated || new Date(),
        title: bookmark.Title,
      };

      yield MigrationUtils.insertBookmarkWrapper(placesInfo).catch(ex => {
        if (!exceptionThrown) {
          exceptionThrown = ex;
        }
        Cu.reportError(ex);
      });
    }

    if (exceptionThrown) {
      throw exceptionThrown;
    }
  }),

  _fetchBookmarksFromDB() {
    let folderMap = new Map();
    let columns = [
      {name: "URL", type: "string"},
      {name: "Title", type: "string"},
      {name: "DateUpdated", type: "date"},
      {name: "IsFolder", type: "boolean"},
      {name: "IsDeleted", type: "boolean"},
      {name: "ParentId", type: "guid"},
      {name: "ItemId", type: "guid"}
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
    let bookmarks = readTableFromEdgeDB(this.TABLE_NAME, columns, filterFn, this.db);
    return {bookmarks, folderMap};
  },

  _getGuidForFolder: Task.async(function*(folderId, folderMap, rootGuid) {
    // If the folderId is not known as a folder in the folder map, we assume
    // we just need the root
    if (!folderMap.has(folderId)) {
      return rootGuid;
    }
    let folder = folderMap.get(folderId);
    // If the folder already has a places guid, just return that.
    if (folder._guid) {
      return folder._guid;
    }

    // Hacks! The bookmarks bar is special:
    if (folder.Title == "_Favorites_Bar_") {
      let toolbarGuid = PlacesUtils.bookmarks.toolbarGuid;
      if (!MigrationUtils.isStartupMigration) {
        toolbarGuid =
          yield MigrationUtils.createImportedBookmarksFolder("Edge", toolbarGuid);
      }
      folder._guid = toolbarGuid;
      return folder._guid;
    }
    // Otherwise, get the right parent guid recursively:
    let parentGuid = yield this._getGuidForFolder(folder.ParentId, folderMap, rootGuid);
    let folderInfo = {
      title: folder.Title,
      type: PlacesUtils.bookmarks.TYPE_FOLDER,
      dateAdded: folder.DateUpdated || new Date(),
      parentGuid,
    };
    // and add ourselves as a kid, and return the guid we got.
    let parentBM = yield MigrationUtils.insertBookmarkWrapper(folderInfo);
    folder._guid = parentBM.guid;
    return folder._guid;
  }),
};

function EdgeProfileMigrator() {
  this.wrappedJSObject = this;
}

EdgeProfileMigrator.prototype = Object.create(MigratorPrototype);

EdgeProfileMigrator.prototype.getESEMigratorForTesting = function(dbOverride) {
  return new EdgeBookmarksMigrator(dbOverride);
};

EdgeProfileMigrator.prototype.getResources = function() {
  let bookmarksMigrator = new EdgeBookmarksMigrator();
  if (!bookmarksMigrator.exists) {
    bookmarksMigrator = MSMigrationUtils.getBookmarksMigrator(MSMigrationUtils.MIGRATION_TYPE_EDGE);
  }
  let resources = [
    bookmarksMigrator,
    MSMigrationUtils.getCookiesMigrator(MSMigrationUtils.MIGRATION_TYPE_EDGE),
    new EdgeTypedURLMigrator(),
    new EdgeReadingListMigrator(),
  ];
  let windowsVaultFormPasswordsMigrator =
    MSMigrationUtils.getWindowsVaultFormPasswordsMigrator();
  windowsVaultFormPasswordsMigrator.name = "EdgeVaultFormPasswords";
  resources.push(windowsVaultFormPasswordsMigrator);
  return resources.filter(r => r.exists);
};

EdgeProfileMigrator.prototype.getLastUsedDate = function() {
  // Don't do this if we don't have a single profile (see the comment for
  // sourceProfiles) or if we can't find the database file:
  if (this.sourceProfiles !== null || !gEdgeDatabase) {
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
    resolve(Math.max.apply(Math, times));
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
EdgeProfileMigrator.prototype.__defineGetter__("sourceProfiles", function() {
  let isWin10OrHigher = AppConstants.isPlatformAndVersionAtLeast("win", "10");
  return isWin10OrHigher ? null : [];
});

EdgeProfileMigrator.prototype.__defineGetter__("sourceLocked", function() {
  // There is an exclusive lock on some databases. Assume they are locked for now.
  return true;
});


EdgeProfileMigrator.prototype.classDescription = "Edge Profile Migrator";
EdgeProfileMigrator.prototype.contractID = "@mozilla.org/profile/migrator;1?app=browser&type=edge";
EdgeProfileMigrator.prototype.classID = Components.ID("{62e8834b-2d17-49f5-96ff-56344903a2ae}");

this.NSGetFactory = XPCOMUtils.generateNSGetFactory([EdgeProfileMigrator]);
