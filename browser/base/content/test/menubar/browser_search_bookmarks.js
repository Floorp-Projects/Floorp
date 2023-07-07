/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

/**
 * Tests the Search Bookmarks option from the menubar starts Address Bar search
 * mode for bookmarks.
 */
ChromeUtils.defineLazyGetter(this, "UrlbarTestUtils", () => {
  const { UrlbarTestUtils: module } = ChromeUtils.importESModule(
    "resource://testing-common/UrlbarTestUtils.sys.mjs"
  );
  module.init(this);
  return module;
});

add_task(async function test_menu_search_bookmarks_with_window_open() {
  info("Opening bookmarks menu");
  let searchBookmarksMenuEntry = document.getElementById(
    "menu_searchBookmarks"
  );

  searchBookmarksMenuEntry.doCommand();

  await isUrlbarInBookmarksSearchMode(window);
});

add_task(async function test_menu_search_bookmarks_opens_new_window() {
  let newWindowPromise = TestUtils.topicObserved(
    "browser-delayed-startup-finished"
  );

  info(
    "Executing command in untracked browser window (simulating non-browser window)."
  );
  BrowserWindowTracker.untrackForTestsOnly(window);
  let searchBookmarksMenuEntry = document.getElementById(
    "menu_searchBookmarks"
  );
  searchBookmarksMenuEntry.doCommand();
  BrowserWindowTracker.track(window);

  info("Waiting for new window to open.");
  let [newWindow] = await newWindowPromise;
  await isUrlbarInBookmarksSearchMode(newWindow);
  await BrowserTestUtils.closeWindow(newWindow);
});

async function isUrlbarInBookmarksSearchMode(targetWin) {
  is(
    targetWin,
    BrowserWindowTracker.getTopWindow(),
    "Target window is top window."
  );
  await UrlbarTestUtils.promisePopupOpen(targetWin, () => {});

  // Verify URLBar is in search mode with correct restriction
  let searchMode = UrlbarUtils.searchModeForToken("*");
  searchMode.entry = "bookmarkmenu";
  await UrlbarTestUtils.assertSearchMode(targetWin, searchMode);
}
