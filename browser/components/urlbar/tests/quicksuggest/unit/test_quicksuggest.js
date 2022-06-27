/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

// Basic tests for the quick suggest provider using the remote settings source.
// See also test_quicksuggest_merino.js.

"use strict";

const TELEMETRY_REMOTE_SETTINGS_LATENCY =
  "FX_URLBAR_QUICK_SUGGEST_REMOTE_SETTINGS_LATENCY_MS";

const SPONSORED_SEARCH_STRING = "frab";
const NONSPONSORED_SEARCH_STRING = "nonspon";

const HTTP_SEARCH_STRING = "http prefix";
const HTTPS_SEARCH_STRING = "https prefix";
const PREFIX_SUGGESTIONS_STRIPPED_URL = "example.com/prefix-test";

const { TIMESTAMP_TEMPLATE, TIMESTAMP_LENGTH } = UrlbarProviderQuickSuggest;
const TIMESTAMP_SEARCH_STRING = "timestamp";
const TIMESTAMP_SUGGESTION_URL = `http://example.com/timestamp-${TIMESTAMP_TEMPLATE}`;
const TIMESTAMP_SUGGESTION_CLICK_URL = `http://click.reporting.test.com/timestamp-${TIMESTAMP_TEMPLATE}-foo`;

const REMOTE_SETTINGS_DATA = [
  {
    id: 1,
    url: "http://test.com/q=frabbits",
    title: "frabbits",
    keywords: [SPONSORED_SEARCH_STRING],
    click_url: "http://click.reporting.test.com/",
    impression_url: "http://impression.reporting.test.com/",
    advertiser: "TestAdvertiser",
    iab_category: "22 - Shopping",
  },
  {
    id: 2,
    url: "http://test.com/?q=nonsponsored",
    title: "Non-Sponsored",
    keywords: [NONSPONSORED_SEARCH_STRING],
    click_url: "http://click.reporting.test.com/nonsponsored",
    impression_url: "http://impression.reporting.test.com/nonsponsored",
    advertiser: "TestAdvertiserNonSponsored",
    iab_category: "5 - Education",
  },
  {
    id: 3,
    url: "http://" + PREFIX_SUGGESTIONS_STRIPPED_URL,
    title: "http suggestion",
    keywords: [HTTP_SEARCH_STRING],
    click_url: "http://click.reporting.test.com/prefix",
    impression_url: "http://impression.reporting.test.com/prefix",
    advertiser: "TestAdvertiserPrefix",
    iab_category: "22 - Shopping",
  },
  {
    id: 4,
    url: "https://" + PREFIX_SUGGESTIONS_STRIPPED_URL,
    title: "https suggestion",
    keywords: [HTTPS_SEARCH_STRING],
    click_url: "http://click.reporting.test.com/prefix",
    impression_url: "http://impression.reporting.test.com/prefix",
    advertiser: "TestAdvertiserPrefix",
    iab_category: "22 - Shopping",
  },
  {
    id: 5,
    url: TIMESTAMP_SUGGESTION_URL,
    title: "Timestamp suggestion",
    keywords: [TIMESTAMP_SEARCH_STRING],
    click_url: TIMESTAMP_SUGGESTION_CLICK_URL,
    impression_url: "http://impression.reporting.test.com/timestamp",
    advertiser: "TestAdvertiserTimestamp",
    iab_category: "22 - Shopping",
  },
];

const EXPECTED_SPONSORED_RESULT = {
  type: UrlbarUtils.RESULT_TYPE.URL,
  source: UrlbarUtils.RESULT_SOURCE.SEARCH,
  heuristic: false,
  payload: {
    qsSuggestion: "frab",
    title: "frabbits",
    url: "http://test.com/q=frabbits",
    originalUrl: "http://test.com/q=frabbits",
    icon: null,
    sponsoredImpressionUrl: "http://impression.reporting.test.com/",
    sponsoredClickUrl: "http://click.reporting.test.com/",
    sponsoredBlockId: 1,
    sponsoredAdvertiser: "TestAdvertiser",
    sponsoredIabCategory: "22 - Shopping",
    isSponsored: true,
    helpUrl: UrlbarProviderQuickSuggest.helpUrl,
    helpL10nId: "firefox-suggest-urlbar-learn-more",
    displayUrl: "http://test.com/q=frabbits",
    source: "remote-settings",
  },
};

const EXPECTED_NONSPONSORED_RESULT = {
  type: UrlbarUtils.RESULT_TYPE.URL,
  source: UrlbarUtils.RESULT_SOURCE.SEARCH,
  heuristic: false,
  payload: {
    qsSuggestion: "nonspon",
    title: "Non-Sponsored",
    url: "http://test.com/?q=nonsponsored",
    originalUrl: "http://test.com/?q=nonsponsored",
    icon: null,
    sponsoredImpressionUrl: "http://impression.reporting.test.com/nonsponsored",
    sponsoredClickUrl: "http://click.reporting.test.com/nonsponsored",
    sponsoredBlockId: 2,
    sponsoredAdvertiser: "TestAdvertiserNonSponsored",
    sponsoredIabCategory: "5 - Education",
    isSponsored: false,
    helpUrl: UrlbarProviderQuickSuggest.helpUrl,
    helpL10nId: "firefox-suggest-urlbar-learn-more",
    displayUrl: "http://test.com/?q=nonsponsored",
    source: "remote-settings",
  },
};

