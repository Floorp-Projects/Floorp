/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/**
 *  Test enabled commands in the left pane folder of the Library.
 */

var bookmarks;
var testFolder;
var library;

add_task(async function setup() {
  let children = [{
    title: "TestFolder",
    type: PlacesUtils.bookmarks.TYPE_FOLDER,
  }];

  for (let i = 0; i < 10; i++) {
    children.push({
      title: `test${i}`,
      url: `http://example.com/${i}`,
    });
  }

  bookmarks = await PlacesUtils.bookmarks.insertTree({
    guid: PlacesUtils.bookmarks.unfiledGuid,
    children
  });

  testFolder = bookmarks.shift();

  library = await promiseLibrary("UnfiledBookmarks");

  registerCleanupFunction(async function() {
    library.close();

    await PlacesUtils.bookmarks.eraseEverything();
    await PlacesTestUtils.clearHistory();
  });
});

async function move_bookmarks_to(sourceFolders, folderGuid) {
  for (let i = 0; i < sourceFolders.length; i++) {
    sourceFolders[i] = await PlacesUtils.promiseItemId(sourceFolders[i]);
  }

  library.PlacesOrganizer.selectLeftPaneContainerByHierarchy(sourceFolders);

  let folderId = await PlacesUtils.promiseItemId(folderGuid);

  let promiseMoveNotifications = [];
  for (let bm of bookmarks) {
    let guid = bm.guid;

    promiseMoveNotifications.push(PlacesTestUtils.waitForNotification(
      "onItemMoved",
      (itemId, parentId, oldIndex, newParentId, newIndex, itemType, itemGuid,
       oldParentGuid, newParentGuid) =>
        itemGuid == guid && newParentGuid == folderGuid
    ));
  }

  library.ContentTree.view.selectItems(bookmarks.map(bm => bm.guid));

  await withBookmarksDialog(
    false,
    () => {
      library.ContentTree.view._controller.doCommand("placesCmd_moveBookmarks");
    },
    async (dialogWin) => {
      let tree = dialogWin.document.getElementById("foldersTree");

      tree.selectItems([PlacesUtils.unfiledBookmarksFolderId], true);

      tree.selectItems([folderId], true);

      dialogWin.document.documentElement.acceptDialog();

      info("Waiting for notifications of moves");
      await Promise.all(promiseMoveNotifications);
      Assert.ok(true, "should have completed all moves successfully");
    },
    null,
    "chrome://browser/content/places/moveBookmarks.xul",
    true
  );
}

add_task(async function test_moveBookmarks_to_subfolder() {
  await move_bookmarks_to([PlacesUtils.bookmarks.unfiledGuid], testFolder.guid);
});

add_task(async function test_moveBookmarks_to_other_top_level() {
  await move_bookmarks_to([PlacesUtils.bookmarks.unfiledGuid, testFolder.guid],
    PlacesUtils.bookmarks.toolbarGuid);
});
