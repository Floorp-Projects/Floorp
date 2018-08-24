"use strict";

const SCALAR_URLBAR = "browser.engagement.navigation.urlbar";

// The preference to enable suggestions in the urlbar.
const SUGGEST_URLBAR_PREF = "browser.urlbar.suggest.searches";
// The name of the search engine used to generate suggestions.
const SUGGESTION_ENGINE_NAME = "browser_UsageTelemetry usageTelemetrySearchSuggestions.xml";
const ONEOFF_URLBAR_PREF = "browser.urlbar.oneOffSearches";

ChromeUtils.defineModuleGetter(this, "URLBAR_SELECTED_RESULT_TYPES",
                               "resource:///modules/BrowserUsageTelemetry.jsm");

ChromeUtils.defineModuleGetter(this, "URLBAR_SELECTED_RESULT_METHODS",
                               "resource:///modules/BrowserUsageTelemetry.jsm");

function checkHistogramResults(resultIndexes, expected, histogram) {
  for (let i = 0; i < resultIndexes.counts.length; i++) {
    if (i == expected) {
      Assert.equal(resultIndexes.counts[i], 1,
        `expected counts should match for ${histogram} index ${i}`);
    } else {
      Assert.equal(resultIndexes.counts[i], 0,
        `unexpected counts should be zero for ${histogram} index ${i}`);
    }
  }
}

let searchInAwesomebar = async function(inputText, win = window) {
  await new Promise(r => waitForFocus(r, win));
  // Write the search query in the urlbar.
  win.gURLBar.focus();
  win.gURLBar.value = inputText;

  // This is not strictly necessary, but some things, like clearing oneoff
  // buttons status, depend on actual input events that the user would normally
  // generate.
  let event = win.document.createEvent("Events");
  event.initEvent("input", true, true);
  win.gURLBar.dispatchEvent(event);
  win.gURLBar.controller.startSearch(inputText);

  // Wait for the popup to show.
  await BrowserTestUtils.waitForEvent(win.gURLBar.popup, "popupshown");
  // And then for the search to complete.
  await BrowserTestUtils.waitForCondition(() => win.gURLBar.controller.searchStatus >=
                                                Ci.nsIAutoCompleteController.STATUS_COMPLETE_NO_MATCH);
};

/**
 * Click one of the entries in the urlbar suggestion popup.
 *
 * @param {String} entryName
 *        The name of the elemet to click on.
 * @param {Number} button [optional]
 *        which button to click.
 */
function clickURLBarSuggestion(entryName, button = 1) {
  // The entry in the suggestion list should follow the format:
  // "<search term> <engine name> Search"
  const expectedSuggestionName = entryName + " " + SUGGESTION_ENGINE_NAME + " Search";
  return BrowserTestUtils.waitForCondition(() => {
    for (let child of gURLBar.popup.richlistbox.children) {
      if (child.label === expectedSuggestionName) {
        // This entry is the search suggestion we're looking for.
        if (button == 1)
          child.click();
        else if (button == 2) {
          EventUtils.synthesizeMouseAtCenter(child, {type: "mousedown", button: 2});
        }
        return true;
      }
    }
    return false;
  }, "Waiting for the expected suggestion to appear");
}

/**
 * Create an engine to generate search suggestions and add it as default
 * for this test.
 */
async function withNewSearchEngine(taskFn) {
  const url = getRootDirectory(gTestPath) + "usageTelemetrySearchSuggestions.xml";
  let suggestionEngine = await new Promise((resolve, reject) => {
    Services.search.addEngine(url, null, "", false, {
      onSuccess(engine) { resolve(engine); },
      onError() { reject(); }
    });
  });

  let previousEngine = Services.search.currentEngine;
  Services.search.currentEngine = suggestionEngine;

  try {
    await taskFn(suggestionEngine);
  } finally {
    Services.search.currentEngine = previousEngine;
    Services.search.removeEngine(suggestionEngine);
  }
}

