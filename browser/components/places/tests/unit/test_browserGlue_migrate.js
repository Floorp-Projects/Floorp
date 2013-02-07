/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

/**
 * Tests that nsBrowserGlue does not overwrite bookmarks imported from the
 * migrators.  They usually run before nsBrowserGlue, so if we find any
 * bookmark on init, we should not try to import.
 */

const PREF_SMART_BOOKMARKS_VERSION = "browser.places.smartBookmarksVersion";

function run_test() {
  do_test_pending();

  // Create our bookmarks.html copying bookmarks.glue.html to the profile
  // folder.  It should be ignored.
  create_bookmarks_html("bookmarks.glue.html");

  // Remove current database file.
  let db = gProfD.clone();
  db.append("places.sqlite");
  if (db.exists()) {
    db.remove(false);
    do_check_false(db.exists());
  }

  // Initialize Places through the History Service and check that a new
  // database has been created.
  do_check_eq(PlacesUtils.history.databaseStatus,
              PlacesUtils.history.DATABASE_STATUS_CREATE);

  // A migrator would run before nsBrowserGlue Places initialization, so mimic
  // that behavior adding a bookmark and notifying the migration.
  let bg = Cc["@mozilla.org/browser/browserglue;1"].
           getService(Ci.nsIObserver);

  bg.observe(null, "initial-migration-will-import-default-bookmarks", null);

  PlacesUtils.bookmarks.insertBookmark(PlacesUtils.bookmarks.bookmarksMenuFolder, uri("http://mozilla.org/"),
                    PlacesUtils.bookmarks.DEFAULT_INDEX, "migrated");

  let bookmarksObserver = {
    onBeginUpdateBatch: function() {},
    onEndUpdateBatch: function() {
      // Check if the currently finished batch created the smart bookmarks.
      let itemId =
        PlacesUtils.bookmarks.getIdForItemAt(PlacesUtils.toolbarFolderId, 0);
      do_check_neq(itemId, -1);
      if (PlacesUtils.annotations
                     .itemHasAnnotation(itemId, "Places/SmartBookmark")) {
        do_execute_soon(onSmartBookmarksCreation);
      }
    },
    onItemAdded: function() {},
    onItemRemoved: function(id, folder, index, itemType) {},
    onItemChanged: function() {},
    onItemVisited: function(id, visitID, time) {},
    onItemMoved: function() {},
    QueryInterface: XPCOMUtils.generateQI([Ci.nsINavBookmarkObserver])
  };
  // The test will continue once import has finished and smart bookmarks
  // have been created.
  PlacesUtils.bookmarks.addObserver(bookmarksObserver, false);

  bg.observe(null, "initial-migration-did-import-default-bookmarks", null);
}

function onSmartBookmarksCreation() {
  // Check the created bookmarks still exist.
  let itemId =
    PlacesUtils.bookmarks.getIdForItemAt(PlacesUtils.bookmarksMenuFolderId,
                                         SMART_BOOKMARKS_ON_MENU);
  do_check_eq(PlacesUtils.bookmarks.getItemTitle(itemId), "migrated");

  // Check that we have not imported any new bookmark.
  itemId =
    PlacesUtils.bookmarks.getIdForItemAt(PlacesUtils.bookmarksMenuFolderId,
                                         SMART_BOOKMARKS_ON_MENU + 1)
  do_check_eq(itemId, -1);
  itemId =
    PlacesUtils.bookmarks.getIdForItemAt(PlacesUtils.toolbarFolderId,
                                         SMART_BOOKMARKS_ON_MENU)
  do_check_eq(itemId, -1);

  remove_bookmarks_html();

  do_test_finished();
}
