/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

ChromeUtils.import("resource://gre/modules/XPCOMUtils.jsm");
ChromeUtils.import("resource://gre/modules/Services.jsm");
ChromeUtils.import("resource://gre/modules/NetUtil.jsm");
ChromeUtils.import("resource://gre/modules/FileUtils.jsm");
ChromeUtils.import("resource://gre/modules/osfile.jsm");
ChromeUtils.import("resource:///modules/MigrationUtils.jsm");

ChromeUtils.defineModuleGetter(this, "PlacesUtils",
                               "resource://gre/modules/PlacesUtils.jsm");
ChromeUtils.defineModuleGetter(this, "Sqlite",
                               "resource://gre/modules/Sqlite.jsm");

XPCOMUtils.defineLazyGlobalGetters(this, ["URL"]);

const kBookmarksFileName = "360sefav.db";

function copyToTempUTF8File(file, charset) {
  let inputStream = Cc["@mozilla.org/network/file-input-stream;1"]
                      .createInstance(Ci.nsIFileInputStream);
  inputStream.init(file, -1, -1, 0);
  let inputStr = NetUtil.readInputStreamToString(
    inputStream, inputStream.available(), { charset });

  // Use random to reduce the likelihood of a name collision in createUnique.
  let rand = Math.floor(Math.random() * Math.pow(2, 15));
  let leafName = "mozilla-temp-" + rand;
  let tempUTF8File = FileUtils.getFile(
    "TmpD", ["mozilla-temp-files", leafName], true);
  tempUTF8File.createUnique(Ci.nsIFile.NORMAL_FILE_TYPE, 0o600);

  let out = FileUtils.openAtomicFileOutputStream(tempUTF8File);
  try {
    let bufferedOut = Cc["@mozilla.org/network/buffered-output-stream;1"]
                        .createInstance(Ci.nsIBufferedOutputStream);
    bufferedOut.init(out, 4096);
    try {
      let converterOut = Cc["@mozilla.org/intl/converter-output-stream;1"]
                           .createInstance(Ci.nsIConverterOutputStream);
      converterOut.init(bufferedOut, "utf-8");
      try {
        converterOut.writeString(inputStr || "");
        bufferedOut.QueryInterface(Ci.nsISafeOutputStream).finish();
      } finally {
        converterOut.close();
      }
    } finally {
      bufferedOut.close();
    }
  } finally {
    out.close();
  }

  return tempUTF8File;
}

function parseINIStrings(file) {
  let factory = Cc["@mozilla.org/xpcom/ini-parser-factory;1"].
                getService(Ci.nsIINIParserFactory);
  let parser = factory.createINIParser(file);
  let obj = {};
  let sections = parser.getSections();
  while (sections.hasMore()) {
    let section = sections.getNext();
    obj[section] = {};

    let keys = parser.getKeys(section);
    while (keys.hasMore()) {
      let key = keys.getNext();
      obj[section][key] = parser.getString(section, key);
    }
  }
  return obj;
}

function getHash(aStr) {
  // return the two-digit hexadecimal code for a byte
  let toHexString = charCode => ("0" + charCode.toString(16)).slice(-2);

  let hasher = Cc["@mozilla.org/security/hash;1"].
               createInstance(Ci.nsICryptoHash);
  hasher.init(Ci.nsICryptoHash.MD5);
  let stringStream = Cc["@mozilla.org/io/string-input-stream;1"].
                     createInstance(Ci.nsIStringInputStream);
  stringStream.data = aStr;
  hasher.updateFromStream(stringStream, -1);

  // convert the binary hash data to a hex string.
  let binary = hasher.finish(false);
  return Array.from(binary, (c, i) => toHexString(binary.charCodeAt(i))).join("").toLowerCase();
}

function Bookmarks(aProfileFolder) {
  let file = aProfileFolder.clone();
  file.append(kBookmarksFileName);

  this._file = file;
}
Bookmarks.prototype = {
  type: MigrationUtils.resourceTypes.BOOKMARKS,

  get exists() {
    return this._file.exists() && this._file.isReadable();
  },

  migrate(aCallback) {
    return (async () => {
      let folderMap = new Map();
      let toolbarBMs = [];

      let connection = await Sqlite.openConnection({
        path: this._file.path,
      });

      try {
        let rows = await connection.execute(
          `WITH RECURSIVE
           bookmark(id, parent_id, is_folder, title, url, pos) AS (
             VALUES(0, -1, 1, '', '', 0)
             UNION
             SELECT f.id, f.parent_id, f.is_folder, f.title, f.url, f.pos
             FROM tb_fav AS f
             JOIN bookmark AS b ON f.parent_id = b.id
             ORDER BY f.pos ASC
           )
           SELECT id, parent_id, is_folder, title, url FROM bookmark WHERE id`);

        for (let row of rows) {
          let id = parseInt(row.getResultByName("id"), 10);
          let parent_id = parseInt(row.getResultByName("parent_id"), 10);
          let is_folder = parseInt(row.getResultByName("is_folder"), 10);
          let title = row.getResultByName("title");
          let url = row.getResultByName("url");

          let bmToInsert;

          if (is_folder) {
            bmToInsert = {
              children: [],
              title,
              type: PlacesUtils.bookmarks.TYPE_FOLDER,
            };
            folderMap.set(id, bmToInsert);
          } else {
            try {
              new URL(url);
            } catch (ex) {
              Cu.reportError(`Ignoring ${url} when importing from 360se because of exception: ${ex}`);
              continue;
            }

            bmToInsert = {
              title,
              url,
            };
          }

          if (folderMap.has(parent_id)) {
            folderMap.get(parent_id).children.push(bmToInsert);
          } else if (parent_id === 0) {
            toolbarBMs.push(bmToInsert);
          }
        }
      } finally {
        await connection.close();
      }

      if (toolbarBMs.length) {
        let parentGuid = PlacesUtils.bookmarks.toolbarGuid;
        if (!MigrationUtils.isStartupMigration) {
          parentGuid =
            await MigrationUtils.createImportedBookmarksFolder("360se", parentGuid);
        }
        await MigrationUtils.insertManyBookmarksWrapper(toolbarBMs, parentGuid);
      }
    })().then(() => aCallback(true),
                        e => { Cu.reportError(e); aCallback(false); });
  },
};

