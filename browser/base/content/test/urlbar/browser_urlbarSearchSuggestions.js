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
  let tab = yield BrowserTestUtils.openNewForegroundTab(gBrowser);
  gURLBar.focus();
  yield promiseAutocompleteResultPopup("foo");
  let [idx, suggestion, engineName] = yield promiseFirstSuggestion();
  Assert.equal(engineName,
               "browser_searchSuggestionEngine%20searchSuggestionEngine.xml",
               "Expected suggestion engine");
  let item = gURLBar.popup.richlistbox.getItemAtIndex(idx);

  let uri = Services.search.currentEngine.getSubmission(suggestion).uri;
  let loadPromise = BrowserTestUtils.browserLoaded(gBrowser.selectedBrowser,
                                                   false, uri.spec);
  item.click();
  yield loadPromise;
  yield BrowserTestUtils.removeTab(tab);
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
  return [-1, null, null];
}

function* promiseFirstSuggestion() {
  let tuple = [-1, null, null];
  yield BrowserTestUtils.waitForCondition(() => {
    tuple = getFirstSuggestion();
    return tuple[0] >= 0;
  });
  return tuple;
}
