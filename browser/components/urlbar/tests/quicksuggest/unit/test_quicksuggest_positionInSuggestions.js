/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

/**
 * Tests for quick suggest result position specified in suggestions.
 */

ChromeUtils.defineESModuleGetters(this, {
  UrlbarProviderHeuristicFallback:
    "resource:///modules/UrlbarProviderHeuristicFallback.sys.mjs",
  UrlbarProviderPlaces: "resource:///modules/UrlbarProviderPlaces.sys.mjs",
  UrlbarProviderTabToSearch:
    "resource:///modules/UrlbarProviderTabToSearch.sys.mjs",
});

const SPONSORED_SECOND_POSITION_SUGGEST = {
  id: 1,
  url: "http://example.com/?q=sponsored-second",
  title: "sponsored second",
  keywords: ["s-s"],
  click_url: "http://click.reporting.test.com/",
  impression_url: "http://impression.reporting.test.com/",
  advertiser: "TestAdvertiser",
  iab_category: "22 - Shopping",
  position: 1,
};
const SPONSORED_NORMAL_POSITION_SUGGEST = {
  id: 2,
  url: "http://example.com/?q=sponsored-normal",
  title: "sponsored normal",
  keywords: ["s-n"],
  click_url: "http://click.reporting.test.com/",
  impression_url: "http://impression.reporting.test.com/",
  advertiser: "TestAdvertiser",
  iab_category: "22 - Shopping",
};
const NONSPONSORED_SECOND_POSITION_SUGGEST = {
  id: 3,
  url: "http://example.com/?q=nonsponsored-second",
  title: "nonsponsored second",
  keywords: ["n-s"],
  click_url: "http://click.reporting.test.com/nonsponsored",
  impression_url: "http://impression.reporting.test.com/nonsponsored",
  advertiser: "TestAdvertiserNonSponsored",
  iab_category: "5 - Education",
  position: 1,
};
const NONSPONSORED_NORMAL_POSITION_SUGGEST = {
  id: 4,
  url: "http://example.com/?q=nonsponsored-normal",
  title: "nonsponsored normal",
  keywords: ["n-n"],
  click_url: "http://click.reporting.test.com/nonsponsored",
  impression_url: "http://impression.reporting.test.com/nonsponsored",
  advertiser: "TestAdvertiserNonSponsored",
  iab_category: "5 - Education",
};
const FIRST_POSITION_SUGGEST = {
  id: 5,
  url: "http://example.com/?q=first-position",
  title: "first position suggest",
  keywords: ["first-position"],
  click_url: "http://click.reporting.test.com/first-position",
  impression_url: "http://impression.reporting.test.com/first-position",
  advertiser: "TestAdvertiserFirstPositionQuickSuggest",
  iab_category: "22 - Shopping",
  position: 0,
};
const SECOND_POSITION_SUGGEST = {
  id: 6,
  url: "http://example.com/?q=second-position",
  title: "second position suggest",
  keywords: ["second-position"],
  click_url: "http://click.reporting.test.com/second-position",
  impression_url: "http://impression.reporting.test.com/second-position",
  advertiser: "TestAdvertiserSecondPositionQuickSuggest",
  iab_category: "22 - Shopping",
  position: 1,
};
const THIRD_POSITION_SUGGEST = {
  id: 7,
  url: "http://example.com/?q=third-position",
  title: "third position suggest",
  keywords: ["third-position"],
  click_url: "http://click.reporting.test.com/third-position",
  impression_url: "http://impression.reporting.test.com/third-position",
  advertiser: "TestAdvertiserThirdPositionQuickSuggest",
  iab_category: "22 - Shopping",
  position: 2,
};

const TABTOSEARCH_ENGINE_DOMAIN_FOR_FIRST_POSITION_TEST =
  "first-position.example.com";
const TABTOSEARCH_ENGINE_DOMAIN_FOR_SECOND_POSITION_TEST =
  "second-position.example.com";

const SECOND_POSITION_INTERVENTION_RESULT = new UrlbarResult(
  UrlbarUtils.RESULT_TYPE.URL,
  UrlbarUtils.RESULT_SOURCE.HISTORY,
  { url: "http://mozilla.org/a" }
);
SECOND_POSITION_INTERVENTION_RESULT.suggestedIndex = 1;
const SECOND_POSITION_INTERVENTION_RESULT_PROVIDER = new UrlbarTestUtils.TestProvider(
  {
    results: [SECOND_POSITION_INTERVENTION_RESULT],
    priority: 0,
    name: "second_position_intervention_provider",
  }
);

const EXPECTED_GENERAL_HEURISTIC_RESULT = {
  providerName: UrlbarProviderHeuristicFallback.name,
  type: UrlbarUtils.RESULT_TYPE.SEARCH,
  source: UrlbarUtils.RESULT_SOURCE.SEARCH,
  heuristic: true,
};

