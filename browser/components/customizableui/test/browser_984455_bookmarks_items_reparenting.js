/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

var gNavBar = document.getElementById(CustomizableUI.AREA_NAVBAR);
var gOverflowList = document.getElementById(
  gNavBar.getAttribute("default-overflowtarget")
);

const kBookmarksButton = "bookmarks-menu-button";
const kBookmarksItems = "personal-bookmarks";
const kOriginalWindowWidth = window.outerWidth;

/**
 * Helper function that opens the bookmarks menu, and returns a Promise that
 * resolves as soon as the menu is ready for interaction.
 */
function bookmarksMenuPanelShown() {
  return new Promise(resolve => {
    let bookmarksMenuPopup = document.getElementById("BMB_bookmarksPopup");
    let onPopupShown = e => {
      if (e.target == bookmarksMenuPopup) {
        bookmarksMenuPopup.removeEventListener("popupshown", onPopupShown);
        resolve();
      }
    };
    bookmarksMenuPopup.addEventListener("popupshown", onPopupShown);
  });
}

/**
 * Checks that the placesContext menu is correctly attached to the
 * controller of some view. Returns a Promise that resolves as soon
 * as the context menu is closed.
 *
 * @param aItemWithContextMenu the item that we need to synthesize the
 *        right click on in order to open the context menu.
 */
function checkPlacesContextMenu(aItemWithContextMenu) {
  return (async function () {
    let contextMenu = document.getElementById("placesContext");
    let newBookmarkItem = document.getElementById("placesContext_new:bookmark");
    info("Waiting for context menu on " + aItemWithContextMenu.id);
    let shownPromise = popupShown(contextMenu);
    EventUtils.synthesizeMouseAtCenter(aItemWithContextMenu, {
      type: "contextmenu",
      button: 2,
    });
    await shownPromise;

    ok(
      !newBookmarkItem.hasAttribute("disabled"),
      "New bookmark item shouldn't be disabled"
    );

    info("Closing context menu");
    let hiddenPromise = popupHidden(contextMenu);
    // Use hidePopup instead of the closePopup helper because macOS native
    // context menus can't be closed by synthesized ESC in automation.
    contextMenu.hidePopup();
    await hiddenPromise;
  })();
}

/**
 * Opens the bookmarks menu panel, and then opens each of the "special"
 * submenus in that list. Then it checks that those submenu's context menus
 * are properly hooked up to a controller.
 */
function checkSpecialContextMenus() {
  return (async function () {
    let bookmarksMenuButton = document.getElementById(kBookmarksButton);
    let bookmarksMenuPopup = document.getElementById("BMB_bookmarksPopup");

    const kSpecialItemIDs = {
      BMB_bookmarksToolbar: "BMB_bookmarksToolbarPopup",
      BMB_unsortedBookmarks: "BMB_unsortedBookmarksPopup",
    };

    // Open the bookmarks menu button context menus and ensure that
    // they have the proper views attached.
    let shownPromise = bookmarksMenuPanelShown();

    EventUtils.synthesizeMouseAtCenter(bookmarksMenuButton, {});
    info("Waiting for bookmarks menu popup to show after clicking dropmarker.");
    await shownPromise;

    for (let menuID in kSpecialItemIDs) {
      let menuItem = document.getElementById(menuID);
      let menuPopup = document.getElementById(kSpecialItemIDs[menuID]);
      info("Waiting to open menu for " + menuID);
      shownPromise = popupShown(menuPopup);
      menuPopup.openPopup(menuItem, null, 0, 0, false, false, null);
      await shownPromise;

      await checkPlacesContextMenu(menuPopup);
      info("Closing menu for " + menuID);
      await closePopup(menuPopup);
    }

    info("Closing bookmarks menu");
    await closePopup(bookmarksMenuPopup);
  })();
}

