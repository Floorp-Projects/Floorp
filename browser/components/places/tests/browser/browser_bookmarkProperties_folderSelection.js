/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/
 */

const TEST_URL = "about:robots";
let bookmarkPanel;
let folders;
let win;

add_task(async function setup() {
  await PlacesUtils.bookmarks.eraseEverything();

  win = await BrowserTestUtils.openNewBrowserWindow();
  await BrowserTestUtils.openNewForegroundTab({
    gBrowser: win.gBrowser,
    opening: TEST_URL,
    waitForStateStop: true,
  });

  let oldTimeout = win.StarUI._autoCloseTimeout;
  // Make the timeout something big, so it doesn't iteract badly with tests.
  win.StarUI._autoCloseTimeout = 6000000;

  win.StarUI._createPanelIfNeeded();
  bookmarkPanel = win.document.getElementById("editBookmarkPanel");
  bookmarkPanel.setAttribute("animate", false);

  registerCleanupFunction(async () => {
    bookmarkPanel = null;
    win.StarUI._autoCloseTimeout = oldTimeout;
    await BrowserTestUtils.closeWindow(win);
    win = null;
    await PlacesUtils.bookmarks.eraseEverything();
  });
});

add_task(async function test_selectChoose() {
  await clickBookmarkStar(win);

  // Open folder selector.
  let menuList = win.document.getElementById("editBMPanel_folderMenuList");
  let folderTreeRow = win.document.getElementById("editBMPanel_folderTreeRow");

  let expectedFolder = gBookmarksToolbar2h2020
    ? "BookmarksToolbarFolderTitle"
    : "OtherBookmarksFolderTitle";
  let expectedGuid = gBookmarksToolbar2h2020
    ? PlacesUtils.bookmarks.toolbarGuid
    : PlacesUtils.bookmarks.unfiledGuid;

  Assert.equal(
    menuList.label,
    PlacesUtils.getString(expectedFolder),
    "Should have the other bookmarks folder selected by default"
  );
  Assert.equal(
    menuList.getAttribute("selectedGuid"),
    expectedGuid,
    "Should have the correct default guid selected"
  );
  Assert.equal(
    folderTreeRow.collapsed,
    true,
    "Should have the folder tree collapsed"
  );

  let promisePopup = BrowserTestUtils.waitForEvent(
    menuList.menupopup,
    "popupshown"
  );
  EventUtils.synthesizeMouseAtCenter(menuList, {}, win);
  await promisePopup;

  // Click the choose item.
  EventUtils.synthesizeMouseAtCenter(
    win.document.getElementById("editBMPanel_chooseFolderMenuItem"),
    {},
    win
  );

  await TestUtils.waitForCondition(
    () => !folderTreeRow.collapsed,
    "Should show the folder tree"
  );
  let folderTree = win.document.getElementById("editBMPanel_folderTree");
  Assert.ok(folderTree.view, "The view should have been connected");

  Assert.equal(
    menuList.getAttribute("selectedGuid"),
    expectedGuid,
    "Should still have the correct selected guid"
  );
  Assert.equal(
    menuList.label,
    PlacesUtils.getString(expectedFolder),
    "Should have kept the same menu label"
  );

  let input = folderTree.shadowRoot.querySelector("input");

  let newFolderButton = win.document.getElementById(
    "editBMPanel_newFolderButton"
  );
  newFolderButton.click(); // This will start editing.

  // Wait for editing:
  await TestUtils.waitForCondition(() => !input.hidden);

  // Click the arrow to collapse the list.
  EventUtils.synthesizeMouseAtCenter(
    win.document.getElementById("editBMPanel_foldersExpander"),
    {},
    win
  );

  await TestUtils.waitForCondition(
    () => folderTreeRow.collapsed,
    "Should hide the folder tree"
  );
  ok(input.hidden, "Folder tree should not be broken.");

  // Click the arrow to re-show the list.
  EventUtils.synthesizeMouseAtCenter(
    win.document.getElementById("editBMPanel_foldersExpander"),
    {},
    win
  );

  await TestUtils.waitForCondition(
    () => !folderTreeRow.collapsed,
    "Should re-show the folder tree"
  );
  ok(input.hidden, "Folder tree should still not be broken.");

  await hideBookmarksPanel(win);
  Assert.ok(!folderTree.view, "The view should have been disconnected");
});

add_task(async function test_selectBookmarksMenu() {
  await clickBookmarkStar(win);

  // Open folder selector.
  let menuList = win.document.getElementById("editBMPanel_folderMenuList");

  let promisePopup = BrowserTestUtils.waitForEvent(
    menuList.menupopup,
    "popupshown"
  );
  EventUtils.synthesizeMouseAtCenter(menuList, {}, win);
  await promisePopup;

  // Click the choose item.
  EventUtils.synthesizeMouseAtCenter(
    win.document.getElementById("editBMPanel_bmRootItem"),
    {},
    win
  );

  // TODO Bug 1695011: This delay is to stop the test impacting on other tests.
  // It is likely that we need a different wait here, however we need to figure
  // out what is going on in the background to impact the other tests.
  // eslint-disable-next-line mozilla/no-arbitrary-setTimeout
  await new Promise(r => setTimeout(r, 100));
  await TestUtils.waitForCondition(
    () =>
      menuList.getAttribute("selectedGuid") == PlacesUtils.bookmarks.menuGuid,
    "Should select the menu folder item"
  );

  Assert.equal(
    menuList.label,
    PlacesUtils.getString("BookmarksMenuFolderTitle"),
    "Should have updated the menu label"
  );

  await hideBookmarksPanel(win);
});
