/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

const TEST_URL = "about:about";
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
  });
});

/**
 * Helper to check the selected folder is correct.
 */
async function checkSelection() {
  // Open folder selector.
  let menuList = win.document.getElementById("editBMPanel_folderMenuList");

  let expectedFolder = win.gBookmarksToolbar2h2020
    ? "BookmarksToolbarFolderTitle"
    : "OtherBookmarksFolderTitle";
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

  let hiddenPromise = promisePopupHidden(
    win.document.getElementById("editBookmarkPanel")
  );
  // Confirm and close the dialog.
  win.document.getElementById("editBookmarkPanelRemoveButton").click();
  await hiddenPromise;
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
  await withBookmarksDialog(
    true,
    async function openDialog() {
      const contextMenu = win.document.getElementById("contentAreaContextMenu");
      is(contextMenu.state, "closed", "checking if popup is closed");
      let promisePopupShown = BrowserTestUtils.waitForEvent(
        contextMenu,
        "popupshown"
      );
      BrowserTestUtils.synthesizeMouseAtCenter(
        "a[href]",
        { type: "contextmenu", button: 2 },
        win.gBrowser.selectedBrowser
      );
      await promisePopupShown;
      win.document.getElementById("context-bookmarklink").click();
    },
    async function test(dialogWin) {
      let expectedFolder = win.gBookmarksToolbar2h2020
        ? "BookmarksToolbarFolderTitle"
        : "OtherBookmarksFolderTitle";
      let expectedFolderName = PlacesUtils.getString(expectedFolder);

      let folderPicker = dialogWin.document.getElementById(
        "editBMPanel_folderMenuList"
      );

      // Check the initial state of the folder picker.
      await BrowserTestUtils.waitForCondition(
        () => folderPicker.selectedItem.label == expectedFolderName,
        "The folder is the expected one."
      );
    }
  );
});
