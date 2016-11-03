/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

var gNavBar = document.getElementById(CustomizableUI.AREA_NAVBAR);
var gOverflowList = document.getElementById(gNavBar.getAttribute("overflowtarget"));

const kBookmarksButton = "bookmarks-menu-button";
const kBookmarksItems = "personal-bookmarks";
const kOriginalWindowWidth = window.outerWidth;
const kSmallWidth = 400;

/**
 * Helper function that opens the bookmarks menu, and returns a Promise that
 * resolves as soon as the menu is ready for interaction.
 */
function bookmarksMenuPanelShown() {
  let deferred = Promise.defer();
  let bookmarksMenuPopup = document.getElementById("BMB_bookmarksPopup");
  let onTransitionEnd = (e) => {
    if (e.target == bookmarksMenuPopup) {
      bookmarksMenuPopup.removeEventListener("transitionend", onTransitionEnd);
      deferred.resolve();
    }
  }
  bookmarksMenuPopup.addEventListener("transitionend", onTransitionEnd);
  return deferred.promise;
}

/**
 * Checks that the placesContext menu is correctly attached to the
 * controller of some view. Returns a Promise that resolves as soon
 * as the context menu is closed.
 *
 * @param aItemWithContextMenu the item that we need to synthesize hte
 *        right click on in order to open the context menu.
 */
function checkPlacesContextMenu(aItemWithContextMenu) {
  return Task.spawn(function* () {
    let contextMenu = document.getElementById("placesContext");
    let newBookmarkItem = document.getElementById("placesContext_new:bookmark");
    info("Waiting for context menu on " + aItemWithContextMenu.id);
    let shownPromise = popupShown(contextMenu);
    EventUtils.synthesizeMouseAtCenter(aItemWithContextMenu,
                                       {type: "contextmenu", button: 2});
    yield shownPromise;

    ok(!newBookmarkItem.hasAttribute("disabled"),
       "New bookmark item shouldn't be disabled");

    info("Closing context menu");
    yield closePopup(contextMenu);
  });
}

/**
 * Opens the bookmarks menu panel, and then opens each of the "special"
 * submenus in that list. Then it checks that those submenu's context menus
 * are properly hooked up to a controller.
 */
function checkSpecialContextMenus() {
  return Task.spawn(function* () {
    let bookmarksMenuButton = document.getElementById(kBookmarksButton);
    let bookmarksMenuPopup = document.getElementById("BMB_bookmarksPopup");

    const kSpecialItemIDs = {
      "BMB_bookmarksToolbar": "BMB_bookmarksToolbarPopup",
      "BMB_unsortedBookmarks": "BMB_unsortedBookmarksPopup",
    };

    // Open the bookmarks menu button context menus and ensure that
    // they have the proper views attached.
    let shownPromise = bookmarksMenuPanelShown();
    let dropmarker = document.getAnonymousElementByAttribute(bookmarksMenuButton,
                                                             "anonid", "dropmarker");
    EventUtils.synthesizeMouseAtCenter(dropmarker, {});
    info("Waiting for bookmarks menu popup to show after clicking dropmarker.")
    yield shownPromise;

    for (let menuID in kSpecialItemIDs) {
      let menuItem = document.getElementById(menuID);
      let menuPopup = document.getElementById(kSpecialItemIDs[menuID]);
      info("Waiting to open menu for " + menuID);
      let shownPromise = popupShown(menuPopup);
      menuPopup.openPopup(menuItem, null, 0, 0, false, false, null);
      yield shownPromise;

      yield checkPlacesContextMenu(menuPopup);
      info("Closing menu for " + menuID);
      yield closePopup(menuPopup);
    }

    info("Closing bookmarks menu");
    yield closePopup(bookmarksMenuPopup);
  });
}

/**
 * Closes a focused popup by simulating pressing the Escape key,
 * and returns a Promise that resolves as soon as the popup is closed.
 *
 * @param aPopup the popup node to close.
 */
function closePopup(aPopup) {
  let hiddenPromise = popupHidden(aPopup);
  EventUtils.synthesizeKey("VK_ESCAPE", {});
  return hiddenPromise;
}

/**
 * Helper function that checks that the context menu of the
 * bookmark toolbar items chevron popup is correctly hooked up
 * to the controller of a view.
 */
function checkBookmarksItemsChevronContextMenu() {
  return Task.spawn(function*() {
    let chevronPopup = document.getElementById("PlacesChevronPopup");
    let shownPromise = popupShown(chevronPopup);
    let chevron = document.getElementById("PlacesChevron");
    EventUtils.synthesizeMouseAtCenter(chevron, {});
    info("Waiting for bookmark toolbar item chevron popup to show");
    yield shownPromise;
    yield waitForCondition(() => {
      for (let child of chevronPopup.children) {
        if (child.style.visibility != "hidden")
          return true;
      }
      return false;
    });
    yield checkPlacesContextMenu(chevronPopup);
    info("Waiting for bookmark toolbar item chevron popup to close");
    yield closePopup(chevronPopup);
  });
}

