/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

/**
 * This file tests urlbar telemetry for Quick Suggest results.
 */

"use strict";

XPCOMUtils.defineLazyModuleGetters(this, {
  UrlbarQuickSuggest: "resource:///modules/UrlbarQuickSuggest.jsm",
});

const TEST_SJS =
  "http://mochi.test:8888/browser/browser/components/urlbar/tests/browser/quicksuggest.sjs";
const TEST_URL = TEST_SJS + "?q=frabbits";
const TEST_SEARCH_STRING = "frab";
const TEST_DATA = [
  {
    id: 1,
    url: TEST_URL,
    title: "frabbits",
    keywords: [TEST_SEARCH_STRING],
  },
];

const TEST_HELP_URL = "http://example.com/help";

const TELEMETRY_SCALARS = {
  IMPRESSION: "contextual.services.quicksuggest.impression",
  CLICK: "contextual.services.quicksuggest.click",
  HELP: "contextual.services.quicksuggest.help",
};

const ONBOARDING_COUNT_PREF = "quicksuggest.onboardingCount";

add_task(async function init() {
  await PlacesUtils.history.clear();
  await UrlbarTestUtils.formHistory.clear();
  await SpecialPowers.pushPrefEnv({
    set: [
      ["browser.urlbar.quicksuggest.enabled", true],
      ["browser.urlbar.quicksuggest.helpURL", TEST_HELP_URL],
      ["browser.urlbar.suggest.searches", true],
    ],
  });

  // Add a mock engine so we don't hit the network.
  let engine = await Services.search.addEngineWithDetails("Test", {
    template: "http://example.com/?search={searchTerms}",
  });
  let oldDefaultEngine = await Services.search.getDefault();
  Services.search.setDefault(engine);

  // Set up Quick Suggest.
  await UrlbarQuickSuggest.init();
  await UrlbarQuickSuggest._processSuggestionsJSON(TEST_DATA);

  // Enable local telemetry recording for the duration of the test.
  let oldCanRecord = Services.telemetry.canRecordExtended;
  Services.telemetry.canRecordExtended = true;

  Services.telemetry.clearScalars();

  registerCleanupFunction(async () => {
    Services.search.setDefault(oldDefaultEngine);
    await Services.search.removeEngine(engine);
    Services.telemetry.canRecordExtended = oldCanRecord;
  });
});

// Tests the impression scalar.
add_task(async function impression() {
  await BrowserTestUtils.withNewTab("about:blank", async () => {
    await UrlbarTestUtils.promiseAutocompleteResultPopup({
      window,
      value: TEST_SEARCH_STRING,
      fireInputEvent: true,
    });
    let index = 1;
    await assertIsQuickSuggest(index);
    await UrlbarTestUtils.promisePopupClose(window, () => {
      EventUtils.synthesizeKey("KEY_Enter");
    });
    assertScalars({ [TELEMETRY_SCALARS.IMPRESSION]: index + 1 });
  });
});

// Makes sure the impression scalar is not incremented when the urlbar
// engagement is abandoned.
add_task(async function noImpression_abandonment() {
  await BrowserTestUtils.withNewTab("about:blank", async () => {
    await UrlbarTestUtils.promiseAutocompleteResultPopup({
      window,
      value: TEST_SEARCH_STRING,
      fireInputEvent: true,
    });
    await assertIsQuickSuggest();
    await UrlbarTestUtils.promisePopupClose(window, () => {
      gURLBar.blur();
    });
    assertScalars({});
  });
});

// Makes sure the impression scalar is not incremented when a Quick Suggest
// result is not present.
add_task(async function noImpression_noQuickSuggestResult() {
  await BrowserTestUtils.withNewTab("about:blank", async () => {
    await UrlbarTestUtils.promiseAutocompleteResultPopup({
      window,
      value: "noImpression_noQuickSuggestResult",
      fireInputEvent: true,
    });
    await assertNoQuickSuggestResults();
    await UrlbarTestUtils.promisePopupClose(window, () => {
      EventUtils.synthesizeKey("KEY_Enter");
    });
    assertScalars({});
  });
});

// Tests the click scalar by picking a Quick Suggest result with the keyboard.
add_task(async function click_keyboard() {
  await BrowserTestUtils.withNewTab("about:blank", async () => {
    await UrlbarTestUtils.promiseAutocompleteResultPopup({
      window,
      value: TEST_SEARCH_STRING,
      fireInputEvent: true,
    });
    let index = 1;
    await assertIsQuickSuggest(index);
    await UrlbarTestUtils.promisePopupClose(window, () => {
      EventUtils.synthesizeKey("KEY_ArrowDown");
      EventUtils.synthesizeKey("KEY_Enter");
    });
    assertScalars({
      [TELEMETRY_SCALARS.IMPRESSION]: index + 1,
      [TELEMETRY_SCALARS.CLICK]: index + 1,
    });
  });
});

