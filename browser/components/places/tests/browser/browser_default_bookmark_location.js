/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

const LOCATION_PREF = "browser.bookmarks.defaultLocation";
const TEST_URL = "about:about";
let bookmarkPanel;
let win;

add_setup(async function () {
  Services.prefs.clearUserPref(LOCATION_PREF);
  await PlacesUtils.bookmarks.eraseEverything();

  win = await BrowserTestUtils.openNewBrowserWindow();
  await BrowserTestUtils.openNewForegroundTab({
    gBrowser: win.gBrowser,
    opening: TEST_URL,
    waitForStateStop: true,
  });

  let oldTimeout = win.StarUI._autoCloseTimeout;
  // Make the timeout something big, so it doesn't interact badly with tests.
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
    Services.prefs.clearUserPref(LOCATION_PREF);
  });
});

async function cancelBookmarkCreationInPanel() {
  let hiddenPromise = promisePopupHidden(
    win.document.getElementById("editBookmarkPanel")
  );
  // Confirm and close the dialog.

  win.document.getElementById("editBookmarkPanelRemoveButton").click();
  await hiddenPromise;
}

/**
 * Helper to check the selected folder is correct.
 */
async function checkSelection() {
  // Open folder selector.
  let menuList = win.document.getElementById("editBMPanel_folderMenuList");

  let expectedFolder = "BookmarksToolbarFolderTitle";
  Assert.equal(
    menuList.label,
    PlacesUtils.getString(expectedFolder),
    `Should have ${expectedFolder} selected by default`
  );
  Assert.equal(
    menuList.getAttribute("selectedGuid"),
    await PlacesUIUtils.defaultParentGuid,
    "Should have the correct default guid selected"
  );

  await cancelBookmarkCreationInPanel();
}

/**
 * Verify that bookmarks created with the star button go to the default
 * bookmark location.
 */
add_task(async function test_star_location() {
  await clickBookmarkStar(win);
  await checkSelection();
});

/**
 * Verify that bookmarks created with the shortcut go to the default bookmark
 * location.
 */
add_task(async function test_shortcut_location() {
  let shownPromise = promisePopupShown(
    win.document.getElementById("editBookmarkPanel")
  );
  win.document.getElementById("Browser:AddBookmarkAs").doCommand();
  await shownPromise;
  await checkSelection();
});

// Note: Bookmarking frames is tested in browser_addBookmarkForFrame.js

/**
 * Verify that bookmarks created with the link context menu go to the default
 * bookmark location.
 */
add_task(async function test_context_menu_link() {
  for (let t = 0; t < 2; t++) {
    if (t == 1) {
      // For the second iteration, ensure that the default folder is invalid first.
      await createAndRemoveDefaultFolder();
    }

    await withBookmarksDialog(
      true,
      async function openDialog() {
        const contextMenu = win.document.getElementById(
          "contentAreaContextMenu"
        );
        is(contextMenu.state, "closed", "checking if popup is closed");
        let promisePopupShown = BrowserTestUtils.waitForEvent(
          contextMenu,
          "popupshown"
        );
        BrowserTestUtils.synthesizeMouseAtCenter(
          "a[href*=config]", // Bookmark about:config
          { type: "contextmenu", button: 2 },
          win.gBrowser.selectedBrowser
        );
        await promisePopupShown;
        contextMenu.activateItem(
          win.document.getElementById("context-bookmarklink")
        );
      },
      async function test(dialogWin) {
        let expectedFolder = "BookmarksToolbarFolderTitle";
        let expectedFolderName = PlacesUtils.getString(expectedFolder);

        let folderPicker = dialogWin.document.getElementById(
          "editBMPanel_folderMenuList"
        );

        // Check the initial state of the folder picker.
        await TestUtils.waitForCondition(
          () => folderPicker.selectedItem.label == expectedFolderName,
          "The folder is the expected one."
        );
      }
    );
  }
});

/**
 * Verify that if we change the location, we persist that selection.
 */
