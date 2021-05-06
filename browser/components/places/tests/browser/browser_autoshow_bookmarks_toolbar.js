/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

const LOCATION_PREF = "browser.bookmarks.defaultLocation";
let bookmarkPanel;
let win;

add_task(async function setup() {
  await SpecialPowers.pushPrefEnv({
    set: [["browser.toolbars.bookmarks.2h2020", true]],
  });
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

  let hiddenPromise = promisePopupHidden(
    win.document.getElementById("editBookmarkPanel")
  );
  // Confirm and close the dialog.

  let guid = win.gEditItemOverlay._paneInfo.itemGuid;
  let promiseRemoved = PlacesTestUtils.waitForNotification(
    "bookmark-removed",
    events => events.some(e => e.guid == guid),
    "places"
  );
  win.document.getElementById("editBookmarkPanelRemoveButton").click();
  await hiddenPromise;
  await promiseRemoved;
}

/**
 * Test that if we create a bookmark on the toolbar, we show the
 * toolbar:
 */
add_task(async function test_new_on_toolbar() {
  await BrowserTestUtils.withNewTab(
    { gBrowser: win.gBrowser, url: "https://example.com/1" },
    async browser => {
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
    async browser => {
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

/**
 * Test that if we edit an existing bookmark to move it into the toolbar,
 * it gets shown.
 */
add_task(async function test_move_existing_to_toolbar() {
  await SpecialPowers.pushPrefEnv({
    set: [["browser.toolbars.bookmarks.2h2020", false]],
  });
  // Create the bookmark first:
  let bm = await PlacesUtils.bookmarks.insert({
    parentGuid: PlacesUtils.bookmarks.unfiledGuid,
    title: "Test for editing",
    url: "https://example.com/moving-test",
  });
  await BrowserTestUtils.withNewTab(
    { gBrowser: win.gBrowser, url: "https://example.com/moving-test" },
    async browser => {
      let toolbar = win.document.getElementById("PersonalToolbar");
      Assert.equal(
        toolbar.collapsed,
        true,
        "Bookmarks toolbar should start out collapsed."
      );
      await TestUtils.waitForCondition(
        () => win.BookmarkingUI.status == BookmarkingUI.STATUS_STARRED,
        "Page should be starred."
      );

      await clickBookmarkStar(win);

      let menuList = win.document.getElementById("editBMPanel_folderMenuList");
      let itemMovedPromise = PlacesTestUtils.waitForNotification(
        "bookmark-moved",
        events =>
          events.some(
            e =>
              e.parentGuid === PlacesUtils.bookmarks.toolbarGuid &&
              e.guid === bm.guid
          ),
        "places"
      );
      let promisePopup = BrowserTestUtils.waitForEvent(
        menuList.menupopup,
        "popupshown"
      );
      EventUtils.synthesizeMouseAtCenter(menuList, {}, win);
      await promisePopup;

      info("Clicking the toolbar folder item.");
      // Click the toolbar folder item.
      EventUtils.synthesizeMouseAtCenter(
        win.document.getElementById("editBMPanel_toolbarFolderItem"),
        {},
        win
      );

      await itemMovedPromise;
      await TestUtils.waitForCondition(
        () => !toolbar.collapsed,
        "Toolbar should be shown."
      );

      let expectedFolder = "BookmarksToolbarFolderTitle";
      let reason = "when moving a bookmark there.";
      await checkResponse({ showToolbar: true, expectedFolder, reason });
    }
  );
});