const EXPECTED_HTTP_RESULT = {
  type: UrlbarUtils.RESULT_TYPE.URL,
  source: UrlbarUtils.RESULT_SOURCE.SEARCH,
  heuristic: false,
  payload: {
    qsSuggestion: HTTP_SEARCH_STRING,
    title: "http suggestion",
    url: "http://" + PREFIX_SUGGESTIONS_STRIPPED_URL,
    originalUrl: "http://" + PREFIX_SUGGESTIONS_STRIPPED_URL,
    icon: null,
    sponsoredImpressionUrl: "http://impression.reporting.test.com/prefix",
    sponsoredClickUrl: "http://click.reporting.test.com/prefix",
    sponsoredBlockId: 3,
    sponsoredAdvertiser: "TestAdvertiserPrefix",
    sponsoredIabCategory: "22 - Shopping",
    isSponsored: true,
    helpUrl: UrlbarProviderQuickSuggest.helpUrl,
    helpL10nId: "firefox-suggest-urlbar-learn-more",
    displayUrl: "http://" + PREFIX_SUGGESTIONS_STRIPPED_URL,
    source: "remote-settings",
  },
};

const EXPECTED_HTTPS_RESULT = {
  type: UrlbarUtils.RESULT_TYPE.URL,
  source: UrlbarUtils.RESULT_SOURCE.SEARCH,
  heuristic: false,
  payload: {
    qsSuggestion: HTTPS_SEARCH_STRING,
    title: "https suggestion",
    url: "https://" + PREFIX_SUGGESTIONS_STRIPPED_URL,
    originalUrl: "https://" + PREFIX_SUGGESTIONS_STRIPPED_URL,
    icon: null,
    sponsoredImpressionUrl: "http://impression.reporting.test.com/prefix",
    sponsoredClickUrl: "http://click.reporting.test.com/prefix",
    sponsoredBlockId: 4,
    sponsoredAdvertiser: "TestAdvertiserPrefix",
    sponsoredIabCategory: "22 - Shopping",
    isSponsored: true,
    helpUrl: UrlbarProviderQuickSuggest.helpUrl,
    helpL10nId: "firefox-suggest-urlbar-learn-more",
    displayUrl: PREFIX_SUGGESTIONS_STRIPPED_URL,
    source: "remote-settings",
  },
};

let cleanUpQuickSuggest;

add_task(async function init() {
  UrlbarPrefs.set("quicksuggest.enabled", true);
  UrlbarPrefs.set("quicksuggest.shouldShowOnboardingDialog", false);
  UrlbarPrefs.set("quicksuggest.remoteSettings.enabled", true);
  UrlbarPrefs.set("merino.enabled", false);

  // Install a default test engine.
  let engine = await addTestSuggestionsEngine();
  await Services.search.setDefault(engine);

  cleanUpQuickSuggest = await QuickSuggestTestUtils.ensureQuickSuggestInit(
    REMOTE_SETTINGS_DATA
  );
});

// Tests with only non-sponsored suggestions enabled with a matching search
// string.
add_task(async function nonsponsoredOnly_match() {
  UrlbarPrefs.set("suggest.quicksuggest.nonsponsored", true);
  UrlbarPrefs.set("suggest.quicksuggest.sponsored", false);

  let context = createContext(NONSPONSORED_SEARCH_STRING, {
    providers: [UrlbarProviderQuickSuggest.name],
    isPrivate: false,
  });
  await check_results({
    context,
    matches: [EXPECTED_NONSPONSORED_RESULT],
  });
});

// Tests with only non-sponsored suggestions enabled with a non-matching search
// string.
add_task(async function nonsponsoredOnly_noMatch() {
  UrlbarPrefs.set("suggest.quicksuggest.nonsponsored", true);
  UrlbarPrefs.set("suggest.quicksuggest.sponsored", false);

  let context = createContext(SPONSORED_SEARCH_STRING, {
    providers: [UrlbarProviderQuickSuggest.name],
    isPrivate: false,
  });
  await check_results({ context, matches: [] });
});

// Tests with only sponsored suggestions enabled with a matching search string.
add_task(async function sponsoredOnly_sponsored() {
  UrlbarPrefs.set("suggest.quicksuggest.nonsponsored", false);
  UrlbarPrefs.set("suggest.quicksuggest.sponsored", true);

  let context = createContext(SPONSORED_SEARCH_STRING, {
    providers: [UrlbarProviderQuickSuggest.name],
    isPrivate: false,
  });
  await check_results({
    context,
    matches: [EXPECTED_SPONSORED_RESULT],
  });
});

// Tests with only sponsored suggestions enabled with a non-matching search
// string.
add_task(async function sponsoredOnly_nonsponsored() {
  UrlbarPrefs.set("suggest.quicksuggest.nonsponsored", false);
  UrlbarPrefs.set("suggest.quicksuggest.sponsored", true);

  let context = createContext(NONSPONSORED_SEARCH_STRING, {
    providers: [UrlbarProviderQuickSuggest.name],
    isPrivate: false,
  });
  await check_results({ context, matches: [] });
});

// Tests with both sponsored and non-sponsored suggestions enabled with a
// search string that matches the sponsored suggestion.
add_task(async function both_sponsored() {
  UrlbarPrefs.set("suggest.quicksuggest.nonsponsored", true);
  UrlbarPrefs.set("suggest.quicksuggest.sponsored", true);

  let context = createContext(SPONSORED_SEARCH_STRING, {
    providers: [UrlbarProviderQuickSuggest.name],
    isPrivate: false,
  });
  await check_results({
    context,
    matches: [EXPECTED_SPONSORED_RESULT],
  });
});

// Tests with both sponsored and non-sponsored suggestions enabled with a
// search string that matches the non-sponsored suggestion.
add_task(async function both_nonsponsored() {
  UrlbarPrefs.set("suggest.quicksuggest.nonsponsored", true);
  UrlbarPrefs.set("suggest.quicksuggest.sponsored", true);

  let context = createContext(NONSPONSORED_SEARCH_STRING, {
    providers: [UrlbarProviderQuickSuggest.name],
    isPrivate: false,
  });
  await check_results({
    context,
    matches: [EXPECTED_NONSPONSORED_RESULT],
  });
});

// Tests with both sponsored and non-sponsored suggestions enabled with a
// search string that doesn't match either suggestion.
add_task(async function both_noMatch() {
  UrlbarPrefs.set("suggest.quicksuggest.nonsponsored", true);
  UrlbarPrefs.set("suggest.quicksuggest.sponsored", true);

  let context = createContext("this doesn't match anything", {
    providers: [UrlbarProviderQuickSuggest.name],
    isPrivate: false,
  });
  await check_results({ context, matches: [] });
});

