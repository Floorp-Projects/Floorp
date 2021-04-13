/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

/**
 * Tests that recently added bookmarks can be opened.
 */

const BASE_URL =
  "http://example.org/browser/browser/components/places/tests/browser/";
const bookmarkItems = [
  {
    url: `${BASE_URL}bookmark_dummy_1.html`,
    title: "Custom Title 1",
  },
  {
    url: `${BASE_URL}bookmark_dummy_2.html`,
    title: "Custom Title 2",
  },
];
let openedTabs = [];

async function openBookmarksPanelInLibraryToolbarButton() {
  let libraryBtn = document.getElementById("library-button");
  libraryBtn.click();
  let libView = document.getElementById("appMenu-libraryView");
  let viewShownPromise = BrowserTestUtils.waitForEvent(libView, "ViewShown");
  await viewShownPromise;

  let bookmarksButton;
  await BrowserTestUtils.waitForCondition(() => {
    bookmarksButton = document.getElementById(
      "appMenu-library-bookmarks-button"
    );
    return bookmarksButton;
  }, "Should have the library bookmarks button");
  bookmarksButton.click();

  let BookmarksView = document.getElementById("PanelUI-bookmarks");
  let viewRecentPromise = BrowserTestUtils.waitForEvent(
    BookmarksView,
    "ViewShown"
  );
  await viewRecentPromise;
}

async function openBookmarkedItemInNewTab(itemFromMenu) {
  let placesContext = document.getElementById("placesContext");
  let openContextMenuPromise = BrowserTestUtils.waitForEvent(
    placesContext,
    "popupshown"
  );
  EventUtils.synthesizeMouseAtCenter(itemFromMenu, {
    button: 2,
    type: "contextmenu",
  });
  await openContextMenuPromise;
  info("Opened context menu");

  let tabCreatedPromise = BrowserTestUtils.waitForNewTab(gBrowser, null, true);

  let openInNewTabOption = document.getElementById("placesContext_open:newtab");
  placesContext.activateItem(openInNewTabOption);
  info("Click open in new tab");

  let lastOpenedTab = await tabCreatedPromise;
  Assert.equal(
    lastOpenedTab.linkedBrowser.currentURI.spec,
    itemFromMenu._placesNode.uri,
    "Should have opened the correct URI"
  );
  openedTabs.push(lastOpenedTab);
}

async function closeLibraryMenu() {
  let libView = document.getElementById("appMenu-libraryView");
  let viewHiddenPromise = BrowserTestUtils.waitForEvent(libView, "ViewHiding");
  EventUtils.synthesizeKey("KEY_Escape", {}, window);
  await viewHiddenPromise;
}

async function closeTabs() {
  for (var i = 0; i < openedTabs.length; i++) {
    await gBrowser.removeTab(openedTabs[i]);
  }
  Assert.equal(gBrowser.tabs.length, 1, "Should close all opened tabs");
}

async function getRecentlyBookmarkedItems() {
  let historyMenu = document.getElementById("panelMenu_bookmarksMenu");
  let items = historyMenu.querySelectorAll("toolbarbutton");
  Assert.ok(items, "Recently bookmarked items should exists");

  await BrowserTestUtils.waitForCondition(
    () => items[0].attributes !== "undefined",
    "Custom bookmark exists"
  );

  if (items) {
    Assert.equal(
      items[0]._placesNode.uri,
      bookmarkItems[1].url,
      "Should match the expected url"
    );
    Assert.equal(
      items[0].getAttribute("label"),
      bookmarkItems[1].title,
      "Should be the expected title"
    );
    Assert.equal(
      items[1]._placesNode.uri,
      bookmarkItems[0].url,
      "Should match the expected url"
    );
    Assert.equal(
      items[1].getAttribute("label"),
      bookmarkItems[0].title,
      "Should be the expected title"
    );
  }
  return Array.from(items).slice(0, 2);
}

add_task(async function setup() {
  let libraryButton = CustomizableUI.getPlacementOfWidget("library-button");
  if (!libraryButton) {
    CustomizableUI.addWidgetToArea(
      "library-button",
      CustomizableUI.AREA_NAVBAR
    );
  }
  await PlacesUtils.bookmarks.insertTree({
    guid: PlacesUtils.bookmarks.menuGuid,
    children: bookmarkItems,
  });

  registerCleanupFunction(async () => {
    await PlacesUtils.bookmarks.eraseEverything();
    CustomizableUI.reset();
  });
});

add_task(async function test_recently_added() {
  await openBookmarksPanelInLibraryToolbarButton();

  let historyItems = await getRecentlyBookmarkedItems();

  for (let item of historyItems) {
    await openBookmarkedItemInNewTab(item);
  }

  await closeLibraryMenu();

  registerCleanupFunction(async () => {
    await closeTabs();
  });
});
