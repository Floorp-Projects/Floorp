/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*-
 * vim: sw=2 ts=2 sts=2 et */
 /* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

/*
 * Migrates from a Firefox profile in a lossy manner in order to clean up a
 * user's profile.  Data is only migrated where the benefits outweigh the
 * potential problems caused by importing undesired/invalid configurations
 * from the source profile.
 */

const { classes: Cc, interfaces: Ci, utils: Cu } = Components;

Cu.import("resource://gre/modules/XPCOMUtils.jsm");
Cu.import("resource:///modules/MigrationUtils.jsm");
Cu.import("resource://gre/modules/Services.jsm");

XPCOMUtils.defineLazyModuleGetter(this, "PlacesBackups",
                                  "resource://gre/modules/PlacesBackups.jsm");
XPCOMUtils.defineLazyModuleGetter(this, "SessionMigration",
                                  "resource:///modules/sessionstore/SessionMigration.jsm");
XPCOMUtils.defineLazyModuleGetter(this, "OS",
                                  "resource://gre/modules/osfile.jsm");
XPCOMUtils.defineLazyModuleGetter(this, "FileUtils",
                                  "resource://gre/modules/FileUtils.jsm");
XPCOMUtils.defineLazyModuleGetter(this, "ProfileAge",
                                  "resource://gre/modules/ProfileAge.jsm");


function FirefoxProfileMigrator() {
  this.wrappedJSObject = this; // for testing...
}

FirefoxProfileMigrator.prototype = Object.create(MigratorPrototype);

FirefoxProfileMigrator.prototype._getAllProfiles = function () {
  let allProfiles = new Map();
  let profiles =
    Components.classes["@mozilla.org/toolkit/profile-service;1"]
              .getService(Components.interfaces.nsIToolkitProfileService)
              .profiles;
  while (profiles.hasMoreElements()) {
    let profile = profiles.getNext().QueryInterface(Ci.nsIToolkitProfile);
    let rootDir = profile.rootDir;

    if (rootDir.exists() && rootDir.isReadable() &&
        !rootDir.equals(MigrationUtils.profileStartup.directory)) {
      allProfiles.set(profile.name, rootDir);
    }
  }
  return allProfiles;
};

function sorter(a, b) {
  return a.id.toLocaleLowerCase().localeCompare(b.id.toLocaleLowerCase());
}

Object.defineProperty(FirefoxProfileMigrator.prototype, "sourceProfiles", {
  get: function() {
    return [{id: x, name: x} for (x of this._getAllProfiles().keys())].sort(sorter);
  }
});

FirefoxProfileMigrator.prototype._getFileObject = function(dir, fileName) {
  let file = dir.clone();
  file.append(fileName);

  // File resources are monolithic.  We don't make partial copies since
  // they are not expected to work alone. Return null to avoid trying to
  // copy non-existing files.
  return file.exists() ? file : null;
};

FirefoxProfileMigrator.prototype.getResources = function(aProfile) {
  let sourceProfileDir = aProfile ? this._getAllProfiles().get(aProfile.id) :
    Components.classes["@mozilla.org/toolkit/profile-service;1"]
              .getService(Components.interfaces.nsIToolkitProfileService)
              .selectedProfile.rootDir;
  if (!sourceProfileDir || !sourceProfileDir.exists() ||
      !sourceProfileDir.isReadable())
    return null;

  // Being a startup-only migrator, we can rely on
  // MigrationUtils.profileStartup being set.
  let currentProfileDir = MigrationUtils.profileStartup.directory;

  // Surely data cannot be imported from the current profile.
  if (sourceProfileDir.equals(currentProfileDir))
    return null;

  return this._getResourcesInternal(sourceProfileDir, currentProfileDir, aProfile);
};

