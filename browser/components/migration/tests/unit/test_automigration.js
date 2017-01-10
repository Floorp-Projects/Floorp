"use strict";

let AutoMigrateBackstage = Cu.import("resource:///modules/AutoMigrate.jsm"); /* globals AutoMigrate */

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

// This should be replaced by using History.fetch with a fetchVisits option,
// once that becomes available
function* visitsForURL(url) {
  let visitCount = 0;
  let db = yield PlacesUtils.promiseDBConnection();
  visitCount = yield db.execute(
    `SELECT count(*) FROM moz_historyvisits v
     JOIN moz_places h ON h.id = v.place_id
     WHERE url_hash = hash(:url) AND url = :url`,
     {url});
  visitCount = visitCount[0].getInt64(0);
  return visitCount;
}


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
  Assert.ok(Preferences.has("browser.migrate.automigrate.browser"),
            "Should have set browser pref");
  Assert.ok((yield AutoMigrate.canUndo()), "Should be able to undo migration");

  yield AutoMigrate.undo();
  Assert.ok(true, "Should be able to finish an undo cycle.");
});

/**
 * Fake a migration and then try to undo it to verify all data gets removed.
 */
add_task(function* checkUndoRemoval() {
  MigrationUtils.initializeUndoData();
  // Insert a login and check that that worked.
  MigrationUtils.insertLoginWrapper({
    hostname: "www.mozilla.org",
    formSubmitURL: "http://www.mozilla.org",
    username: "user",
    password: "pass",
  });
  let storedLogins = Services.logins.findLogins({}, "www.mozilla.org",
                                                "http://www.mozilla.org", null);
  Assert.equal(storedLogins.length, 1, "Should have 1 login");

  // Insert a bookmark and check that we have exactly 1 bookmark for that URI.
  yield MigrationUtils.insertBookmarkWrapper({
    parentGuid: PlacesUtils.bookmarks.toolbarGuid,
    url: "http://www.example.org/",
    title: "Some example bookmark",
  });

  let bookmark = yield PlacesUtils.bookmarks.fetch({url: "http://www.example.org/"});
  Assert.ok(bookmark, "Should have a bookmark before undo");
  Assert.equal(bookmark.title, "Some example bookmark", "Should have correct bookmark before undo.");

  // Insert 2 history visits
  let now_uSec = Date.now() * 1000;
  let visitedURI = Services.io.newURI("http://www.example.com/");
  let frecencyUpdatePromise = new Promise(resolve => {
    let expectedChanges = 2;
    let observer = {
      onFrecencyChanged() {
        if (!--expectedChanges) {
          PlacesUtils.history.removeObserver(observer);
          resolve();
        }
      },
    };
    PlacesUtils.history.addObserver(observer, false);
  });
  yield MigrationUtils.insertVisitsWrapper([{
    uri: visitedURI,
    visits: [
      {
        transitionType: PlacesUtils.history.TRANSITION_LINK,
        visitDate: now_uSec,
      },
      {
        transitionType: PlacesUtils.history.TRANSITION_LINK,
        visitDate: now_uSec - 100 * kUsecPerMin,
      },
    ]
  }]);
  yield frecencyUpdatePromise;

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

  yield AutoMigrate.saveUndoState();

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
});

add_task(function* checkUndoBookmarksState() {
  MigrationUtils.initializeUndoData();
  const {TYPE_FOLDER, TYPE_BOOKMARK} = PlacesUtils.bookmarks;
  let title = "Some example bookmark";
  let url = "http://www.example.com";
  let parentGuid = PlacesUtils.bookmarks.toolbarGuid;
  let {guid, lastModified} = yield MigrationUtils.insertBookmarkWrapper({
    title, url, parentGuid
  });
  Assert.deepEqual((yield MigrationUtils.stopAndRetrieveUndoData()).get("bookmarks"),
      [{lastModified, parentGuid, guid, type: TYPE_BOOKMARK}]);

  MigrationUtils.initializeUndoData();
  ({guid, lastModified} = yield MigrationUtils.insertBookmarkWrapper({
    title, parentGuid, type: TYPE_FOLDER
  }));
  let folder = {guid, lastModified, parentGuid, type: TYPE_FOLDER};
  let folderGuid = folder.guid;
  ({guid, lastModified} = yield MigrationUtils.insertBookmarkWrapper({
    title, url, parentGuid: folderGuid
  }));
  let kid1 = {guid, lastModified, parentGuid: folderGuid, type: TYPE_BOOKMARK};
  ({guid, lastModified} = yield MigrationUtils.insertBookmarkWrapper({
    title, url, parentGuid: folderGuid
  }));
  let kid2 = {guid, lastModified, parentGuid: folderGuid, type: TYPE_BOOKMARK};

  let bookmarksUndo = (yield MigrationUtils.stopAndRetrieveUndoData()).get("bookmarks");
  Assert.equal(bookmarksUndo.length, 3);
  // We expect that the last modified time from first kid #1 and then kid #2
  // has been propagated to the folder:
  folder.lastModified = kid2.lastModified;
  // Not just using deepEqual on the entire array (which should work) because
  // the failure messages get truncated by xpcshell which is unhelpful.
  Assert.deepEqual(bookmarksUndo[0], folder);
  Assert.deepEqual(bookmarksUndo[1], kid1);
  Assert.deepEqual(bookmarksUndo[2], kid2);
  yield PlacesUtils.bookmarks.eraseEverything();
});

