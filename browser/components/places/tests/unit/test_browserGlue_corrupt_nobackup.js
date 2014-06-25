/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/**
 * Tests that nsBrowserGlue correctly imports from bookmarks.html if database
 * is corrupt but a JSON backup is not available.
 */

Components.utils.import("resource://gre/modules/XPCOMUtils.jsm");

XPCOMUtils.defineLazyServiceGetter(this, "bs",
                                   "@mozilla.org/browser/nav-bookmarks-service;1",
                                   "nsINavBookmarksService");
XPCOMUtils.defineLazyServiceGetter(this, "anno",
                                   "@mozilla.org/browser/annotation-service;1",
                                   "nsIAnnotationService");

let bookmarksObserver = {
  onBeginUpdateBatch: function() {},
  onEndUpdateBatch: function() {
    let itemId = bs.getIdForItemAt(bs.toolbarFolder, 0);
    do_check_neq(itemId, -1);
    if (anno.itemHasAnnotation(itemId, "Places/SmartBookmark"))
      continue_test();
  },
  onItemAdded: function() {},
  onItemRemoved: function(id, folder, index, itemType) {},
  onItemChanged: function() {},
  onItemVisited: function(id, visitID, time) {},
  onItemMoved: function() {},
  QueryInterface: XPCOMUtils.generateQI([Ci.nsINavBookmarkObserver])
};

function run_test() {
  do_test_pending();

  // Create bookmarks.html in the profile.
  create_bookmarks_html("bookmarks.glue.html");
  // Remove JSON backup from profile.
  remove_all_JSON_backups();

  // Remove current database file.
  let db = gProfD.clone();
  db.append("places.sqlite");
  if (db.exists()) {
    db.remove(false);
    do_check_false(db.exists());
  }
  // Create a corrupt database.
  let corruptDB = gTestDir.clone();
  corruptDB.append("corruptDB.sqlite");
  corruptDB.copyTo(gProfD, "places.sqlite");
  do_check_true(db.exists());

  // Initialize nsBrowserGlue before Places.
  Cc["@mozilla.org/browser/browserglue;1"].getService(Ci.nsIBrowserGlue);

  // Initialize Places through the History Service.
  let hs = Cc["@mozilla.org/browser/nav-history-service;1"].
           getService(Ci.nsINavHistoryService);
  // Check the database was corrupt.
  // nsBrowserGlue uses databaseStatus to manage initialization.
  do_check_eq(hs.databaseStatus, hs.DATABASE_STATUS_CORRUPT);

  // The test will continue once import has finished and smart bookmarks
  // have been created.
  bs.addObserver(bookmarksObserver, false);
}

function continue_test() {
  // Check that bookmarks html has been restored.
  let itemId = bs.getIdForItemAt(bs.toolbarFolder, SMART_BOOKMARKS_ON_TOOLBAR);
  do_check_eq(bs.getItemTitle(itemId), "example");

  remove_bookmarks_html();

  do_test_finished();
}
