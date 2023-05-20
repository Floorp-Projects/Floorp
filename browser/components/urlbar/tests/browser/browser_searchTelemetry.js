"use strict";

const SUGGEST_URLBAR_PREF = "browser.urlbar.suggest.searches";
const MAX_FORM_HISTORY_PREF = "browser.urlbar.maxHistoricalSearchSuggestions";
const TEST_ENGINE_BASENAME = "searchSuggestionEngine.xml";

// Must run first.
add_task(async function prepare() {
  await SpecialPowers.pushPrefEnv({
    set: [
      [SUGGEST_URLBAR_PREF, true],
      [MAX_FORM_HISTORY_PREF, 2],
    ],
  });

  await SearchTestUtils.promiseNewSearchEngine({
    url: getRootDirectory(gTestPath) + TEST_ENGINE_BASENAME,
    setAsDefault: true,
  });

  registerCleanupFunction(async function () {
    // Clicking urlbar results causes visits to their associated pages, so clear
    // that history now.
    await PlacesUtils.history.clear();
    await UrlbarTestUtils.formHistory.clear();
  });

  // Move the mouse away from the urlbar one-offs so that a one-off engine is
  // not inadvertently selected.
  await EventUtils.promiseNativeMouseEvent({
    type: "mousemove",
    target: window.document.documentElement,
    offsetX: 0,
    offsetY: 0,
  });
});

add_task(async function heuristicResultMouse() {
  await compareCounts(async function () {
    let tab = await BrowserTestUtils.openNewForegroundTab(gBrowser);
    gURLBar.focus();
    await UrlbarTestUtils.promiseAutocompleteResultPopup({
      window,
      value: "heuristicResult",
    });
    let result = await UrlbarTestUtils.getDetailsOfResultAt(window, 0);
    Assert.equal(
      result.type,
      UrlbarUtils.RESULT_TYPE.SEARCH,
      "Should be of type search"
    );
    let loadPromise = BrowserTestUtils.browserLoaded(tab.linkedBrowser);
    let element = await UrlbarTestUtils.waitForAutocompleteResultAt(window, 0);
    EventUtils.synthesizeMouseAtCenter(element, {});
    await loadPromise;
    BrowserTestUtils.removeTab(tab);
    await UrlbarTestUtils.formHistory.clear();
  });
});

add_task(async function heuristicResultKeyboard() {
  await compareCounts(async function () {
    let tab = await BrowserTestUtils.openNewForegroundTab(gBrowser);
    gURLBar.focus();
    await UrlbarTestUtils.promiseAutocompleteResultPopup({
      window,
      value: "heuristicResult",
    });
    let result = await UrlbarTestUtils.getDetailsOfResultAt(window, 0);
    Assert.equal(
      result.type,
      UrlbarUtils.RESULT_TYPE.SEARCH,
      "Should be of type search"
    );
    let loadPromise = BrowserTestUtils.browserLoaded(tab.linkedBrowser);
    EventUtils.sendKey("return");
    await loadPromise;
    BrowserTestUtils.removeTab(tab);
    await UrlbarTestUtils.formHistory.clear();
  });
});

add_task(async function searchSuggestionMouse() {
  await compareCounts(async function () {
    let tab = await BrowserTestUtils.openNewForegroundTab(gBrowser);
    gURLBar.focus();
    await UrlbarTestUtils.promiseAutocompleteResultPopup({
      window,
      value: "searchSuggestion",
    });
    let idx = await getFirstSuggestionIndex();
    Assert.greaterOrEqual(idx, 0, "there should be a first suggestion");
    let loadPromise = BrowserTestUtils.browserLoaded(tab.linkedBrowser);
    let element = await UrlbarTestUtils.waitForAutocompleteResultAt(
      window,
      idx
    );
    EventUtils.synthesizeMouseAtCenter(element, {});
    await loadPromise;
    BrowserTestUtils.removeTab(tab);
    await UrlbarTestUtils.formHistory.clear();
  });
});

