Cu.import("resource:///modules/MigrationUtils.jsm");
Cu.import("resource://gre/modules/PlacesUtils.jsm");
Cu.import("resource://gre/modules/Preferences.jsm");
Cu.import("resource://testing-common/TestUtils.jsm");
Cu.import("resource://testing-common/PlacesTestUtils.jsm");
let AutoMigrateBackstage = Cu.import("resource:///modules/AutoMigrate.jsm");

let gShimmedMigratorKeyPicker = null;
let gShimmedMigrator = null;

const kUsecPerMin = 60 * 1000000;

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
  let profB = fakeMigrator.sourceProfiles[1];
  Assert.throws(() => AutoMigrate.pickProfile(fakeMigrator),
                /Don't know how to pick a profile when more/,
                "Should throw when there are multiple profiles.");
  Assert.throws(() => AutoMigrate.pickProfile(fakeMigrator, "c"),
                /Profile specified was not found/,
                "Should throw when the profile supplied doesn't exist.");
  let profileToMigrate = AutoMigrate.pickProfile(fakeMigrator, "b");
  Assert.equal(profileToMigrate, profB, "Should return profile supplied");

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
  let profA = fakeMigrator.sourceProfiles[0];
  profileToMigrate = AutoMigrate.pickProfile(fakeMigrator);
  Assert.equal(profileToMigrate, profA, "Should return the only profile if only one is present.");
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

  let {BOOKMARKS, HISTORY, PASSWORDS} = Ci.nsIBrowserProfileMigrator;
  let expectedTypes = BOOKMARKS | HISTORY | PASSWORDS;
  Assert.deepEqual(gShimmedMigrator._migrateArgs, [expectedTypes, "startup", null],
                   "migrate called with 'null' as a profile");
});

/**
 * Test the undo preconditions and a no-op undo in the automigrator.
 */
add_task(function* checkUndoPreconditions() {
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
      TestUtils.executeSoon(function() {
        Services.obs.notifyObservers(null, "Migration:Ended", undefined);
      });
    },
  };

  gShimmedMigratorKeyPicker = function() {
    return "gobbledygook";
  };
  AutoMigrate.migrate("startup");
  let migrationFinishedPromise = TestUtils.topicObserved("Migration:Ended");
  Assert.strictEqual(gShimmedMigrator._getMigrateDataArgs, null,
                     "getMigrateData called with 'null' as a profile");

  let {BOOKMARKS, HISTORY, PASSWORDS} = Ci.nsIBrowserProfileMigrator;
  let expectedTypes = BOOKMARKS | HISTORY | PASSWORDS;
  Assert.deepEqual(gShimmedMigrator._migrateArgs, [expectedTypes, "startup", null],
                   "migrate called with 'null' as a profile");

  yield migrationFinishedPromise;
  Assert.ok(Preferences.has("browser.migrate.automigrate.started"),
            "Should have set start time pref");
  Assert.ok(Preferences.has("browser.migrate.automigrate.finished"),
            "Should have set finish time pref");
  Assert.ok(AutoMigrate.canUndo(), "Should be able to undo migration");

  let [beginRange, endRange] = AutoMigrate.getUndoRange();
  let stringRange = `beginRange: ${beginRange}; endRange: ${endRange}`;
  Assert.ok(beginRange <= endRange,
            "Migration should have started before or when it ended " + stringRange);

  yield AutoMigrate.undo();
  Assert.ok(true, "Should be able to finish an undo cycle.");
});

/**
 * Fake a migration and then try to undo it to verify all data gets removed.
 */
