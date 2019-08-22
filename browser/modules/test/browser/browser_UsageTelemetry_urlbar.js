/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/
 */

/**
 * This file tests urlbar telemetry with search related actions.
 */

"use strict";

const SCALAR_URLBAR = "browser.engagement.navigation.urlbar";

// The preference to enable suggestions in the urlbar.
const SUGGEST_URLBAR_PREF = "browser.urlbar.suggest.searches";
// The name of the search engine used to generate suggestions.
const SUGGESTION_ENGINE_NAME =
  "browser_UsageTelemetry usageTelemetrySearchSuggestions.xml";
const ONEOFF_URLBAR_PREF = "browser.urlbar.oneOffSearches";

XPCOMUtils.defineLazyModuleGetters(this, {
  SearchTelemetry: "resource:///modules/SearchTelemetry.jsm",
  UrlbarTestUtils: "resource://testing-common/UrlbarTestUtils.jsm",
  URLBAR_SELECTED_RESULT_TYPES: "resource:///modules/BrowserUsageTelemetry.jsm",
  URLBAR_SELECTED_RESULT_METHODS:
    "resource:///modules/BrowserUsageTelemetry.jsm",
});

function searchInAwesomebar(value, win = window) {
  return UrlbarTestUtils.promiseAutocompleteResultPopup({
    window: win,
    waitForFocus,
    value,
    fireInputEvent: true,
  });
}

/**
 * Click one of the entries in the urlbar suggestion popup.
 *
 * @param {String} resultTitle
 *        The title of the result to click on.
 * @param {Number} button [optional]
 *        which button to click.
 */
async function clickURLBarSuggestion(resultTitle, button = 1) {
  await UrlbarTestUtils.promiseSearchComplete(window);

  const count = UrlbarTestUtils.getResultCount(window);
  for (let i = 0; i < count; i++) {
    let result = await UrlbarTestUtils.getDetailsOfResultAt(window, i);
    if (result.displayed.title == resultTitle) {
      // This entry is the search suggestion we're looking for.
      let element = await UrlbarTestUtils.waitForAutocompleteResultAt(
        window,
        i
      );
      if (button == 1) {
        EventUtils.synthesizeMouseAtCenter(element, {});
      } else if (button == 2) {
        EventUtils.synthesizeMouseAtCenter(element, {
          type: "mousedown",
          button: 2,
        });
      }
      return;
    }
  }
}

/**
 * Create an engine to generate search suggestions and add it as default
 * for this test.
 */
async function withNewSearchEngine(taskFn) {
  const url =
    getRootDirectory(gTestPath) + "usageTelemetrySearchSuggestions.xml";
  let suggestionEngine = await Services.search.addEngine(url, "", false);
  let previousEngine = await Services.search.getDefault();
  await Services.search.setDefault(suggestionEngine);

  try {
    await taskFn(suggestionEngine);
  } finally {
    await Services.search.setDefault(previousEngine);
    await Services.search.removeEngine(suggestionEngine);
  }
}

add_task(async function setup() {
  // Create a new search engine.
  await Services.search.addEngineWithDetails("MozSearch", {
    alias: "mozalias",
    method: "GET",
    template: "http://example.com/?q={searchTerms}",
  });

  // Make it the default search engine.
  let engine = Services.search.getEngineByName("MozSearch");
  let originalEngine = await Services.search.getDefault();
  await Services.search.setDefault(engine);

  // Give it some mock internal aliases.
  engine.wrappedJSObject.__internalAliases = ["@mozaliasfoo", "@mozaliasbar"];

  // And the first one-off engine.
  await Services.search.moveEngine(engine, 0);

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
  await SpecialPowers.pushPrefEnv({
    set: [["browser.urlbar.maxHistoricalSearchSuggestions", 0]],
  });

  // Use the default matching bucket configuration.
  await SpecialPowers.pushPrefEnv({
    set: [["browser.urlbar.matchBuckets", "general:5,suggestion:4"]],
  });

  // Make sure to restore the engine once we're done.
  registerCleanupFunction(async function() {
    Services.telemetry.canRecordExtended = oldCanRecord;
    await Services.search.setDefault(originalEngine);
    await Services.search.removeEngine(engine);
    Services.prefs.setBoolPref(SUGGEST_URLBAR_PREF, suggestionsEnabled);
    Services.prefs.clearUserPref(ONEOFF_URLBAR_PREF);
    await PlacesUtils.history.clear();
    Services.telemetry.setEventRecordingEnabled("navigation", false);
  });
});