add_task(async function setup() {
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
  let suggestionsEnabled = Services.prefs.getBoolPref(SUGGEST_URLBAR_PREF);
  Services.prefs.setBoolPref(SUGGEST_URLBAR_PREF, true);

  // Enable the urlbar one-off buttons.
  Services.prefs.setBoolPref(ONEOFF_URLBAR_PREF, true);

  // Enable local telemetry recording for the duration of the tests.
  let oldCanRecord = Services.telemetry.canRecordExtended;
  Services.telemetry.canRecordExtended = true;

  // Enable event recording for the events tested here.
  Services.telemetry.setEventRecordingEnabled("navigation", true);

  // Clear history so that history added by previous tests doesn't mess up this
  // test when it selects results in the urlbar.
  await PlacesUtils.history.clear();

  // Clear historical search suggestions to avoid interference from previous
  // tests.
  await SpecialPowers.pushPrefEnv({"set": [["browser.urlbar.maxHistoricalSearchSuggestions", 0]]});

  // Use the default matching bucket configuration.
  await SpecialPowers.pushPrefEnv({"set": [["browser.urlbar.matchBuckets", "general:5,suggestion:4"]]});

  // Make sure to restore the engine once we're done.
  registerCleanupFunction(async function() {
    Services.telemetry.canRecordExtended = oldCanRecord;
    Services.search.currentEngine = originalEngine;
    Services.search.removeEngine(engine);
    Services.prefs.setBoolPref(SUGGEST_URLBAR_PREF, suggestionsEnabled);
    Services.prefs.clearUserPref(ONEOFF_URLBAR_PREF);
    await PlacesUtils.history.clear();
    Services.telemetry.setEventRecordingEnabled("navigation", false);
  });
});

add_task(async function test_simpleQuery() {
  Services.telemetry.clearScalars();
  Services.telemetry.clearEvents();

  let resultIndexHist = getAndClearHistogram("FX_URLBAR_SELECTED_RESULT_INDEX");
  let resultTypeHist = getAndClearHistogram("FX_URLBAR_SELECTED_RESULT_TYPE");
  let resultIndexByTypeHist = getAndClearKeyedHistogram("FX_URLBAR_SELECTED_RESULT_INDEX_BY_TYPE");
  let resultMethodHist = getAndClearHistogram("FX_URLBAR_SELECTED_RESULT_METHOD");
  let search_hist = getAndClearKeyedHistogram("SEARCH_COUNTS");

  let tab = await BrowserTestUtils.openNewForegroundTab(gBrowser, "about:blank");

  info("Simulate entering a simple search.");
  let p = BrowserTestUtils.browserLoaded(tab.linkedBrowser);
  await searchInAwesomebar("simple query");
  EventUtils.synthesizeKey("KEY_Enter");
  await p;

  // Check if the scalars contain the expected values.
  const scalars = getParentProcessScalars(Ci.nsITelemetry.DATASET_RELEASE_CHANNEL_OPTIN, true, false);
  checkKeyedScalar(scalars, SCALAR_URLBAR, "search_enter", 1);
  Assert.equal(Object.keys(scalars[SCALAR_URLBAR]).length, 1,
               "This search must only increment one entry in the scalar.");

  // Make sure SEARCH_COUNTS contains identical values.
  checkKeyedHistogram(search_hist, "other-MozSearch.urlbar", 1);

  // Also check events.
  let events = Services.telemetry.snapshotEvents(Ci.nsITelemetry.DATASET_RELEASE_CHANNEL_OPTIN, false);
  events = (events.parent || []).filter(e => e[1] == "navigation" && e[2] == "search");
  checkEvents(events, [["navigation", "search", "urlbar", "enter", {engine: "other-MozSearch"}]]);

  // Check the histograms as well.
  let resultIndexes = resultIndexHist.snapshot();
  checkHistogramResults(resultIndexes, 0, "FX_URLBAR_SELECTED_RESULT_INDEX");

  let resultTypes = resultTypeHist.snapshot();
  checkHistogramResults(resultTypes,
    URLBAR_SELECTED_RESULT_TYPES.searchengine,
    "FX_URLBAR_SELECTED_RESULT_TYPE");

  let resultIndexByType = resultIndexByTypeHist.snapshot("searchengine");
  checkHistogramResults(resultIndexByType,
    0,
    "FX_URLBAR_SELECTED_RESULT_INDEX_BY_TYPE");

  let resultMethods = resultMethodHist.snapshot();
  checkHistogramResults(resultMethods,
    URLBAR_SELECTED_RESULT_METHODS.enter,
    "FX_URLBAR_SELECTED_RESULT_METHOD");

  BrowserTestUtils.removeTab(tab);
});