// Tests with both the main and sponsored prefs disabled with a search string
// that matches the sponsored suggestion.
add_task(async function neither_sponsored() {
  UrlbarPrefs.set("suggest.quicksuggest.nonsponsored", false);
  UrlbarPrefs.set("suggest.quicksuggest.sponsored", false);

  let context = createContext(SPONSORED_SEARCH_STRING, {
    providers: [UrlbarProviderQuickSuggest.name],
    isPrivate: false,
  });
  await check_results({ context, matches: [] });
});

// Tests with both the main and sponsored prefs disabled with a search string
// that matches the non-sponsored suggestion.
add_task(async function neither_nonsponsored() {
  UrlbarPrefs.set("suggest.quicksuggest.nonsponsored", false);
  UrlbarPrefs.set("suggest.quicksuggest.sponsored", false);

  let context = createContext(NONSPONSORED_SEARCH_STRING, {
    providers: [UrlbarProviderQuickSuggest.name],
    isPrivate: false,
  });
  await check_results({ context, matches: [] });
});

// Search string matching should be case insensitive and ignore leading spaces.
add_task(async function caseInsensitiveAndLeadingSpaces() {
  UrlbarPrefs.set("suggest.quicksuggest.nonsponsored", true);
  UrlbarPrefs.set("suggest.quicksuggest.sponsored", true);

  let context = createContext("  " + SPONSORED_SEARCH_STRING.toUpperCase(), {
    providers: [UrlbarProviderQuickSuggest.name],
    isPrivate: false,
  });
  await check_results({
    context,
    matches: [EXPECTED_SPONSORED_RESULT],
  });
});

// Results should be returned even when `browser.search.suggest.enabled` is
// false.
add_task(async function browser_search_suggest_enabled() {
  UrlbarPrefs.set("suggest.quicksuggest.nonsponsored", true);
  UrlbarPrefs.set("suggest.quicksuggest.sponsored", true);
  UrlbarPrefs.set("browser.search.suggest.enabled", false);

  let context = createContext(SPONSORED_SEARCH_STRING, {
    providers: [UrlbarProviderQuickSuggest.name],
    isPrivate: false,
  });
  await check_results({
    context,
    matches: [EXPECTED_SPONSORED_RESULT],
  });

  UrlbarPrefs.clear("browser.search.suggest.enabled");
});

// Results should be returned even when `browser.urlbar.suggest.searches` is
// false.
add_task(async function browser_search_suggest_enabled() {
  UrlbarPrefs.set("suggest.quicksuggest.nonsponsored", true);
  UrlbarPrefs.set("suggest.quicksuggest.sponsored", true);
  UrlbarPrefs.set("suggest.searches", false);

  let context = createContext(SPONSORED_SEARCH_STRING, {
    providers: [UrlbarProviderQuickSuggest.name],
    isPrivate: false,
  });
  await check_results({
    context,
    matches: [EXPECTED_SPONSORED_RESULT],
  });

  UrlbarPrefs.clear("suggest.searches");
});

// Neither sponsored nor non-sponsored results should appear in private contexts
// even when suggestions in private windows are enabled.
add_task(async function privateContext() {
  UrlbarPrefs.set("suggest.quicksuggest.nonsponsored", true);
  UrlbarPrefs.set("suggest.quicksuggest.sponsored", true);

  for (let privateSuggestionsEnabled of [true, false]) {
    UrlbarPrefs.set(
      "browser.search.suggest.enabled.private",
      privateSuggestionsEnabled
    );
    let context = createContext(SPONSORED_SEARCH_STRING, {
      providers: [UrlbarProviderQuickSuggest.name],
      isPrivate: true,
    });
    await check_results({
      context,
      matches: [],
    });
  }

  UrlbarPrefs.clear("browser.search.suggest.enabled.private");
});

// When search suggestions come before general results and the only general
// result is a quick suggest result, it should come last.
add_task(async function suggestionsBeforeGeneral_only() {
  UrlbarPrefs.set("suggest.quicksuggest.nonsponsored", true);
  UrlbarPrefs.set("suggest.quicksuggest.sponsored", true);
  UrlbarPrefs.set("browser.search.suggest.enabled", true);
  UrlbarPrefs.set("suggest.searches", true);
  UrlbarPrefs.set("showSearchSuggestionsFirst", true);

  let context = createContext(SPONSORED_SEARCH_STRING, { isPrivate: false });
  await check_results({
    context,
    matches: [
      makeSearchResult(context, {
        heuristic: true,
        query: SPONSORED_SEARCH_STRING,
        engineName: Services.search.defaultEngine.name,
      }),
      makeSearchResult(context, {
        query: SPONSORED_SEARCH_STRING,
        suggestion: SPONSORED_SEARCH_STRING + " foo",
        engineName: Services.search.defaultEngine.name,
      }),
      makeSearchResult(context, {
        query: SPONSORED_SEARCH_STRING,
        suggestion: SPONSORED_SEARCH_STRING + " bar",
        engineName: Services.search.defaultEngine.name,
      }),
      EXPECTED_SPONSORED_RESULT,
    ],
  });

  UrlbarPrefs.clear("browser.search.suggest.enabled");
  UrlbarPrefs.clear("suggest.searches");
  UrlbarPrefs.clear("showSearchSuggestionsFirst");
});