add_task(function* testBookmarkRemovalByUndo() {
  const {TYPE_FOLDER} = PlacesUtils.bookmarks;
  MigrationUtils.initializeUndoData();
  let title = "Some example bookmark";
  let url = "http://www.mymagicaluniqueurl.com";
  let parentGuid = PlacesUtils.bookmarks.toolbarGuid;
  let {guid} = yield MigrationUtils.insertBookmarkWrapper({
    title: "Some folder", parentGuid, type: TYPE_FOLDER
  });
  let folderGuid = guid;
  let itemsToRemove = [];
  ({guid} = yield MigrationUtils.insertBookmarkWrapper({
    title: "Inner folder", parentGuid: folderGuid, type: TYPE_FOLDER
  }));
  let innerFolderGuid = guid;
  itemsToRemove.push(innerFolderGuid);

  ({guid} = yield MigrationUtils.insertBookmarkWrapper({
    title: "Inner inner folder", parentGuid: innerFolderGuid, type: TYPE_FOLDER
  }));
  itemsToRemove.push(guid);

  ({guid} = yield MigrationUtils.insertBookmarkWrapper({
    title: "Inner nested item", url: "http://inner-nested-example.com", parentGuid: guid
  }));
  itemsToRemove.push(guid);

  ({guid} = yield MigrationUtils.insertBookmarkWrapper({
    title, url, parentGuid: folderGuid
  }));
  itemsToRemove.push(guid);

  for (let toBeRemovedGuid of itemsToRemove) {
    let dbResultForGuid = yield PlacesUtils.bookmarks.fetch(toBeRemovedGuid);
    Assert.ok(dbResultForGuid, "Should be able to find items that will be removed.");
  }
  let bookmarkUndoState = (yield MigrationUtils.stopAndRetrieveUndoData()).get("bookmarks");
  // Now insert a separate item into this folder, not related to the migration.
  let newItem = yield PlacesUtils.bookmarks.insert(
    {title: "Not imported", parentGuid: folderGuid, url: "http://www.example.com"}
  );

  yield AutoMigrate._removeUnchangedBookmarks(bookmarkUndoState);
  Assert.ok(true, "Successfully removed imported items.");

  let itemFromDB = yield PlacesUtils.bookmarks.fetch(newItem.guid);
  Assert.ok(itemFromDB, "Item we inserted outside of migration is still there.");
  itemFromDB = yield PlacesUtils.bookmarks.fetch(folderGuid);
  Assert.ok(itemFromDB, "Folder we inserted in migration is still there because of new kids.");
  for (let removedGuid of itemsToRemove) {
    let dbResultForGuid = yield PlacesUtils.bookmarks.fetch(removedGuid);
    let dbgStr = dbResultForGuid && dbResultForGuid.title;
    Assert.equal(null, dbResultForGuid, "Should not be able to find items that should have been removed, but found " + dbgStr);
  }
  yield PlacesUtils.bookmarks.eraseEverything();
});

add_task(function* checkUndoLoginsState() {
  MigrationUtils.initializeUndoData();
  MigrationUtils.insertLoginWrapper({
    username: "foo",
    password: "bar",
    hostname: "https://example.com",
    formSubmitURL: "https://example.com/",
    timeCreated: new Date(),
  });
  let storedLogins = Services.logins.findLogins({}, "https://example.com", "", "");
  let storedLogin = storedLogins[0];
  storedLogin.QueryInterface(Ci.nsILoginMetaInfo);
  let {guid, timePasswordChanged} = storedLogin;
  let undoLoginData = (yield MigrationUtils.stopAndRetrieveUndoData()).get("logins");
  Assert.deepEqual([{guid, timePasswordChanged}], undoLoginData);
  Services.logins.removeAllLogins();
});