add_task(async function test_simpleQuery() {
  Services.telemetry.clearScalars();
  Services.telemetry.clearEvents();

  let resultIndexHist = TelemetryTestUtils.getAndClearHistogram(
    "FX_URLBAR_SELECTED_RESULT_INDEX"
  );
  let resultTypeHist = TelemetryTestUtils.getAndClearHistogram(
    "FX_URLBAR_SELECTED_RESULT_TYPE"
  );
  let resultIndexByTypeHist = TelemetryTestUtils.getAndClearKeyedHistogram(
    "FX_URLBAR_SELECTED_RESULT_INDEX_BY_TYPE"
  );
  let resultMethodHist = TelemetryTestUtils.getAndClearHistogram(
    "FX_URLBAR_SELECTED_RESULT_METHOD"
  );
  let search_hist = TelemetryTestUtils.getAndClearKeyedHistogram(
    "SEARCH_COUNTS"
  );

  let tab = await BrowserTestUtils.openNewForegroundTab(
    gBrowser,
    "about:blank"
  );

  info("Simulate entering a simple search.");
  let p = BrowserTestUtils.browserLoaded(tab.linkedBrowser);
  await searchInAwesomebar("simple query");
  EventUtils.synthesizeKey("KEY_Enter");
  await p;

  // Check if the scalars contain the expected values.
  const scalars = TelemetryTestUtils.getProcessScalars("parent", true, false);
  TelemetryTestUtils.assertKeyedScalar(
    scalars,
    SCALAR_URLBAR,
    "search_enter",
    1
  );
  Assert.equal(
    Object.keys(scalars[SCALAR_URLBAR]).length,
    1,
    "This search must only increment one entry in the scalar."
  );

  // SEARCH_COUNTS should be incremented, but only the urlbar source since an
  // internal @search keyword was not used.
  TelemetryTestUtils.assertKeyedHistogramSum(
    search_hist,
    "other-MozSearch.urlbar",
    1
  );
  TelemetryTestUtils.assertKeyedHistogramSum(
    search_hist,
    "other-MozSearch.alias",
    undefined
  );

  // Also check events.
  TelemetryTestUtils.assertEvents(
    [
      [
        "navigation",
        "search",
        "urlbar",
        "enter",
        { engine: "other-MozSearch" },
      ],
    ],
    { category: "navigation", method: "search" }
  );

  // Check the histograms as well.
  TelemetryTestUtils.assertHistogram(resultIndexHist, 0, 1);

  TelemetryTestUtils.assertHistogram(
    resultTypeHist,
    URLBAR_SELECTED_RESULT_TYPES.searchengine,
    1
  );

  TelemetryTestUtils.assertKeyedHistogramValue(
    resultIndexByTypeHist,
    "searchengine",
    0,
    1
  );

  TelemetryTestUtils.assertHistogram(
    resultMethodHist,
    URLBAR_SELECTED_RESULT_METHODS.enter,
    1
  );

  BrowserTestUtils.removeTab(tab);
});

