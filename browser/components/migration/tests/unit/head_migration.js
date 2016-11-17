"use strict";

/* exported gProfD, promiseMigration, registerFakePath */

var { classes: Cc, interfaces: Ci, results: Cr, utils: Cu } = Components;

Cu.importGlobalProperties([ "URL" ]);

Cu.import("resource://gre/modules/XPCOMUtils.jsm");
Cu.import("resource://gre/modules/Services.jsm");

XPCOMUtils.defineLazyModuleGetter(this, "PlacesUtils",
                                  "resource://gre/modules/PlacesUtils.jsm");
XPCOMUtils.defineLazyModuleGetter(this, "FileUtils",
                                  "resource://gre/modules/FileUtils.jsm");
XPCOMUtils.defineLazyModuleGetter(this, "MigrationUtils",
                                  "resource:///modules/MigrationUtils.jsm");

// Initialize profile.
var gProfD = do_get_profile();

Cu.import("resource://testing-common/AppInfo.jsm"); /* globals updateAppInfo */
updateAppInfo();

/**
 * Migrates the requested resource and waits for the migration to be complete.
 */
function promiseMigration(migrator, resourceType, aProfile = null) {
  // Ensure resource migration is available.
  let availableSources = migrator.getMigrateData(aProfile, false);
  Assert.ok((availableSources & resourceType) > 0, "Resource supported by migrator");

  return new Promise(resolve => {
    Services.obs.addObserver(function onMigrationEnded() {
      Services.obs.removeObserver(onMigrationEnded, "Migration:Ended");
      resolve();
    }, "Migration:Ended", false);

    migrator.migrate(resourceType, null, aProfile);
  });
}

/**
 * Replaces a directory service entry with a given nsIFile.
 */
function registerFakePath(key, file) {
   // Register our own provider for the Library directory.
  let provider = {
    getFile(prop, persistent) {
      persistent.value = true;
      if (prop == key) {
        return file;
      }
      throw Cr.NS_ERROR_FAILURE;
    },
    QueryInterface: XPCOMUtils.generateQI([ Ci.nsIDirectoryServiceProvider ])
  };
  Services.dirsvc.QueryInterface(Ci.nsIDirectoryService)
                 .registerProvider(provider);
  do_register_cleanup(() => {
    Services.dirsvc.QueryInterface(Ci.nsIDirectoryService)
                   .unregisterProvider(provider);
  });
}
