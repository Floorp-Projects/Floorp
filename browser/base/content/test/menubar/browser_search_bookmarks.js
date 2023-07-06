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
  let newWindowPromise = TestUtils.topicObserved(
    "browser-delayed-startup-finished"
  );

  info("Executing command in untracked browser window.");
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
  await new Promise(resolve => {
    targetWin.gURLBar.controller.addQueryListener({
      onViewOpen() {
        targetWin.gURLBar.controller.removeQueryListener(this);
        resolve();
      },
    });
  });

  // Verify URLBar is in search mode with correct restriction
  is(
    targetWin.gURLBar.searchMode?.source,
    UrlbarUtils.RESULT_SOURCE.BOOKMARKS,
    "Addressbar in correct mode."
  );
}
