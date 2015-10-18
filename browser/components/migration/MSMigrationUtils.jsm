/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

this.EXPORTED_SYMBOLS = ["MSMigrationUtils"];

const { classes: Cc, interfaces: Ci, utils: Cu, results: Cr } = Components;

Cu.import("resource://gre/modules/AppConstants.jsm");
Cu.import("resource://gre/modules/XPCOMUtils.jsm");
Cu.import("resource://gre/modules/Services.jsm");
Cu.import("resource://gre/modules/Task.jsm");
Cu.import("resource:///modules/MigrationUtils.jsm");
Cu.import("resource://gre/modules/NetUtil.jsm");
Cu.import("resource://gre/modules/LoginHelper.jsm");

XPCOMUtils.defineLazyModuleGetter(this, "PlacesUtils",
                                  "resource://gre/modules/PlacesUtils.jsm");
XPCOMUtils.defineLazyModuleGetter(this, "WindowsRegistry",
                                  "resource://gre/modules/WindowsRegistry.jsm");
XPCOMUtils.defineLazyModuleGetter(this, "ctypes",
                                  "resource://gre/modules/ctypes.jsm");

const EDGE_COOKIE_PATH_OPTIONS = ["", "#!001\\", "#!002\\"];
const EDGE_COOKIES_SUFFIX = "MicrosoftEdge\\Cookies";
const EDGE_FAVORITES = "AC\\MicrosoftEdge\\User\\Default\\Favorites";
const EDGE_READINGLIST = "AC\\MicrosoftEdge\\User\\Default\\DataStore\\Data\\";
const FREE_CLOSE_FAILED = 0;
const INTERNET_EXPLORER_EDGE_GUID = [0x3CCD5499,
                                     0x4B1087A8,
                                     0x886015A2,
                                     0x553BDD88];
const RESULT_SUCCESS = 0;
const VAULT_ENUMERATE_ALL_ITEMS = 512;
const WEB_CREDENTIALS_VAULT_ID = [0x4BF4C442,
                                  0x41A09B8A,
                                  0x4ADD80B3,
                                  0x28DB4D70];

Cu.importGlobalProperties(["File"]);

////////////////////////////////////////////////////////////////////////////////
//// Helpers.

const wintypes = {
  BOOL: ctypes.int,
  DWORD: ctypes.uint32_t,
  DWORDLONG: ctypes.uint64_t,
  CHAR: ctypes.char,
  PCHAR: ctypes.char.ptr,
  LPCWSTR: ctypes.char16_t.ptr,
  PDWORD: ctypes.uint32_t.ptr,
  VOIDP: ctypes.voidptr_t,
  WORD: ctypes.uint16_t,
}

// TODO: Bug 1202978 - Refactor MSMigrationUtils ctypes helpers
function CtypesKernelHelpers() {
  this._structs = {};
  this._functions = {};
  this._libs = {};

  this._structs.SYSTEMTIME = new ctypes.StructType("SYSTEMTIME", [
    {wYear: wintypes.WORD},
    {wMonth: wintypes.WORD},
    {wDayOfWeek: wintypes.WORD},
    {wDay: wintypes.WORD},
    {wHour: wintypes.WORD},
    {wMinute: wintypes.WORD},
    {wSecond: wintypes.WORD},
    {wMilliseconds: wintypes.WORD}
  ]);

  this._structs.FILETIME = new ctypes.StructType("FILETIME", [
    {dwLowDateTime: wintypes.DWORD},
    {dwHighDateTime: wintypes.DWORD}
  ]);

  try {
    this._libs.kernel32 = ctypes.open("Kernel32");

    this._functions.FileTimeToSystemTime =
      this._libs.kernel32.declare("FileTimeToSystemTime",
                                  ctypes.default_abi,
                                  wintypes.BOOL,
                                  this._structs.FILETIME.ptr,
                                  this._structs.SYSTEMTIME.ptr);
  } catch (ex) {
    this.finalize();
  }
}

