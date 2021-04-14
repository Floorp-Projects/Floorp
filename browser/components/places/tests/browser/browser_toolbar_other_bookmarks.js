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

  await setupBookmarksToolbar();

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

  info("Check the initial state of the Other Bookmarks folder.");
  let win = await BrowserTestUtils.openNewBrowserWindow();
  await setupBookmarksToolbar(win);
  ok(
    !win.document.getElementById("OtherBookmarks"),
    "Shouldn't have an Other Bookmarks button."
  );
  await BrowserTestUtils.closeWindow(win);

  info("Check visibility of an empty Other Bookmarks folder.");
  await testIsOtherBookmarksHidden(true);

  info("Ensure folder appears in toolbar when a new bookmark is added.");
  let bookmarks = await PlacesUtils.bookmarks.insertTree({
    guid: PlacesUtils.bookmarks.unfiledGuid,
    children: bookmarksInfo,
  });
  await testIsOtherBookmarksHidden(false);

  info("Ensure folder disappears from toolbar when no bookmarks are present.");
  await PlacesUtils.bookmarks.remove(bookmarks);
  await testIsOtherBookmarksHidden(true);
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
  await testIsOtherBookmarksHidden(true);

  info("Move toolbar bookmarks to Other Bookmarks folder.");
  await PlacesUtils.bookmarks.moveToFolder(
    bookmarks.map(b => b.guid),
    PlacesUtils.bookmarks.unfiledGuid,
    PlacesUtils.bookmarks.DEFAULT_INDEX
  );
  await testIsOtherBookmarksHidden(false);

  info("Move bookmarks from Other Bookmarks back to the toolbar.");
  await PlacesUtils.bookmarks.moveToFolder(
    bookmarks.map(b => b.guid),
    PlacesUtils.bookmarks.toolbarGuid,
    PlacesUtils.bookmarks.DEFAULT_INDEX
  );
  await testIsOtherBookmarksHidden(true);
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

  await testIsOtherBookmarksHidden(false);

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
  await setupBookmarksToolbar();

  await SpecialPowers.pushPrefEnv({
    set: [[BOOKMARKS_H2_2020_PREF, true]],
  });

  await PlacesUtils.bookmarks.insertTree({
    guid: PlacesUtils.bookmarks.unfiledGuid,
    children: bookmarksInfo,
  });

  await testIsOtherBookmarksHidden(false);

  // Test that moving the personal-bookmarks widget out of the
  // Bookmarks Toolbar will hide the "Other Bookmarks" folder.
  let widgetId = "personal-bookmarks";
  CustomizableUI.addWidgetToArea(widgetId, CustomizableUI.AREA_NAVBAR);
  await testIsOtherBookmarksHidden(true);

  CustomizableUI.reset();
  await testIsOtherBookmarksHidden(false);
});

// Test that the menu popup updates when menu items are deleted from it while
// it's open.
add_task(async function testDeletingMenuItems() {
  await setupBookmarksToolbar();

  await SpecialPowers.pushPrefEnv({
    set: [[BOOKMARKS_H2_2020_PREF, true]],
  });

  await PlacesUtils.bookmarks.insertTree({
    guid: PlacesUtils.bookmarks.unfiledGuid,
    children: bookmarksInfo,
  });

  await testIsOtherBookmarksHidden(false);

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
  let deleteMenuBookmark = document.getElementById(
    "placesContext_deleteBookmark"
  );
  placesContext.activateItem(deleteMenuBookmark);

  await TestUtils.waitForCondition(() => {
    let popup = document.querySelector("#OtherBookmarksPopup");
    let items = popup.querySelectorAll("menuitem");
    return items.length === 2;
  }, "Failed to delete bookmark menuitem. Expected 2 menu items after deletion.");
  ok(true, "Menu item was removed from the popup.");
  await closeMenuPopup("#OtherBookmarksPopup");
});

