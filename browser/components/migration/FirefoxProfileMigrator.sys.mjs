/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set sw=2 ts=2 sts=2 et */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/*
 * Migrates from a Firefox profile in a lossy manner in order to clean up a
 * user's profile.  Data is only migrated where the benefits outweigh the
 * potential problems caused by importing undesired/invalid configurations
 * from the source profile.
 */

import { MigrationUtils } from "resource:///modules/MigrationUtils.sys.mjs";

import { MigratorBase } from "resource:///modules/MigratorBase.sys.mjs";

const lazy = {};

ChromeUtils.defineESModuleGetters(lazy, {
  FileUtils: "resource://gre/modules/FileUtils.sys.mjs",
  PlacesBackups: "resource://gre/modules/PlacesBackups.sys.mjs",
  ProfileAge: "resource://gre/modules/ProfileAge.sys.mjs",
  SessionMigration: "resource:///modules/sessionstore/SessionMigration.sys.mjs",
});

/**
 * Firefox profile migrator. Currently, this class only does "pave over"
 * migrations, where various parts of an old profile overwrite a new
 * profile. This is distinct from other migrators which attempt to import
 * old profile data into the existing profile.
 *
 * This migrator is what powers the "Profile Refresh" mechanism.
 */
export class FirefoxProfileMigrator extends MigratorBase {
  static get key() {
    return "floorp";
  }

  static get displayNameL10nID() {
    return "migration-wizard-migrator-display-name-firefox";
  }

  static get brandImage() {
    return "chrome://branding/content/icon128.png";
  }

  _getAllProfiles() {
    let allProfiles = new Map();
    let profileService = Cc[
      "@mozilla.org/toolkit/profile-service;1"
    ].getService(Ci.nsIToolkitProfileService);
    for (let profile of profileService.profiles) {
      let rootDir = profile.rootDir;

      if (
        rootDir.exists() &&
        rootDir.isReadable() &&
        !rootDir.equals(MigrationUtils.profileStartup.directory)
      ) {
        allProfiles.set(profile.name, rootDir);
      }
    }
    return allProfiles;
  }

  getSourceProfiles() {
    let sorter = (a, b) => {
      return a.id.toLocaleLowerCase().localeCompare(b.id.toLocaleLowerCase());
    };

    return [...this._getAllProfiles().keys()]
      .map(x => ({ id: x, name: x }))
      .sort(sorter);
  }

  _getFileObject(dir, fileName) {
    let file = dir.clone();
    file.append(fileName);

    // File resources are monolithic.  We don't make partial copies since
    // they are not expected to work alone. Return null to avoid trying to
    // copy non-existing files.
    return file.exists() ? file : null;
  }

  getResources(aProfile) {
    let sourceProfileDir = aProfile
      ? this._getAllProfiles().get(aProfile.id)
      : Cc["@mozilla.org/toolkit/profile-service;1"].getService(
          Ci.nsIToolkitProfileService
        ).defaultProfile.rootDir;
    if (
      !sourceProfileDir ||
      !sourceProfileDir.exists() ||
      !sourceProfileDir.isReadable()
    ) {
      return null;
    }

    // Being a startup-only migrator, we can rely on
    // MigrationUtils.profileStartup being set.
    let currentProfileDir = MigrationUtils.profileStartup.directory;

    // Surely data cannot be imported from the current profile.
    if (sourceProfileDir.equals(currentProfileDir)) {
      return null;
    }

    return this._getResourcesInternal(sourceProfileDir, currentProfileDir);
  }

  getLastUsedDate() {
    // We always pretend we're really old, so that we don't mess
    // up the determination of which browser is the most 'recent'
    // to import from.
    return Promise.resolve(new Date(0));
  }

