"use strict";

const SCALAR_URLBAR = "browser.engagement.navigation.urlbar";

// The preference to enable suggestions in the urlbar.
const SUGGEST_URLBAR_PREF = "browser.urlbar.suggest.searches";
// The name of the search engine used to generate suggestions.
const SUGGESTION_ENGINE_NAME = "browser_UsageTelemetry usageTelemetrySearchSuggestions.xml";
const ONEOFF_URLBAR_PREF = "browser.urlbar.oneOffSearches";

let searchInAwesomebar = Task.async(function* (inputText, win = window) {
  yield new Promise(r => waitForFocus(r, win));
  // Write the search query in the urlbar.
  win.gURLBar.focus();
  win.gURLBar.value = inputText;
  win.gURLBar.controller.startSearch(inputText);
  // Wait for the popup to show.
  yield BrowserTestUtils.waitForEvent(win.gURLBar.popup, "popupshown");
  // And then for the search to complete.
  yield BrowserTestUtils.waitForCondition(() => win.gURLBar.controller.searchStatus >=
                                                Ci.nsIAutoCompleteController.STATUS_COMPLETE_NO_MATCH);
});

/**
 * Click one of the entries in the urlbar suggestion popup.
 *
 * @param {String} entryName
 *        The name of the elemet to click on.
 */
function clickURLBarSuggestion(entryName) {
  // The entry in the suggestion list should follow the format:
  // "<search term> <engine name> Search"
  const expectedSuggestionName = entryName + " " + SUGGESTION_ENGINE_NAME + " Search";
  for (let child of gURLBar.popup.richlistbox.children) {
    if (child.label === expectedSuggestionName) {
      // This entry is the search suggestion we're looking for.
      child.click();
      return;
    }
  }
}

add_task(function* setup() {
  // Create a new search engine.
  Services.search.addEngineWithDetails("MozSearch", "", "mozalias", "", "GET",
                                       "http://example.com/?q={searchTerms}");

  // Make it the default search engine.
  let engine = Services.search.getEngineByName("MozSearch");
  let originalEngine = Services.search.currentEngine;
  Services.search.currentEngine = engine;

  // And the first one-off engine.
  Services.search.moveEngine(engine, 0);

  // Enable search suggestions in the urlbar.
  Services.prefs.setBoolPref(SUGGEST_URLBAR_PREF, true);

  // Enable the urlbar one-off buttons.
  Services.prefs.setBoolPref(ONEOFF_URLBAR_PREF, true);

  // Enable Extended Telemetry.
  yield SpecialPowers.pushPrefEnv({"set": [["toolkit.telemetry.enabled", true]]});

  // Make sure to restore the engine once we're done.
  registerCleanupFunction(function* () {
    Services.search.currentEngine = originalEngine;
    Services.search.removeEngine(engine);
    Services.prefs.clearUserPref(SUGGEST_URLBAR_PREF, true);
    Services.prefs.clearUserPref(ONEOFF_URLBAR_PREF);
  });
});

add_task(function* test_simpleQuery() {
  // Let's reset the counts.
  Services.telemetry.clearScalars();
  let search_hist = getSearchCountsHistogram();

  let tab = yield BrowserTestUtils.openNewForegroundTab(gBrowser, "about:blank");

  info("Simulate entering a simple search.");
  let p = BrowserTestUtils.browserLoaded(tab.linkedBrowser);
  yield searchInAwesomebar("simple query");
  EventUtils.sendKey("return");
  yield p;

  // Check if the scalars contain the expected values.
  const scalars =
    Services.telemetry.snapshotKeyedScalars(Ci.nsITelemetry.DATASET_RELEASE_CHANNEL_OPTIN, false);
  checkKeyedScalar(scalars, SCALAR_URLBAR, "search_enter", 1);
  Assert.equal(Object.keys(scalars[SCALAR_URLBAR]).length, 1,
               "This search must only increment one entry in the scalar.");

  // Make sure SEARCH_COUNTS contains identical values.
  checkKeyedHistogram(search_hist, 'other-MozSearch.urlbar', 1);

  yield BrowserTestUtils.removeTab(tab);
});

