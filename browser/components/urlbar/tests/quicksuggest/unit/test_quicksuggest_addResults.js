/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

// Tests the chunking feature of `RemoteSettingsClient.#addResults()`.

"use strict";

ChromeUtils.defineESModuleGetters(this, {
  SuggestionsMap:
    "resource:///modules/urlbar/private/QuickSuggestRemoteSettings.sys.mjs",
});

XPCOMUtils.defineLazyModuleGetters(this, {
  ObjectUtils: "resource://gre/modules/ObjectUtils.jsm",
});

// This overrides `SuggestionsMap.chunkSize`. Testing the actual value can make
// the test run too long. This is OK because the correctness of the chunking
// behavior doesn't depend on the chunk size.
const TEST_CHUNK_SIZE = 100;

add_task(async function init() {
  // Sanity check the actual `chunkSize` value.
  Assert.equal(
    typeof SuggestionsMap.chunkSize,
    "number",
    "Sanity check: chunkSize is a number"
  );
  Assert.greater(SuggestionsMap.chunkSize, 0, "Sanity check: chunkSize > 0");

  // Set our test value.
  SuggestionsMap.chunkSize = TEST_CHUNK_SIZE;
});

// Tests many suggestions with one keyword each.
add_task(async function chunking_singleKeyword() {
  let suggestionCounts = [
    1 * SuggestionsMap.chunkSize - 1,
    1 * SuggestionsMap.chunkSize,
    1 * SuggestionsMap.chunkSize + 1,
    2 * SuggestionsMap.chunkSize - 1,
    2 * SuggestionsMap.chunkSize,
    2 * SuggestionsMap.chunkSize + 1,
    3 * SuggestionsMap.chunkSize - 1,
    3 * SuggestionsMap.chunkSize,
    3 * SuggestionsMap.chunkSize + 1,
  ];
  for (let count of suggestionCounts) {
    await doChunkingTest(count, 1);
  }
});

// Tests a small number of suggestions with many keywords each.
add_task(async function chunking_manyKeywords() {
  let keywordCounts = [
    1 * SuggestionsMap.chunkSize - 1,
    1 * SuggestionsMap.chunkSize,
    1 * SuggestionsMap.chunkSize + 1,
    2 * SuggestionsMap.chunkSize - 1,
    2 * SuggestionsMap.chunkSize,
    2 * SuggestionsMap.chunkSize + 1,
    3 * SuggestionsMap.chunkSize - 1,
    3 * SuggestionsMap.chunkSize,
    3 * SuggestionsMap.chunkSize + 1,
  ];
  for (let suggestionCount = 1; suggestionCount <= 3; suggestionCount++) {
    for (let keywordCount of keywordCounts) {
      await doChunkingTest(suggestionCount, keywordCount);
    }
  }
});

async function doChunkingTest(suggestionCount, keywordCountPerSuggestion) {
  info(
    "Running chunking test: " +
      JSON.stringify({ suggestionCount, keywordCountPerSuggestion })
  );

  // Create `suggestionCount` suggestions, each with `keywordCountPerSuggestion`
  // keywords.
  let suggestions = [];
  for (let i = 0; i < suggestionCount; i++) {
    let keywords = [];
    for (let k = 0; k < keywordCountPerSuggestion; k++) {
      keywords.push(`keyword-${i}-${k}`);
    }
    suggestions.push({
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

  // Add the suggestions.
  let map = new SuggestionsMap();
  await map.add(suggestions);

  // Make sure all keyword-suggestion pairs have been added.
  for (let i = 0; i < suggestionCount; i++) {
    for (let k = 0; k < keywordCountPerSuggestion; k++) {
      let keyword = `keyword-${i}-${k}`;

      // Check the map. Logging all assertions takes a ton of time and makes the
      // test run much longer than it otherwise would, especially if `chunkSize`
      // is large, so only log failing assertions.
      let actualSuggestions = map.get(keyword);
      if (!ObjectUtils.deepEqual(actualSuggestions, [suggestions[i]])) {
        Assert.deepEqual(
          actualSuggestions,
          [suggestions[i]],
          `Suggestion ${i} is present for keyword ${keyword}`
        );
      }
    }
  }
}

add_task(async function duplicateKeywords() {
  let suggestions = [
    {
      title: "suggestion 0",
      keywords: ["a", "b", "c"],
    },
    {
      title: "suggestion 1",
      keywords: ["b", "c", "d"],
    },
    {
      title: "suggestion 2",
      keywords: ["c", "d", "e"],
    },
    {
      title: "suggestion 3",
      keywords: ["f"],
    },
  ];

  let expectedIndexesByKeyword = {
    a: [0],
    b: [0, 1],
    c: [0, 1, 2],
    d: [1, 2],
    e: [2],
    f: [3],
  };

  let map = new SuggestionsMap();
  await map.add(suggestions);

  for (let [keyword, indexes] of Object.entries(expectedIndexesByKeyword)) {
    Assert.deepEqual(
      map.get(keyword),
      indexes.map(i => suggestions[i]),
      "get() with keyword: " + keyword
    );
  }
});