add_task(async function no_errors_when_bookmarks_placed_in_palette() {
  CustomizableUI.removeWidgetFromArea("personal-bookmarks");

  let consoleErrors = 0;

  let errorListener = {
    observe(error) {
      ok(false, `${error.message}, ${error.stack}, ${JSON.stringify(error)}`);
      consoleErrors++;
    },
  };
  Services.console.registerListener(errorListener);

  let bookmarks = await PlacesUtils.bookmarks.insertTree({
    guid: PlacesUtils.bookmarks.unfiledGuid,
    children: bookmarksInfo,
  });
  is(consoleErrors, 0, "There should be no console errors");

  Services.console.unregisterListener(errorListener);
  await PlacesUtils.bookmarks.remove(bookmarks);
  CustomizableUI.reset();
});

// Test "Show Other Bookmarks" menu item visibility in toolbar context menu.
add_task(async function testShowingOtherBookmarksContextMenuItem() {
  await setupBookmarksToolbar();

  await SpecialPowers.pushPrefEnv({
    set: [[BOOKMARKS_H2_2020_PREF, true]],
  });

  info("Add bookmark to Other Bookmarks.");
  let bookmark = await PlacesUtils.bookmarks.insertTree({
    guid: PlacesUtils.bookmarks.unfiledGuid,
    children: [{ title: "firefox", url: "http://example.com" }],
  });

  info("'Show Other Bookmarks' menu item should be checked by default.");
  await testOtherBookmarksCheckedState(true);

  info("Toggle off showing the Other Bookmarks folder.");
  await selectShowOtherBookmarksMenuItem();
  await testOtherBookmarksCheckedState(false);
  await testIsOtherBookmarksHidden(true);

  info("Toggle on showing the Other Bookmarks folder.");
  await selectShowOtherBookmarksMenuItem();
  await testOtherBookmarksCheckedState(true);
  await testIsOtherBookmarksHidden(false);

  info(
    "Ensure 'Show Other Bookmarks' isn't shown when Other Bookmarks is empty."
  );
  await PlacesUtils.bookmarks.remove(bookmark);
  await testIsOtherBookmarksMenuItemShown(false);

  info("Add a bookmark to the empty Other Bookmarks folder.");
  await PlacesUtils.bookmarks.insertTree({
    guid: PlacesUtils.bookmarks.unfiledGuid,
    children: [{ title: "firefox", url: "http://example.com" }],
  });
  await testIsOtherBookmarksMenuItemShown(true);

  info(
    "Ensure that displaying Other Bookmarks is consistent across separate windows."
  );
  let newWin = await BrowserTestUtils.openNewBrowserWindow();

  await TestUtils.waitForCondition(() => {
    let otherBookmarks = newWin.document.getElementById("OtherBookmarks");
    return otherBookmarks && !otherBookmarks.hidden;
  }, "Other Bookmarks folder failed to show in other window.");

  info("Hide the Other Bookmarks folder from the original window.");
  await selectShowOtherBookmarksMenuItem();

  await TestUtils.waitForCondition(() => {
    let otherBookmarks = newWin.document.getElementById("OtherBookmarks");
    return !otherBookmarks || otherBookmarks.hidden;
  }, "Other Bookmarks folder failed to be hidden in other window.");
  ok(true, "Other Bookmarks was successfully hidden in other window.");

  info("Show the Other Bookmarks folder from the original window.");
  await selectShowOtherBookmarksMenuItem();

  await TestUtils.waitForCondition(() => {
    let otherBookmarks = newWin.document.getElementById("OtherBookmarks");
    return otherBookmarks && !otherBookmarks.hidden;
  }, "Other Bookmarks folder failed to be shown in other window.");
  ok(true, "Other Bookmarks was successfully shown in other window.");

  await BrowserTestUtils.closeWindow(newWin);
});

// Test 'Show Other Bookmarks' isn't shown when pref is false.
add_task(async function showOtherBookmarksMenuItemPrefDisabled() {
  await setupBookmarksToolbar();
  await SpecialPowers.pushPrefEnv({
    set: [[BOOKMARKS_H2_2020_PREF, false]],
  });
  await testIsOtherBookmarksMenuItemShown(false);
});