/**
 * Closes a focused popup by simulating pressing the Escape key,
 * and returns a Promise that resolves as soon as the popup is closed.
 *
 * @param aPopup the popup node to close.
 */
function closePopup(aPopup) {
  let hiddenPromise = popupHidden(aPopup);
  EventUtils.synthesizeKey("KEY_Escape");
  return hiddenPromise;
}

/**
 * Helper function that checks that the context menu of the
 * bookmark toolbar items chevron popup is correctly hooked up
 * to the controller of a view.
 */
function checkBookmarksItemsChevronContextMenu() {
  return (async function () {
    let chevronPopup = document.getElementById("PlacesChevronPopup");
    let shownPromise = popupShown(chevronPopup);
    let chevron = document.getElementById("PlacesChevron");
    EventUtils.synthesizeMouseAtCenter(chevron, {});
    info("Waiting for bookmark toolbar item chevron popup to show");
    await shownPromise;
    await TestUtils.waitForCondition(() => {
      for (let child of chevronPopup.children) {
        if (child.style.visibility != "hidden") {
          return true;
        }
      }
      return false;
    });
    await checkPlacesContextMenu(chevronPopup);
    info("Waiting for bookmark toolbar item chevron popup to close");
    await closePopup(chevronPopup);
  })();
}

/**
 * Forces the window to a width that causes the nav-bar to overflow
 * its contents. Returns a Promise that resolves as soon as the
 * overflowable nav-bar is showing its chevron.
 */
function overflowEverything() {
  info("Waiting for overflow");
  let waitOverflowing = BrowserTestUtils.waitForMutationCondition(
    gNavBar,
    { attributes: true, attributeFilter: ["overflowing"] },
    () => gNavBar.hasAttribute("overflowing")
  );
  window.resizeTo(kForceOverflowWidthPx, window.outerHeight);
  return waitOverflowing;
}

/**
 * Returns the window to its original size from the start of the test,
 * and returns a Promise that resolves when the nav-bar is no longer
 * overflowing.
 */
function stopOverflowing() {
  info("Waiting until we stop overflowing");
  let waitOverflowing = BrowserTestUtils.waitForMutationCondition(
    gNavBar,
    { attributes: true, attributeFilter: ["overflowing"] },
    () => !gNavBar.hasAttribute("overflowing")
  );
  window.resizeTo(kOriginalWindowWidth, window.outerHeight);
  return waitOverflowing;
}

/**
 * Ensure bookmarks are visible on the toolbar.
 * @param {DOMWindow} win the browser window
 */
async function waitBookmarksToolbarIsUpdated(win = window) {
  await TestUtils.waitForCondition(
    async () => (await win.PlacesToolbarHelper.getIsEmpty()) === false,
    "Waiting for the Bookmarks toolbar to have been rebuilt and not be empty"
  );
  if (
    win.PlacesToolbarHelper._viewElt._placesView._updateNodesVisibilityTimer
  ) {
    await BrowserTestUtils.waitForEvent(
      win,
      "BookmarksToolbarVisibilityUpdated"
    );
  }
}

/**
 * Checks that an item with ID aID is overflowing in the nav-bar.
 *
 * @param aID the ID of the node to check for overflowingness.
 */
function checkOverflowing(aID) {
  ok(
    !gNavBar.querySelector("#" + aID),
    "Item with ID " + aID + " should no longer be in the gNavBar"
  );
  let item = gOverflowList.querySelector("#" + aID);
  ok(item, "Item with ID " + aID + " should be overflowing");
  is(
    item.getAttribute("overflowedItem"),
    "true",
    "Item with ID " + aID + " should have overflowedItem attribute"
  );
}

/**
 * Checks that an item with ID aID is not overflowing in the nav-bar.
 *
 * @param aID the ID of hte node to check for non-overflowingness.
 */