// When search suggestions come before general results and there are other
// general results besides quick suggest, the quick suggest result should come
// last.
add_task(async function suggestionsBeforeGeneral_others() {
  UrlbarPrefs.set("suggest.quicksuggest.nonsponsored", true);
  UrlbarPrefs.set("suggest.quicksuggest.sponsored", true);
  UrlbarPrefs.set("browser.search.suggest.enabled", true);
  UrlbarPrefs.set("suggest.searches", true);
  UrlbarPrefs.set("showSearchSuggestionsFirst", true);

  let context = createContext(SPONSORED_SEARCH_STRING, { isPrivate: false });

  // Add some history that will match our query below.
  let maxResults = UrlbarPrefs.get("maxRichResults");
  let historyResults = [];
  for (let i = 0; i < maxResults; i++) {
    let url = "http://example.com/" + SPONSORED_SEARCH_STRING + i;
    historyResults.push(
      makeVisitResult(context, {
        uri: url,
        title: "test visit for " + url,
      })
    );
    await PlacesTestUtils.addVisits(url);
  }
  historyResults = historyResults.reverse().slice(0, historyResults.length - 4);

  await check_results({
    context,
    matches: [
      makeSearchResult(context, {
        heuristic: true,
        query: SPONSORED_SEARCH_STRING,
        engineName: Services.search.defaultEngine.name,
      }),
      makeSearchResult(context, {
        query: SPONSORED_SEARCH_STRING,
        suggestion: SPONSORED_SEARCH_STRING + " foo",
        engineName: Services.search.defaultEngine.name,
      }),
      makeSearchResult(context, {
        query: SPONSORED_SEARCH_STRING,
        suggestion: SPONSORED_SEARCH_STRING + " bar",
        engineName: Services.search.defaultEngine.name,
      }),
      ...historyResults,
      EXPECTED_SPONSORED_RESULT,
    ],
  });

  UrlbarPrefs.clear("browser.search.suggest.enabled");
  UrlbarPrefs.clear("suggest.searches");
  UrlbarPrefs.clear("showSearchSuggestionsFirst");
  await PlacesUtils.history.clear();
});

// When general results come before search suggestions and the only general
// result is a quick suggest result, it should come before suggestions.
add_task(async function generalBeforeSuggestions_only() {
  UrlbarPrefs.set("suggest.quicksuggest.nonsponsored", true);
  UrlbarPrefs.set("suggest.quicksuggest.sponsored", true);
  UrlbarPrefs.set("browser.search.suggest.enabled", true);
  UrlbarPrefs.set("suggest.searches", true);
  UrlbarPrefs.set("showSearchSuggestionsFirst", false);

  let context = createContext(SPONSORED_SEARCH_STRING, { isPrivate: false });
  await check_results({
    context,
    matches: [
      makeSearchResult(context, {
        heuristic: true,
        query: SPONSORED_SEARCH_STRING,
        engineName: Services.search.defaultEngine.name,
      }),
      EXPECTED_SPONSORED_RESULT,
      makeSearchResult(context, {
        query: SPONSORED_SEARCH_STRING,
        suggestion: SPONSORED_SEARCH_STRING + " foo",
        engineName: Services.search.defaultEngine.name,
      }),
      makeSearchResult(context, {
        query: SPONSORED_SEARCH_STRING,
        suggestion: SPONSORED_SEARCH_STRING + " bar",
        engineName: Services.search.defaultEngine.name,
      }),
    ],
  });

  UrlbarPrefs.clear("browser.search.suggest.enabled");
  UrlbarPrefs.clear("suggest.searches");
  UrlbarPrefs.clear("showSearchSuggestionsFirst");
});

// When general results come before search suggestions and there are other
// general results besides quick suggest, the quick suggest result should be the
// last general result.
add_task(async function generalBeforeSuggestions_others() {
  UrlbarPrefs.set("suggest.quicksuggest.nonsponsored", true);
  UrlbarPrefs.set("suggest.quicksuggest.sponsored", true);
  UrlbarPrefs.set("browser.search.suggest.enabled", true);
  UrlbarPrefs.set("suggest.searches", true);
  UrlbarPrefs.set("showSearchSuggestionsFirst", false);

  let context = createContext(SPONSORED_SEARCH_STRING, { isPrivate: false });

  // Add some history that will match our query below.
  let maxResults = UrlbarPrefs.get("maxRichResults");
  let historyResults = [];
  for (let i = 0; i < maxResults; i++) {
    let url = "http://example.com/" + SPONSORED_SEARCH_STRING + i;
    historyResults.push(
      makeVisitResult(context, {
        uri: url,
        title: "test visit for " + url,
      })
    );
    await PlacesTestUtils.addVisits(url);
  }
  historyResults = historyResults.reverse().slice(0, historyResults.length - 4);

  await check_results({
    context,
    matches: [
      makeSearchResult(context, {
        heuristic: true,
        query: SPONSORED_SEARCH_STRING,
        engineName: Services.search.defaultEngine.name,
      }),
      ...historyResults,
      EXPECTED_SPONSORED_RESULT,
      makeSearchResult(context, {
        query: SPONSORED_SEARCH_STRING,
        suggestion: SPONSORED_SEARCH_STRING + " foo",
        engineName: Services.search.defaultEngine.name,
      }),
      makeSearchResult(context, {
        query: SPONSORED_SEARCH_STRING,
        suggestion: SPONSORED_SEARCH_STRING + " bar",
        engineName: Services.search.defaultEngine.name,
      }),
    ],
  });

  UrlbarPrefs.clear("browser.search.suggest.enabled");
  UrlbarPrefs.clear("suggest.searches");
  UrlbarPrefs.clear("showSearchSuggestionsFirst");
  await PlacesUtils.history.clear();
});

add_task(async function dedupeAgainstURL_samePrefix() {
  await doDedupeAgainstURLTest({
    searchString: HTTP_SEARCH_STRING,
    expectedQuickSuggestResult: EXPECTED_HTTP_RESULT,
    otherPrefix: "http://",
    expectOther: false,
  });
});

add_task(async function dedupeAgainstURL_higherPrefix() {
  await doDedupeAgainstURLTest({
    searchString: HTTPS_SEARCH_STRING,
    expectedQuickSuggestResult: EXPECTED_HTTPS_RESULT,
    otherPrefix: "http://",
    expectOther: false,
  });
});

