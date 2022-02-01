/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

// Tests best match quick suggest results.

"use strict";

XPCOMUtils.defineLazyModuleGetters(this, {
  UrlbarProviderQuickSuggest:
    "resource:///modules/UrlbarProviderQuickSuggest.jsm",
});

const MAX_RESULT_COUNT = UrlbarPrefs.get("maxRichResults");

const BEST_MATCH_SEARCH_STRING = "bestmatch";
const NON_BEST_MATCH_SEARCH_STRING = "foobar";
const BEST_MATCH_POSITION_SEARCH_STRING = "bestmatchposition";
const BEST_MATCH_POSITION = Math.round(MAX_RESULT_COUNT / 2);

const SUGGESTIONS = [
  {
    id: 1,
    url: "http://example.com/best-match",
    title: `${BEST_MATCH_SEARCH_STRING} title`,
    // Include a keyword that's a substring of the full search string so we can
    // test title highlights. The suggestion's `full_keyword` will end up being
    // the full search string, and when we search for the substring keyword,
    // `result.titleHighlights` will contain the remaining part of the full
    // string. (This only applies when best match is disabled.)
    keywords: [BEST_MATCH_SEARCH_STRING.slice(0, -1), BEST_MATCH_SEARCH_STRING],
    click_url: "http://example.com/click",
    impression_url: "http://example.com/impression",
    advertiser: "TestAdvertiser",
    _test_is_best_match: true,
  },
  {
    id: 2,
    url: "http://example.com/non-best-match",
    title: `${NON_BEST_MATCH_SEARCH_STRING} title`,
    // See above, but since this is the non-best match suggestion, the comment
    // always applies here.
    keywords: [
      NON_BEST_MATCH_SEARCH_STRING.slice(0, -1),
      NON_BEST_MATCH_SEARCH_STRING,
    ],
    click_url: "http://example.com/click",
    impression_url: "http://example.com/impression",
    advertiser: "TestAdvertiser",
  },
  {
    id: 3,
    url: "http://example.com/best-match-position",
    title: `${BEST_MATCH_POSITION_SEARCH_STRING} title`,
    keywords: [BEST_MATCH_POSITION_SEARCH_STRING],
    click_url: "http://example.com/click",
    impression_url: "http://example.com/impression",
    advertiser: "TestAdvertiser",
    position: BEST_MATCH_POSITION,
    _test_is_best_match: true,
  },
];

const EXPECTED_BEST_MATCH_RESULT = {
  type: UrlbarUtils.RESULT_TYPE.URL,
  source: UrlbarUtils.RESULT_SOURCE.SEARCH,
  heuristic: false,
  isBestMatch: true,
  payload: {
    url: "http://example.com/best-match",
    title: `${BEST_MATCH_SEARCH_STRING} title`,
    icon: null,
    isSponsored: true,
    sponsoredImpressionUrl: "http://example.com/impression",
    sponsoredClickUrl: "http://example.com/click",
    sponsoredBlockId: 1,
    sponsoredAdvertiser: "TestAdvertiser",
    helpUrl: UrlbarProviderQuickSuggest.helpUrl,
    helpL10nId: "firefox-suggest-urlbar-learn-more",
    displayUrl: "http://example.com/best-match",
    source: "remote-settings",
  },
};

const EXPECTED_NON_BEST_MATCH_RESULT = {
  type: UrlbarUtils.RESULT_TYPE.URL,
  source: UrlbarUtils.RESULT_SOURCE.SEARCH,
  heuristic: false,
  payload: {
    url: "http://example.com/non-best-match",
    title: `${NON_BEST_MATCH_SEARCH_STRING} title`,
    qsSuggestion: NON_BEST_MATCH_SEARCH_STRING,
    icon: null,
    isSponsored: true,
    sponsoredImpressionUrl: "http://example.com/impression",
    sponsoredClickUrl: "http://example.com/click",
    sponsoredBlockId: 2,
    sponsoredAdvertiser: "TestAdvertiser",
    helpUrl: UrlbarProviderQuickSuggest.helpUrl,
    helpL10nId: "firefox-suggest-urlbar-learn-more",
    displayUrl: "http://example.com/non-best-match",
    source: "remote-settings",
  },
};

const EXPECTED_BEST_MATCH_POSITION_RESULT = {
  type: UrlbarUtils.RESULT_TYPE.URL,
  source: UrlbarUtils.RESULT_SOURCE.SEARCH,
  heuristic: false,
  isBestMatch: true,
  payload: {
    url: "http://example.com/best-match-position",
    title: `${BEST_MATCH_POSITION_SEARCH_STRING} title`,
    icon: null,
    isSponsored: true,
    sponsoredImpressionUrl: "http://example.com/impression",
    sponsoredClickUrl: "http://example.com/click",
    sponsoredBlockId: 3,
    sponsoredAdvertiser: "TestAdvertiser",
    helpUrl: UrlbarProviderQuickSuggest.helpUrl,
    helpL10nId: "firefox-suggest-urlbar-learn-more",
    displayUrl: "http://example.com/best-match-position",
    source: "remote-settings",
  },
};