/**
 * Forces the window to a width that causes the nav-bar to overflow
 * its contents. Returns a Promise that resolves as soon as the
 * overflowable nav-bar is showing its chevron.
 */
function overflowEverything() {
  info("Waiting for overflow");
  window.resizeTo(kSmallWidth, window.outerHeight);
  return waitForCondition(() => gNavBar.hasAttribute("overflowing"));
}

/**
 * Returns the window to its original size from the start of the test,
 * and returns a Promise that resolves when the nav-bar is no longer
 * overflowing.
 */
function stopOverflowing() {
  info("Waiting until we stop overflowing");
  window.resizeTo(kOriginalWindowWidth, window.outerHeight);
  return waitForCondition(() => !gNavBar.hasAttribute("overflowing"));
}

/**
 * Checks that an item with ID aID is overflowing in the nav-bar.
 *
 * @param aID the ID of the node to check for overflowingness.
 */
function checkOverflowing(aID) {
  ok(!gNavBar.querySelector("#" + aID),
     "Item with ID " + aID + " should no longer be in the gNavBar");
  let item = gOverflowList.querySelector("#" + aID);
  ok(item, "Item with ID " + aID + " should be overflowing");
  is(item.getAttribute("overflowedItem"), "true",
     "Item with ID " + aID + " should have overflowedItem attribute");
}

/**
 * Checks that an item with ID aID is not overflowing in the nav-bar.
 *
 * @param aID the ID of hte node to check for non-overflowingness.
 */
function checkNotOverflowing(aID) {
  ok(!gOverflowList.querySelector("#" + aID),
     "Item with ID " + aID + " should no longer be overflowing");
  let item = gNavBar.querySelector("#" + aID);
  ok(item, "Item with ID " + aID + " should be in the nav bar");
  ok(!item.hasAttribute("overflowedItem"),
     "Item with ID " + aID + " should not have overflowedItem attribute");
}

/**
 * Test that overflowing the bookmarks menu button doesn't break the
 * context menus for the Unsorted and Bookmarks Toolbar menu items.
 */
add_task(function* testOverflowingBookmarksButtonContextMenu() {
  ok(!gNavBar.hasAttribute("overflowing"), "Should start with a non-overflowing toolbar.");
  ok(CustomizableUI.inDefaultState, "Should start in default state.");

  // Open the Unsorted and Bookmarks Toolbar context menus and ensure
  // that they have views attached.
  yield checkSpecialContextMenus();

  yield overflowEverything();
  checkOverflowing(kBookmarksButton);

  yield stopOverflowing();
  checkNotOverflowing(kBookmarksButton);

  yield checkSpecialContextMenus();
});

/**
 * Test that the bookmarks toolbar items context menu still works if moved
 * to the menu from the overflow panel, and then back to the toolbar.
 */
add_task(function* testOverflowingBookmarksItemsContextMenu() {
  info("Ensuring panel is ready.");
  yield PanelUI.ensureReady();

  let bookmarksToolbarItems = document.getElementById(kBookmarksItems);
  gCustomizeMode.addToToolbar(bookmarksToolbarItems);
  yield checkPlacesContextMenu(bookmarksToolbarItems);

  yield overflowEverything();
  checkOverflowing(kBookmarksItems)

  gCustomizeMode.addToPanel(bookmarksToolbarItems);

  yield stopOverflowing();

  gCustomizeMode.addToToolbar(bookmarksToolbarItems);
  yield checkPlacesContextMenu(bookmarksToolbarItems);
});

/**
 * Test that overflowing the bookmarks toolbar items doesn't cause the
 * context menu in the bookmarks toolbar items chevron to stop working.
 */
add_task(function* testOverflowingBookmarksItemsChevronContextMenu() {
  // If it's not already there, let's move the bookmarks toolbar items to
  // the nav-bar.
  let bookmarksToolbarItems = document.getElementById(kBookmarksItems);
  gCustomizeMode.addToToolbar(bookmarksToolbarItems);

  // We make the PlacesToolbarItems element be super tiny in order to force
  // the bookmarks toolbar items into overflowing and making the chevron
  // show itself.
  let placesToolbarItems = document.getElementById("PlacesToolbarItems");
  let placesChevron = document.getElementById("PlacesChevron");
  placesToolbarItems.style.maxWidth = "10px";
  info("Waiting for chevron to no longer be collapsed");
  yield waitForCondition(() => !placesChevron.collapsed);

  yield checkBookmarksItemsChevronContextMenu();

  yield overflowEverything();
  checkOverflowing(kBookmarksItems);

  yield stopOverflowing();
  checkNotOverflowing(kBookmarksItems);

  yield checkBookmarksItemsChevronContextMenu();

  placesToolbarItems.style.removeProperty("max-width");
});

add_task(function* asyncCleanup() {
  window.resizeTo(kOriginalWindowWidth, window.outerHeight);
  yield resetCustomization();
});