add_task(async function test_searchAlias() {
  Services.telemetry.clearScalars();
  Services.telemetry.clearEvents();

  let resultIndexHist = getAndClearHistogram("FX_URLBAR_SELECTED_RESULT_INDEX");
  let resultTypeHist = getAndClearHistogram("FX_URLBAR_SELECTED_RESULT_TYPE");
  let resultIndexByTypeHist = getAndClearKeyedHistogram("FX_URLBAR_SELECTED_RESULT_INDEX_BY_TYPE");
  let resultMethodHist = getAndClearHistogram("FX_URLBAR_SELECTED_RESULT_METHOD");
  let search_hist = getAndClearKeyedHistogram("SEARCH_COUNTS");

  let tab = await BrowserTestUtils.openNewForegroundTab(gBrowser, "about:blank");

  info("Search using a search alias.");
  let p = BrowserTestUtils.browserLoaded(tab.linkedBrowser);
  await searchInAwesomebar("mozalias query");
  EventUtils.synthesizeKey("KEY_Enter");
  await p;

  // Check if the scalars contain the expected values.
  const scalars = getParentProcessScalars(Ci.nsITelemetry.DATASET_RELEASE_CHANNEL_OPTIN, true, false);
  checkKeyedScalar(scalars, SCALAR_URLBAR, "search_alias", 1);
  Assert.equal(Object.keys(scalars[SCALAR_URLBAR]).length, 1,
               "This search must only increment one entry in the scalar.");

  // Make sure SEARCH_COUNTS contains identical values.
  checkKeyedHistogram(search_hist, "other-MozSearch.urlbar", 1);

  // Also check events.
  let events = Services.telemetry.snapshotEvents(Ci.nsITelemetry.DATASET_RELEASE_CHANNEL_OPTIN, false);
  events = (events.parent || []).filter(e => e[1] == "navigation" && e[2] == "search");
  checkEvents(events, [["navigation", "search", "urlbar", "alias", {engine: "other-MozSearch"}]]);

  // Check the histograms as well.
  let resultIndexes = resultIndexHist.snapshot();
  checkHistogramResults(resultIndexes, 0, "FX_URLBAR_SELECTED_RESULT_INDEX");

  let resultTypes = resultTypeHist.snapshot();
  checkHistogramResults(resultTypes,
    URLBAR_SELECTED_RESULT_TYPES.searchengine,
    "FX_URLBAR_SELECTED_RESULT_TYPE");

  let resultIndexByType = resultIndexByTypeHist.snapshot("searchengine");
  checkHistogramResults(resultIndexByType,
    0,
    "FX_URLBAR_SELECTED_RESULT_INDEX_BY_TYPE");

  let resultMethods = resultMethodHist.snapshot();
  checkHistogramResults(resultMethods,
    URLBAR_SELECTED_RESULT_METHODS.enter,
    "FX_URLBAR_SELECTED_RESULT_METHOD");

  BrowserTestUtils.removeTab(tab);
});

