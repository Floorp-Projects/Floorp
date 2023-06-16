/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

/**
 * Tests that a search result has the correct attributes and visits the
 * expected URL for the engine.
 */

add_setup(async function () {
  await SpecialPowers.pushPrefEnv({
    set: [
      ["browser.search.separatePrivateDefault.ui.enabled", true],
      ["browser.search.separatePrivateDefault", false],
    ],
  });

  await SearchTestUtils.installSearchExtension(
    { name: "MozSearch" },
    { setAsDefault: true }
  );
  await SearchTestUtils.installSearchExtension({
    name: "MozSearchPrivate",
    search_url: "https://example.com/private",
  });

  registerCleanupFunction(async function () {
    await PlacesUtils.history.clear();
    await UrlbarTestUtils.formHistory.clear();
  });
});

async function testSearch(win, expectedName, expectedBaseUrl) {
  await UrlbarTestUtils.promiseAutocompleteResultPopup({
    window: win,
    value: "open a search",
  });
  let result = await UrlbarTestUtils.getDetailsOfResultAt(win, 0);

  Assert.equal(
    result.type,
    UrlbarUtils.RESULT_TYPE.SEARCH,
    "Should have type search"
  );
  Assert.deepEqual(
    result.searchParams,
    {
      engine: expectedName,
      keyword: undefined,
      query: "open a search",
      suggestion: undefined,
      inPrivateWindow: undefined,
      isPrivateEngine: undefined,
    },
    "Should have the correct result parameters."
  );

  Assert.equal(
    result.image,
    UrlbarUtils.ICON.SEARCH_GLASS,
    "Should have the search icon image"
  );

  let tabPromise = BrowserTestUtils.browserLoaded(win.gBrowser.selectedBrowser);
  let element = await UrlbarTestUtils.waitForAutocompleteResultAt(win, 0);
  EventUtils.synthesizeMouseAtCenter(element, {}, win);
  await tabPromise;

  Assert.equal(
    win.gBrowser.selectedBrowser.currentURI.spec,
    expectedBaseUrl + "?q=open+a+search",
    "Should have loaded the correct page"
  );
}

add_task(async function test_search_normal_window() {
  const tab = await BrowserTestUtils.openNewForegroundTab(
    gBrowser,
    "about:mozilla"
  );

  registerCleanupFunction(async function () {
    try {
      BrowserTestUtils.removeTab(tab);
    } catch (ex) {
      /* tab may have already been closed in case of failure */
    }
  });

  await testSearch(window, "MozSearch", "https://example.com/");
});

add_task(async function test_search_private_window_no_separate_default() {
  const win = await BrowserTestUtils.openNewBrowserWindow({ private: true });

  registerCleanupFunction(async function () {
    await BrowserTestUtils.closeWindow(win);
  });

  await testSearch(win, "MozSearch", "https://example.com/");
});

add_task(async function test_search_private_window() {
  await SpecialPowers.pushPrefEnv({
    set: [["browser.search.separatePrivateDefault", true]],
  });

  let engine = Services.search.getEngineByName("MozSearchPrivate");
  let originalEngine = await Services.search.getDefaultPrivate();
  await Services.search.setDefaultPrivate(
    engine,
    Ci.nsISearchService.CHANGE_REASON_UNKNOWN
  );

  registerCleanupFunction(async () => {
    await BrowserTestUtils.closeWindow(win);
    await Services.search.setDefaultPrivate(
      originalEngine,
      Ci.nsISearchService.CHANGE_REASON_UNKNOWN
    );
  });

  const win = await BrowserTestUtils.openNewBrowserWindow({ private: true });

  await testSearch(win, "MozSearchPrivate", "https://example.com/private");
});
