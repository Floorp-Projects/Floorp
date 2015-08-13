/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

this.EXPORTED_SYMBOLS = ["MSMigrationUtils"];

const { classes: Cc, interfaces: Ci, utils: Cu, results: Cr } = Components;

Cu.import("resource://gre/modules/XPCOMUtils.jsm");
Cu.import("resource://gre/modules/Services.jsm");
Cu.import("resource://gre/modules/Task.jsm");
Cu.import("resource:///modules/MigrationUtils.jsm");

XPCOMUtils.defineLazyModuleGetter(this, "PlacesUtils",
                                  "resource://gre/modules/PlacesUtils.jsm");
XPCOMUtils.defineLazyModuleGetter(this, "WindowsRegistry",
                                  "resource://gre/modules/WindowsRegistry.jsm");

const EDGE_FAVORITES = "AC\\MicrosoftEdge\\User\\Default\\Favorites";

function Bookmarks(migrationType) {
  this._migrationType = migrationType;
}

Bookmarks.prototype = {
  type: MigrationUtils.resourceTypes.BOOKMARKS,

  get exists() !!this._favoritesFolder,

  get importedAppLabel() this._migrationType == MSMigrationUtils.MIGRATION_TYPE_IE ? "IE" : "Edge",

  __favoritesFolder: null,
  get _favoritesFolder() {
    if (!this.__favoritesFolder) {
      if (this._migrationType == MSMigrationUtils.MIGRATION_TYPE_IE) {
        let favoritesFolder = Services.dirsvc.get("Favs", Ci.nsIFile);
        if (favoritesFolder.exists() && favoritesFolder.isReadable())
          return this.__favoritesFolder = favoritesFolder;
      }
      if (this._migrationType == MSMigrationUtils.MIGRATION_TYPE_EDGE) {
        let appData = Services.dirsvc.get("LocalAppData", Ci.nsIFile);
        appData.append("Packages");
        try {
          let edgeDir = appData.clone();
          edgeDir.append("Microsoft.MicrosoftEdge_8wekyb3d8bbwe");
          edgeDir.appendRelativePath(EDGE_FAVORITES);
          if (edgeDir.exists() && edgeDir.isDirectory()) {
            return this.__favoritesFolder = edgeDir;
          }
        } catch (ex) {} /* Ignore e.g. permissions errors here. */

        // Let's try the long way:
        try {
          let dirEntries = appData.directoryEntries;
          while (dirEntries.hasMoreElements()) {
            let subDir = dirEntries.getNext();
            subDir.QueryInterface(Ci.nsIFile);
            if (subDir.leafName.startsWith("Microsoft.MicrosoftEdge")) {
              subDir.appendRelativePath(EDGE_FAVORITES);
              if (subDir.exists() && subDir.isDirectory()) {
                return this.__favoritesFolder = subDir;
              }
            }
          }
        } catch (ex) {
          Cu.reportError("Exception trying to find the Edge favorites directory: " + ex);
        }
      }
    }
    return this.__favoritesFolder;
  },

  __toolbarFolderName: null,
  get _toolbarFolderName() {
    if (!this.__toolbarFolderName) {
      if (this._migrationType == MSMigrationUtils.MIGRATION_TYPE_IE) {
        // Retrieve the name of IE's favorites subfolder that holds the bookmarks
        // in the toolbar. This was previously stored in the registry and changed
        // in IE7 to always be called "Links".
        let folderName = WindowsRegistry.readRegKey(Ci.nsIWindowsRegKey.ROOT_KEY_CURRENT_USER,
                                                    "Software\\Microsoft\\Internet Explorer\\Toolbar",
                                                    "LinksFolderName");
        this.__toolbarFolderName = folderName || "Links";
      } else {
        this.__toolbarFolderName = "Links";
      }
    }
    return this.__toolbarFolderName;
  },

  migrate: function B_migrate(aCallback) {
    return Task.spawn(function* () {
      // Import to the bookmarks menu.
      let folderGuid = PlacesUtils.bookmarks.menuGuid;
      if (!MigrationUtils.isStartupMigration) {
        folderGuid =
          yield MigrationUtils.createImportedBookmarksFolder(this.importedAppLabel, folderGuid);
      }
      yield this._migrateFolder(this._favoritesFolder, folderGuid);
    }.bind(this)).then(() => aCallback(true),
                        e => { Cu.reportError(e); aCallback(false) });
  },

  _migrateFolder: Task.async(function* (aSourceFolder, aDestFolderGuid) {
    // TODO (bug 741993): the favorites order is stored in the Registry, at
    // HCU\Software\Microsoft\Windows\CurrentVersion\Explorer\MenuOrder\Favorites
    // for IE, and in a similar location for Edge.
    // Until we support it, bookmarks are imported in alphabetical order.
    let entries = aSourceFolder.directoryEntries;
    while (entries.hasMoreElements()) {
      let entry = entries.getNext().QueryInterface(Ci.nsIFile);
      try {
        // Make sure that entry.path == entry.target to not follow .lnk folder
        // shortcuts which could lead to infinite cycles.
        // Don't use isSymlink(), since it would throw for invalid
        // lnk files pointing to URLs or to unresolvable paths.
        if (entry.path == entry.target && entry.isDirectory()) {
          let folderGuid;
          if (entry.leafName == this._toolbarFolderName &&
              entry.parent.equals(this._favoritesFolder)) {
            // Import to the bookmarks toolbar.
            folderGuid = PlacesUtils.bookmarks.toolbarGuid;
            if (!MigrationUtils.isStartupMigration) {
              folderGuid =
                yield MigrationUtils.createImportedBookmarksFolder(this.importedAppLabel, folderGuid);
            }
          }
          else {
            // Import to a new folder.
            folderGuid = (yield PlacesUtils.bookmarks.insert({
              type: PlacesUtils.bookmarks.TYPE_FOLDER,
              parentGuid: aDestFolderGuid,
              title: entry.leafName
            })).guid;
          }

          if (entry.isReadable()) {
            // Recursively import the folder.
            yield this._migrateFolder(entry, folderGuid);
          }
        }
        else {
          // Strip the .url extension, to both check this is a valid link file,
          // and get the associated title.
          let matches = entry.leafName.match(/(.+)\.url$/i);
          if (matches) {
            let fileHandler = Cc["@mozilla.org/network/protocol;1?name=file"].
                              getService(Ci.nsIFileProtocolHandler);
            let uri = fileHandler.readURLFile(entry);
            let title = matches[1];

            yield PlacesUtils.bookmarks.insert({
              parentGuid: aDestFolderGuid, url: uri, title
            });
          }
        }
      } catch (ex) {
        Components.utils.reportError("Unable to import " + this.importedAppLabel + " favorite (" + entry.leafName + "): " + ex);
      }
    }
  })
};

let MSMigrationUtils = {
  MIGRATION_TYPE_IE: 1,
  MIGRATION_TYPE_EDGE: 2,
  getBookmarksMigrator(migrationType = this.MIGRATION_TYPE_IE) {
    return new Bookmarks(migrationType);
  },
};
