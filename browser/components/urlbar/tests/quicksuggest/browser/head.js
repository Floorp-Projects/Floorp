/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

let sandbox;

/* import-globals-from ../../browser/head-common.js */
Services.scriptloader.loadSubScript(
  "chrome://mochitests/content/browser/browser/components/urlbar/tests/browser/head-common.js",
  this
);

ChromeUtils.defineESModuleGetters(this, {
  CONTEXTUAL_SERVICES_PING_TYPES:
    "resource:///modules/PartnerLinkAttribution.jsm",
  QuickSuggest: "resource:///modules/QuickSuggest.sys.mjs",
  TelemetryTestUtils: "resource://testing-common/TelemetryTestUtils.sys.mjs",
  UrlbarProviderQuickSuggest:
    "resource:///modules/UrlbarProviderQuickSuggest.sys.mjs",
});

XPCOMUtils.defineLazyGetter(this, "QuickSuggestTestUtils", () => {
  const { QuickSuggestTestUtils: Utils } = ChromeUtils.importESModule(
    "resource://testing-common/QuickSuggestTestUtils.sys.mjs"
  );
  return new Utils(this);
});

XPCOMUtils.defineLazyGetter(this, "MerinoTestUtils", () => {
  const { MerinoTestUtils: Utils } = ChromeUtils.importESModule(
    "resource://testing-common/MerinoTestUtils.sys.mjs"
  );
  return new Utils(this);
});

registerCleanupFunction(async () => {
  // Ensure the popup is always closed at the end of each test to avoid
  // interfering with the next test.
  await UrlbarTestUtils.promisePopupClose(window);
});

/**
 * Call this in your setup task if you use `doTelemetryTest()`.
 *
 * @param {object} options
 *   Options
 * @param {Array} options.suggestions
 *   Quick suggest will be initialized with these suggestions. They should be
 *   the suggestions you want to test.
 */
async function setUpTelemetryTest({ suggestions }) {
  await SpecialPowers.pushPrefEnv({
    set: [
      // Enable blocking on primary sponsored and nonsponsored suggestions so we
      // can test the block button.
      ["browser.urlbar.quicksuggest.blockingEnabled", true],
      ["browser.urlbar.bestMatch.blockingEnabled", true],
      // Switch-to-tab results can sometimes appear after the test clicks a help
      // button and closes the new tab, which interferes with the expected
      // indexes of quick suggest results, so disable them.
      ["browser.urlbar.suggest.openpage", false],
      // Disable the persisted-search-terms search tip because it can interfere.
      ["browser.urlbar.tipShownCount.searchTip_persist", 999],
    ],
  });

  await PlacesUtils.history.clear();
  await PlacesUtils.bookmarks.eraseEverything();
  await UrlbarTestUtils.formHistory.clear();

  Services.telemetry.clearScalars();
  Services.telemetry.clearEvents();

  // Add a mock engine so we don't hit the network.
  await SearchTestUtils.installSearchExtension({}, { setAsDefault: true });

  await QuickSuggestTestUtils.ensureQuickSuggestInit(suggestions);
}

/**
 * Main entry point for testing primary telemetry for quick suggest suggestions:
 * impressions, clicks, helps, and blocks. This can be used to declaratively
 * test all primary telemetry for any suggestion type.
 *
 * @param {object} options
 *   Options
 * @param {number} options.index
 *   The expected index of the suggestion in the results list.
 * @param {object} options.suggestion
 *   The suggestion being tested.
 * @param {object} options.impressionOnly
 *   An object describing the expected impression-only telemetry, i.e.,
 *   telemetry recorded when an impression occurs but not a click. It must have
 *   the following properties:
 *     {object} scalars
 *       An object that maps expected scalar names to values.
 *     {object} event
 *       The expected recorded event.
 *     {object} ping
 *       The expected recorded custom telemetry ping.
 * @param {object} options.selectables
 *   An object describing the telemetry that's expected to be recorded when each
 *   selectable element in the suggestion's row is picked. This object maps HTML
 *   class names to objects. Each property's name must be an HTML class name
 *   that uniquely identifies a selectable element within the row. The value
 *   must be an object that describes the telemetry that's expected to be
 *   recorded when that element is picked, and this inner object must have the
 *   following properties:
 *     {object} scalars
 *       An object that maps expected scalar names to values.
 *     {object} event
 *       The expected recorded event.
 *     {Array} pings
 *       A list of expected recorded custom telemetry pings.
 * @param {Function} options.showSuggestion
 *   This function should open the view and show the suggestion.
 */
