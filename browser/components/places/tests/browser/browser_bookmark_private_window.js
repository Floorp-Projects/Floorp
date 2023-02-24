/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/
 */

/**
 * Test that the a website can be bookmarked from a private window.
 */
"use strict";

const TEST_URL = "about:buildconfig";

// Cleanup.
registerCleanupFunction(async () => {
  await PlacesUtils.bookmarks.eraseEverything();
});

add_task(async function test_add_bookmark_from_private_window() {
  let win = await BrowserTestUtils.openNewBrowserWindow({ private: true });
  let tab = await BrowserTestUtils.openNewForegroundTab(win.gBrowser, TEST_URL);

  registerCleanupFunction(async () => {
    BrowserTestUtils.removeTab(tab);
    await BrowserTestUtils.closeWindow(win);
  });

  // Open the bookmark panel.
  win.StarUI._createPanelIfNeeded();
  let bookmarkPanel = win.document.getElementById("editBookmarkPanel");
  let shownPromise = promisePopupShown(bookmarkPanel);
  let bookmarkAddedPromise = PlacesTestUtils.waitForNotification(
    "bookmark-added",
    events => events.some(({ url }) => url === TEST_URL)
  );
  let bookmarkStar = win.BookmarkingUI.star;
  bookmarkStar.click();
  await shownPromise;

  // Check if the bookmark star changes its state after click.
  Assert.equal(
    bookmarkStar.getAttribute("starred"),
    "true",
    "Bookmark star changed its state correctly."
  );

  // Close the bookmark panel.
  let hiddenPromise = promisePopupHidden(bookmarkPanel);
  let doneButton = win.document.getElementById("editBookmarkPanelDoneButton");
  doneButton.click();
  await hiddenPromise;
  await bookmarkAddedPromise;

  let bm = await PlacesUtils.bookmarks.fetch({ url: TEST_URL });
  Assert.equal(
    bm.url,
    TEST_URL,
    "The bookmark was successfully saved in the database."
  );
});
