const SUGGEST_URLBAR_PREF = "browser.urlbar.suggest.searches";
const TEST_ENGINE_BASENAME = "searchSuggestionEngine.xml";

// Must run first.
add_task(async function prepare() {
  let suggestionsEnabled = Services.prefs.getBoolPref(SUGGEST_URLBAR_PREF);
  Services.prefs.setBoolPref(SUGGEST_URLBAR_PREF, true);
  let engine = await promiseNewSearchEngine(TEST_ENGINE_BASENAME);
  let oldCurrentEngine = Services.search.currentEngine;
  Services.search.currentEngine = engine;
  registerCleanupFunction(async function() {
    Services.prefs.setBoolPref(SUGGEST_URLBAR_PREF, suggestionsEnabled);
    Services.search.currentEngine = oldCurrentEngine;

    // Clicking suggestions causes visits to search results pages, so clear that
    // history now.
    await PlacesTestUtils.clearHistory();

    // Make sure the popup is closed for the next test.
    gURLBar.blur();
    Assert.ok(!gURLBar.popup.popupOpen, "popup should be closed");
  });
});

add_task(async function clickSuggestion() {
  let tab = await BrowserTestUtils.openNewForegroundTab(gBrowser);
  gURLBar.focus();
  await promiseAutocompleteResultPopup("foo");
  let [idx, suggestion, engineName] = await promiseFirstSuggestion();
  Assert.equal(engineName,
               "browser_searchSuggestionEngine%20searchSuggestionEngine.xml",
               "Expected suggestion engine");
  let item = gURLBar.popup.richlistbox.getItemAtIndex(idx);

  let uri = Services.search.currentEngine.getSubmission(suggestion).uri;
  let loadPromise = BrowserTestUtils.browserLoaded(gBrowser.selectedBrowser,
                                                   false, uri.spec);
  item.click();
  await loadPromise;
  await BrowserTestUtils.removeTab(tab);
});

async function testPressEnterOnSuggestion(expectedUrl = null, keyModifiers = {}) {
  let tab = await BrowserTestUtils.openNewForegroundTab(gBrowser);
  gURLBar.focus();
  await promiseAutocompleteResultPopup("foo");
  let [idx, suggestion, engineName] = await promiseFirstSuggestion();
  Assert.equal(engineName,
               "browser_searchSuggestionEngine%20searchSuggestionEngine.xml",
               "Expected suggestion engine");

  if (!expectedUrl) {
    expectedUrl = Services.search.currentEngine.getSubmission(suggestion).uri.spec;
  }

  let promiseLoad = waitForDocLoadAndStopIt(expectedUrl);

  for (let i = 0; i < idx; ++i) {
    EventUtils.synthesizeKey("VK_DOWN", {});
  }
  EventUtils.synthesizeKey("VK_RETURN", keyModifiers);

  await promiseLoad;
  await BrowserTestUtils.removeTab(tab);
}

add_task(async function plainEnterOnSuggestion() {
  await testPressEnterOnSuggestion();
});

add_task(async function ctrlEnterOnSuggestion() {
  await testPressEnterOnSuggestion("http://www.foofoo.com/",
                                   AppConstants.platform === "macosx" ?
                                     { metaKey: true } :
                                     { ctrlKey: true });
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

async function promiseFirstSuggestion() {
  let tuple = [-1, null, null];
  await BrowserTestUtils.waitForCondition(() => {
    tuple = getFirstSuggestion();
    return tuple[0] >= 0;
  });
  return tuple;
}
