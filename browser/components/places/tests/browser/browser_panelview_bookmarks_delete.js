/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

const { CustomizableUITestUtils } = ChromeUtils.import(
  "resource://testing-common/CustomizableUITestUtils.jsm"
);
let gCUITestUtils = new CustomizableUITestUtils(window);

const TEST_URL = "https://www.example.com/";

/**
 * Checks that the Bookmarks subview is updated after deleting an item.
 */
add_task(async function test_panelview_bookmarks_delete() {
  await PlacesUtils.bookmarks.insert({
    parentGuid: PlacesUtils.bookmarks.unfiledGuid,
    url: TEST_URL,
    title: TEST_URL,
  });

  await gCUITestUtils.openMainMenu();

  // The Proton AppMenu has the Bookmarks menu be a top-level item, but
  // the original AppMenu has it underneath the Library. We branch here
  // and open the Library if we're running with the old AppMenu.
  if (PanelUI.protonAppMenuEnabled) {
    document.getElementById("appMenu-bookmarks-button").click();
    let bookmarksView = document.getElementById("PanelUI-bookmarks");
    let promise = BrowserTestUtils.waitForEvent(bookmarksView, "ViewShown");
    await promise;
  } else {
    document.getElementById("appMenu-library-button").click();
    let libraryView = document.getElementById("appMenu-libraryView");
    let promise = BrowserTestUtils.waitForEvent(libraryView, "ViewShown");
    await promise;

    document.getElementById("appMenu-library-bookmarks-button").click();
    let bookmarksView = document.getElementById("PanelUI-bookmarks");
    promise = BrowserTestUtils.waitForEvent(bookmarksView, "ViewShown");
    await promise;
  }

  let list = document.getElementById("panelMenu_bookmarksMenu");
  let listItem = [...list.children].find(node => node.label == TEST_URL);

  let placesContext = document.getElementById("placesContext");
  let promise = BrowserTestUtils.waitForEvent(placesContext, "popupshown");
  EventUtils.synthesizeMouseAtCenter(listItem, {
    button: 2,
    type: "contextmenu",
  });
  await promise;

  promise = new Promise(resolve => {
    let observer = new MutationObserver(mutations => {
      if (listItem.parentNode == null) {
        Assert.ok(true, "The bookmarks list item was removed.");
        observer.disconnect();
        resolve();
      }
    });
    observer.observe(list, { childList: true });
  });
  let placesContextDelete = document.getElementById(
    "placesContext_deleteBookmark"
  );
  placesContext.activateItem(placesContextDelete, {});
  await promise;

  await gCUITestUtils.hideMainMenu();
});
