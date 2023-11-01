/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

// Tests non-unique keywords, i.e., keywords used by multiple suggestions.

"use strict";

const { DEFAULT_SUGGESTION_SCORE } = UrlbarProviderQuickSuggest;

// For each of these objects, the test creates a quick suggest result (the kind
// stored in the remote settings data, not a urlbar result), the corresponding
// expected quick suggest suggestion, and the corresponding expected urlbar
// result. The test assumes results and suggestions are returned in the order
// listed here.
let SUGGESTIONS_DATA = [
  {
    keywords: ["aaa"],
    isSponsored: true,
    score: DEFAULT_SUGGESTION_SCORE,
  },
  {
    keywords: ["aaa", "bbb"],
    isSponsored: false,
    score: 2 * DEFAULT_SUGGESTION_SCORE,
  },
  {
    keywords: ["bbb"],
    isSponsored: true,
    score: 4 * DEFAULT_SUGGESTION_SCORE,
  },
  {
    keywords: ["bbb"],
    isSponsored: false,
    score: 3 * DEFAULT_SUGGESTION_SCORE,
  },
  {
    keywords: ["ccc"],
    isSponsored: true,
    score: DEFAULT_SUGGESTION_SCORE,
  },
];

// Test cases. In this object, keywords map to subtest cases. For each keyword,
// the test calls `query(keyword)` and checks that the indexes (relative to
// `SUGGESTIONS_DATA`) of the returned quick suggest results are the ones in
// `expectedIndexes`. Then the test does a series of urlbar searches using the
// keyword as the search string, one search per object in `searches`. Sponsored
// and non-sponsored urlbar results are enabled as defined by `sponsored` and
// `nonsponsored`. `expectedIndex` is the expected index (relative to
// `SUGGESTIONS_DATA`) of the returned urlbar result.
let TESTS = {
  aaa: {
    // 0: sponsored
    // 1: nonsponsored, score = 2x
    expectedIndexes: [0, 1],
    searches: [
      {
        sponsored: true,
        nonsponsored: true,
        expectedIndex: 1,
      },
      {
        sponsored: false,
        nonsponsored: true,
        expectedIndex: 1,
      },
      {
        sponsored: true,
        nonsponsored: false,
        expectedIndex: 0,
      },
      {
        sponsored: false,
        nonsponsored: false,
        expectedIndex: undefined,
      },
    ],
  },
  bbb: {
    // 1: nonsponsored, score = 2x
    // 2: sponsored, score = 4x,
    // 3: nonsponsored, score = 3x
    expectedIndexes: [1, 2, 3],
    searches: [
      {
        sponsored: true,
        nonsponsored: true,
        expectedIndex: 2,
      },
      {
        sponsored: false,
        nonsponsored: true,
        expectedIndex: 3,
      },
      {
        sponsored: true,
        nonsponsored: false,
        expectedIndex: 2,
      },
      {
        sponsored: false,
        nonsponsored: false,
        expectedIndex: undefined,
      },
    ],
  },
  ccc: {
    // 4: sponsored
    expectedIndexes: [4],
    searches: [
      {
        sponsored: true,
        nonsponsored: true,
        expectedIndex: 4,
      },
      {
        sponsored: false,
        nonsponsored: true,
        expectedIndex: undefined,
      },
      {
        sponsored: true,
        nonsponsored: false,
        expectedIndex: 4,
      },
      {
        sponsored: false,
        nonsponsored: false,
        expectedIndex: undefined,
      },
    ],
  },
};

