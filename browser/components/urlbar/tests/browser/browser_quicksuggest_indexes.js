/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

// Tests the configurable indexes of sponsored and non-sponsored ("Firefox
// Suggest") quick suggest results.

"use strict";

XPCOMUtils.defineLazyModuleGetters(this, {
  UrlbarQuickSuggest: "resource:///modules/UrlbarQuickSuggest.jsm",
});

const SUGGESTIONS_FIRST_PREF = "browser.urlbar.showSearchSuggestionsFirst";
const SUGGESTIONS_PREF = "browser.urlbar.suggest.searches";

const TEST_ENGINE_BASENAME = "searchSuggestionEngine.xml";
const MAX_RESULTS = UrlbarPrefs.get("maxRichResults");

const SPONSORED_INDEX_PREF = "browser.urlbar.quicksuggest.sponsoredIndex";
const NON_SPONSORED_INDEX_PREF =
  "browser.urlbar.quicksuggest.nonSponsoredIndex";

const SPONSORED_SEARCH_STRING = "frabbits";
const NON_SPONSORED_SEARCH_STRING = "nonspon";

const TEST_URL =
  "http://mochi.test:8888/browser/browser/components/urlbar/tests/browser/quicksuggest.sjs";

const TEST_DATA = [
  {
    id: 1,
    url: `${TEST_URL}?q=${SPONSORED_SEARCH_STRING}`,
    title: "frabbits",
    keywords: [SPONSORED_SEARCH_STRING],
    click_url: "http://click.reporting.test.com/",
    impression_url: "http://impression.reporting.test.com/",
    advertiser: "TestAdvertiser",
  },
  {
    id: 2,
    url: `${TEST_URL}?q=${NON_SPONSORED_SEARCH_STRING}`,
    title: "Non-Sponsored",
    keywords: [NON_SPONSORED_SEARCH_STRING],
    click_url: "http://click.reporting.test.com/nonsponsored",
    impression_url: "http://impression.reporting.test.com/nonsponsored",
    advertiser: "TestAdvertiserNonSponsored",
    iab_category: "5 - Education",
  },
];

add_task(async function init() {
  // This test intermittently times out on Mac TV WebRender.
  if (AppConstants.platform == "macosx") {
    requestLongerTimeout(3);
  }

  await PlacesUtils.history.clear();
  await PlacesUtils.bookmarks.eraseEverything();
  await UrlbarTestUtils.formHistory.clear();

  // Add a mock engine so we don't hit the network.
  await SearchTestUtils.installSearchExtension();
  let oldDefaultEngine = await Services.search.getDefault();
  await Services.search.setDefault(Services.search.getEngineByName("Example"));

  await UrlbarQuickSuggest.init();
  let { _createTree } = UrlbarQuickSuggest;
  UrlbarQuickSuggest._createTree = () => {};
  await UrlbarQuickSuggest._processSuggestionsJSON(TEST_DATA);

  await SpecialPowers.pushPrefEnv({
    set: [
      [SUGGESTIONS_PREF, true],
      ["browser.urlbar.quicksuggest.enabled", true],
      ["browser.urlbar.quicksuggest.shouldShowOnboardingDialog", false],
    ],
  });

  registerCleanupFunction(async () => {
    await PlacesUtils.history.clear();
    Services.search.setDefault(oldDefaultEngine);
    UrlbarQuickSuggest._createTree = _createTree;
  });
});

// Tests with history only
add_task(async function noSuggestions() {
  await doTestPermutations(({ withHistory, generalIndex }) => ({
    expectedResultCount: withHistory ? MAX_RESULTS : 2,
    expectedIndex: generalIndex == 0 || !withHistory ? 1 : MAX_RESULTS - 1,
  }));
});

// Tests with suggestions followed by history
add_task(async function suggestionsFirst() {
  await SpecialPowers.pushPrefEnv({
    set: [[SUGGESTIONS_FIRST_PREF, true]],
  });
  await withSuggestions(async () => {
    await doTestPermutations(({ withHistory, generalIndex }) => ({
      expectedResultCount: withHistory ? MAX_RESULTS : 4,
      expectedIndex: generalIndex == 0 || !withHistory ? 3 : MAX_RESULTS - 1,
    }));
  });
  await SpecialPowers.popPrefEnv();
});

// Tests with history followed by suggestions
add_task(async function suggestionsLast() {
  await SpecialPowers.pushPrefEnv({
    set: [[SUGGESTIONS_FIRST_PREF, false]],
  });
  await withSuggestions(async () => {
    await doTestPermutations(({ withHistory, generalIndex }) => ({
      expectedResultCount: withHistory ? MAX_RESULTS : 4,
      expectedIndex: generalIndex == 0 || !withHistory ? 1 : MAX_RESULTS - 3,
    }));
  });
  await SpecialPowers.popPrefEnv();
});

/**
 * Does a round of test permutations.
 *
 * @param {function} callback
 *   For each permutation, this will be called with the arguments of `doTest()`,
 *   and it should return an object with the appropriate values of
 *   `expectedResultCount` and `expectedIndex`.
 */