// Performs a search using the first result, a one-off button, and the Return
// (Enter) key.
add_task(async function test_oneOff_enter() {
  Services.telemetry.clearScalars();
  Services.telemetry.clearEvents();

  let resultIndexHist = getAndClearHistogram("FX_URLBAR_SELECTED_RESULT_INDEX");
  let resultTypeHist = getAndClearHistogram("FX_URLBAR_SELECTED_RESULT_TYPE");
  let resultIndexByTypeHist = getAndClearKeyedHistogram("FX_URLBAR_SELECTED_RESULT_INDEX_BY_TYPE");
  let resultMethodHist = getAndClearHistogram("FX_URLBAR_SELECTED_RESULT_METHOD");
  let search_hist = getAndClearKeyedHistogram("SEARCH_COUNTS");

  let tab = await BrowserTestUtils.openNewForegroundTab(gBrowser, "about:blank");

  info("Perform a one-off search using the first engine.");
  let p = BrowserTestUtils.browserLoaded(tab.linkedBrowser);
  await searchInAwesomebar("query");

  info("Pressing Alt+Down to take us to the first one-off engine.");
  EventUtils.synthesizeKey("KEY_ArrowDown", { altKey: true });
  EventUtils.synthesizeKey("KEY_Enter");
  await p;

  // Check if the scalars contain the expected values.
  const scalars = getParentProcessScalars(Ci.nsITelemetry.DATASET_RELEASE_CHANNEL_OPTIN, true, false);
  checkKeyedScalar(scalars, SCALAR_URLBAR, "search_oneoff", 1);
  Assert.equal(Object.keys(scalars[SCALAR_URLBAR]).length, 1,
               "This search must only increment one entry in the scalar.");

  // Make sure SEARCH_COUNTS contains identical values.
  checkKeyedHistogram(search_hist, "other-MozSearch.urlbar", 1);

  // Also check events.
  let events = Services.telemetry.snapshotEvents(Ci.nsITelemetry.DATASET_RELEASE_CHANNEL_OPTIN, false);
  events = (events.parent || []).filter(e => e[1] == "navigation" && e[2] == "search");
  checkEvents(events, [["navigation", "search", "urlbar", "oneoff", {engine: "other-MozSearch"}]]);

  // Check the histograms as well.
  let resultIndexes = resultIndexHist.snapshot();
  checkHistogramResults(resultIndexes, 0, "FX_URLBAR_SELECTED_RESULT_INDEX");

  let resultTypes = resultTypeHist.snapshot();
  checkHistogramResults(resultTypes,
    URLBAR_SELECTED_RESULT_TYPES.searchengine,
    "FX_URLBAR_SELECTED_RESULT_TYPE");

  let resultIndexByType = resultIndexByTypeHist.snapshot("searchengine");
  checkHistogramResults(resultIndexByType,
    0,
    "FX_URLBAR_SELECTED_RESULT_INDEX_BY_TYPE");

  let resultMethods = resultMethodHist.snapshot();
  checkHistogramResults(resultMethods,
    URLBAR_SELECTED_RESULT_METHODS.enter,
    "FX_URLBAR_SELECTED_RESULT_METHOD");

  BrowserTestUtils.removeTab(tab);
});

// Performs a search using the second result, a one-off button, and the Return
// (Enter) key.  This only tests the FX_URLBAR_SELECTED_RESULT_METHOD histogram
// since test_oneOff_enter covers everything else.
add_task(async function test_oneOff_enterSelection() {
  Services.telemetry.clearScalars();
  let resultMethodHist = getAndClearHistogram("FX_URLBAR_SELECTED_RESULT_METHOD");

  await withNewSearchEngine(async function() {
    let tab = await BrowserTestUtils.openNewForegroundTab(gBrowser, "about:blank");

    info("Type a query. Suggestions should be generated by the test engine.");
    let p = BrowserTestUtils.browserLoaded(tab.linkedBrowser);
    await searchInAwesomebar("query");

    info("Select the second result, press Alt+Down to take us to the first one-off engine.");
    EventUtils.synthesizeKey("KEY_ArrowDown");
    EventUtils.synthesizeKey("KEY_ArrowDown", {altKey: true});
    EventUtils.synthesizeKey("KEY_Enter");
    await p;

    let resultMethods = resultMethodHist.snapshot();
    checkHistogramResults(resultMethods,
      URLBAR_SELECTED_RESULT_METHODS.arrowEnterSelection,
      "FX_URLBAR_SELECTED_RESULT_METHOD");

    BrowserTestUtils.removeTab(tab);
  });
});

