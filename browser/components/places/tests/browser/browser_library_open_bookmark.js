/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/
 */

/**
 * Test that the a bookmark can be opened from the Library by mouse double click.
 */
"use strict";

const TEST_URL = "about:buildconfig";

/**
 * Open a bookmark from the Library and check whether it is focused and where
 * the bookmark opens, based on preferences and/or the click event.
 *
 * @param {object} options Destructured parameters.
 * @param {boolean} [options.loadInBackground=false] Whether the pref to load
 *   bookmarks in the background is set.
 * @param {boolean} [options.loadInTabs=false] Whether the pref to load bookmarks
 *   in new tabs (as opposed to the current tab) is set.
 * @param {"current" | "tab"} [options.where="current"] Where the bookmark is
 *   expected to open (current tab or new tab).
 * @param {MouseEventOptions} [options.clickEvent={ clickCount: 2, button: 0 }]
 *   Options for the simulated bookmark click. Many properties are available.
 * @see {EventUtils.synthesizeMouse}
 * @see {nsIDOMWindowUtils::sendMouseEvent}
 */
async function doOpenBookmarkTest({
  loadInBackground = false,
  loadInTabs = false,
  where = "current",
  clickEvent = { clickCount: 2, button: 0 },
} = {}) {
  await SpecialPowers.pushPrefEnv({
    set: [
      ["browser.tabs.loadBookmarksInBackground", loadInBackground],
      ["browser.tabs.loadBookmarksInTabs", loadInTabs],
    ],
  });

  // If `where` is "current", open a tab that will be reused by the bookmark. If
  // it's "tab", open a tab that can't be reused, expecting a new tab to be
  // opened when the bookmark is clicked.
  let tab1 = await BrowserTestUtils.openNewForegroundTab(
    gBrowser,
    where === "tab" ? "about:about" : "about:blank"
  );
  let loadedPromise =
    where === "tab"
      ? BrowserTestUtils.waitForNewTab(gBrowser, TEST_URL, true)
      : BrowserTestUtils.browserLoaded(gBrowser, false, TEST_URL);

  let bm = await PlacesUtils.bookmarks.insert({
    parentGuid: PlacesUtils.bookmarks.unfiledGuid,
    url: TEST_URL,
    title: TEST_URL,
  });

  let gLibrary = await promiseLibrary("UnfiledBookmarks");
  gLibrary.focus();

  let bmLibrary = gLibrary.ContentTree.view.view.nodeForTreeIndex(0);
  Assert.equal(bmLibrary.title, bm.title, "Found bookmark in the right pane");

  gLibrary.ContentTree.view.selectNode(bmLibrary);
  synthesizeClickOnSelectedTreeCell(gLibrary.ContentTree.view, clickEvent);

  await loadedPromise;

  // Only expect the browser window to be focused if the bookmark was opened in
  // the foreground.
  if (loadInBackground) {
    is(
      Services.wm.getMostRecentWindow(null),
      gLibrary,
      "Library window is still focused"
    );
  } else {
    isnot(
      Services.wm.getMostRecentWindow(null),
      gLibrary,
      "Library window is not focused"
    );
  }

  BrowserTestUtils.removeTab(tab1);
  // There is only a tab to close if the bookmark was opened in a new tab.
  if (where === "tab") {
    BrowserTestUtils.removeTab(await loadedPromise);
  }
}

add_setup(async function () {
  registerCleanupFunction(async function () {
    for (let library of Services.wm.getEnumerator("Places:Organizer")) {
      await promiseLibraryClosed(library);
    }
    await PlacesUtils.bookmarks.eraseEverything();
  });
});

add_task(function test_open_bookmark_from_library() {
  return doOpenBookmarkTest();
});

add_task(
  /* Bug 1765440 - Open a bookmark from the library as a background tab, and
  confirm that the library remains focused. */
  function test_open_bookmark_in_background_from_library() {
    return doOpenBookmarkTest({
      where: "tab",
      loadInBackground: true,
      loadInTabs: true,
      clickEvent: { clickCount: 2, button: 0 },
    });
  }
);

add_task(
  /* Bug 1765440 - Open a bookmark from the library in a background tab via the
  middle mouse button, and confirm that the library remains focused. */
  function test_open_bookmark_in_background_from_library_middle_click() {
    return doOpenBookmarkTest({
      where: "tab",
      loadInBackground: true,
      loadInTabs: false,
      clickEvent: { clickCount: 1, button: 1 },
    });
  }
);
