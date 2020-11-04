/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
/* eslint-disable mozilla/no-arbitrary-setTimeout */

"use strict";

const BOOKMARKS_H2_2020_PREF = "browser.toolbars.bookmarks.2h2020";
const bookmarksInfo = [
  {
    title: "firefox",
    url: "http://example.com",
  },
  {
    title: "rules",
    url: "http://example.com/2",
  },
  {
    title: "yo",
    url: "http://example.com/2",
  },
];

/**
 * Test showing the "Other Bookmarks" folder in the bookmarks toolbar.
 */

// Setup.
add_task(async function setup() {
  let toolbar = document.getElementById("PersonalToolbar");
  let wasCollapsed = toolbar.collapsed;

  // Uncollapse the personal toolbar if needed.
  if (wasCollapsed) {
    await promiseSetToolbarVisibility(toolbar, true);
  }

  // Cleanup.
  registerCleanupFunction(async () => {
    // Collapse the personal toolbar if needed.
    if (wasCollapsed) {
      await promiseSetToolbarVisibility(toolbar, false);
    }
    await PlacesUtils.bookmarks.eraseEverything();
  });
});

// Test the "Other Bookmarks" folder is shown in the toolbar when
// bookmarks are stored under that folder.
add_task(async function testShowingOtherBookmarksInToolbar() {
  await SpecialPowers.pushPrefEnv({
    set: [[BOOKMARKS_H2_2020_PREF, true]],
  });

  info("Check visibility of an empty Other Bookmarks folder.");
  testIsOtherBookmarksHidden(true);

  info("Ensure folder appears in toolbar when a new bookmark is added.");
  let bookmarks = await PlacesUtils.bookmarks.insertTree({
    guid: PlacesUtils.bookmarks.unfiledGuid,
    children: bookmarksInfo,
  });
  testIsOtherBookmarksHidden(false);

  info("Ensure folder disappears from toolbar when no bookmarks are present.");
  await PlacesUtils.bookmarks.remove(bookmarks);
  testIsOtherBookmarksHidden(true);
});

// Test that folder visibility is correct when moving bookmarks to an empty
// "Other Bookmarks" folder and vice versa.
add_task(async function testOtherBookmarksVisibilityWhenMovingBookmarks() {
  await SpecialPowers.pushPrefEnv({
    set: [[BOOKMARKS_H2_2020_PREF, true]],
  });

  info("Add bookmarks to Bookmarks Toolbar.");
  let bookmarks = await PlacesUtils.bookmarks.insertTree({
    guid: PlacesUtils.bookmarks.toolbarGuid,
    children: bookmarksInfo,
  });
  testIsOtherBookmarksHidden(true);

  info("Move toolbar bookmarks to Other Bookmarks folder.");
  await PlacesUtils.bookmarks.moveToFolder(
    bookmarks.map(b => b.guid),
    PlacesUtils.bookmarks.unfiledGuid,
    PlacesUtils.bookmarks.DEFAULT_INDEX
  );
  testIsOtherBookmarksHidden(false);

  info("Move bookmarks from Other Bookmarks back to the toolbar.");
  await PlacesUtils.bookmarks.moveToFolder(
    bookmarks.map(b => b.guid),
    PlacesUtils.bookmarks.toolbarGuid,
    PlacesUtils.bookmarks.DEFAULT_INDEX
  );
  testIsOtherBookmarksHidden(true);
});

// Test OtherBookmarksPopup in toolbar.
add_task(async function testOtherBookmarksMenuPopup() {
  await SpecialPowers.pushPrefEnv({
    set: [[BOOKMARKS_H2_2020_PREF, true]],
  });

  info("Add bookmarks to Other Bookmarks folder.");
  let bookmarks = await PlacesUtils.bookmarks.insertTree({
    guid: PlacesUtils.bookmarks.unfiledGuid,
    children: bookmarksInfo,
  });

  testIsOtherBookmarksHidden(false);

  info("Check the popup menu has correct number of children.");
  await openMenuPopup("#OtherBookmarksPopup", "#OtherBookmarks");
  testNumberOfMenuPopupChildren("#OtherBookmarksPopup", 3);
  await closeMenuPopup("#OtherBookmarksPopup");

  info("Remove a bookmark.");
  await PlacesUtils.bookmarks.remove(bookmarks[0]);

  await openMenuPopup("#OtherBookmarksPopup", "#OtherBookmarks");
  testNumberOfMenuPopupChildren("#OtherBookmarksPopup", 2);
  await closeMenuPopup("#OtherBookmarksPopup");
});