function Qihoo360seProfileMigrator() {
  let paths = [
    // for v6 and above
    {
      users: ["360se6", "apps", "data", "users"],
      defaultUser: "default",
    },
    // for earlier versions
    {
      users: ["360se"],
      defaultUser: "data",
    },
  ];
  this._usersDir = null;
  this._defaultUserPath = null;
  for (let path of paths) {
    let usersDir = FileUtils.getDir("AppData", path.users, false);
    if (usersDir.exists()) {
      this._usersDir = usersDir;
      this._defaultUserPath = path.defaultUser;
      break;
    }
  }
}

Qihoo360seProfileMigrator.prototype = Object.create(MigratorPrototype);

Qihoo360seProfileMigrator.prototype.getSourceProfiles = function() {
  if ("__sourceProfiles" in this)
    return this.__sourceProfiles;

  if (!this._usersDir) {
    this.__sourceProfiles = [];
    return this.__sourceProfiles;
  }

  let profiles = [];
  let noLoggedInUser = true;
  try {
    let loginIni = this._usersDir.clone();
    loginIni.append("login.ini");
    if (!loginIni.exists()) {
      throw new Error("360 Secure Browser's 'login.ini' does not exist.");
    }
    if (!loginIni.isReadable()) {
      throw new Error("360 Secure Browser's 'login.ini' file could not be read.");
    }

    let loginIniInUtf8 = copyToTempUTF8File(loginIni, "GBK");
    let loginIniObj = parseINIStrings(loginIniInUtf8);
    try {
      loginIniInUtf8.remove(false);
    } catch (ex) {}

    let nowLoginEmail = loginIniObj.NowLogin && loginIniObj.NowLogin.email;

    /*
     * NowLogin section may:
     * 1. be missing or without email, before any user logs in.
     * 2. represents the current logged in user
     * 3. represents the most recent logged in user
     *
     * In the second case, user represented by NowLogin should be the first
     * profile; otherwise the default user should be selected by default.
     */
    if (nowLoginEmail) {
      if (loginIniObj.NowLogin.IsLogined === "1") {
        noLoggedInUser = false;
      }

      profiles.push({
        id: this._getIdFromConfig(loginIniObj.NowLogin),
        name: nowLoginEmail,
      });
    }

    for (let section in loginIniObj) {
      if (!loginIniObj[section].email ||
          (nowLoginEmail && loginIniObj[section].email == nowLoginEmail)) {
        continue;
      }

      profiles.push({
        id: this._getIdFromConfig(loginIniObj[section]),
        name: loginIniObj[section].email,
      });
    }
  } catch (e) {
    Cu.reportError("Error detecting 360 Secure Browser profiles: " + e);
  } finally {
    profiles[noLoggedInUser ? "unshift" : "push"]({
      id: this._defaultUserPath,
      name: "Default",
    });
  }

  this.__sourceProfiles = profiles.filter(profile => {
    let resources = this.getResources(profile);
    return resources && resources.length > 0;
  });
  return this.__sourceProfiles;
};

Qihoo360seProfileMigrator.prototype._getIdFromConfig = function(aConfig) {
  return aConfig.UserMd5 || getHash(aConfig.email);
};

Qihoo360seProfileMigrator.prototype.getResources = function(aProfile) {
  let profileFolder = this._usersDir.clone();
  profileFolder.append(aProfile.id);

  if (!profileFolder.exists()) {
    return [];
  }

  let resources = [
    new Bookmarks(profileFolder),
  ];
  return resources.filter(r => r.exists);
};

Qihoo360seProfileMigrator.prototype.getLastUsedDate = async function() {
  let sourceProfiles = await this.getSourceProfiles();
  let bookmarksPaths = sourceProfiles.map(({id}) => {
    return OS.Path.join(this._usersDir.path, id, kBookmarksFileName);
  });
  if (!bookmarksPaths.length) {
    return new Date(0);
  }
  let datePromises = bookmarksPaths.map(path => {
    return OS.File.stat(path).catch(() => null).then(info => {
      return info ? info.lastModificationDate : 0;
    });
  });
  return Promise.all(datePromises).then(dates => {
    return new Date(Math.max.apply(Math, dates));
  });
};

Qihoo360seProfileMigrator.prototype.classDescription = "360 Secure Browser Profile Migrator";
Qihoo360seProfileMigrator.prototype.contractID = "@mozilla.org/profile/migrator;1?app=browser&type=360se";
Qihoo360seProfileMigrator.prototype.classID = Components.ID("{d0037b95-296a-4a4e-94b2-c3d075d20ab1}");

this.NSGetFactory = XPCOMUtils.generateNSGetFactory([Qihoo360seProfileMigrator]);
