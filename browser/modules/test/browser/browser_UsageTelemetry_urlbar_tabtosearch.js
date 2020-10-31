/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/
 */

/**
 * This file tests telemetry for tabtosearch results.
 * NB: This file does not test the search mode `entry` field for tab-to-search
 * results. That is tested in browser_UsageTelemetry_urlbar_searchmode.js.
 */

"use strict";

const ENGINE_NAME = "MozSearch";
const ENGINE_DOMAIN = "example.com";

XPCOMUtils.defineLazyModuleGetters(this, {
  UrlbarTestUtils: "resource://testing-common/UrlbarTestUtils.jsm",
});

function snapshotHistograms() {
  Services.telemetry.clearScalars();
  Services.telemetry.clearEvents();
  return {
    resultIndexHist: TelemetryTestUtils.getAndClearHistogram(
      "FX_URLBAR_SELECTED_RESULT_INDEX"
    ),
    resultTypeHist: TelemetryTestUtils.getAndClearHistogram(
      "FX_URLBAR_SELECTED_RESULT_TYPE_2"
    ),
    resultIndexByTypeHist: TelemetryTestUtils.getAndClearKeyedHistogram(
      "FX_URLBAR_SELECTED_RESULT_INDEX_BY_TYPE_2"
    ),
    resultMethodHist: TelemetryTestUtils.getAndClearHistogram(
      "FX_URLBAR_SELECTED_RESULT_METHOD"
    ),
    search_hist: TelemetryTestUtils.getAndClearKeyedHistogram("SEARCH_COUNTS"),
  };
}

function assertTelemetryResults(histograms, type, index, method) {
  TelemetryTestUtils.assertHistogram(histograms.resultIndexHist, index, 1);

  TelemetryTestUtils.assertHistogram(
    histograms.resultTypeHist,
    UrlbarUtils.SELECTED_RESULT_TYPES[type],
    1
  );

  TelemetryTestUtils.assertKeyedHistogramValue(
    histograms.resultIndexByTypeHist,
    type,
    index,
    1
  );

  TelemetryTestUtils.assertHistogram(histograms.resultMethodHist, method, 1);

  TelemetryTestUtils.assertKeyedScalar(
    TelemetryTestUtils.getProcessScalars("parent", true, true),
    `urlbar.picked.${type}`,
    index,
    1
  );
}

add_task(async function setup() {
  await SpecialPowers.pushPrefEnv({
    set: [
      ["browser.urlbar.update2", true],
      ["browser.urlbar.update2.tabToComplete", true],
      ["browser.urlbar.tabToSearch.onboard.maxShown", 0],
    ],
  });

  let engine = await Services.search.addEngineWithDetails(ENGINE_NAME, {
    template: `http://${ENGINE_DOMAIN}/?q={searchTerms}`,
  });

  UrlbarTestUtils.init(this);

  registerCleanupFunction(async () => {
    UrlbarTestUtils.uninit();
    await Services.search.removeEngine(engine);
  });
});

add_task(async function test() {
  await BrowserTestUtils.withNewTab("about:blank", async () => {
    const histograms = snapshotHistograms();

    for (let i = 0; i < 3; i++) {
      await PlacesTestUtils.addVisits([`https://${ENGINE_DOMAIN}/`]);
    }

    await UrlbarTestUtils.promiseAutocompleteResultPopup({
      window,
      value: ENGINE_DOMAIN.slice(0, 4),
    });

    let tabToSearchResult = (
      await UrlbarTestUtils.waitForAutocompleteResultAt(window, 1)
    ).result;
    Assert.equal(
      tabToSearchResult.providerName,
      "TabToSearch",
      "The second result is a tab-to-search result."
    );
    Assert.equal(
      tabToSearchResult.payload.engine,
      ENGINE_NAME,
      "The tab-to-search result is for the correct engine."
    );
    EventUtils.synthesizeKey("KEY_ArrowDown");
    Assert.equal(
      UrlbarTestUtils.getSelectedRowIndex(window),
      1,
      "Sanity check: The second result is selected."
    );

    // Select the tab-to-search result.
    let searchPromise = UrlbarTestUtils.promiseSearchComplete(window);
    EventUtils.synthesizeKey("KEY_Enter");
    await searchPromise;

    await UrlbarTestUtils.assertSearchMode(window, {
      engineName: ENGINE_NAME,
      entry: "tabtosearch",
    });

    assertTelemetryResults(
      histograms,
      "tabtosearch",
      1,
      UrlbarTestUtils.SELECTED_RESULT_METHODS.arrowEnterSelection
    );

    await UrlbarTestUtils.exitSearchMode(window);
    await UrlbarTestUtils.promisePopupClose(window, () => {
      gURLBar.blur();
    });
    await PlacesUtils.history.clear();
  });
});
