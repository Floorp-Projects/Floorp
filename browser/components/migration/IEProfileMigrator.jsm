/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const kLoginsKey =
  "Software\\Microsoft\\Internet Explorer\\IntelliForms\\Storage2";

const { AppConstants } = ChromeUtils.import(
  "resource://gre/modules/AppConstants.jsm"
);
const { OS } = ChromeUtils.import("resource://gre/modules/osfile.jsm");
const { Services } = ChromeUtils.import("resource://gre/modules/Services.jsm");
const { XPCOMUtils } = ChromeUtils.import(
  "resource://gre/modules/XPCOMUtils.jsm"
);
const { MigrationUtils, MigratorPrototype } = ChromeUtils.import(
  "resource:///modules/MigrationUtils.jsm"
);
const { MSMigrationUtils } = ChromeUtils.import(
  "resource:///modules/MSMigrationUtils.jsm"
);

ChromeUtils.defineModuleGetter(
  this,
  "ctypes",
  "resource://gre/modules/ctypes.jsm"
);
ChromeUtils.defineModuleGetter(
  this,
  "PlacesUtils",
  "resource://gre/modules/PlacesUtils.jsm"
);
ChromeUtils.defineModuleGetter(
  this,
  "OSCrypto",
  "resource://gre/modules/OSCrypto.jsm"
);

XPCOMUtils.defineLazyGlobalGetters(this, ["URL"]);

// Resources

function History() {}

History.prototype = {
  type: MigrationUtils.resourceTypes.HISTORY,

  get exists() {
    return true;
  },

  migrate: function H_migrate(aCallback) {
    let pageInfos = [];
    let typedURLs = MSMigrationUtils.getTypedURLs(
      "Software\\Microsoft\\Internet Explorer"
    );
    for (let entry of Cc[
      "@mozilla.org/profile/migrator/iehistoryenumerator;1"
    ].createInstance(Ci.nsISimpleEnumerator)) {
      let url = entry.get("uri").QueryInterface(Ci.nsIURI);
      // MSIE stores some types of URLs in its history that we don't handle,
      // like HTMLHelp and others.  Since we don't properly map handling for
      // all of them we just avoid importing them.
      if (!["http", "https", "ftp", "file"].includes(url.scheme)) {
        continue;
      }

      let title = entry.get("title");
      // Embed visits have no title and don't need to be imported.
      if (!title.length) {
        continue;
      }

      // The typed urls are already fixed-up, so we can use them for comparison.
      let transition = typedURLs.has(url.spec)
        ? PlacesUtils.history.TRANSITIONS.LINK
        : PlacesUtils.history.TRANSITIONS.TYPED;
      // use the current date if we have no visits for this entry.
      let time = entry.get("time");

      pageInfos.push({
        url,
        title,
        visits: [
          {
            transition,
            date: time ? PlacesUtils.toDate(entry.get("time")) : new Date(),
          },
        ],
      });
    }

    // Check whether there is any history to import.
    if (!pageInfos.length) {
      aCallback(true);
      return;
    }

    MigrationUtils.insertVisitsWrapper(pageInfos).then(
      () => aCallback(true),
      () => aCallback(false)
    );
  },
};

// IE form password migrator supporting windows from XP until 7 and IE from 7 until 11
function IE7FormPasswords() {
  // used to distinguish between this migrator and other passwords migrators in tests.
  this.name = "IE7FormPasswords";
}

