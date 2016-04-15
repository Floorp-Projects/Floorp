/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/**
 * Tests that nsBrowserGlue is correctly interpreting the preferences settable
 * by the user or by other components.
 */

const PREF_SMART_BOOKMARKS_VERSION = "browser.places.smartBookmarksVersion";
const PREF_AUTO_EXPORT_HTML = "browser.bookmarks.autoExportHTML";
const PREF_IMPORT_BOOKMARKS_HTML = "browser.places.importBookmarksHTML";
const PREF_RESTORE_DEFAULT_BOOKMARKS = "browser.bookmarks.restore_default_bookmarks";

function run_test() {
  remove_bookmarks_html();
  remove_all_JSON_backups();
  run_next_test();
}

do_register_cleanup(() => PlacesUtils.bookmarks.eraseEverything());

function countFolderChildren(aFolderItemId) {
  let rootNode = PlacesUtils.getFolderContents(aFolderItemId).root;
  let cc = rootNode.childCount;
  // Dump contents.
  for (let i = 0; i < cc ; i++) {
    let node = rootNode.getChild(i);
    let title = PlacesUtils.nodeIsSeparator(node) ? "---" : node.title;
    print("Found child(" + i + "): " + title);
  }
  rootNode.containerOpen = false;
  return cc;
}

add_task(function* setup() {
  // Initialize browserGlue, but remove it's listener to places-init-complete.
  let bg = Cc["@mozilla.org/browser/browserglue;1"].getService(Ci.nsIObserver);

  // Initialize Places.
  PlacesUtils.history;

  // Wait for Places init notification.
  yield promiseTopicObserved("places-browser-init-complete");

  // Ensure preferences status.
  Assert.ok(!Services.prefs.getBoolPref(PREF_AUTO_EXPORT_HTML));
  Assert.ok(!Services.prefs.getBoolPref(PREF_RESTORE_DEFAULT_BOOKMARKS));
  Assert.throws(() => Services.prefs.getBoolPref(PREF_IMPORT_BOOKMARKS_HTML));
});

add_task(function* test_version_0() {
  do_print("All smart bookmarks are created if smart bookmarks version is 0.");

  // Sanity check: we should have default bookmark.
  Assert.ok(yield PlacesUtils.bookmarks.fetch({
    parentGuid: PlacesUtils.bookmarks.toolbarGuid,
    index: 0
  }));

  Assert.ok(yield PlacesUtils.bookmarks.fetch({
    parentGuid: PlacesUtils.bookmarks.menuGuid,
    index: 0
  }));

  // Set preferences.
  Services.prefs.setIntPref(PREF_SMART_BOOKMARKS_VERSION, 0);

  yield rebuildSmartBookmarks();

  // Count items.
  Assert.equal(countFolderChildren(PlacesUtils.toolbarFolderId),
               SMART_BOOKMARKS_ON_TOOLBAR + DEFAULT_BOOKMARKS_ON_TOOLBAR);
  Assert.equal(countFolderChildren(PlacesUtils.bookmarksMenuFolderId),
               SMART_BOOKMARKS_ON_MENU + DEFAULT_BOOKMARKS_ON_MENU);

  // Check version has been updated.
  Assert.equal(Services.prefs.getIntPref(PREF_SMART_BOOKMARKS_VERSION),
               SMART_BOOKMARKS_VERSION);
});

