/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

/**
 * This file tests urlbar telemetry for Quick Suggest results.
 */

"use strict";

XPCOMUtils.defineLazyModuleGetters(this, {
  CONTEXTUAL_SERVICES_PING_TYPES:
    "resource:///modules/PartnerLinkAttribution.jsm",
  PartnerLinkAttribution: "resource:///modules/PartnerLinkAttribution.jsm",
  UrlbarProviderQuickSuggest:
    "resource:///modules/UrlbarProviderQuickSuggest.jsm",
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
    click_url: "http://click.reporting.test.com/",
    impression_url: "http://impression.reporting.test.com/",
    advertiser: "Test-Advertiser",
  },
];

const TEST_HELP_URL = "http://example.com/help";

const TELEMETRY_SCALARS = {
  IMPRESSION: "contextual.services.quicksuggest.impression",
  CLICK: "contextual.services.quicksuggest.click",
  HELP: "contextual.services.quicksuggest.help",
};

const TELEMETRY_EVENT_CATEGORY = "contextservices.quicksuggest";

const EXPERIMENT_PREF = "browser.urlbar.quicksuggest.enabled";
const SUGGEST_PREF = "suggest.quicksuggest";

// Spy for the custom impression/click sender
let spy;

add_task(async function init() {
  sandbox = sinon.createSandbox();
  spy = sandbox.spy(
    PartnerLinkAttribution._pingCentre,
    "sendStructuredIngestionPing"
  );
  sandbox.stub(UrlbarQuickSuggest, "_setupRemoteSettings").resolves(true);

  await PlacesUtils.history.clear();
  await UrlbarTestUtils.formHistory.clear();
  await SpecialPowers.pushPrefEnv({
    set: [
      [EXPERIMENT_PREF, true],
      ["browser.urlbar.suggest.searches", true],
      ["browser.urlbar.quicksuggest.showedOnboardingDialog", true],
    ],
  });

  // Add a mock engine so we don't hit the network.
  await SearchTestUtils.installSearchExtension();
  let oldDefaultEngine = await Services.search.getDefault();
  Services.search.setDefault(Services.search.getEngineByName("Example"));

  // Set up Quick Suggest.
  await UrlbarQuickSuggest.init();
  await UrlbarQuickSuggest._processSuggestionsJSON(TEST_DATA);
  UrlbarProviderQuickSuggest._helpUrl = TEST_HELP_URL;

  // Enable local telemetry recording for the duration of the test.
  let oldCanRecord = Services.telemetry.canRecordExtended;
  Services.telemetry.canRecordExtended = true;

  Services.telemetry.clearScalars();

  registerCleanupFunction(async () => {
    sandbox.restore();
    Services.search.setDefault(oldDefaultEngine);
    Services.telemetry.canRecordExtended = oldCanRecord;
    delete UrlbarProviderQuickSuggest._helpUrl;
  });
});

// Tests the impression scalar and the custom impression ping.
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
    assertCustomImpression(index);
  });
});

// Makes sure the impression scalar and the custom impression are not incremented
// when the urlbar engagement is abandoned.
add_task(async function noImpression_abandonment() {
  await BrowserTestUtils.withNewTab("about:blank", async () => {
    spy.resetHistory();
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
    assertNoCustomImpression();
  });
});

// Makes sure the impression scalar and the custom impression are not incremented
// when a Quick Suggest result is not present.
add_task(async function noImpression_noQuickSuggestResult() {
  await BrowserTestUtils.withNewTab("about:blank", async () => {
    spy.resetHistory();
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
    assertNoCustomImpression();
  });
});

// Tests the click scalar and the custom click ping by picking a Quick Suggest
// result with the keyboard.
add_task(async function click_keyboard() {
  await BrowserTestUtils.withNewTab("about:blank", async () => {
    spy.resetHistory();
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
    assertCustomClick(index);
  });
});

// Tests the click scalar and the custom click ping by picking a Quick Suggest
// result with the mouse.
add_task(async function click_mouse() {
  await BrowserTestUtils.withNewTab("about:blank", async () => {
    spy.resetHistory();
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
    assertCustomClick(index);
  });
});

// Tests the help scalar by picking a Quick Suggest result help button with the
// keyboard.
add_task(async function help_keyboard() {
  spy.resetHistory();
  await UrlbarTestUtils.promiseAutocompleteResultPopup({
    window,
    value: TEST_SEARCH_STRING,
    fireInputEvent: true,
  });
  let index = 1;
  let result = await assertIsQuickSuggest(index);
  let helpButton = result.element.row._elements.get("helpButton");
  Assert.ok(helpButton, "The result has a help button");
  let helpLoadPromise = BrowserTestUtils.waitForNewTab(gBrowser);
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
  assertNoCustomClick();
  BrowserTestUtils.removeTab(gBrowser.selectedTab);
});