add_task(function* test_searchAlias() {
  // Let's reset the counts.
  Services.telemetry.clearScalars();
  let search_hist = getSearchCountsHistogram();

  let tab = yield BrowserTestUtils.openNewForegroundTab(gBrowser, "about:blank");

  info("Search using a search alias.");
  let p = BrowserTestUtils.browserLoaded(tab.linkedBrowser);
  yield searchInAwesomebar("mozalias query");
  EventUtils.sendKey("return");
  yield p;

  // Check if the scalars contain the expected values.
  const scalars =
    Services.telemetry.snapshotKeyedScalars(Ci.nsITelemetry.DATASET_RELEASE_CHANNEL_OPTIN, false);
  checkKeyedScalar(scalars, SCALAR_URLBAR, "search_alias", 1);
  Assert.equal(Object.keys(scalars[SCALAR_URLBAR]).length, 1,
               "This search must only increment one entry in the scalar.");

  // Make sure SEARCH_COUNTS contains identical values.
  checkKeyedHistogram(search_hist, 'other-MozSearch.urlbar', 1);

  yield BrowserTestUtils.removeTab(tab);
});

add_task(function* test_oneOff() {
  // Let's reset the counts.
  Services.telemetry.clearScalars();
  let search_hist = getSearchCountsHistogram();

  let tab = yield BrowserTestUtils.openNewForegroundTab(gBrowser, "about:blank");

  info("Perform a one-off search using the first engine.");
  let p = BrowserTestUtils.browserLoaded(tab.linkedBrowser);
  yield searchInAwesomebar("query");

  info("Pressing Alt+Down to take us to the first one-off engine.");
  EventUtils.synthesizeKey("VK_DOWN", { altKey: true });
  EventUtils.sendKey("return");
  yield p;

  // Check if the scalars contain the expected values.
  const scalars =
    Services.telemetry.snapshotKeyedScalars(Ci.nsITelemetry.DATASET_RELEASE_CHANNEL_OPTIN, false);
  checkKeyedScalar(scalars, SCALAR_URLBAR, "search_oneoff", 1);
  Assert.equal(Object.keys(scalars[SCALAR_URLBAR]).length, 1,
               "This search must only increment one entry in the scalar.");

  // Make sure SEARCH_COUNTS contains identical values.
  checkKeyedHistogram(search_hist, 'other-MozSearch.urlbar', 1);

  yield BrowserTestUtils.removeTab(tab);
});

add_task(function* test_suggestion() {
  // Let's reset the counts.
  Services.telemetry.clearScalars();
  let search_hist = getSearchCountsHistogram();

  // Create an engine to generate search suggestions and add it as default
  // for this test.
  const url = getRootDirectory(gTestPath) + "usageTelemetrySearchSuggestions.xml";
  let suggestionEngine = yield new Promise((resolve, reject) => {
    Services.search.addEngine(url, null, "", false, {
      onSuccess(engine) { resolve(engine) },
      onError() { reject() }
    });
  });

  let previousEngine = Services.search.currentEngine;
  Services.search.currentEngine = suggestionEngine;

  let tab = yield BrowserTestUtils.openNewForegroundTab(gBrowser, "about:blank");

  info("Perform a one-off search using the first engine.");
  let p = BrowserTestUtils.browserLoaded(tab.linkedBrowser);
  yield searchInAwesomebar("query");
  info("Clicking the urlbar suggestion.");
  clickURLBarSuggestion("queryfoo");
  yield p;

  // Check if the scalars contain the expected values.
  const scalars =
    Services.telemetry.snapshotKeyedScalars(Ci.nsITelemetry.DATASET_RELEASE_CHANNEL_OPTIN, false);
  checkKeyedScalar(scalars, SCALAR_URLBAR, "search_suggestion", 1);
  Assert.equal(Object.keys(scalars[SCALAR_URLBAR]).length, 1,
               "This search must only increment one entry in the scalar.");

  // Make sure SEARCH_COUNTS contains identical values.
  checkKeyedHistogram(search_hist, 'other-' + suggestionEngine.name + '.urlbar', 1);

  Services.search.currentEngine = previousEngine;
  Services.search.removeEngine(suggestionEngine);
  yield BrowserTestUtils.removeTab(tab);
});