add_task(function* test_version_change() {
  do_print("An existing smart bookmark is replaced when version changes.");

  // Sanity check: we have a smart bookmark on the toolbar.
  let bm = yield PlacesUtils.bookmarks.fetch({
    parentGuid: PlacesUtils.bookmarks.toolbarGuid,
    index: 0
  });
  yield checkItemHasAnnotation(bm.guid, SMART_BOOKMARKS_ANNO);

  // Change its title.
  yield PlacesUtils.bookmarks.update({guid: bm.guid, title: "new title"});
  bm = yield PlacesUtils.bookmarks.fetch({guid: bm.guid});
  Assert.equal(bm.title, "new title");

  // Sanity check items.
  Assert.equal(countFolderChildren(PlacesUtils.toolbarFolderId),
               SMART_BOOKMARKS_ON_TOOLBAR + DEFAULT_BOOKMARKS_ON_TOOLBAR);
  Assert.equal(countFolderChildren(PlacesUtils.bookmarksMenuFolderId),
               SMART_BOOKMARKS_ON_MENU + DEFAULT_BOOKMARKS_ON_MENU);

  // Set preferences.
  Services.prefs.setIntPref(PREF_SMART_BOOKMARKS_VERSION, 1);

  yield rebuildSmartBookmarks();

  // Count items.
  Assert.equal(countFolderChildren(PlacesUtils.toolbarFolderId),
               SMART_BOOKMARKS_ON_TOOLBAR + DEFAULT_BOOKMARKS_ON_TOOLBAR);
  Assert.equal(countFolderChildren(PlacesUtils.bookmarksMenuFolderId),
               SMART_BOOKMARKS_ON_MENU + DEFAULT_BOOKMARKS_ON_MENU);

  // Check smart bookmark has been replaced, itemId has changed.
  bm = yield PlacesUtils.bookmarks.fetch({
    parentGuid: PlacesUtils.bookmarks.toolbarGuid,
    index: 0
  });
  yield checkItemHasAnnotation(bm.guid, SMART_BOOKMARKS_ANNO);
  Assert.notEqual(bm.title, "new title");

  // Check version has been updated.
  Assert.equal(Services.prefs.getIntPref(PREF_SMART_BOOKMARKS_VERSION),
               SMART_BOOKMARKS_VERSION);
});

add_task(function* test_version_change_pos() {
  do_print("bookmarks position is retained when version changes.");

  // Sanity check items.
  Assert.equal(countFolderChildren(PlacesUtils.toolbarFolderId),
               SMART_BOOKMARKS_ON_TOOLBAR + DEFAULT_BOOKMARKS_ON_TOOLBAR);
  Assert.equal(countFolderChildren(PlacesUtils.bookmarksMenuFolderId),
               SMART_BOOKMARKS_ON_MENU + DEFAULT_BOOKMARKS_ON_MENU);

  let bm = yield PlacesUtils.bookmarks.fetch({
    parentGuid: PlacesUtils.bookmarks.menuGuid,
    index: 0
  });
  yield checkItemHasAnnotation(bm.guid, SMART_BOOKMARKS_ANNO);
  let firstItemTitle = bm.title;

  // Set preferences.
  Services.prefs.setIntPref(PREF_SMART_BOOKMARKS_VERSION, 1);

  yield rebuildSmartBookmarks();

  // Count items.
  Assert.equal(countFolderChildren(PlacesUtils.toolbarFolderId),
               SMART_BOOKMARKS_ON_TOOLBAR + DEFAULT_BOOKMARKS_ON_TOOLBAR);
  Assert.equal(countFolderChildren(PlacesUtils.bookmarksMenuFolderId),
               SMART_BOOKMARKS_ON_MENU + DEFAULT_BOOKMARKS_ON_MENU);

  // Check smart bookmarks are still in correct position.
  bm = yield PlacesUtils.bookmarks.fetch({
    parentGuid: PlacesUtils.bookmarks.menuGuid,
    index: 0
  });
  yield checkItemHasAnnotation(bm.guid, SMART_BOOKMARKS_ANNO);
  Assert.equal(bm.title, firstItemTitle);

  // Check version has been updated.
  Assert.equal(Services.prefs.getIntPref(PREF_SMART_BOOKMARKS_VERSION),
               SMART_BOOKMARKS_VERSION);
});

