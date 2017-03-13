/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

var Cc = Components.classes;
var Ci = Components.interfaces;
var Cu = Components.utils;

Cu.import("resource://gre/modules/AppConstants.jsm");
Cu.import("resource://gre/modules/FileUtils.jsm");
Cu.import("resource://gre/modules/osfile.jsm"); /* globals OS */
Cu.import("resource://gre/modules/Services.jsm");
Cu.import("resource://gre/modules/Task.jsm");
Cu.import("resource://gre/modules/XPCOMUtils.jsm");
Cu.import("resource:///modules/MigrationUtils.jsm"); /* globals MigratorPrototype */

XPCOMUtils.defineLazyModuleGetter(this, "Downloads",
                                  "resource://gre/modules/Downloads.jsm");
XPCOMUtils.defineLazyModuleGetter(this, "PropertyListUtils",
                                  "resource://gre/modules/PropertyListUtils.jsm");
XPCOMUtils.defineLazyModuleGetter(this, "PlacesUtils",
                                  "resource://gre/modules/PlacesUtils.jsm");
XPCOMUtils.defineLazyModuleGetter(this, "NetUtil",
                                  "resource://gre/modules/NetUtil.jsm");
XPCOMUtils.defineLazyModuleGetter(this, "FormHistory",
                                  "resource://gre/modules/FormHistory.jsm");

Cu.importGlobalProperties(["URL"]);

function Bookmarks(aBookmarksFile) {
  this._file = aBookmarksFile;
}
Bookmarks.prototype = {
  type: MigrationUtils.resourceTypes.BOOKMARKS,

  migrate: function B_migrate(aCallback) {
    return Task.spawn(function* () {
      let dict = yield new Promise(resolve =>
        PropertyListUtils.read(this._file, resolve)
      );
      if (!dict)
        throw new Error("Could not read Bookmarks.plist");
      let children = dict.get("Children");
      if (!children)
        throw new Error("Invalid Bookmarks.plist format");

      let collection = dict.get("Title") == "com.apple.ReadingList" ?
        this.READING_LIST_COLLECTION : this.ROOT_COLLECTION;
      yield this._migrateCollection(children, collection);
    }.bind(this)).then(() => aCallback(true),
                        e => { Cu.reportError(e); aCallback(false) });
  },

  // Bookmarks collections in Safari.  Constants for migrateCollection.
  ROOT_COLLECTION:         0,
  MENU_COLLECTION:         1,
  TOOLBAR_COLLECTION:      2,
  READING_LIST_COLLECTION: 3,

  /**
   * Recursively migrate a Safari collection of bookmarks.
   *
   * @param aEntries
   *        the collection's children
   * @param aCollection
   *        one of the values above.
   */
  _migrateCollection: Task.async(function* (aEntries, aCollection) {
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
          if (title == "BookmarksBar")
            yield this._migrateCollection(children, this.TOOLBAR_COLLECTION);
          else if (title == "BookmarksMenu")
            yield this._migrateCollection(children, this.MENU_COLLECTION);
          else if (title == "com.apple.ReadingList")
            yield this._migrateCollection(children, this.READING_LIST_COLLECTION);
          else if (entry.get("ShouldOmitFromUI") !== true)
            entriesFiltered.push(entry);
        } else if (type == "WebBookmarkTypeLeaf") {
          entriesFiltered.push(entry);
        }
      }
    } else {
      entriesFiltered = aEntries;
    }

    if (entriesFiltered.length == 0)
      return;

    let folderGuid = -1;
    switch (aCollection) {
      case this.ROOT_COLLECTION: {
        // In Safari, it is possible (though quite cumbersome) to move
        // bookmarks to the bookmarks root, which is the parent folder of
        // all bookmarks "collections".  That is somewhat in parallel with
        // both the places root and the unfiled-bookmarks root.
        // Because the former is only an implementation detail in our UI,
        // the unfiled root seems to be the best choice.
        folderGuid = PlacesUtils.bookmarks.unfiledGuid;
        break;
      }
      case this.MENU_COLLECTION: {
        folderGuid = PlacesUtils.bookmarks.menuGuid;
        if (!MigrationUtils.isStartupMigration) {
          folderGuid =
            yield MigrationUtils.createImportedBookmarksFolder("Safari", folderGuid);
        }
        break;
      }
      case this.TOOLBAR_COLLECTION: {
        folderGuid = PlacesUtils.bookmarks.toolbarGuid;
        if (!MigrationUtils.isStartupMigration) {
          folderGuid =
            yield MigrationUtils.createImportedBookmarksFolder("Safari", folderGuid);
        }
        break;
      }
      case this.READING_LIST_COLLECTION: {
        // Reading list items are imported as regular bookmarks.
        // They are imported under their own folder, created either under the
        // bookmarks menu (in the case of startup migration).
        folderGuid = (yield MigrationUtils.insertBookmarkWrapper({
          parentGuid: PlacesUtils.bookmarks.menuGuid,
          type: PlacesUtils.bookmarks.TYPE_FOLDER,
          title: MigrationUtils.getLocalizedString("importedSafariReadingList"),
        })).guid;
        break;
      }
      default:
        throw new Error("Unexpected value for aCollection!");
    }
    if (folderGuid == -1)
      throw new Error("Invalid folder GUID");

    yield this._migrateEntries(entriesFiltered, folderGuid);
  }),

  // migrate the given array of safari bookmarks to the given places
  // folder.
  _migrateEntries(entries, parentGuid) {
    let convertedEntries = this._convertEntries(entries);
    return MigrationUtils.insertManyBookmarksWrapper(convertedEntries, parentGuid);
  },

  _convertEntries(entries) {
    return entries.map(function(entry) {
      let type = entry.get("WebBookmarkType");
      if (type == "WebBookmarkTypeList" && entry.has("Children")) {
        return {
          title: entry.get("Title"),
          type: PlacesUtils.bookmarks.TYPE_FOLDER,
          children: this._convertEntries(entry.get("Children")),
        };
      }
      if (type == "WebBookmarkTypeLeaf" && entry.has("URLString")) {
        // Check we understand this URL before adding it:
        let url = entry.get("URLString");
        try {
          new URL(url);
        } catch (ex) {
          Cu.reportError(`Ignoring ${url} when importing from Safari because of exception: ${ex}`);
          return null;
        }
        let title;
        if (entry.has("URIDictionary"))
          title = entry.get("URIDictionary").get("title");
        return { url, title };
      }
      return null;
    }).filter(e => !!e);
  },
};