async function doTestPermutations(callback) {
  for (let isSponsored of [true, false]) {
    for (let withHistory of [true, false]) {
      for (let generalIndex of [0, -1]) {
        let opts = {
          isSponsored,
          withHistory,
          generalIndex,
        };
        await doTest(Object.assign(opts, callback(opts)));
      }
    }
  }
}

/**
 * Does one test run.
 *
 * @param {boolean} isSponsored
 *   True to use a sponsored result, false to use a non-sponsored result.
 * @param {boolean} withHistory
 *   True to run with a bunch of history, false to run with no history.
 * @param {number} generalIndex
 *   The value to set as the relevant index pref, i.e., the index within the
 *   general bucket of the quick suggest result.
 * @param {number} expectedResultCount
 *   The expected total result count for sanity checking.
 * @param {number} expectedIndex
 *   The expected index of the quick suggest result in the whole results list.
 */
async function doTest({
  isSponsored,
  withHistory,
  generalIndex,
  expectedResultCount,
  expectedIndex,
}) {
  info(
    "Running test with options: " +
      JSON.stringify({
        isSponsored,
        withHistory,
        generalIndex,
        expectedResultCount,
        expectedIndex,
      })
  );

  // Set the index pref.
  let indexPref = isSponsored ? SPONSORED_INDEX_PREF : NON_SPONSORED_INDEX_PREF;
  await SpecialPowers.pushPrefEnv({
    set: [[indexPref, generalIndex]],
  });

  // Add history.
  if (withHistory) {
    await addHistory();
  }

  // Do a search.
  let value = isSponsored
    ? SPONSORED_SEARCH_STRING
    : NON_SPONSORED_SEARCH_STRING;
  await UrlbarTestUtils.promiseAutocompleteResultPopup({
    window,
    value,
  });

  // Check the result count and quick suggest result.
  Assert.equal(
    UrlbarTestUtils.getResultCount(window),
    expectedResultCount,
    "Expected result count"
  );
  await assertIsQuickSuggest({ isSponsored, index: expectedIndex });

  await UrlbarTestUtils.promisePopupClose(window);
  await PlacesUtils.history.clear();
  await SpecialPowers.popPrefEnv();
}

/**
 * Adds history that matches the sponsored and non-sponsored search strings.
 */
async function addHistory() {
  for (let i = 0; i < MAX_RESULTS; i++) {
    await PlacesTestUtils.addVisits([
      "http://example.com/" + SPONSORED_SEARCH_STRING + i,
      "http://example.com/" + NON_SPONSORED_SEARCH_STRING + i,
    ]);
  }
}

/**
 * Asserts that a result is a Quick Suggest result.
 *
 * @param {number} [index]
 *   The expected index of the Quick Suggest result.  Pass -1 to use the index
 *   of the last result.
 * @param {boolean} [isSponsored]
 *   True if the result is expected to be sponsored and false if non-sponsored
 *   (i.e., "Firefox Suggest").
 * @param {object} [win]
 *   The window in which to read the results from.
 * @returns {result}
 *   The result at the given index.
 */
async function assertIsQuickSuggest({
  index = -1,
  isSponsored = true,
  win = window,
} = {}) {
  if (index < 0) {
    index = UrlbarTestUtils.getResultCount(win) - 1;
    Assert.greater(index, -1, "Sanity check: Result count should be > 0");
  }

  let result = await UrlbarTestUtils.getDetailsOfResultAt(win, index);
  Assert.equal(result.type, UrlbarUtils.RESULT_TYPE.URL);

  // Confusingly, `isSponsored` is set on the result payload for all quick
  // suggest results, even non-sponsored ones.  It's just a marker of whether
  // the result is a quick suggest.
  Assert.ok(result.isSponsored, "Result isSponsored");

  let url;
  let actionText;
  if (isSponsored) {
    url = `${TEST_URL}?q=${SPONSORED_SEARCH_STRING}`;
    actionText = "Sponsored";
  } else {
    url = `${TEST_URL}?q=${NON_SPONSORED_SEARCH_STRING}`;
    actionText = "Firefox Suggest";
  }
  Assert.equal(result.url, url, "Result URL");
  Assert.equal(
    result.element.row._elements.get("action").textContent,
    actionText,
    "Result action text"
  );

  let helpButton = result.element.row._elements.get("helpButton");
  Assert.ok(helpButton, "The help button should be present");

  return result;
}

/**
 * Adds a search engine that provides suggestions, calls your callback, and then
 * removes the engine.
 *
 * @param {function} callback
 *   Your callback function.
 */
async function withSuggestions(callback) {
  await SpecialPowers.pushPrefEnv({
    set: [[SUGGESTIONS_PREF, true]],
  });
  let engine = await SearchTestUtils.promiseNewSearchEngine(
    getRootDirectory(gTestPath) + TEST_ENGINE_BASENAME
  );
  let oldDefaultEngine = await Services.search.getDefault();
  await Services.search.setDefault(engine);
  try {
    await callback(engine);
  } finally {
    await Services.search.setDefault(oldDefaultEngine);
    await Services.search.removeEngine(engine);
    await SpecialPowers.popPrefEnv();
  }
}