add_task(function* testLoginsRemovalByUndo() {
  MigrationUtils.initializeUndoData();
  MigrationUtils.insertLoginWrapper({
    username: "foo",
    password: "bar",
    hostname: "https://example.com",
    formSubmitURL: "https://example.com/",
    timeCreated: new Date(),
  });
  MigrationUtils.insertLoginWrapper({
    username: "foo",
    password: "bar",
    hostname: "https://example.org",
    formSubmitURL: "https://example.org/",
    timeCreated: new Date(new Date().getTime() - 10000),
  });
  // This should update the existing login
  LoginHelper.maybeImportLogin({
    username: "foo",
    password: "bazzy",
    hostname: "https://example.org",
    formSubmitURL: "https://example.org/",
    timePasswordChanged: new Date(),
  });
  Assert.equal(1, LoginHelper.searchLoginsWithObject({hostname: "https://example.org", formSubmitURL: "https://example.org/"}).length,
               "Should be only 1 login for example.org (that was updated)");
  let undoLoginData = (yield MigrationUtils.stopAndRetrieveUndoData()).get("logins");

  yield AutoMigrate._removeUnchangedLogins(undoLoginData);
  Assert.equal(0, LoginHelper.searchLoginsWithObject({hostname: "https://example.com", formSubmitURL: "https://example.com/"}).length,
                   "unchanged example.com entry should have been removed.");
  Assert.equal(1, LoginHelper.searchLoginsWithObject({hostname: "https://example.org", formSubmitURL: "https://example.org/"}).length,
                   "changed example.org entry should have persisted.");
  Services.logins.removeAllLogins();
});

add_task(function* checkUndoVisitsState() {
  MigrationUtils.initializeUndoData();
  yield MigrationUtils.insertVisitsWrapper([{
    uri: NetUtil.newURI("http://www.example.com/"),
    title: "Example",
    visits: [{
      visitDate: new Date("2015-07-10").getTime() * 1000,
      transitionType: Ci.nsINavHistoryService.TRANSITION_LINK,
    }, {
      visitDate: new Date("2015-09-10").getTime() * 1000,
      transitionType: Ci.nsINavHistoryService.TRANSITION_LINK,
    }, {
      visitDate: new Date("2015-08-10").getTime() * 1000,
      transitionType: Ci.nsINavHistoryService.TRANSITION_LINK,
    }],
  }, {
    uri: NetUtil.newURI("http://www.example.org/"),
    title: "Example",
    visits: [{
      visitDate: new Date("2016-04-03").getTime() * 1000,
      transitionType: Ci.nsINavHistoryService.TRANSITION_LINK,
    }, {
      visitDate: new Date("2015-08-03").getTime() * 1000,
      transitionType: Ci.nsINavHistoryService.TRANSITION_LINK,
    }],
  }, {
    uri: NetUtil.newURI("http://www.example.com/"),
    title: "Example",
    visits: [{
      visitDate: new Date("2015-10-10").getTime() * 1000,
      transitionType: Ci.nsINavHistoryService.TRANSITION_LINK,
    }],
  }]);
  let undoVisitData = (yield MigrationUtils.stopAndRetrieveUndoData()).get("visits");
  Assert.deepEqual(Array.from(undoVisitData.map(v => v.url)).sort(),
                   ["http://www.example.com/", "http://www.example.org/"]);
  Assert.deepEqual(undoVisitData.find(v => v.url == "http://www.example.com/"), {
    url: "http://www.example.com/",
    visitCount: 4,
    first: new Date("2015-07-10").getTime() * 1000,
    last: new Date("2015-10-10").getTime() * 1000,
  });
  Assert.deepEqual(undoVisitData.find(v => v.url == "http://www.example.org/"), {
    url: "http://www.example.org/",
    visitCount: 2,
    first: new Date("2015-08-03").getTime() * 1000,
    last: new Date("2016-04-03").getTime() * 1000,
  });

  yield PlacesTestUtils.clearHistory();
});

