/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

// Tests best match quick suggest results.

"use strict";

const MAX_RESULT_COUNT = UrlbarPrefs.get("maxRichResults");

// This search string length needs to be >= 4 to trigger its suggestion as a
// best match instead of a usual quick suggest.
const BEST_MATCH_POSITION_SEARCH_STRING = "bestmatchposition";
const BEST_MATCH_POSITION = Math.round(MAX_RESULT_COUNT / 2);

const REMOTE_SETTINGS_RESULTS = [
  {
    id: 1,
    url: "http://example.com/",
    title: "Fullkeyword title",
    keywords: [
      "fu",
      "ful",
      "full",
      "fullk",
      "fullke",
      "fullkey",
      "fullkeyw",
      "fullkeywo",
      "fullkeywor",
      "fullkeyword",
    ],
    click_url: "http://example.com/click",
    impression_url: "http://example.com/impression",
    advertiser: "TestAdvertiser",
  },
  {
    id: 2,
    url: "http://example.com/best-match-position",
    title: `${BEST_MATCH_POSITION_SEARCH_STRING} title`,
    keywords: [BEST_MATCH_POSITION_SEARCH_STRING],
    click_url: "http://example.com/click",
    impression_url: "http://example.com/impression",
    advertiser: "TestAdvertiser",
    position: BEST_MATCH_POSITION,
  },
];

const EXPECTED_BEST_MATCH_URLBAR_RESULT = {
  type: UrlbarUtils.RESULT_TYPE.URL,
  source: UrlbarUtils.RESULT_SOURCE.SEARCH,
  heuristic: false,
  isBestMatch: true,
  payload: {
    subtype: UrlbarProviderQuickSuggest.RESULT_SUBTYPE.SPONSORED,
    url: "http://example.com/",
    originalUrl: "http://example.com/",
    title: "Fullkeyword title",
    icon: null,
    isSponsored: true,
    sponsoredImpressionUrl: "http://example.com/impression",
    sponsoredClickUrl: "http://example.com/click",
    sponsoredBlockId: 1,
    sponsoredAdvertiser: "TestAdvertiser",
    helpUrl: QuickSuggest.HELP_URL,
    helpL10n: {
      id: UrlbarPrefs.get("resultMenu")
        ? "urlbar-result-menu-learn-more-about-firefox-suggest"
        : "firefox-suggest-urlbar-learn-more",
    },
    isBlockable: UrlbarPrefs.get("bestMatchBlockingEnabled"),
    blockL10n: {
      id: UrlbarPrefs.get("resultMenu")
        ? "urlbar-result-menu-dismiss-firefox-suggest"
        : "firefox-suggest-urlbar-block",
    },
    displayUrl: "http://example.com",
    source: "remote-settings",
  },
};

const EXPECTED_NON_BEST_MATCH_URLBAR_RESULT = {
  type: UrlbarUtils.RESULT_TYPE.URL,
  source: UrlbarUtils.RESULT_SOURCE.SEARCH,
  heuristic: false,
  payload: {
    subtype: UrlbarProviderQuickSuggest.RESULT_SUBTYPE.SPONSORED,
    url: "http://example.com/",
    originalUrl: "http://example.com/",
    title: "Fullkeyword title",
    qsSuggestion: "fullkeyword",
    icon: null,
    isSponsored: true,
    sponsoredImpressionUrl: "http://example.com/impression",
    sponsoredClickUrl: "http://example.com/click",
    sponsoredBlockId: 1,
    sponsoredAdvertiser: "TestAdvertiser",
    helpUrl: QuickSuggest.HELP_URL,
    helpL10n: {
      id: UrlbarPrefs.get("resultMenu")
        ? "urlbar-result-menu-learn-more-about-firefox-suggest"
        : "firefox-suggest-urlbar-learn-more",
    },
    isBlockable: UrlbarPrefs.get("quickSuggestBlockingEnabled"),
    blockL10n: {
      id: UrlbarPrefs.get("resultMenu")
        ? "urlbar-result-menu-dismiss-firefox-suggest"
        : "firefox-suggest-urlbar-block",
    },
    displayUrl: "http://example.com",
    source: "remote-settings",
  },
};