add_task(async function dedupeAgainstURL_lowerPrefix() {
  await doDedupeAgainstURLTest({
    searchString: HTTP_SEARCH_STRING,
    expectedQuickSuggestResult: EXPECTED_HTTP_RESULT,
    otherPrefix: "https://",
    expectOther: true,
  });
});

/**
 * Tests how the muxer dedupes URL results against quick suggest results.
 * Depending on prefix rank, quick suggest results should be preferred over
 * other URL results with the same stripped URL: Other results should be
 * discarded when their prefix rank is lower than the prefix rank of the quick
 * suggest. They should not be discarded when their prefix rank is higher, and
 * in that case both results should be included.
 *
 * This function adds a visit to the URL formed by the given `otherPrefix` and
 * `PREFIX_SUGGESTIONS_STRIPPED_URL`. The visit's title will be set to the given
 * `searchString` so that both the visit and the quick suggest will match it.
 *
 * @param {string} searchString
 *   The search string that should trigger one of the mock prefix-test quick
 *   suggest results.
 * @param {object} expectedQuickSuggestResult
 *   The expected quick suggest result.
 * @param {string} otherPrefix
 *   The visit will be created with a URL with this prefix, e.g., "http://".
 * @param {boolean} expectOther
 *   Whether the visit result should appear in the final results.
 */
async function doDedupeAgainstURLTest({
  searchString,
  expectedQuickSuggestResult,
  otherPrefix,
  expectOther,
}) {
  // Disable search suggestions.
  UrlbarPrefs.set("suggest.searches", false);

  // Add a visit that will match our query below.
  let otherURL = otherPrefix + PREFIX_SUGGESTIONS_STRIPPED_URL;
  await PlacesTestUtils.addVisits({ uri: otherURL, title: searchString });

  // First, do a search with quick suggest disabled to make sure the search
  // string matches the visit.
  info("Doing first query");
  UrlbarPrefs.set("suggest.quicksuggest.nonsponsored", false);
  UrlbarPrefs.set("suggest.quicksuggest.sponsored", false);
  let context = createContext(searchString, { isPrivate: false });
  await check_results({
    context,
    matches: [
      makeSearchResult(context, {
        heuristic: true,
        query: searchString,
        engineName: Services.search.defaultEngine.name,
      }),
      makeVisitResult(context, {
        uri: otherURL,
        title: searchString,
      }),
    ],
  });

  // Now do another search with quick suggest enabled.
  UrlbarPrefs.set("suggest.quicksuggest.nonsponsored", true);
  UrlbarPrefs.set("suggest.quicksuggest.sponsored", true);

  context = createContext(searchString, { isPrivate: false });

  let expectedResults = [
    makeSearchResult(context, {
      heuristic: true,
      query: searchString,
      engineName: Services.search.defaultEngine.name,
    }),
  ];
  if (expectOther) {
    expectedResults.push(
      makeVisitResult(context, {
        uri: otherURL,
        title: searchString,
      })
    );
  }
  expectedResults.push(expectedQuickSuggestResult);

  info("Doing second query");
  await check_results({ context, matches: expectedResults });

  UrlbarPrefs.clear("suggest.quicksuggest.nonsponsored");
  UrlbarPrefs.clear("suggest.quicksuggest.sponsored");
  UrlbarPrefs.clear("suggest.searches");
  await PlacesUtils.history.clear();
}

// Tests the remote settings latency histogram.
add_task(async function latencyTelemetry() {
  UrlbarPrefs.set("suggest.quicksuggest.nonsponsored", true);
  UrlbarPrefs.set("suggest.quicksuggest.sponsored", true);

  let histogram = Services.telemetry.getHistogramById(
    TELEMETRY_REMOTE_SETTINGS_LATENCY
  );
  histogram.clear();

  let context = createContext(SPONSORED_SEARCH_STRING, {
    providers: [UrlbarProviderQuickSuggest.name],
    isPrivate: false,
  });
  await check_results({
    context,
    matches: [EXPECTED_SPONSORED_RESULT],
  });

  // In the latency histogram, there should be a single value across all
  // buckets.
  Assert.deepEqual(
    Object.values(histogram.snapshot().values).filter(v => v > 0),
    [1],
    "Latency histogram updated after search"
  );
  Assert.ok(
    !TelemetryStopwatch.running(TELEMETRY_REMOTE_SETTINGS_LATENCY, context),
    "Stopwatch not running after search"
  );
});

