/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

/**
 * Tests that nsBrowserGlue is correctly interpreting the preferences settable
 * by the user or by other components.
 */

const PREF_IMPORT_BOOKMARKS_HTML = "browser.places.importBookmarksHTML";
const PREF_RESTORE_DEFAULT_BOOKMARKS =
  "browser.bookmarks.restore_default_bookmarks";
const PREF_AUTO_EXPORT_HTML = "browser.bookmarks.autoExportHTML";

const TOPIC_BROWSERGLUE_TEST = "browser-glue-test";
const TOPICDATA_FORCE_PLACES_INIT = "test-force-places-init";

var bg = Cc["@mozilla.org/browser/browserglue;1"].getService(Ci.nsIObserver);

add_task(async function setup() {
  // Create our bookmarks.html from bookmarks.glue.html.
  create_bookmarks_html("bookmarks.glue.html");

  remove_all_JSON_backups();

  // Create our JSON backup from bookmarks.glue.json.
  create_JSON_backup("bookmarks.glue.json");

  registerCleanupFunction(function() {
    remove_bookmarks_html();
    remove_all_JSON_backups();

    return PlacesUtils.bookmarks.eraseEverything();
  });
});

function simulatePlacesInit() {
  info("Simulate Places init");
  // Force nsBrowserGlue::_initPlaces().
  bg.observe(null, TOPIC_BROWSERGLUE_TEST, TOPICDATA_FORCE_PLACES_INIT);
  return promiseTopicObserved("places-browser-init-complete");
}

add_task(async function test_checkPreferences() {
  // Initialize Places through the History Service and check that a new
  // database has been created.
  let promiseComplete = promiseTopicObserved("places-browser-init-complete");
  Assert.equal(
    PlacesUtils.history.databaseStatus,
    PlacesUtils.history.DATABASE_STATUS_CREATE
  );
  await promiseComplete;

  // Ensure preferences status.
  Assert.ok(!Services.prefs.getBoolPref(PREF_AUTO_EXPORT_HTML));

  Assert.throws(
    () => Services.prefs.getBoolPref(PREF_IMPORT_BOOKMARKS_HTML),
    /NS_ERROR_UNEXPECTED/
  );
  Assert.throws(
    () => Services.prefs.getBoolPref(PREF_RESTORE_DEFAULT_BOOKMARKS),
    /NS_ERROR_UNEXPECTED/
  );
});

add_task(async function test_import() {
  info("Import from bookmarks.html if importBookmarksHTML is true.");

  await PlacesUtils.bookmarks.eraseEverything();

  // Sanity check: we should not have any bookmark on the toolbar.
  Assert.ok(
    !(await PlacesUtils.bookmarks.fetch({
      parentGuid: PlacesUtils.bookmarks.toolbarGuid,
      index: 0,
    }))
  );

  // Set preferences.
  Services.prefs.setBoolPref(PREF_IMPORT_BOOKMARKS_HTML, true);

  await simulatePlacesInit();

  // Check bookmarks.html has been imported.
  let bm = await PlacesUtils.bookmarks.fetch({
    parentGuid: PlacesUtils.bookmarks.toolbarGuid,
    index: 0,
  });
  Assert.equal(bm.title, "example");

  // Check preferences have been reverted.
  Assert.ok(!Services.prefs.getBoolPref(PREF_IMPORT_BOOKMARKS_HTML));
});

add_task(async function test_restore() {
  info(
    "restore from default bookmarks.html if " +
      "restore_default_bookmarks is true."
  );

  await PlacesUtils.bookmarks.eraseEverything();

  // Sanity check: we should not have any bookmark on the toolbar.
  Assert.ok(
    !(await PlacesUtils.bookmarks.fetch({
      parentGuid: PlacesUtils.bookmarks.toolbarGuid,
      index: 0,
    }))
  );

  // Set preferences.
  Services.prefs.setBoolPref(PREF_RESTORE_DEFAULT_BOOKMARKS, true);

  await simulatePlacesInit();

  // Check bookmarks.html has been restored.
  Assert.ok(
    await PlacesUtils.bookmarks.fetch({
      parentGuid: PlacesUtils.bookmarks.toolbarGuid,
      index: 0,
    })
  );

  // Check preferences have been reverted.
  Assert.ok(!Services.prefs.getBoolPref(PREF_RESTORE_DEFAULT_BOOKMARKS));
});

add_task(async function test_restore_import() {
  info(
    "setting both importBookmarksHTML and " +
      "restore_default_bookmarks should restore defaults."
  );

  await PlacesUtils.bookmarks.eraseEverything();

  // Sanity check: we should not have any bookmark on the toolbar.
  Assert.ok(
    !(await PlacesUtils.bookmarks.fetch({
      parentGuid: PlacesUtils.bookmarks.toolbarGuid,
      index: 0,
    }))
  );

  // Set preferences.
  Services.prefs.setBoolPref(PREF_IMPORT_BOOKMARKS_HTML, true);
  Services.prefs.setBoolPref(PREF_RESTORE_DEFAULT_BOOKMARKS, true);

  await simulatePlacesInit();

  // Check bookmarks.html has been restored.
  Assert.ok(
    await PlacesUtils.bookmarks.fetch({
      parentGuid: PlacesUtils.bookmarks.toolbarGuid,
      index: 0,
    })
  );

  // Check preferences have been reverted.
  Assert.ok(!Services.prefs.getBoolPref(PREF_RESTORE_DEFAULT_BOOKMARKS));
  Assert.ok(!Services.prefs.getBoolPref(PREF_IMPORT_BOOKMARKS_HTML));
});
