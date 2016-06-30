/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

this.EXPORTED_SYMBOLS = ["AutoMigrate"];

const { classes: Cc, interfaces: Ci, results: Cr, utils: Cu } = Components;

const kAutoMigrateStartedPref = "browser.migrate.automigrate-started";
const kAutoMigrateFinishedPref = "browser.migrate.automigrate-finished";

Cu.import("resource:///modules/MigrationUtils.jsm");
Cu.import("resource://gre/modules/PlacesUtils.jsm");
Cu.import("resource://gre/modules/Services.jsm");
Cu.import("resource://gre/modules/Task.jsm");
Cu.import("resource://gre/modules/XPCOMUtils.jsm");

const AutoMigrate = {
  get resourceTypesToUse() {
    let {BOOKMARKS, HISTORY, PASSWORDS} = Ci.nsIBrowserProfileMigrator;
    return BOOKMARKS | HISTORY | PASSWORDS;
  },

  /**
   * Automatically pick a migrator and resources to migrate,
   * then migrate those and start up.
   *
   * @throws if automatically deciding on migrators/data
   *         failed for some reason.
   */
  migrate(profileStartup, migratorKey, profileToMigrate) {
    let histogram = Services.telemetry.getHistogramById("FX_STARTUP_MIGRATION_AUTOMATED_IMPORT_PROCESS_SUCCESS");
    histogram.add(0);
    let migrator = this.pickMigrator(migratorKey);
    histogram.add(5);

    profileToMigrate = this.pickProfile(migrator, profileToMigrate);
    histogram.add(10);

    let resourceTypes = migrator.getMigrateData(profileToMigrate, profileStartup);
    if (!(resourceTypes & this.resourceTypesToUse)) {
      throw new Error("No usable resources were found for the selected browser!");
    }
    histogram.add(15);

    let sawErrors = false;
    let migrationObserver = function(subject, topic, data) {
      if (topic == "Migration:ItemError") {
        sawErrors = true;
      } else if (topic == "Migration:Ended") {
        histogram.add(25);
        if (sawErrors) {
          histogram.add(26);
        }
        Services.obs.removeObserver(migrationObserver, "Migration:Ended");
        Services.obs.removeObserver(migrationObserver, "Migration:ItemError");
        Services.prefs.setCharPref(kAutoMigrateStartedPref, startTime.toString());
        Services.prefs.setCharPref(kAutoMigrateFinishedPref, Date.now().toString());
      }
    };

    Services.obs.addObserver(migrationObserver, "Migration:Ended", false);
    Services.obs.addObserver(migrationObserver, "Migration:ItemError", false);
    // We'll save this when the migration has finished, at which point the pref
    // service will be available.
    let startTime = Date.now();
    migrator.migrate(this.resourceTypesToUse, profileStartup, profileToMigrate);
    histogram.add(20);
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
   * @returns                       the profile to migrate, or null if migrating
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
      return suggestedProfile;
    }
    if (profiles && profiles.length > 1) {
      throw new Error("Don't know how to pick a profile when more than 1 profile is present.");
    }
    return profiles ? profiles[0] : null;
  },

  getUndoRange() {
    let start, finish;
    try {
      start = parseInt(Services.prefs.getCharPref(kAutoMigrateStartedPref), 10);
      finish = parseInt(Services.prefs.getCharPref(kAutoMigrateFinishedPref), 10);
    } catch (ex) {
      Cu.reportError(ex);
    }
    if (!finish || !start) {
      return null;
    }
    return [new Date(start), new Date(finish)];
  },

  canUndo() {
    if (!this.getUndoRange()) {
      return Promise.resolve(false);
    }
    // Return a promise resolving to false if we're signed into sync, resolve
    // to true otherwise.
    let {fxAccounts} = Cu.import("resource://gre/modules/FxAccounts.jsm", {});
    return fxAccounts.getSignedInUser().then(user => !user, () => Promise.resolve(true));
  },

  undo: Task.async(function* () {
    if (!(yield this.canUndo())) {
      throw new Error("Can't undo!");
    }

    yield PlacesUtils.bookmarks.eraseEverything();

    // NB: we drop the start time of the migration for now. This is because
    // imported history will always end up being 'backdated' to the actual
    // visit time recorded by the browser from which we imported. As a result,
    // a lower bound on this item doesn't really make sense.
    // Note that for form data this could be different, but we currently don't
    // support form data import from any non-Firefox browser, so it isn't
    // imported from other browsers by the automigration code, nor do we
    // remove it here.
    let range = this.getUndoRange();
    yield PlacesUtils.history.removeVisitsByFilter({
      beginDate: new Date(0),
      endDate: range[1]
    });

    try {
      Services.logins.removeAllLogins();
    } catch (ex) {
      // ignore failure.
    }
    Services.prefs.clearUserPref("browser.migrate.automigrate-started");
    Services.prefs.clearUserPref("browser.migrate.automigrate-finished");
  }),
};

