/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

const { classes: Cc, interfaces: Ci, utils: Cu, results: Cr } = Components;

Cu.import("resource://gre/modules/XPCOMUtils.jsm");
Cu.import("resource://gre/modules/Services.jsm");
Cu.import("resource://gre/modules/AppConstants.jsm");
Cu.import("resource://gre/modules/Task.jsm");
Cu.import("resource:///modules/MigrationUtils.jsm");
Cu.import("resource:///modules/MSMigrationUtils.jsm");
XPCOMUtils.defineLazyModuleGetter(this, "PlacesUtils",
                                  "resource://gre/modules/PlacesUtils.jsm");
XPCOMUtils.defineLazyModuleGetter(this, "ESEDBReader",
                                  "resource:///modules/ESEDBReader.jsm");

const kEdgeRegistryRoot = "SOFTWARE\\Classes\\Local Settings\\Software\\" +
  "Microsoft\\Windows\\CurrentVersion\\AppContainer\\Storage\\" +
  "microsoft.microsoftedge_8wekyb3d8bbwe\\MicrosoftEdge";
const kEdgeReadingListPath = "AC\\MicrosoftEdge\\User\\Default\\DataStore\\Data\\";

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

  migrate: function(aCallback) {
    let rv = true;
    let typedURLs = this._typedURLs;
    let places = [];
    for (let [urlString, time] of typedURLs) {
      let uri;
      try {
        uri = Services.io.newURI(urlString, null, null);
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

    PlacesUtils.asyncHistory.updatePlaces(places, {
      _success: false,
      handleResult: function() {
        // Importing any entry is considered a successful import.
        this._success = true;
      },
      handleError: function() {},
      handleCompletion: function() {
        aCallback(this._success);
      }
    });
  },
}

function EdgeReadingListMigrator() {
}

EdgeReadingListMigrator.prototype = {
  type: MigrationUtils.resourceTypes.BOOKMARKS,
  _dbFile: null,

  get exists() {
    this._dbFile = this._getDBFile();
    return !!this._dbFile;
  },

  _getDBFile() {
    let edgeDir = MSMigrationUtils.getEdgeLocalDataFolder();
    if (!edgeDir) {
      return null;
    }
    edgeDir.appendRelativePath(kEdgeReadingListPath);
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
    let readingListItems = [];
    let database;
    try {
      let logFile = this._dbFile.parent;
      logFile.append("LogFiles");
      database = ESEDBReader.openDB(this._dbFile.parent, this._dbFile, logFile);
      let columns = [
        {name: "URL", type: "string"},
        {name: "Title", type: "string"},
        {name: "AddedDate", type: "date"}
      ];

      // Later versions have an isDeleted column:
      let isDeletedColumn = database.checkForColumn("ReadingList", "isDeleted");
      if (isDeletedColumn && isDeletedColumn.dbType == ESEDBReader.COLUMN_TYPES.JET_coltypBit) {
        columns.push({name: "isDeleted", type: "boolean"});
      }

      let tableReader = database.tableItems("ReadingList", columns);
      for (let row of tableReader) {
        if (!row.isDeleted) {
          readingListItems.push(row);
        }
      }
    } catch (ex) {
      Cu.reportError("Failed to extract Edge reading list information from " +
                     "the database at " + this._dbFile.path + " due to the following error: " + ex);
      // Deliberately make this fail so we expose failure in the UI:
      throw ex;
    } finally {
      if (database) {
        ESEDBReader.closeDB(database);
      }
    }
    if (!readingListItems.length) {
      return;
    }
    let destFolderGuid = yield this._ensureReadingListFolder(parentGuid);
    let exceptionThrown;
    for (let item of readingListItems) {
      let dateAdded = item.AddedDate || new Date();
      yield PlacesUtils.bookmarks.insert({
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
      this.__readingListFolderGuid = (yield PlacesUtils.bookmarks.insert(folderSpec)).guid;
    }
    return this.__readingListFolderGuid;
  }),
};

function EdgeProfileMigrator() {
}

EdgeProfileMigrator.prototype = Object.create(MigratorPrototype);

EdgeProfileMigrator.prototype.getResources = function() {
  let resources = [
    MSMigrationUtils.getBookmarksMigrator(MSMigrationUtils.MIGRATION_TYPE_EDGE),
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