function History(aHistoryFile) {
  this._file = aHistoryFile;
}
History.prototype = {
  type: MigrationUtils.resourceTypes.HISTORY,

  // Helper method for converting the visit date property to a PRTime value.
  // The visit date is stored as a string, so it's not read as a Date
  // object by PropertyListUtils.
  _parseCocoaDate: function H___parseCocoaDate(aCocoaDateStr) {
    let asDouble = parseFloat(aCocoaDateStr);
    if (!isNaN(asDouble)) {
      // reference date of NSDate.
      let date = new Date("1 January 2001, GMT");
      date.setMilliseconds(asDouble * 1000);
      return date * 1000;
    }
    return 0;
  },

  migrate: function H_migrate(aCallback) {
    PropertyListUtils.read(this._file, function migrateHistory(aDict) {
      try {
        if (!aDict)
          throw new Error("Could not read history property list");
        if (!aDict.has("WebHistoryDates"))
          throw new Error("Unexpected history-property list format");

        // Safari's History file contains only top-level urls.  It does not
        // distinguish between typed urls and linked urls.
        let transType = PlacesUtils.history.TRANSITION_LINK;

        let places = [];
        let entries = aDict.get("WebHistoryDates");
        for (let entry of entries) {
          if (entry.has("lastVisitedDate")) {
            let visitDate = this._parseCocoaDate(entry.get("lastVisitedDate"));
            try {
              places.push({ uri: NetUtil.newURI(entry.get("")),
                            title: entry.get("title"),
                            visits: [{ transitionType: transType,
                                       visitDate }] });
            } catch (ex) {
              // Safari's History file may contain malformed URIs which
              // will be ignored.
              Cu.reportError(ex);
            }
          }
        }
        if (places.length > 0) {
          MigrationUtils.insertVisitsWrapper(places, {
            ignoreErrors: true,
            ignoreResults: true,
            handleCompletion(updatedCount) {
              aCallback(updatedCount > 0);
            }
          });
        } else {
          aCallback(false);
        }
      } catch (ex) {
        Cu.reportError(ex);
        aCallback(false);
      }
    }.bind(this));
  }
};

/**
 * Safari's preferences property list is independently used for three purposes:
 * (a) importation of preferences
 * (b) importation of search strings
 * (c) retrieving the home page.
 *
 * So, rather than reading it three times, it's cached and managed here.
 */
