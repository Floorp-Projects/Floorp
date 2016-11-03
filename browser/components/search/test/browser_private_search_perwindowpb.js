// This test performs a search in a public window, then a different
// search in a private window, and then checks in the public window
// whether there is an autocomplete entry for the private search.

add_task(function* () {
  // Don't use about:home as the homepage for new windows
  Services.prefs.setIntPref("browser.startup.page", 0);
  registerCleanupFunction(() => Services.prefs.clearUserPref("browser.startup.page"));

  let windowsToClose = [];

  function performSearch(aWin, aIsPrivate) {
    let searchBar = aWin.BrowserSearch.searchBar;
    ok(searchBar, "got search bar");

    let loadPromise = BrowserTestUtils.browserLoaded(aWin.gBrowser.selectedBrowser);

    searchBar.value = aIsPrivate ? "private test" : "public test";
    searchBar.focus();
    EventUtils.synthesizeKey("VK_RETURN", {}, aWin);

    return loadPromise;
  }

  function* testOnWindow(aIsPrivate) {
    let win = yield BrowserTestUtils.openNewBrowserWindow({ private: aIsPrivate });
    yield SimpleTest.promiseFocus(win);
    windowsToClose.push(win);
    return win;
  }

  yield promiseNewEngine("426329.xml", { iconURL: "data:image/x-icon,%00" });

  let newWindow = yield* testOnWindow(false);
  yield performSearch(newWindow, false);

  newWindow = yield* testOnWindow(true);
  yield performSearch(newWindow, true);

  newWindow = yield* testOnWindow(false);

  let searchBar = newWindow.BrowserSearch.searchBar;
  searchBar.value = "p";
  searchBar.focus();

  let popup = searchBar.textbox.popup;
  let popupPromise = BrowserTestUtils.waitForEvent(popup, "popupshown");
  searchBar.textbox.showHistoryPopup();
  yield popupPromise;

  let entries = getMenuEntries(searchBar);
  for (let i = 0; i < entries.length; i++) {
    isnot(entries[i], "private test",
          "shouldn't see private autocomplete entries");
  }

  searchBar.textbox.toggleHistoryPopup();
  searchBar.value = "";

  windowsToClose.forEach(function(win) {
    win.close();
  });
});

function getMenuEntries(searchBar) {
  let entries = [];
  let autocompleteMenu = searchBar.textbox.popup;
  // Could perhaps pull values directly from the controller, but it seems
  // more reliable to test the values that are actually in the tree?
  let column = autocompleteMenu.tree.columns[0];
  let numRows = autocompleteMenu.tree.view.rowCount;
  for (let i = 0; i < numRows; i++) {
    entries.push(autocompleteMenu.tree.view.getValueAt(i, column));
  }
  return entries;
}
