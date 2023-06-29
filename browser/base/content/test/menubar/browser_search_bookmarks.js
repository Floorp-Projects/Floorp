/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */
add_task(async function test_menu_search_bookmarks_with_window_open() {
  info("Opening bookmarks menu");
  let searchBookmarksMenuEntry = document.getElementById(
    "menu_searchBookmarks"
  );

  searchBookmarksMenuEntry.doCommand();

  await isUrlbarInBookmarksSearchMode(window);
});

add_task(async function test_menu_search_bookmarks_in_new_window() {
  info("Opening bookmarks menu");
  let newWindow = await BrowserUIUtils.openNewBrowserWindow();

  let searchBookmarksMenuEntry = document.getElementById(
    "menu_searchBookmarks"
  );

  info(`Executing command.`);
  searchBookmarksMenuEntry.doCommand();

  is(
    newWindow,
    BrowserWindowTracker.getTopWindow(),
    "New window is top window."
  );
  await isUrlbarInBookmarksSearchMode(newWindow);
  await BrowserTestUtils.closeWindow(newWindow);
});

async function isUrlbarInBookmarksSearchMode(targetWin) {
  await new Promise(resolve => {
    targetWin.gURLBar.controller.addQueryListener({
      onViewOpen() {
        targetWin.gURLBar.controller.removeQueryListener(this);
        resolve();
      },
    });
  });

  // Verify URLBar is in search mode with correct restriction
  is(targetWin != null, true, "top window is not empty");
  is(
    targetWin.gURLBar.searchMode?.source,
    UrlbarUtils.RESULT_SOURCE.BOOKMARKS,
    "Addressbar in correct mode."
  );
}