add_task(async function test_searchAlias() {
  Services.telemetry.clearScalars();
  Services.telemetry.clearEvents();

  let resultIndexHist = TelemetryTestUtils.getAndClearHistogram(
    "FX_URLBAR_SELECTED_RESULT_INDEX"
  );
  let resultTypeHist = TelemetryTestUtils.getAndClearHistogram(
    "FX_URLBAR_SELECTED_RESULT_TYPE"
  );
  let resultIndexByTypeHist = TelemetryTestUtils.getAndClearKeyedHistogram(
    "FX_URLBAR_SELECTED_RESULT_INDEX_BY_TYPE"
  );
  let resultMethodHist = TelemetryTestUtils.getAndClearHistogram(
    "FX_URLBAR_SELECTED_RESULT_METHOD"
  );
  let search_hist = TelemetryTestUtils.getAndClearKeyedHistogram(
    "SEARCH_COUNTS"
  );

  let tab = await BrowserTestUtils.openNewForegroundTab(
    gBrowser,
    "about:blank"
  );

  info("Search using a search alias.");
  let p = BrowserTestUtils.browserLoaded(tab.linkedBrowser);
  await searchInAwesomebar("mozalias query");
  EventUtils.synthesizeKey("KEY_Enter");
  await p;

  // Check if the scalars contain the expected values.
  const scalars = TelemetryTestUtils.getProcessScalars("parent", true, false);
  TelemetryTestUtils.assertKeyedScalar(
    scalars,
    SCALAR_URLBAR,
    "search_alias",
    1
  );
  Assert.equal(
    Object.keys(scalars[SCALAR_URLBAR]).length,
    1,
    "This search must only increment one entry in the scalar."
  );

  // SEARCH_COUNTS should be incremented, but only the urlbar source since an
  // internal @search keyword was not used.
  TelemetryTestUtils.assertKeyedHistogramSum(
    search_hist,
    "other-MozSearch.urlbar",
    1
  );
  TelemetryTestUtils.assertKeyedHistogramSum(
    search_hist,
    "other-MozSearch.alias",
    undefined
  );

  // Also check events.
  TelemetryTestUtils.assertEvents(
    [
      [
        "navigation",
        "search",
        "urlbar",
        "alias",
        { engine: "other-MozSearch" },
      ],
    ],
    { category: "navigation", method: "search" }
  );

  // Check the histograms as well.
  TelemetryTestUtils.assertHistogram(resultIndexHist, 0, 1);

  TelemetryTestUtils.assertHistogram(
    resultTypeHist,
    URLBAR_SELECTED_RESULT_TYPES.searchengine,
    1
  );

  TelemetryTestUtils.assertKeyedHistogramValue(
    resultIndexByTypeHist,
    "searchengine",
    0,
    1
  );

  TelemetryTestUtils.assertHistogram(
    resultMethodHist,
    URLBAR_SELECTED_RESULT_METHODS.enter,
    1
  );

  BrowserTestUtils.removeTab(tab);
});

add_task(async function test_internalSearchAlias() {
  let search_hist = TelemetryTestUtils.getAndClearKeyedHistogram(
    "SEARCH_COUNTS"
  );

  let tab = await BrowserTestUtils.openNewForegroundTab(
    gBrowser,
    "about:blank"
  );

  info("Search using an internal search alias.");
  let p = BrowserTestUtils.browserLoaded(tab.linkedBrowser);
  await searchInAwesomebar("@mozaliasfoo query");
  EventUtils.synthesizeKey("KEY_Enter");
  await p;

  // SEARCH_COUNTS should be incremented, but only the alias source since an
  // internal @search keyword was used.
  TelemetryTestUtils.assertKeyedHistogramSum(
    search_hist,
    "other-MozSearch.urlbar",
    undefined
  );
  TelemetryTestUtils.assertKeyedHistogramSum(
    search_hist,
    "other-MozSearch.alias",
    1
  );

  info("Search using the other internal search alias.");
  p = BrowserTestUtils.browserLoaded(tab.linkedBrowser);
  await searchInAwesomebar("@mozaliasbar query");
  EventUtils.synthesizeKey("KEY_Enter");
  await p;

  TelemetryTestUtils.assertKeyedHistogramSum(
    search_hist,
    "other-MozSearch.urlbar",
    undefined
  );
  TelemetryTestUtils.assertKeyedHistogramSum(
    search_hist,
    "other-MozSearch.alias",
    2
  );

  BrowserTestUtils.removeTab(tab);
});

