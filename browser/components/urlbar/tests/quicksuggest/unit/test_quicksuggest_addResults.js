/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

// Tests the chunking feature of `UrlbarQuickSuggest._addResults()`.

"use strict";

XPCOMUtils.defineLazyModuleGetters(this, {
  ObjectUtils: "resource://gre/modules/ObjectUtils.jsm",
});

// This overrides `UrlbarQuickSuggest._addResultsChunkSize`. Testing the actual
// value can make the test run too long. This is OK because the correctness of
// the chunking behavior doesn't depend on the chunk size.
const TEST_ADD_RESULTS_CHUNK_SIZE = 100;

add_task(async function init() {
  UrlbarPrefs.set("quicksuggest.enabled", true);
  await QuickSuggestTestUtils.ensureQuickSuggestInit();

  // Sanity check the actual `UrlbarQuickSuggest._addResultsChunkSize` value.
  Assert.equal(
    typeof UrlbarQuickSuggest._addResultsChunkSize,
    "number",
    "Sanity check: UrlbarQuickSuggest._addResultsChunkSize is a number"
  );
  Assert.greater(
    UrlbarQuickSuggest._addResultsChunkSize,
    0,
    "Sanity check: UrlbarQuickSuggest._addResultsChunkSize > 0"
  );

  // Set our test value.
  UrlbarQuickSuggest._addResultsChunkSize = TEST_ADD_RESULTS_CHUNK_SIZE;
});

// Tests many results with one keyword each.
add_task(async function chunking_singleKeyword() {
  let resultCounts = [
    1 * UrlbarQuickSuggest._addResultsChunkSize - 1,
    1 * UrlbarQuickSuggest._addResultsChunkSize,
    1 * UrlbarQuickSuggest._addResultsChunkSize + 1,
    2 * UrlbarQuickSuggest._addResultsChunkSize - 1,
    2 * UrlbarQuickSuggest._addResultsChunkSize,
    2 * UrlbarQuickSuggest._addResultsChunkSize + 1,
    3 * UrlbarQuickSuggest._addResultsChunkSize - 1,
    3 * UrlbarQuickSuggest._addResultsChunkSize,
    3 * UrlbarQuickSuggest._addResultsChunkSize + 1,
  ];
  for (let count of resultCounts) {
    await doChunkingTest(count, 1);
  }
});

// Tests a small number of results with many keywords each.
add_task(async function chunking_manyKeywords() {
  let keywordCounts = [
    1 * UrlbarQuickSuggest._addResultsChunkSize - 1,
    1 * UrlbarQuickSuggest._addResultsChunkSize,
    1 * UrlbarQuickSuggest._addResultsChunkSize + 1,
    2 * UrlbarQuickSuggest._addResultsChunkSize - 1,
    2 * UrlbarQuickSuggest._addResultsChunkSize,
    2 * UrlbarQuickSuggest._addResultsChunkSize + 1,
    3 * UrlbarQuickSuggest._addResultsChunkSize - 1,
    3 * UrlbarQuickSuggest._addResultsChunkSize,
    3 * UrlbarQuickSuggest._addResultsChunkSize + 1,
  ];
  for (let resultCount = 1; resultCount <= 3; resultCount++) {
    for (let keywordCount of keywordCounts) {
      await doChunkingTest(resultCount, keywordCount);
    }
  }
});

async function doChunkingTest(resultCount, keywordCountPerResult) {
  info(
    "Running chunking test: " +
      JSON.stringify({ resultCount, keywordCountPerResult })
  );

  // Create `resultCount` results, each with `keywordCountPerResult` keywords.
  let results = [];
  for (let i = 0; i < resultCount; i++) {
    let keywords = [];
    for (let k = 0; k < keywordCountPerResult; k++) {
      keywords.push(`keyword-${i}-${k}`);
    }
    results.push({
      keywords,
      id: i,
      url: "http://example.com/" + i,
      title: "Suggestion " + i,
      click_url: "http://example.com/click",
      impression_url: "http://example.com/impression",
      advertiser: "TestAdvertiser",
      iab_category: "22 - Shopping",
    });
  }

  // Add the results.
  UrlbarQuickSuggest._resultsByKeyword.clear();
  await UrlbarQuickSuggest._addResults(results);

  // Make sure all keyword-result pairs have been added.
  for (let i = 0; i < resultCount; i++) {
    for (let k = 0; k < keywordCountPerResult; k++) {
      let keyword = `keyword-${i}-${k}`;

      // Check the `_resultsByKeyword` map. Logging all assertions takes a ton
      // of time and makes the test run much longer than it otherwise would,
      // especially if `_addResultsChunkSize` is large, so only log failing
      // assertions.
      let actualResult = UrlbarQuickSuggest._resultsByKeyword.get(keyword);
      if (!ObjectUtils.deepEqual(actualResult, results[i])) {
        Assert.deepEqual(
          actualResult,
          results[i],
          `Result ${i} is in _resultsByKeyword for keyword ${keyword}`
        );
      }

      // Call `query()` and make sure a suggestion is returned for the result.
      // Computing the expected value of `full_keyword` is kind of a pain and
      // it's not important to check it, so first delete it from the returned
      // suggestion.
      let actualSuggestions = await UrlbarQuickSuggest.query(keyword);
      for (let s of actualSuggestions) {
        delete s.full_keyword;
      }
      let expectedSuggestions = [
        {
          block_id: i,
          url: "http://example.com/" + i,
          title: "Suggestion " + i,
          click_url: "http://example.com/click",
          impression_url: "http://example.com/impression",
          advertiser: "TestAdvertiser",
          iab_category: "22 - Shopping",
          is_sponsored: true,
          score: UrlbarQuickSuggest.DEFAULT_SUGGESTION_SCORE,
          source: "remote-settings",
          icon: null,
          position: undefined,
          _test_is_best_match: undefined,
        },
      ];
      if (!ObjectUtils.deepEqual(actualSuggestions, expectedSuggestions)) {
        Assert.deepEqual(
          actualSuggestions,
          expectedSuggestions,
          `query() returns a suggestion for result ${i} with keyword ${keyword}`
        );
      }
    }
  }
}
