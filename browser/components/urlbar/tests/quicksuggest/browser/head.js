/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

let sandbox;

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

ChromeUtils.defineLazyGetter(this, "QuickSuggestTestUtils", () => {
  const { QuickSuggestTestUtils: module } = ChromeUtils.importESModule(
    "resource://testing-common/QuickSuggestTestUtils.sys.mjs"
  );
  module.init(this);
  return module;
});

ChromeUtils.defineLazyGetter(this, "MerinoTestUtils", () => {
  const { MerinoTestUtils: module } = ChromeUtils.importESModule(
    "resource://testing-common/MerinoTestUtils.sys.mjs"
  );
  module.init(this);
  return module;
});

ChromeUtils.defineLazyGetter(this, "PlacesFrecencyRecalculator", () => {
  return Cc["@mozilla.org/places/frecency-recalculator;1"].getService(
    Ci.nsIObserver
  ).wrappedJSObject;
});

registerCleanupFunction(async () => {
  // Ensure the popup is always closed at the end of each test to avoid
  // interfering with the next test.
  await UrlbarTestUtils.promisePopupClose(window);
});

/**
 * Updates the Top Sites feed.
 *
 * @param {Function} condition
 *   A callback that returns true after Top Sites are successfully updated.
 * @param {boolean} searchShortcuts
 *   True if Top Sites search shortcuts should be enabled.
 */
async function updateTopSites(condition, searchShortcuts = false) {
  // Toggle the pref to clear the feed cache and force an update.
  await SpecialPowers.pushPrefEnv({
    set: [
      [
        "browser.newtabpage.activity-stream.discoverystream.endpointSpocsClear",
        "",
      ],
      ["browser.newtabpage.activity-stream.feeds.system.topsites", false],
      ["browser.newtabpage.activity-stream.feeds.system.topsites", true],
      [
        "browser.newtabpage.activity-stream.improvesearch.topSiteSearchShortcuts",
        searchShortcuts,
      ],
    ],
  });

  // Wait for the feed to be updated.
  await TestUtils.waitForCondition(() => {
    let sites = AboutNewTab.getTopSites();
    return condition(sites);
  }, "Waiting for top sites to be updated");
}

/**
 * Call this in your setup task if you use `doTelemetryTest()`.
 *
 * @param {object} options
 *   Options
 * @param {Array} options.remoteSettingsResults
 *   Array of remote settings result objects. If not given, no suggestions
 *   will be present in remote settings.
 * @param {Array} options.merinoSuggestions
 *   Array of Merino suggestion objects. If given, this function will start
 *   the mock Merino server and set `quicksuggest.dataCollection.enabled` to
 *   true so that `UrlbarProviderQuickSuggest` will fetch suggestions from it.
 *   Otherwise Merino will not serve suggestions, but you can still set up
 *   Merino without using this function by using `MerinoTestUtils` directly.
 * @param {Array} options.config
 *   Quick suggest will be initialized with this config. Leave undefined to use
 *   the default config. See `QuickSuggestTestUtils` for details.
 */
