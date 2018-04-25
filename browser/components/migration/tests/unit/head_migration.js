"use strict";

/* exported gProfD, promiseMigration, registerFakePath */

Cu.importGlobalProperties([ "URL" ]);

ChromeUtils.import("resource:///modules/MigrationUtils.jsm");
ChromeUtils.import("resource://gre/modules/LoginHelper.jsm");
ChromeUtils.import("resource://gre/modules/NetUtil.jsm");
ChromeUtils.import("resource://gre/modules/PlacesUtils.jsm");
ChromeUtils.import("resource://gre/modules/Preferences.jsm");
ChromeUtils.import("resource://gre/modules/PromiseUtils.jsm");
ChromeUtils.import("resource://gre/modules/Services.jsm");
ChromeUtils.import("resource://gre/modules/XPCOMUtils.jsm");
ChromeUtils.import("resource://testing-common/TestUtils.jsm");
ChromeUtils.import("resource://testing-common/PlacesTestUtils.jsm");

// eslint-disable-next-line no-unused-vars
ChromeUtils.defineModuleGetter(this, "FileUtils",
                               "resource://gre/modules/FileUtils.jsm");
// eslint-disable-next-line no-unused-vars
ChromeUtils.defineModuleGetter(this, "Sqlite",
                               "resource://gre/modules/Sqlite.jsm");

// Initialize profile.
var gProfD = do_get_profile();

ChromeUtils.import("resource://testing-common/AppInfo.jsm");
updateAppInfo();

/**
 * Migrates the requested resource and waits for the migration to be complete.
 */
async function promiseMigration(migrator, resourceType, aProfile = null) {
  // Ensure resource migration is available.
  let availableSources = await migrator.getMigrateData(aProfile, false);
  Assert.ok((availableSources & resourceType) > 0, "Resource supported by migrator");

  return new Promise(resolve => {
    Services.obs.addObserver(function onMigrationEnded() {
      Services.obs.removeObserver(onMigrationEnded, "Migration:Ended");
      resolve();
    }, "Migration:Ended");

    migrator.migrate(resourceType, null, aProfile);
  });
}

/**
 * Replaces a directory service entry with a given nsIFile.
 */
function registerFakePath(key, file) {
  let dirsvc = Services.dirsvc.QueryInterface(Ci.nsIProperties);
  let originalFile;
  try {
    // If a file is already provided save it and undefine, otherwise set will
    // throw for persistent entries (ones that are cached).
    originalFile = dirsvc.get(key, Ci.nsIFile);
    dirsvc.undefine(key);
  } catch (e) {
    // dirsvc.get will throw if nothing provides for the key and dirsvc.undefine
    // will throw if it's not a persistent entry, in either case we don't want
    // to set the original file in cleanup.
    originalFile = undefined;
  }

  dirsvc.set(key, file);
  registerCleanupFunction(() => {
    dirsvc.undefine(key);
    if (originalFile) {
      dirsvc.set(key, originalFile);
    }
  });
}
