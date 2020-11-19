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

/**
 * Checks to see if the second result in the Urlbar is an onboarding result
 * with the correct engine.
 */
async function checkForOnboardingResult(engineName) {
  Assert.ok(UrlbarTestUtils.isPopupOpen(window), "Popup should be open.");
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
    engineName,
    "The tab-to-search result is for the first engine."
  );
  Assert.equal(
    tabToSearchResult.payload.dynamicType,
    "onboardTabToSearch",
    "The tab-to-search result is an onboarding result."
  );
}

add_task(async function setup() {
  await SpecialPowers.pushPrefEnv({
    set: [
      ["browser.urlbar.update2", true],
      ["browser.urlbar.update2.tabToComplete", true],
      ["browser.urlbar.tabToSearch.onboard.interactionsLeft", 0],
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

add_task(async function onboarding_impressions() {
  await SpecialPowers.pushPrefEnv({
    set: [["browser.urlbar.tabToSearch.onboard.interactionsLeft", 3]],
  });

  await BrowserTestUtils.withNewTab("about:blank", async browser => {
    const firstEngineHost = "example";
    let secondEngine = await Services.search.addEngineWithDetails(
      `${ENGINE_NAME}2`,
      { template: `http://${firstEngineHost}-2.com/?q={searchTerms}` }
    );

    for (let i = 0; i < 3; i++) {
      await PlacesTestUtils.addVisits([`https://${firstEngineHost}-2.com`]);
      await PlacesTestUtils.addVisits([`https://${ENGINE_DOMAIN}/`]);
    }

    // First do multiple searches for substrings of firstEngineHost. The view
    // should show the same tab-to-search onboarding result the entire time, so
    // we should not continue to increment urlbar.tips.
    for (let i = 1; i < firstEngineHost.length; i++) {
      info(
        `Search for "${firstEngineHost.slice(
          0,
          i
        )}". Only record one impression for this sequence.`
      );
      await UrlbarTestUtils.promiseAutocompleteResultPopup({
        window,
        value: firstEngineHost.slice(0, i),
        fireInputEvent: true,
      });
      await checkForOnboardingResult(ENGINE_NAME);
    }

    await UrlbarTestUtils.promisePopupClose(window, () => gURLBar.blur());
    TelemetryTestUtils.assertKeyedScalar(
      TelemetryTestUtils.getProcessScalars("parent", true),
      "urlbar.tips",
      "tabtosearch_onboard-shown",
      1
    );

    info("Type through autofill to second engine hostname. Record impression.");
    await UrlbarTestUtils.promiseAutocompleteResultPopup({
      window,
      value: firstEngineHost,
      fireInputEvent: true,
    });
    await checkForOnboardingResult(ENGINE_NAME);
    await UrlbarTestUtils.promiseAutocompleteResultPopup({
      window,
      value: `${firstEngineHost}-`,
      fireInputEvent: true,
    });
    await checkForOnboardingResult(`${ENGINE_NAME}2`);
    await UrlbarTestUtils.promisePopupClose(window, () => gURLBar.blur());
    // Since the user typed past the autofill for the first engine, we showed a
    // different onboarding result and now we increment
    // tabtosearch_onboard-shown.
    TelemetryTestUtils.assertKeyedScalar(
      TelemetryTestUtils.getProcessScalars("parent", true),
      "urlbar.tips",
      "tabtosearch_onboard-shown",
      3
    );

    info("Make a typo and return to autofill. Do not record impression.");
    await UrlbarTestUtils.promiseAutocompleteResultPopup({
      window,
      value: `${firstEngineHost}-`,
      fireInputEvent: true,
    });
    await checkForOnboardingResult(`${ENGINE_NAME}2`);
    await UrlbarTestUtils.promiseAutocompleteResultPopup({
      window,
      value: `${firstEngineHost}-3`,
      fireInputEvent: true,
    });
    Assert.equal(
      UrlbarTestUtils.getResultCount(window),
      1,
      "We are not showing a tab-to-search result."
    );
    await UrlbarTestUtils.promiseAutocompleteResultPopup({
      window,
      value: `${firstEngineHost}-2`,
      fireInputEvent: true,
    });
    await checkForOnboardingResult(`${ENGINE_NAME}2`);
    await UrlbarTestUtils.promisePopupClose(window, () => gURLBar.blur());
    TelemetryTestUtils.assertKeyedScalar(
      TelemetryTestUtils.getProcessScalars("parent", true),
      "urlbar.tips",
      "tabtosearch_onboard-shown",
      4
    );

    info("Cancel then restart autofill. Do not record impression.");
    await UrlbarTestUtils.promiseAutocompleteResultPopup({
      window,
      value: `${firstEngineHost}-2`,
      fireInputEvent: true,
    });
    await checkForOnboardingResult(`${ENGINE_NAME}2`);
    let searchPromise = UrlbarTestUtils.promiseSearchComplete(window);
    EventUtils.synthesizeKey("KEY_Backspace");
    await searchPromise;
    Assert.greater(
      UrlbarTestUtils.getResultCount(window),
      1,
      "Sanity check: we have more than one result."
    );
    let result = (await UrlbarTestUtils.waitForAutocompleteResultAt(window, 1))
      .result;
    Assert.notEqual(
      result.type,
      UrlbarUtils.RESULT_TYPE.SEARCH,
      "The second result is not a tab-to-search result."
    );
    searchPromise = UrlbarTestUtils.promiseSearchComplete(window);
    // Type the "." from `example-2.com`.
    EventUtils.synthesizeKey(".");
    await searchPromise;
    await checkForOnboardingResult(`${ENGINE_NAME}2`);
    await UrlbarTestUtils.promisePopupClose(window, () => gURLBar.blur());
    TelemetryTestUtils.assertKeyedScalar(
      TelemetryTestUtils.getProcessScalars("parent", true),
      "urlbar.tips",
      "tabtosearch_onboard-shown",
      5
    );

    // See javadoc for UrlbarProviderTabToSearch.onEngagement for discussion
    // about retained results.
    info("Reopen the result set with retained results. Record impression.");
    await UrlbarTestUtils.promisePopupOpen(window, () => {
      EventUtils.synthesizeMouseAtCenter(gURLBar.inputField, {});
    });
    await checkForOnboardingResult(`${ENGINE_NAME}2`);
    await UrlbarTestUtils.promisePopupClose(window, () => gURLBar.blur());
    TelemetryTestUtils.assertKeyedScalar(
      TelemetryTestUtils.getProcessScalars("parent", true),
      "urlbar.tips",
      "tabtosearch_onboard-shown",
      6
    );

    info(
      "Open a result page and then autofill engine host. Record impression."
    );
    await UrlbarTestUtils.promiseAutocompleteResultPopup({
      window,
      value: firstEngineHost,
      fireInputEvent: true,
    });
    await checkForOnboardingResult(ENGINE_NAME);
    // Press enter on the heuristic result so we visit example.com without
    // doing an additional search.
    let loadPromise = BrowserTestUtils.browserLoaded(browser);
    EventUtils.synthesizeKey("KEY_Enter");
    await loadPromise;
    // Click the Urlbar and type to simulate what a user would actually do. If
    // we use promiseAutocompleteResultPopup, no query would be made between
    // this one and the previous tab-to-search query. Thus
    // `onboardingEnginesShown` would not be cleared. This would not happen
    // in real-world usage.
    await UrlbarTestUtils.promisePopupOpen(window, () => {
      EventUtils.synthesizeMouseAtCenter(gURLBar.inputField, {});
    });
    searchPromise = UrlbarTestUtils.promiseSearchComplete(window);
    EventUtils.synthesizeKey(firstEngineHost.slice(0, 4));
    await searchPromise;
    await checkForOnboardingResult(ENGINE_NAME);
    await UrlbarTestUtils.promisePopupClose(window, () => gURLBar.blur());
    // We clear the scalar this time.
    TelemetryTestUtils.assertKeyedScalar(
      TelemetryTestUtils.getProcessScalars("parent", true, true),
      "urlbar.tips",
      "tabtosearch_onboard-shown",
      8
    );

    await PlacesUtils.history.clear();
    await Services.search.removeEngine(secondEngine);
  });
});
