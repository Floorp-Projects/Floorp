// This test performs a search in a public window, then a different
// search in a private window, and then checks in the public window
// whether there is an autocomplete entry for the private search.

add_setup(async function () {
  await gCUITestUtils.addSearchBar();
  await SearchTestUtils.promiseNewSearchEngine({
    url: getRootDirectory(gTestPath) + "426329.xml",
    setAsDefault: true,
  });

  registerCleanupFunction(async () => {
    gCUITestUtils.removeSearchBar();
  });
});

add_task(async function () {
  let windowsToClose = [];

  function performSearch(aWin, aIsPrivate) {
    let searchBar = aWin.BrowserSearch.searchBar;
    ok(searchBar, "got search bar");

    let loadPromise = BrowserTestUtils.browserLoaded(
      aWin.gBrowser.selectedBrowser
    );

    searchBar.value = aIsPrivate ? "private test" : "public test";
    searchBar.focus();
    EventUtils.synthesizeKey("VK_RETURN", {}, aWin);

    return loadPromise;
  }

  async function testOnWindow(aIsPrivate) {
    let win = await BrowserTestUtils.openNewBrowserWindow({
      private: aIsPrivate,
    });
    await SimpleTest.promiseFocus(win);
    windowsToClose.push(win);
    return win;
  }

  let newWindow = await testOnWindow(false);
  await performSearch(newWindow, false);

  newWindow = await testOnWindow(true);
  await performSearch(newWindow, true);

  newWindow = await testOnWindow(false);

  let searchBar = newWindow.BrowserSearch.searchBar;
  searchBar.value = "p";
  searchBar.focus();

  let popup = searchBar.textbox.popup;
  let popupPromise = BrowserTestUtils.waitForEvent(popup, "popupshown");
  searchBar.textbox.showHistoryPopup();
  await popupPromise;

  let entries = getMenuEntries(searchBar);
  for (let i = 0; i < entries.length; i++) {
    isnot(
      entries[i],
      "private test",
      "shouldn't see private autocomplete entries"
    );
  }

  searchBar.textbox.toggleHistoryPopup();
  searchBar.value = "";

  windowsToClose.forEach(function (win) {
    win.close();
  });
});

function getMenuEntries(searchBar) {
  // Could perhaps pull values directly from the controller, but it seems
  // more reliable to test the values that are actually in the richlistbox?
  return Array.from(searchBar.textbox.popup.richlistbox.itemChildren, item =>
    item.getAttribute("ac-value")
  );
}