function checkNotOverflowing(aID) {
  ok(
    !gOverflowList.querySelector("#" + aID),
    "Item with ID " + aID + " should no longer be overflowing"
  );
  let item = gNavBar.querySelector("#" + aID);
  ok(item, "Item with ID " + aID + " should be in the nav bar");
  ok(
    !item.hasAttribute("overflowedItem"),
    "Item with ID " + aID + " should not have overflowedItem attribute"
  );
}

/**
 * Test that overflowing the bookmarks menu button doesn't break the
 * context menus for the Unsorted and Bookmarks Toolbar menu items.
 */
add_task(async function testOverflowingBookmarksButtonContextMenu() {
  ok(CustomizableUI.inDefaultState, "Should start in default state.");
  // The DevEdition has the DevTools button in the toolbar by default. Remove it
  // to prevent branch-specific available toolbar space.
  CustomizableUI.removeWidgetFromArea("developer-button");
  CustomizableUI.removeWidgetFromArea(
    "library-button",
    CustomizableUI.AREA_NAVBAR
  );
  CustomizableUI.addWidgetToArea(kBookmarksButton, CustomizableUI.AREA_NAVBAR);
  ok(
    !gNavBar.hasAttribute("overflowing"),
    "Should start with a non-overflowing toolbar."
  );

  // Open the Unsorted and Bookmarks Toolbar context menus and ensure
  // that they have views attached.
  await checkSpecialContextMenus();

  await overflowEverything();
  checkOverflowing(kBookmarksButton);

  await stopOverflowing();
  checkNotOverflowing(kBookmarksButton);

  await checkSpecialContextMenus();
});

/**
 * Test that the bookmarks toolbar items context menu still works if moved
 * to the menu from the overflow panel, and then back to the toolbar.
 */
add_task(async function testOverflowingBookmarksItemsContextMenu() {
  info("Ensuring panel is ready.");
  await PanelUI.ensureReady();

  let bookmarksToolbarItems = document.getElementById(kBookmarksItems);
  await gCustomizeMode.addToToolbar(bookmarksToolbarItems);
  await waitBookmarksToolbarIsUpdated();
  await checkPlacesContextMenu(bookmarksToolbarItems);

  await overflowEverything();
  checkOverflowing(kBookmarksItems);

  await gCustomizeMode.addToPanel(bookmarksToolbarItems);

  await stopOverflowing();

  await gCustomizeMode.addToToolbar(bookmarksToolbarItems);
  await waitBookmarksToolbarIsUpdated();
  await checkPlacesContextMenu(bookmarksToolbarItems);
});

/**
 * Test that overflowing the bookmarks toolbar items doesn't cause the
 * context menu in the bookmarks toolbar items chevron to stop working.
 */
add_task(async function testOverflowingBookmarksItemsChevronContextMenu() {
  // If it's not already there, let's move the bookmarks toolbar items to
  // the nav-bar.
  let bookmarksToolbarItems = document.getElementById(kBookmarksItems);
  await gCustomizeMode.addToToolbar(bookmarksToolbarItems);

  // We make the PlacesToolbarItems element be super tiny in order to force
  // the bookmarks toolbar items into overflowing and making the chevron
  // show itself.
  let placesToolbarItems = document.getElementById("PlacesToolbarItems");
  let placesChevron = document.getElementById("PlacesChevron");
  placesToolbarItems.style.maxWidth = "10px";
  info("Waiting for chevron to no longer be collapsed");
  await TestUtils.waitForCondition(() => !placesChevron.collapsed);

  await checkBookmarksItemsChevronContextMenu();

  await overflowEverything();
  checkOverflowing(kBookmarksItems);

  await stopOverflowing();
  checkNotOverflowing(kBookmarksItems);

  await waitBookmarksToolbarIsUpdated();
  await checkBookmarksItemsChevronContextMenu();

  placesToolbarItems.style.removeProperty("max-width");
});

add_task(async function asyncCleanup() {
  window.resizeTo(kOriginalWindowWidth, window.outerHeight);
  await resetCustomization();
});
