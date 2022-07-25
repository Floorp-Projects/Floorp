/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/
 */

/**
 * This file tests urlbar telemetry with places related actions (e.g. history/
 * bookmark selection).
 */

"use strict";

const SCALAR_URLBAR = "browser.engagement.navigation.urlbar";

const TEST_URL = getRootDirectory(gTestPath).replace(
  "chrome://mochitests/content",
  "http://mochi.test:8888"
);

XPCOMUtils.defineLazyModuleGetters(this, {
  UrlbarTestUtils: "resource://testing-common/UrlbarTestUtils.jsm",
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
  Assert.ok(
    !(SCALAR_URLBAR in scalars),
    `Should not have recorded ${SCALAR_URLBAR}`
  );

  // Make sure SEARCH_COUNTS contains identical values.
  TelemetryTestUtils.assertKeyedHistogramSum(
    search_hist,
    "other-MozSearch.urlbar",
    undefined
  );
  TelemetryTestUtils.assertKeyedHistogramSum(
    search_hist,
    "other-MozSearch.alias",
    undefined
  );

  // Also check events.
  let events = Services.telemetry.snapshotEvents(
    Ci.nsITelemetry.DATASET_PRERELEASE_CHANNELS,
    false
  );
  events = (events.parent || []).filter(
    e => e[1] == "navigation" && e[2] == "search"
  );
  Assert.deepEqual(
    events,
    [],
    "Should not have recorded any navigation search events"
  );
}

function snapshotHistograms() {
  Services.telemetry.clearScalars();
  Services.telemetry.clearEvents();
  return {
    resultMethodHist: TelemetryTestUtils.getAndClearHistogram(
      "FX_URLBAR_SELECTED_RESULT_METHOD"
    ),
    search_hist: TelemetryTestUtils.getAndClearKeyedHistogram("SEARCH_COUNTS"),
  };
}

function assertTelemetryResults(histograms, type, index, method) {
  TelemetryTestUtils.assertHistogram(histograms.resultMethodHist, method, 1);

  TelemetryTestUtils.assertKeyedScalar(
    TelemetryTestUtils.getProcessScalars("parent", true, true),
    `urlbar.picked.${type}`,
    index,
    1
  );
}

add_setup(async function() {
  await SpecialPowers.pushPrefEnv({
    set: [
      // Disable search suggestions in the urlbar.
      ["browser.urlbar.suggest.searches", false],
      // Clear historical search suggestions to avoid interference from previous
      // tests.
      ["browser.urlbar.maxHistoricalSearchSuggestions", 0],
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

  await PlacesUtils.keywords.insert({
    keyword: "get",
    url: TEST_URL + "?q=%s",
  });

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

  let tab = await BrowserTestUtils.openNewForegroundTab(
    gBrowser,
    "about:blank"
  );

  await PlacesTestUtils.addVisits([
    {
      uri: "http://example.com",
      title: "example",
      transition: Ci.nsINavHistoryService.TRANSITION_TYPED,
    },
  ]);

  let p = BrowserTestUtils.browserLoaded(tab.linkedBrowser);
  await searchInAwesomebar("example");
  EventUtils.synthesizeKey("KEY_ArrowDown");
  EventUtils.synthesizeKey("KEY_Enter");
  await p;

  assertSearchTelemetryEmpty(histograms.search_hist);
  assertTelemetryResults(
    histograms,
    "history",
    1,
    UrlbarTestUtils.SELECTED_RESULT_METHODS.arrowEnterSelection
  );

  BrowserTestUtils.removeTab(tab);
});

add_task(async function test_bookmark() {
  const histograms = snapshotHistograms();

  let tab = await BrowserTestUtils.openNewForegroundTab(
    gBrowser,
    "about:blank"
  );

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
  assertTelemetryResults(
    histograms,
    "bookmark",
    1,
    UrlbarTestUtils.SELECTED_RESULT_METHODS.arrowEnterSelection
  );

  await PlacesUtils.bookmarks.remove(bm);

  BrowserTestUtils.removeTab(tab);
});

add_task(async function test_keyword() {
  const histograms = snapshotHistograms();

  let tab = await BrowserTestUtils.openNewForegroundTab(
    gBrowser,
    "about:blank"
  );

  let p = BrowserTestUtils.browserLoaded(tab.linkedBrowser);
  await searchInAwesomebar("get example");
  EventUtils.synthesizeKey("KEY_Enter");
  await p;

  assertSearchTelemetryEmpty(histograms.search_hist);
  assertTelemetryResults(
    histograms,
    "keyword",
    0,
    UrlbarTestUtils.SELECTED_RESULT_METHODS.enter
  );

  BrowserTestUtils.removeTab(tab);
});

add_task(async function test_switchtab() {
  const histograms = snapshotHistograms();

  let homeTab = await BrowserTestUtils.openNewForegroundTab(
    gBrowser,
    "about:buildconfig"
  );
  let tab = await BrowserTestUtils.openNewForegroundTab(
    gBrowser,
    "about:mozilla"
  );

  let p = BrowserTestUtils.waitForEvent(gBrowser, "TabSwitchDone");
  await searchInAwesomebar("about:buildconfig");
  EventUtils.synthesizeKey("KEY_ArrowDown");
  EventUtils.synthesizeKey("KEY_Enter");
  await p;

  assertSearchTelemetryEmpty(histograms.search_hist);
  assertTelemetryResults(
    histograms,
    "switchtab",
    1,
    UrlbarTestUtils.SELECTED_RESULT_METHODS.arrowEnterSelection
  );

  BrowserTestUtils.removeTab(tab);
  BrowserTestUtils.removeTab(homeTab);
});

add_task(async function test_visitURL() {
  const histograms = snapshotHistograms();

  let tab = await BrowserTestUtils.openNewForegroundTab(
    gBrowser,
    "about:blank"
  );

  let p = BrowserTestUtils.browserLoaded(tab.linkedBrowser);
  await searchInAwesomebar("http://example.com");
  EventUtils.synthesizeKey("KEY_Enter");
  await p;

  assertSearchTelemetryEmpty(histograms.search_hist);
  assertTelemetryResults(
    histograms,
    "visiturl",
    0,
    UrlbarTestUtils.SELECTED_RESULT_METHODS.enter
  );

  BrowserTestUtils.removeTab(tab);
});
