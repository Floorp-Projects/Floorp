/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */


const SMART_BOOKMARKS_PREF = "browser.places.smartBookmarksVersion";

var gluesvc = Cc["@mozilla.org/browser/browserglue;1"].
                getService(Ci.nsIObserver);
// Avoid default bookmarks import.
gluesvc.observe(null, "initial-migration-will-import-default-bookmarks", "");

function run_test() {
  run_next_test();
}

add_task(function* smart_bookmarks_disabled() {
  Services.prefs.setIntPref("browser.places.smartBookmarksVersion", -1);
  yield rebuildSmartBookmarks();

  let smartBookmarkItemIds =
    PlacesUtils.annotations.getItemsWithAnnotation(SMART_BOOKMARKS_ANNO);
  Assert.equal(smartBookmarkItemIds.length, 0);

  do_print("check that pref has not been bumped up");
  Assert.equal(Services.prefs.getIntPref("browser.places.smartBookmarksVersion"), -1);
});

add_task(function* create_smart_bookmarks() {
  Services.prefs.setIntPref("browser.places.smartBookmarksVersion", 0);
  yield rebuildSmartBookmarks();

  let smartBookmarkItemIds =
    PlacesUtils.annotations.getItemsWithAnnotation(SMART_BOOKMARKS_ANNO);
  Assert.notEqual(smartBookmarkItemIds.length, 0);

  do_print("check that pref has been bumped up");
  Assert.ok(Services.prefs.getIntPref("browser.places.smartBookmarksVersion") > 0);
});

add_task(function* remove_smart_bookmark_and_restore() {
  let smartBookmarkItemIds =
    PlacesUtils.annotations.getItemsWithAnnotation(SMART_BOOKMARKS_ANNO);
  let smartBookmarksCount = smartBookmarkItemIds.length;
  do_print("remove one smart bookmark and restore");

  let guid = yield PlacesUtils.promiseItemGuid(smartBookmarkItemIds[0]);
  yield PlacesUtils.bookmarks.remove(guid);
  Services.prefs.setIntPref("browser.places.smartBookmarksVersion", 0);

  yield rebuildSmartBookmarks();
  smartBookmarkItemIds =
    PlacesUtils.annotations.getItemsWithAnnotation(SMART_BOOKMARKS_ANNO);
  Assert.equal(smartBookmarkItemIds.length, smartBookmarksCount);

  do_print("check that pref has been bumped up");
  Assert.ok(Services.prefs.getIntPref("browser.places.smartBookmarksVersion") > 0);
});

add_task(function* move_smart_bookmark_rename_and_restore() {
  let smartBookmarkItemIds =
    PlacesUtils.annotations.getItemsWithAnnotation(SMART_BOOKMARKS_ANNO);
  let smartBookmarksCount = smartBookmarkItemIds.length;
  do_print("smart bookmark should be restored in place");

  let guid = yield PlacesUtils.promiseItemGuid(smartBookmarkItemIds[0]);
  let bm = yield PlacesUtils.bookmarks.fetch(guid);
  let oldTitle = bm.title;

  // create a subfolder and move inside it
  let subfolder = yield PlacesUtils.bookmarks.insert({
    parentGuid: bm.parentGuid,
    title: "test",
    index: PlacesUtils.bookmarks.DEFAULT_INDEX,
    type: PlacesUtils.bookmarks.TYPE_FOLDER
  });

  // change title and move into new subfolder
  yield PlacesUtils.bookmarks.update({
    guid: guid,
    parentGuid: subfolder.guid,
    index: PlacesUtils.bookmarks.DEFAULT_INDEX,
    title: "new title"
  });

  // restore
  Services.prefs.setIntPref("browser.places.smartBookmarksVersion", 0);
  yield rebuildSmartBookmarks();

  smartBookmarkItemIds =
    PlacesUtils.annotations.getItemsWithAnnotation(SMART_BOOKMARKS_ANNO);
  Assert.equal(smartBookmarkItemIds.length, smartBookmarksCount);

  guid = yield PlacesUtils.promiseItemGuid(smartBookmarkItemIds[0]);
  bm = yield PlacesUtils.bookmarks.fetch(guid);
  Assert.equal(bm.parentGuid, subfolder.guid);
  Assert.equal(bm.title, oldTitle);

  do_print("check that pref has been bumped up");
  Assert.ok(Services.prefs.getIntPref("browser.places.smartBookmarksVersion") > 0);
});
