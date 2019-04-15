/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/
 */

/**
 * This file tests urlbar telemetry with places related actions (e.g. history/
 * bookmark selection).
 */

"use strict";

const SCALAR_URLBAR = "browser.engagement.navigation.urlbar";

const TEST_URL = getRootDirectory(gTestPath)
  .replace("chrome://mochitests/content", "http://mochi.test:8888");

XPCOMUtils.defineLazyModuleGetters(this, {
  UrlbarTestUtils: "resource://testing-common/UrlbarTestUtils.jsm",
  URLBAR_SELECTED_RESULT_TYPES: "resource:///modules/BrowserUsageTelemetry.jsm",
  URLBAR_SELECTED_RESULT_METHODS: "resource:///modules/BrowserUsageTelemetry.jsm",
});

function searchInAwesomebar(value, win = window) {
  return UrlbarTestUtils.promiseAutocompleteResultPopup({
    window: win,
    waitForFocus,
    value,
    fireInputEvent: true,
  });
}

function assertSearchTelemetryEmpty(search_hist) {
  const scalars = TelemetryTestUtils.getProcessScalars("parent", true, false);
  Assert.ok(!(SCALAR_URLBAR in scalars), `Should not have recorded ${SCALAR_URLBAR}`);

  // Make sure SEARCH_COUNTS contains identical values.
  TelemetryTestUtils.assertKeyedHistogramSum(search_hist, "other-MozSearch.urlbar", undefined);
  TelemetryTestUtils.assertKeyedHistogramSum(search_hist, "other-MozSearch.alias", undefined);

  // Also check events.
  let events = Services.telemetry.snapshotEvents(Ci.nsITelemetry.DATASET_PRERELEASE_CHANNELS, false);
  events = (events.parent || []).filter(e => e[1] == "navigation" && e[2] == "search");
  Assert.deepEqual(events, [], "Should not have recorded any navigation search events");
}

function snapshotHistograms() {
  Services.telemetry.clearScalars();
  Services.telemetry.clearEvents();
  return {
    resultIndexHist: TelemetryTestUtils.getAndClearHistogram("FX_URLBAR_SELECTED_RESULT_INDEX"),
    resultTypeHist: TelemetryTestUtils.getAndClearHistogram("FX_URLBAR_SELECTED_RESULT_TYPE"),
    resultIndexByTypeHist: TelemetryTestUtils.getAndClearKeyedHistogram("FX_URLBAR_SELECTED_RESULT_INDEX_BY_TYPE"),
    resultMethodHist: TelemetryTestUtils.getAndClearHistogram("FX_URLBAR_SELECTED_RESULT_METHOD"),
    search_hist: TelemetryTestUtils.getAndClearKeyedHistogram("SEARCH_COUNTS"),
  };
}

function assertHistogramResults(histograms, type, index, method) {
  TelemetryTestUtils.assertHistogram(histograms.resultIndexHist, index, 1);

  TelemetryTestUtils.assertHistogram(histograms.resultTypeHist,
    URLBAR_SELECTED_RESULT_TYPES[type], 1);

  TelemetryTestUtils.assertKeyedHistogramValue(histograms.resultIndexByTypeHist,
    type, index, 1);

  TelemetryTestUtils.assertHistogram(histograms.resultMethodHist,
    method, 1);
}


add_task(async function setup() {
  await SpecialPowers.pushPrefEnv({
    set: [
      // Disable search suggestions in the urlbar.
      ["browser.urlbar.suggest.searches", false],
      // Clear historical search suggestions to avoid interference from previous
      // tests.
      ["browser.urlbar.maxHistoricalSearchSuggestions", 0],
      // Use the default matching bucket configuration.
      ["browser.urlbar.matchBuckets", "general:5,suggestion:4"],
      // Turn autofill off.
      ["browser.urlbar.autoFill", false],
    ],
  });

  // Enable local telemetry recording for the duration of the tests.
  let oldCanRecord = Services.telemetry.canRecordExtended;
  Services.telemetry.canRecordExtended = true;

  // Enable event recording for the events tested here.
  Services.telemetry.setEventRecordingEnabled("navigation", true);

  // Clear history so that history added by previous tests doesn't mess up this
  // test when it selects results in the urlbar.
  await PlacesUtils.history.clear();
  await PlacesUtils.bookmarks.eraseEverything();

  await PlacesUtils.keywords.insert({ keyword: "get",
                                      url: TEST_URL + "?q=%s" });

  // Make sure to restore the engine once we're done.
  registerCleanupFunction(async function() {
    await PlacesUtils.keywords.remove("get");
    Services.telemetry.canRecordExtended = oldCanRecord;
    await PlacesUtils.history.clear();
    await PlacesUtils.bookmarks.eraseEverything();
    Services.telemetry.setEventRecordingEnabled("navigation", false);
  });
});

add_task(async function test_history() {
  const histograms = snapshotHistograms();

  let tab = await BrowserTestUtils.openNewForegroundTab(gBrowser, "about:blank");

  await PlacesTestUtils.addVisits([{
    uri: "http://example.com",
    title: "example",
    transition: Ci.nsINavHistoryService.TRANSITION_TYPED,
  }]);

  let p = BrowserTestUtils.browserLoaded(tab.linkedBrowser);
  await searchInAwesomebar("example");
  EventUtils.synthesizeKey("KEY_ArrowDown");
  EventUtils.synthesizeKey("KEY_Enter");
  await p;

  assertSearchTelemetryEmpty(histograms.search_hist);
  assertHistogramResults(histograms, "history", 1,
    URLBAR_SELECTED_RESULT_METHODS.arrowEnterSelection);

  BrowserTestUtils.removeTab(tab);
});

