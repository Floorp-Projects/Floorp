/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

// Tests `SuggestionsMap`.

"use strict";

ChromeUtils.defineESModuleGetters(this, {
  ObjectUtils: "resource://gre/modules/ObjectUtils.sys.mjs",
  SuggestionsMap: "resource:///modules/urlbar/private/SuggestBackendJs.sys.mjs",
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
      keywords: ["a", "a", "a", "b", "b", "c"],
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
      keywords: ["f", "f"],
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

add_task(async function mapKeywords() {
  let suggestions = [
    {
      title: "suggestion 0",
      keywords: ["a", "a", "a", "b", "b", "c"],
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
      keywords: ["f", "f"],
    },
  ];

  let expectedIndexesByKeyword = {
    a: [],
    b: [],
    c: [],
    d: [],
    e: [],
    f: [],
    ax: [0],
    bx: [0, 1],
    cx: [0, 1, 2],
    dx: [1, 2],
    ex: [2],
    fx: [3],
    fy: [3],
    fz: [3],
  };

  let map = new SuggestionsMap();
  await map.add(suggestions, {
    mapKeyword: keyword => {
      if (keyword == "f") {
        return [keyword + "x", keyword + "y", keyword + "z"];
      }
      return [keyword + "x"];
    },
  });

  for (let [keyword, indexes] of Object.entries(expectedIndexesByKeyword)) {
    Assert.deepEqual(
      map.get(keyword),
      indexes.map(i => suggestions[i]),
      "get() with keyword: " + keyword
    );
  }
});

// Tests `keywordsProperty`.
add_task(async function keywordsProperty() {
  let suggestion = {
    title: "suggestion",
    keywords: ["should be ignored"],
    foo: ["hello"],
  };

  let map = new SuggestionsMap();
  await map.add([suggestion], {
    keywordsProperty: "foo",
  });

  Assert.deepEqual(
    map.get("hello"),
    [suggestion],
    "Keyword in `foo` should match"
  );
  Assert.deepEqual(
    map.get("should be ignored"),
    [],
    "Keyword in `keywords` should not match"
  );
});

// Tests `MAP_KEYWORD_PREFIXES_STARTING_AT_FIRST_WORD`.
add_task(async function prefixesStartingAtFirstWord() {
  let suggestion = {
    title: "suggestion",
    keywords: ["one two three", "four five six"],
  };

  // keyword passed to `get()` -> should match
  let tests = {
    o: false,
    on: false,
    one: true,
    "one ": true,
    "one t": true,
    "one tw": true,
    "one two": true,
    "one two ": true,
    "one two t": true,
    "one two th": true,
    "one two thr": true,
    "one two thre": true,
    "one two three": true,
    "one two three ": false,
    f: false,
    fo: false,
    fou: false,
    four: true,
    "four ": true,
    "four f": true,
    "four fi": true,
    "four fiv": true,
    "four five": true,
    "four five ": true,
    "four five s": true,
    "four five si": true,
    "four five six": true,
    "four five six ": false,
  };

  let map = new SuggestionsMap();
  await map.add([suggestion], {
    mapKeyword: SuggestionsMap.MAP_KEYWORD_PREFIXES_STARTING_AT_FIRST_WORD,
  });

  for (let [keyword, shouldMatch] of Object.entries(tests)) {
    Assert.deepEqual(
      map.get(keyword),
      shouldMatch ? [suggestion] : [],
      "get() with keyword: " + keyword
    );
  }
});
