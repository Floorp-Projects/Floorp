/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

/**
 * Tests that nsBrowserGlue is correctly interpreting the preferences settable
 * by the user or by other components.
 */

const PREF_IMPORT_BOOKMARKS_HTML = "browser.places.importBookmarksHTML";
const PREF_RESTORE_DEFAULT_BOOKMARKS = "browser.bookmarks.restore_default_bookmarks";
const PREF_SMART_BOOKMARKS_VERSION = "browser.places.smartBookmarksVersion";
const PREF_AUTO_EXPORT_HTML = "browser.bookmarks.autoExportHTML";

const TOPIC_BROWSERGLUE_TEST = "browser-glue-test";
const TOPICDATA_FORCE_PLACES_INIT = "force-places-init";

var bg = Cc["@mozilla.org/browser/browserglue;1"].
         getService(Ci.nsIObserver);

function run_test() {
  // Create our bookmarks.html from bookmarks.glue.html.
  create_bookmarks_html("bookmarks.glue.html");

  remove_all_JSON_backups();

  // Create our JSON backup from bookmarks.glue.json.
  create_JSON_backup("bookmarks.glue.json");

  run_next_test();
}

do_register_cleanup(function() {
  remove_bookmarks_html();
  remove_all_JSON_backups();

  return PlacesUtils.bookmarks.eraseEverything();
});

function simulatePlacesInit() {
  do_print("Simulate Places init");
  // Force nsBrowserGlue::_initPlaces().
  bg.observe(null, TOPIC_BROWSERGLUE_TEST, TOPICDATA_FORCE_PLACES_INIT);
  return promiseTopicObserved("places-browser-init-complete");
}

add_task(function* test_checkPreferences() {
  // Initialize Places through the History Service and check that a new
  // database has been created.
  Assert.equal(PlacesUtils.history.databaseStatus,
               PlacesUtils.history.DATABASE_STATUS_CREATE);

  // Wait for Places init notification.
  yield promiseTopicObserved("places-browser-init-complete");

  // Ensure preferences status.
  Assert.ok(!Services.prefs.getBoolPref(PREF_AUTO_EXPORT_HTML));

  Assert.throws(() => Services.prefs.getBoolPref(PREF_IMPORT_BOOKMARKS_HTML));
  Assert.throws(() => Services.prefs.getBoolPref(PREF_RESTORE_DEFAULT_BOOKMARKS));
});

add_task(function* test_import() {
  do_print("Import from bookmarks.html if importBookmarksHTML is true.");

  yield PlacesUtils.bookmarks.eraseEverything();

  // Sanity check: we should not have any bookmark on the toolbar.
  Assert.ok(!(yield PlacesUtils.bookmarks.fetch({
    parentGuid: PlacesUtils.bookmarks.toolbarGuid,
    index: 0
  })));

  // Set preferences.
  Services.prefs.setBoolPref(PREF_IMPORT_BOOKMARKS_HTML, true);

  yield simulatePlacesInit();

  // Check bookmarks.html has been imported, and a smart bookmark has been
  // created.
  let bm = yield PlacesUtils.bookmarks.fetch({
    parentGuid: PlacesUtils.bookmarks.toolbarGuid,
    index: SMART_BOOKMARKS_ON_TOOLBAR
  });
  Assert.equal(bm.title, "example");

  // Check preferences have been reverted.
  Assert.ok(!Services.prefs.getBoolPref(PREF_IMPORT_BOOKMARKS_HTML));
});

add_task(function* test_import_noSmartBookmarks() {
  do_print("import from bookmarks.html, but don't create smart bookmarks " +
              "if they are disabled");

  yield PlacesUtils.bookmarks.eraseEverything();

  // Sanity check: we should not have any bookmark on the toolbar.
  Assert.ok(!(yield PlacesUtils.bookmarks.fetch({
    parentGuid: PlacesUtils.bookmarks.toolbarGuid,
    index: 0
  })));

  // Set preferences.
  Services.prefs.setIntPref(PREF_SMART_BOOKMARKS_VERSION, -1);
  Services.prefs.setBoolPref(PREF_IMPORT_BOOKMARKS_HTML, true);

  yield simulatePlacesInit();

  // Check bookmarks.html has been imported, but smart bookmarks have not
  // been created.
  let bm = yield PlacesUtils.bookmarks.fetch({
    parentGuid: PlacesUtils.bookmarks.toolbarGuid,
    index: 0
  });
  Assert.equal(bm.title, "example");

  // Check preferences have been reverted.
  Assert.ok(!Services.prefs.getBoolPref(PREF_IMPORT_BOOKMARKS_HTML));
});