// Performs a search using the first result, a one-off button, and the Return
// (Enter) key.
add_task(async function test_oneOff_enter() {
  Services.telemetry.clearScalars();
  Services.telemetry.clearEvents();

  let resultIndexHist = TelemetryTestUtils.getAndClearHistogram(
    "FX_URLBAR_SELECTED_RESULT_INDEX"
  );
  let resultTypeHist = TelemetryTestUtils.getAndClearHistogram(
    "FX_URLBAR_SELECTED_RESULT_TYPE"
  );
  let resultIndexByTypeHist = TelemetryTestUtils.getAndClearKeyedHistogram(
    "FX_URLBAR_SELECTED_RESULT_INDEX_BY_TYPE"
  );
  let resultMethodHist = TelemetryTestUtils.getAndClearHistogram(
    "FX_URLBAR_SELECTED_RESULT_METHOD"
  );
  let search_hist = TelemetryTestUtils.getAndClearKeyedHistogram(
    "SEARCH_COUNTS"
  );

  let tab = await BrowserTestUtils.openNewForegroundTab(
    gBrowser,
    "about:blank"
  );

  info("Perform a one-off search using the first engine.");
  let p = BrowserTestUtils.browserLoaded(tab.linkedBrowser);
  await searchInAwesomebar("query");

  info("Pressing Alt+Down to take us to the first one-off engine.");
  EventUtils.synthesizeKey("KEY_ArrowDown", { altKey: true });
  EventUtils.synthesizeKey("KEY_Enter");
  await p;

  // Check if the scalars contain the expected values.
  const scalars = TelemetryTestUtils.getProcessScalars("parent", true, false);
  TelemetryTestUtils.assertKeyedScalar(
    scalars,
    SCALAR_URLBAR,
    "search_oneoff",
    1
  );
  Assert.equal(
    Object.keys(scalars[SCALAR_URLBAR]).length,
    1,
    "This search must only increment one entry in the scalar."
  );

  // SEARCH_COUNTS should be incremented, but only the urlbar source since an
  // internal @search keyword was not used.
  TelemetryTestUtils.assertKeyedHistogramSum(
    search_hist,
    "other-MozSearch.urlbar",
    1
  );
  TelemetryTestUtils.assertKeyedHistogramSum(
    search_hist,
    "other-MozSearch.alias",
    undefined
  );

  // Also check events.
  TelemetryTestUtils.assertEvents(
    [
      [
        "navigation",
        "search",
        "urlbar",
        "oneoff",
        { engine: "other-MozSearch" },
      ],
    ],
    { category: "navigation", method: "search" }
  );

  // Check the histograms as well.
  TelemetryTestUtils.assertHistogram(resultIndexHist, 0, 1);

  TelemetryTestUtils.assertHistogram(
    resultTypeHist,
    URLBAR_SELECTED_RESULT_TYPES.searchengine,
    1
  );

  TelemetryTestUtils.assertKeyedHistogramValue(
    resultIndexByTypeHist,
    "searchengine",
    0,
    1
  );

  TelemetryTestUtils.assertHistogram(
    resultMethodHist,
    URLBAR_SELECTED_RESULT_METHODS.enter,
    1
  );

  BrowserTestUtils.removeTab(tab);
});

// Performs a search using the second result, a one-off button, and the Return
// (Enter) key.  This only tests the FX_URLBAR_SELECTED_RESULT_METHOD histogram
// since test_oneOff_enter covers everything else.
add_task(async function test_oneOff_enterSelection() {
  Services.telemetry.clearScalars();
  let resultMethodHist = TelemetryTestUtils.getAndClearHistogram(
    "FX_URLBAR_SELECTED_RESULT_METHOD"
  );

  await withNewSearchEngine(async function() {
    let tab = await BrowserTestUtils.openNewForegroundTab(
      gBrowser,
      "about:blank"
    );

    info("Type a query. Suggestions should be generated by the test engine.");
    let p = BrowserTestUtils.browserLoaded(tab.linkedBrowser);
    await searchInAwesomebar("query");

    info(
      "Select the second result, press Alt+Down to take us to the first one-off engine."
    );
    EventUtils.synthesizeKey("KEY_ArrowDown");
    EventUtils.synthesizeKey("KEY_ArrowDown", { altKey: true });
    EventUtils.synthesizeKey("KEY_Enter");
    await p;

    TelemetryTestUtils.assertHistogram(
      resultMethodHist,
      URLBAR_SELECTED_RESULT_METHODS.arrowEnterSelection,
      1
    );

    BrowserTestUtils.removeTab(tab);
  });
});

// Performs a search using a click on a one-off button.  This only tests the
// FX_URLBAR_SELECTED_RESULT_METHOD histogram since test_oneOff_enter covers
// everything else.
add_task(async function test_oneOff_click() {
  Services.telemetry.clearScalars();

  let resultMethodHist = TelemetryTestUtils.getAndClearHistogram(
    "FX_URLBAR_SELECTED_RESULT_METHOD"
  );

  let tab = await BrowserTestUtils.openNewForegroundTab(
    gBrowser,
    "about:blank"
  );

  info("Type a query.");
  let p = BrowserTestUtils.browserLoaded(tab.linkedBrowser);
  await searchInAwesomebar("query");
  info("Click the first one-off button.");
  UrlbarTestUtils.getOneOffSearchButtons(window)
    .getSelectableButtons(false)[0]
    .click();
  await p;

  TelemetryTestUtils.assertHistogram(
    resultMethodHist,
    URLBAR_SELECTED_RESULT_METHODS.click,
    1
  );

  BrowserTestUtils.removeTab(tab);
});

