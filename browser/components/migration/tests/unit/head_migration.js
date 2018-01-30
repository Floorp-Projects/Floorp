"use strict";

/* exported gProfD, promiseMigration, registerFakePath */

var { classes: Cc, interfaces: Ci, results: Cr, utils: Cu } = Components;

Cu.importGlobalProperties([ "URL" ]);

Cu.import("resource:///modules/MigrationUtils.jsm");
Cu.import("resource://gre/modules/LoginHelper.jsm");
Cu.import("resource://gre/modules/NetUtil.jsm");
Cu.import("resource://gre/modules/PlacesUtils.jsm");
Cu.import("resource://gre/modules/Preferences.jsm");
Cu.import("resource://gre/modules/PromiseUtils.jsm");
Cu.import("resource://gre/modules/Services.jsm");
Cu.import("resource://gre/modules/XPCOMUtils.jsm");
Cu.import("resource://testing-common/TestUtils.jsm");
Cu.import("resource://testing-common/PlacesTestUtils.jsm");

XPCOMUtils.defineLazyModuleGetter(this, "FileUtils",
                                  "resource://gre/modules/FileUtils.jsm");
XPCOMUtils.defineLazyModuleGetter(this, "Sqlite",
                                  "resource://gre/modules/Sqlite.jsm");
// Initialize profile.
var gProfD = do_get_profile();

Cu.import("resource://testing-common/AppInfo.jsm");
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