add_task(async function searchSuggestionKeyboard() {
  await compareCounts(async function () {
    let tab = await BrowserTestUtils.openNewForegroundTab(gBrowser);
    gURLBar.focus();
    await UrlbarTestUtils.promiseAutocompleteResultPopup({
      window,
      value: "searchSuggestion",
    });
    let idx = await getFirstSuggestionIndex();
    Assert.greaterOrEqual(idx, 0, "there should be a first suggestion");
    let loadPromise = BrowserTestUtils.browserLoaded(tab.linkedBrowser);
    while (idx--) {
      EventUtils.sendKey("down");
    }
    EventUtils.sendKey("return");
    await loadPromise;
    BrowserTestUtils.removeTab(tab);
    await UrlbarTestUtils.formHistory.clear();
  });
});

add_task(async function formHistoryMouse() {
  await compareCounts(async function () {
    await UrlbarTestUtils.formHistory.add(["foofoo", "foobar"]);
    let tab = await BrowserTestUtils.openNewForegroundTab(gBrowser);
    gURLBar.focus();
    await UrlbarTestUtils.promiseAutocompleteResultPopup({
      window,
      value: "foo",
    });
    let index = await getFirstSuggestionIndex();
    Assert.greaterOrEqual(index, 0, "there should be a first suggestion");
    let result = await UrlbarTestUtils.getDetailsOfResultAt(window, index);
    Assert.equal(result.type, UrlbarUtils.RESULT_TYPE.SEARCH);
    Assert.equal(result.source, UrlbarUtils.RESULT_SOURCE.HISTORY);
    let loadPromise = BrowserTestUtils.browserLoaded(tab.linkedBrowser);
    let element = await UrlbarTestUtils.waitForAutocompleteResultAt(
      window,
      index
    );
    EventUtils.synthesizeMouseAtCenter(element, {});
    await loadPromise;
    BrowserTestUtils.removeTab(tab);
    await UrlbarTestUtils.formHistory.clear();
  });
});

add_task(async function formHistoryKeyboard() {
  await compareCounts(async function () {
    await UrlbarTestUtils.formHistory.add(["foofoo", "foobar"]);
    let tab = await BrowserTestUtils.openNewForegroundTab(gBrowser);
    gURLBar.focus();
    await UrlbarTestUtils.promiseAutocompleteResultPopup({
      window,
      value: "foo",
    });
    let index = await getFirstSuggestionIndex();
    Assert.greaterOrEqual(index, 0, "there should be a first suggestion");
    let result = await UrlbarTestUtils.getDetailsOfResultAt(window, index);
    Assert.equal(result.type, UrlbarUtils.RESULT_TYPE.SEARCH);
    Assert.equal(result.source, UrlbarUtils.RESULT_SOURCE.HISTORY);
    let loadPromise = BrowserTestUtils.browserLoaded(tab.linkedBrowser);
    while (index--) {
      EventUtils.sendKey("down");
    }
    EventUtils.sendKey("return");
    await loadPromise;
    BrowserTestUtils.removeTab(tab);
    await UrlbarTestUtils.formHistory.clear();
  });
});

/**
 * This does three things: gets current telemetry/FHR counts, calls
 * clickCallback, gets telemetry/FHR counts again to compare them to the old
 * counts.
 *
 * @param {Function} clickCallback Use this to open the urlbar popup and choose
 *   and click a result.
 */
async function compareCounts(clickCallback) {
  // Search events triggered by clicks (not the Return key in the urlbar) are
  // recorded in three places:
  // * Telemetry histogram named "SEARCH_COUNTS"
  // * FHR

  let engine = await Services.search.getDefault();

  let histogramKey = `other-${engine.name}.urlbar`;
  let histogram = Services.telemetry.getKeyedHistogramById("SEARCH_COUNTS");
  histogram.clear();

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
    if (
      result.type == UrlbarUtils.RESULT_TYPE.SEARCH &&
      result.searchParams.suggestion
    ) {
      return i;
    }
  }
  return -1;
}
