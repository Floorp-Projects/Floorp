/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

/**
 * Tests that a search result has the correct attributes and visits the
 * expected URL for the engine.
 */

add_task(async function() {
  await Services.search.addEngineWithDetails("MozSearch", "", "", "", "GET",
    "http://example.com/?q={searchTerms}");
  let engine = Services.search.getEngineByName("MozSearch");
  let originalEngine = await Services.search.getDefault();
  await Services.search.setDefault(engine);

  let tab = await BrowserTestUtils.openNewForegroundTab(gBrowser, "about:mozilla");

  registerCleanupFunction(async function() {
    await Services.search.setDefault(originalEngine);
    await Services.search.removeEngine(engine);
    try {
      BrowserTestUtils.removeTab(tab);
    } catch (ex) { /* tab may have already been closed in case of failure */ }
    await PlacesUtils.history.clear();
  });

  await promiseAutocompleteResultPopup("open a search");
  let result = await UrlbarTestUtils.getDetailsOfResultAt(window, 0);

  Assert.equal(result.type, UrlbarUtils.RESULT_TYPE.SEARCH,
    "Should have type search");
  Assert.deepEqual(result.searchParams, {
      engine: "MozSearch",
      keyword: undefined,
      query: "open a search",
      suggestion: undefined,
  }, "Should have the correct result parameters.");

  if (UrlbarPrefs.get("quantumbar")) {
    Assert.equal(result.image, UrlbarUtils.ICON.SEARCH_GLASS,
      "Should have the search icon image");
  } else {
    Assert.equal(result.image, "", "Should not have an image defined");
  }

  let tabPromise = BrowserTestUtils.browserLoaded(gBrowser.selectedBrowser);
  let element = await UrlbarTestUtils.waitForAutocompleteResultAt(window, 0);
  EventUtils.synthesizeMouseAtCenter(element, {}, window);
  await tabPromise;

  Assert.equal(gBrowser.selectedBrowser.currentURI.spec,
    "http://example.com/?q=open+a+search", "Should have loaded the correct page");
});
