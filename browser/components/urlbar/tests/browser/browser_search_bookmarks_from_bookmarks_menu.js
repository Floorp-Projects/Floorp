/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

add_task(async function searchBookmarksFromBooksmarksMenu() {
  // Add Button to toolbar
  CustomizableUI.addWidgetToArea(
    "bookmarks-menu-button",
    CustomizableUI.AREA_NAVBAR,
    0
  );
  let bookmarksMenuButton = document.getElementById("bookmarks-menu-button");
  ok(bookmarksMenuButton, "Bookmarks Menu Button added");

  // Open Bookmarks-Menu-Popup
  let bookmarksMenuPopup = document.getElementById("BMB_bookmarksPopup");
  let PopupShownPromise = BrowserTestUtils.waitForEvent(
    bookmarksMenuPopup,
    "popupshown"
  );
  EventUtils.synthesizeMouseAtCenter(bookmarksMenuButton, {
    type: "mousedown",
  });
  await PopupShownPromise;
  ok(true, "Bookmarks Menu Popup shown");

  // Click on 'Search Bookmarks'
  let searchBookmarksButton = document.getElementById("BMB_searchBookmarks");
  ok(
    BrowserTestUtils.isVisible(
      searchBookmarksButton,
      "'Search Bookmarks Button' is visible."
    )
  );
  EventUtils.synthesizeMouseAtCenter(searchBookmarksButton, {});

  await new Promise(resolve => {
    window.gURLBar.controller.addQueryListener({
      onViewOpen() {
        window.gURLBar.controller.removeQueryListener(this);
        resolve();
      },
    });
  });

  // Verify URLBar is in search mode with correct restriction
  is(
    gURLBar.searchMode?.source,
    UrlbarUtils.RESULT_SOURCE.BOOKMARKS,
    "Addressbar in correct mode."
  );

  resetCUIAndReinitUrlbarInput();
});