async function doTelemetryTest({
  index,
  suggestion,
  impressionOnly,
  selectables,
  showSuggestion = () =>
    UrlbarTestUtils.promiseAutocompleteResultPopup({
      window,
      value: suggestion.keywords[0],
      fireInputEvent: true,
    }),
}) {
  // Do the impression-only test. It will return the `classList` values of all
  // the selectable elements in the row so we can use them below.
  let selectableClassLists = await doImpressionOnlyTest({
    index,
    suggestion,
    showSuggestion,
    expected: impressionOnly,
  });
  if (!selectableClassLists) {
    Assert.ok(
      false,
      "Impression test didn't complete successfully, stopping telemetry test"
    );
    return;
  }

  info(
    "Got classLists of actual selectable elements in the row: " +
      JSON.stringify(selectableClassLists)
  );

  let allMatchedExpectedClasses = new Set();

  // For each actual selectable element in the row, do a selectable test by
  // picking the element and checking telemetry.
  for (let classList of selectableClassLists) {
    info(
      "Setting up selectable test for actual element with classList " +
        JSON.stringify(classList)
    );

    // Each of the actual selectable elements should match exactly one of the
    // test's expected selectable classes.
    //
    // * If an element doesn't match any expected class, then the test does not
    //   account for that element, which is an error in the test.
    // * If an element matches more than one expected class, then the expected
    //   class is not specific enough, which is also an error in the test.

    // Collect all the expected classes that match the actual element.
    let matchingExpectedClasses = Object.keys(selectables).filter(className =>
      classList.includes(className)
    );

    if (!matchingExpectedClasses.length) {
      Assert.ok(
        false,
        "Actual selectable element doesn't match any expected classes. The element's classList is " +
          JSON.stringify(classList)
      );
      continue;
    }
    if (matchingExpectedClasses.length > 1) {
      Assert.ok(
        false,
        "Actual selectable element matches multiple expected classes. The element's classList is " +
          JSON.stringify(classList)
      );
      continue;
    }

    let className = matchingExpectedClasses[0];
    allMatchedExpectedClasses.add(className);

    await doSelectableTest({
      suggestion,
      showSuggestion,
      index,
      className,
      expected: selectables[className],
    });
  }

  // Finally, if an expected class doesn't match any actual element, then the
  // test expects an element to be picked that either isn't present or isn't
  // selectable, which is an error in the test.
  Assert.deepEqual(
    Object.keys(selectables).filter(
      className => !allMatchedExpectedClasses.has(className)
    ),
    [],
    "There should be no expected classes that didn't match actual selectable elements"
  );
}

/**
 * Helper for `doTelemetryTest()` that does an impression-only test.
 *
 * @param {object} options
 *   Options
 * @param {number} options.index
 *   The expected index of the suggestion in the results list.
 * @param {object} options.suggestion
 *   The suggestion being tested.
 * @param {object} options.expected
 *   An object describing the expected impression-only telemetry. It must have
 *   the following properties:
 *     {object} scalars
 *       An object that maps expected scalar names to values.
 *     {object} event
 *       The expected recorded event.
 *     {object} ping
 *       The expected custom telemetry ping.
 * @param {Function} options.showSuggestion
 *   This function should open the view and show the suggestion.
 * @returns {Array}
 *   The `classList` values of all the selectable elements in the suggestion's
 *   row. Each item in this array is a selectable element's `classList` that has
 *   been converted to an array of strings.
 */
async function doImpressionOnlyTest({
  index,
  suggestion,
  expected,
  showSuggestion,
}) {
  info("Starting impression-only test");

  Services.telemetry.clearEvents();
  let { spy, spyCleanup } = QuickSuggestTestUtils.createTelemetryPingSpy();

  info("Showing suggestion");
  await showSuggestion();

  // Get the quick suggest row.
  let row = await validateSuggestionRow(index, suggestion);
  if (!row) {
    Assert.ok(
      false,
      "Couldn't get quick suggest row, stopping impression-only test"
    );
    await spyCleanup();
    return null;
  }

  // We need to get a non-quick-suggest row so we can pick it to trigger
  // impression-only telemetry. We also verify no other rows are quick suggests.
  let otherRow;
  let rowCount = UrlbarTestUtils.getResultCount(window);
  for (let i = 0; i < rowCount; i++) {
    if (i != index) {
      let r = await UrlbarTestUtils.waitForAutocompleteResultAt(window, i);
      Assert.notEqual(
        r.result.providerName,
        UrlbarProviderQuickSuggest.name,
        "No other row should be a quick suggest: index = " + i
      );
      otherRow = otherRow || r;
    }
  }
  if (!otherRow) {
    Assert.ok(
      false,
      "Couldn't get non-quick-suggest row, stopping impression-only test"
    );
    await spyCleanup();
    return null;
  }

  // Collect the `classList` values for all selectable elements in the row.
  let selectableClassLists = [];
  let selectables = row.querySelectorAll(":is([selectable], [role=button])");
  for (let element of selectables) {
    selectableClassLists.push([...element.classList]);
  }

  // Pick the non-quick-suggest row. Assumptions:
  // * The middle of the row is selectable
  // * Picking the row will load a page
  info("Clicking non-quick-suggest row and waiting for view to close");
  let loadPromise = BrowserTestUtils.browserLoaded(gBrowser.selectedBrowser);
  await UrlbarTestUtils.promisePopupClose(window, () =>
    EventUtils.synthesizeMouseAtCenter(otherRow, {})
  );

  info("Waiting for page to load after clicking non-quick-suggest row");
  await loadPromise;

  // Check telemetry.
  QuickSuggestTestUtils.assertScalars(expected.scalars);
  QuickSuggestTestUtils.assertEvents([expected.event]);
  QuickSuggestTestUtils.assertPings(spy, [expected.ping]);

  // Clean up.
  await PlacesUtils.history.clear();
  await UrlbarTestUtils.formHistory.clear();
  await spyCleanup();

  return selectableClassLists;
}

