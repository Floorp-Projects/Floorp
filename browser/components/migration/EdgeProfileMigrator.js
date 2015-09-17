/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

const { classes: Cc, interfaces: Ci, utils: Cu, results: Cr } = Components;

Cu.import("resource://gre/modules/XPCOMUtils.jsm");
Cu.import("resource://gre/modules/AppConstants.jsm");
Cu.import("resource:///modules/MigrationUtils.jsm");
Cu.import("resource:///modules/MSMigrationUtils.jsm");

function EdgeProfileMigrator() {
}

EdgeProfileMigrator.prototype = Object.create(MigratorPrototype);

EdgeProfileMigrator.prototype.getResources = function() {
  let resources = [
    MSMigrationUtils.getBookmarksMigrator(MSMigrationUtils.MIGRATION_TYPE_EDGE),
    MSMigrationUtils.getCookiesMigrator(MSMigrationUtils.MIGRATION_TYPE_EDGE),
  ];
  let windowsVaultFormPasswordsMigrator =
    MSMigrationUtils.getWindowsVaultFormPasswordsMigrator();
  windowsVaultFormPasswordsMigrator.name = "EdgeVaultFormPasswords";
  resources.push(windowsVaultFormPasswordsMigrator);
  return resources.filter(r => r.exists);
};

/* Somewhat counterintuitively, this returns:
 * - |null| to indicate "There is only 1 (default) profile" (on win10+)
 * - |[]| to indicate "There are no profiles" (on <=win8.1) which will avoid using this migrator.
 * See MigrationUtils.jsm for slightly more info on how sourceProfiles is used.
 */
EdgeProfileMigrator.prototype.__defineGetter__("sourceProfiles", function() {
  let isWin10OrHigher = AppConstants.isPlatformAndVersionAtLeast("win", "10.0");
  return isWin10OrHigher ? null : [];
});

EdgeProfileMigrator.prototype.classDescription = "Edge Profile Migrator";
EdgeProfileMigrator.prototype.contractID = "@mozilla.org/profile/migrator;1?app=browser&type=edge";
EdgeProfileMigrator.prototype.classID = Components.ID("{62e8834b-2d17-49f5-96ff-56344903a2ae}");

this.NSGetFactory = XPCOMUtils.generateNSGetFactory([EdgeProfileMigrator]);
