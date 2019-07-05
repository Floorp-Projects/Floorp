/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/
 */

"use strict";

const TEST_URL = "about:buildconfig";

add_task(async function test_change_title_from_BookmarkStar() {
  await PlacesUtils.bookmarks.insert({
    parentGuid: PlacesUtils.bookmarks.unfiledGuid,
    url: TEST_URL,
    title: "Before Edit",
  });

  let tab = await BrowserTestUtils.openNewForegroundTab({
    gBrowser,
    opening: TEST_URL,
    waitForStateStop: true,
  });

  registerCleanupFunction(async () => {
    BrowserTestUtils.removeTab(tab);
    await PlacesUtils.bookmarks.eraseEverything();
  });

  let bookmarkPanel = document.getElementById("editBookmarkPanel");
  let shownPromise = promisePopupShown(bookmarkPanel);

  let bookmarkStar = BookmarkingUI.star;
  bookmarkStar.click();

  await shownPromise;

  window.gEditItemOverlay.toggleFolderTreeVisibility();

  let folderTree = document.getElementById("editBMPanel_folderTree");

  // canDrop should always return false.
  let bookmarkWithId = JSON.stringify(
    Object.assign({
      url: "http://example.com",
      title: "Fake BM",
    })
  );

  let dt = {
    dropEffect: "move",
    mozCursor: "auto",
    mozItemCount: 1,
    types: [PlacesUtils.TYPE_X_MOZ_PLACE],
    mozTypesAt(i) {
      return this.types;
    },
    mozGetDataAt(i) {
      return bookmarkWithId;
    },
  };

  Assert.ok(
    !folderTree.view.canDrop(1, Ci.nsITreeView.DROP_BEFORE, dt),
    "Should not be able to drop a bookmark"
  );

  // User Actions should be disabled.
  const userActions = [
    "cmd_undo",
    "cmd_redo",
    "cmd_cut",
    "cmd_copy",
    "cmd_paste",
    "cmd_delete",
    "cmd_selectAll",
    // Anything starting with placesCmd_ should also be disabled.
    "placesCmd_",
  ];
  for (let action of userActions) {
    Assert.ok(
      !folderTree.view._controller.supportsCommand(action),
      `${action} should be disabled for the folder tree in bookmarks properties`
    );
  }

  let hiddenPromise = promisePopupHidden(bookmarkPanel);
  let doneButton = document.getElementById("editBookmarkPanelDoneButton");
  doneButton.click();
  await hiddenPromise;
});