add_task(async function test_bookmark() {
  const histograms = snapshotHistograms();

  let tab = await BrowserTestUtils.openNewForegroundTab(gBrowser, "about:blank");

  let bm = await PlacesUtils.bookmarks.insert({
    url: "http://example.com",
    title: "example",
    parentGuid: PlacesUtils.bookmarks.menuGuid,
  });

  let p = BrowserTestUtils.browserLoaded(tab.linkedBrowser);
  await searchInAwesomebar("example");
  EventUtils.synthesizeKey("KEY_ArrowDown");
  EventUtils.synthesizeKey("KEY_Enter");
  await p;

  assertSearchTelemetryEmpty(histograms.search_hist);
  assertHistogramResults(histograms, "bookmark", 1,
    URLBAR_SELECTED_RESULT_METHODS.arrowEnterSelection);

  await PlacesUtils.bookmarks.remove(bm);

  BrowserTestUtils.removeTab(tab);
});

add_task(async function test_keyword() {
  const histograms = snapshotHistograms();

  let tab = await BrowserTestUtils.openNewForegroundTab(gBrowser, "about:blank");

  let p = BrowserTestUtils.browserLoaded(tab.linkedBrowser);
  await searchInAwesomebar("get example");
  EventUtils.synthesizeKey("KEY_Enter");
  await p;

  assertSearchTelemetryEmpty(histograms.search_hist);
  assertHistogramResults(histograms, "keyword", 0,
    URLBAR_SELECTED_RESULT_METHODS.enter);

  BrowserTestUtils.removeTab(tab);
});


add_task(async function test_switchtab() {
  const histograms = snapshotHistograms();

  let homeTab = await BrowserTestUtils.openNewForegroundTab(gBrowser, "about:buildconfig");
  let tab = await BrowserTestUtils.openNewForegroundTab(gBrowser, "about:mozilla");

  let p = BrowserTestUtils.waitForEvent(gBrowser, "TabSwitchDone");
  await searchInAwesomebar("about:buildconfig");
  EventUtils.synthesizeKey("KEY_ArrowDown");
  EventUtils.synthesizeKey("KEY_Enter");
  await p;

  assertSearchTelemetryEmpty(histograms.search_hist);
  assertHistogramResults(histograms, "switchtab", 1,
    URLBAR_SELECTED_RESULT_METHODS.arrowEnterSelection);

  BrowserTestUtils.removeTab(tab);
  BrowserTestUtils.removeTab(homeTab);
});

add_task(async function test_tag() {
  if (UrlbarPrefs.get("quantumbar")) {
    return;
  }
  const histograms = snapshotHistograms();

  let tab = await BrowserTestUtils.openNewForegroundTab(gBrowser, "about:blank");

  Services.prefs.setBoolPref("browser.urlbar.suggest.bookmark", false);

  PlacesUtils.tagging.tagURI(Services.io.newURI("http://example.com"), ["tag"]);

  let p = BrowserTestUtils.browserLoaded(tab.linkedBrowser);
  await searchInAwesomebar("tag");
  EventUtils.synthesizeKey("KEY_ArrowDown");
  EventUtils.synthesizeKey("KEY_Enter");
  await p;

  assertSearchTelemetryEmpty(histograms.search_hist);
  assertHistogramResults(histograms, "tag", 1,
    URLBAR_SELECTED_RESULT_METHODS.arrowEnterSelection);

  PlacesUtils.tagging.untagURI(Services.io.newURI("http://example.com"), ["tag"]);
  Services.prefs.clearUserPref("browser.urlbar.suggest.bookmark");
  BrowserTestUtils.removeTab(tab);
});

add_task(async function test_visitURL() {
  const histograms = snapshotHistograms();

  let tab = await BrowserTestUtils.openNewForegroundTab(gBrowser, "about:blank");

  let p = BrowserTestUtils.browserLoaded(tab.linkedBrowser);
  await searchInAwesomebar("http://example.com");
  EventUtils.synthesizeKey("KEY_Enter");
  await p;

  assertSearchTelemetryEmpty(histograms.search_hist);
  assertHistogramResults(histograms, "visiturl", 0,
    URLBAR_SELECTED_RESULT_METHODS.enter);

  BrowserTestUtils.removeTab(tab);
});

add_task(async function test_autofill() {
  const histograms = snapshotHistograms();

  let tab = await BrowserTestUtils.openNewForegroundTab(gBrowser, "about:blank");

  await PlacesTestUtils.addVisits([{
    uri: "http://example.com/mypage",
    title: "example",
    transition: Ci.nsINavHistoryService.TRANSITION_TYPED,
  }]);

  Services.prefs.setBoolPref("browser.urlbar.autoFill", true);

  let p = BrowserTestUtils.browserLoaded(tab.linkedBrowser);
  await searchInAwesomebar("example.com/my");
  EventUtils.synthesizeKey("KEY_Enter");
  await p;

  assertSearchTelemetryEmpty(histograms.search_hist);
  assertHistogramResults(histograms, "autofill", 0,
    URLBAR_SELECTED_RESULT_METHODS.enter);

  BrowserTestUtils.removeTab(tab);
});
