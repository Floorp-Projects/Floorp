const SUGGEST_URLBAR_PREF = "browser.urlbar.suggest.searches";
const TEST_ENGINE_BASENAME = "searchSuggestionEngine.xml";

// Must run first.
add_task(function* prepare() {
  Services.prefs.setBoolPref(SUGGEST_URLBAR_PREF, true);
  let engine = yield promiseNewSearchEngine(TEST_ENGINE_BASENAME);
  let oldCurrentEngine = Services.search.currentEngine;
  Services.search.currentEngine = engine;
  registerCleanupFunction(function* () {
    Services.prefs.clearUserPref(SUGGEST_URLBAR_PREF);
    Services.search.currentEngine = oldCurrentEngine;

    // Clicking suggestions causes visits to search results pages, so clear that
    // history now.
    yield PlacesTestUtils.clearHistory();

    // Make sure the popup is closed for the next test.
    gURLBar.blur();
    Assert.ok(!gURLBar.popup.popupOpen, "popup should be closed");
  });
});

add_task(function* clickSuggestion() {
  gBrowser.selectedTab = gBrowser.addTab();
  gURLBar.focus();
  yield promiseAutocompleteResultPopup("foo");
  let [idx, suggestion, engineName] = yield promiseFirstSuggestion();
  Assert.equal(engineName,
               "browser_searchSuggestionEngine%20searchSuggestionEngine.xml",
               "Expected suggestion engine");
  let item = gURLBar.popup.richlistbox.getItemAtIndex(idx);
  let loadPromise = promiseTabLoaded(gBrowser.selectedTab);
  item.click();
  yield loadPromise;
  let uri = Services.search.currentEngine.getSubmission(suggestion).uri;
  Assert.ok(uri.equals(gBrowser.currentURI),
            "The search results page should have loaded");
  gBrowser.removeTab(gBrowser.selectedTab);
});

function getFirstSuggestion() {
  let controller = gURLBar.popup.input.controller;
  let matchCount = controller.matchCount;
  for (let i = 0; i < matchCount; i++) {
    let url = controller.getValueAt(i);
    let mozActionMatch = url.match(/^moz-action:([^,]+),(.*)$/);
    if (mozActionMatch) {
      let [, type, paramStr] = mozActionMatch;
      let params = JSON.parse(paramStr);
      if (type == "searchengine" && "searchSuggestion" in params) {
        return [i, params.searchSuggestion, params.engineName];
      }
    }
  }
  return [-1, null];
}

function promiseFirstSuggestion() {
  return new Promise(resolve => {
    let tuple;
    waitForCondition(() => {
      tuple = getFirstSuggestion();
      return tuple[0] >= 0;
    }, () => resolve(tuple));
  });
}
