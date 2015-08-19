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
XPCOMUtils.defineLazyModuleGetter(this, "ctypes",
                                  "resource://gre/modules/ctypes.jsm");

const EDGE_COOKIE_PATH_OPTIONS = ["", "#!001\\", "#!002\\"];
const EDGE_COOKIES_SUFFIX = "MicrosoftEdge\\Cookies";
const EDGE_FAVORITES = "AC\\MicrosoftEdge\\User\\Default\\Favorites";

Cu.importGlobalProperties(["File"]);

////////////////////////////////////////////////////////////////////////////////
//// Helpers.

let CtypesHelpers = {
  _structs: {},
  _functions: {},
  _libs: {},

  /**
   * Must be invoked once before first use of any of the provided helpers.
   */
  initialize() {
    const WORD = ctypes.uint16_t;
    const DWORD = ctypes.uint32_t;
    const BOOL = ctypes.int;

    this._structs.SYSTEMTIME = new ctypes.StructType('SYSTEMTIME', [
      {wYear: WORD},
      {wMonth: WORD},
      {wDayOfWeek: WORD},
      {wDay: WORD},
      {wHour: WORD},
      {wMinute: WORD},
      {wSecond: WORD},
      {wMilliseconds: WORD}
    ]);

    this._structs.FILETIME = new ctypes.StructType('FILETIME', [
      {dwLowDateTime: DWORD},
      {dwHighDateTime: DWORD}
    ]);

    try {
      this._libs.kernel32 = ctypes.open("Kernel32");
      this._functions.FileTimeToSystemTime =
        this._libs.kernel32.declare("FileTimeToSystemTime",
                                    ctypes.default_abi,
                                    BOOL,
                                    this._structs.FILETIME.ptr,
                                    this._structs.SYSTEMTIME.ptr);
    } catch (ex) {
      this.finalize();
    }
  },

  /**
   * Must be invoked once after last use of any of the provided helpers.
   */
  finalize() {
    this._structs = {};
    this._functions = {};
    for each (let lib in this._libs) {
      try {
        lib.close();
      } catch (ex) {}
    }
    this._libs = {};
  },

  /**
   * Converts a FILETIME struct (2 DWORDS), to a SYSTEMTIME struct,
   * and then deduces the number of seconds since the epoch (which
   * is the data we want for the cookie expiry date).
   *
   * @param aTimeHi
   *        Least significant DWORD.
   * @param aTimeLo
   *        Most significant DWORD.
   * @return the number of seconds since the epoch
   */
  fileTimeToSecondsSinceEpoch(aTimeHi, aTimeLo) {
    let fileTime = this._structs.FILETIME();
    fileTime.dwLowDateTime = aTimeLo;
    fileTime.dwHighDateTime = aTimeHi;
    let systemTime = this._structs.SYSTEMTIME();
    let result = this._functions.FileTimeToSystemTime(fileTime.address(),
                                                      systemTime.address());
    if (result == 0)
      throw new Error(ctypes.winLastError);

    // System time is in UTC, so we use Date.UTC to get milliseconds from epoch,
    // then divide by 1000 to get seconds, and round down:
    return Math.floor(Date.UTC(systemTime.wYear,
                               systemTime.wMonth - 1,
                               systemTime.wDay,
                               systemTime.wHour,
                               systemTime.wMinute,
                               systemTime.wSecond,
                               systemTime.wMilliseconds) / 1000);
  }
};

/**
 * Checks whether an host is an IP (v4 or v6) address.
 *
 * @param aHost
 *        The host to check.
 * @return whether aHost is an IP address.
 */
function hostIsIPAddress(aHost) {
  try {
    Services.eTLD.getBaseDomainFromHost(aHost);
  } catch (e if e.result == Cr.NS_ERROR_HOST_IS_IP_ADDRESS) {
    return true;
  } catch (e) {}
  return false;
}

