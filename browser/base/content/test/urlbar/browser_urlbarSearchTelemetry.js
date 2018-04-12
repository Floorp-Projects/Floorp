"use strict";

ChromeUtils.import("resource:///modules/BrowserUITelemetry.jsm");

const SUGGEST_URLBAR_PREF = "browser.urlbar.suggest.searches";
const TEST_ENGINE_BASENAME = "searchSuggestionEngine.xml";

// Must run first.
add_task(async function prepare() {
  let suggestionsEnabled = Services.prefs.getBoolPref(SUGGEST_URLBAR_PREF);
  Services.prefs.setBoolPref(SUGGEST_URLBAR_PREF, true);
  let engine = await SearchTestUtils.promiseNewSearchEngine(
    getRootDirectory(gTestPath) + TEST_ENGINE_BASENAME);
  let oldCurrentEngine = Services.search.currentEngine;
  Services.search.currentEngine = engine;

  registerCleanupFunction(async function() {
    Services.prefs.setBoolPref(SUGGEST_URLBAR_PREF, suggestionsEnabled);
    Services.search.currentEngine = oldCurrentEngine;

    // Clicking urlbar results causes visits to their associated pages, so clear
    // that history now.
    await PlacesUtils.history.clear();

    // Make sure the popup is closed for the next test.
    gURLBar.blur();
    Assert.ok(!gURLBar.popup.popupOpen, "popup should be closed");
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
    let action = getActionAtIndex(0);
    Assert.ok(!!action, "there should be an action at index 0");
    Assert.equal(action.type, "searchengine", "type should be searchengine");
    let loadPromise = BrowserTestUtils.browserLoaded(tab.linkedBrowser);
    gURLBar.popup.richlistbox.getItemAtIndex(0).click();
    await loadPromise;
    BrowserTestUtils.removeTab(tab);
  });
});

add_task(async function heuristicResultKeyboard() {
  await compareCounts(async function() {
    let tab = await BrowserTestUtils.openNewForegroundTab(gBrowser);
    gURLBar.focus();
    await promiseAutocompleteResultPopup("heuristicResult");
    let action = getActionAtIndex(0);
    Assert.ok(!!action, "there should be an action at index 0");
    Assert.equal(action.type, "searchengine", "type should be searchengine");
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
    let idx = getFirstSuggestionIndex();
    Assert.ok(idx >= 0, "there should be a first suggestion");
    let loadPromise = BrowserTestUtils.browserLoaded(tab.linkedBrowser);
    gURLBar.popup.richlistbox.getItemAtIndex(idx).click();
    await loadPromise;
    BrowserTestUtils.removeTab(tab);
  });
});

add_task(async function searchSuggestionKeyboard() {
  await compareCounts(async function() {
    let tab = await BrowserTestUtils.openNewForegroundTab(gBrowser);
    gURLBar.focus();
    await promiseAutocompleteResultPopup("searchSuggestion");
    let idx = getFirstSuggestionIndex();
    Assert.ok(idx >= 0, "there should be a first suggestion");
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
 * @param clickCallback Use this to open the urlbar popup and choose and click a
 *        result.
 */
async function compareCounts(clickCallback) {
  // Search events triggered by clicks (not the Return key in the urlbar) are
  // recorded in three places:
  // * BrowserUITelemetry
  // * Telemetry histogram named "SEARCH_COUNTS"
  // * FHR

  let engine = Services.search.currentEngine;
  let engineID = "org.mozilla.testsearchsuggestions";

  // First, get the current counts.

  // BrowserUITelemetry
  let uiTelemCount = 0;
  let bucket = BrowserUITelemetry.currentBucket;
  let events = BrowserUITelemetry.getToolbarMeasures().countableEvents;
  if (events[bucket] &&
      events[bucket].search &&
      events[bucket].search.urlbar) {
    uiTelemCount = events[bucket].search.urlbar;
  }

  // telemetry histogram SEARCH_COUNTS
  let histogramCount = 0;
  let histogramKey = engineID + ".urlbar";
  let histogram;
  try {
    histogram = Services.telemetry.getKeyedHistogramById("SEARCH_COUNTS");
  } catch (ex) {
    // No searches performed yet, not a problem.
  }
  if (histogram) {
    let snapshot = histogram.snapshot();
    if (histogramKey in snapshot) {
      histogramCount = snapshot[histogramKey].sum;
    }
  }

  // FHR -- first make sure the engine has an identifier so that FHR is happy.
  Object.defineProperty(engine.wrappedJSObject, "identifier",
                        { value: engineID });

  gURLBar.focus();
  await clickCallback();

  // Now get the new counts and compare them to the old.

  // BrowserUITelemetry
  events = BrowserUITelemetry.getToolbarMeasures().countableEvents;
  Assert.ok(bucket in events, "bucket should be recorded");
  events = events[bucket];
  Assert.ok("search" in events, "search should be recorded");
  events = events.search;
  Assert.ok("urlbar" in events, "urlbar should be recorded");
  Assert.equal(events.urlbar, uiTelemCount + 1,
               "clicked suggestion should be recorded");

  // telemetry histogram SEARCH_COUNTS
  histogram = Services.telemetry.getKeyedHistogramById("SEARCH_COUNTS");
  let snapshot = histogram.snapshot();
  Assert.ok(histogramKey in snapshot, "histogram with key should be recorded");
  Assert.equal(snapshot[histogramKey].sum, histogramCount + 1,
               "histogram sum should be incremented");
}

/**
 * Returns the "action" object at the given index in the urlbar results:
 * { type, params: {}}
 *
 * @param index The index in the urlbar results.
 * @return An action object, or null if index >= number of results.
 */
function getActionAtIndex(index) {
  let controller = gURLBar.popup.input.controller;
  if (controller.matchCount <= index) {
    return null;
  }
  let url = controller.getValueAt(index);
  let mozActionMatch = url.match(/^moz-action:([^,]+),(.*)$/);
  if (!mozActionMatch) {
    let msg = "result at index " + index + " is not a moz-action: " + url;
    Assert.ok(false, msg);
    throw new Error(msg);
  }
  let [, type, paramStr] = mozActionMatch;
  return {
    type,
    params: JSON.parse(paramStr),
  };
}

/**
 * Returns the index of the first search suggestion in the urlbar results.
 *
 * @return An index, or -1 if there are no search suggestions.
 */
function getFirstSuggestionIndex() {
  let controller = gURLBar.popup.input.controller;
  let matchCount = controller.matchCount;
  for (let i = 0; i < matchCount; i++) {
    let url = controller.getValueAt(i);
    let mozActionMatch = url.match(/^moz-action:([^,]+),(.*)$/);
    if (mozActionMatch) {
      let [, type, paramStr] = mozActionMatch;
      let params = JSON.parse(paramStr);
      if (type == "searchengine" && "searchSuggestion" in params) {
        return i;
      }
    }
  }
  return -1;
}