add_task(async function () {
  // Create results and suggestions based on `SUGGESTIONS_DATA`.
  let qsResults = [];
  let qsSuggestions = [];
  let urlbarResults = [];
  for (let i = 0; i < SUGGESTIONS_DATA.length; i++) {
    let { keywords, isSponsored, score } = SUGGESTIONS_DATA[i];

    // quick suggest result
    let qsResult = {
      keywords,
      score,
      id: i,
      url: "http://example.com/" + i,
      title: "Title " + i,
      click_url: "http://example.com/click",
      impression_url: "http://example.com/impression",
      advertiser: "TestAdvertiser",
      iab_category: isSponsored ? "22 - Shopping" : "5 - Education",
    };
    qsResults.push(qsResult);

    // expected quick suggest suggestion
    let qsSuggestion = {
      ...qsResult,
      score,
      block_id: qsResult.id,
      is_sponsored: isSponsored,
      source: "remote-settings",
      icon: null,
      position: undefined,
      provider: "AdmWikipedia",
    };
    delete qsSuggestion.keywords;
    delete qsSuggestion.id;
    qsSuggestions.push(qsSuggestion);

    // expected urlbar result
    urlbarResults.push({
      type: UrlbarUtils.RESULT_TYPE.URL,
      source: UrlbarUtils.RESULT_SOURCE.SEARCH,
      heuristic: false,
      payload: {
        isSponsored,
        telemetryType: isSponsored ? "adm_sponsored" : "adm_nonsponsored",
        sponsoredBlockId: qsResult.id,
        url: qsResult.url,
        originalUrl: qsResult.url,
        displayUrl: qsResult.url,
        title: qsResult.title,
        sponsoredClickUrl: qsResult.click_url,
        sponsoredImpressionUrl: qsResult.impression_url,
        sponsoredAdvertiser: qsResult.advertiser,
        sponsoredIabCategory: qsResult.iab_category,
        icon: null,
        descriptionL10n: isSponsored
          ? { id: "urlbar-result-action-sponsored" }
          : undefined,
        helpUrl: QuickSuggest.HELP_URL,
        helpL10n: {
          id: "urlbar-result-menu-learn-more-about-firefox-suggest",
        },
        isBlockable: true,
        blockL10n: {
          id: "urlbar-result-menu-dismiss-firefox-suggest",
        },
        source: "remote-settings",
        provider: "AdmWikipedia",
      },
    });
  }

  await QuickSuggestTestUtils.ensureQuickSuggestInit({
    remoteSettingsRecords: [
      {
        type: "data",
        attachment: qsResults,
      },
    ],
    prefs: [
      ["suggest.quicksuggest.sponsored", true],
      ["suggest.quicksuggest.nonsponsored", true],
    ],
  });

  // Run a test for each keyword.
  for (let [keyword, test] of Object.entries(TESTS)) {
    info("Running subtest " + JSON.stringify({ keyword, test }));

    let { expectedIndexes, searches } = test;

    // Call `query()`.
    Assert.deepEqual(
      await QuickSuggest.jsBackend.query(keyword),
      expectedIndexes.map(i => ({
        ...qsSuggestions[i],
        full_keyword: keyword,
      })),
      `query() for keyword ${keyword}`
    );

    // Now do a urlbar search for the keyword with all possible combinations of
    // sponsored and non-sponsored suggestions enabled and disabled.
    for (let sponsored of [true, false]) {
      for (let nonsponsored of [true, false]) {
        // Find the matching `searches` object.
        let search = searches.find(
          s => s.sponsored == sponsored && s.nonsponsored == nonsponsored
        );
        Assert.ok(
          search,
          "Sanity check: Search test case specified for " +
            JSON.stringify({ keyword, sponsored, nonsponsored })
        );

        info(
          "Running urlbar search subtest " +
            JSON.stringify({ keyword, expectedIndexes, search })
        );

        UrlbarPrefs.set("suggest.quicksuggest.sponsored", sponsored);
        UrlbarPrefs.set("suggest.quicksuggest.nonsponsored", nonsponsored);
        await QuickSuggestTestUtils.forceSync();

        // Set up the search and do it.
        let context = createContext(keyword, {
          providers: [UrlbarProviderQuickSuggest.name],
          isPrivate: false,
        });

        let matches = [];
        if (search.expectedIndex !== undefined) {
          matches.push({
            ...urlbarResults[search.expectedIndex],
            payload: {
              ...urlbarResults[search.expectedIndex].payload,
              qsSuggestion: keyword,
            },
          });
        }

        await check_results({ context, matches });
      }
    }

    UrlbarPrefs.set("suggest.quicksuggest.sponsored", true);
    UrlbarPrefs.set("suggest.quicksuggest.nonsponsored", true);
    await QuickSuggestTestUtils.forceSync();
  }
});
