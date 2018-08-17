/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

ChromeUtils.import("resource://testing-common/CustomizableUITestUtils.jsm", this);
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

  let libraryView = document.getElementById("appMenu-libraryView");
  let promise = BrowserTestUtils.waitForEvent(libraryView, "ViewShown");
  document.getElementById("appMenu-library-button").click();
  await promise;

  let bookmarksView = document.getElementById("PanelUI-bookmarks");
  promise = BrowserTestUtils.waitForEvent(bookmarksView, "ViewShown");
  document.getElementById("appMenu-library-bookmarks-button").click();
  await promise;

  let list = document.getElementById("panelMenu_bookmarksMenu");
  let listItem = [...list.children].find(node => node.label == TEST_URL);

  let placesContext = document.getElementById("placesContext");
  promise = BrowserTestUtils.waitForEvent(placesContext, "popupshown");
  EventUtils.synthesizeMouseAtCenter(listItem, {
    button: 2,
    type: "contextmenu"
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
  let placesContextDelete = document.getElementById("placesContext_delete");
  EventUtils.synthesizeMouseAtCenter(placesContextDelete, {});
  await promise;

  await gCUITestUtils.hideMainMenu();
});
