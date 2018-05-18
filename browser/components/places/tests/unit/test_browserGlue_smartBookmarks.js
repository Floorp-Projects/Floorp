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

registerCleanupFunction(() => PlacesUtils.bookmarks.eraseEverything());

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

add_task(async function setup() {
  // Initialize browserGlue, but remove it's listener to places-init-complete.
  Cc["@mozilla.org/browser/browserglue;1"].getService(Ci.nsIObserver);

  // Initialize Places through the History Service and check that a new
  // database has been created.
  let promiseComplete = promiseTopicObserved("places-browser-init-complete");
  Assert.equal(PlacesUtils.history.databaseStatus,
               PlacesUtils.history.DATABASE_STATUS_CREATE);
  await promiseComplete;

  // Ensure preferences status.
  Assert.ok(!Services.prefs.getBoolPref(PREF_AUTO_EXPORT_HTML));
  Assert.ok(!Services.prefs.getBoolPref(PREF_RESTORE_DEFAULT_BOOKMARKS));
  Assert.throws(() => Services.prefs.getBoolPref(PREF_IMPORT_BOOKMARKS_HTML),
    /NS_ERROR_UNEXPECTED/);
});

add_task(async function test_version_0() {
  info("All smart bookmarks are created if smart bookmarks version is 0.");

  // Sanity check: we should have default bookmark.
  Assert.ok(await PlacesUtils.bookmarks.fetch({
    parentGuid: PlacesUtils.bookmarks.toolbarGuid,
    index: 0
  }));

  Assert.ok(await PlacesUtils.bookmarks.fetch({
    parentGuid: PlacesUtils.bookmarks.menuGuid,
    index: 0
  }));

  // Set preferences.
  Services.prefs.setIntPref(PREF_SMART_BOOKMARKS_VERSION, 0);

  await rebuildSmartBookmarks();

  // Count items.
  Assert.equal(countFolderChildren(PlacesUtils.toolbarFolderId),
               SMART_BOOKMARKS_ON_TOOLBAR + DEFAULT_BOOKMARKS_ON_TOOLBAR);
  Assert.equal(countFolderChildren(PlacesUtils.bookmarksMenuFolderId),
               SMART_BOOKMARKS_ON_MENU + DEFAULT_BOOKMARKS_ON_MENU);

  // Check version has been updated.
  Assert.equal(Services.prefs.getIntPref(PREF_SMART_BOOKMARKS_VERSION),
               SMART_BOOKMARKS_VERSION);
});

add_task(async function test_version_change() {
  info("An existing smart bookmark is replaced when version changes.");

  // Sanity check: we have a smart bookmark on the toolbar.
  let bm = await PlacesUtils.bookmarks.fetch({
    parentGuid: PlacesUtils.bookmarks.toolbarGuid,
    index: 0
  });
  await checkItemHasAnnotation(bm.guid, SMART_BOOKMARKS_ANNO);

  // Change its title.
  await PlacesUtils.bookmarks.update({guid: bm.guid, title: "new title"});
  bm = await PlacesUtils.bookmarks.fetch({guid: bm.guid});
  Assert.equal(bm.title, "new title");

  // Sanity check items.
  Assert.equal(countFolderChildren(PlacesUtils.toolbarFolderId),
               SMART_BOOKMARKS_ON_TOOLBAR + DEFAULT_BOOKMARKS_ON_TOOLBAR);
  Assert.equal(countFolderChildren(PlacesUtils.bookmarksMenuFolderId),
               SMART_BOOKMARKS_ON_MENU + DEFAULT_BOOKMARKS_ON_MENU);

  // Set preferences.
  Services.prefs.setIntPref(PREF_SMART_BOOKMARKS_VERSION, 1);

  await rebuildSmartBookmarks();

  // Count items.
  Assert.equal(countFolderChildren(PlacesUtils.toolbarFolderId),
               SMART_BOOKMARKS_ON_TOOLBAR + DEFAULT_BOOKMARKS_ON_TOOLBAR);
  Assert.equal(countFolderChildren(PlacesUtils.bookmarksMenuFolderId),
               SMART_BOOKMARKS_ON_MENU + DEFAULT_BOOKMARKS_ON_MENU);

  // Check smart bookmark has been replaced, itemId has changed.
  bm = await PlacesUtils.bookmarks.fetch({
    parentGuid: PlacesUtils.bookmarks.toolbarGuid,
    index: 0
  });
  await checkItemHasAnnotation(bm.guid, SMART_BOOKMARKS_ANNO);
  Assert.notEqual(bm.title, "new title");

  // Check version has been updated.
  Assert.equal(Services.prefs.getIntPref(PREF_SMART_BOOKMARKS_VERSION),
               SMART_BOOKMARKS_VERSION);
});