FirefoxProfileMigrator.prototype._getResourcesInternal = function(sourceProfileDir, currentProfileDir, aProfile) {
  let getFileResource = function(aMigrationType, aFileNames) {
    let files = [];
    for (let fileName of aFileNames) {
      let file = this._getFileObject(sourceProfileDir, fileName);
      if (file)
        files.push(file);
    }
    if (!files.length) {
      return null;
    }
    return {
      type: aMigrationType,
      migrate: function(aCallback) {
        for (let file of files) {
          file.copyTo(currentProfileDir, "");
        }
        aCallback(true);
      }
    };
  }.bind(this);

  let types = MigrationUtils.resourceTypes;
  let places = getFileResource(types.HISTORY, ["places.sqlite"]);
  let cookies = getFileResource(types.COOKIES, ["cookies.sqlite"]);
  let passwords = getFileResource(types.PASSWORDS,
                                  ["signons.sqlite", "logins.json", "key3.db",
                                   "signedInUser.json"]);
  let formData = getFileResource(types.FORMDATA, ["formhistory.sqlite"]);
  let bookmarksBackups = getFileResource(types.OTHERDATA,
    [PlacesBackups.profileRelativeFolderPath]);
  let dictionary = getFileResource(types.OTHERDATA, ["persdict.dat"]);

  let sessionCheckpoints = this._getFileObject(sourceProfileDir, "sessionCheckpoints.json");
  let sessionFile = this._getFileObject(sourceProfileDir, "sessionstore.js");
  let session;
  if (sessionFile) {
    session = aProfile ? getFileResource(types.SESSION, ["sessionstore.js"]) : {
      type: types.SESSION,
      migrate: function(aCallback) {
        sessionCheckpoints.copyTo(currentProfileDir, "sessionCheckpoints.json");
        let newSessionFile = currentProfileDir.clone();
        newSessionFile.append("sessionstore.js");
        let migrationPromise = SessionMigration.migrate(sessionFile.path, newSessionFile.path);
        migrationPromise.then(function() {
          let buildID = Services.appinfo.platformBuildID;
          let mstone = Services.appinfo.platformVersion;
          // Force the browser to one-off resume the session that we give it:
          Services.prefs.setBoolPref("browser.sessionstore.resume_session_once", true);
          // Reset the homepage_override prefs so that the browser doesn't override our
          // session with the "what's new" page:
          Services.prefs.setCharPref("browser.startup.homepage_override.mstone", mstone);
          Services.prefs.setCharPref("browser.startup.homepage_override.buildID", buildID);
          // It's too early in startup for the pref service to have a profile directory,
          // so we have to manually tell it where to save the prefs file.
          let newPrefsFile = currentProfileDir.clone();
          newPrefsFile.append("prefs.js");
          Services.prefs.savePrefFile(newPrefsFile);
          aCallback(true);
        }, function() {
          aCallback(false);
        });
      }
    }
  }

  // FHR related migrations.
  let times = {
    name: "times", // name is used only by tests.
    type: types.OTHERDATA,
    migrate: aCallback => {
      let file = this._getFileObject(sourceProfileDir, "times.json");
      if (file) {
        file.copyTo(currentProfileDir, "");
      }
      // And record the fact a migration (ie, a reset) happened.
      let timesAccessor = new ProfileAge(currentProfileDir.path);
      timesAccessor.recordProfileReset().then(
        () => aCallback(true),
        () => aCallback(false)
      );
    }
  };
  let healthReporter = {
    name: "healthreporter", // name is used only by tests...
    type: types.OTHERDATA,
    migrate: aCallback => {
      // the health-reporter can't have been initialized yet so it's safe to
      // copy the SQL file.

      // We only support the default database name - copied from healthreporter.jsm
      const DEFAULT_DATABASE_NAME = "healthreport.sqlite";
      let path = OS.Path.join(sourceProfileDir.path, DEFAULT_DATABASE_NAME);
      let sqliteFile = FileUtils.File(path);
      if (sqliteFile.exists()) {
        sqliteFile.copyTo(currentProfileDir, "");
      }
      // In unusual cases there may be 2 additional files - a "write ahead log"
      // (-wal) file and a "shared memory file" (-shm).  The wal file contains
      // data that will be replayed when the DB is next opened, while the shm
      // file is ignored in that case - the replay happens using only the wal.
      // So we *do* copy a wal if it exists, but not a shm.
      // See https://www.sqlite.org/tempfiles.html for more.
      // (Note also we attempt these copies even if we can't find the DB, and
      // rely on FHR itself to do the right thing if it can)
      path = OS.Path.join(sourceProfileDir.path, DEFAULT_DATABASE_NAME + "-wal");
      let sqliteWal = FileUtils.File(path);
      if (sqliteWal.exists()) {
        sqliteWal.copyTo(currentProfileDir, "");
      }

      // If the 'healthreport' directory exists we copy everything from it.
      let subdir = this._getFileObject(sourceProfileDir, "healthreport");
      if (subdir && subdir.isDirectory()) {
        // Copy all regular files.
        let dest = currentProfileDir.clone();
        dest.append("healthreport");
        dest.create(Components.interfaces.nsIFile.DIRECTORY_TYPE,
                    FileUtils.PERMS_DIRECTORY);
        let enumerator = subdir.directoryEntries;
        while (enumerator.hasMoreElements()) {
          let file = enumerator.getNext().QueryInterface(Components.interfaces.nsIFile);
          if (file.isDirectory()) {
            continue;
          }
          file.copyTo(dest, "");
        }
      }
      // If the 'datareporting' directory exists we copy just state.json
      subdir = this._getFileObject(sourceProfileDir, "datareporting");
      if (subdir && subdir.isDirectory()) {
        let stateFile = this._getFileObject(subdir, "state.json");
        if (stateFile) {
          let dest = currentProfileDir.clone();
          dest.append("datareporting");
          dest.create(Components.interfaces.nsIFile.DIRECTORY_TYPE,
                      FileUtils.PERMS_DIRECTORY);
          stateFile.copyTo(dest, "");
        }
      }
      aCallback(true);
    }
  }

  return [r for each (r in [places, cookies, passwords, formData,
                            dictionary, bookmarksBackups, session,
                            times, healthReporter]) if (r)];
};

Object.defineProperty(FirefoxProfileMigrator.prototype, "startupOnlyMigrator", {
  get: () => true
});


FirefoxProfileMigrator.prototype.classDescription = "Firefox Profile Migrator";
FirefoxProfileMigrator.prototype.contractID = "@mozilla.org/profile/migrator;1?app=browser&type=firefox";
FirefoxProfileMigrator.prototype.classID = Components.ID("{91185366-ba97-4438-acba-48deaca63386}");

this.NSGetFactory = XPCOMUtils.generateNSGetFactory([FirefoxProfileMigrator]);
