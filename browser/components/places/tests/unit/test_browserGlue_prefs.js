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

let bg = Cc["@mozilla.org/browser/browserglue;1"].
         getService(Ci.nsIBrowserGlue);

function waitForImportAndSmartBookmarks(aCallback) {
  Services.obs.addObserver(function waitImport() {
    Services.obs.removeObserver(waitImport, "bookmarks-restore-success");
    // Delay to test eventual smart bookmarks creation.
    do_execute_soon(function () {
      promiseAsyncUpdates().then(aCallback);
    });
  }, "bookmarks-restore-success", false);
}

[

  // This test must be the first one.
  function test_checkPreferences() {
    // Initialize Places through the History Service and check that a new
    // database has been created.
    do_check_eq(PlacesUtils.history.databaseStatus,
                PlacesUtils.history.DATABASE_STATUS_CREATE);

    // Wait for Places init notification.
    Services.obs.addObserver(function(aSubject, aTopic, aData) {
      Services.obs.removeObserver(arguments.callee,
                                  "places-browser-init-complete");
      do_execute_soon(function () {
        // Ensure preferences status.
        do_check_false(Services.prefs.getBoolPref(PREF_AUTO_EXPORT_HTML));

        try {
          do_check_false(Services.prefs.getBoolPref(PREF_IMPORT_BOOKMARKS_HTML));
          do_throw("importBookmarksHTML pref should not exist");
        }
        catch(ex) {}

        try {
          do_check_false(Services.prefs.getBoolPref(PREF_RESTORE_DEFAULT_BOOKMARKS));
          do_throw("importBookmarksHTML pref should not exist");
        }
        catch(ex) {}

        run_next_test();
      });
    }, "places-browser-init-complete", false);
  },

  function test_import()
  {
    do_log_info("Import from bookmarks.html if importBookmarksHTML is true.");

    remove_all_bookmarks();
    // Sanity check: we should not have any bookmark on the toolbar.
    let itemId =
      PlacesUtils.bookmarks.getIdForItemAt(PlacesUtils.toolbarFolderId, 0);
    do_check_eq(itemId, -1);

    // Set preferences.
    Services.prefs.setBoolPref(PREF_IMPORT_BOOKMARKS_HTML, true);

    waitForImportAndSmartBookmarks(function () {
      // Check bookmarks.html has been imported, and a smart bookmark has been
      // created.
      itemId = PlacesUtils.bookmarks.getIdForItemAt(PlacesUtils.toolbarFolderId,
                                                    SMART_BOOKMARKS_ON_TOOLBAR);
      do_check_eq(PlacesUtils.bookmarks.getItemTitle(itemId), "example");
      // Check preferences have been reverted.
      do_check_false(Services.prefs.getBoolPref(PREF_IMPORT_BOOKMARKS_HTML));

      run_next_test();
    });
    // Force nsBrowserGlue::_initPlaces().
    do_log_info("Simulate Places init");
    bg.QueryInterface(Ci.nsIObserver).observe(null,
                                              TOPIC_BROWSERGLUE_TEST,
                                              TOPICDATA_FORCE_PLACES_INIT);
  },

  function test_import_noSmartBookmarks()
  {
    do_log_info("import from bookmarks.html, but don't create smart bookmarks \
                 if they are disabled");

    remove_all_bookmarks();
    // Sanity check: we should not have any bookmark on the toolbar.
    let itemId =
      PlacesUtils.bookmarks.getIdForItemAt(PlacesUtils.toolbarFolderId, 0);
    do_check_eq(itemId, -1);

    // Set preferences.
    Services.prefs.setIntPref(PREF_SMART_BOOKMARKS_VERSION, -1);
    Services.prefs.setBoolPref(PREF_IMPORT_BOOKMARKS_HTML, true);

    waitForImportAndSmartBookmarks(function () {
      // Check bookmarks.html has been imported, but smart bookmarks have not
      // been created.
      itemId =
        PlacesUtils.bookmarks.getIdForItemAt(PlacesUtils.toolbarFolderId, 0);
      do_check_eq(PlacesUtils.bookmarks.getItemTitle(itemId), "example");
      // Check preferences have been reverted.
      do_check_false(Services.prefs.getBoolPref(PREF_IMPORT_BOOKMARKS_HTML));

      run_next_test();
    });
    // Force nsBrowserGlue::_initPlaces().
    do_log_info("Simulate Places init");
    bg.QueryInterface(Ci.nsIObserver).observe(null,
                                              TOPIC_BROWSERGLUE_TEST,
                                              TOPICDATA_FORCE_PLACES_INIT);
  },

  function test_import_autoExport_updatedSmartBookmarks()
  {
    do_log_info("Import from bookmarks.html, but don't create smart bookmarks \
                 if autoExportHTML is true and they are at latest version");

    remove_all_bookmarks();
    // Sanity check: we should not have any bookmark on the toolbar.
    let itemId =
      PlacesUtils.bookmarks.getIdForItemAt(PlacesUtils.toolbarFolderId, 0);
    do_check_eq(itemId, -1);

    // Set preferences.
    Services.prefs.setIntPref(PREF_SMART_BOOKMARKS_VERSION, 999);
    Services.prefs.setBoolPref(PREF_AUTO_EXPORT_HTML, true);
    Services.prefs.setBoolPref(PREF_IMPORT_BOOKMARKS_HTML, true);

    waitForImportAndSmartBookmarks(function () {
      // Check bookmarks.html has been imported, but smart bookmarks have not
      // been created.
      itemId =
        PlacesUtils.bookmarks.getIdForItemAt(PlacesUtils.toolbarFolderId, 0);
      do_check_eq(PlacesUtils.bookmarks.getItemTitle(itemId), "example");
      do_check_false(Services.prefs.getBoolPref(PREF_IMPORT_BOOKMARKS_HTML));
      // Check preferences have been reverted.
      Services.prefs.setBoolPref(PREF_AUTO_EXPORT_HTML, false);

      run_next_test();
    });
    // Force nsBrowserGlue::_initPlaces()
    do_log_info("Simulate Places init");
    bg.QueryInterface(Ci.nsIObserver).observe(null,
                                              TOPIC_BROWSERGLUE_TEST,
                                              TOPICDATA_FORCE_PLACES_INIT);
  },

  function test_import_autoExport_oldSmartBookmarks()
  {
    do_log_info("Import from bookmarks.html, and create smart bookmarks if \
                 autoExportHTML is true and they are not at latest version.");

    remove_all_bookmarks();
    // Sanity check: we should not have any bookmark on the toolbar.
    let itemId =
      PlacesUtils.bookmarks.getIdForItemAt(PlacesUtils.toolbarFolderId, 0);
    do_check_eq(itemId, -1);

    // Set preferences.
    Services.prefs.setIntPref(PREF_SMART_BOOKMARKS_VERSION, 0);
    Services.prefs.setBoolPref(PREF_AUTO_EXPORT_HTML, true);
    Services.prefs.setBoolPref(PREF_IMPORT_BOOKMARKS_HTML, true);

    waitForImportAndSmartBookmarks(function () {
      // Check bookmarks.html has been imported, but smart bookmarks have not
      // been created.
      itemId =
        PlacesUtils.bookmarks.getIdForItemAt(PlacesUtils.toolbarFolderId,
                                             SMART_BOOKMARKS_ON_TOOLBAR);
      do_check_eq(PlacesUtils.bookmarks.getItemTitle(itemId), "example");
      do_check_false(Services.prefs.getBoolPref(PREF_IMPORT_BOOKMARKS_HTML));
      // Check preferences have been reverted.
      Services.prefs.setBoolPref(PREF_AUTO_EXPORT_HTML, false);

      run_next_test();
    });
    // Force nsBrowserGlue::_initPlaces()
    do_log_info("Simulate Places init");
    bg.QueryInterface(Ci.nsIObserver).observe(null,
                                              TOPIC_BROWSERGLUE_TEST,
                                              TOPICDATA_FORCE_PLACES_INIT);
  },

  function test_restore()
  {
    do_log_info("restore from default bookmarks.html if \
                 restore_default_bookmarks is true.");

    remove_all_bookmarks();
    // Sanity check: we should not have any bookmark on the toolbar.
    let itemId =
      PlacesUtils.bookmarks.getIdForItemAt(PlacesUtils.toolbarFolderId, 0);
    do_check_eq(itemId, -1);

    // Set preferences.
    Services.prefs.setBoolPref(PREF_RESTORE_DEFAULT_BOOKMARKS, true);

    waitForImportAndSmartBookmarks(function () {
      // Check bookmarks.html has been restored.
      itemId =
        PlacesUtils.bookmarks.getIdForItemAt(PlacesUtils.toolbarFolderId,
                                             SMART_BOOKMARKS_ON_TOOLBAR);
      do_check_true(itemId > 0);
      // Check preferences have been reverted.
      do_check_false(Services.prefs.getBoolPref(PREF_RESTORE_DEFAULT_BOOKMARKS));

      run_next_test();
    });
    // Force nsBrowserGlue::_initPlaces()
    do_log_info("Simulate Places init");
    bg.QueryInterface(Ci.nsIObserver).observe(null,
                                              TOPIC_BROWSERGLUE_TEST,
                                              TOPICDATA_FORCE_PLACES_INIT);

  },

  function test_restore_import()
  {
    do_log_info("setting both importBookmarksHTML and \
                 restore_default_bookmarks should restore defaults.");

    remove_all_bookmarks();
    // Sanity check: we should not have any bookmark on the toolbar.
    let itemId =
      PlacesUtils.bookmarks.getIdForItemAt(PlacesUtils.toolbarFolderId, 0);
    do_check_eq(itemId, -1);

    // Set preferences.
    Services.prefs.setBoolPref(PREF_IMPORT_BOOKMARKS_HTML, true);
    Services.prefs.setBoolPref(PREF_RESTORE_DEFAULT_BOOKMARKS, true);

    waitForImportAndSmartBookmarks(function () {
      // Check bookmarks.html has been restored.
      itemId =
        PlacesUtils.bookmarks.getIdForItemAt(PlacesUtils.toolbarFolderId,
                                             SMART_BOOKMARKS_ON_TOOLBAR);
      do_check_true(itemId > 0);
      // Check preferences have been reverted.
      do_check_false(Services.prefs.getBoolPref(PREF_RESTORE_DEFAULT_BOOKMARKS));
      do_check_false(Services.prefs.getBoolPref(PREF_IMPORT_BOOKMARKS_HTML));

      run_next_test();
    });
    // Force nsBrowserGlue::_initPlaces()
    do_log_info("Simulate Places init");
    bg.QueryInterface(Ci.nsIObserver).observe(null,
                                              TOPIC_BROWSERGLUE_TEST,
                                              TOPICDATA_FORCE_PLACES_INIT);
  }

].forEach(add_test);

do_register_cleanup(function () {
  remove_all_bookmarks();
  remove_bookmarks_html();
  remove_all_JSON_backups();
});

function run_test()
{
  // Create our bookmarks.html from bookmarks.glue.html.
  create_bookmarks_html("bookmarks.glue.html");
  remove_all_JSON_backups();
  // Create our JSON backup from bookmarks.glue.json.
  create_JSON_backup("bookmarks.glue.json");

  run_next_test();
}