// Tests setup and teardown of the remote settings client depending on whether
// quick suggest is enabled.
add_task(async function setupAndTeardown() {
  // Disable the suggest prefs so the settings client starts out torn down.
  UrlbarPrefs.set("suggest.quicksuggest.nonsponsored", false);
  UrlbarPrefs.set("suggest.quicksuggest.sponsored", false);
  await UrlbarQuickSuggest.readyPromise;
  Assert.ok(
    !UrlbarQuickSuggest._rs,
    "Settings client is null after disabling suggest prefs"
  );

  // Setting one of the suggest prefs should cause the client to be set up. We
  // assume all previous tasks left `quicksuggest.enabled` true (from the init
  // task).
  UrlbarPrefs.set("suggest.quicksuggest.nonsponsored", true);
  await UrlbarQuickSuggest.readyPromise;
  Assert.ok(
    UrlbarQuickSuggest._rs,
    "Settings client is non-null after enabling suggest.quicksuggest.nonsponsored"
  );

  UrlbarPrefs.set("suggest.quicksuggest.nonsponsored", false);
  await UrlbarQuickSuggest.readyPromise;
  Assert.ok(
    !UrlbarQuickSuggest._rs,
    "Settings client is null after disabling suggest.quicksuggest.nonsponsored"
  );

  UrlbarPrefs.set("suggest.quicksuggest.sponsored", true);
  await UrlbarQuickSuggest.readyPromise;
  Assert.ok(
    UrlbarQuickSuggest._rs,
    "Settings client is non-null after enabling suggest.quicksuggest.sponsored"
  );

  UrlbarPrefs.set("suggest.quicksuggest.nonsponsored", true);
  await UrlbarQuickSuggest.readyPromise;
  Assert.ok(
    UrlbarQuickSuggest._rs,
    "Settings client remains non-null after enabling suggest.quicksuggest.nonsponsored"
  );

  UrlbarPrefs.set("suggest.quicksuggest.nonsponsored", false);
  await UrlbarQuickSuggest.readyPromise;
  Assert.ok(
    UrlbarQuickSuggest._rs,
    "Settings client remains non-null after disabling suggest.quicksuggest.nonsponsored"
  );

  UrlbarPrefs.set("suggest.quicksuggest.sponsored", false);
  await UrlbarQuickSuggest.readyPromise;
  Assert.ok(
    !UrlbarQuickSuggest._rs,
    "Settings client is null after disabling suggest.quicksuggest.sponsored"
  );

  UrlbarPrefs.set("suggest.quicksuggest.nonsponsored", true);
  await UrlbarQuickSuggest.readyPromise;
  Assert.ok(
    UrlbarQuickSuggest._rs,
    "Settings client is non-null after enabling suggest.quicksuggest.nonsponsored"
  );

  UrlbarPrefs.set("quicksuggest.enabled", false);
  await UrlbarQuickSuggest.readyPromise;
  Assert.ok(
    !UrlbarQuickSuggest._rs,
    "Settings client is null after disabling quicksuggest.enabled"
  );

  // Leave the prefs in the same state as when the task started.
  UrlbarPrefs.clear("suggest.quicksuggest.nonsponsored");
  UrlbarPrefs.clear("suggest.quicksuggest.sponsored");
  UrlbarPrefs.set("quicksuggest.enabled", true);
  await UrlbarQuickSuggest.readyPromise;
  Assert.ok(
    !UrlbarQuickSuggest._rs,
    "Settings client remains null at end of task"
  );
});

// Timestamp templates in URLs should be replaced with real timestamps.
add_task(async function timestamps() {
  UrlbarPrefs.set("suggest.quicksuggest.nonsponsored", true);
  UrlbarPrefs.set("suggest.quicksuggest.sponsored", true);

  // Do a search.
  let context = createContext(TIMESTAMP_SEARCH_STRING, {
    providers: [UrlbarProviderQuickSuggest.name],
    isPrivate: false,
  });
  let controller = UrlbarTestUtils.newMockController({
    input: {
      isPrivate: context.isPrivate,
      onFirstResult() {
        return false;
      },
      window: {
        location: {
          href: AppConstants.BROWSER_CHROME_URL,
        },
      },
    },
  });
  await controller.startQuery(context);

  // Should be one quick suggest result.
  Assert.equal(context.results.length, 1, "One result returned");
  let result = context.results[0];

  QuickSuggestTestUtils.assertTimestampsReplaced(result, {
    url: TIMESTAMP_SUGGESTION_URL,
    sponsoredClickUrl: TIMESTAMP_SUGGESTION_CLICK_URL,
  });
});

