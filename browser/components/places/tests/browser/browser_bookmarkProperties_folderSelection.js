/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/
 */

const TEST_URL = "about:robots";
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

add_task(async function test_selectChoose() {
  await clickBookmarkStar();

  // Open folder selector.
  let menuList = document.getElementById("editBMPanel_folderMenuList");
  let folderTreeRow = document.getElementById("editBMPanel_folderTreeRow");

  Assert.equal(menuList.label, PlacesUtils.getString("OtherBookmarksFolderTitle"),
    "Should have the other bookmarks folder selected by default");
  Assert.equal(menuList.getAttribute("selectedGuid"), PlacesUtils.bookmarks.unfiledGuid,
    "Should have the correct default guid selected");
  Assert.equal(folderTreeRow.collapsed, true,
    "Should have the folder tree collapsed");

  let promisePopup = BrowserTestUtils.waitForEvent(menuList.menupopup, "popupshown");
  EventUtils.synthesizeMouseAtCenter(menuList, {}, window);
  await promisePopup;

  // Click the choose item.
  EventUtils.synthesizeMouseAtCenter(document.getElementById("editBMPanel_chooseFolderMenuItem"), {}, window);

  await TestUtils.waitForCondition(() => !folderTreeRow.collapsed,
    "Should show the folder tree");

  Assert.equal(menuList.getAttribute("selectedGuid"), PlacesUtils.bookmarks.unfiledGuid,
    "Should still have the correct selected guid");
  Assert.equal(menuList.label, PlacesUtils.getString("OtherBookmarksFolderTitle"),
    "Should have kept the same menu label");

  await hideBookmarksPanel();
});


add_task(async function test_selectBookmarksMenu() {
  await clickBookmarkStar();

  // Open folder selector.
  let menuList = document.getElementById("editBMPanel_folderMenuList");

  let promisePopup = BrowserTestUtils.waitForEvent(menuList.menupopup, "popupshown");
  EventUtils.synthesizeMouseAtCenter(menuList, {}, window);
  await promisePopup;

  // Click the choose item.
  EventUtils.synthesizeMouseAtCenter(document.getElementById("editBMPanel_bmRootItem"), {}, window);

  await TestUtils.waitForCondition(
    () => menuList.getAttribute("selectedGuid") == PlacesUtils.bookmarks.menuGuid,
    "Should select the menu folder item");

  Assert.equal(menuList.label, PlacesUtils.getString("BookmarksMenuFolderTitle"),
    "Should have updated the menu label");

  await hideBookmarksPanel();
});
