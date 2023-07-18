/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

/**
 * Tests the Search History option from the menubar starts Address Bar search
 * mode for history.
 */
ChromeUtils.defineLazyGetter(this, "UrlbarTestUtils", () => {
  const { UrlbarTestUtils: module } = ChromeUtils.importESModule(
    "resource://testing-common/UrlbarTestUtils.sys.mjs"
  );
  module.init(this);
  return module;
});

add_task(async function test_menu_search_history_with_window_open() {
  info("Opening history menu");
  let searchHistoryMenuEntry = document.getElementById("menu_searchHistory");

  searchHistoryMenuEntry.doCommand();

  await isUrlbarInHistorySearchMode(window);
});

add_task(async function test_menu_search_history_opens_new_window() {
  let newWindowPromise = TestUtils.topicObserved(
    "browser-delayed-startup-finished"
  );

  info(
    "Executing command in untracked browser window (simulating non-browser window)."
  );
  BrowserWindowTracker.untrackForTestsOnly(window);
  let searchHistoryMenuEntry = document.getElementById("menu_searchHistory");
  searchHistoryMenuEntry.doCommand();
  BrowserWindowTracker.track(window);

  info("Waiting for new window to open.");
  let [newWindow] = await newWindowPromise;
  await isUrlbarInHistorySearchMode(newWindow);
  await BrowserTestUtils.closeWindow(newWindow);
});

async function isUrlbarInHistorySearchMode(targetWin) {
  is(
    targetWin,
    BrowserWindowTracker.getTopWindow(),
    "Target window is top window."
  );
  await UrlbarTestUtils.promisePopupOpen(targetWin, () => {});

  // Verify URLBar is in search mode with correct restriction
  let searchMode = UrlbarUtils.searchModeForToken("^");
  searchMode.entry = "historymenu";
  await UrlbarTestUtils.assertSearchMode(targetWin, searchMode);
}
