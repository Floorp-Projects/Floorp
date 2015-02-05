/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/**
 * Tests that nsBrowserGlue correctly restores default bookmarks if database is
 * corrupt, nor a JSON backup nor bookmarks.html are available.
 */

function run_test() {
  // Remove bookmarks.html from profile.
  remove_bookmarks_html();

  // Remove JSON backup from profile.
  remove_all_JSON_backups();

  run_next_test();
}

add_task(function* () {
  // Create a corrupt database.
  yield createCorruptDB();

  // Initialize nsBrowserGlue before Places.
  Cc["@mozilla.org/browser/browserglue;1"].getService(Ci.nsIBrowserGlue);

  // Check the database was corrupt.
  // nsBrowserGlue uses databaseStatus to manage initialization.
  Assert.equal(PlacesUtils.history.databaseStatus,
               PlacesUtils.history.DATABASE_STATUS_CORRUPT);

  // The test will continue once import has finished and smart bookmarks
  // have been created.
  yield promiseEndUpdateBatch();

  let bm = yield PlacesUtils.bookmarks.fetch({
    parentGuid: PlacesUtils.bookmarks.toolbarGuid,
    index: 0
  });
  yield checkItemHasAnnotation(bm.guid, SMART_BOOKMARKS_ANNO);

  // Check that default bookmarks have been restored.
  bm = yield PlacesUtils.bookmarks.fetch({
    parentGuid: PlacesUtils.bookmarks.toolbarGuid,
    index: SMART_BOOKMARKS_ON_TOOLBAR
  });
  do_check_eq(bm.title, "Getting Started");
});
