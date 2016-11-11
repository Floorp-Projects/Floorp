/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/**
 * Tests that nsBrowserGlue correctly restores bookmarks from a JSON backup if
 * database has been created and one backup is available.
 */

function run_test() {
  // Create our bookmarks.html from bookmarks.glue.html.
  create_bookmarks_html("bookmarks.glue.html");

  remove_all_JSON_backups();

  // Create our JSON backup from bookmarks.glue.json.
  create_JSON_backup("bookmarks.glue.json");

  // Remove current database file.
  clearDB();

  run_next_test();
}

do_register_cleanup(function() {
  remove_bookmarks_html();
  remove_all_JSON_backups();
  return PlacesUtils.bookmarks.eraseEverything();
});

add_task(function* test_main() {
  // Initialize nsBrowserGlue before Places.
  Cc["@mozilla.org/browser/browserglue;1"].getService(Ci.nsISupports);

  // Initialize Places through the History Service.
  let hs = Cc["@mozilla.org/browser/nav-history-service;1"].
           getService(Ci.nsINavHistoryService);

  // Check a new database has been created.
  // nsBrowserGlue uses databaseStatus to manage initialization.
  Assert.equal(hs.databaseStatus, hs.DATABASE_STATUS_CREATE);

  // The test will continue once restore has finished and smart bookmarks
  // have been created.
  yield promiseTopicObserved("places-browser-init-complete");

  let bm = yield PlacesUtils.bookmarks.fetch({
    parentGuid: PlacesUtils.bookmarks.toolbarGuid,
    index: 0
  });
  yield checkItemHasAnnotation(bm.guid, SMART_BOOKMARKS_ANNO);

  // Check that JSON backup has been restored.
  // Notice restore from JSON notification is fired before smart bookmarks creation.
  bm = yield PlacesUtils.bookmarks.fetch({
    parentGuid: PlacesUtils.bookmarks.toolbarGuid,
    index: SMART_BOOKMARKS_ON_TOOLBAR
  });
  Assert.equal(bm.title, "examplejson");
});
