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

  let searchMode = UrlbarUtils.searchModeForToken("*");
  searchMode.entry = "bookmarkmenu";
  await UrlbarTestUtils.assertSearchMode(window, searchMode);
});

add_task(async function test_menu_search_bookmarks_in_new_window() {
  info("Opening bookmarks menu");
  let newWindow = await BrowserUIUtils.openNewBrowserWindow();

  let searchBookmarksMenuEntry = newWindow.document.getElementById(
    "menu_searchBookmarks"
  );

  info(`Executing command.`);
  searchBookmarksMenuEntry.doCommand();

  is(
    newWindow,
    BrowserWindowTracker.getTopWindow(),
    "New window is top window."
  );
  let searchMode = UrlbarUtils.searchModeForToken("*");
  searchMode.entry = "bookmarkmenu";
  await UrlbarTestUtils.assertSearchMode(newWindow, searchMode);
  await BrowserTestUtils.closeWindow(newWindow);
});