// Performs a search using a click on a one-off button.  This only tests the
// FX_URLBAR_SELECTED_RESULT_METHOD histogram since test_oneOff_enter covers
// everything else.
add_task(async function test_oneOff_click() {
  Services.telemetry.clearScalars();

  let resultMethodHist = getAndClearHistogram("FX_URLBAR_SELECTED_RESULT_METHOD");

  let tab = await BrowserTestUtils.openNewForegroundTab(gBrowser, "about:blank");

  info("Type a query.");
  let p = BrowserTestUtils.browserLoaded(tab.linkedBrowser);
  await searchInAwesomebar("query");
  info("Click the first one-off button.");
  gURLBar.popup.oneOffSearchButtons.getSelectableButtons(false)[0].click();
  await p;

  let resultMethods = resultMethodHist.snapshot();
  checkHistogramResults(resultMethods,
    URLBAR_SELECTED_RESULT_METHODS.click,
    "FX_URLBAR_SELECTED_RESULT_METHOD");

  BrowserTestUtils.removeTab(tab);
});

// Clicks the first suggestion offered by the test search engine.
add_task(async function test_suggestion_click() {
  Services.telemetry.clearScalars();
  Services.telemetry.clearEvents();

  let resultIndexHist = getAndClearHistogram("FX_URLBAR_SELECTED_RESULT_INDEX");
  let resultTypeHist = getAndClearHistogram("FX_URLBAR_SELECTED_RESULT_TYPE");
  let resultIndexByTypeHist = getAndClearKeyedHistogram("FX_URLBAR_SELECTED_RESULT_INDEX_BY_TYPE");
  let resultMethodHist = getAndClearHistogram("FX_URLBAR_SELECTED_RESULT_METHOD");
  let search_hist = getAndClearKeyedHistogram("SEARCH_COUNTS");

  await withNewSearchEngine(async function(engine) {
    let tab = await BrowserTestUtils.openNewForegroundTab(gBrowser, "about:blank");

    info("Type a query. Suggestions should be generated by the test engine.");
    let p = BrowserTestUtils.browserLoaded(tab.linkedBrowser);
    await searchInAwesomebar("query");
    info("Clicking the urlbar suggestion.");
    await clickURLBarSuggestion("queryfoo");
    await p;

    // Check if the scalars contain the expected values.
    const scalars = getParentProcessScalars(Ci.nsITelemetry.DATASET_RELEASE_CHANNEL_OPTIN, true, false);
    checkKeyedScalar(scalars, SCALAR_URLBAR, "search_suggestion", 1);
    Assert.equal(Object.keys(scalars[SCALAR_URLBAR]).length, 1,
                "This search must only increment one entry in the scalar.");

    // Make sure SEARCH_COUNTS contains identical values.
    let searchEngineId = "other-" + engine.name;
    checkKeyedHistogram(search_hist, searchEngineId + ".urlbar", 1);

    // Also check events.
    let events = Services.telemetry.snapshotEvents(Ci.nsITelemetry.DATASET_RELEASE_CHANNEL_OPTIN, false);
    events = (events.parent || []).filter(e => e[1] == "navigation" && e[2] == "search");
    checkEvents(events, [["navigation", "search", "urlbar", "suggestion", {engine: searchEngineId}]]);

    // Check the histograms as well.
    let resultIndexes = resultIndexHist.snapshot();
    checkHistogramResults(resultIndexes, 3, "FX_URLBAR_SELECTED_RESULT_INDEX");

    let resultTypes = resultTypeHist.snapshot();
    checkHistogramResults(resultTypes,
      URLBAR_SELECTED_RESULT_TYPES.searchsuggestion,
      "FX_URLBAR_SELECTED_RESULT_TYPE");

    let resultIndexByType = resultIndexByTypeHist.snapshot("searchsuggestion");
    checkHistogramResults(resultIndexByType,
      3,
      "FX_URLBAR_SELECTED_RESULT_INDEX_BY_TYPE");

    let resultMethods = resultMethodHist.snapshot();
    checkHistogramResults(resultMethods,
      URLBAR_SELECTED_RESULT_METHODS.click,
      "FX_URLBAR_SELECTED_RESULT_METHOD");

    BrowserTestUtils.removeTab(tab);
  });
});