// Clicks the first suggestion offered by the test search engine.
add_task(async function test_suggestion_click() {
  Services.telemetry.clearScalars();
  Services.telemetry.clearEvents();

  let resultIndexHist = TelemetryTestUtils.getAndClearHistogram(
    "FX_URLBAR_SELECTED_RESULT_INDEX"
  );
  let resultTypeHist = TelemetryTestUtils.getAndClearHistogram(
    "FX_URLBAR_SELECTED_RESULT_TYPE"
  );
  let resultIndexByTypeHist = TelemetryTestUtils.getAndClearKeyedHistogram(
    "FX_URLBAR_SELECTED_RESULT_INDEX_BY_TYPE"
  );
  let resultMethodHist = TelemetryTestUtils.getAndClearHistogram(
    "FX_URLBAR_SELECTED_RESULT_METHOD"
  );
  let search_hist = TelemetryTestUtils.getAndClearKeyedHistogram(
    "SEARCH_COUNTS"
  );

  await withNewSearchEngine(async function(engine) {
    let tab = await BrowserTestUtils.openNewForegroundTab(
      gBrowser,
      "about:blank"
    );

    info("Type a query. Suggestions should be generated by the test engine.");
    let p = BrowserTestUtils.browserLoaded(tab.linkedBrowser);
    await searchInAwesomebar("query");
    info("Clicking the urlbar suggestion.");
    await clickURLBarSuggestion("queryfoo");
    await p;

    // Check if the scalars contain the expected values.
    const scalars = TelemetryTestUtils.getProcessScalars("parent", true, false);
    TelemetryTestUtils.assertKeyedScalar(
      scalars,
      SCALAR_URLBAR,
      "search_suggestion",
      1
    );
    Assert.equal(
      Object.keys(scalars[SCALAR_URLBAR]).length,
      1,
      "This search must only increment one entry in the scalar."
    );

    // SEARCH_COUNTS should be incremented.
    let searchEngineId = "other-" + engine.name;
    TelemetryTestUtils.assertKeyedHistogramSum(
      search_hist,
      searchEngineId + ".urlbar",
      1
    );

    // Also check events.
    TelemetryTestUtils.assertEvents(
      [
        [
          "navigation",
          "search",
          "urlbar",
          "suggestion",
          { engine: searchEngineId },
        ],
      ],
      { category: "navigation", method: "search" }
    );

    // Check the histograms as well.
    TelemetryTestUtils.assertHistogram(resultIndexHist, 3, 1);

    TelemetryTestUtils.assertHistogram(
      resultTypeHist,
      URLBAR_SELECTED_RESULT_TYPES.searchsuggestion,
      1
    );

    TelemetryTestUtils.assertKeyedHistogramValue(
      resultIndexByTypeHist,
      "searchsuggestion",
      3,
      1
    );

    TelemetryTestUtils.assertHistogram(
      resultMethodHist,
      URLBAR_SELECTED_RESULT_METHODS.click,
      1
    );

    BrowserTestUtils.removeTab(tab);
  });
});

// Selects and presses the Return (Enter) key on the first suggestion offered by
// the test search engine.  This only tests the FX_URLBAR_SELECTED_RESULT_METHOD
// histogram since test_suggestion_click covers everything else.
add_task(async function test_suggestion_arrowEnterSelection() {
  Services.telemetry.clearScalars();
  let resultMethodHist = TelemetryTestUtils.getAndClearHistogram(
    "FX_URLBAR_SELECTED_RESULT_METHOD"
  );

  await withNewSearchEngine(async function() {
    let tab = await BrowserTestUtils.openNewForegroundTab(
      gBrowser,
      "about:blank"
    );

    info("Type a query. Suggestions should be generated by the test engine.");
    let p = BrowserTestUtils.browserLoaded(tab.linkedBrowser);
    await searchInAwesomebar("query");
    info("Select the second result and press Return.");
    EventUtils.synthesizeKey("KEY_ArrowDown");
    EventUtils.synthesizeKey("KEY_Enter");
    await p;

    TelemetryTestUtils.assertHistogram(
      resultMethodHist,
      URLBAR_SELECTED_RESULT_METHODS.arrowEnterSelection,
      1
    );

    BrowserTestUtils.removeTab(tab);
  });
});