// Real quick suggest URLs include a timestamp template that
// UrlbarProviderQuickSuggest fills in when it fetches suggestions. When the
// user picks a quick suggest, its URL with its particular timestamp is added to
// history. If the user triggers the quick suggest again later, its new
// timestamp may be different from the one in the user's history. In that case,
// the two URLs should be treated as dupes and only the quick suggest should be
// shown, not the URL from history.
add_task(async function dedupeAgainstURL_timestamps() {
  // Disable search suggestions.
  UrlbarPrefs.set("suggest.searches", false);

  // Add a visit that will match the query below and dupe the quick suggest.
  let dupeURL = TIMESTAMP_SUGGESTION_URL.replace(
    TIMESTAMP_TEMPLATE,
    "2013051113"
  );

  // Add other visits that will match the query and almost dupe the quick
  // suggest but not quite because they have invalid timestamps.
  let badTimestamps = [
    // not numeric digits
    "x".repeat(TIMESTAMP_LENGTH),
    // too few digits
    "5".repeat(TIMESTAMP_LENGTH - 1),
    // empty string, too few digits
    "",
  ];
  let badTimestampURLs = badTimestamps.map(str =>
    TIMESTAMP_SUGGESTION_URL.replace(TIMESTAMP_TEMPLATE, str)
  );

  await PlacesTestUtils.addVisits(
    [dupeURL, ...badTimestampURLs].map(uri => ({
      uri,
      title: TIMESTAMP_SEARCH_STRING,
    }))
  );

  // First, do a search with quick suggest disabled to make sure the search
  // string matches all the other URLs.
  info("Doing first query");
  UrlbarPrefs.set("suggest.quicksuggest.nonsponsored", false);
  UrlbarPrefs.set("suggest.quicksuggest.sponsored", false);
  let context = createContext(TIMESTAMP_SEARCH_STRING, { isPrivate: false });

  let expectedHeuristic = makeSearchResult(context, {
    heuristic: true,
    query: TIMESTAMP_SEARCH_STRING,
    engineName: Services.search.defaultEngine.name,
  });
  let expectedDupeResult = makeVisitResult(context, {
    uri: dupeURL,
    title: TIMESTAMP_SEARCH_STRING,
  });
  let expectedBadTimestampResults = [...badTimestampURLs].reverse().map(uri =>
    makeVisitResult(context, {
      uri,
      title: TIMESTAMP_SEARCH_STRING,
    })
  );

  await check_results({
    context,
    matches: [
      expectedHeuristic,
      ...expectedBadTimestampResults,
      expectedDupeResult,
    ],
  });

  // Now do another search with quick suggest enabled.
  info("Doing second query");
  UrlbarPrefs.set("suggest.quicksuggest.nonsponsored", true);
  UrlbarPrefs.set("suggest.quicksuggest.sponsored", true);
  context = createContext(TIMESTAMP_SEARCH_STRING, { isPrivate: false });

  // The expected quick suggest result without the timestamp-related payload
  // properties.
  let expectedQuickSuggest = {
    type: UrlbarUtils.RESULT_TYPE.URL,
    source: UrlbarUtils.RESULT_SOURCE.SEARCH,
    heuristic: false,
    payload: {
      originalUrl: TIMESTAMP_SUGGESTION_URL,
      qsSuggestion: TIMESTAMP_SEARCH_STRING,
      title: "Timestamp suggestion",
      icon: null,
      sponsoredImpressionUrl: "http://impression.reporting.test.com/timestamp",
      sponsoredBlockId: 5,
      sponsoredAdvertiser: "TestAdvertiserTimestamp",
      sponsoredIabCategory: "22 - Shopping",
      isSponsored: true,
      helpUrl: UrlbarProviderQuickSuggest.helpUrl,
      helpL10nId: "firefox-suggest-urlbar-learn-more",
      source: "remote-settings",
    },
  };

  let expectedResults = [
    expectedHeuristic,
    ...expectedBadTimestampResults,
    expectedQuickSuggest,
  ];

  let controller = UrlbarTestUtils.newMockController({
    input: {
      isPrivate: false,
      onFirstResult() {
        return false;
      },
      window: {
        location: {
          href: AppConstants.BROWSER_CHROME_URL,
        },
      },
    },
  });
  await controller.startQuery(context);
  info("Actual results: " + JSON.stringify(context.results));

  Assert.equal(
    context.results.length,
    expectedResults.length,
    "Found the expected number of results"
  );

  function getPayload(result, keysToIgnore = []) {
    let payload = {};
    for (let [key, value] of Object.entries(result.payload)) {
      if (value !== undefined && !keysToIgnore.includes(key)) {
        payload[key] = value;
      }
    }
    return payload;
  }

  // Check actual vs. expected result properties.
  for (let i = 0; i < expectedResults.length; i++) {
    let actual = context.results[i];
    let expected = expectedResults[i];
    info(
      `Comparing results at index ${i}:` +
        " actual=" +
        JSON.stringify(actual) +
        " expected=" +
        JSON.stringify(expected)
    );
    Assert.equal(
      actual.type,
      expected.type,
      `result.type at result index ${i}`
    );
    Assert.equal(
      actual.source,
      expected.source,
      `result.source at result index ${i}`
    );
    Assert.equal(
      actual.heuristic,
      expected.heuristic,
      `result.heuristic at result index ${i}`
    );

    // Check payloads except for the last result, which should be the quick
    // suggest.
    if (i != expectedResults.length - 1) {
      Assert.deepEqual(
        getPayload(context.results[i]),
        getPayload(expectedResults[i]),
        "Payload at index " + i
      );
    }
  }

  // Check the quick suggest's payload excluding the timestamp-related
  // properties.
  let actualQuickSuggest = context.results[context.results.length - 1];
  let timestampKeys = [
    "displayUrl",
    "sponsoredClickUrl",
    "url",
    "urlTimestampIndex",
  ];
  Assert.deepEqual(
    getPayload(actualQuickSuggest, timestampKeys),
    getPayload(expectedQuickSuggest, timestampKeys),
    "Quick suggest payload excluding timestamp-related keys"
  );

  // Now check the timestamps in the payload.
  QuickSuggestTestUtils.assertTimestampsReplaced(actualQuickSuggest, {
    url: TIMESTAMP_SUGGESTION_URL,
    sponsoredClickUrl: TIMESTAMP_SUGGESTION_CLICK_URL,
  });

  // Clean up.
  UrlbarPrefs.clear("suggest.quicksuggest.nonsponsored");
  UrlbarPrefs.clear("suggest.quicksuggest.sponsored");
  UrlbarPrefs.clear("suggest.searches");
  await PlacesUtils.history.clear();
});