add_task(async function test_version_change_pos() {
  info("bookmarks position is retained when version changes.");

  // Sanity check items.
  Assert.equal(countFolderChildren(PlacesUtils.toolbarFolderId),
               SMART_BOOKMARKS_ON_TOOLBAR + DEFAULT_BOOKMARKS_ON_TOOLBAR);
  Assert.equal(countFolderChildren(PlacesUtils.bookmarksMenuFolderId),
               SMART_BOOKMARKS_ON_MENU + DEFAULT_BOOKMARKS_ON_MENU);

  let bm = await PlacesUtils.bookmarks.fetch({
    parentGuid: PlacesUtils.bookmarks.menuGuid,
    index: 0
  });
  await checkItemHasAnnotation(bm.guid, SMART_BOOKMARKS_ANNO);
  let firstItemTitle = bm.title;

  // Set preferences.
  Services.prefs.setIntPref(PREF_SMART_BOOKMARKS_VERSION, 1);

  await rebuildSmartBookmarks();

  // Count items.
  Assert.equal(countFolderChildren(PlacesUtils.toolbarFolderId),
               SMART_BOOKMARKS_ON_TOOLBAR + DEFAULT_BOOKMARKS_ON_TOOLBAR);
  Assert.equal(countFolderChildren(PlacesUtils.bookmarksMenuFolderId),
               SMART_BOOKMARKS_ON_MENU + DEFAULT_BOOKMARKS_ON_MENU);

  // Check smart bookmarks are still in correct position.
  bm = await PlacesUtils.bookmarks.fetch({
    parentGuid: PlacesUtils.bookmarks.menuGuid,
    index: 0
  });
  await checkItemHasAnnotation(bm.guid, SMART_BOOKMARKS_ANNO);
  Assert.equal(bm.title, firstItemTitle);

  // Check version has been updated.
  Assert.equal(Services.prefs.getIntPref(PREF_SMART_BOOKMARKS_VERSION),
               SMART_BOOKMARKS_VERSION);
});

add_task(async function test_version_change_pos_moved() {
  info("moved bookmarks position is retained when version changes.");

  // Sanity check items.
  Assert.equal(countFolderChildren(PlacesUtils.toolbarFolderId),
               SMART_BOOKMARKS_ON_TOOLBAR + DEFAULT_BOOKMARKS_ON_TOOLBAR);
  Assert.equal(countFolderChildren(PlacesUtils.bookmarksMenuFolderId),
               SMART_BOOKMARKS_ON_MENU + DEFAULT_BOOKMARKS_ON_MENU);

  let bm1 = await PlacesUtils.bookmarks.fetch({
    parentGuid: PlacesUtils.bookmarks.menuGuid,
    index: 0
  });
  await checkItemHasAnnotation(bm1.guid, SMART_BOOKMARKS_ANNO);
  let firstItemTitle = bm1.title;

  // Move the first smart bookmark to the end of the menu.
  await PlacesUtils.bookmarks.update({
    parentGuid: PlacesUtils.bookmarks.menuGuid,
    guid: bm1.guid,
    index: PlacesUtils.bookmarks.DEFAULT_INDEX
  });

  let bm = await PlacesUtils.bookmarks.fetch({
    parentGuid: PlacesUtils.bookmarks.menuGuid,
    index: PlacesUtils.bookmarks.DEFAULT_INDEX
  });
  Assert.equal(bm.guid, bm1.guid);

  // Set preferences.
  Services.prefs.setIntPref(PREF_SMART_BOOKMARKS_VERSION, 1);

  await rebuildSmartBookmarks();

  // Count items.
  Assert.equal(countFolderChildren(PlacesUtils.toolbarFolderId),
               SMART_BOOKMARKS_ON_TOOLBAR + DEFAULT_BOOKMARKS_ON_TOOLBAR);
  Assert.equal(countFolderChildren(PlacesUtils.bookmarksMenuFolderId),
               SMART_BOOKMARKS_ON_MENU + DEFAULT_BOOKMARKS_ON_MENU);

  bm1 = await PlacesUtils.bookmarks.fetch({
    parentGuid: PlacesUtils.bookmarks.menuGuid,
    index: PlacesUtils.bookmarks.DEFAULT_INDEX
  });
  await checkItemHasAnnotation(bm1.guid, SMART_BOOKMARKS_ANNO);
  Assert.equal(bm1.title, firstItemTitle);

  // Move back the smart bookmark to the original position.
  await PlacesUtils.bookmarks.update({
    parentGuid: PlacesUtils.bookmarks.menuGuid,
    guid: bm1.guid,
    index: 1
  });

  // Check version has been updated.
  Assert.equal(Services.prefs.getIntPref(PREF_SMART_BOOKMARKS_VERSION),
              SMART_BOOKMARKS_VERSION);
});

add_task(async function test_recreation() {
  info("An explicitly removed smart bookmark should not be recreated.");

  // Remove toolbar's smart bookmarks
  let bm = await PlacesUtils.bookmarks.fetch({
    parentGuid: PlacesUtils.bookmarks.toolbarGuid,
    index: 0
  });
  await PlacesUtils.bookmarks.remove(bm.guid);

  // Sanity check items.
  Assert.equal(countFolderChildren(PlacesUtils.toolbarFolderId),
               DEFAULT_BOOKMARKS_ON_TOOLBAR);
  Assert.equal(countFolderChildren(PlacesUtils.bookmarksMenuFolderId),
               SMART_BOOKMARKS_ON_MENU + DEFAULT_BOOKMARKS_ON_MENU);

  // Set preferences.
  Services.prefs.setIntPref(PREF_SMART_BOOKMARKS_VERSION, 1);

  await rebuildSmartBookmarks();

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

add_task(async function test_recreation_version_0() {
  info("Even if a smart bookmark has been removed recreate it if version is 0.");

  // Sanity check items.
  Assert.equal(countFolderChildren(PlacesUtils.toolbarFolderId),
               DEFAULT_BOOKMARKS_ON_TOOLBAR);
  Assert.equal(countFolderChildren(PlacesUtils.bookmarksMenuFolderId),
               SMART_BOOKMARKS_ON_MENU + DEFAULT_BOOKMARKS_ON_MENU);

  // Set preferences.
  Services.prefs.setIntPref(PREF_SMART_BOOKMARKS_VERSION, 0);

  await rebuildSmartBookmarks();

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