// Test that node visibility for toolbar overflow is consisten when the "Other Bookmarks"
// folder is shown/hidden.
add_task(async function testOtherBookmarksToolbarOverFlow() {
  await setupBookmarksToolbar();
  await SpecialPowers.pushPrefEnv({
    set: [[BOOKMARKS_H2_2020_PREF, true]],
  });

  info(
    "Ensure that visible nodes when showing/hiding Other Bookmarks is consistent across separate windows."
  );
  // Add bookmarks to other bookmarks
  await PlacesUtils.bookmarks.insertTree({
    guid: PlacesUtils.bookmarks.unfiledGuid,
    children: [{ title: "firefox", url: "http://example.com" }],
  });

  let newWin = await BrowserTestUtils.openNewBrowserWindow();

  // Add bookmarks to the toolbar
  await PlacesUtils.bookmarks.insertTree({
    guid: PlacesUtils.bookmarks.toolbarGuid,
    children: Array(100)
      .fill("")
      .map((_, i) => ({ title: `test ${i}`, url: `http:example.com/${i}` })),
  });

  info("Hide the Other Bookmarks folder from the original window.");
  await selectShowOtherBookmarksMenuItem("#OtherBookmarks");
  await BrowserTestUtils.waitForEvent(
    document.getElementById("PersonalToolbar"),
    "BookmarksToolbarVisibilityUpdated"
  );
  ok(true, "Nodes successfully updated for both windows.");
  await testUpdatedNodeVisibility(newWin);

  info("Show the Other Bookmarks folder from the original window.");
  await selectShowOtherBookmarksMenuItem("#PlacesChevron");
  await BrowserTestUtils.waitForEvent(
    document.getElementById("PersonalToolbar"),
    "BookmarksToolbarVisibilityUpdated"
  );
  ok(true, "Nodes successfully updated for both windows.");
  await testUpdatedNodeVisibility(newWin);

  await BrowserTestUtils.closeWindow(newWin);
});

/**
 * Tests whether or not the "Other Bookmarks" folder is visible.
 *
 * @param {Boolean} expected
 *        The expected state of the Other Bookmarks folder. There are 3:
 *        - the folder node isn't initialized and is therefore not visible,
 *        - the folder node is initialized and is hidden
 *        - the folder node is initialized and is visible
 */