// Tests the click scalar by picking a Quick Suggest result with the mouse.
add_task(async function click_mouse() {
  await BrowserTestUtils.withNewTab("about:blank", async () => {
    await UrlbarTestUtils.promiseAutocompleteResultPopup({
      window,
      value: TEST_SEARCH_STRING,
      fireInputEvent: true,
    });
    let index = 1;
    let result = await assertIsQuickSuggest(index);
    await UrlbarTestUtils.promisePopupClose(window, () => {
      EventUtils.synthesizeMouseAtCenter(result.element.row, {});
    });
    assertScalars({
      [TELEMETRY_SCALARS.IMPRESSION]: index + 1,
      [TELEMETRY_SCALARS.CLICK]: index + 1,
    });
  });
});

// Tests the help scalar by picking a Quick Suggest result help button with the
// keyboard.
add_task(async function help_keyboard() {
  UrlbarPrefs.clear(ONBOARDING_COUNT_PREF);
  await BrowserTestUtils.withNewTab("about:blank", async () => {
    await UrlbarTestUtils.promiseAutocompleteResultPopup({
      window,
      value: TEST_SEARCH_STRING,
      fireInputEvent: true,
    });
    let index = 1;
    let result = await assertIsQuickSuggest(index);
    let helpButton = result.element.row._elements.get("helpButton");
    Assert.ok(helpButton, "The result has an onboarding help button");
    let helpLoadPromise = BrowserTestUtils.browserLoaded(
      gBrowser.selectedBrowser
    );
    await UrlbarTestUtils.promisePopupClose(window, () => {
      EventUtils.synthesizeKey("KEY_ArrowDown", { repeat: 2 });
      EventUtils.synthesizeKey("KEY_Enter");
    });
    await helpLoadPromise;
    Assert.equal(gBrowser.currentURI.spec, TEST_HELP_URL, "Help URL loaded");
    assertScalars({
      [TELEMETRY_SCALARS.IMPRESSION]: index + 1,
      [TELEMETRY_SCALARS.HELP]: index + 1,
    });
  });
});

// Tests the help scalar by picking a Quick Suggest result help button with the
// mouse.
add_task(async function help_mouse() {
  UrlbarPrefs.clear(ONBOARDING_COUNT_PREF);
  await BrowserTestUtils.withNewTab("about:blank", async () => {
    await UrlbarTestUtils.promiseAutocompleteResultPopup({
      window,
      value: TEST_SEARCH_STRING,
      fireInputEvent: true,
    });
    let index = 1;
    let result = await assertIsQuickSuggest(index);
    let helpButton = result.element.row._elements.get("helpButton");
    Assert.ok(helpButton, "The result has an onboarding help button");
    let helpLoadPromise = BrowserTestUtils.browserLoaded(
      gBrowser.selectedBrowser
    );
    await UrlbarTestUtils.promisePopupClose(window, () => {
      EventUtils.synthesizeMouseAtCenter(helpButton, {});
    });
    await helpLoadPromise;
    Assert.equal(gBrowser.currentURI.spec, TEST_HELP_URL, "Help URL loaded");
    assertScalars({
      [TELEMETRY_SCALARS.IMPRESSION]: index + 1,
      [TELEMETRY_SCALARS.HELP]: index + 1,
    });
  });
});

/**
 * Checks the values of all the Quick Suggest scalars.
 *
 * @param {object} expectedIndexesByScalarName
 *   Maps scalar names to the expected 1-based indexes of results.  If you
 *   expect a scalar to be incremented, then include it in this object.  If you
 *   expect a scalar not to be incremented, don't include it.
 */
function assertScalars(expectedIndexesByScalarName) {
  let scalars = TelemetryTestUtils.getProcessScalars("parent", true, true);
  for (let scalarName of Object.values(TELEMETRY_SCALARS)) {
    if (scalarName in expectedIndexesByScalarName) {
      TelemetryTestUtils.assertKeyedScalar(
        scalars,
        scalarName,
        expectedIndexesByScalarName[scalarName],
        1
      );
    } else {
      Assert.ok(
        !(scalarName in scalars),
        "Scalar should not be present: " + scalarName
      );
    }
  }
}

/**
 * Asserts that a result is a Quick Suggest result.
 *
 * @param {number} [index]
 *   The expected index of the Quick Suggest result.  Pass -1 to use the index
 *   of the last result.
 * @returns {result}
 *   The result at the given index.
 */
async function assertIsQuickSuggest(index = -1) {
  if (index < 0) {
    index = UrlbarTestUtils.getResultCount(window) - 1;
    Assert.greater(index, -1, "Sanity check: Result count should be > 0");
  }
  let result = await UrlbarTestUtils.getDetailsOfResultAt(window, index);
  Assert.equal(result.type, UrlbarUtils.RESULT_TYPE.URL, "Result type");
  Assert.equal(result.url, TEST_URL, "Result URL");
  Assert.ok(result.isSponsored, "Result isSponsored");
  return result;
}

/**
 * Asserts that none of the results are Quick Suggest results.
 */
async function assertNoQuickSuggestResults() {
  for (let i = 0; i < UrlbarTestUtils.getResultCount(window); i++) {
    let r = await UrlbarTestUtils.getDetailsOfResultAt(window, i);
    Assert.ok(
      r.type != UrlbarUtils.RESULT_TYPE.URL ||
        !r.url.includes(TEST_URL) ||
        !r.isSponsored,
      `Result at index ${i} should not be a QuickSuggest result`
    );
  }
}