/**
 * Helper for `doTelemetryTest()` that picks a selectable element in a
 * suggestion's row and checks telemetry.
 *
 * @param {object} options
 *   Options
 * @param {number} options.index
 *   The expected index of the suggestion in the results list.
 * @param {object} options.suggestion
 *   The suggestion being tested.
 * @param {string} options.className
 *   An HTML class name that should uniquely identify the selectable element
 *   within its row.
 * @param {object} options.expected
 *   An object describing the telemetry that's expected to be recorded when the
 *   selectable element is picked. It must have the following properties:
 *     {object} scalars
 *       An object that maps expected scalar names to values.
 *     {object} event
 *       The expected recorded event.
 *     {Array} pings
 *       A list of expected custom telemetry pings.
 * @param {Function} options.showSuggestion
 *   This function should open the view and show the suggestion.
 */
async function doSelectableTest({
  index,
  suggestion,
  className,
  expected,
  showSuggestion,
}) {
  info("Starting selectable test: " + JSON.stringify({ className }));

  Services.telemetry.clearEvents();
  let { spy, spyCleanup } = QuickSuggestTestUtils.createTelemetryPingSpy();

  await showSuggestion();

  let row = await validateSuggestionRow(index, suggestion);
  if (!row) {
    Assert.ok(
      false,
      "Couldn't get quick suggest row, stopping selectable test"
    );
    await spyCleanup();
    return;
  }

  let element = row.querySelector("." + className);
  Assert.ok(element, "Sanity check: Target selectable element should exist");

  let helpLoadPromise;
  if (className == "urlbarView-button-help") {
    helpLoadPromise = BrowserTestUtils.waitForNewTab(gBrowser);
  }

  EventUtils.synthesizeMouseAtCenter(element, {});

  QuickSuggestTestUtils.assertScalars(expected.scalars);
  QuickSuggestTestUtils.assertEvents([expected.event]);
  QuickSuggestTestUtils.assertPings(spy, expected.pings);

  if (helpLoadPromise) {
    await helpLoadPromise;
    await TestUtils.waitForTick();
    BrowserTestUtils.removeTab(gBrowser.selectedTab);
  }

  if (className == "urlbarView-button-block") {
    await QuickSuggest.blockedSuggestions.clear();
  }

  await PlacesUtils.history.clear();
  await spyCleanup();
}

/**
 * Gets a row in the view, which is assumed to be open, and asserts that it's a
 * particular quick suggest row. If it is, the row is returned. If it's not,
 * null is returned.
 *
 * @param {number} index
 *   The expected index of the quick suggest row.
 * @param {object} suggestion
 *   The expected suggestion.
 * @returns {Element}
 *   If the row is the expected suggestion, the row element is returned.
 *   Otherwise null is returned.
 */
async function validateSuggestionRow(index, suggestion) {
  let rowCount = UrlbarTestUtils.getResultCount(window);
  Assert.less(
    index,
    rowCount,
    "Expected quick suggest row index should be < row count"
  );
  if (rowCount <= index) {
    return null;
  }

  let row = await UrlbarTestUtils.waitForAutocompleteResultAt(window, index);
  Assert.equal(
    row.result.providerName,
    UrlbarProviderQuickSuggest.name,
    "Expected quick suggest row should actually be a quick suggest"
  );
  Assert.equal(
    row.result.payload.url,
    suggestion.url,
    "The quick suggest row should represent the expected suggestion"
  );
  if (
    row.result.providerName != UrlbarProviderQuickSuggest.name ||
    row.result.payload.url != suggestion.url
  ) {
    return null;
  }

  return row;
}