async function setUpTelemetryTest({
  remoteSettingsResults,
  merinoSuggestions = null,
  config = QuickSuggestTestUtils.DEFAULT_CONFIG,
}) {
  if (UrlbarPrefs.get("resultMenu")) {
    todo(
      false,
      "telemetry for the result menu to be implemented in bug 1790020"
    );
    await SpecialPowers.pushPrefEnv({
      set: [["browser.urlbar.resultMenu", false]],
    });
  }
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

  await QuickSuggestTestUtils.ensureQuickSuggestInit({
    remoteSettingsResults,
    merinoSuggestions,
    config,
  });
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
 *       The expected recorded custom telemetry ping. If no ping is expected,
 *       leave this undefined or pass null.
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
 *       A list of expected recorded custom telemetry pings. If no pings are
 *       expected, pass an empty array.
 * @param {string} options.providerName
 *   The name of the provider that is expected to create the UrlbarResult for
 *   the suggestion.
 * @param {Function} options.teardown
 *   If given, this function will be called after each selectable test. If
 *   picking an element causes side effects that need to be cleaned up before
 *   starting the next selectable test, they can be cleaned up here.
 * @param {Function} options.showSuggestion
 *   This function should open the view and show the suggestion.
 */
async function doTelemetryTest({
  index,
  suggestion,
  impressionOnly,
  selectables,
  providerName = UrlbarProviderQuickSuggest.name,
  teardown = null,
  showSuggestion = () =>
    UrlbarTestUtils.promiseAutocompleteResultPopup({
      window,
      // If the suggestion object is a remote settings result, it will have a
      // `keywords` property. Otherwise the suggestion object must be a Merino
      // suggestion, and the search string doesn't matter in that case because
      // the mock Merino server will be set up to return suggestions regardless.
      value: suggestion.keywords?.[0] || "test",
      fireInputEvent: true,
    }),
}) {
  // Do the impression-only test. It will return the `classList` values of all
  // the selectable elements in the row so we can use them below.
  let selectableClassLists = await doImpressionOnlyTest({
    index,
    suggestion,
    providerName,
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
      providerName,
      showSuggestion,
      index,
      className,
      expected: selectables[className],
    });

    if (teardown) {
      info("Calling teardown");
      await teardown();
      info("Finished teardown");
    }
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
 * @param {string} options.providerName
 *   The name of the provider that is expected to create the UrlbarResult for
 *   the suggestion.
 * @param {object} options.expected
 *   An object describing the expected impression-only telemetry. It must have
 *   the following properties:
 *     {object} scalars
 *       An object that maps expected scalar names to values.
 *     {object} event
 *       The expected recorded event.
 *     {object} ping
 *       The expected recorded custom telemetry ping. If no ping is expected,
 *       leave this undefined or pass null.
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
  providerName,
  expected,
  showSuggestion,
}) {
  info("Starting impression-only test");

  Services.telemetry.clearEvents();
  let { spy, spyCleanup } = QuickSuggestTestUtils.createTelemetryPingSpy();

  let gleanPingSubmitted = false;
  GleanPings.quickSuggest.testBeforeNextSubmit(() => {
    gleanPingSubmitted = true;
    if (!expected.ping || !("type" in expected.ping)) {
      return;
    }
    _assertGleanPing(expected.ping);
  });

  info("Showing suggestion");
  await showSuggestion();

  // Get the suggestion row.
  let row = await validateSuggestionRow(index, suggestion, providerName);
  if (!row) {
    Assert.ok(
      false,
      "Couldn't get suggestion row, stopping impression-only test"
    );
    await spyCleanup();
    return null;
  }

  // We need to get a different selectable row so we can pick it to trigger
  // impression-only telemetry. For simplicity we'll look for a row that will
  // load a URL when picked. We'll also verify no other rows are from the
  // expected provider.
  let otherRow;
  let rowCount = UrlbarTestUtils.getResultCount(window);
  for (let i = 0; i < rowCount; i++) {
    if (i != index) {
      let r = await UrlbarTestUtils.waitForAutocompleteResultAt(window, i);
      Assert.notEqual(
        r.result.providerName,
        providerName,
        "No other row should be from expected provider: index = " + i
      );
      if (
        !otherRow &&
        (r.result.payload.url ||
          (r.result.type == UrlbarUtils.RESULT_TYPE.SEARCH &&
            (r.result.payload.query || r.result.payload.suggestion))) &&
        r.hasAttribute("row-selectable")
      ) {
        otherRow = r;
      }
    }
  }
  if (!otherRow) {
    Assert.ok(
      false,
      "Couldn't get a different selectable row with a URL, stopping impression-only test"
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

  // Pick the different row. Assumptions:
  // * The middle of the row is selectable
  // * Picking the row will load a page
  info("Clicking different row and waiting for view to close");
  let loadPromise = BrowserTestUtils.browserLoaded(gBrowser.selectedBrowser);
  await UrlbarTestUtils.promisePopupClose(window, () =>
    EventUtils.synthesizeMouseAtCenter(otherRow, {})
  );

  info("Waiting for page to load after clicking different row");
  await loadPromise;

  // Check telemetry.
  info("Checking scalars. Expected: " + JSON.stringify(expected.scalars));
  QuickSuggestTestUtils.assertScalars(expected.scalars);

  info("Checking events. Expected: " + JSON.stringify([expected.event]));
  QuickSuggestTestUtils.assertEvents([expected.event]);

  let expectedPings = expected.ping ? [expected.ping] : [];
  info("Checking pings. Expected: " + JSON.stringify(expectedPings));
  QuickSuggestTestUtils.assertPings(spy, expectedPings);

  Assert.ok(
    !expected.ping || gleanPingSubmitted,
    "No ping checked or Glean ping submitted ok."
  );

  // Clean up.
  await PlacesUtils.history.clear();
  await UrlbarTestUtils.formHistory.clear();
  await spyCleanup();

  info("Finished impression-only test");

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
 * @param {string} options.providerName
 *   The name of the provider that is expected to create the UrlbarResult for
 *   the suggestion.
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
 *       A list of expected recorded custom telemetry pings. If no pings are
 *       expected, leave this undefined or pass an empty array.
 * @param {Function} options.showSuggestion
 *   This function should open the view and show the suggestion.
 */
async function doSelectableTest({
  index,
  suggestion,
  providerName,
  className,
  expected,
  showSuggestion,
}) {
  info("Starting selectable test: " + JSON.stringify({ className }));

  Services.telemetry.clearEvents();
  let { spy, spyCleanup } = QuickSuggestTestUtils.createTelemetryPingSpy();

  let gleanPingsSubmitted = 0;
  if (expected.pings) {
    let checkPing = (ping, next) => {
      gleanPingsSubmitted++;
      _assertGleanPing(ping);
      if (next) {
        GleanPings.quickSuggest.testBeforeNextSubmit(next);
      }
    };
    // Build the chain of `testBeforeNextSubmit`s backwards.
    let next = undefined;
    expected.pings
      .slice()
      .reverse()
      .forEach(ping => {
        next = checkPing.bind(null, ping, next);
      });
    GleanPings.quickSuggest.testBeforeNextSubmit(next);
  }

  info("Showing suggestion");
  await showSuggestion();

  let row = await validateSuggestionRow(index, suggestion, providerName);
  if (!row) {
    Assert.ok(false, "Couldn't get suggestion row, stopping selectable test");
    await spyCleanup();
    return;
  }

  let element = row.querySelector("." + className);
  Assert.ok(element, "Sanity check: Target selectable element should exist");

  let loadPromise;
  if (className == "urlbarView-row-inner") {
    // We assume clicking the row-inner will cause a page to load in the current
    // browser.
    loadPromise = BrowserTestUtils.browserLoaded(gBrowser.selectedBrowser);
  } else if (className == "urlbarView-button-help") {
    loadPromise = BrowserTestUtils.waitForNewTab(gBrowser);
  }

  info("Clicking element: " + className);
  EventUtils.synthesizeMouseAtCenter(element, {});

  if (loadPromise) {
    info("Waiting for load");
    await loadPromise;
    await TestUtils.waitForTick();
    if (className == "urlbarView-button-help") {
      info("Closing help tab");
      BrowserTestUtils.removeTab(gBrowser.selectedTab);
    }
  }

  info("Checking scalars. Expected: " + JSON.stringify(expected.scalars));
  QuickSuggestTestUtils.assertScalars(expected.scalars);

  info("Checking events. Expected: " + JSON.stringify([expected.event]));
  QuickSuggestTestUtils.assertEvents([expected.event]);

  let expectedPings = expected.pings ?? [];
  info("Checking pings. Expected: " + JSON.stringify(expectedPings));
  QuickSuggestTestUtils.assertPings(spy, expectedPings);

  Assert.equal(
    expectedPings.length,
    gleanPingsSubmitted,
    "Submitted one Glean ping per PC ping."
  );

  if (className == "urlbarView-button-block") {
    await QuickSuggest.blockedSuggestions.clear();
  }
  await PlacesUtils.history.clear();
  await spyCleanup();

  info("Finished selectable test: " + JSON.stringify({ className }));
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
 * @param {string} providerName
 *   The name of the provider that is expected to create the UrlbarResult for
 *   the suggestion.
 * @returns {Element}
 *   If the row is the expected suggestion, the row element is returned.
 *   Otherwise null is returned.
 */
async function validateSuggestionRow(index, suggestion, providerName) {
  let rowCount = UrlbarTestUtils.getResultCount(window);
  Assert.less(
    index,
    rowCount,
    "Expected suggestion row index should be < row count"
  );
  if (rowCount <= index) {
    return null;
  }

  let row = await UrlbarTestUtils.waitForAutocompleteResultAt(window, index);
  Assert.equal(
    row.result.providerName,
    providerName,
    "Expected suggestion row should be from expected provider"
  );
  Assert.equal(
    row.result.payload.url,
    suggestion.url,
    "The suggestion row should represent the expected suggestion"
  );
  if (
    row.result.providerName != providerName ||
    row.result.payload.url != suggestion.url
  ) {
    return null;
  }

  return row;
}

function _assertGleanPing(ping) {
  Assert.equal(Glean.quickSuggest.pingType.testGetValue(), ping.type);
  const keymap = {
    match_type: Glean.quickSuggest.matchType,
    position: Glean.quickSuggest.position,
    improve_suggest_experience_checked:
      Glean.quickSuggest.improveSuggestExperience,
    is_clicked: Glean.quickSuggest.isClicked,
    block_id: Glean.quickSuggest.blockId,
    advertiser: Glean.quickSuggest.advertiser,
    iab_category: Glean.quickSuggest.iabCategory,
  };
  for (const [key, value] of Object.entries(ping.payload)) {
    Assert.ok(key in keymap, `A Glean metric exists for field ${key}`);
    Assert.equal(value ?? "", keymap[key].testGetValue());
  }
}
