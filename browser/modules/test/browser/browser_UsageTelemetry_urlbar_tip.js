/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/
 */

/**
 * This file tests urlbar telemetry for tip results.
 */

"use strict";

XPCOMUtils.defineLazyModuleGetters(this, {
  UrlbarProvider: "resource:///modules/UrlbarUtils.jsm",
  UrlbarProvidersManager: "resource:///modules/UrlbarProvidersManager.jsm",
  UrlbarResult: "resource:///modules/UrlbarResult.jsm",
  UrlbarTestUtils: "resource://testing-common/UrlbarTestUtils.jsm",
  URLBAR_SELECTED_RESULT_TYPES: "resource:///modules/BrowserUsageTelemetry.jsm",
  URLBAR_SELECTED_RESULT_METHODS:
    "resource:///modules/BrowserUsageTelemetry.jsm",
});

function snapshotHistograms() {
  Services.telemetry.clearScalars();
  Services.telemetry.clearEvents();
  return {
    resultIndexHist: TelemetryTestUtils.getAndClearHistogram(
      "FX_URLBAR_SELECTED_RESULT_INDEX"
    ),
    resultTypeHist: TelemetryTestUtils.getAndClearHistogram(
      "FX_URLBAR_SELECTED_RESULT_TYPE"
    ),
    resultIndexByTypeHist: TelemetryTestUtils.getAndClearKeyedHistogram(
      "FX_URLBAR_SELECTED_RESULT_INDEX_BY_TYPE"
    ),
    resultMethodHist: TelemetryTestUtils.getAndClearHistogram(
      "FX_URLBAR_SELECTED_RESULT_METHOD"
    ),
    search_hist: TelemetryTestUtils.getAndClearKeyedHistogram("SEARCH_COUNTS"),
  };
}

function assertHistogramResults(histograms, type, index, method) {
  TelemetryTestUtils.assertHistogram(histograms.resultIndexHist, index, 1);

  TelemetryTestUtils.assertHistogram(
    histograms.resultTypeHist,
    URLBAR_SELECTED_RESULT_TYPES[type],
    1
  );

  TelemetryTestUtils.assertKeyedHistogramValue(
    histograms.resultIndexByTypeHist,
    type,
    index,
    1
  );

  TelemetryTestUtils.assertHistogram(histograms.resultMethodHist, method, 1);
}

add_task(async function setup() {
  await SpecialPowers.pushPrefEnv({
    set: [
      // Disable search suggestions in the urlbar.
      ["browser.urlbar.suggest.searches", false],
      // Turn autofill off.
      ["browser.urlbar.autoFill", false],
    ],
  });
  await PlacesUtils.history.clear();
  await PlacesUtils.bookmarks.eraseEverything();
});

add_task(async function test() {
  // Add a restricting provider that returns a preselected heuristic tip result.
  let provider = new TipProvider([
    Object.assign(
      new UrlbarResult(
        UrlbarUtils.RESULT_TYPE.TIP,
        UrlbarUtils.RESULT_SOURCE.OTHER_LOCAL,
        {
          icon: "",
          text: "This is a test tip.",
          buttonText: "OK",
          type: "test",
        }
      ),
      { heuristic: true }
    ),
  ]);
  UrlbarProvidersManager.registerProvider(provider);

  const histograms = snapshotHistograms();

  let tab = await BrowserTestUtils.openNewForegroundTab(
    gBrowser,
    "about:blank"
  );

  // Show the view and press enter to select the tip.
  await UrlbarTestUtils.promiseAutocompleteResultPopup({
    window,
    waitForFocus,
    value: "test",
    fireInputEvent: true,
  });
  EventUtils.synthesizeKey("KEY_Enter");

  assertHistogramResults(
    histograms,
    "tip",
    0,
    URLBAR_SELECTED_RESULT_METHODS.enter
  );

  UrlbarProvidersManager.unregisterProvider(provider);
  BrowserTestUtils.removeTab(tab);
});

class TipProvider extends UrlbarProvider {
  constructor(results) {
    super();
    this._results = results;
  }
  get name() {
    return "TestTipProvider";
  }
  get type() {
    return UrlbarUtils.PROVIDER_TYPE.PROFILE;
  }
  isActive(context) {
    return true;
  }
  getPriority(context) {
    return 1;
  }
  async startQuery(context, addCallback) {
    context.preselected = true;
    for (const result of this._results) {
      addCallback(this, result);
    }
  }
  cancelQuery(context) {}
  pickResult(result) {}
}
