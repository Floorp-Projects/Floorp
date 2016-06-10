/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

this.EXPORTED_SYMBOLS = ["AutoMigrate"];

const { classes: Cc, interfaces: Ci, results: Cr, utils: Cu } = Components;

Cu.import("resource:///modules/MigrationUtils.jsm");
Cu.import("resource://gre/modules/Services.jsm");

const AutoMigrate = {
  get resourceTypesToUse() {
    let {BOOKMARKS, HISTORY, FORMDATA, PASSWORDS} = Ci.nsIBrowserProfileMigrator;
    return BOOKMARKS | HISTORY | FORMDATA | PASSWORDS;
  },

  /**
   * Automatically pick a migrator and resources to migrate,
   * then migrate those and start up.
   *
   * @throws if automatically deciding on migrators/data
   *         failed for some reason.
   */
  migrate(profileStartup, migratorKey, profileToMigrate) {
    let histogram = Services.telemetry.getKeyedHistogramById("FX_STARTUP_MIGRATION_AUTOMATED_IMPORT_SUCCEEDED");
    histogram.add("initialized");
    let migrator = this.pickMigrator(migratorKey);
    histogram.add("got-browser");

    profileToMigrate = this.pickProfile(migrator, profileToMigrate);
    histogram.add("got-profile");

    let resourceTypes = migrator.getMigrateData(profileToMigrate, profileStartup);
    if (!(resourceTypes & this.resourceTypesToUse)) {
      throw new Error("No usable resources were found for the selected browser!");
    }
    histogram.add("got-data");

    let sawErrors = false;
    let migrationObserver = function(subject, topic, data) {
      if (topic == "Migration:ItemError") {
        sawErrors = true;
      } else if (topic == "Migration:Ended") {
        histogram.add(sawErrors ? "finished-with-errors" : "finished");
        Services.obs.removeObserver(migrationObserver, "Migration:Ended");
        Services.obs.removeObserver(migrationObserver, "Migration:ItemError");
      }
    };

    Services.obs.addObserver(migrationObserver, "Migration:Ended", false);
    Services.obs.addObserver(migrationObserver, "Migration:ItemError", false);
    migrator.migrate(this.resourceTypesToUse, profileStartup, profileToMigrate);
    histogram.add("migrate-called-without-exceptions");
  },

  /**
   * Pick and return a migrator to use for automatically migrating.
   *
   * @param {String} migratorKey   optional, a migrator key to prefer/pick.
   * @returns                      the migrator to use for migrating.
   */
  pickMigrator(migratorKey) {
    if (!migratorKey) {
      let defaultKey = MigrationUtils.getMigratorKeyForDefaultBrowser();
      if (!defaultKey) {
        throw new Error("Could not determine default browser key to migrate from");
      }
      migratorKey = defaultKey;
    }
    if (migratorKey == "firefox") {
      throw new Error("Can't automatically migrate from Firefox.");
    }

    let migrator = MigrationUtils.getMigrator(migratorKey);
    if (!migrator) {
      throw new Error("Migrator specified or a default was found, but the migrator object is not available.");
    }
    return migrator;
  },

  /**
   * Pick a source profile (from the original browser) to use.
   *
   * @param {Migrator} migrator     the migrator object to use
   * @param {String}   suggestedId  the id of the profile to migrate, if pre-specified, or null
   * @returns                       the id of the profile to migrate, or null if migrating
   *                                from the default profile.
   */
  pickProfile(migrator, suggestedId) {
    let profiles = migrator.sourceProfiles;
    if (profiles && !profiles.length) {
      throw new Error("No profile data found to migrate.");
    }
    if (suggestedId) {
      if (!profiles) {
        throw new Error("Profile specified but only a default profile found.");
      }
      let suggestedProfile = profiles.find(profile => profile.id == suggestedId);
      if (!suggestedProfile) {
        throw new Error("Profile specified was not found.");
      }
      return suggestedProfile.id;
    }
    if (profiles && profiles.length > 1) {
      throw new Error("Don't know how to pick a profile when more than 1 profile is present.");
    }
    return profiles ? profiles[0].id : null;
  },
};