let gEdgeDir;
function getEdgeLocalDataFolder() {
  if (gEdgeDir) {
    return gEdgeDir.clone();
  }
  let packages = Services.dirsvc.get("LocalAppData", Ci.nsIFile);
  packages.append("Packages");
  let edgeDir = packages.clone();
  edgeDir.append("Microsoft.MicrosoftEdge_8wekyb3d8bbwe");
  try {
    if (edgeDir.exists() && edgeDir.isReadable() && edgeDir.isDirectory()) {
      gEdgeDir = edgeDir;
      return edgeDir.clone();
    }

    // Let's try the long way:
    let dirEntries = packages.directoryEntries;
    while (dirEntries.hasMoreElements()) {
      let subDir = dirEntries.getNext();
      subDir.QueryInterface(Ci.nsIFile);
      if (subDir.leafName.startsWith("Microsoft.MicrosoftEdge") && subDir.isReadable() &&
          subDir.isDirectory()) {
        gEdgeDir = subDir;
        return subDir.clone();
      }
    }
  } catch (ex) {
    Cu.reportError("Exception trying to find the Edge favorites directory: " + ex);
  }
  return null;
}


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
        let edgeDir = getEdgeLocalDataFolder();
        if (edgeDir) {
          edgeDir.appendRelativePath(EDGE_FAVORITES);
          if (edgeDir.exists() && edgeDir.isReadable() && edgeDir.isDirectory()) {
            return this.__favoritesFolder = edgeDir;
          }
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

function Cookies(migrationType) {
  this._migrationType = migrationType;
}

Cookies.prototype = {
  type: MigrationUtils.resourceTypes.COOKIES,

  get exists() {
    if (this._migrationType == MSMigrationUtils.MIGRATION_TYPE_IE) {
      return !!this._cookiesFolder;
    }
    return !!this._cookiesFolders;
  },

  __cookiesFolder: null,
  get _cookiesFolder() {
    // Edge stores cookies in a number of places, and this shouldn't get called:
    if (this._migrationType != MSMigrationUtils.MIGRATION_TYPE_IE) {
      throw new Error("Shouldn't be looking for a single cookie folder unless we're migrating IE");
    }

    // Cookies are stored in txt files, in a Cookies folder whose path varies
    // across the different OS versions.  CookD takes care of most of these
    // cases, though, in Windows Vista/7, UAC makes a difference.
    // If UAC is enabled, the most common destination is CookD/Low.  Though,
    // if the user runs the application in administrator mode or disables UAC,
    // cookies are stored in the original CookD destination.  Cause running the
    // browser in administrator mode is unsafe and discouraged, we just care
    // about the UAC state.
    if (!this.__cookiesFolder) {
      let cookiesFolder = Services.dirsvc.get("CookD", Ci.nsIFile);
      if (cookiesFolder.exists() && cookiesFolder.isReadable()) {
        // Check if UAC is enabled.
        if (Services.appinfo.QueryInterface(Ci.nsIWinAppHelper).userCanElevate) {
          cookiesFolder.append("Low");
        }
        this.__cookiesFolder = cookiesFolder;
      }
    }
    return this.__cookiesFolder;
  },

  __cookiesFolders: null,
  get _cookiesFolders() {
    if (this._migrationType != MSMigrationUtils.MIGRATION_TYPE_EDGE) {
      throw new Error("Shouldn't be looking for multiple cookie folders unless we're migrating Edge");
    }

    let folders = [];
    let edgeDir = getEdgeLocalDataFolder();
    if (edgeDir) {
      edgeDir.append("AC");
      for (let path of EDGE_COOKIE_PATH_OPTIONS) {
        let folder = edgeDir.clone();
        let fullPath = path + EDGE_COOKIES_SUFFIX;
        folder.appendRelativePath(fullPath);
        if (folder.exists() && folder.isReadable() && folder.isDirectory()) {
          folders.push(folder);
        }
      }
    }
    return this.__cookiesFolders = folders.length ? folders : null;
  },

  migrate(aCallback) {
    CtypesHelpers.initialize();

    let cookiesGenerator = (function genCookie() {
      let success = false;
      let folders = this._migrationType == MSMigrationUtils.MIGRATION_TYPE_EDGE ?
                      this.__cookiesFolders : [this.__cookiesFolder];
      for (let folder of folders) {
        let entries = folder.directoryEntries;
        while (entries.hasMoreElements()) {
          let entry = entries.getNext().QueryInterface(Ci.nsIFile);
          // Skip eventual bogus entries.
          if (!entry.isFile() || !/\.txt$/.test(entry.leafName))
            continue;

          this._readCookieFile(entry, function(aSuccess) {
            // Importing even a single cookie file is considered a success.
            if (aSuccess)
              success = true;
            try {
              cookiesGenerator.next();
            } catch (ex) {}
          });

          yield undefined;
        }
      }

      CtypesHelpers.finalize();

      aCallback(success);
    }).apply(this);
    cookiesGenerator.next();
  },

  _readCookieFile(aFile, aCallback) {
    let fileReader = Cc["@mozilla.org/files/filereader;1"].
                     createInstance(Ci.nsIDOMFileReader);
    let onLoadEnd = () => {
      fileReader.removeEventListener("loadend", onLoadEnd, false);

      if (fileReader.readyState != fileReader.DONE) {
        Cu.reportError("Could not read cookie contents: " + fileReader.error);
        aCallback(false);
        return;
      }

      let success = true;
      try {
        this._parseCookieBuffer(fileReader.result);
      } catch (ex) {
        Components.utils.reportError("Unable to migrate cookie: " + ex);
        success = false;
      } finally {
        aCallback(success);
      }
    };
    fileReader.addEventListener("loadend", onLoadEnd, false);
    fileReader.readAsText(new File(aFile));
  },

  /**
   * Parses a cookie file buffer and returns an array of the contained cookies.
   *
   * The cookie file format is a newline-separated-values with a "*" used as
   * delimeter between multiple records.
   * Each cookie has the following fields:
   *  - name
   *  - value
   *  - host/path
   *  - flags
   *  - Expiration time most significant integer
   *  - Expiration time least significant integer
   *  - Creation time most significant integer
   *  - Creation time least significant integer
   *  - Record delimiter "*"
   *
   * @note All the times are in FILETIME format.
   */
  _parseCookieBuffer(aTextBuffer) {
    // Note the last record is an empty string.
    let records = [r for each (r in aTextBuffer.split("*\n")) if (r)];
    for (let record of records) {
      let [name, value, hostpath, flags,
           expireTimeLo, expireTimeHi] = record.split("\n");

      // IE stores deleted cookies with a zero-length value, skip them.
      if (value.length == 0)
        continue;

      let hostLen = hostpath.indexOf("/");
      let host = hostpath.substr(0, hostLen);
      let path = hostpath.substr(hostLen);

      // For a non-null domain, assume it's what Mozilla considers
      // a domain cookie.  See bug 222343.
      if (host.length > 0) {
        // Fist delete any possible extant matching host cookie.
        Services.cookies.remove(host, name, path, false);
        // Now make it a domain cookie.
        if (host[0] != "." && !hostIsIPAddress(host))
          host = "." + host;
      }

      let expireTime = CtypesHelpers.fileTimeToSecondsSinceEpoch(Number(expireTimeHi),
                                                                 Number(expireTimeLo));
      Services.cookies.add(host,
                           path,
                           name,
                           value,
                           Number(flags) & 0x1, // secure
                           false, // httpOnly
                           false, // session
                           expireTime);
    }
  }
};


let MSMigrationUtils = {
  MIGRATION_TYPE_IE: 1,
  MIGRATION_TYPE_EDGE: 2,
  getBookmarksMigrator(migrationType = this.MIGRATION_TYPE_IE) {
    return new Bookmarks(migrationType);
  },
  getCookiesMigrator(migrationType = this.MIGRATION_TYPE_IE) {
    return new Cookies(migrationType);
  },
};