function MainPreferencesPropertyList(aPreferencesFile) {
  this._file = aPreferencesFile;
  this._callbacks = [];
}
MainPreferencesPropertyList.prototype = {
  /**
   * @see PropertyListUtils.read
   */
  read: function MPPL_read(aCallback) {
    if ("_dict" in this) {
      aCallback(this._dict);
      return;
    }

    let alreadyReading = this._callbacks.length > 0;
    this._callbacks.push(aCallback);
    if (!alreadyReading) {
      PropertyListUtils.read(this._file, function readPrefs(aDict) {
        this._dict = aDict;
        for (let callback of this._callbacks) {
          try {
            callback(aDict);
          } catch (ex) {
            Cu.reportError(ex);
          }
        }
        this._callbacks.splice(0);
      }.bind(this));
    }
  },

  // Workaround for nsIBrowserProfileMigrator.sourceHomePageURL until
  // it's replaced with an async method.
  _readSync: function MPPL__readSync() {
    if ("_dict" in this)
      return this._dict;

    let inputStream = Cc["@mozilla.org/network/file-input-stream;1"].
                      createInstance(Ci.nsIFileInputStream);
    inputStream.init(this._file, -1, -1, 0);
    let binaryStream = Cc["@mozilla.org/binaryinputstream;1"].
                       createInstance(Ci.nsIBinaryInputStream);
    binaryStream.setInputStream(inputStream);
    let bytes = binaryStream.readByteArray(inputStream.available());
    this._dict = PropertyListUtils._readFromArrayBufferSync(
      new Uint8Array(bytes).buffer);
    return this._dict;
  }
};

function SearchStrings(aMainPreferencesPropertyListInstance) {
  this._mainPreferencesPropertyList = aMainPreferencesPropertyListInstance;
}
SearchStrings.prototype = {
  type: MigrationUtils.resourceTypes.OTHERDATA,

  migrate: function SS_migrate(aCallback) {
    this._mainPreferencesPropertyList.read(MigrationUtils.wrapMigrateFunction(
      function migrateSearchStrings(aDict) {
        if (!aDict)
          throw new Error("Could not get preferences dictionary");

        if (aDict.has("RecentSearchStrings")) {
          let recentSearchStrings = aDict.get("RecentSearchStrings");
          if (recentSearchStrings && recentSearchStrings.length > 0) {
            let changes = recentSearchStrings.map((searchString) => (
              {op: "add",
               fieldname: "searchbar-history",
               value: searchString}));
            FormHistory.update(changes);
          }
        }
      }, aCallback));
  }
};

function SafariProfileMigrator() {
}

SafariProfileMigrator.prototype = Object.create(MigratorPrototype);

SafariProfileMigrator.prototype.getResources = function SM_getResources() {
  let profileDir = FileUtils.getDir("ULibDir", ["Safari"], false);
  if (!profileDir.exists())
    return null;

  let resources = [];
  let pushProfileFileResource = function(aFileName, aConstructor) {
    let file = profileDir.clone();
    file.append(aFileName);
    if (file.exists())
      resources.push(new aConstructor(file));
  };

  pushProfileFileResource("History.plist", History);
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

  return resources;
};

SafariProfileMigrator.prototype.getLastUsedDate = function SM_getLastUsedDate() {
  let profileDir = FileUtils.getDir("ULibDir", ["Safari"], false);
  let datePromises = ["Bookmarks.plist", "History.plist"].map(file => {
    let path = OS.Path.join(profileDir.path, file);
    return OS.File.stat(path).catch(() => null).then(info => {
      return info ? info.lastModificationDate : 0;
    });
  });
  return Promise.all(datePromises).then(dates => {
    return new Date(Math.max.apply(Math, dates));
  });
};

Object.defineProperty(SafariProfileMigrator.prototype, "mainPreferencesPropertyList", {
  get: function get_mainPreferencesPropertyList() {
    if (this._mainPreferencesPropertyList === undefined) {
      let file = FileUtils.getDir("UsrPrfs", [], false);
      if (file.exists()) {
        file.append("com.apple.Safari.plist");
        if (file.exists()) {
          this._mainPreferencesPropertyList =
            new MainPreferencesPropertyList(file);
          return this._mainPreferencesPropertyList;
        }
      }
      this._mainPreferencesPropertyList = null;
      return this._mainPreferencesPropertyList;
    }
    return this._mainPreferencesPropertyList;
  }
});

Object.defineProperty(SafariProfileMigrator.prototype, "sourceHomePageURL", {
  get: function get_sourceHomePageURL() {
    if (this.mainPreferencesPropertyList) {
      let dict = this.mainPreferencesPropertyList._readSync();
      if (dict.has("HomePage"))
        return dict.get("HomePage");
    }
    return "";
  }
});

SafariProfileMigrator.prototype.classDescription = "Safari Profile Migrator";
SafariProfileMigrator.prototype.contractID = "@mozilla.org/profile/migrator;1?app=browser&type=safari";
SafariProfileMigrator.prototype.classID = Components.ID("{4b609ecf-60b2-4655-9df4-dc149e474da1}");

this.NSGetFactory = XPCOMUtils.generateNSGetFactory([SafariProfileMigrator]);