add_task(function* checkUndoVisitsState() {
  MigrationUtils.initializeUndoData();
  yield MigrationUtils.insertVisitsWrapper([{
    uri: NetUtil.newURI("http://www.example.com/"),
    title: "Example",
    visits: [{
      visitDate: new Date("2015-07-10").getTime() * 1000,
      transitionType: Ci.nsINavHistoryService.TRANSITION_LINK,
    }, {
      visitDate: new Date("2015-09-10").getTime() * 1000,
      transitionType: Ci.nsINavHistoryService.TRANSITION_LINK,
    }, {
      visitDate: new Date("2015-08-10").getTime() * 1000,
      transitionType: Ci.nsINavHistoryService.TRANSITION_LINK,
    }],
  }, {
    uri: NetUtil.newURI("http://www.example.org/"),
    title: "Example",
    visits: [{
      visitDate: new Date("2016-04-03").getTime() * 1000,
      transitionType: Ci.nsINavHistoryService.TRANSITION_LINK,
    }, {
      visitDate: new Date("2015-08-03").getTime() * 1000,
      transitionType: Ci.nsINavHistoryService.TRANSITION_LINK,
    }],
  }, {
    uri: NetUtil.newURI("http://www.example.com/"),
    title: "Example",
    visits: [{
      visitDate: new Date("2015-10-10").getTime() * 1000,
      transitionType: Ci.nsINavHistoryService.TRANSITION_LINK,
    }],
  }, {
    uri: NetUtil.newURI("http://www.mozilla.org/"),
    title: "Example",
    visits: [{
      visitDate: new Date("2015-01-01").getTime() * 1000,
      transitionType: Ci.nsINavHistoryService.TRANSITION_LINK,
    }],
  }]);

  // We have to wait until frecency updates have been handled in order
  // to accurately determine whether we're doing the right thing.
  let frecencyUpdatesHandled = new Promise(resolve => {
    PlacesUtils.history.addObserver({
      onFrecencyChanged(aURI) {
        if (aURI.spec == "http://www.unrelated.org/") {
          PlacesUtils.history.removeObserver(this);
          resolve();
        }
      }
    }, false);
  });
  yield PlacesUtils.history.insertMany([{
    url: "http://www.example.com/",
    title: "Example",
    visits: [{
      date: new Date("2015-08-16"),
    }],
  }, {
    url: "http://www.example.org/",
    title: "Example",
    visits: [{
      date: new Date("2016-01-03"),
    }, {
      date: new Date("2015-05-03"),
    }],
  }, {
    url: "http://www.unrelated.org/",
    title: "Unrelated",
    visits: [{
      date: new Date("2015-09-01"),
    }],
  }]);
  yield frecencyUpdatesHandled;
  let undoVisitData = (yield MigrationUtils.stopAndRetrieveUndoData()).get("visits");

  let frecencyChangesExpected = new Map([
    ["http://www.example.com/", PromiseUtils.defer()],
    ["http://www.example.org/", PromiseUtils.defer()]
  ]);
  let uriDeletedExpected = new Map([
    ["http://www.mozilla.org/", PromiseUtils.defer()],
  ]);
  let wrongMethodDeferred = PromiseUtils.defer();
  let observer = {
    onBeginUpdateBatch() {},
    onEndUpdateBatch() {},
    onVisit(uri) {
      wrongMethodDeferred.reject(new Error("Unexpected call to onVisit " + uri.spec));
    },
    onTitleChanged(uri) {
      wrongMethodDeferred.reject(new Error("Unexpected call to onTitleChanged " + uri.spec));
    },
    onClearHistory() {
      wrongMethodDeferred.reject("Unexpected call to onClearHistory");
    },
    onPageChanged(uri) {
      wrongMethodDeferred.reject(new Error("Unexpected call to onPageChanged " + uri.spec));
    },
    onFrecencyChanged(aURI) {
      do_print("frecency change");
      Assert.ok(frecencyChangesExpected.has(aURI.spec),
                "Should be expecting frecency change for " + aURI.spec);
      frecencyChangesExpected.get(aURI.spec).resolve();
    },
    onManyFrecenciesChanged() {
      do_print("Many frecencies changed");
      wrongMethodDeferred.reject(new Error("This test can't deal with onManyFrecenciesChanged to be called"));
    },
    onDeleteURI(aURI) {
      do_print("delete uri");
      Assert.ok(uriDeletedExpected.has(aURI.spec),
                "Should be expecting uri deletion for " + aURI.spec);
      uriDeletedExpected.get(aURI.spec).resolve();
    },
  };
  PlacesUtils.history.addObserver(observer, false);

  yield AutoMigrate._removeSomeVisits(undoVisitData);
  PlacesUtils.history.removeObserver(observer);
  yield Promise.all(uriDeletedExpected.values());
  yield Promise.all(frecencyChangesExpected.values());

  Assert.equal(yield visitsForURL("http://www.example.com/"), 1,
               "1 example.com visit (out of 5) should have persisted despite being within the range, due to limiting");
  Assert.equal(yield visitsForURL("http://www.mozilla.org/"), 0,
               "0 mozilla.org visits should have persisted (out of 1).");
  Assert.equal(yield visitsForURL("http://www.example.org/"), 2,
               "2 example.org visits should have persisted (out of 4).");
  Assert.equal(yield visitsForURL("http://www.unrelated.org/"), 1,
               "1 unrelated.org visits should have persisted as it's not involved in the import.");
  yield PlacesTestUtils.clearHistory();
});

