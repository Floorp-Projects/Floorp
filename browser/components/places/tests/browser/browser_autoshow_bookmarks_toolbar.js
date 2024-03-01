/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

const LOCATION_PREF = "browser.bookmarks.defaultLocation";
const TOOLBAR_VISIBILITY_PREF = "browser.toolbars.bookmarks.visibility";
let bookmarkPanel;
let win;

add_setup(async function () {
  Services.prefs.clearUserPref(LOCATION_PREF);
  await PlacesUtils.bookmarks.eraseEverything();

  win = await BrowserTestUtils.openNewBrowserWindow();

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

/**
 * Helper to check we've shown the toolbar
 *
 * @param {object} options
 *   Options for the test
 * @param {boolean} options.showToolbar
 *   If the toolbar should be shown or not
 * @param {string} options.expectedFolder
 *   The expected folder to be shown
 * @param {string} options.reason
 *   The reason the toolbar should be shown
 */
async function checkResponse({ showToolbar, expectedFolder, reason }) {
  // Check folder.
  let menuList = win.document.getElementById("editBMPanel_folderMenuList");

  Assert.equal(
    menuList.label,
    PlacesUtils.getString(expectedFolder),
    `Should have ${expectedFolder} selected ${reason}.`
  );

  // Check toolbar:
  let toolbar = win.document.getElementById("PersonalToolbar");
  Assert.equal(
    !toolbar.collapsed,
    showToolbar,
    `Toolbar should be ${showToolbar ? "visible" : "hidden"} ${reason}.`
  );

  // Confirm and close the dialog.
  let hiddenPromise = promisePopupHidden(
    win.document.getElementById("editBookmarkPanel")
  );
  win.document.getElementById("editBookmarkPanelRemoveButton").click();
  await hiddenPromise;
}

/**
 * Test that if we create a bookmark on the toolbar, we show the
 * toolbar:
 */
add_task(async function test_new_on_toolbar() {
  await BrowserTestUtils.withNewTab(
    { gBrowser: win.gBrowser, url: "https://example.com/1" },
    async () => {
      let toolbar = win.document.getElementById("PersonalToolbar");
      Assert.equal(
        toolbar.collapsed,
        true,
        "Bookmarks toolbar should start out collapsed."
      );
      let shownPromise = promisePopupShown(
        win.document.getElementById("editBookmarkPanel")
      );
      win.document.getElementById("Browser:AddBookmarkAs").doCommand();
      await shownPromise;
      await TestUtils.waitForCondition(
        () => !toolbar.collapsed,
        "Toolbar should be shown."
      );
      let expectedFolder = "BookmarksToolbarFolderTitle";
      let reason = "when creating a bookmark there";
      await checkResponse({ showToolbar: true, expectedFolder, reason });
    }
  );
});

/**
 * Test that if we create a bookmark on the toolbar, we do not
 * show the toolbar if toolbar should never be shown:
 */
add_task(async function test_new_on_toolbar_never_show_toolbar() {
  await SpecialPowers.pushPrefEnv({
    set: [[TOOLBAR_VISIBILITY_PREF, "never"]],
  });

  await BrowserTestUtils.withNewTab(
    { gBrowser: win.gBrowser, url: "https://example.com/1" },
    async () => {
      let toolbar = win.document.getElementById("PersonalToolbar");
      Assert.equal(
        toolbar.collapsed,
        true,
        "Bookmarks toolbar should start out collapsed."
      );
      let shownPromise = promisePopupShown(
        win.document.getElementById("editBookmarkPanel")
      );
      win.document.getElementById("Browser:AddBookmarkAs").doCommand();
      await shownPromise;
      let expectedFolder = "BookmarksToolbarFolderTitle";
      let reason = "when the visibility pref is 'never'";
      await checkResponse({ showToolbar: false, expectedFolder, reason });
    }
  );

  await SpecialPowers.popPrefEnv();
});

/**
 * Test that if we edit an existing bookmark, we don't show the toolbar.
 */
add_task(async function test_existing_on_toolbar() {
  // Create the bookmark first:
  await PlacesUtils.bookmarks.insert({
    parentGuid: PlacesUtils.bookmarks.toolbarGuid,
    title: "Test for editing",
    url: "https://example.com/editing-test",
  });
  await BrowserTestUtils.withNewTab(
    { gBrowser: win.gBrowser, url: "https://example.com/editing-test" },
    async () => {
      await TestUtils.waitForCondition(
        () => win.BookmarkingUI.status == BookmarkingUI.STATUS_STARRED,
        "Page should be starred."
      );

      let toolbar = win.document.getElementById("PersonalToolbar");
      Assert.equal(
        toolbar.collapsed,
        true,
        "Bookmarks toolbar should start out collapsed."
      );
      await clickBookmarkStar(win);
      let expectedFolder = "BookmarksToolbarFolderTitle";
      let reason = "when editing a bookmark there";
      await checkResponse({ showToolbar: false, expectedFolder, reason });
    }
  );
});