const EXPECTED_BEST_MATCH_POSITION_URLBAR_RESULT = {
  type: UrlbarUtils.RESULT_TYPE.URL,
  source: UrlbarUtils.RESULT_SOURCE.SEARCH,
  heuristic: false,
  isBestMatch: true,
  payload: {
    subtype: UrlbarProviderQuickSuggest.RESULT_SUBTYPE.SPONSORED,
    url: "http://example.com/best-match-position",
    originalUrl: "http://example.com/best-match-position",
    title: `${BEST_MATCH_POSITION_SEARCH_STRING} title`,
    icon: null,
    isSponsored: true,
    sponsoredImpressionUrl: "http://example.com/impression",
    sponsoredClickUrl: "http://example.com/click",
    sponsoredBlockId: 2,
    sponsoredAdvertiser: "TestAdvertiser",
    helpUrl: QuickSuggest.HELP_URL,
    helpL10n: {
      id: UrlbarPrefs.get("resultMenu")
        ? "urlbar-result-menu-learn-more-about-firefox-suggest"
        : "firefox-suggest-urlbar-learn-more",
    },
    isBlockable: UrlbarPrefs.get("bestMatchBlockingEnabled"),
    blockL10n: {
      id: UrlbarPrefs.get("resultMenu")
        ? "urlbar-result-menu-dismiss-firefox-suggest"
        : "firefox-suggest-urlbar-block",
    },
    displayUrl: "http://example.com/best-match-position",
    source: "remote-settings",
  },
};

add_task(async function init() {
  UrlbarPrefs.set("quicksuggest.enabled", true);
  UrlbarPrefs.set("bestMatch.enabled", true);
  UrlbarPrefs.set("suggest.quicksuggest.nonsponsored", true);
  UrlbarPrefs.set("suggest.quicksuggest.sponsored", true);
  UrlbarPrefs.set("suggest.bestmatch", true);

  // Disable search suggestions so we don't hit the network.
  Services.prefs.setBoolPref("browser.search.suggest.enabled", false);

  await QuickSuggestTestUtils.ensureQuickSuggestInit({
    remoteSettingsResults: REMOTE_SETTINGS_RESULTS,
    config: QuickSuggestTestUtils.BEST_MATCH_CONFIG,
  });
});

// Tests a best match result.
add_task(async function bestMatch() {
  let context = createContext("fullkeyword", {
    providers: [UrlbarProviderQuickSuggest.name],
    isPrivate: false,
  });
  await check_results({
    context,
    matches: [EXPECTED_BEST_MATCH_URLBAR_RESULT],
  });

  let result = context.results[0];

  // The title should not include the full keyword and em dash, and the part of
  // the title that the search string matches should be highlighted.
  Assert.equal(result.title, "Fullkeyword title", "result.title");
  Assert.deepEqual(
    result.titleHighlights,
    [[0, "fullkeyword".length]],
    "result.titleHighlights"
  );

  Assert.equal(result.suggestedIndex, 1, "result.suggestedIndex");
  Assert.equal(
    !!result.isSuggestedIndexRelativeToGroup,
    false,
    "result.isSuggestedIndexRelativeToGroup"
  );
});

// Tests a usual, non-best match quick suggest result.
add_task(async function nonBestMatch() {
  // Search for a substring of the full search string so we can test title
  // highlights.
  let context = createContext("fu", {
    providers: [UrlbarProviderQuickSuggest.name],
    isPrivate: false,
  });
  await check_results({
    context,
    matches: [EXPECTED_NON_BEST_MATCH_URLBAR_RESULT],
  });

  let result = context.results[0];

  // The title should include the full keyword and em dash, and the part of the
  // title that the search string does not match should be highlighted.
  Assert.equal(result.title, "fullkeyword — Fullkeyword title", "result.title");
  Assert.deepEqual(
    result.titleHighlights,
    [["fu".length, "fullkeyword".length - "fu".length]],
    "result.titleHighlights"
  );

  Assert.equal(result.suggestedIndex, -1, "result.suggestedIndex");
  Assert.equal(
    result.isSuggestedIndexRelativeToGroup,
    true,
    "result.isSuggestedIndexRelativeToGroup"
  );
});

// Tests prefix keywords leading up to a best match.
add_task(async function prefixKeywords() {
  let sawNonBestMatch = false;
  let sawBestMatch = false;
  for (let keyword of REMOTE_SETTINGS_RESULTS[0].keywords) {
    info(`Searching for "${keyword}"`);
    let context = createContext(keyword, {
      providers: [UrlbarProviderQuickSuggest.name],
      isPrivate: false,
    });

    let expectedResult;
    if (keyword.length < 4) {
      expectedResult = EXPECTED_NON_BEST_MATCH_URLBAR_RESULT;
      sawNonBestMatch = true;
    } else {
      expectedResult = EXPECTED_BEST_MATCH_URLBAR_RESULT;
      sawBestMatch = true;
    }

    await check_results({
      context,
      matches: [expectedResult],
    });
  }

  Assert.ok(sawNonBestMatch, "Sanity check: Saw a non-best match");
  Assert.ok(sawBestMatch, "Sanity check: Saw a best match");
});