add_task(function* test_version_change_pos_moved() {
  do_print("moved bookmarks position is retained when version changes.");

  // Sanity check items.
  Assert.equal(countFolderChildren(PlacesUtils.toolbarFolderId),
               SMART_BOOKMARKS_ON_TOOLBAR + DEFAULT_BOOKMARKS_ON_TOOLBAR);
  Assert.equal(countFolderChildren(PlacesUtils.bookmarksMenuFolderId),
               SMART_BOOKMARKS_ON_MENU + DEFAULT_BOOKMARKS_ON_MENU);

  let bm1 = yield PlacesUtils.bookmarks.fetch({
    parentGuid: PlacesUtils.bookmarks.menuGuid,
    index: 0
  });
  yield checkItemHasAnnotation(bm1.guid, SMART_BOOKMARKS_ANNO);
  let firstItemTitle = bm1.title;

  // Move the first smart bookmark to the end of the menu.
  yield PlacesUtils.bookmarks.update({
    parentGuid: PlacesUtils.bookmarks.menuGuid,
    guid: bm1.guid,
    index: PlacesUtils.bookmarks.DEFAULT_INDEX
  });

  let bm = yield PlacesUtils.bookmarks.fetch({
    parentGuid: PlacesUtils.bookmarks.menuGuid,
    index: PlacesUtils.bookmarks.DEFAULT_INDEX
  });
  Assert.equal(bm.guid, bm1.guid);

  // Set preferences.
  Services.prefs.setIntPref(PREF_SMART_BOOKMARKS_VERSION, 1);

  yield rebuildSmartBookmarks();

  // Count items.
  Assert.equal(countFolderChildren(PlacesUtils.toolbarFolderId),
               SMART_BOOKMARKS_ON_TOOLBAR + DEFAULT_BOOKMARKS_ON_TOOLBAR);
  Assert.equal(countFolderChildren(PlacesUtils.bookmarksMenuFolderId),
               SMART_BOOKMARKS_ON_MENU + DEFAULT_BOOKMARKS_ON_MENU);

  bm1 = yield PlacesUtils.bookmarks.fetch({
    parentGuid: PlacesUtils.bookmarks.menuGuid,
    index: PlacesUtils.bookmarks.DEFAULT_INDEX
  });
  yield checkItemHasAnnotation(bm1.guid, SMART_BOOKMARKS_ANNO);
  Assert.equal(bm1.title, firstItemTitle);

  // Move back the smart bookmark to the original position.
  yield PlacesUtils.bookmarks.update({
    parentGuid: PlacesUtils.bookmarks.menuGuid,
    guid: bm1.guid,
    index: 1
  });

  // Check version has been updated.
  Assert.equal(Services.prefs.getIntPref(PREF_SMART_BOOKMARKS_VERSION),
              SMART_BOOKMARKS_VERSION);
});

add_task(function* test_recreation() {
  do_print("An explicitly removed smart bookmark should not be recreated.");

  // Remove toolbar's smart bookmarks
  let bm = yield PlacesUtils.bookmarks.fetch({
    parentGuid: PlacesUtils.bookmarks.toolbarGuid,
    index: 0
  });
  yield PlacesUtils.bookmarks.remove(bm.guid);

  // Sanity check items.
  Assert.equal(countFolderChildren(PlacesUtils.toolbarFolderId),
               DEFAULT_BOOKMARKS_ON_TOOLBAR);
  Assert.equal(countFolderChildren(PlacesUtils.bookmarksMenuFolderId),
               SMART_BOOKMARKS_ON_MENU + DEFAULT_BOOKMARKS_ON_MENU);

  // Set preferences.
  Services.prefs.setIntPref(PREF_SMART_BOOKMARKS_VERSION, 1);

  yield rebuildSmartBookmarks();

  // Count items.
  // We should not have recreated the smart bookmark on toolbar.
  Assert.equal(countFolderChildren(PlacesUtils.toolbarFolderId),
               DEFAULT_BOOKMARKS_ON_TOOLBAR);
  Assert.equal(countFolderChildren(PlacesUtils.bookmarksMenuFolderId),
               SMART_BOOKMARKS_ON_MENU + DEFAULT_BOOKMARKS_ON_MENU);

  // Check version has been updated.
  Assert.equal(Services.prefs.getIntPref(PREF_SMART_BOOKMARKS_VERSION),
               SMART_BOOKMARKS_VERSION);
});

add_task(function* test_recreation_version_0() {
  do_print("Even if a smart bookmark has been removed recreate it if version is 0.");

  // Sanity check items.
  Assert.equal(countFolderChildren(PlacesUtils.toolbarFolderId),
               DEFAULT_BOOKMARKS_ON_TOOLBAR);
  Assert.equal(countFolderChildren(PlacesUtils.bookmarksMenuFolderId),
               SMART_BOOKMARKS_ON_MENU + DEFAULT_BOOKMARKS_ON_MENU);

  // Set preferences.
  Services.prefs.setIntPref(PREF_SMART_BOOKMARKS_VERSION, 0);

  yield rebuildSmartBookmarks();

  // Count items.
  // We should not have recreated the smart bookmark on toolbar.
  Assert.equal(countFolderChildren(PlacesUtils.toolbarFolderId),
               SMART_BOOKMARKS_ON_TOOLBAR + DEFAULT_BOOKMARKS_ON_TOOLBAR);
  Assert.equal(countFolderChildren(PlacesUtils.bookmarksMenuFolderId),
               SMART_BOOKMARKS_ON_MENU + DEFAULT_BOOKMARKS_ON_MENU);

  // Check version has been updated.
  Assert.equal(Services.prefs.getIntPref(PREF_SMART_BOOKMARKS_VERSION),
               SMART_BOOKMARKS_VERSION);
});
