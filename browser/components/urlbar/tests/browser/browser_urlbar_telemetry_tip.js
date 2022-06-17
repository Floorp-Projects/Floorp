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
});

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

  assertTelemetryResults(
    histograms,
    "tip",
    0,
    UrlbarTestUtils.SELECTED_RESULT_METHODS.enter
  );

  UrlbarProvidersManager.unregisterProvider(provider);
  BrowserTestUtils.removeTab(tab);
});

/**
 * A test URLBar provider.
 */
class TipProvider extends UrlbarProvider {
  constructor(results) {
    super();
    this._results = results;
  }
  get name() {
    return "TestProviderTip";
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
}
