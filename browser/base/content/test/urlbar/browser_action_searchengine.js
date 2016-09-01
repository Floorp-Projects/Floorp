add_task(function* () {
  Services.search.addEngineWithDetails("MozSearch", "", "", "", "GET",
                                       "http://example.com/?q={searchTerms}");
  let engine = Services.search.getEngineByName("MozSearch");
  let originalEngine = Services.search.currentEngine;
  Services.search.currentEngine = engine;

  let tab = gBrowser.selectedTab = gBrowser.addTab("about:mozilla", {animate: false});
  yield promiseTabLoaded(gBrowser.selectedTab);

  registerCleanupFunction(() => {
    Services.search.currentEngine = originalEngine;
    let engine = Services.search.getEngineByName("MozSearch");
    Services.search.removeEngine(engine);

    try {
      gBrowser.removeTab(tab);
    } catch (ex) { /* tab may have already been closed in case of failure */ }

    return PlacesTestUtils.clearHistory();
  });

  yield promiseAutocompleteResultPopup("open a search");
  let result = gURLBar.popup.richlistbox.firstChild;

  isnot(result, null, "Should have a result");
  is(result.getAttribute("url"),
     `moz-action:searchengine,{"engineName":"MozSearch","input":"open%20a%20search","searchQuery":"open%20a%20search"}`,
     "Result should be a moz-action: for the correct search engine");
  is(result.hasAttribute("image"), false, "Result shouldn't have an image attribute");

  let tabPromise = promiseTabLoaded(gBrowser.selectedTab);
  result.click();
  yield tabPromise;

  is(gBrowser.selectedBrowser.currentURI.spec, "http://example.com/?q=open+a+search", "Correct URL should be loaded");
});
