/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/**
 * Tests that nsBrowserGlue correctly restores bookmarks from a JSON backup if
 * database is corrupt and one backup is available.
 */

function run_test() {
  // Create our bookmarks.html from bookmarks.glue.html.
  create_bookmarks_html("bookmarks.glue.html");

  remove_all_JSON_backups();

  // Create our JSON backup from bookmarks.glue.json.
  create_JSON_backup("bookmarks.glue.json");

  run_next_test();
}

registerCleanupFunction(function() {
  remove_bookmarks_html();
  remove_all_JSON_backups();
  return PlacesUtils.bookmarks.eraseEverything();
});

add_task(async function test_main() {
  // Create a corrupt database.
  await createCorruptDB();

  // Initialize nsBrowserGlue before Places.
  Cc["@mozilla.org/browser/browserglue;1"].getService(Ci.nsISupports);

  // Check the database was corrupt.
  // nsBrowserGlue uses databaseStatus to manage initialization.
  Assert.equal(
    PlacesUtils.history.databaseStatus,
    PlacesUtils.history.DATABASE_STATUS_CORRUPT
  );

  // The test will continue once restore has finished.
  await promiseTopicObserved("places-browser-init-complete");

  // Check that JSON backup has been restored.
  let bm = await PlacesUtils.bookmarks.fetch({
    parentGuid: PlacesUtils.bookmarks.toolbarGuid,
    index: 0,
  });
  Assert.equal(bm.title, "examplejson");
});
