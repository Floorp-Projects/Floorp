/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

/**
 * Tests that nsBrowserGlue does not overwrite bookmarks imported from the
 * migrators.  They usually run before nsBrowserGlue, so if we find any
 * bookmark on init, we should not try to import.
 */

const PREF_SMART_BOOKMARKS_VERSION = "browser.places.smartBookmarksVersion";

function run_test() {
  // Create our bookmarks.html from bookmarks.glue.html.
  create_bookmarks_html("bookmarks.glue.html");

  // Remove current database file.
  clearDB();

  run_next_test();
}

registerCleanupFunction(remove_bookmarks_html);

add_task(async function test_migrate_bookmarks() {
  // Initialize Places through the History Service and check that a new
  // database has been created.
  Assert.equal(PlacesUtils.history.databaseStatus,
               PlacesUtils.history.DATABASE_STATUS_CREATE);

  // A migrator would run before nsBrowserGlue Places initialization, so mimic
  // that behavior adding a bookmark and notifying the migration.
  let bg = Cc["@mozilla.org/browser/browserglue;1"].getService(Ci.nsIObserver);
  bg.observe(null, "initial-migration-will-import-default-bookmarks", null);

  await PlacesUtils.bookmarks.insert({
    parentGuid: PlacesUtils.bookmarks.menuGuid,
    index: PlacesUtils.bookmarks.DEFAULT_INDEX,
    type: PlacesUtils.bookmarks.TYPE_BOOKMARK,
    url: "http://mozilla.org/",
    title: "migrated"
  });

  let promise = promiseTopicObserved("places-browser-init-complete");
  bg.observe(null, "initial-migration-did-import-default-bookmarks", null);
  await promise;

  let bm = await PlacesUtils.bookmarks.fetch({
    parentGuid: PlacesUtils.bookmarks.toolbarGuid,
    index: 0
  });
  await checkItemHasAnnotation(bm.guid, SMART_BOOKMARKS_ANNO);

  // Check the created bookmark still exists.
  bm = await PlacesUtils.bookmarks.fetch({
    parentGuid: PlacesUtils.bookmarks.menuGuid,
    index: SMART_BOOKMARKS_ON_MENU
  });
  Assert.equal(bm.title, "migrated");

  // Check that we have not imported any new bookmark.
  Assert.ok(!(await PlacesUtils.bookmarks.fetch({
    parentGuid: PlacesUtils.bookmarks.menuGuid,
    index: SMART_BOOKMARKS_ON_MENU + 1
  })));

  Assert.ok(!(await PlacesUtils.bookmarks.fetch({
    parentGuid: PlacesUtils.bookmarks.toolbarGuid,
    index: SMART_BOOKMARKS_ON_MENU
  })));
});
