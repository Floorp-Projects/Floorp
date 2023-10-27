/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

// Tests top pick quick suggest results. "Top picks" refers to two different
// concepts:
//
// (1) Any type of suggestion from Merino can have a boolean property called
//     `is_top_pick`. When true, Firefox should show the suggestion using the
//     "best match" UI treatment (labeled "top pick" in the UI) that makes a
//     result's row larger than usual and sets `suggestedIndex` to 1.
// (2) There is a Merino provider called "top_picks" that returns a specific
//     type of suggestion called "navigational suggestions". These suggestions
//     also have `is_top_pick` set to true.
//
// This file tests aspects of both concepts.

"use strict";

const SUGGESTION_SEARCH_STRING = "example";
const SUGGESTION_URL = "http://example.com/";
const SUGGESTION_URL_WWW = "http://www.example.com/";
const SUGGESTION_URL_DISPLAY = "http://example.com";

const MERINO_SUGGESTIONS = [
  {
    is_top_pick: true,
    provider: "top_picks",
    url: SUGGESTION_URL,
    title: "title",
    icon: "icon",
    is_sponsored: false,
    score: 1,
  },
];

add_setup(async function init() {
  // Disable search suggestions so we don't hit the network.
  Services.prefs.setBoolPref("browser.search.suggest.enabled", false);

  await QuickSuggestTestUtils.ensureQuickSuggestInit({
    merinoSuggestions: MERINO_SUGGESTIONS,
    prefs: [["suggest.quicksuggest.nonsponsored", true]],
  });
});

// When non-sponsored suggestions are disabled, navigational suggestions should
// be disabled.
add_task(async function nonsponsoredDisabled() {
  // Disable sponsored suggestions. Navigational suggestions are non-sponsored,
  // so doing this should not prevent them from being enabled.
  UrlbarPrefs.set("suggest.quicksuggest.sponsored", false);

  // First make sure the suggestion is added when non-sponsored suggestions are
  // enabled.
  await check_results({
    context: createContext(SUGGESTION_SEARCH_STRING, {
      providers: [UrlbarProviderQuickSuggest.name],
      isPrivate: false,
    }),
    matches: [
      makeExpectedResult({
        isBestMatch: true,
        suggestedIndex: 1,
      }),
    ],
  });

  // Now disable them.
  UrlbarPrefs.set("suggest.quicksuggest.nonsponsored", false);
  await check_results({
    context: createContext(SUGGESTION_SEARCH_STRING, {
      providers: [UrlbarProviderQuickSuggest.name],
      isPrivate: false,
    }),
    matches: [],
  });

  UrlbarPrefs.set("suggest.quicksuggest.nonsponsored", true);
  UrlbarPrefs.clear("suggest.quicksuggest.sponsored");
});

// Test that bestMatch navigational suggestion results are not shown when there
// is a heuristic result for the same domain.
add_task(async function heuristicDeduplication() {
  let expectedNavSuggestResult = makeExpectedResult({
    isBestMatch: true,
    suggestedIndex: 1,
    dupedHeuristic: false,
  });

  let scenarios = [
    [SUGGESTION_URL, false],
    [SUGGESTION_URL_WWW, false],
    ["http://exampledomain.com/", true],
  ];

  // Stub `UrlbarProviderQuickSuggest.startQuery()` so we can collect the
  // results it adds for each query.
  let addedResults = [];
  let sandbox = sinon.createSandbox();
  let startQueryStub = sandbox.stub(UrlbarProviderQuickSuggest, "startQuery");
  startQueryStub.callsFake((queryContext, add) => {
    let fakeAdd = (provider, result) => {
      addedResults.push(result);
      add(provider, result);
    };
    return startQueryStub.wrappedMethod.call(
      UrlbarProviderQuickSuggest,
      queryContext,
      fakeAdd
    );
  });

  for (let [url, expectBestMatch] of scenarios) {
    await PlacesTestUtils.addVisits(url);

    // Do a search and check the results.
    let context = createContext(SUGGESTION_SEARCH_STRING, {
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

    // Regardless of whether it was shown, one result should have been added and
    // its `payload.dupedHeuristic` should be set properly.
    Assert.equal(
      addedResults.length,
      1,
      "The provider should have added one result"
    );
    Assert.equal(
      !addedResults[0].payload.dupedHeuristic,
      expectBestMatch,
      "dupedHeuristic should be the opposite of expectBestMatch"
    );
    addedResults = [];

    await PlacesUtils.history.clear();
  }

  sandbox.restore();
});

function makeExpectedResult({
  isBestMatch,
  suggestedIndex,
  dupedHeuristic,
  telemetryType = "top_picks",
}) {
  let result = {
    isBestMatch,
    suggestedIndex,
    type: UrlbarUtils.RESULT_TYPE.URL,
    source: UrlbarUtils.RESULT_SOURCE.SEARCH,
    heuristic: false,
    payload: {
      dupedHeuristic,
      telemetryType,
      title: "title",
      url: SUGGESTION_URL,
      displayUrl: SUGGESTION_URL_DISPLAY,
      icon: "icon",
      isSponsored: false,
      shouldShowUrl: true,
      source: "merino",
      provider: telemetryType,
      helpUrl: QuickSuggest.HELP_URL,
      helpL10n: {
        id: "urlbar-result-menu-learn-more-about-firefox-suggest",
      },
      isBlockable: true,
      blockL10n: {
        id: "urlbar-result-menu-dismiss-firefox-suggest",
      },
    },
  };
  if (typeof dupedHeuristic == "boolean") {
    result.payload.dupedHeuristic = dupedHeuristic;
  }
  return result;
}