add_task(async function test_change_location_panel() {
  await clickBookmarkStar(win);

  let menuList = win.document.getElementById("editBMPanel_folderMenuList");

  let { toolbarGuid, menuGuid, unfiledGuid } = PlacesUtils.bookmarks;

  let expectedFolderGuid = toolbarGuid;

  info("Pref value: " + Services.prefs.getCharPref(LOCATION_PREF, ""));
  await TestUtils.waitForCondition(
    () => menuList.getAttribute("selectedGuid") == expectedFolderGuid,
    "Should initially select the unfiled or toolbar item"
  );

  // Now move this new bookmark to the menu:
  let promisePopup = BrowserTestUtils.waitForEvent(
    menuList.menupopup,
    "popupshown"
  );
  EventUtils.synthesizeMouseAtCenter(menuList, {}, win);
  await promisePopup;

  // Make sure we wait for the bookmark to be added.
  let itemAddedPromise = PlacesTestUtils.waitForNotification(
    "bookmark-added",
    events => events.some(({ url }) => url === TEST_URL)
  );

  // Wait for the pref to change
  let prefChangedPromise = TestUtils.waitForPrefChange(LOCATION_PREF);

  // Click the choose item.
  EventUtils.synthesizeMouseAtCenter(
    win.document.getElementById("editBMPanel_bmRootItem"),
    {},
    win
  );

  await TestUtils.waitForCondition(
    () => menuList.getAttribute("selectedGuid") == menuGuid,
    "Should select the menu folder item"
  );

  info("Waiting for transactions to finish.");
  await Promise.all(win.gEditItemOverlay.transactionPromises);
  info("Moved; waiting to hide panel.");

  await hideBookmarksPanel(win);
  info("Waiting for pref change.");
  await prefChangedPromise;
  info("Waiting for item to be added.");
  await itemAddedPromise;

  // Check that it's in the menu, and remove the bookmark:
  let bm = await PlacesUtils.bookmarks.fetch({ url: TEST_URL });
  is(bm?.parentGuid, menuGuid, "Bookmark was put in the menu.");
  if (bm) {
    await PlacesUtils.bookmarks.remove(bm);
  }

  // Now create a new bookmark and check it starts in the menu
  await clickBookmarkStar(win);

  let expectedFolder = "BookmarksMenuFolderTitle";
  Assert.equal(
    menuList.label,
    PlacesUtils.getString(expectedFolder),
    `Should have menu folder selected by default`
  );
  Assert.equal(
    menuList.getAttribute("selectedGuid"),
    menuGuid,
    "Should have the correct default guid selected"
  );

  // Now select a different item.
  promisePopup = BrowserTestUtils.waitForEvent(
    menuList.menupopup,
    "popupshown"
  );
  EventUtils.synthesizeMouseAtCenter(menuList, {}, win);
  await promisePopup;

  // Click the toolbar item.
  EventUtils.synthesizeMouseAtCenter(
    win.document.getElementById("editBMPanel_toolbarFolderItem"),
    {},
    win
  );

  await TestUtils.waitForCondition(
    () => menuList.getAttribute("selectedGuid") == toolbarGuid,
    "Should select the toolbar item"
  );

  await cancelBookmarkCreationInPanel();

  is(
    await PlacesUIUtils.defaultParentGuid,
    menuGuid,
    "Default folder should not change if we cancel the panel."
  );

  // Now open the panel for an existing bookmark whose parent doesn't match
  // the default and check we don't overwrite the default folder.
  let testBM = await PlacesUtils.bookmarks.insert({
    parentGuid: unfiledGuid,
    title: "Yoink",
    url: TEST_URL,
  });
  await TestUtils.waitForCondition(
    () => win.BookmarkingUI.star.hasAttribute("starred"),
    "Wait for bookmark to show up for current page."
  );
  await clickBookmarkStar(win);

  await hideBookmarksPanel(win);
  is(
    await PlacesUIUtils.defaultParentGuid,
    menuGuid,
    "Default folder should not change if we accept the panel, but didn't change folders."
  );

  await PlacesUtils.bookmarks.remove(testBM);
});