  _getResourcesInternal(sourceProfileDir, currentProfileDir) {
    let getFileResource = (aMigrationType, aFileNames) => {
      let files = [];
      for (let fileName of aFileNames) {

        if (fileName === "Workspaces.json") {
          // Floorp Workspaces json is a special case, it is in Workspaces folder
          const workspacesDir = sourceProfileDir.clone();
          workspacesDir.append("Workspaces");
          let file = this._getFileObject(workspacesDir, fileName);
          if (file) {
            console.log("Found Workspaces.json");
            files.push(file);
          }
          continue;
        }

        let file = this._getFileObject(sourceProfileDir, fileName);
        if (file) {
          files.push(file);
        }
      }
      if (!files.length) {
        return null;
      }
      return {
        type: aMigrationType,
        migrate(aCallback) {
          for (let file of files) {
            file.copyTo(currentProfileDir, "");
          }
          aCallback(true);
        },
      };
    };

    let _oldRawPrefsMemoized = null;
    async function readOldPrefs() {
      if (!_oldRawPrefsMemoized) {
        let prefsPath = PathUtils.join(sourceProfileDir.path, "prefs.js");
        if (await IOUtils.exists(prefsPath)) {
          _oldRawPrefsMemoized = await IOUtils.readUTF8(prefsPath, {
            encoding: "utf-8",
          });
        }
      }

      return _oldRawPrefsMemoized;
    }

    function savePrefs() {
      // If we've used the pref service to write prefs for the new profile, it's too
      // early in startup for the service to have a profile directory, so we have to
      // manually tell it where to save the prefs file.
      let newPrefsFile = currentProfileDir.clone();
      newPrefsFile.append("prefs.js");
      Services.prefs.savePrefFile(newPrefsFile);
    }

    let types = MigrationUtils.resourceTypes;
    let places = getFileResource(types.HISTORY, [
      "places.sqlite",
      "places.sqlite-wal",
      "Workspaces.json",
    ]);
    let favicons = getFileResource(types.HISTORY, [
      "favicons.sqlite",
      "favicons.sqlite-wal",
    ]);
    let cookies = getFileResource(types.COOKIES, [
      "cookies.sqlite",
      "cookies.sqlite-wal",
    ]);
    let passwords = getFileResource(types.PASSWORDS, [
      "logins.json",
      "key3.db",
      "key4.db",
    ]);
    let formData = getFileResource(types.FORMDATA, [
      "formhistory.sqlite",
      "autofill-profiles.json",
    ]);
    let bookmarksBackups = getFileResource(types.OTHERDATA, [
      lazy.PlacesBackups.profileRelativeFolderPath,
    ]);
    let dictionary = getFileResource(types.OTHERDATA, ["persdict.dat"]);

    let session;
    if (Services.env.get("MOZ_RESET_PROFILE_MIGRATE_SESSION")) {
      // We only want to restore the previous firefox session if the profile refresh was
      // triggered by user. The MOZ_RESET_PROFILE_MIGRATE_SESSION would be set when a user-triggered
      // profile refresh happened in nsAppRunner.cpp. Hence, we detect the MOZ_RESET_PROFILE_MIGRATE_SESSION
      // to see if session data migration is required.
      Services.env.set("MOZ_RESET_PROFILE_MIGRATE_SESSION", "");
      let sessionCheckpoints = this._getFileObject(
        sourceProfileDir,
        "sessionCheckpoints.json"
      );
      let sessionFile = this._getFileObject(
        sourceProfileDir,
        "sessionstore.jsonlz4"
      );
      if (sessionFile) {
        session = {
          type: types.SESSION,
          migrate(aCallback) {
            sessionCheckpoints.copyTo(
              currentProfileDir,
              "sessionCheckpoints.json"
            );
            let newSessionFile = currentProfileDir.clone();
            newSessionFile.append("sessionstore.jsonlz4");
            let migrationPromise = lazy.SessionMigration.migrate(
              sessionFile.path,
              newSessionFile.path
            );
            migrationPromise.then(
              function () {
                let buildID = Services.appinfo.platformBuildID;
                let mstone = Services.appinfo.platformVersion;
                // Force the browser to one-off resume the session that we give it:
                Services.prefs.setBoolPref(
                  "browser.sessionstore.resume_session_once",
                  true
                );
                // Reset the homepage_override prefs so that the browser doesn't override our
                // session with the "what's new" page:
                Services.prefs.setCharPref(
                  "browser.startup.homepage_override.mstone",
                  mstone
                );
                Services.prefs.setCharPref(
                  "browser.startup.homepage_override.buildID",
                  buildID
                );
                savePrefs();
                aCallback(true);
              },
              function () {
                aCallback(false);
              }
            );
          },
        };
      }
    }

    // Sync/FxA related data
    let sync = {
      name: "sync", // name is used only by tests.
      type: types.OTHERDATA,
      migrate: async aCallback => {
        // Try and parse a signedInUser.json file from the source directory and
        // if we can, copy it to the new profile and set sync's username pref
        // (which acts as a de-facto flag to indicate if sync is configured)
        try {
          let oldPath = PathUtils.join(
            sourceProfileDir.path,
            "signedInUser.json"
          );
          let exists = await IOUtils.exists(oldPath);
          if (exists) {
            let data = await IOUtils.readJSON(oldPath);
            if (data && data.accountData && data.accountData.email) {
              let username = data.accountData.email;
              // copy the file itself.
              await IOUtils.copy(
                oldPath,
                PathUtils.join(currentProfileDir.path, "signedInUser.json")
              );
              // Now we need to know whether Sync is actually configured for this
              // user. The only way we know is by looking at the prefs file from
              // the old profile. We avoid trying to do a full parse of the prefs
              // file and even avoid parsing the single string value we care
              // about.
              let oldRawPrefs = await readOldPrefs();
              if (/^user_pref\("services\.sync\.username"/m.test(oldRawPrefs)) {
                // sync's configured in the source profile - ensure it is in the
                // new profile too.
                // Write it to prefs.js and flush the file.
                Services.prefs.setStringPref(
                  "services.sync.username",
                  username
                );
                savePrefs();
              }
            }
          }
        } catch (ex) {
          aCallback(false);
          return;
        }
        aCallback(true);
      },
    };

    // Telemetry related migrations.
    let times = {
      name: "times", // name is used only by tests.
      type: types.OTHERDATA,
      migrate: aCallback => {
        let file = this._getFileObject(sourceProfileDir, "times.json");
        if (file) {
          file.copyTo(currentProfileDir, "");
        }
        // And record the fact a migration (ie, a reset) happened.
        let recordMigration = async () => {
          try {
            let profileTimes = await lazy.ProfileAge(currentProfileDir.path);
            await profileTimes.recordProfileReset();
            aCallback(true);
          } catch (e) {
            aCallback(false);
          }
        };

        recordMigration();
      },
    };
    let telemetry = {
      name: "telemetry", // name is used only by tests...
      type: types.OTHERDATA,
      migrate: async aCallback => {
        let createSubDir = name => {
          let dir = currentProfileDir.clone();
          dir.append(name);
          dir.create(Ci.nsIFile.DIRECTORY_TYPE, lazy.FileUtils.PERMS_DIRECTORY);
          return dir;
        };

        // If the 'datareporting' directory exists we migrate files from it.
        let dataReportingDir = this._getFileObject(
          sourceProfileDir,
          "datareporting"
        );
        if (dataReportingDir && dataReportingDir.isDirectory()) {
          // Copy only specific files.
          let toCopy = ["state.json", "session-state.json"];

          let dest = createSubDir("datareporting");
          let enumerator = dataReportingDir.directoryEntries;
          while (enumerator.hasMoreElements()) {
            let file = enumerator.nextFile;
            if (file.isDirectory() || !toCopy.includes(file.leafName)) {
              continue;
            }
            file.copyTo(dest, "");
          }
        }

        try {
          let oldRawPrefs = await readOldPrefs();
          let writePrefs = false;
          const PREFS = ["bookmarks", "csvpasswords", "history", "passwords"];

          for (let pref of PREFS) {
            let fullPref = `browser\.migrate\.interactions\.${pref}`;
            let regex = new RegExp('^user_pref\\("' + fullPref, "m");
            if (regex.test(oldRawPrefs)) {
              Services.prefs.setBoolPref(fullPref, true);
              writePrefs = true;
            }
          }

          if (writePrefs) {
            savePrefs();
          }
        } catch (e) {
          aCallback(false);
          return;
        }

        aCallback(true);
      },
    };

    return [
      places,
      cookies,
      passwords,
      formData,
      dictionary,
      bookmarksBackups,
      session,
      sync,
      times,
      telemetry,
      favicons,
    ].filter(r => r);
  }

  get startupOnlyMigrator() {
    return true;
  }
}
