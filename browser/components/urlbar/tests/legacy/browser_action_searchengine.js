/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

add_task(async function() {
  Services.search.addEngineWithDetails("MozSearch", "", "", "", "GET",
                                       "http://example.com/?q={searchTerms}");
  let engine = Services.search.getEngineByName("MozSearch");
  let originalEngine = Services.search.defaultEngine;
  Services.search.defaultEngine = engine;

  let tab = await BrowserTestUtils.openNewForegroundTab(gBrowser, "about:mozilla");

  registerCleanupFunction(async function() {
    Services.search.defaultEngine = originalEngine;
    Services.search.removeEngine(engine);
    try {
      BrowserTestUtils.removeTab(tab);
    } catch (ex) { /* tab may have already been closed in case of failure */ }
    await PlacesUtils.history.clear();
  });

  await promiseAutocompleteResultPopup("open a search");
  let result = await waitForAutocompleteResultAt(0);
  isnot(result, null, "Should have a result");
  Assert.deepEqual(
    PlacesUtils.parseActionUrl(result.getAttribute("url")),
    {
      type: "searchengine",
      params: {
        engineName: "MozSearch",
        input: "open a search",
        searchQuery: "open a search",
      },
    },
    "Result should be a moz-action: for the correct search engine"
  );
  is(result.hasAttribute("image"), false, "Result shouldn't have an image attribute");

  let tabPromise = BrowserTestUtils.browserLoaded(gBrowser.selectedBrowser);
  result.click();
  await tabPromise;

  is(gBrowser.selectedBrowser.currentURI.spec, "http://example.com/?q=open+a+search", "Correct URL should be loaded");

  gURLBar.popup.hidePopup();
  await promisePopupHidden(gURLBar.popup);
});