// Test that folders in the Other Bookmarks folder expand
add_task(async function testFolderPopup() {
  await SpecialPowers.pushPrefEnv({
    set: [[BOOKMARKS_H2_2020_PREF, true]],
  });

  await PlacesUtils.bookmarks.insertTree({
    guid: PlacesUtils.bookmarks.unfiledGuid,
    children: [
      {
        title: "folder",
        type: PlacesUtils.bookmarks.TYPE_FOLDER,
        children: [
          {
            title: "example",
            url: "http://example.com/3",
          },
        ],
      },
    ],
  });

  info("Check for popup showing event when folder menuitem is selected.");
  await openMenuPopup("#OtherBookmarksPopup", "#OtherBookmarks");
  await openMenuPopup(
    "#OtherBookmarksPopup menu menupopup",
    "#OtherBookmarksPopup menu"
  );
  ok(true, "Folder menu stored in Other Bookmarks expands.");
  testNumberOfMenuPopupChildren("#OtherBookmarksPopup menu menupopup", 1);
  await closeMenuPopup("#OtherBookmarksPopup");
});

add_task(async function testOnlyShowOtherFolderInBookmarksToolbar() {
  testIsOtherBookmarksHidden(false);

  // Test that moving the personal-bookmarks widget out of the
  // Bookmarks Toolbar will hide the "Other Bookmarks" folder.
  let widgetId = "personal-bookmarks";
  CustomizableUI.addWidgetToArea(widgetId, CustomizableUI.AREA_NAVBAR);
  testIsOtherBookmarksHidden(true);

  CustomizableUI.reset();
  testIsOtherBookmarksHidden(false);
});

// Test that the menu popup updates when menu items are deleted from it while
// it's open.
add_task(async function testDeletingMenuItems() {
  await PlacesUtils.bookmarks.eraseEverything();

  await SpecialPowers.pushPrefEnv({
    set: [[BOOKMARKS_H2_2020_PREF, true]],
  });

  await PlacesUtils.bookmarks.insertTree({
    guid: PlacesUtils.bookmarks.unfiledGuid,
    children: bookmarksInfo,
  });

  await openMenuPopup("#OtherBookmarksPopup", "#OtherBookmarks");
  testNumberOfMenuPopupChildren("#OtherBookmarksPopup", 3);

  info("Open context menu for popup.");
  let placesContext = document.getElementById("placesContext");
  let popupEventPromise = BrowserTestUtils.waitForPopupEvent(
    placesContext,
    "shown"
  );
  let menuitem = document.querySelector("#OtherBookmarksPopup menuitem");
  EventUtils.synthesizeMouseAtCenter(menuitem, { type: "contextmenu" });
  await popupEventPromise;

  info("Delete bookmark menu item from popup.");
  let deleteMenuItem = document.getElementById("placesContext_delete");
  EventUtils.synthesizeMouseAtCenter(deleteMenuItem, {});

  await TestUtils.waitForCondition(() => {
    let popup = document.querySelector("#OtherBookmarksPopup");
    let items = popup.querySelectorAll("menuitem");
    return items.length === 2;
  }, "Failed to delete bookmark menuitem. Expected 2 menu items after deletion.");
  ok(true, "Menu item was removed from the popup.");
  await closeMenuPopup("#OtherBookmarksPopup");
});

/**
 * Tests whether or not the "Other Bookmarks" folder is visible.
 */
async function testIsOtherBookmarksHidden(expected) {
  info("Test whether or not the 'Other Bookmarks' folder is visible.");
  let otherBookmarks = document.getElementById("OtherBookmarks");

  await BrowserTestUtils.waitForAttribute("hidden", otherBookmarks, expected);

  ok(true, `Other Bookmarks folder "hidden" state should be ${expected}.`);
}

/**
 * Tests number of menu items in Other Bookmarks popup.
 *
 * @param {String}  selector
 *        The selector for getting the menupopup element we want to test.
 * @param {Number}  expected
 *        The expected number of menuitem elements inside the menupopup.
 */
function testNumberOfMenuPopupChildren(selector, expected) {
  let popup = document.querySelector(selector);
  let items = popup.querySelectorAll("menuitem");

  is(
    items.length,
    expected,
    `Number of menu items for ${selector} should be ${expected}.`
  );
}

/**
 * Helper for opening a menu popup.
 *
 * @param {String}  popupSelector
 *        The selector for the menupopup element we want to open.
 * @param {String}  targetSelector
 *        The selector for the element with the popup showing event.
 */
async function openMenuPopup(popupSelector, targetSelector) {
  let popup = document.querySelector(popupSelector);
  let target = document.querySelector(targetSelector);

  EventUtils.synthesizeMouseAtCenter(target, {});

  await BrowserTestUtils.waitForPopupEvent(popup, "shown");
}

/**
 * Helper for closing a menu popup.
 *
 * @param {String}  popupSelector
 *        The selector for the menupopup element we want to close.
 */
async function closeMenuPopup(popupSelector) {
  let popup = document.querySelector(popupSelector);

  info("Closing menu popup.");
  popup.hidePopup();
  await BrowserTestUtils.waitForPopupEvent(popup, "hidden");
}
