/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

/**
 * Tests that search result obtained using a search keyword gives an entry with
 * the correct attributes and visits the expected URL for the engine.
 */

add_task(async function() {
  await SearchTestUtils.installSearchExtension({ keyword: "moz" });
  let engine = Services.search.getEngineByName("Example");
  let originalEngine = await Services.search.getDefault();
  await Services.search.setDefault(engine);

  let tab = await BrowserTestUtils.openNewForegroundTab(
    gBrowser,
    "about:mozilla"
  );

  // Disable autofill so mozilla.org isn't autofilled below.
  await SpecialPowers.pushPrefEnv({
    set: [["browser.urlbar.autoFill", false]],
  });

  registerCleanupFunction(async function() {
    await Services.search.setDefault(originalEngine);
    try {
      BrowserTestUtils.removeTab(tab);
    } catch (ex) {
      /* tab may have already been closed in case of failure */
    }
    await PlacesUtils.history.clear();
    await UrlbarTestUtils.formHistory.clear();
  });

  await UrlbarTestUtils.promiseAutocompleteResultPopup({
    window,
    value: "moz",
  });
  Assert.equal(gURLBar.value, "moz", "Value should be unchanged");

  await UrlbarTestUtils.promiseAutocompleteResultPopup({
    window,
    value: "moz open a search",
  });
  // Wait for the second new search that starts when search mode is entered.
  await UrlbarTestUtils.promiseSearchComplete(window);
  await UrlbarTestUtils.assertSearchMode(window, {
    engineName: engine.name,
    entry: "typed",
  });
  Assert.equal(gURLBar.value, "open a search", "value should be query");

  let tabPromise = BrowserTestUtils.browserLoaded(gBrowser.selectedBrowser);
  EventUtils.synthesizeKey("KEY_Enter");
  await tabPromise;

  Assert.equal(
    gBrowser.selectedBrowser.currentURI.spec,
    "https://example.com/?q=open+a+search",
    "Should have loaded the correct page"
  );
});
