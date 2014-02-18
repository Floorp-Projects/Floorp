/* -*- Mode: Java; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */


const SMART_BOOKMARKS_ANNO = "Places/SmartBookmark";
const SMART_BOOKMARKS_PREF = "browser.places.smartBookmarksVersion";

let gluesvc = Cc["@mozilla.org/browser/browserglue;1"].
                getService(Ci.nsIBrowserGlue).
                QueryInterface(Ci.nsIObserver);
// Avoid default bookmarks import.
gluesvc.observe(null, "initial-migration-will-import-default-bookmarks", "");

function run_test() {
  run_next_test();
}

add_task(function smart_bookmarks_disabled() {
  Services.prefs.setIntPref("browser.places.smartBookmarksVersion", -1);
  gluesvc.ensurePlacesDefaultQueriesInitialized();
  let smartBookmarkItemIds =
    PlacesUtils.annotations.getItemsWithAnnotation(SMART_BOOKMARKS_ANNO);
  do_check_eq(smartBookmarkItemIds.length, 0);
  do_log_info("check that pref has not been bumped up");
  do_check_eq(Services.prefs.getIntPref("browser.places.smartBookmarksVersion"), -1);
});

add_task(function create_smart_bookmarks() {
  Services.prefs.setIntPref("browser.places.smartBookmarksVersion", 0);
  gluesvc.ensurePlacesDefaultQueriesInitialized();
  let smartBookmarkItemIds =
    PlacesUtils.annotations.getItemsWithAnnotation(SMART_BOOKMARKS_ANNO);
  do_check_neq(smartBookmarkItemIds.length, 0);
  do_log_info("check that pref has been bumped up");
  do_check_true(Services.prefs.getIntPref("browser.places.smartBookmarksVersion") > 0);
});

add_task(function remove_smart_bookmark_and_restore() {
  let smartBookmarkItemIds =
    PlacesUtils.annotations.getItemsWithAnnotation(SMART_BOOKMARKS_ANNO);
  let smartBookmarksCount = smartBookmarkItemIds.length;
  do_log_info("remove one smart bookmark and restore");
  PlacesUtils.bookmarks.removeItem(smartBookmarkItemIds[0]);
  Services.prefs.setIntPref("browser.places.smartBookmarksVersion", 0);
  gluesvc.ensurePlacesDefaultQueriesInitialized();
  let smartBookmarkItemIds =
    PlacesUtils.annotations.getItemsWithAnnotation(SMART_BOOKMARKS_ANNO);
  do_check_eq(smartBookmarkItemIds.length, smartBookmarksCount);
  do_log_info("check that pref has been bumped up");
  do_check_true(Services.prefs.getIntPref("browser.places.smartBookmarksVersion") > 0);
});

add_task(function move_smart_bookmark_rename_and_restore() {
  let smartBookmarkItemIds =
    PlacesUtils.annotations.getItemsWithAnnotation(SMART_BOOKMARKS_ANNO);
  let smartBookmarksCount = smartBookmarkItemIds.length;
  do_log_info("smart bookmark should be restored in place");
  let parent = PlacesUtils.bookmarks.getFolderIdForItem(smartBookmarkItemIds[0]);
  let oldTitle = PlacesUtils.bookmarks.getItemTitle(smartBookmarkItemIds[0]);
  // create a subfolder and move inside it
  let newParent =
    PlacesUtils.bookmarks.createFolder(parent, "test",
                                       PlacesUtils.bookmarks.DEFAULT_INDEX);
  PlacesUtils.bookmarks.moveItem(smartBookmarkItemIds[0], newParent,
                                 PlacesUtils.bookmarks.DEFAULT_INDEX);
  // change title
  PlacesUtils.bookmarks.setItemTitle(smartBookmarkItemIds[0], "new title");
  // restore
  Services.prefs.setIntPref("browser.places.smartBookmarksVersion", 0);
  gluesvc.ensurePlacesDefaultQueriesInitialized();
  smartBookmarkItemIds =
    PlacesUtils.annotations.getItemsWithAnnotation(SMART_BOOKMARKS_ANNO);
  do_check_eq(smartBookmarkItemIds.length, smartBookmarksCount);
  do_check_eq(PlacesUtils.bookmarks.getFolderIdForItem(smartBookmarkItemIds[0]), newParent);
  do_check_eq(PlacesUtils.bookmarks.getItemTitle(smartBookmarkItemIds[0]), oldTitle);
  do_log_info("check that pref has been bumped up");
  do_check_true(Services.prefs.getIntPref("browser.places.smartBookmarksVersion") > 0);
});
