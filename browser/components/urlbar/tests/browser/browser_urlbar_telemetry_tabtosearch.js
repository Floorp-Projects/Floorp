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
  UrlbarProviderTabToSearch:
    "resource:///modules/UrlbarProviderTabToSearch.jsm",
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

/**
 * Checks to see if the second result in the Urlbar is a tab-to-search result
 * with the correct engine.
 *
 * @param {string} engineName
 *   The expected engine name.
 * @param {boolean} [isOnboarding]
 *   If true, expects the tab-to-search result to be an onbarding result.
 */
async function checkForTabToSearchResult(engineName, isOnboarding) {
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
  if (isOnboarding) {
    Assert.equal(
      tabToSearchResult.payload.dynamicType,
      "onboardTabToSearch",
      "The tab-to-search result is an onboarding result."
    );
  } else {
    Assert.ok(
      !tabToSearchResult.payload.dynamicType,
      "The tab-to-search result should not be an onboarding result."
    );
  }
}

add_setup(async function() {
  await SpecialPowers.pushPrefEnv({
    set: [["browser.urlbar.tabToSearch.onboard.interactionsLeft", 0]],
  });

  await SearchTestUtils.installSearchExtension({
    name: ENGINE_NAME,
    search_url: `https://${ENGINE_DOMAIN}/`,
  });

  UrlbarTestUtils.init(this);
  // Reset the enginesShown sets in case a previous test showed a tab-to-search
  // result but did not end its engagement.
  UrlbarProviderTabToSearch.enginesShown.regular.clear();
  UrlbarProviderTabToSearch.enginesShown.onboarding.clear();

  // Enable local telemetry recording for the duration of the tests.
  let oldCanRecord = Services.telemetry.canRecordExtended;
  Services.telemetry.canRecordExtended = true;

  registerCleanupFunction(async () => {
    Services.telemetry.canRecordExtended = oldCanRecord;
    UrlbarTestUtils.uninit();
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
      fireInputEvent: true,
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

  Services.telemetry.clearScalars();
  Services.telemetry.clearEvents();
});

add_task(async function impressions() {
  await SpecialPowers.pushPrefEnv({
    set: [["browser.urlbar.tabToSearch.onboard.interactionsLeft", 0]],
  });
  await impressions_test(false);
  await SpecialPowers.popPrefEnv();
});

add_task(async function onboarding_impressions() {
  await SpecialPowers.pushPrefEnv({
    set: [["browser.urlbar.tabToSearch.onboard.interactionsLeft", 3]],
  });
  await impressions_test(true);
  await SpecialPowers.popPrefEnv();
  delete UrlbarProviderTabToSearch.onboardingInteractionAtTime;
});

async function impressions_test(isOnboarding) {
  await BrowserTestUtils.withNewTab("about:blank", async browser => {
    const firstEngineHost = "example";
    let extension = await SearchTestUtils.installSearchExtension(
      {
        name: `${ENGINE_NAME}2`,
        search_url: `https://${firstEngineHost}-2.com/`,
      },
      true
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
      await checkForTabToSearchResult(ENGINE_NAME, isOnboarding);
    }

    await UrlbarTestUtils.promisePopupClose(window, () => gURLBar.blur());
    let scalars = TelemetryTestUtils.getProcessScalars("parent", true);
    TelemetryTestUtils.assertKeyedScalar(
      scalars,
      "urlbar.tips",
      isOnboarding ? "tabtosearch_onboard-shown" : "tabtosearch-shown",
      1
    );
    TelemetryTestUtils.assertKeyedScalar(
      scalars,
      isOnboarding
        ? "urlbar.tabtosearch.impressions_onboarding"
        : "urlbar.tabtosearch.impressions",
      // "other" is recorded as the engine name because we're not using a built-in engine.
      "other",
      1
    );

    info("Type through autofill to second engine hostname. Record impression.");
    await UrlbarTestUtils.promiseAutocompleteResultPopup({
      window,
      value: firstEngineHost,
      fireInputEvent: true,
    });
    await checkForTabToSearchResult(ENGINE_NAME, isOnboarding);
    await UrlbarTestUtils.promiseAutocompleteResultPopup({
      window,
      value: `${firstEngineHost}-`,
      fireInputEvent: true,
    });
    await checkForTabToSearchResult(`${ENGINE_NAME}2`, isOnboarding);
    await UrlbarTestUtils.promisePopupClose(window, () => gURLBar.blur());
    // Since the user typed past the autofill for the first engine, we showed a
    // different onboarding result and now we increment
    // tabtosearch_onboard-shown.
    scalars = TelemetryTestUtils.getProcessScalars("parent", true);
    TelemetryTestUtils.assertKeyedScalar(
      scalars,
      "urlbar.tips",
      isOnboarding ? "tabtosearch_onboard-shown" : "tabtosearch-shown",
      3
    );
    TelemetryTestUtils.assertKeyedScalar(
      scalars,
      isOnboarding
        ? "urlbar.tabtosearch.impressions_onboarding"
        : "urlbar.tabtosearch.impressions",
      "other",
      3
    );

    info("Make a typo and return to autofill. Do not record impression.");
    await UrlbarTestUtils.promiseAutocompleteResultPopup({
      window,
      value: `${firstEngineHost}-`,
      fireInputEvent: true,
    });
    await checkForTabToSearchResult(`${ENGINE_NAME}2`, isOnboarding);
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
    await checkForTabToSearchResult(`${ENGINE_NAME}2`, isOnboarding);
    await UrlbarTestUtils.promisePopupClose(window, () => gURLBar.blur());
    scalars = TelemetryTestUtils.getProcessScalars("parent", true);
    TelemetryTestUtils.assertKeyedScalar(
      scalars,
      "urlbar.tips",
      isOnboarding ? "tabtosearch_onboard-shown" : "tabtosearch-shown",
      4
    );
    TelemetryTestUtils.assertKeyedScalar(
      scalars,
      isOnboarding
        ? "urlbar.tabtosearch.impressions_onboarding"
        : "urlbar.tabtosearch.impressions",
      "other",
      4
    );

    info(
      "Cancel then restart autofill. Continue to show the tab-to-search result."
    );
    await UrlbarTestUtils.promiseAutocompleteResultPopup({
      window,
      value: `${firstEngineHost}-2`,
      fireInputEvent: true,
    });
    await checkForTabToSearchResult(`${ENGINE_NAME}2`, isOnboarding);
    let searchPromise = UrlbarTestUtils.promiseSearchComplete(window);
    EventUtils.synthesizeKey("KEY_Backspace");
    await searchPromise;
    await checkForTabToSearchResult(`${ENGINE_NAME}2`, isOnboarding);
    searchPromise = UrlbarTestUtils.promiseSearchComplete(window);
    // Type the "." from `example-2.com`.
    EventUtils.synthesizeKey(".");
    await searchPromise;
    await checkForTabToSearchResult(`${ENGINE_NAME}2`, isOnboarding);
    await UrlbarTestUtils.promisePopupClose(window, () => gURLBar.blur());
    scalars = TelemetryTestUtils.getProcessScalars("parent", true);
    TelemetryTestUtils.assertKeyedScalar(
      scalars,
      "urlbar.tips",
      isOnboarding ? "tabtosearch_onboard-shown" : "tabtosearch-shown",
      5
    );
    TelemetryTestUtils.assertKeyedScalar(
      scalars,
      isOnboarding
        ? "urlbar.tabtosearch.impressions_onboarding"
        : "urlbar.tabtosearch.impressions",
      // "other" is recorded as the engine name because we're not using a built-in engine.
      "other",
      5
    );

    // See javadoc for UrlbarProviderTabToSearch.onEngagement for discussion
    // about retained results.
    info("Reopen the result set with retained results. Record impression.");
    await UrlbarTestUtils.promisePopupOpen(window, () => {
      EventUtils.synthesizeMouseAtCenter(gURLBar.inputField, {});
    });
    await checkForTabToSearchResult(`${ENGINE_NAME}2`, isOnboarding);
    await UrlbarTestUtils.promisePopupClose(window, () => gURLBar.blur());
    scalars = TelemetryTestUtils.getProcessScalars("parent", true);
    TelemetryTestUtils.assertKeyedScalar(
      scalars,
      "urlbar.tips",
      isOnboarding ? "tabtosearch_onboard-shown" : "tabtosearch-shown",
      6
    );
    TelemetryTestUtils.assertKeyedScalar(
      scalars,
      isOnboarding
        ? "urlbar.tabtosearch.impressions_onboarding"
        : "urlbar.tabtosearch.impressions",
      "other",
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
    await checkForTabToSearchResult(ENGINE_NAME, isOnboarding);
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
    await checkForTabToSearchResult(ENGINE_NAME, isOnboarding);
    await UrlbarTestUtils.promisePopupClose(window, () => gURLBar.blur());
    // We clear the scalar this time.
    scalars = TelemetryTestUtils.getProcessScalars("parent", true, true);
    TelemetryTestUtils.assertKeyedScalar(
      scalars,
      "urlbar.tips",
      isOnboarding ? "tabtosearch_onboard-shown" : "tabtosearch-shown",
      8
    );
    TelemetryTestUtils.assertKeyedScalar(
      scalars,
      isOnboarding
        ? "urlbar.tabtosearch.impressions_onboarding"
        : "urlbar.tabtosearch.impressions",
      "other",
      8
    );

    await PlacesUtils.history.clear();
    await extension.unload();
  });
}