// Selects through tab and presses the Return (Enter) key on the first
// suggestion offered by the test search engine.
add_task(async function test_suggestion_tabEnterSelection() {
  Services.telemetry.clearScalars();
  let resultMethodHist = TelemetryTestUtils.getAndClearHistogram(
    "FX_URLBAR_SELECTED_RESULT_METHOD"
  );

  await withNewSearchEngine(async function() {
    let tab = await BrowserTestUtils.openNewForegroundTab(
      gBrowser,
      "about:blank"
    );

    info("Type a query. Suggestions should be generated by the test engine.");
    let p = BrowserTestUtils.browserLoaded(tab.linkedBrowser);
    await searchInAwesomebar("query");
    info("Select the second result and press Return.");
    EventUtils.synthesizeKey("KEY_Tab");
    EventUtils.synthesizeKey("KEY_Enter");
    await p;

    TelemetryTestUtils.assertHistogram(
      resultMethodHist,
      URLBAR_SELECTED_RESULT_METHODS.tabEnterSelection,
      1
    );

    BrowserTestUtils.removeTab(tab);
  });
});

// Selects through code and presses the Return (Enter) key on the first
// suggestion offered by the test search engine.
add_task(async function test_suggestion_enterSelection() {
  Services.telemetry.clearScalars();
  let resultMethodHist = TelemetryTestUtils.getAndClearHistogram(
    "FX_URLBAR_SELECTED_RESULT_METHOD"
  );

  await withNewSearchEngine(async function() {
    let tab = await BrowserTestUtils.openNewForegroundTab(
      gBrowser,
      "about:blank"
    );

    info("Type a query. Suggestions should be generated by the test engine.");
    let p = BrowserTestUtils.browserLoaded(tab.linkedBrowser);
    await searchInAwesomebar("query");
    info("Select the second result and press Return.");
    UrlbarTestUtils.setSelectedIndex(window, 1);
    EventUtils.synthesizeKey("KEY_Enter");
    await p;

    TelemetryTestUtils.assertHistogram(
      resultMethodHist,
      URLBAR_SELECTED_RESULT_METHODS.enterSelection,
      1
    );

    BrowserTestUtils.removeTab(tab);
  });
});

