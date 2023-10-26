/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/
 */

const TEST_URL = "about:robots";
let bookmarkPanel;
let folders;

add_setup(async function () {
  await PlacesUtils.bookmarks.eraseEverything();

  Services.prefs.clearUserPref("browser.bookmarks.defaultLocation");

  const tab = await BrowserTestUtils.openNewForegroundTab({
    gBrowser,
    opening: TEST_URL,
    waitForStateStop: true,
  });

  let oldTimeout = StarUI._autoCloseTimeout;
  // Make the timeout something big, so it doesn't iteract badly with tests.
  StarUI._autoCloseTimeout = 6000000;

  StarUI._createPanelIfNeeded();
  bookmarkPanel = document.getElementById("editBookmarkPanel");
  bookmarkPanel.setAttribute("animate", false);

  registerCleanupFunction(async () => {
    bookmarkPanel = null;
    StarUI._autoCloseTimeout = oldTimeout;
    BrowserTestUtils.removeTab(tab);
    await PlacesUtils.bookmarks.eraseEverything();
  });
});

add_task(async function test_selectChoose() {
  await clickBookmarkStar();

  // Open folder selector.
  let menuList = document.getElementById("editBMPanel_folderMenuList");
  let folderTreeRow = document.getElementById("editBMPanel_folderTreeRow");

  let expectedFolder = "BookmarksToolbarFolderTitle";
  let expectedGuid = PlacesUtils.bookmarks.toolbarGuid;

  Assert.equal(
    menuList.label,
    PlacesUtils.getString(expectedFolder),
    "Should have the expected bookmarks folder selected by default"
  );
  Assert.equal(
    menuList.getAttribute("selectedGuid"),
    expectedGuid,
    "Should have the correct default guid selected"
  );
  Assert.equal(
    folderTreeRow.hidden,
    true,
    "Should have the folder tree hidden"
  );

  let promisePopup = BrowserTestUtils.waitForEvent(
    menuList.menupopup,
    "popupshown"
  );
  EventUtils.synthesizeMouseAtCenter(menuList, {});
  await promisePopup;

  // Click the choose item.
  EventUtils.synthesizeMouseAtCenter(
    document.getElementById("editBMPanel_chooseFolderMenuItem"),
    {}
  );

  await TestUtils.waitForCondition(
    () => !folderTreeRow.hidden,
    "Should show the folder tree"
  );
  let folderTree = document.getElementById("editBMPanel_folderTree");
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

  let newFolderButton = document.getElementById("editBMPanel_newFolderButton");
  newFolderButton.click(); // This will start editing.

  // Wait for editing:
  await TestUtils.waitForCondition(() => !input.hidden);

  // Click the arrow to collapse the list.
  EventUtils.synthesizeMouseAtCenter(
    document.getElementById("editBMPanel_foldersExpander"),
    {}
  );

  await TestUtils.waitForCondition(
    () => folderTreeRow.hidden,
    "Should hide the folder tree"
  );
  ok(input.hidden, "Folder tree should not be broken.");

  // Click the arrow to re-show the list.
  EventUtils.synthesizeMouseAtCenter(
    document.getElementById("editBMPanel_foldersExpander"),
    {}
  );

  await TestUtils.waitForCondition(
    () => !folderTreeRow.hidden,
    "Should re-show the folder tree"
  );
  ok(input.hidden, "Folder tree should still not be broken.");

  const promiseCancel = promisePopupHidden(
    document.getElementById("editBookmarkPanel")
  );
  document.getElementById("editBookmarkPanelRemoveButton").click();
  await promiseCancel;
  Assert.ok(!folderTree.view, "The view should have been disconnected");
});

add_task(async function test_selectBookmarksMenu() {
  await clickBookmarkStar();

  // Open folder selector.
  let menuList = document.getElementById("editBMPanel_folderMenuList");

  const expectedFolder = "BookmarksMenuFolderTitle";
  const expectedGuid = PlacesUtils.bookmarks.menuGuid;

  let promisePopup = BrowserTestUtils.waitForEvent(
    menuList.menupopup,
    "popupshown"
  );
  EventUtils.synthesizeMouseAtCenter(menuList, {});
  await promisePopup;

  // Click the bookmarks menu item.
  EventUtils.synthesizeMouseAtCenter(
    document.getElementById("editBMPanel_bmRootItem"),
    {}
  );

  await TestUtils.waitForCondition(
    () => menuList.getAttribute("selectedGuid") == expectedGuid,
    "Should select the menu folder item"
  );

  Assert.equal(
    menuList.label,
    PlacesUtils.getString(expectedFolder),
    "Should have updated the menu label"
  );

  // Click the arrow to show the folder tree.
  EventUtils.synthesizeMouseAtCenter(
    document.getElementById("editBMPanel_foldersExpander"),
    {}
  );
  const folderTreeRow = document.getElementById("editBMPanel_folderTreeRow");
  await BrowserTestUtils.waitForMutationCondition(
    folderTreeRow,
    { attributeFilter: ["hidden"] },
    () => !folderTreeRow.hidden
  );
  const folderTree = document.getElementById("editBMPanel_folderTree");
  Assert.equal(
    folderTree.selectedNode.bookmarkGuid,
    PlacesUtils.bookmarks.virtualMenuGuid,
    "Folder tree should have the correct selected guid"
  );
  Assert.equal(
    menuList.getAttribute("selectedGuid"),
    expectedGuid,
    "Menu list should have the correct selected guid"
  );
  Assert.equal(
    menuList.label,
    PlacesUtils.getString(expectedFolder),
    "Should have kept the same menu label"
  );

  // Switch back to the toolbar folder.
  const { toolbarGuid } = PlacesUtils.bookmarks;
  promisePopup = BrowserTestUtils.waitForEvent(
    menuList.menupopup,
    "popupshown"
  );
  EventUtils.synthesizeMouseAtCenter(menuList, {});
  await promisePopup;
  EventUtils.synthesizeMouseAtCenter(
    document.getElementById("editBMPanel_toolbarFolderItem"),
    {}
  );
  await TestUtils.waitForCondition(
    () => menuList.getAttribute("selectedGuid") == toolbarGuid,
    "Should select the toolbar folder item"
  );

  // Save the bookmark.
  const promiseBookmarkAdded =
    PlacesTestUtils.waitForNotification("bookmark-added");
  await hideBookmarksPanel();
  const [{ parentGuid }] = await promiseBookmarkAdded;
  Assert.equal(
    parentGuid,
    toolbarGuid,
    "Should have saved the bookmark in the correct folder."
  );
});