IE7FormPasswords.prototype = {
  type: MigrationUtils.resourceTypes.PASSWORDS,

  get exists() {
    // work only on windows until 7
    if (AppConstants.isPlatformAndVersionAtLeast("win", "6.2")) {
      return false;
    }

    try {
      let nsIWindowsRegKey = Ci.nsIWindowsRegKey;
      let key = Cc["@mozilla.org/windows-registry-key;1"].createInstance(
        nsIWindowsRegKey
      );
      key.open(
        nsIWindowsRegKey.ROOT_KEY_CURRENT_USER,
        kLoginsKey,
        nsIWindowsRegKey.ACCESS_READ
      );
      let count = key.valueCount;
      key.close();
      return count > 0;
    } catch (e) {
      return false;
    }
  },

  async migrate(aCallback) {
    let uris = []; // the uris of the websites that are going to be migrated
    for (let entry of Cc[
      "@mozilla.org/profile/migrator/iehistoryenumerator;1"
    ].createInstance(Ci.nsISimpleEnumerator)) {
      let uri = entry.get("uri").QueryInterface(Ci.nsIURI);
      // MSIE stores some types of URLs in its history that we don't handle, like HTMLHelp
      // and others. Since we are not going to import the logins that are performed in these URLs
      // we can just skip them.
      if (!["http", "https", "ftp"].includes(uri.scheme)) {
        continue;
      }

      uris.push(uri);
    }
    await this._migrateURIs(uris);
    aCallback(true);
  },

  /**
   * Migrate the logins that were saved for the uris arguments.
   * @param {nsIURI[]} uris - the uris that are going to be migrated.
   */
  async _migrateURIs(uris) {
    this.ctypesKernelHelpers = new MSMigrationUtils.CtypesKernelHelpers();
    this._crypto = new OSCrypto();
    let nsIWindowsRegKey = Ci.nsIWindowsRegKey;
    let key = Cc["@mozilla.org/windows-registry-key;1"].createInstance(
      nsIWindowsRegKey
    );
    key.open(
      nsIWindowsRegKey.ROOT_KEY_CURRENT_USER,
      kLoginsKey,
      nsIWindowsRegKey.ACCESS_READ
    );

    let urlsSet = new Set(); // set of the already processed urls.
    // number of the successfully decrypted registry values
    let successfullyDecryptedValues = 0;
    /* The logins are stored in the registry, where the key is a hashed URL and its
     * value contains the encrypted details for all logins for that URL.
     *
     * First iterate through IE history, hashing each URL and looking for a match. If
     * found, decrypt the value, using the URL as a salt. Finally add any found logins
     * to the Firefox password manager.
     */

    let logins = [];
    for (let uri of uris) {
      try {
        // remove the query and the ref parts of the URL
        let urlObject = new URL(uri.spec);
        let url = urlObject.origin + urlObject.pathname;
        // if the current url is already processed, it should be skipped
        if (urlsSet.has(url)) {
          continue;
        }
        urlsSet.add(url);
        // hash value of the current uri
        let hashStr = this._crypto.getIELoginHash(url);
        if (!key.hasValue(hashStr)) {
          continue;
        }
        let value = key.readBinaryValue(hashStr);
        // if no value was found, the uri is skipped
        if (value == null) {
          continue;
        }
        let data;
        try {
          // the url is used as salt to decrypt the registry value
          data = this._crypto.decryptData(value, url);
        } catch (e) {
          continue;
        }
        // extract the login details from the decrypted data
        let ieLogins = this._extractDetails(data, uri);
        // if at least a credential was found in the current data, successfullyDecryptedValues should
        // be incremented by one
        if (ieLogins.length) {
          successfullyDecryptedValues++;
        }
        for (let ieLogin of ieLogins) {
          logins.push({
            username: ieLogin.username,
            password: ieLogin.password,
            origin: ieLogin.url,
            timeCreated: ieLogin.creation,
          });
        }
      } catch (e) {
        Cu.reportError(
          "Error while importing logins for " + uri.spec + ": " + e
        );
      }
    }

    if (logins.length) {
      await MigrationUtils.insertLoginsWrapper(logins);
    }

    // if the number of the imported values is less than the number of values in the key, it means
    // that not all the values were imported and an error should be reported
    if (successfullyDecryptedValues < key.valueCount) {
      Cu.reportError(
        "We failed to decrypt and import some logins. " +
          "This is likely because we didn't find the URLs where these " +
          "passwords were submitted in the IE history and which are needed to be used " +
          "as keys in the decryption."
      );
    }

    key.close();
    this._crypto.finalize();
    this.ctypesKernelHelpers.finalize();
  },

  _crypto: null,

  /**
   * Extract the details of one or more logins from the raw decrypted data.
   * @param {string} data - the decrypted data containing raw information.
   * @param {nsURI} uri - the nsURI of page where the login has occur.
   * @returns {Object[]} array of objects where each of them contains the username, password, URL,
   * and creation time representing all the logins found in the data arguments.
   */
  _extractDetails(data, uri) {
    // the structure of the header of the IE7 decrypted data for all the logins sharing the same URL
    let loginData = new ctypes.StructType("loginData", [
      // Bytes 0-3 are not needed and not documented
      { unknown1: ctypes.uint32_t },
      // Bytes 4-7 are the header size
      { headerSize: ctypes.uint32_t },
      // Bytes 8-11 are the data size
      { dataSize: ctypes.uint32_t },
      // Bytes 12-19 are not needed and not documented
      { unknown2: ctypes.uint32_t },
      { unknown3: ctypes.uint32_t },
      // Bytes 20-23 are the data count: each username and password is considered as a data
      { dataMax: ctypes.uint32_t },
      // Bytes 24-35 are not needed and not documented
      { unknown4: ctypes.uint32_t },
      { unknown5: ctypes.uint32_t },
      { unknown6: ctypes.uint32_t },
    ]);

    // the structure of a IE7 decrypted login item
    let loginItem = new ctypes.StructType("loginItem", [
      // Bytes 0-3 are the offset of the username
      { usernameOffset: ctypes.uint32_t },
      // Bytes 4-11 are the date
      { loDateTime: ctypes.uint32_t },
      { hiDateTime: ctypes.uint32_t },
      // Bytes 12-15 are not needed and not documented
      { foo: ctypes.uint32_t },
      // Bytes 16-19 are the offset of the password
      { passwordOffset: ctypes.uint32_t },
      // Bytes 20-31 are not needed and not documented
      { unknown1: ctypes.uint32_t },
      { unknown2: ctypes.uint32_t },
      { unknown3: ctypes.uint32_t },
    ]);

    let url = uri.prePath;
    let results = [];
    let arr = this._crypto.stringToArray(data);
    // convert data to ctypes.unsigned_char.array(arr.length)
    let cdata = ctypes.unsigned_char.array(arr.length)(arr);
    // Bytes 0-35 contain the loginData data structure for all the logins sharing the same URL
    let currentLoginData = ctypes.cast(cdata, loginData);
    let headerSize = currentLoginData.headerSize;
    let currentInfoIndex = loginData.size;
    // pointer to the current login item
    let currentLoginItemPointer = ctypes.cast(
      cdata.addressOfElement(currentInfoIndex),
      loginItem.ptr
    );
    // currentLoginData.dataMax is the data count: each username and password is considered as
    // a data. So, the number of logins is the number of data dived by 2
    let numLogins = currentLoginData.dataMax / 2;
    for (let n = 0; n < numLogins; n++) {
      // Bytes 0-31 starting from currentInfoIndex contain the loginItem data structure for the
      // current login
      let currentLoginItem = currentLoginItemPointer.contents;
      let creation =
        this.ctypesKernelHelpers.fileTimeToSecondsSinceEpoch(
          currentLoginItem.hiDateTime,
          currentLoginItem.loDateTime
        ) * 1000;
      let currentResult = {
        creation,
        url,
      };
      // The username is UTF-16 and null-terminated.
      currentResult.username = ctypes
        .cast(
          cdata.addressOfElement(
            headerSize + 12 + currentLoginItem.usernameOffset
          ),
          ctypes.char16_t.ptr
        )
        .readString();
      // The password is UTF-16 and null-terminated.
      currentResult.password = ctypes
        .cast(
          cdata.addressOfElement(
            headerSize + 12 + currentLoginItem.passwordOffset
          ),
          ctypes.char16_t.ptr
        )
        .readString();
      results.push(currentResult);
      // move to the next login item
      currentLoginItemPointer = currentLoginItemPointer.increment();
    }
    return results;
  },
};