// Tests the API for blocking suggestions and the backing pref.
add_task(async function blockedSuggestionsAPI() {
  // Start with no blocked suggestions.
  await UrlbarProviderQuickSuggest.clearBlockedSuggestions();
  Assert.equal(
    UrlbarProviderQuickSuggest._blockedDigests.size,
    0,
    "_blockedDigests is empty"
  );
  Assert.equal(
    UrlbarPrefs.get("quicksuggest.blockedDigests"),
    "",
    "quicksuggest.blockedDigests is an empty string"
  );

  // Make some URLs.
  let urls = [];
  for (let i = 0; i < 3; i++) {
    urls.push("http://example.com/" + i);
  }

  // Block each URL in turn and make sure previously blocked URLs are still
  // blocked and the remaining URLs are not blocked.
  for (let i = 0; i < urls.length; i++) {
    await UrlbarProviderQuickSuggest.blockSuggestion(urls[i]);
    for (let j = 0; j < urls.length; j++) {
      Assert.equal(
        await UrlbarProviderQuickSuggest.isSuggestionBlocked(urls[j]),
        j <= i,
        `Suggestion at index ${j} is blocked or not as expected`
      );
    }
  }

  // Make sure all URLs are blocked for good measure.
  for (let url of urls) {
    Assert.ok(
      await UrlbarProviderQuickSuggest.isSuggestionBlocked(url),
      `Suggestion is blocked: ${url}`
    );
  }

  // Check `_blockedDigests` and `quicksuggest.blockedDigests`.
  Assert.equal(
    UrlbarProviderQuickSuggest._blockedDigests.size,
    urls.length,
    "_blockedDigests has correct size"
  );
  let array = JSON.parse(UrlbarPrefs.get("quicksuggest.blockedDigests"));
  Assert.ok(Array.isArray(array), "Parsed value of pref is an array");
  Assert.equal(array.length, urls.length, "Array has correct length");

  // Write some junk to `quicksuggest.blockedDigests`. `_blockedDigests` should
  // not be changed and all previously blocked URLs should remain blocked.
  UrlbarPrefs.set("quicksuggest.blockedDigests", "not a json array");
  await UrlbarProviderQuickSuggest._blockTaskQueue.emptyPromise;
  for (let url of urls) {
    Assert.ok(
      await UrlbarProviderQuickSuggest.isSuggestionBlocked(url),
      `Suggestion remains blocked: ${url}`
    );
  }
  Assert.equal(
    UrlbarProviderQuickSuggest._blockedDigests.size,
    urls.length,
    "_blockedDigests still has correct size"
  );

  // Block a new URL. All URLs should remain blocked and the pref should be
  // updated.
  let newURL = "http://example.com/new-block";
  await UrlbarProviderQuickSuggest.blockSuggestion(newURL);
  urls.push(newURL);
  for (let url of urls) {
    Assert.ok(
      await UrlbarProviderQuickSuggest.isSuggestionBlocked(url),
      `Suggestion is blocked: ${url}`
    );
  }
  Assert.equal(
    UrlbarProviderQuickSuggest._blockedDigests.size,
    urls.length,
    "_blockedDigests has correct size"
  );
  array = JSON.parse(UrlbarPrefs.get("quicksuggest.blockedDigests"));
  Assert.ok(Array.isArray(array), "Parsed value of pref is an array");
  Assert.equal(array.length, urls.length, "Array has correct length");

  // Add a new URL digest directly to the JSON'ed array in the pref.
  newURL = "http://example.com/direct-to-pref";
  urls.push(newURL);
  array = JSON.parse(UrlbarPrefs.get("quicksuggest.blockedDigests"));
  array.push(await UrlbarProviderQuickSuggest._getDigest(newURL));
  UrlbarPrefs.set("quicksuggest.blockedDigests", JSON.stringify(array));
  await UrlbarProviderQuickSuggest._blockTaskQueue.emptyPromise;

  // All URLs should remain blocked and the new URL should be blocked.
  for (let url of urls) {
    Assert.ok(
      await UrlbarProviderQuickSuggest.isSuggestionBlocked(url),
      `Suggestion is blocked: ${url}`
    );
  }
  Assert.equal(
    UrlbarProviderQuickSuggest._blockedDigests.size,
    urls.length,
    "_blockedDigests has correct size"
  );

  // Clear the pref. All URLs should be unblocked.
  UrlbarPrefs.clear("quicksuggest.blockedDigests");
  await UrlbarProviderQuickSuggest._blockTaskQueue.emptyPromise;
  for (let url of urls) {
    Assert.ok(
      !(await UrlbarProviderQuickSuggest.isSuggestionBlocked(url)),
      `Suggestion is no longer blocked: ${url}`
    );
  }
  Assert.equal(
    UrlbarProviderQuickSuggest._blockedDigests.size,
    0,
    "_blockedDigests is now empty"
  );

  // Block all the URLs again and test `clearBlockedSuggestions()`.
  for (let url of urls) {
    await UrlbarProviderQuickSuggest.blockSuggestion(url);
  }
  for (let url of urls) {
    Assert.ok(
      await UrlbarProviderQuickSuggest.isSuggestionBlocked(url),
      `Suggestion is blocked: ${url}`
    );
  }
  await UrlbarProviderQuickSuggest.clearBlockedSuggestions();
  for (let url of urls) {
    Assert.ok(
      !(await UrlbarProviderQuickSuggest.isSuggestionBlocked(url)),
      `Suggestion is no longer blocked: ${url}`
    );
  }
  Assert.equal(
    UrlbarProviderQuickSuggest._blockedDigests.size,
    0,
    "_blockedDigests is now empty"
  );
});

// Test whether the blocking for remote settings results works.
add_task(async function block() {
  for (const suggestion of REMOTE_SETTINGS_DATA) {
    await UrlbarProviderQuickSuggest.blockSuggestion(suggestion.url);
  }

  for (const suggestion of REMOTE_SETTINGS_DATA) {
    const context = createContext(suggestion.keywords[0], {
      providers: [UrlbarProviderQuickSuggest.name],
      isPrivate: false,
    });
    await check_results({
      context,
      matches: [],
    });
  }

  await UrlbarProviderQuickSuggest.clearBlockedSuggestions();
});

// Makes sure remote settings data is fetched using the correct `type` based on
// the value of the `quickSuggestRemoteSettingsDataType` Nimbus variable.
add_task(async function remoteSettingsDataType() {
  // `QuickSuggestTestUtils.ensureQuickSuggestInit()` stubs
  // `UrlbarQuickSuggest._queueSettingsSync()`, which we want to test below, so
  // remove the stub by calling the cleanup function it returned.
  await cleanUpQuickSuggest();

  // We need to spy on `UrlbarQuickSuggest._rs.get()`, but `_rs` is created
  // lazily. Set `suggest.quicksuggest.sponsored` to trigger its creation.
  UrlbarPrefs.set("suggest.quicksuggest.sponsored", true);
  await UrlbarQuickSuggest.readyPromise;

  let sandbox = sinon.createSandbox();
  let spy = sandbox.spy(UrlbarQuickSuggest._rs, "get");

  for (let dataType of [undefined, "test-data-type"]) {
    // Set up a mock Nimbus rollout with the data type.
    let value = {};
    if (dataType) {
      value.quickSuggestRemoteSettingsDataType = dataType;
    }
    let cleanUpNimbus = await QuickSuggestTestUtils.initNimbusFeature(value);

    // This call will trigger `UrlbarQuickSuggest._rs.get()`.
    await UrlbarQuickSuggest._queueSettingsSync();

    let expectedDataType = dataType || "data";
    Assert.ok(
      spy.calledWith({ filters: { type: expectedDataType } }),
      "_rs.get() called with expected data type: " + expectedDataType
    );

    spy.resetHistory();
    await cleanUpNimbus();
  }

  sandbox.restore();

  // Restore the stub for the remainder of the test.
  cleanUpQuickSuggest = await QuickSuggestTestUtils.ensureQuickSuggestInit(
    REMOTE_SETTINGS_DATA
  );
});
