/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/**
 *  Test enabled commands in the left pane folder of the Library.
 */

registerCleanupFunction(async function() {
  await PlacesUtils.bookmarks.eraseEverything();
  await PlacesTestUtils.clearHistory();
});

add_task(async function test_moveBookmarks() {
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

  let bookmarks = await PlacesUtils.bookmarks.insertTree({
    guid: PlacesUtils.bookmarks.unfiledGuid,
    children
  });

  let folderId = await PlacesUtils.promiseItemId(bookmarks[0].guid);

  let itemIds = [];
  let promiseMoveNotifications = [];
  for (let i = 0; i < 10; i++) {
    // + 1 due to the folder being inserted first.
    let guid = bookmarks[i + 1].guid;

    itemIds.push(await PlacesUtils.promiseItemId(guid));
    promiseMoveNotifications.push(PlacesTestUtils.waitForNotification(
      "onItemMoved",
      (itemId, parentId, oldIndex, newParentId, newIndex, itemType, itemGuid,
       oldParentGuid, newParentGuid) =>
        itemGuid == guid && newParentGuid == bookmarks[0].guid
    ));
  }

  let library = await promiseLibrary("UnfiledBookmarks");

  library.ContentTree.view.selectItems(itemIds);

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

  library.close();
});
