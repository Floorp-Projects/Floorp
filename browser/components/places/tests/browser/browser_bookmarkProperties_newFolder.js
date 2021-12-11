/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/
 */

const TEST_URL = "about:robots";
StarUI._createPanelIfNeeded();
const bookmarkPanel = document.getElementById("editBookmarkPanel");
let folders;

add_task(async function setup() {
  await PlacesUtils.bookmarks.eraseEverything();

  bookmarkPanel.setAttribute("animate", false);

  let oldTimeout = StarUI._autoCloseTimeout;
  // Make the timeout something big, so it doesn't iteract badly with tests.
  StarUI._autoCloseTimeout = 6000000;

  let tab = await BrowserTestUtils.openNewForegroundTab({
    gBrowser,
    opening: TEST_URL,
    waitForStateStop: true,
  });

  registerCleanupFunction(async () => {
    StarUI._autoCloseTimeout = oldTimeout;
    BrowserTestUtils.removeTab(tab);
    bookmarkPanel.removeAttribute("animate");
    await PlacesUtils.bookmarks.eraseEverything();
  });
});

add_task(async function test_newFolder() {
  await clickBookmarkStar();

  // Open folder selector.
  document.getElementById("editBMPanel_foldersExpander").click();

  let folderTree = document.getElementById("editBMPanel_folderTree");

  // Create new folder.
  let newFolderButton = document.getElementById("editBMPanel_newFolderButton");
  newFolderButton.click();

  let newFolderGuid;
  let newFolderObserver = PlacesTestUtils.waitForNotification(
    "bookmark-added",
    events => {
      for (let { guid, itemType } of events) {
        newFolderGuid = guid;
        if (itemType == PlacesUtils.bookmarks.TYPE_FOLDER) {
          return true;
        }
      }
      return false;
    },
    "places"
  );

  let menulist = document.getElementById("editBMPanel_folderMenuList");

  await newFolderObserver;

  // Wait for the folder to be created and for editing to start.
  await TestUtils.waitForCondition(
    () => folderTree.hasAttribute("editing"),
    "Should be in edit mode for the new folder"
  );

  Assert.equal(
    menulist.selectedItem.label,
    newFolderButton.label,
    "Should have the new folder selected by default"
  );

  let renameObserver = PlacesTestUtils.waitForNotification(
    "bookmark-title-changed",
    events => events.some(e => e.title === "f"),
    "places"
  );

  // Enter a new name.
  EventUtils.synthesizeKey("f", {}, window);
  EventUtils.synthesizeKey("VK_RETURN", {}, window);

  await renameObserver;

  await TestUtils.waitForCondition(
    () => !folderTree.hasAttribute("editing"),
    "Should have stopped editing the new folder"
  );

  Assert.equal(
    menulist.selectedItem.label,
    "f",
    "Should have the new folder title"
  );

  let bookmark = await PlacesUtils.bookmarks.fetch({ url: TEST_URL });

  Assert.equal(
    bookmark.parentGuid,
    newFolderGuid,
    "The bookmark should be parented by the new folder"
  );

  await hideBookmarksPanel();
});