const EXPECTED_GENERAL_PLACES_RESULT = {
  providerName: UrlbarProviderPlaces.name,
  type: UrlbarUtils.RESULT_TYPE.URL,
  source: UrlbarUtils.RESULT_SOURCE.HISTORY,
  heuristic: false,
};

const EXPECTED_GENERAL_TABTOSEARCH_RESULT = {
  providerName: UrlbarProviderTabToSearch.name,
  type: UrlbarUtils.RESULT_TYPE.DYNAMIC,
  source: UrlbarUtils.RESULT_SOURCE.SEARCH,
  heuristic: false,
};

const EXPECTED_GENERAL_INTERVENTION_RESULT = {
  providerName: SECOND_POSITION_INTERVENTION_RESULT_PROVIDER.name,
  type: UrlbarUtils.RESULT_TYPE.URL,
  source: UrlbarUtils.RESULT_SOURCE.HISTORY,
  heuristic: false,
};

function createExpectedQuickSuggestResult(suggest) {
  return {
    providerName: UrlbarProviderQuickSuggest.name,
    type: UrlbarUtils.RESULT_TYPE.URL,
    source: UrlbarUtils.RESULT_SOURCE.SEARCH,
    heuristic: false,
    payload: {
      qsSuggestion: suggest.keywords[0],
      title: suggest.title,
      url: suggest.url,
      originalUrl: suggest.url,
      icon: null,
      sponsoredImpressionUrl: suggest.impression_url,
      sponsoredClickUrl: suggest.click_url,
      sponsoredBlockId: suggest.id,
      sponsoredAdvertiser: suggest.advertiser,
      sponsoredIabCategory: suggest.iab_category,
      isSponsored: suggest.iab_category !== "5 - Education",
      helpUrl: UrlbarProviderQuickSuggest.helpUrl,
      helpL10nId: "firefox-suggest-urlbar-learn-more",
      displayUrl: suggest.url,
      source: "remote-settings",
    },
  };
}