// When tab-to-search is shown in the same search, both it and the best match
// will have a `suggestedIndex` value of 1. The TTS should appear first.
add_task(async function tabToSearch() {
  // Disable tab-to-search onboarding results so we get a regular TTS result,
  // which we can test a little more easily with `makeSearchResult()`.
  UrlbarPrefs.set("tabToSearch.onboard.interactionsLeft", 0);

  // Install a test engine. The main part of its domain name needs to match the
  // best match result too so we can trigger both its TTS and the best match.
  let engineURL = "https://foo.fullkeyword.com/";
  let extension = await SearchTestUtils.installSearchExtension(
    {
      name: "Test",
      search_url: engineURL,
    },
    { skipUnload: true }
  );
  let engine = Services.search.getEngineByName("Test");

  // Also need to add a visit to trigger TTS.
  await PlacesTestUtils.addVisits(engineURL);

  let context = createContext("fullkeyword", {
    isPrivate: false,
  });
  await check_results({
    context,
    matches: [
      // search heuristic
      makeSearchResult(context, {
        engineName: Services.search.defaultEngine.name,
        engineIconUri: Services.search.defaultEngine.iconURI?.spec,
        heuristic: true,
      }),
      // tab to search
      makeSearchResult(context, {
        engineName: engine.name,
        engineIconUri: UrlbarUtils.ICON.SEARCH_GLASS,
        uri: UrlbarUtils.stripPublicSuffixFromHost(engine.searchUrlDomain),
        providesSearchMode: true,
        query: "",
        providerName: "TabToSearch",
        satisfiesAutofillThreshold: true,
      }),
      // best match
      EXPECTED_BEST_MATCH_URLBAR_RESULT,
      // visit
      makeVisitResult(context, {
        uri: engineURL,
        title: `test visit for ${engineURL}`,
      }),
    ],
  });

  await cleanupPlaces();
  await extension.unload();

  UrlbarPrefs.clear("tabToSearch.onboard.interactionsLeft");
});

// When the best match feature gate is disabled, quick suggest results should be
// shown as the usual non-best match results.
add_task(async function disabled_featureGate() {
  UrlbarPrefs.set("bestMatch.enabled", false);
  await doDisabledTest();
  UrlbarPrefs.set("bestMatch.enabled", true);
});

// When the best match suggestions are disabled, quick suggest results should be
// shown as the usual non-best match results.
add_task(async function disabled_suggestions() {
  UrlbarPrefs.set("suggest.bestmatch", false);
  await doDisabledTest();
  UrlbarPrefs.set("suggest.bestmatch", true);
});

// When best match is disabled, quick suggest results should be shown as the
// usual, non-best match results.
async function doDisabledTest() {
  let context = createContext("fullkeywor", {
    providers: [UrlbarProviderQuickSuggest.name],
    isPrivate: false,
  });
  await check_results({
    context,
    matches: [EXPECTED_NON_BEST_MATCH_URLBAR_RESULT],
  });

  let result = context.results[0];

  // The title should include the full keyword and em dash, and the part of the
  // title that the search string does not match should be highlighted.
  Assert.equal(result.title, "fullkeyword — Fullkeyword title", "result.title");
  Assert.deepEqual(
    result.titleHighlights,
    [["fullkeywor".length, 1]],
    "result.titleHighlights"
  );

  Assert.equal(result.suggestedIndex, -1, "result.suggestedIndex");
  Assert.equal(
    result.isSuggestedIndexRelativeToGroup,
    true,
    "result.isSuggestedIndexRelativeToGroup"
  );
}

// `suggestion.position` should be ignored when the suggestion is a best match.
add_task(async function position() {
  Assert.greater(
    BEST_MATCH_POSITION,
    1,
    "Precondition: `suggestion.position` > the best match index"
  );

  UrlbarPrefs.set("quicksuggest.allowPositionInSuggestions", true);

  let context = createContext(BEST_MATCH_POSITION_SEARCH_STRING, {
    isPrivate: false,
  });

  // Add some visits to fill up the view.
  let maxResultCount = UrlbarPrefs.get("maxRichResults");
  let visitResults = [];
  for (let i = 0; i < maxResultCount; i++) {
    let url = `http://example.com/${BEST_MATCH_POSITION_SEARCH_STRING}-${i}`;
    await PlacesTestUtils.addVisits(url);
    visitResults.unshift(
      makeVisitResult(context, {
        uri: url,
        title: `test visit for ${url}`,
      })
    );
  }

  // Do a search.
  await check_results({
    context,
    matches: [
      // search heuristic
      makeSearchResult(context, {
        engineName: Services.search.defaultEngine.name,
        engineIconUri: Services.search.defaultEngine.iconURI?.spec,
        heuristic: true,
      }),
      // best match whose backing suggestion has a `position`
      EXPECTED_BEST_MATCH_POSITION_URLBAR_RESULT,
      // visits
      ...visitResults.slice(0, MAX_RESULT_COUNT - 2),
    ],
  });

  await cleanupPlaces();
  UrlbarPrefs.clear("quicksuggest.allowPositionInSuggestions");
});