// Tests the help scalar by picking a Quick Suggest result help button with the
// mouse.
add_task(async function help_mouse() {
  spy.resetHistory();
  await UrlbarTestUtils.promiseAutocompleteResultPopup({
    window,
    value: TEST_SEARCH_STRING,
    fireInputEvent: true,
  });
  let index = 1;
  let result = await assertIsQuickSuggest(index);
  let helpButton = result.element.row._elements.get("helpButton");
  Assert.ok(helpButton, "The result has a help button");
  let helpLoadPromise = BrowserTestUtils.waitForNewTab(gBrowser);
  await UrlbarTestUtils.promisePopupClose(window, () => {
    EventUtils.synthesizeMouseAtCenter(helpButton, {});
  });
  await helpLoadPromise;
  Assert.equal(gBrowser.currentURI.spec, TEST_HELP_URL, "Help URL loaded");
  assertScalars({
    [TELEMETRY_SCALARS.IMPRESSION]: index + 1,
    [TELEMETRY_SCALARS.HELP]: index + 1,
  });
  assertNoCustomClick();
  BrowserTestUtils.removeTab(gBrowser.selectedTab);
});

// Tests the contextservices.quicksuggest enable_toggled event telemetry by
// toggling the suggest.quicksuggest pref.
add_task(async function enableToggled() {
  Services.telemetry.clearEvents();

  // Toggle the suggest.quicksuggest pref twice.  We should get two events.
  let enabled = UrlbarPrefs.get(SUGGEST_PREF);
  for (let i = 0; i < 2; i++) {
    enabled = !enabled;
    UrlbarPrefs.set(SUGGEST_PREF, enabled);
    TelemetryTestUtils.assertEvents([
      {
        category: TELEMETRY_EVENT_CATEGORY,
        method: "enable_toggled",
        object: enabled ? "enabled" : "disabled",
      },
    ]);
  }

  // Set the main quicksuggest.enabled pref to false and toggle the
  // suggest.quicksuggest pref again.  We shouldn't get any events.
  await SpecialPowers.pushPrefEnv({
    set: [[EXPERIMENT_PREF, false]],
  });
  enabled = !enabled;
  UrlbarPrefs.set(SUGGEST_PREF, enabled);
  TelemetryTestUtils.assertEvents([], { category: TELEMETRY_EVENT_CATEGORY });
  await SpecialPowers.popPrefEnv();

  UrlbarPrefs.clear(SUGGEST_PREF);
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

/**
 * Asserts that a custom impression ping is sent with the expected payload.
 *
 * @param {number} [index]
 *   The expected index of the Quick Suggest result.
 */
function assertCustomImpression(index) {
  Assert.ok(spy.calledOnce, "Should send a custom impression ping");
  // Validate the impression ping
  let [payload, endpoint] = spy.firstCall.args;
  Assert.ok(
    endpoint.includes(CONTEXTUAL_SERVICES_PING_TYPES.QS_IMPRESSION),
    "Should set the endpoint for QuickSuggest impression"
  );
  Assert.ok(!!payload.context_id, "Should set the context_id");
  Assert.equal(
    payload.advertiser,
    "test-advertiser",
    "Should set the advertiser"
  );
  Assert.equal(
    payload.reporting_url,
    "http://impression.reporting.test.com/",
    "Should set the impression reporting URL"
  );
  Assert.equal(payload.block_id, 1, "Should set the block_id");
  Assert.equal(payload.position, index + 1, "Should set the position");
  Assert.equal(
    payload.search_query,
    TEST_SEARCH_STRING,
    "Should set the search_query"
  );
  Assert.equal(
    payload.matched_keywords,
    TEST_SEARCH_STRING,
    "Should set the matched_keywords"
  );
}

/**
 * Asserts no custom impression ping is sent.
 */
function assertNoCustomImpression() {
  Assert.ok(spy.notCalled, "Should not send a custom impression");
}

/**
 * Asserts that a custom click ping is sent with the expected payload.
 *
 * @param {number} [index]
 *   The expected index of the Quick Suggest result.
 */
function assertCustomClick(index) {
  // Impression is sent first, followed by the click
  Assert.ok(spy.calledTwice, "Should send a custom impression ping");
  // Validate the impression ping
  let [payload, endpoint] = spy.secondCall.args;
  Assert.ok(
    endpoint.includes(CONTEXTUAL_SERVICES_PING_TYPES.QS_SELECTION),
    "Should set the endpoint for QuickSuggest click"
  );
  Assert.ok(!!payload.context_id, "Should set the context_id");
  Assert.equal(
    payload.advertiser,
    "test-advertiser",
    "Should set the advertiser"
  );
  Assert.equal(
    payload.reporting_url,
    "http://click.reporting.test.com/",
    "Should set the click reporting URL"
  );
  Assert.equal(payload.block_id, 1, "Should set the block_id");
  Assert.equal(payload.position, index + 1, "Should set the position");
}

/**
 * Asserts no custom click ping is sent.
 */
function assertNoCustomClick() {
  // Only called once for the impression
  Assert.ok(spy.calledOnce, "Should not send a custom impression");
}
