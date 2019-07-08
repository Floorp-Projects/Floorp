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

  bookmarkPanel = win.document.getElementById("editBookmarkPanel");
  bookmarkPanel.setAttribute("animate", false);

  registerCleanupFunction(async () => {
    bookmarkPanel = null;
    win.StarUI._autoCloseTimeout = oldTimeout;
    // BrowserTestUtils.removeTab(tab);
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

  Assert.equal(
    menuList.label,
    PlacesUtils.getString("OtherBookmarksFolderTitle"),
    "Should have the other bookmarks folder selected by default"
  );
  Assert.equal(
    menuList.getAttribute("selectedGuid"),
    PlacesUtils.bookmarks.unfiledGuid,
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

  Assert.equal(
    menuList.getAttribute("selectedGuid"),
    PlacesUtils.bookmarks.unfiledGuid,
    "Should still have the correct selected guid"
  );
  Assert.equal(
    menuList.label,
    PlacesUtils.getString("OtherBookmarksFolderTitle"),
    "Should have kept the same menu label"
  );

  await hideBookmarksPanel(win);
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