add_task(function* test_import_autoExport_updatedSmartBookmarks() {
  do_print("Import from bookmarks.html, but don't create smart bookmarks " +
              "if autoExportHTML is true and they are at latest version");

  yield PlacesUtils.bookmarks.eraseEverything();

  // Sanity check: we should not have any bookmark on the toolbar.
  Assert.ok(!(yield PlacesUtils.bookmarks.fetch({
    parentGuid: PlacesUtils.bookmarks.toolbarGuid,
    index: 0
  })));

  // Set preferences.
  Services.prefs.setIntPref(PREF_SMART_BOOKMARKS_VERSION, 999);
  Services.prefs.setBoolPref(PREF_AUTO_EXPORT_HTML, true);
  Services.prefs.setBoolPref(PREF_IMPORT_BOOKMARKS_HTML, true);

  yield simulatePlacesInit();

  // Check bookmarks.html has been imported, but smart bookmarks have not
  // been created.
  let bm = yield PlacesUtils.bookmarks.fetch({
    parentGuid: PlacesUtils.bookmarks.toolbarGuid,
    index: 0
  });
  Assert.equal(bm.title, "example");

  // Check preferences have been reverted.
  Assert.ok(!Services.prefs.getBoolPref(PREF_IMPORT_BOOKMARKS_HTML));

  Services.prefs.setBoolPref(PREF_AUTO_EXPORT_HTML, false);
});

add_task(function* test_import_autoExport_oldSmartBookmarks() {
  do_print("Import from bookmarks.html, and create smart bookmarks if " +
              "autoExportHTML is true and they are not at latest version.");

  yield PlacesUtils.bookmarks.eraseEverything();

  // Sanity check: we should not have any bookmark on the toolbar.
  Assert.ok(!(yield PlacesUtils.bookmarks.fetch({
    parentGuid: PlacesUtils.bookmarks.toolbarGuid,
    index: 0
  })));

  // Set preferences.
  Services.prefs.setIntPref(PREF_SMART_BOOKMARKS_VERSION, 0);
  Services.prefs.setBoolPref(PREF_AUTO_EXPORT_HTML, true);
  Services.prefs.setBoolPref(PREF_IMPORT_BOOKMARKS_HTML, true);

  yield simulatePlacesInit();

  // Check bookmarks.html has been imported, but smart bookmarks have not
  // been created.
  let bm = yield PlacesUtils.bookmarks.fetch({
    parentGuid: PlacesUtils.bookmarks.toolbarGuid,
    index: SMART_BOOKMARKS_ON_TOOLBAR
  });
  Assert.equal(bm.title, "example");

  // Check preferences have been reverted.
  Assert.ok(!Services.prefs.getBoolPref(PREF_IMPORT_BOOKMARKS_HTML));

  Services.prefs.setBoolPref(PREF_AUTO_EXPORT_HTML, false);
});

add_task(function* test_restore() {
  do_print("restore from default bookmarks.html if " +
              "restore_default_bookmarks is true.");

  yield PlacesUtils.bookmarks.eraseEverything();

  // Sanity check: we should not have any bookmark on the toolbar.
  Assert.ok(!(yield PlacesUtils.bookmarks.fetch({
    parentGuid: PlacesUtils.bookmarks.toolbarGuid,
    index: 0
  })));

  // Set preferences.
  Services.prefs.setBoolPref(PREF_RESTORE_DEFAULT_BOOKMARKS, true);

  yield simulatePlacesInit();

  // Check bookmarks.html has been restored.
  Assert.ok(yield PlacesUtils.bookmarks.fetch({
    parentGuid: PlacesUtils.bookmarks.toolbarGuid,
    index: SMART_BOOKMARKS_ON_TOOLBAR
  }));

  // Check preferences have been reverted.
  Assert.ok(!Services.prefs.getBoolPref(PREF_RESTORE_DEFAULT_BOOKMARKS));
});

add_task(function* test_restore_import() {
  do_print("setting both importBookmarksHTML and " +
              "restore_default_bookmarks should restore defaults.");

  yield PlacesUtils.bookmarks.eraseEverything();

  // Sanity check: we should not have any bookmark on the toolbar.
  Assert.ok(!(yield PlacesUtils.bookmarks.fetch({
    parentGuid: PlacesUtils.bookmarks.toolbarGuid,
    index: 0
  })));

  // Set preferences.
  Services.prefs.setBoolPref(PREF_IMPORT_BOOKMARKS_HTML, true);
  Services.prefs.setBoolPref(PREF_RESTORE_DEFAULT_BOOKMARKS, true);

  yield simulatePlacesInit();

  // Check bookmarks.html has been restored.
  Assert.ok(yield PlacesUtils.bookmarks.fetch({
    parentGuid: PlacesUtils.bookmarks.toolbarGuid,
    index: SMART_BOOKMARKS_ON_TOOLBAR
  }));

  // Check preferences have been reverted.
  Assert.ok(!Services.prefs.getBoolPref(PREF_RESTORE_DEFAULT_BOOKMARKS));
  Assert.ok(!Services.prefs.getBoolPref(PREF_IMPORT_BOOKMARKS_HTML));
});