const TEST_CASES = [
  {
    description: "Test for second placable sponsored suggest",
    input: SPONSORED_SECOND_POSITION_SUGGEST.keywords[0],
    prefs: {
      "quicksuggest.allowPositionInSuggestions": true,
      "suggest.quicksuggest.sponsored": true,
    },
    providers: [
      UrlbarProviderHeuristicFallback.name,
      UrlbarProviderQuickSuggest.name,
      UrlbarProviderPlaces.name,
    ],
    expected: [
      EXPECTED_GENERAL_HEURISTIC_RESULT,
      createExpectedQuickSuggestResult(SPONSORED_SECOND_POSITION_SUGGEST),
      EXPECTED_GENERAL_PLACES_RESULT,
    ],
  },
  {
    description: "Test for normal sponsored suggest",
    input: SPONSORED_NORMAL_POSITION_SUGGEST.keywords[0],
    prefs: {
      "quicksuggest.allowPositionInSuggestions": true,
      "suggest.quicksuggest.sponsored": true,
    },
    providers: [
      UrlbarProviderHeuristicFallback.name,
      UrlbarProviderQuickSuggest.name,
      UrlbarProviderPlaces.name,
    ],
    expected: [
      EXPECTED_GENERAL_HEURISTIC_RESULT,
      EXPECTED_GENERAL_PLACES_RESULT,
      createExpectedQuickSuggestResult(SPONSORED_NORMAL_POSITION_SUGGEST),
    ],
  },
  {
    description: "Test for second placable nonsponsored suggest",
    input: NONSPONSORED_SECOND_POSITION_SUGGEST.keywords[0],
    prefs: {
      "quicksuggest.allowPositionInSuggestions": true,
      "suggest.quicksuggest.nonsponsored": true,
    },
    providers: [
      UrlbarProviderHeuristicFallback.name,
      UrlbarProviderQuickSuggest.name,
      UrlbarProviderPlaces.name,
    ],
    expected: [
      EXPECTED_GENERAL_HEURISTIC_RESULT,
      createExpectedQuickSuggestResult(NONSPONSORED_SECOND_POSITION_SUGGEST),
      EXPECTED_GENERAL_PLACES_RESULT,
    ],
  },
  {
    description: "Test for normal nonsponsored suggest",
    input: NONSPONSORED_NORMAL_POSITION_SUGGEST.keywords[0],
    prefs: {
      "quicksuggest.allowPositionInSuggestions": true,
      "suggest.quicksuggest.nonsponsored": true,
    },
    providers: [
      UrlbarProviderHeuristicFallback.name,
      UrlbarProviderQuickSuggest.name,
      UrlbarProviderPlaces.name,
    ],
    expected: [
      EXPECTED_GENERAL_HEURISTIC_RESULT,
      EXPECTED_GENERAL_PLACES_RESULT,
      createExpectedQuickSuggestResult(NONSPONSORED_NORMAL_POSITION_SUGGEST),
    ],
  },
  {
    description:
      "Test for second placable sponsored suggest but secondPosition pref is disabled",
    input: SPONSORED_SECOND_POSITION_SUGGEST.keywords[0],
    prefs: {
      "quicksuggest.allowPositionInSuggestions": false,
      "suggest.quicksuggest.sponsored": true,
    },
    providers: [
      UrlbarProviderHeuristicFallback.name,
      UrlbarProviderQuickSuggest.name,
      UrlbarProviderPlaces.name,
    ],
    expected: [
      EXPECTED_GENERAL_HEURISTIC_RESULT,
      EXPECTED_GENERAL_PLACES_RESULT,
      createExpectedQuickSuggestResult(SPONSORED_SECOND_POSITION_SUGGEST),
    ],
  },
  {
    description: "Test the results with multi providers having same index",
    input: SECOND_POSITION_SUGGEST.keywords[0],
    prefs: {
      "quicksuggest.allowPositionInSuggestions": true,
      "suggest.quicksuggest.sponsored": true,
    },
    providers: [
      UrlbarProviderQuickSuggest.name,
      UrlbarProviderTabToSearch.name,
      SECOND_POSITION_INTERVENTION_RESULT_PROVIDER.name,
    ],
    expected: [
      EXPECTED_GENERAL_TABTOSEARCH_RESULT,
      createExpectedQuickSuggestResult(SECOND_POSITION_SUGGEST),
      EXPECTED_GENERAL_INTERVENTION_RESULT,
    ],
  },
  {
    description: "Test the results with tab-to-search",
    input: SECOND_POSITION_SUGGEST.keywords[0],
    prefs: {
      "quicksuggest.allowPositionInSuggestions": true,
      "suggest.quicksuggest.sponsored": true,
    },
    providers: [
      UrlbarProviderTabToSearch.name,
      UrlbarProviderQuickSuggest.name,
    ],
    expected: [
      EXPECTED_GENERAL_TABTOSEARCH_RESULT,
      createExpectedQuickSuggestResult(SECOND_POSITION_SUGGEST),
    ],
  },
  {
    description: "Test the results with another intervention",
    input: SECOND_POSITION_SUGGEST.keywords[0],
    prefs: {
      "quicksuggest.allowPositionInSuggestions": true,
      "suggest.quicksuggest.sponsored": true,
    },
    providers: [
      UrlbarProviderQuickSuggest.name,
      SECOND_POSITION_INTERVENTION_RESULT_PROVIDER.name,
    ],
    expected: [
      createExpectedQuickSuggestResult(SECOND_POSITION_SUGGEST),
      EXPECTED_GENERAL_INTERVENTION_RESULT,
    ],
  },
  {
    description: "Test the results with heuristic and tab-to-search",
    input: SECOND_POSITION_SUGGEST.keywords[0],
    prefs: {
      "quicksuggest.allowPositionInSuggestions": true,
      "suggest.quicksuggest.sponsored": true,
    },
    providers: [
      UrlbarProviderHeuristicFallback.name,
      UrlbarProviderTabToSearch.name,
      UrlbarProviderQuickSuggest.name,
    ],
    expected: [
      EXPECTED_GENERAL_HEURISTIC_RESULT,
      EXPECTED_GENERAL_TABTOSEARCH_RESULT,
      createExpectedQuickSuggestResult(SECOND_POSITION_SUGGEST),
    ],
  },
  {
    description: "Test the results with heuristic tab-to-search and places",
    input: SECOND_POSITION_SUGGEST.keywords[0],
    prefs: {
      "quicksuggest.allowPositionInSuggestions": true,
      "suggest.quicksuggest.sponsored": true,
    },
    providers: [
      UrlbarProviderHeuristicFallback.name,
      UrlbarProviderTabToSearch.name,
      UrlbarProviderQuickSuggest.name,
      UrlbarProviderPlaces.name,
    ],
    expected: [
      EXPECTED_GENERAL_HEURISTIC_RESULT,
      EXPECTED_GENERAL_TABTOSEARCH_RESULT,
      createExpectedQuickSuggestResult(SECOND_POSITION_SUGGEST),
      EXPECTED_GENERAL_PLACES_RESULT,
    ],
  },
  {
    description: "Test the results with heuristic and another intervention",
    input: SECOND_POSITION_SUGGEST.keywords[0],
    prefs: {
      "quicksuggest.allowPositionInSuggestions": true,
      "suggest.quicksuggest.sponsored": true,
    },
    providers: [
      UrlbarProviderHeuristicFallback.name,
      UrlbarProviderQuickSuggest.name,
      SECOND_POSITION_INTERVENTION_RESULT_PROVIDER.name,
    ],
    expected: [
      EXPECTED_GENERAL_HEURISTIC_RESULT,
      createExpectedQuickSuggestResult(SECOND_POSITION_SUGGEST),
      EXPECTED_GENERAL_INTERVENTION_RESULT,
    ],
  },
  {
    description:
      "Test the results with heuristic, another intervention and places",
    input: SECOND_POSITION_SUGGEST.keywords[0],
    prefs: {
      "quicksuggest.allowPositionInSuggestions": true,
      "suggest.quicksuggest.sponsored": true,
    },
    providers: [
      UrlbarProviderHeuristicFallback.name,
      UrlbarProviderQuickSuggest.name,
      SECOND_POSITION_INTERVENTION_RESULT_PROVIDER.name,
      UrlbarProviderPlaces.name,
    ],
    expected: [
      EXPECTED_GENERAL_HEURISTIC_RESULT,
      createExpectedQuickSuggestResult(SECOND_POSITION_SUGGEST),
      EXPECTED_GENERAL_INTERVENTION_RESULT,
      EXPECTED_GENERAL_PLACES_RESULT,
    ],
  },
  {
    description: "Test for 0 indexed quick suggest",
    input: FIRST_POSITION_SUGGEST.keywords[0],
    prefs: {
      "quicksuggest.allowPositionInSuggestions": true,
      "suggest.quicksuggest.sponsored": true,
    },
    providers: [
      UrlbarProviderTabToSearch.name,
      UrlbarProviderQuickSuggest.name,
    ],
    expected: [
      createExpectedQuickSuggestResult(FIRST_POSITION_SUGGEST),
      EXPECTED_GENERAL_TABTOSEARCH_RESULT,
    ],
  },
  {
    description: "Test for 2 indexed quick suggest",
    input: THIRD_POSITION_SUGGEST.keywords[0],
    prefs: {
      "quicksuggest.allowPositionInSuggestions": true,
      "suggest.quicksuggest.sponsored": true,
    },
    providers: [
      UrlbarProviderQuickSuggest.name,
      SECOND_POSITION_INTERVENTION_RESULT_PROVIDER.name,
    ],
    expected: [
      EXPECTED_GENERAL_INTERVENTION_RESULT,
      createExpectedQuickSuggestResult(THIRD_POSITION_SUGGEST),
    ],
  },
];