// Tests a suggestion that is blocked from being a best match.
add_task(async function blockedAsBestMatch() {
  let config = QuickSuggestTestUtils.BEST_MATCH_CONFIG;
  config.best_match.blocked_suggestion_ids = [1];
  await QuickSuggestTestUtils.withConfig({
    config,
    callback: async () => {
      let context = createContext("fullkeyword", {
        providers: [UrlbarProviderQuickSuggest.name],
        isPrivate: false,
      });
      await check_results({
        context,
        matches: [EXPECTED_NON_BEST_MATCH_URLBAR_RESULT],
      });
    },
  });
});

// Tests without a best_match config to make sure nothing breaks.
add_task(async function noConfig() {
  await QuickSuggestTestUtils.withConfig({
    config: {},
    callback: async () => {
      let context = createContext("fullkeyword", {
        providers: [UrlbarProviderQuickSuggest.name],
        isPrivate: false,
      });
      await check_results({
        context,
        matches: [EXPECTED_NON_BEST_MATCH_URLBAR_RESULT],
      });
    },
  });
});

// Test that bestMatch navigational suggestion results are not shown when there
// is a heuristic result for the same domain.
add_task(async function heuristicDeduplication() {
  UrlbarPrefs.set("merino.enabled", true);
  UrlbarPrefs.set("quicksuggest.dataCollection.enabled", true);

  await MerinoTestUtils.server.start();
  MerinoTestUtils.server.response.body.suggestions = [
    {
      title: "Navigational suggestion",
      url: "http://example.com/",
      provider: "top_picks",
      is_sponsored: false,
      score: 0.25,
      block_id: 0,
      is_top_pick: true,
    },
  ];

  let expectedNavSuggestResult = {
    type: UrlbarUtils.RESULT_TYPE.URL,
    source: UrlbarUtils.RESULT_SOURCE.SEARCH,
    heuristic: false,
    isBestMatch: true,
    payload: {
      subtype: UrlbarProviderQuickSuggest.RESULT_SUBTYPE.NAVIGATIONAL,
      url: "http://example.com/",
      originalUrl: "http://example.com/",
      title: "Navigational suggestion",
      isSponsored: false,
      sponsoredBlockId: 0,
      helpUrl: QuickSuggest.HELP_URL,
      helpL10n: {
        id: UrlbarPrefs.get("resultMenu")
          ? "urlbar-result-menu-learn-more-about-firefox-suggest"
          : "firefox-suggest-urlbar-learn-more",
      },
      isBlockable: UrlbarPrefs.get("quickSuggestBlockingEnabled"),
      blockL10n: {
        id: UrlbarPrefs.get("resultMenu")
          ? "urlbar-result-menu-dismiss-firefox-suggest"
          : "firefox-suggest-urlbar-block",
      },
      displayUrl: "http://example.com",
      source: "merino",
      requestId: "request_id",
      dupedHeuristic: false,
    },
  };

  let scenarios = [
    ["http://example.com/", false],
    ["http://www.example.com/", false],
    ["http://exampledomain.com/", true],
  ];

  for (let [url, expectBestMatch] of scenarios) {
    await PlacesTestUtils.addVisits(url);
    let context = createContext("example", {
      providers: [UrlbarProviderQuickSuggest.name, UrlbarProviderAutofill.name],
      isPrivate: false,
    });
    const EXPECTED_AUTOFILL_RESULT = makeVisitResult(context, {
      uri: url,
      title: `test visit for ${url}`,
      heuristic: true,
    });
    await check_results({
      context,
      matches: expectBestMatch
        ? [EXPECTED_AUTOFILL_RESULT, expectedNavSuggestResult]
        : [EXPECTED_AUTOFILL_RESULT],
    });
    await PlacesUtils.history.clear();
  }

  await MerinoTestUtils.server.stop();
  UrlbarPrefs.clear("merino.enabled");
  UrlbarPrefs.clear("quicksuggest.dataCollection.enabled");
});
