/**
 * This test checks that the Show in Folder context menu item in the
 * bookmarks menu under the app menu actually shows the bookmark in
 * its folder location in the sidebar.
 */
"use strict";

const TEST_PARENT_FOLDER = "The Parent Folder";
const TEST_URL = "https://example.com/";
const TEST_TITLE = "Test Bookmark";

let appMenuButton = document.getElementById("PanelUI-menu-button");
let bookmarksAppMenu = document.getElementById("PanelUI-bookmarks");
let sidebarWasAlreadyOpen = SidebarController.isOpen;

const { CustomizableUITestUtils } = ChromeUtils.importESModule(
  "resource://testing-common/CustomizableUITestUtils.sys.mjs"
);
let gCUITestUtils = new CustomizableUITestUtils(window);

add_task(async function toolbarBookmarkShowInFolder() {
  let parentFolder = await PlacesUtils.bookmarks.insert({
    parentGuid: PlacesUtils.bookmarks.unfiledGuid,
    type: PlacesUtils.bookmarks.TYPE_FOLDER,
    title: TEST_PARENT_FOLDER,
  });
  await PlacesUtils.bookmarks.insert({
    parentGuid: parentFolder.guid,
    url: TEST_URL,
    title: TEST_TITLE,
  });

  // Open app menu and select bookmarks view
  await gCUITestUtils.openMainMenu();
  let appMenuBookmarks = document.getElementById("appMenu-bookmarks-button");
  appMenuBookmarks.click();
  let bookmarksView = document.getElementById("PanelUI-bookmarks");
  let bmViewPromise = BrowserTestUtils.waitForEvent(bookmarksView, "ViewShown");
  await bmViewPromise;

  // Find the test bookmark and open the context menu on it
  let list = document.getElementById("panelMenu_bookmarksMenu");
  let listItem = [...list.children].find(node => node.label == TEST_TITLE);
  let placesContext = document.getElementById("placesContext");
  let contextPromise = BrowserTestUtils.waitForEvent(
    placesContext,
    "popupshown"
  );
  EventUtils.synthesizeMouseAtCenter(listItem, {
    button: 2,
    type: "contextmenu",
  });
  await contextPromise;

  // Select Show in Folder and wait for the sidebar to show up
  let sidebarShownPromise = BrowserTestUtils.waitForEvent(
    window,
    "SidebarShown"
  );
  placesContext.activateItem(
    document.getElementById("placesContext_showInFolder")
  );
  await sidebarShownPromise;

  // Get the sidebar tree element and find the selected node
  let sidebar = document.getElementById("sidebar");
  let tree = sidebar.contentDocument.getElementById("bookmarks-view");
  let treeNode = tree.selectedNode;

  Assert.equal(
    treeNode.parent.bookmarkGuid,
    parentFolder.guid,
    "Containing folder node is correct"
  );
  Assert.equal(
    treeNode.title,
    listItem.label,
    "The bookmark title matches selected node"
  );
  Assert.equal(
    treeNode.uri,
    TEST_URL,
    "The bookmark URL matches selected node"
  );

  // Cleanup
  await PlacesUtils.bookmarks.eraseEverything();
  if (!sidebarWasAlreadyOpen) {
    SidebarController.hide();
  }
  await gCUITestUtils.hideMainMenu();
});
