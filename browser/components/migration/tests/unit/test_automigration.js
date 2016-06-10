Cu.import("resource:///modules/MigrationUtils.jsm");
let AutoMigrateBackstage = Cu.import("resource:///modules/AutoMigrate.jsm");

let gShimmedMigratorKeyPicker = null;
let gShimmedMigrator = null;

// This is really a proxy on MigrationUtils, but if we specify that directly,
// we get in trouble because the object itself is frozen, and Proxies can't
// return a different value to an object when directly proxying a frozen
// object.
AutoMigrateBackstage.MigrationUtils = new Proxy({}, {
  get(target, name) {
    if (name == "getMigratorKeyForDefaultBrowser" && gShimmedMigratorKeyPicker) {
      return gShimmedMigratorKeyPicker;
    }
    if (name == "getMigrator" && gShimmedMigrator) {
      return function() { return gShimmedMigrator };
    }
    return MigrationUtils[name];
  },
});

do_register_cleanup(function() {
  AutoMigrateBackstage.MigrationUtils = MigrationUtils;
});

/**
 * Test automatically picking a browser to migrate from
 */
add_task(function* checkMigratorPicking() {
  Assert.throws(() => AutoMigrate.pickMigrator("firefox"),
                /Can't automatically migrate from Firefox/,
                "Should throw when explicitly picking Firefox.");

  Assert.throws(() => AutoMigrate.pickMigrator("gobbledygook"),
                /migrator object is not available/,
                "Should throw when passing unknown migrator key");
  gShimmedMigratorKeyPicker = function() {
    return "firefox";
  };
  Assert.throws(() => AutoMigrate.pickMigrator(),
                /Can't automatically migrate from Firefox/,
                "Should throw when implicitly picking Firefox.");
  gShimmedMigratorKeyPicker = function() {
    return "gobbledygook";
  };
  Assert.throws(() => AutoMigrate.pickMigrator(),
                /migrator object is not available/,
                "Should throw when an unknown migrator is the default");
  gShimmedMigratorKeyPicker = function() {
    return "";
  };
  Assert.throws(() => AutoMigrate.pickMigrator(),
                /Could not determine default browser key/,
                "Should throw when an unknown migrator is the default");
});


/**
 * Test automatically picking a profile to migrate from
 */
add_task(function* checkProfilePicking() {
  let fakeMigrator = {sourceProfiles: [{id: "a"}, {id: "b"}]};
  Assert.throws(() => AutoMigrate.pickProfile(fakeMigrator),
                /Don't know how to pick a profile when more/,
                "Should throw when there are multiple profiles.");
  Assert.throws(() => AutoMigrate.pickProfile(fakeMigrator, "c"),
                /Profile specified was not found/,
                "Should throw when the profile supplied doesn't exist.");
  let profileToMigrate = AutoMigrate.pickProfile(fakeMigrator, "b");
  Assert.equal(profileToMigrate, "b", "Should return profile supplied");

  fakeMigrator.sourceProfiles = null;
  Assert.throws(() => AutoMigrate.pickProfile(fakeMigrator, "c"),
                /Profile specified but only a default profile found./,
                "Should throw when the profile supplied doesn't exist.");
  profileToMigrate = AutoMigrate.pickProfile(fakeMigrator);
  Assert.equal(profileToMigrate, null, "Should return default profile when that's the only one.");

  fakeMigrator.sourceProfiles = [];
  Assert.throws(() => AutoMigrate.pickProfile(fakeMigrator),
                /No profile data found/,
                "Should throw when no profile data is present.");

  fakeMigrator.sourceProfiles = [{id: "a"}];
  profileToMigrate = AutoMigrate.pickProfile(fakeMigrator);
  Assert.equal(profileToMigrate, "a", "Should return the only profile if only one is present.");
});

/**
 * Test the complete automatic process including browser and profile selection,
 * and actual migration (which implies startup)
 */
add_task(function* checkIntegration() {
  gShimmedMigrator = {
    get sourceProfiles() {
      do_print("Read sourceProfiles");
      return null;
    },
    getMigrateData(profileToMigrate) {
      this._getMigrateDataArgs = profileToMigrate;
      return Ci.nsIBrowserProfileMigrator.BOOKMARKS;
    },
    migrate(types, startup, profileToMigrate) {
      this._migrateArgs = [types, startup, profileToMigrate];
    },
  };
  gShimmedMigratorKeyPicker = function() {
    return "gobbledygook";
  };
  AutoMigrate.migrate("startup");
  Assert.strictEqual(gShimmedMigrator._getMigrateDataArgs, null,
                     "getMigrateData called with 'null' as a profile");

  let {BOOKMARKS, HISTORY, FORMDATA, PASSWORDS} = Ci.nsIBrowserProfileMigrator;
  let expectedTypes = BOOKMARKS | HISTORY | FORMDATA | PASSWORDS;
  Assert.deepEqual(gShimmedMigrator._migrateArgs, [expectedTypes, "startup", null],
                   "getMigrateData called with 'null' as a profile");
});