add_task(async function test_privateWindow() {
  // Override the search telemetry search provider info to
  // count in-content SEARCH_COUNTs telemetry for our test engine.
  SearchTelemetry.overrideSearchTelemetryForTests({
    example: {
      regexp: "^http://example\\.com/",
      queryParam: "q",
    },
  });

  let search_hist = TelemetryTestUtils.getAndClearKeyedHistogram(
    "SEARCH_COUNTS"
  );

  // First, do a bunch of searches in a private window.
  let win = await BrowserTestUtils.openNewBrowserWindow({ private: true });

  info("Search in a private window and the pref does not exist");
  let p = BrowserTestUtils.browserLoaded(win.gBrowser.selectedBrowser);
  await searchInAwesomebar("query", win);
  EventUtils.synthesizeKey("KEY_Enter", undefined, win);
  await p;

  // SEARCH_COUNTS should be incremented.
  TelemetryTestUtils.assertKeyedHistogramSum(
    search_hist,
    "other-MozSearch.urlbar",
    1
  );
  TelemetryTestUtils.assertKeyedHistogramSum(
    search_hist,
    "example.in-content:organic:none",
    1
  );

  info("Search again in a private window after setting the pref to true");
  Services.prefs.setBoolPref("browser.engagement.search_counts.pbm", true);
  p = BrowserTestUtils.browserLoaded(win.gBrowser.selectedBrowser);
  await searchInAwesomebar("another query", win);
  EventUtils.synthesizeKey("KEY_Enter", undefined, win);
  await p;

  // SEARCH_COUNTS should *not* be incremented.
  TelemetryTestUtils.assertKeyedHistogramSum(
    search_hist,
    "other-MozSearch.urlbar",
    1
  );
  TelemetryTestUtils.assertKeyedHistogramSum(
    search_hist,
    "example.in-content:organic:none",
    1
  );

  info("Search again in a private window after setting the pref to false");
  Services.prefs.setBoolPref("browser.engagement.search_counts.pbm", false);
  p = BrowserTestUtils.browserLoaded(win.gBrowser.selectedBrowser);
  await searchInAwesomebar("another query", win);
  EventUtils.synthesizeKey("KEY_Enter", undefined, win);
  await p;

  // SEARCH_COUNTS should be incremented.
  TelemetryTestUtils.assertKeyedHistogramSum(
    search_hist,
    "other-MozSearch.urlbar",
    2
  );
  TelemetryTestUtils.assertKeyedHistogramSum(
    search_hist,
    "example.in-content:organic:none",
    2
  );

  info("Search again in a private window after clearing the pref");
  Services.prefs.clearUserPref("browser.engagement.search_counts.pbm");
  p = BrowserTestUtils.browserLoaded(win.gBrowser.selectedBrowser);
  await searchInAwesomebar("another query", win);
  EventUtils.synthesizeKey("KEY_Enter", undefined, win);
  await p;

  // SEARCH_COUNTS should be incremented.
  TelemetryTestUtils.assertKeyedHistogramSum(
    search_hist,
    "other-MozSearch.urlbar",
    3
  );
  TelemetryTestUtils.assertKeyedHistogramSum(
    search_hist,
    "example.in-content:organic:none",
    3
  );

  await BrowserTestUtils.closeWindow(win);

  // Now, do a bunch of searches in a non-private window.  Telemetry should
  // always be recorded regardless of the pref's value.
  win = await BrowserTestUtils.openNewBrowserWindow();

  info("Search in a non-private window and the pref does not exist");
  p = BrowserTestUtils.browserLoaded(win.gBrowser.selectedBrowser);
  await searchInAwesomebar("query", win);
  EventUtils.synthesizeKey("KEY_Enter", undefined, win);
  await p;

  // SEARCH_COUNTS should be incremented.
  TelemetryTestUtils.assertKeyedHistogramSum(
    search_hist,
    "other-MozSearch.urlbar",
    4
  );
  TelemetryTestUtils.assertKeyedHistogramSum(
    search_hist,
    "example.in-content:organic:none",
    4
  );

  info("Search again in a non-private window after setting the pref to true");
  Services.prefs.setBoolPref("browser.engagement.search_counts.pbm", true);
  p = BrowserTestUtils.browserLoaded(win.gBrowser.selectedBrowser);
  await searchInAwesomebar("another query", win);
  EventUtils.synthesizeKey("KEY_Enter", undefined, win);
  await p;

  // SEARCH_COUNTS should be incremented.
  TelemetryTestUtils.assertKeyedHistogramSum(
    search_hist,
    "other-MozSearch.urlbar",
    5
  );
  TelemetryTestUtils.assertKeyedHistogramSum(
    search_hist,
    "example.in-content:organic:none",
    5
  );

  info("Search again in a non-private window after setting the pref to false");
  Services.prefs.setBoolPref("browser.engagement.search_counts.pbm", false);
  p = BrowserTestUtils.browserLoaded(win.gBrowser.selectedBrowser);
  await searchInAwesomebar("another query", win);
  EventUtils.synthesizeKey("KEY_Enter", undefined, win);
  await p;

  // SEARCH_COUNTS should be incremented.
  TelemetryTestUtils.assertKeyedHistogramSum(
    search_hist,
    "other-MozSearch.urlbar",
    6
  );
  TelemetryTestUtils.assertKeyedHistogramSum(
    search_hist,
    "example.in-content:organic:none",
    6
  );

  info("Search again in a non-private window after clearing the pref");
  Services.prefs.clearUserPref("browser.engagement.search_counts.pbm");
  p = BrowserTestUtils.browserLoaded(win.gBrowser.selectedBrowser);
  await searchInAwesomebar("another query", win);
  EventUtils.synthesizeKey("KEY_Enter", undefined, win);
  await p;

  // SEARCH_COUNTS should be incremented.
  TelemetryTestUtils.assertKeyedHistogramSum(
    search_hist,
    "other-MozSearch.urlbar",
    7
  );
  TelemetryTestUtils.assertKeyedHistogramSum(
    search_hist,
    "example.in-content:organic:none",
    7
  );

  await BrowserTestUtils.closeWindow(win);

  // Reset the search provider info.
  SearchTelemetry.overrideSearchTelemetryForTests();
});