function IEProfileMigrator() {
  this.wrappedJSObject = this; // export this to be able to use it in the unittest.
}

IEProfileMigrator.prototype = Object.create(MigratorPrototype);

IEProfileMigrator.prototype.getResources = function IE_getResources() {
  let resources = [
    MSMigrationUtils.getBookmarksMigrator(),
    new History(),
    MSMigrationUtils.getCookiesMigrator(),
  ];
  // Only support the form password migrator for Windows XP to 7.
  if (AppConstants.isPlatformAndVersionAtMost("win", "6.1")) {
    resources.push(new IE7FormPasswords());
  }
  let windowsVaultFormPasswordsMigrator = MSMigrationUtils.getWindowsVaultFormPasswordsMigrator();
  windowsVaultFormPasswordsMigrator.name = "IEVaultFormPasswords";
  resources.push(windowsVaultFormPasswordsMigrator);
  return resources.filter(r => r.exists);
};

IEProfileMigrator.prototype.getLastUsedDate = function IE_getLastUsedDate() {
  let datePromises = ["Favs", "CookD"].map(dirId => {
    let { path } = Services.dirsvc.get(dirId, Ci.nsIFile);
    return OS.File.stat(path)
      .catch(() => null)
      .then(info => {
        return info ? info.lastModificationDate : 0;
      });
  });
  datePromises.push(
    new Promise(resolve => {
      let typedURLs = new Map();
      try {
        typedURLs = MSMigrationUtils.getTypedURLs(
          "Software\\Microsoft\\Internet Explorer"
        );
      } catch (ex) {}
      let dates = [0, ...typedURLs.values()];
      // dates is an array of PRTimes, which are in microseconds - convert to milliseconds
      resolve(Math.max.apply(Math, dates) / 1000);
    })
  );
  return Promise.all(datePromises).then(dates => {
    return new Date(Math.max.apply(Math, dates));
  });
};

IEProfileMigrator.prototype.classDescription = "IE Profile Migrator";
IEProfileMigrator.prototype.contractID =
  "@mozilla.org/profile/migrator;1?app=browser&type=ie";
IEProfileMigrator.prototype.classID = Components.ID(
  "{3d2532e3-4932-4774-b7ba-968f5899d3a4}"
);

var EXPORTED_SYMBOLS = ["IEProfileMigrator"];
