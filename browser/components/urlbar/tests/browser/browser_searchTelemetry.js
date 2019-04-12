"use strict";

const SUGGEST_URLBAR_PREF = "browser.urlbar.suggest.searches";
const TEST_ENGINE_BASENAME = "searchSuggestionEngine.xml";

// Must run first.
add_task(async function prepare() {
  let suggestionsEnabled = Services.prefs.getBoolPref(SUGGEST_URLBAR_PREF);
  Services.prefs.setBoolPref(SUGGEST_URLBAR_PREF, true);
  let engine = await SearchTestUtils.promiseNewSearchEngine(
    getRootDirectory(gTestPath) + TEST_ENGINE_BASENAME);
  let oldDefaultEngine = await Services.search.getDefault();
  await Services.search.setDefault(engine);

  registerCleanupFunction(async function() {
    Services.prefs.setBoolPref(SUGGEST_URLBAR_PREF, suggestionsEnabled);
    await Services.search.setDefault(oldDefaultEngine);

    // Clicking urlbar results causes visits to their associated pages, so clear
    // that history now.
    await PlacesUtils.history.clear();
  });

  // Move the mouse away from the urlbar one-offs so that a one-off engine is
  // not inadvertently selected.
  await new Promise(resolve => {
    EventUtils.synthesizeNativeMouseMove(window.document.documentElement, 0, 0,
                                         resolve);
  });
});

add_task(async function heuristicResultMouse() {
  await compareCounts(async function() {
    let tab = await BrowserTestUtils.openNewForegroundTab(gBrowser);
    gURLBar.focus();
    await promiseAutocompleteResultPopup("heuristicResult");
    let result = await UrlbarTestUtils.getDetailsOfResultAt(window, 0);
    Assert.equal(result.type, UrlbarUtils.RESULT_TYPE.SEARCH,
      "Should be of type search");
    let loadPromise = BrowserTestUtils.browserLoaded(tab.linkedBrowser);
    let element = await UrlbarTestUtils.waitForAutocompleteResultAt(window, 0);
    EventUtils.synthesizeMouseAtCenter(element, {});
    await loadPromise;
    BrowserTestUtils.removeTab(tab);
  });
});

add_task(async function heuristicResultKeyboard() {
  await compareCounts(async function() {
    let tab = await BrowserTestUtils.openNewForegroundTab(gBrowser);
    gURLBar.focus();
    await promiseAutocompleteResultPopup("heuristicResult");
    let result = await UrlbarTestUtils.getDetailsOfResultAt(window, 0);
    Assert.equal(result.type, UrlbarUtils.RESULT_TYPE.SEARCH,
      "Should be of type search");
    let loadPromise = BrowserTestUtils.browserLoaded(tab.linkedBrowser);
    EventUtils.sendKey("return");
    await loadPromise;
    BrowserTestUtils.removeTab(tab);
  });
});

add_task(async function searchSuggestionMouse() {
  await compareCounts(async function() {
    let tab = await BrowserTestUtils.openNewForegroundTab(gBrowser);
    gURLBar.focus();
    await promiseAutocompleteResultPopup("searchSuggestion");
    let idx = await getFirstSuggestionIndex();
    Assert.greaterOrEqual(idx, 0, "there should be a first suggestion");
    let loadPromise = BrowserTestUtils.browserLoaded(tab.linkedBrowser);
    let element = await UrlbarTestUtils.waitForAutocompleteResultAt(window, idx);
    EventUtils.synthesizeMouseAtCenter(element, {});
    await loadPromise;
    BrowserTestUtils.removeTab(tab);
  });
});

add_task(async function searchSuggestionKeyboard() {
  await compareCounts(async function() {
    let tab = await BrowserTestUtils.openNewForegroundTab(gBrowser);
    gURLBar.focus();
    await promiseAutocompleteResultPopup("searchSuggestion");
    let idx = await getFirstSuggestionIndex();
    Assert.greaterOrEqual(idx, 0, "there should be a first suggestion");
    let loadPromise = BrowserTestUtils.browserLoaded(tab.linkedBrowser);
    while (idx--) {
      EventUtils.sendKey("down");
    }
    EventUtils.sendKey("return");
    await loadPromise;
    BrowserTestUtils.removeTab(tab);
  });
});

/**
 * This does three things: gets current telemetry/FHR counts, calls
 * clickCallback, gets telemetry/FHR counts again to compare them to the old
 * counts.
 *
 * @param {function} clickCallback Use this to open the urlbar popup and choose
 *   and click a result.
 */
async function compareCounts(clickCallback) {
  // Search events triggered by clicks (not the Return key in the urlbar) are
  // recorded in three places:
  // * Telemetry histogram named "SEARCH_COUNTS"
  // * FHR

  let engine = await Services.search.getDefault();
  let engineID = "org.mozilla.testsearchsuggestions";

  let histogramKey = engineID + ".urlbar";
  let histogram = Services.telemetry.getKeyedHistogramById("SEARCH_COUNTS");
  histogram.clear();

  // FHR -- first make sure the engine has an identifier so that FHR is happy.
  Object.defineProperty(engine.wrappedJSObject, "identifier",
                        { value: engineID });

  gURLBar.focus();
  await clickCallback();

  TelemetryTestUtils.assertKeyedHistogramSum(histogram, histogramKey, 1);
}

/**
 * Returns the index of the first search suggestion in the urlbar results.
 *
 * @returns {number} An index, or -1 if there are no search suggestions.
 */
async function getFirstSuggestionIndex() {
  const matchCount = UrlbarTestUtils.getResultCount(window);
  for (let i = 0; i < matchCount; i++) {
    let result = await UrlbarTestUtils.getDetailsOfResultAt(window, i);
    if (result.type == UrlbarUtils.RESULT_TYPE.SEARCH &&
        result.searchParams.suggestion) {
      return i;
    }
  }
  return -1;
}