// Selects and presses the Return (Enter) key on the first suggestion offered by
// the test search engine.  This only tests the FX_URLBAR_SELECTED_RESULT_METHOD
// histogram since test_suggestion_click covers everything else.
add_task(async function test_suggestion_arrowEnterSelection() {
  Services.telemetry.clearScalars();
  let resultMethodHist = getAndClearHistogram("FX_URLBAR_SELECTED_RESULT_METHOD");

  await withNewSearchEngine(async function() {
    let tab = await BrowserTestUtils.openNewForegroundTab(gBrowser, "about:blank");

    info("Type a query. Suggestions should be generated by the test engine.");
    let p = BrowserTestUtils.browserLoaded(tab.linkedBrowser);
    await searchInAwesomebar("query");
    info("Select the second result and press Return.");
    EventUtils.synthesizeKey("KEY_ArrowDown");
    EventUtils.synthesizeKey("KEY_Enter");
    await p;

    let resultMethods = resultMethodHist.snapshot();
    checkHistogramResults(resultMethods,
      URLBAR_SELECTED_RESULT_METHODS.arrowEnterSelection,
      "FX_URLBAR_SELECTED_RESULT_METHOD");

    BrowserTestUtils.removeTab(tab);
  });
});

// Selects through tab and presses the Return (Enter) key on the first
// suggestion offered by the test search engine.
add_task(async function test_suggestion_tabEnterSelection() {
  Services.telemetry.clearScalars();
  let resultMethodHist = getAndClearHistogram("FX_URLBAR_SELECTED_RESULT_METHOD");

  await withNewSearchEngine(async function() {
    let tab = await BrowserTestUtils.openNewForegroundTab(gBrowser, "about:blank");

    info("Type a query. Suggestions should be generated by the test engine.");
    let p = BrowserTestUtils.browserLoaded(tab.linkedBrowser);
    await searchInAwesomebar("query");
    info("Select the second result and press Return.");
    EventUtils.synthesizeKey("KEY_Tab");
    EventUtils.synthesizeKey("KEY_Enter");
    await p;

    let resultMethods = resultMethodHist.snapshot();
    checkHistogramResults(resultMethods,
      URLBAR_SELECTED_RESULT_METHODS.tabEnterSelection,
      "FX_URLBAR_SELECTED_RESULT_METHOD");

    BrowserTestUtils.removeTab(tab);
  });
});

// Selects through code and presses the Return (Enter) key on the first
// suggestion offered by the test search engine.
add_task(async function test_suggestion_enterSelection() {
  Services.telemetry.clearScalars();
  let resultMethodHist = getAndClearHistogram("FX_URLBAR_SELECTED_RESULT_METHOD");

  await withNewSearchEngine(async function() {
    let tab = await BrowserTestUtils.openNewForegroundTab(gBrowser, "about:blank");

    info("Type a query. Suggestions should be generated by the test engine.");
    let p = BrowserTestUtils.browserLoaded(tab.linkedBrowser);
    await searchInAwesomebar("query");
    info("Select the second result and press Return.");
    gURLBar.popup.selectedIndex = 1;
    EventUtils.synthesizeKey("KEY_Enter");
    await p;

    let resultMethods = resultMethodHist.snapshot();
    checkHistogramResults(resultMethods,
      URLBAR_SELECTED_RESULT_METHODS.enterSelection,
      "FX_URLBAR_SELECTED_RESULT_METHOD");

    BrowserTestUtils.removeTab(tab);
  });
});

// Selects through mouse right button and press the Return (Enter) key.
add_task(async function test_suggestion_rightclick() {
  Services.telemetry.clearScalars();
  let resultMethodHist = getAndClearHistogram("FX_URLBAR_SELECTED_RESULT_METHOD");

  await withNewSearchEngine(async function() {
    let tab = await BrowserTestUtils.openNewForegroundTab(gBrowser, "about:blank");

    info("Type a query. Suggestions should be generated by the test engine.");
    let p = BrowserTestUtils.browserLoaded(tab.linkedBrowser);
    await searchInAwesomebar("query");
    info("Right click the the second result and then press Return.");
    await clickURLBarSuggestion("queryfoo", 2);
    EventUtils.synthesizeKey("KEY_Enter");
    await p;

    let resultMethods = resultMethodHist.snapshot();
    checkHistogramResults(resultMethods,
      URLBAR_SELECTED_RESULT_METHODS.rightClickEnter,
      "FX_URLBAR_SELECTED_RESULT_METHOD");

    BrowserTestUtils.removeTab(tab);
  });
});