async function testIsOtherBookmarksHidden(expected) {
  info("Test whether or not the 'Other Bookmarks' folder is visible.");

  // Ensure the toolbar is visible.
  let toolbar = document.getElementById("PersonalToolbar");
  await promiseSetToolbarVisibility(toolbar, true);

  let otherBookmarks = document.getElementById("OtherBookmarks");

  await TestUtils.waitForCondition(() => {
    otherBookmarks = document.getElementById("OtherBookmarks");
    let isHidden = !otherBookmarks || otherBookmarks.hidden;
    return isHidden === expected;
  }, "Other Bookmarks folder failed to change hidden state.");

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
 * Test helper for checking the 'checked' state of the "Show Other Bookmarks" menu item
 * after selecting it from the context menu.
 *
 * @param {Boolean} expectedCheckedState
 *        Whether or not the menu item is checked.
 */
async function testOtherBookmarksCheckedState(expectedCheckedState) {
  info("Check 'Show Other Bookmarks' menu item state");
  await openToolbarContextMenu();

  let otherBookmarksMenuItem = document.querySelector(
    "#show-other-bookmarks_PersonalToolbar"
  );

  is(
    otherBookmarksMenuItem.getAttribute("checked"),
    `${expectedCheckedState}`,
    `Other Bookmarks item's checked state should be ${expectedCheckedState}`
  );

  await closeToolbarContextMenu();
}

/**
 * Test helper for checking whether or not the 'Show Other Bookmarks' menu item
 * appears in the toolbar's context menu.
 *
 * @param {Boolean} expected
 *        Whether or not the menu item appears in the toolbar conext menu.
 */
async function testIsOtherBookmarksMenuItemShown(expected) {
  await openToolbarContextMenu();

  let otherBookmarksMenuItem = document.querySelector(
    "#show-other-bookmarks_PersonalToolbar"
  );

  is(
    !!otherBookmarksMenuItem,
    expected,
    "'Show Other Bookmarks' menu item appearance state is correct."
  );

  await closeToolbarContextMenu();
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

/**
 * Helper for opening the toolbar context menu.
 *
 * @param {String}  toolbarSelector
 *        Optional. The selector for the toolbar context menu.
 *        Defaults to #PlacesToolbarItems.
 */
async function openToolbarContextMenu(toolbarSelector = "#PlacesToolbarItems") {
  let contextMenu = document.getElementById("placesContext");
  let toolbar = document.querySelector(toolbarSelector);
  let openToolbarContextMenuPromise = BrowserTestUtils.waitForPopupEvent(
    contextMenu,
    "shown"
  );

  // Use the end of the toolbar because the beginning (and even middle, on
  // some resolutions) might be occluded by the empty toolbar message, which
  // has a different context menu.
  let bounds = toolbar.getBoundingClientRect();
  EventUtils.synthesizeMouse(toolbar, bounds.width - 5, 5, {
    type: "contextmenu",
  });

  await openToolbarContextMenuPromise;
}

/**
 * Helper for closing the toolbar context menu.
 */
async function closeToolbarContextMenu() {
  let contextMenu = document.getElementById("placesContext");
  let closeToolbarContextMenuPromise = BrowserTestUtils.waitForPopupEvent(
    contextMenu,
    "hidden"
  );
  contextMenu.hidePopup();
  await closeToolbarContextMenuPromise;
}

/**
 * Helper for setting up the bookmarks toolbar state. This ensures the beginning
 * of a task will always have the bookmark toolbar in a state that makes the
 * Other Bookmarks folder testable.
 */
async function setupBookmarksToolbar(win = window) {
  let toolbar = win.document.getElementById("PersonalToolbar");
  let wasCollapsed = toolbar.collapsed;

  // Uncollapse the personal toolbar if needed.
  if (wasCollapsed) {
    await promiseSetToolbarVisibility(toolbar, true);
  }

  await PlacesUtils.bookmarks.eraseEverything();
}

/**
 * Helper for selecting the "Show Other Bookmarks" menu item from the bookmarks
 * toolbar context menu.
 *
 * @param {String}  selector
 *        Optional. The selector for the node that triggers showing the
 *        "Show Other Bookmarks" context menu item in the toolbar.
 *        Defaults to #PlacesToolbarItem when `openToolbarContextMenu` is
 *        called.
 */
async function selectShowOtherBookmarksMenuItem(selector) {
  info("Select 'Show Other Bookmarks' menu item");
  await openToolbarContextMenu(selector);

  let otherBookmarksMenuItem = document.querySelector(
    "#show-other-bookmarks_PersonalToolbar"
  );
  let contextMenu = document.getElementById("placesContext");

  contextMenu.activateItem(otherBookmarksMenuItem);

  await BrowserTestUtils.waitForPopupEvent(contextMenu, "hidden");
  await closeToolbarContextMenu();
}

/**
 * Test helper for node visibility in the bookmarks toolbar between two windows.
 *
 * @param {Window}  otherWin
 *        The other window whose toolbar items we want to compare with.
 */
function testUpdatedNodeVisibility(otherWin) {
  // Get visible toolbar nodes for both the current and other windows.
  let toolbarItems = document.getElementById("PlacesToolbarItems");
  let currentVisibleNodes = [];

  for (let node of toolbarItems.children) {
    if (node.style.visibility === "visible") {
      currentVisibleNodes.push(node);
    }
  }

  let otherToolbarItems = otherWin.document.getElementById(
    "PlacesToolbarItems"
  );
  let otherVisibleNodes = [];

  for (let node of otherToolbarItems.children) {
    if (node.style.visibility === "visible") {
      otherVisibleNodes.push(node);
    }
  }

  let lastIdx = otherVisibleNodes.length - 1;

  is(
    currentVisibleNodes[lastIdx]?.bookmarkGuid,
    otherVisibleNodes[lastIdx]?.bookmarkGuid,
    "Last visible toolbar bookmark is the same in both windows."
  );
}