add_task(async function setup() {
  UrlbarPrefs.set("quicksuggest.enabled", true);

  // Setup for quick suggest result.
  await QuickSuggestTestUtils.ensureQuickSuggestInit([
    SPONSORED_SECOND_POSITION_SUGGEST,
    SPONSORED_NORMAL_POSITION_SUGGEST,
    NONSPONSORED_SECOND_POSITION_SUGGEST,
    NONSPONSORED_NORMAL_POSITION_SUGGEST,
    FIRST_POSITION_SUGGEST,
    SECOND_POSITION_SUGGEST,
    THIRD_POSITION_SUGGEST,
  ]);

  // Setup for places result.
  await PlacesUtils.history.clear();
  await PlacesTestUtils.addVisits([
    "http://example.com/" + SPONSORED_SECOND_POSITION_SUGGEST.keywords[0],
    "http://example.com/" + SPONSORED_NORMAL_POSITION_SUGGEST.keywords[0],
    "http://example.com/" + NONSPONSORED_SECOND_POSITION_SUGGEST.keywords[0],
    "http://example.com/" + NONSPONSORED_NORMAL_POSITION_SUGGEST.keywords[0],
    "http://example.com/" + SECOND_POSITION_SUGGEST.keywords[0],
  ]);

  // Setup for tab-to-search result.
  await SearchTestUtils.installSearchExtension({
    name: "first",
    search_url: `https://${TABTOSEARCH_ENGINE_DOMAIN_FOR_FIRST_POSITION_TEST}/`,
  });
  await SearchTestUtils.installSearchExtension({
    name: "second",
    search_url: `https://${TABTOSEARCH_ENGINE_DOMAIN_FOR_SECOND_POSITION_TEST}/`,
  });

  /// Setup for another intervention result.
  UrlbarProvidersManager.registerProvider(
    SECOND_POSITION_INTERVENTION_RESULT_PROVIDER
  );
});

add_task(async function basic() {
  for (const { description, input, prefs, providers, expected } of TEST_CASES) {
    info(description);

    for (let name in prefs) {
      UrlbarPrefs.set(name, prefs[name]);
    }

    const context = createContext(input, {
      providers,
      isPrivate: false,
    });
    await check_results({
      context,
      matches: expected,
    });

    for (let name in prefs) {
      UrlbarPrefs.clear(name);
    }
  }
});