add_task(function* checkUndoRemoval() {
  let startTime = "" + Date.now();

  // Insert a login and check that that worked.
  let login = Cc["@mozilla.org/login-manager/loginInfo;1"].createInstance(Ci.nsILoginInfo);
  login.init("www.mozilla.org", "http://www.mozilla.org", null, "user", "pass", "userEl", "passEl");
  Services.logins.addLogin(login);
  let storedLogins = Services.logins.findLogins({}, "www.mozilla.org",
                                                "http://www.mozilla.org", null);
  Assert.equal(storedLogins.length, 1, "Should have 1 login");

  // Insert a bookmark and check that we have exactly 1 bookmark for that URI.
  yield PlacesUtils.bookmarks.insert({
    parentGuid: PlacesUtils.bookmarks.toolbarGuid,
    url: "http://www.example.org/",
    title: "Some example bookmark",
  });

  let bookmark = yield PlacesUtils.bookmarks.fetch({url: "http://www.example.org/"});
  Assert.ok(bookmark, "Should have a bookmark before undo");
  Assert.equal(bookmark.title, "Some example bookmark", "Should have correct bookmark before undo.");

  // Insert 2 history visits - one in the current migration time, one from before.
  let now_uSec = Date.now() * 1000;
  let visitedURI = Services.io.newURI("http://www.example.com/", null, null);
  yield PlacesTestUtils.addVisits([
    {uri: visitedURI, visitDate: now_uSec},
    {uri: visitedURI, visitDate: now_uSec - 100 * kUsecPerMin},
  ]);

  // Verify that both visits get reported.
  let opts = PlacesUtils.history.getNewQueryOptions();
  opts.resultType = opts.RESULTS_AS_VISIT;
  let query = PlacesUtils.history.getNewQuery();
  query.uri = visitedURI;
  let visits = PlacesUtils.history.executeQuery(query, opts);
  visits.root.containerOpen = true;
  Assert.equal(visits.root.childCount, 2, "Should have 2 visits");
  // Clean up:
  visits.root.containerOpen = false;

  // Now set finished pref:
  let endTime = "" + Date.now();
  Preferences.set("browser.migrate.automigrate.started", startTime);
  Preferences.set("browser.migrate.automigrate.finished", endTime);

  // Verify that we can undo, then undo:
  Assert.ok(AutoMigrate.canUndo(), "Should be possible to undo migration");
  yield AutoMigrate.undo();

  // Check that the undo removed the history visits:
  visits = PlacesUtils.history.executeQuery(query, opts);
  visits.root.containerOpen = true;
  Assert.equal(visits.root.childCount, 0, "Should have no more visits");
  visits.root.containerOpen = false;

  // Check that the undo removed the bookmarks:
  bookmark = yield PlacesUtils.bookmarks.fetch({url: "http://www.example.org/"});
  Assert.ok(!bookmark, "Should have no bookmarks after undo");

  // Check that the undo removed the passwords:
  storedLogins = Services.logins.findLogins({}, "www.mozilla.org",
                                            "http://www.mozilla.org", null);
  Assert.equal(storedLogins.length, 0, "Should have no logins");

  // Finally check prefs got cleared:
  Assert.ok(!Preferences.has("browser.migrate.automigrate.started"),
            "Should no longer have pref for migration start time.");
  Assert.ok(!Preferences.has("browser.migrate.automigrate.finished"),
            "Should no longer have pref for migration finish time.");
});

add_task(function* checkUndoDisablingByBookmarksAndPasswords() {
  let startTime = "" + Date.now();
  Services.prefs.setCharPref("browser.migrate.automigrate.started", startTime);
  // Now set finished pref:
  let endTime = "" + (Date.now() + 1000);
  Services.prefs.setCharPref("browser.migrate.automigrate.finished", endTime);
  AutoMigrate.maybeInitUndoObserver();

  ok(AutoMigrate.canUndo(), "Should be able to undo.");

  // Insert a login and check that that disabled undo.
  let login = Cc["@mozilla.org/login-manager/loginInfo;1"].createInstance(Ci.nsILoginInfo);
  login.init("www.mozilla.org", "http://www.mozilla.org", null, "user", "pass", "userEl", "passEl");
  Services.logins.addLogin(login);

  ok(!AutoMigrate.canUndo(), "Should no longer be able to undo.");
  Services.prefs.setCharPref("browser.migrate.automigrate.started", startTime);
  Services.prefs.setCharPref("browser.migrate.automigrate.finished", endTime);
  ok(AutoMigrate.canUndo(), "Should be able to undo.");
  AutoMigrate.maybeInitUndoObserver();

  // Insert a bookmark and check that that disabled undo.
  yield PlacesUtils.bookmarks.insert({
    parentGuid: PlacesUtils.bookmarks.toolbarGuid,
    url: "http://www.example.org/",
    title: "Some example bookmark",
  });
  ok(!AutoMigrate.canUndo(), "Should no longer be able to undo.");

  try {
    Services.logins.removeAllLogins();
  } catch (ex) {}
  yield PlacesUtils.bookmarks.eraseEverything();
});

