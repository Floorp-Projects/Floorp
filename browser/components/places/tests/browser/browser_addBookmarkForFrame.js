/**
 * Tests that the add bookmark for frame dialog functions correctly.
 */

const BASE_URL = "http://mochi.test:8888/browser/browser/components/places/tests/browser";
const PAGE_URL = BASE_URL + "/framedPage.html";
const LEFT_URL = BASE_URL + "/frameLeft.html";
const RIGHT_URL = BASE_URL + "/frameRight.html";

async function withAddBookmarkForFrame(taskFn) {
  // Open a tab and wait for all the subframes to load.
  let tab = await BrowserTestUtils.openNewForegroundTab(gBrowser, PAGE_URL);

  let contentAreaContextMenu = document.getElementById("contentAreaContextMenu");

  let popupShownPromise = BrowserTestUtils.waitForEvent(contentAreaContextMenu, "popupshown");
  await BrowserTestUtils.synthesizeMouseAtCenter("#left",
    { type: "contextmenu", button: 2}, gBrowser.selectedBrowser);
  await popupShownPromise;

  await withBookmarksDialog(true, function() {
    let frameMenuItem = document.getElementById("frame");
    frameMenuItem.click();

    let bookmarkFrame = document.getElementById("context-bookmarkframe");
    bookmarkFrame.click();
  }, taskFn);

  await BrowserTestUtils.removeTab(tab);
}

add_task(async function test_open_add_bookmark_for_frame() {
  info("Test basic opening of the add bookmark for frame dialog.");
  await withAddBookmarkForFrame(async dialogWin => {
    let namepicker = dialogWin.document.getElementById("editBMPanel_namePicker");
    Assert.ok(!namepicker.readOnly, "Name field is writable");
    Assert.equal(namepicker.value, "Left frame", "Name field is correct.");

    let expectedFolderName =
      PlacesUtils.getString("BookmarksMenuFolderTitle");

    let folderPicker = dialogWin.document.getElementById("editBMPanel_folderMenuList");
    await BrowserTestUtils.waitForCondition(
      () => folderPicker.selectedItem.label == expectedFolderName,
      "The folder is the expected one.");

    let tagsField = dialogWin.document.getElementById("editBMPanel_tagsField");
    Assert.equal(tagsField.value, "", "The tags field should be empty");
  });
});

add_task(async function test_move_bookmark_whilst_add_bookmark_open() {
  info("Test moving a bookmark whilst the add bookmark for frame dialog is open.");
  await withAddBookmarkForFrame(async dialogWin => {
    let bookmarksMenuFolderName = PlacesUtils.getString("BookmarksMenuFolderTitle");
    let toolbarFolderName = PlacesUtils.getString("BookmarksToolbarFolderTitle");

    let url = makeURI(LEFT_URL);
    let folderPicker = dialogWin.document.getElementById("editBMPanel_folderMenuList");

    // Check the initial state of the folder picker.
    await BrowserTestUtils.waitForCondition(
      () => folderPicker.selectedItem.label == bookmarksMenuFolderName,
      "The folder is the expected one.");

    // Check the bookmark has been created as expected.
    let bookmark = await PlacesUtils.bookmarks.fetch({url});

    Assert.equal(bookmark.parentGuid,
                 PlacesUtils.bookmarks.menuGuid,
                 "The bookmark should be in the menuGuid folder.");

    // Now move the bookmark and check the folder picker is updated correctly.
    bookmark.parentGuid = PlacesUtils.bookmarks.toolbarGuid;
    bookmark.index = PlacesUtils.bookmarks.DEFAULT_INDEX;

    await PlacesUtils.bookmarks.update(bookmark);

    await BrowserTestUtils.waitForCondition(
      () => folderPicker.selectedItem.label == toolbarFolderName,
      "The folder picker has changed to the new folder");
  });
});
