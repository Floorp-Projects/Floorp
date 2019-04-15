/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/
 */

/**
 * This file tests urlbar telemetry with extension actions.
 */

"use strict";

const SCALAR_URLBAR = "browser.engagement.navigation.urlbar";

XPCOMUtils.defineLazyModuleGetters(this, {
  UrlbarTestUtils: "resource://testing-common/UrlbarTestUtils.jsm",
  URLBAR_SELECTED_RESULT_TYPES: "resource:///modules/BrowserUsageTelemetry.jsm",
  URLBAR_SELECTED_RESULT_METHODS: "resource:///modules/BrowserUsageTelemetry.jsm",
});

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

  // Make sure to restore the engine once we're done.
  registerCleanupFunction(async function() {
    Services.telemetry.canRecordExtended = oldCanRecord;
    await PlacesUtils.history.clear();
    await PlacesUtils.bookmarks.eraseEverything();
    Services.telemetry.setEventRecordingEnabled("navigation", false);
  });
});

add_task(async function test_extension() {
  let extension = ExtensionTestUtils.loadExtension({
    manifest: {
      "omnibox": {
        "keyword": "omniboxtest",
      },

      background() {
        /* global browser */
        browser.omnibox.setDefaultSuggestion({
          description: "doit",
        });
        // Just do nothing for this test.
        browser.omnibox.onInputEntered.addListener(() => {
        });
        browser.omnibox.onInputChanged.addListener((text, suggest) => {
          suggest([]);
        });
      },
    },
  });

  await extension.startup();

  const histograms = snapshotHistograms();

  let tab = await BrowserTestUtils.openNewForegroundTab(gBrowser, "about:blank");

  await UrlbarTestUtils.promiseAutocompleteResultPopup({
    window,
    waitForFocus,
    value: "omniboxtest ",
    fireInputEvent: true,
  });
  EventUtils.synthesizeKey("KEY_Enter");

  assertSearchTelemetryEmpty(histograms.search_hist);
  assertHistogramResults(histograms, "extension", 0,
    URLBAR_SELECTED_RESULT_METHODS.enter);

  await extension.unload();
  BrowserTestUtils.removeTab(tab);
});