add_task(async function init() {
  UrlbarPrefs.set("quicksuggest.enabled", true);
  UrlbarPrefs.set("suggest.quicksuggest.nonsponsored", true);
  UrlbarPrefs.set("suggest.quicksuggest.sponsored", true);
  UrlbarPrefs.set("bestMatch.enabled", true);

  // Disable search suggestions so we don't hit the network.
  Services.prefs.setBoolPref("browser.search.suggest.enabled", false);

  await QuickSuggestTestUtils.ensureQuickSuggestInit(SUGGESTIONS);
});

// Tests a best match result.
add_task(async function bestMatch() {
  let context = createContext(BEST_MATCH_SEARCH_STRING, {
    providers: [UrlbarProviderQuickSuggest.name],
    isPrivate: false,
  });
  await check_results({
    context,
    matches: [EXPECTED_BEST_MATCH_RESULT],
  });

  let result = context.results[0];

  // The title should not include the full keyword and em dash, and the part of
  // the title that the search string matches should be highlighted.
  Assert.equal(
    result.title,
    `${BEST_MATCH_SEARCH_STRING} title`,
    "result.title"
  );
  Assert.deepEqual(
    result.titleHighlights,
    [[0, BEST_MATCH_SEARCH_STRING.length]],
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
  let searchString = NON_BEST_MATCH_SEARCH_STRING.slice(0, -1);
  let context = createContext(searchString, {
    providers: [UrlbarProviderQuickSuggest.name],
    isPrivate: false,
  });
  await check_results({
    context,
    matches: [EXPECTED_NON_BEST_MATCH_RESULT],
  });

  let result = context.results[0];

  // The title should include the full keyword and em dash, and the part of the
  // title that the search string does not match should be highlighted.
  Assert.equal(
    result.title,
    `${NON_BEST_MATCH_SEARCH_STRING} — ${NON_BEST_MATCH_SEARCH_STRING} title`,
    "result.title"
  );
  Assert.deepEqual(
    result.titleHighlights,
    [[NON_BEST_MATCH_SEARCH_STRING.length - 1, 1]],
    "result.titleHighlights"
  );

  Assert.equal(result.suggestedIndex, -1, "result.suggestedIndex");
  Assert.equal(
    result.isSuggestedIndexRelativeToGroup,
    true,
    "result.isSuggestedIndexRelativeToGroup"
  );
});

// When tab-to-search is shown in the same search, both it and the best match
// will have a `suggestedIndex` value of 1. The TTS should appear first.
add_task(async function tabToSearch() {
  // Disable tab-to-search onboarding results so we get a regular TTS result,
  // which we can test a little more easily with `makeSearchResult()`.
  UrlbarPrefs.set("tabToSearch.onboard.interactionsLeft", 0);

  // Install a test engine. The main part of its domain name needs to match the
  // best match result too so we can trigger both its TTS and the best match.
  let engineURL = `https://foo.${BEST_MATCH_SEARCH_STRING}.com/`;
  let extension = await SearchTestUtils.installSearchExtension(
    {
      name: "Test",
      search_url: engineURL,
    },
    true
  );
  let engine = Services.search.getEngineByName("Test");

  // Also need to add a visit to trigger TTS.
  await PlacesTestUtils.addVisits(engineURL);

  let context = createContext(BEST_MATCH_SEARCH_STRING, {
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
        uri: UrlbarUtils.stripPublicSuffixFromHost(engine.getResultDomain()),
        providesSearchMode: true,
        query: "",
        providerName: "TabToSearch",
        satisfiesAutofillThreshold: true,
      }),
      // best match
      EXPECTED_BEST_MATCH_RESULT,
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

// When best match is disabled, quick suggest results should be shown as the
// usual, non-best match results.
add_task(async function disabled() {
  UrlbarPrefs.set("bestMatch.enabled", false);

  // Since best match is disabled, the "best match" suggestion will not actually
  // be a best match here, so modify a copy of it accordingly.
  let expectedResult = { ...EXPECTED_BEST_MATCH_RESULT };
  delete expectedResult.isBestMatch;
  expectedResult.payload = { ...expectedResult.payload };
  expectedResult.payload.qsSuggestion = BEST_MATCH_SEARCH_STRING;

  // Search for a substring of the full search string so we can test title
  // highlights.
  let searchString = BEST_MATCH_SEARCH_STRING.slice(0, -1);
  let context = createContext(searchString, {
    providers: [UrlbarProviderQuickSuggest.name],
    isPrivate: false,
  });
  await check_results({
    context,
    matches: [expectedResult],
  });

  let result = context.results[0];

  // The title should include the full keyword and em dash, and the part of the
  // title that the search string does not match should be highlighted.
  Assert.equal(
    result.title,
    `${BEST_MATCH_SEARCH_STRING} — ${BEST_MATCH_SEARCH_STRING} title`,
    "result.title"
  );
  Assert.deepEqual(
    result.titleHighlights,
    [[BEST_MATCH_SEARCH_STRING.length - 1, 1]],
    "result.titleHighlights"
  );

  Assert.equal(result.suggestedIndex, -1, "result.suggestedIndex");
  Assert.equal(
    result.isSuggestedIndexRelativeToGroup,
    true,
    "result.isSuggestedIndexRelativeToGroup"
  );

  UrlbarPrefs.set("bestMatch.enabled", true);
});

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
      EXPECTED_BEST_MATCH_POSITION_RESULT,
      // visits
      ...visitResults.slice(0, MAX_RESULT_COUNT - 2),
    ],
  });

  await cleanupPlaces();
  UrlbarPrefs.clear("quicksuggest.allowPositionInSuggestions");
});