CtypesKernelHelpers.prototype = {
  /**
   * Must be invoked once after last use of any of the provided helpers.
   */
  finalize() {
    this._structs = {};
    this._functions = {};
    for (let key in this._libs) {
      let lib = this._libs[key];
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

function CtypesVaultHelpers() {
  this._structs = {};
  this._functions = {};

  this._structs.GUID = new ctypes.StructType("GUID", [
    {id: wintypes.DWORD.array(4)},
  ]);

  this._structs.VAULT_ITEM_ELEMENT = new ctypes.StructType("VAULT_ITEM_ELEMENT", [
    // not documented
    {schemaElementId: wintypes.DWORD},
    // not documented
    {unknown1: wintypes.DWORD},
    // vault type
    {type: wintypes.DWORD},
    // not documented
    {unknown2: wintypes.DWORD},
    // value of the item
    {itemValue: wintypes.LPCWSTR},
    // not documented
    {unknown3: wintypes.CHAR.array(12)},
  ]);

  this._structs.VAULT_ELEMENT = new ctypes.StructType("VAULT_ELEMENT", [
    // vault item schemaId
    {schemaId: this._structs.GUID},
    // a pointer to the name of the browser VAULT_ITEM_ELEMENT
    {pszCredentialFriendlyName: wintypes.LPCWSTR},
    // a pointer to the url VAULT_ITEM_ELEMENT
    {pResourceElement: this._structs.VAULT_ITEM_ELEMENT.ptr},
    // a pointer to the username VAULT_ITEM_ELEMENT
    {pIdentityElement: this._structs.VAULT_ITEM_ELEMENT.ptr},
    // not documented
    {pAuthenticatorElement: this._structs.VAULT_ITEM_ELEMENT.ptr},
    // not documented
    {pPackageSid: this._structs.VAULT_ITEM_ELEMENT.ptr},
    // time stamp in local format
    {lowLastModified: wintypes.DWORD},
    {highLastModified: wintypes.DWORD},
    // not documented
    {flags: wintypes.DWORD},
    // not documented
    {dwPropertiesCount: wintypes.DWORD},
    // not documented
    {pPropertyElements: this._structs.VAULT_ITEM_ELEMENT.ptr},
  ]);

  try {
    this._vaultcliLib = ctypes.open("vaultcli.dll");

    this._functions.VaultOpenVault =
      this._vaultcliLib.declare("VaultOpenVault",
                                ctypes.winapi_abi,
                                wintypes.DWORD,
                                // GUID
                                this._structs.GUID.ptr,
                                // Flags
                                wintypes.DWORD,
                                // Vault Handle
                                wintypes.VOIDP.ptr);
    this._functions.VaultEnumerateItems =
      this._vaultcliLib.declare("VaultEnumerateItems",
                                ctypes.winapi_abi,
                                wintypes.DWORD,
                                // Vault Handle
                                wintypes.VOIDP,
                                // Flags
                                wintypes.DWORD,
                                // Items Count
                                wintypes.PDWORD,
                                // Items
                                ctypes.voidptr_t);
    this._functions.VaultCloseVault =
      this._vaultcliLib.declare("VaultCloseVault",
                                ctypes.winapi_abi,
                                wintypes.DWORD,
                                // Vault Handle
                                wintypes.VOIDP);
    this._functions.VaultGetItem =
      this._vaultcliLib.declare("VaultGetItem",
                                ctypes.winapi_abi,
                                wintypes.DWORD,
                                // Vault Handle
                                wintypes.VOIDP,
                                // Schema Id
                                this._structs.GUID.ptr,
                                // Resource
                                this._structs.VAULT_ITEM_ELEMENT.ptr,
                                // Identity
                                this._structs.VAULT_ITEM_ELEMENT.ptr,
                                // Package Sid
                                this._structs.VAULT_ITEM_ELEMENT.ptr,
                                // HWND Owner
                                wintypes.DWORD,
                                // Flags
                                wintypes.DWORD,
                                // Items
                                this._structs.VAULT_ELEMENT.ptr.ptr);
    this._functions.VaultFree =
      this._vaultcliLib.declare("VaultFree",
                                ctypes.winapi_abi,
                                wintypes.DWORD,
                                // Memory
                                this._structs.VAULT_ELEMENT.ptr);
  } catch (ex) {
    this.finalize();
  }
}

CtypesVaultHelpers.prototype = {
  /**
   * Must be invoked once after last use of any of the provided helpers.
   */
  finalize() {
    this._structs = {};
    this._functions = {};
    try {
      this._vaultcliLib.close();
    } catch (ex) {}
    this._vaultcliLib = null;
  }
}

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

var gEdgeDir;
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
};


function Bookmarks(migrationType) {
  this._migrationType = migrationType;
}

Bookmarks.prototype = {
  type: MigrationUtils.resourceTypes.BOOKMARKS,

  get exists() {
    return !!this._favoritesFolder;
  },

  get importedAppLabel() {
    return this._migrationType == MSMigrationUtils.MIGRATION_TYPE_IE ? "IE" : "Edge";
  },

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

      if (this._migrationType == MSMigrationUtils.MIGRATION_TYPE_EDGE) {
        yield this._migrateEdgeReadingList(PlacesUtils.bookmarks.menuGuid);
      }
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
  }),

  _migrateEdgeReadingList: Task.async(function*(parentGuid) {
    let edgeDir = getEdgeLocalDataFolder();
    if (!edgeDir) {
      return;
    }

    this._readingListExtractor = Cc["@mozilla.org/profile/migrator/edgereadinglistextractor;1"].
                                 createInstance(Ci.nsIEdgeReadingListExtractor);
    edgeDir.appendRelativePath(EDGE_READINGLIST);
    if (edgeDir.exists() && edgeDir.isReadable() && edgeDir.isDirectory()) {
      let expectedDir = edgeDir.clone();
      expectedDir.appendRelativePath("nouser1\\120712-0049");
      if (expectedDir.exists() && expectedDir.isReadable() && expectedDir.isDirectory()) {
        yield this._migrateEdgeReadingListDB(expectedDir, parentGuid);
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
          yield this._migrateEdgeReadingListDB(dir, parentGuid);
        }
      }
    }
  }),
  _migrateEdgeReadingListDB: Task.async(function*(dbFile, parentGuid) {
    dbFile.appendRelativePath("DBStore\\spartan.edb");
    if (!dbFile.exists() || !dbFile.isReadable() || !dbFile.isFile()) {
      return;
    }
    let readingListItems;
    try {
      readingListItems = this._readingListExtractor.extract(dbFile.path);
    } catch (ex) {
      Cu.reportError("Failed to extract Edge reading list information from " +
                     "the database at " + dbPath + " due to the following error: " + ex);
      return;
    }
    if (!readingListItems.length) {
      return;
    }
    let destFolderGuid = yield this._ensureEdgeReadingListFolder(parentGuid);
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

  _ensureEdgeReadingListFolder: Task.async(function*(parentGuid) {
    if (!this.__edgeReadingListFolderGuid) {
      let folderTitle = MigrationUtils.getLocalizedString("importedEdgeReadingList");
      let folderSpec = {type: PlacesUtils.bookmarks.TYPE_FOLDER, parentGuid, title: folderTitle};
      this.__edgeReadingListFolderGuid = (yield PlacesUtils.bookmarks.insert(folderSpec)).guid;
    }
    return this.__edgeReadingListFolderGuid;
  }),
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
    this.ctypesKernelHelpers = new CtypesKernelHelpers();

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

      this.ctypesKernelHelpers.finalize();

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
   * Unfortunately, "*" can also occur inside the value of the cookie, so we
   * can't rely exclusively on it as a record separator.
   *
   * @note All the times are in FILETIME format.
   */
  _parseCookieBuffer(aTextBuffer) {
    // Note the last record is an empty string...
    let records = [];
    let lines = aTextBuffer.split("\n");
    while (lines.length > 0) {
      let record = lines.splice(0, 9);
      // ... which means this is going to be a 1-element array for that record
      if (record.length > 1) {
        records.push(record);
      }
    }
    for (let record of records) {
      let [name, value, hostpath, flags,
           expireTimeLo, expireTimeHi] = record;

      // IE stores deleted cookies with a zero-length value, skip them.
      if (value.length == 0)
        continue;

      // IE sometimes has cookies created by apps that use "~~local~~/local/file/path"
      // as the hostpath, ignore those:
      if (hostpath.startsWith("~~local~~"))
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

      // Fallback: expire in 1h (NB: time is in seconds since epoch, so we have
      // to divide the result of Date.now() (which is in milliseconds) by 1000).
      let expireTime = Math.floor(Date.now() / 1000) + 3600;
      try {
        expireTime = this.ctypesKernelHelpers.fileTimeToSecondsSinceEpoch(Number(expireTimeHi),
                                                                          Number(expireTimeLo));
      } catch (ex) {
        Cu.reportError("Failed to get expiry time for cookie for " + host);
      }

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

function getTypedURLs(registryKeyPath) {
  // The list of typed URLs is a sort of annotation stored in the registry.
  // The number of entries stored is not UI-configurable, but has changed
  // between different Windows versions. We just keep reading up to the first
  // non-existing entry to support different limits / states of the registry.
  let typedURLs = new Map();
  let typedURLKey = Cc["@mozilla.org/windows-registry-key;1"].
                    createInstance(Ci.nsIWindowsRegKey);
  let typedURLTimeKey = Cc["@mozilla.org/windows-registry-key;1"].
                        createInstance(Ci.nsIWindowsRegKey);
  let cTypes = new CtypesKernelHelpers();
  try {
    typedURLKey.open(Ci.nsIWindowsRegKey.ROOT_KEY_CURRENT_USER,
                     registryKeyPath + "\\TypedURLs",
                     Ci.nsIWindowsRegKey.ACCESS_READ);
    try {
      typedURLTimeKey.open(Ci.nsIWindowsRegKey.ROOT_KEY_CURRENT_USER,
                           registryKeyPath + "\\TypedURLsTime",
                           Ci.nsIWindowsRegKey.ACCESS_READ);
    } catch (ex) {
      typedURLTimeKey = null;
    }
    let entryName;
    for (let entry = 1; typedURLKey.hasValue((entryName = "url" + entry)); entry++) {
      let url = typedURLKey.readStringValue(entryName);
      let timeTyped = 0;
      if (typedURLTimeKey && typedURLTimeKey.hasValue(entryName)) {
        let urlTime = "";
        try {
          urlTime = typedURLTimeKey.readBinaryValue(entryName);
        } catch (ex) {
          Cu.reportError("Couldn't read url time for " + entryName);
        }
        if (urlTime.length == 8) {
          let urlTimeHex = [];
          for (let i = 0; i < 8; i++) {
            let c = urlTime.charCodeAt(i).toString(16);
            if (c.length == 1)
              c = "0" + c;
            urlTimeHex.unshift(c);
          }
          try {
            let hi = parseInt(urlTimeHex.slice(0, 4).join(''), 16);
            let lo = parseInt(urlTimeHex.slice(4, 8).join(''), 16);
            // Convert to seconds since epoch:
            timeTyped = cTypes.fileTimeToSecondsSinceEpoch(hi, lo);
            // Callers expect PRTime, which is microseconds since epoch:
            timeTyped *= 1000 * 1000;
          } catch (ex) {
            // Ignore conversion exceptions. Callers will have to deal
            // with the fallback value (0).
          }
        }
      }
      typedURLs.set(url, timeTyped);
    }
  } catch (ex) {
    Cu.reportError("Error reading typed URL history: " + ex);
  } finally {
    typedURLKey.close();
    typedURLTimeKey.close();
    cTypes.finalize();
  }
  return typedURLs;
}


// Migrator for form passwords on Windows 8 and higher.
function WindowsVaultFormPasswords () {
}

WindowsVaultFormPasswords.prototype = {
  type: MigrationUtils.resourceTypes.PASSWORDS,

  get exists() {
    // work only on windows 8+
    if (AppConstants.isPlatformAndVersionAtLeast("win", "6.2")) {
      // check if there are passwords available for migration.
      return this.migrate(() => {}, true);
    }
    return false;
  },

  /**
   * If aOnlyCheckExists is false, import the form passwords on Windows 8 and higher from the vault
   * and then call the aCallback.
   * Otherwise, check if there are passwords in the vault.
   * @param {function} aCallback - a callback called when the migration is done.
   * @param {boolean} [aOnlyCheckExists=false] - if aOnlyCheckExists is true, just check if there are some
   * passwords to migrate. Import the passwords from the vault and call aCallback otherwise.
   * @return true if there are passwords in the vault and aOnlyCheckExists is set to true,
   * false if there is no password in the vault and aOnlyCheckExists is set to true, undefined if
   * aOnlyCheckExists is set to false.
   */
  migrate(aCallback, aOnlyCheckExists = false) {
    // check if the vault item is an IE/Edge one
    function _isIEOrEdgePassword(id) {
      return id[0] == INTERNET_EXPLORER_EDGE_GUID[0] &&
             id[1] == INTERNET_EXPLORER_EDGE_GUID[1] &&
             id[2] == INTERNET_EXPLORER_EDGE_GUID[2] &&
             id[3] == INTERNET_EXPLORER_EDGE_GUID[3];
    }

    let ctypesVaultHelpers = new CtypesVaultHelpers();
    let ctypesKernelHelpers = new CtypesKernelHelpers();
    let migrationSucceeded = true;
    let successfulVaultOpen = false;
    let error, vault;
    try {

      // web credentials vault id
      let vaultGuid = new ctypesVaultHelpers._structs.GUID(WEB_CREDENTIALS_VAULT_ID);
      // number of available vaults
      let vaultCount = new wintypes.DWORD;
      error = new wintypes.DWORD;
      // web credentials vault
      vault = new wintypes.VOIDP;
      // open the current vault using the vaultGuid
      error = ctypesVaultHelpers._functions.VaultOpenVault(vaultGuid.address(), 0, vault.address());
      if (error != RESULT_SUCCESS) {
        throw new Error("Unable to open Vault: " + error);
      }
      successfulVaultOpen = true;

      let item = new ctypesVaultHelpers._structs.VAULT_ELEMENT.ptr;
      let itemCount = new wintypes.DWORD;
      // enumerate all the available items. This api is going to return a table of all the
      // available items and item is going to point to the first element of this table.
      error = ctypesVaultHelpers._functions.VaultEnumerateItems(vault, VAULT_ENUMERATE_ALL_ITEMS,
                                                                itemCount.address(),
                                                                item.address());
      if (error != RESULT_SUCCESS) {
        throw new Error("Unable to enumerate Vault items: " + error);
      }
      for (let j = 0; j < itemCount.value; j++) {
        try {
          // if it's not an ie/edge password, skip it
          if (!_isIEOrEdgePassword(item.contents.schemaId.id)) {
            continue;
          }
          // if aOnlyCheckExists is set to true, the purpose of the call is to return true if there is at
          // least a password which is true in this case because a password was by now already found
          if (aOnlyCheckExists) {
            return true;
          }
          let url = item.contents.pResourceElement.contents.itemValue.readString();
          let username = item.contents.pIdentityElement.contents.itemValue.readString();
          // the current login credential object
          let credential = new ctypesVaultHelpers._structs.VAULT_ELEMENT.ptr;
          error = ctypesVaultHelpers._functions.VaultGetItem(vault,
                                                             item.contents.schemaId.address(),
                                                             item.contents.pResourceElement,
                                                             item.contents.pIdentityElement, null,
                                                             0, 0, credential.address());
          if (error != RESULT_SUCCESS) {
            throw new Error("Unable to get item: " + error);
          }

          let password = credential.contents.pAuthenticatorElement.contents.itemValue.readString();
          let creation = Date.now();
          try {
            // login manager wants time in milliseconds since epoch, so convert
            // to seconds since epoch and multiply to get milliseconds:
            creation = ctypesKernelHelpers.
                         fileTimeToSecondsSinceEpoch(item.contents.highLastModified,
                                                     item.contents.lowLastModified) * 1000;
          } catch (ex) {
            // Ignore exceptions in the dates and just create the login for right now.
          }
          // create a new login
          let login = {
            username, password,
            hostname: NetUtil.newURI(url).prePath,
            timeCreated: creation,
          };
          LoginHelper.maybeImportLogin(login);

          // close current item
          error = ctypesVaultHelpers._functions.VaultFree(credential);
          if (error == FREE_CLOSE_FAILED) {
            throw new Error("Unable to free item: " + error);
          }
        } catch (e) {
          migrationSucceeded = false;
          Cu.reportError(e);
        } finally {
          // move to next item in the table returned by VaultEnumerateItems
          item = item.increment();
        }
      }
    } catch (e) {
      Cu.reportError(e);
      migrationSucceeded = false;
    } finally {
      if (successfulVaultOpen) {
        // close current vault
        error = ctypesVaultHelpers._functions.VaultCloseVault(vault);
        if (error == FREE_CLOSE_FAILED) {
          Cu.reportError("Unable to close vault: " + error);
        }
      }
      ctypesKernelHelpers.finalize();
      ctypesVaultHelpers.finalize();
      aCallback(migrationSucceeded);
    }
    if (aOnlyCheckExists) {
      return false;
    }
  }
};

var MSMigrationUtils = {
  MIGRATION_TYPE_IE: 1,
  MIGRATION_TYPE_EDGE: 2,
  CtypesKernelHelpers: CtypesKernelHelpers,
  getBookmarksMigrator(migrationType = this.MIGRATION_TYPE_IE) {
    return new Bookmarks(migrationType);
  },
  getCookiesMigrator(migrationType = this.MIGRATION_TYPE_IE) {
    return new Cookies(migrationType);
  },
  getWindowsVaultFormPasswordsMigrator() {
    return new WindowsVaultFormPasswords();
  },
  getTypedURLs,
};
