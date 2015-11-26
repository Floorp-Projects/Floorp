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

  get exists() {
    return !!MSMigrationUtils.getEdgeLocalDataFolder();
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
    let edgeDir = MSMigrationUtils.getEdgeLocalDataFolder();
    if (!edgeDir) {
      return;
    }
    this._readingListExtractor = Cc["@mozilla.org/profile/migrator/edgereadinglistextractor;1"].
                                 createInstance(Ci.nsIEdgeReadingListExtractor);
    edgeDir.appendRelativePath(kEdgeReadingListPath);
    let errorProduced = null;
    if (edgeDir.exists() && edgeDir.isReadable() && edgeDir.isDirectory()) {
      let expectedDir = edgeDir.clone();
      expectedDir.appendRelativePath("nouser1\\120712-0049");
      if (expectedDir.exists() && expectedDir.isReadable() && expectedDir.isDirectory()) {
        yield this._migrateReadingListDB(expectedDir, parentGuid).catch(ex => {
          if (!errorProduced)
            errorProduced = ex;
        });
      } else {
        let getSubdirs = someDir => {
          let subdirs = someDir.directoryEntries;
          let rv = [];
          while (subdirs.hasMoreElements()) {
            let subdir = subdirs.getNext().QueryInterface(Ci.nsIFile);
            if (subdir.isDirectory() && subdir.isReadable()) {
              rv.push(subdir);
            }
          }
          return rv;
        };
        let dirs = getSubdirs(edgeDir).map(getSubdirs);
        for (let dir of dirs) {
          yield this._migrateReadingListDB(dir, parentGuid).catch(ex => {
            if (!errorProduced)
              errorProduced = ex;
          });
        }
      }
    }
    if (errorProduced) {
      throw errorProduced;
    }
  }),
  _migrateReadingListDB: Task.async(function*(dbFile, parentGuid) {
    dbFile.appendRelativePath("DBStore\\spartan.edb");

    if (!dbFile.exists() || !dbFile.isReadable() || !dbFile.isFile()) {
      return;
    }
    let readingListItems;
    try {
      readingListItems = this._readingListExtractor.extract(dbFile.path);
    } catch (ex) {
      Cu.reportError("Failed to extract Edge reading list information from " +
                     "the database at " + dbFile.path + " due to the following error: " + ex);
      // Deliberately make this fail so we expose failure in the UI:
      throw ex;
      return;
    }
    if (!readingListItems.length) {
      return;
    }
    let destFolderGuid = yield this._ensureReadingListFolder(parentGuid);
    for (let i = 0; i < readingListItems.length; i++) {
      let readingListItem = readingListItems.queryElementAt(i, Ci.nsIPropertyBag2);
      let url = readingListItem.get("uri");
      let title = readingListItem.get("title");
      let time = readingListItem.get("time");
      // time is a PRTime, which is microseconds (since unix epoch), or null.
      // We need milliseconds for the date constructor, so divide by 1000:
      let dateAdded = time ? new Date(time / 1000) : new Date();
      yield PlacesUtils.bookmarks.insert({
        parentGuid: destFolderGuid, url: url, title, dateAdded
      });
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

EdgeProfileMigrator.prototype.classDescription = "Edge Profile Migrator";
EdgeProfileMigrator.prototype.contractID = "@mozilla.org/profile/migrator;1?app=browser&type=edge";
EdgeProfileMigrator.prototype.classID = Components.ID("{62e8834b-2d17-49f5-96ff-56344903a2ae}");

this.NSGetFactory = XPCOMUtils.generateNSGetFactory([EdgeProfileMigrator]);
