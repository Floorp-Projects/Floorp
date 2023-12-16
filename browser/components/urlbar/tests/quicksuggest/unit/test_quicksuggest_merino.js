/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

// Tests Merino integration with UrlbarProviderQuickSuggest.

"use strict";

// relative to `browser.urlbar`
const PREF_DATA_COLLECTION_ENABLED = "quicksuggest.dataCollection.enabled";

const SEARCH_STRING = "frab";

const { DEFAULT_SUGGESTION_SCORE } = UrlbarProviderQuickSuggest;

const REMOTE_SETTINGS_RESULTS = [
  {
    id: 1,
    url: "http://test.com/q=frabbits",
    title: "frabbits",
    keywords: [SEARCH_STRING],
    click_url: "http://click.reporting.test.com/",
    impression_url: "http://impression.reporting.test.com/",
    advertiser: "TestAdvertiser",
  },
];

const EXPECTED_REMOTE_SETTINGS_URLBAR_RESULT = {
  type: UrlbarUtils.RESULT_TYPE.URL,
  source: UrlbarUtils.RESULT_SOURCE.SEARCH,
  heuristic: false,
  payload: {
    telemetryType: "adm_sponsored",
    qsSuggestion: SEARCH_STRING,
    title: "frabbits",
    url: "http://test.com/q=frabbits",
    originalUrl: "http://test.com/q=frabbits",
    icon: null,
    sponsoredImpressionUrl: "http://impression.reporting.test.com/",
    sponsoredClickUrl: "http://click.reporting.test.com/",
    sponsoredBlockId: 1,
    sponsoredAdvertiser: "TestAdvertiser",
    isSponsored: true,
    descriptionL10n: { id: "urlbar-result-action-sponsored" },
    helpUrl: QuickSuggest.HELP_URL,
    helpL10n: {
      id: "urlbar-result-menu-learn-more-about-firefox-suggest",
    },
    isBlockable: true,
    blockL10n: {
      id: "urlbar-result-menu-dismiss-firefox-suggest",
    },
    displayUrl: "http://test.com/q=frabbits",
    source: "remote-settings",
    provider: "AdmWikipedia",
  },
};

const EXPECTED_MERINO_URLBAR_RESULT = {
  type: UrlbarUtils.RESULT_TYPE.URL,
  source: UrlbarUtils.RESULT_SOURCE.SEARCH,
  heuristic: false,
  payload: {
    telemetryType: "adm_sponsored",
    qsSuggestion: "full_keyword",
    title: "title",
    url: "http://example.com/amp",
    originalUrl: "http://example.com/amp",
    icon: null,
    sponsoredImpressionUrl: "http://example.com/amp-impression",
    sponsoredClickUrl: "http://example.com/amp-click",
    sponsoredBlockId: 1,
    sponsoredAdvertiser: "amp",
    isSponsored: true,
    descriptionL10n: { id: "urlbar-result-action-sponsored" },
    helpUrl: QuickSuggest.HELP_URL,
    helpL10n: {
      id: "urlbar-result-menu-learn-more-about-firefox-suggest",
    },
    isBlockable: true,
    blockL10n: {
      id: "urlbar-result-menu-dismiss-firefox-suggest",
    },
    displayUrl: "http://example.com/amp",
    requestId: "request_id",
    source: "merino",
    provider: "adm",
  },
};

// `UrlbarProviderQuickSuggest.#merino` is lazily created on the first Merino
// fetch, so it's easiest to create `gClient` lazily too.
ChromeUtils.defineLazyGetter(
  this,
  "gClient",
  () => UrlbarProviderQuickSuggest._test_merino
);

add_setup(async () => {
  await MerinoTestUtils.server.start();

  // Set up the remote settings client with the test data.
  await QuickSuggestTestUtils.ensureQuickSuggestInit({
    remoteSettingsRecords: [
      {
        type: "data",
        attachment: REMOTE_SETTINGS_RESULTS,
      },
    ],
    prefs: [
      ["suggest.quicksuggest.nonsponsored", true],
      ["suggest.quicksuggest.sponsored", true],
    ],
  });

  Assert.equal(
    typeof DEFAULT_SUGGESTION_SCORE,
    "number",
    "Sanity check: DEFAULT_SUGGESTION_SCORE is defined"
  );
});

// Tests with the Merino endpoint URL set to an empty string, which disables
// fetching from Merino.
add_task(async function merinoDisabled() {
  let mockEndpointUrl = UrlbarPrefs.get("merino.endpointURL");
  UrlbarPrefs.set("merino.endpointURL", "");
  UrlbarPrefs.set(PREF_DATA_COLLECTION_ENABLED, true);

  // Clear the remote settings suggestions so that if Merino is actually queried
  // -- which would be a bug -- we don't accidentally mask the Merino suggestion
  // by also matching an RS suggestion with the same or higher score.
  await QuickSuggestTestUtils.setRemoteSettingsRecords([]);

  let histograms = MerinoTestUtils.getAndClearHistograms();

  let context = createContext(SEARCH_STRING, {
    providers: [UrlbarProviderQuickSuggest.name],
    isPrivate: false,
  });
  await check_results({
    context,
    matches: [],
  });

  MerinoTestUtils.checkAndClearHistograms({
    histograms,
    response: null,
    latencyRecorded: false,
    client: UrlbarProviderQuickSuggest._test_merino,
  });

  UrlbarPrefs.set("merino.endpointURL", mockEndpointUrl);

  await QuickSuggestTestUtils.setRemoteSettingsRecords([
    {
      type: "data",
      attachment: REMOTE_SETTINGS_RESULTS,
    },
  ]);
});

// Tests with Merino enabled but with data collection disabled. Results should
// not be fetched from Merino in that case.
add_task(async function dataCollectionDisabled() {
  UrlbarPrefs.set(PREF_DATA_COLLECTION_ENABLED, false);

  // Clear the remote settings suggestions so that if Merino is actually queried
  // -- which would be a bug -- we don't accidentally mask the Merino suggestion
  // by also matching an RS suggestion with the same or higher score.
  await QuickSuggestTestUtils.setRemoteSettingsRecords([]);

  let context = createContext(SEARCH_STRING, {
    providers: [UrlbarProviderQuickSuggest.name],
    isPrivate: false,
  });
  await check_results({
    context,
    matches: [],
  });

  await QuickSuggestTestUtils.setRemoteSettingsRecords([
    {
      type: "data",
      attachment: REMOTE_SETTINGS_RESULTS,
    },
  ]);
});

// When the Merino suggestion has a higher score than the remote settings
// suggestion, the Merino suggestion should be used.
add_task(async function higherScore() {
  UrlbarPrefs.set(PREF_DATA_COLLECTION_ENABLED, true);

  let histograms = MerinoTestUtils.getAndClearHistograms();

  MerinoTestUtils.server.response.body.suggestions[0].score =
    2 * DEFAULT_SUGGESTION_SCORE;

  let context = createContext(SEARCH_STRING, {
    providers: [UrlbarProviderQuickSuggest.name],
    isPrivate: false,
  });
  await check_results({
    context,
    matches: [EXPECTED_MERINO_URLBAR_RESULT],
  });

  MerinoTestUtils.checkAndClearHistograms({
    histograms,
    response: "success",
    latencyRecorded: true,
    client: gClient,
  });

  MerinoTestUtils.server.reset();
  gClient.resetSession();
});

// When the Merino suggestion has a lower score than the remote settings
// suggestion, the remote settings suggestion should be used.
add_task(async function lowerScore() {
  UrlbarPrefs.set(PREF_DATA_COLLECTION_ENABLED, true);

  let histograms = MerinoTestUtils.getAndClearHistograms();

  MerinoTestUtils.server.response.body.suggestions[0].score =
    DEFAULT_SUGGESTION_SCORE / 2;

  let context = createContext(SEARCH_STRING, {
    providers: [UrlbarProviderQuickSuggest.name],
    isPrivate: false,
  });
  await check_results({
    context,
    matches: [EXPECTED_REMOTE_SETTINGS_URLBAR_RESULT],
  });

  MerinoTestUtils.checkAndClearHistograms({
    histograms,
    response: "success",
    latencyRecorded: true,
    client: gClient,
  });

  MerinoTestUtils.server.reset();
  gClient.resetSession();
});

// When the Merino and remote settings suggestions have the same score, the
// remote settings suggestion should be used.
add_task(async function sameScore() {
  UrlbarPrefs.set(PREF_DATA_COLLECTION_ENABLED, true);

  let histograms = MerinoTestUtils.getAndClearHistograms();

  MerinoTestUtils.server.response.body.suggestions[0].score =
    DEFAULT_SUGGESTION_SCORE;

  let context = createContext(SEARCH_STRING, {
    providers: [UrlbarProviderQuickSuggest.name],
    isPrivate: false,
  });
  await check_results({
    context,
    matches: [EXPECTED_REMOTE_SETTINGS_URLBAR_RESULT],
  });

  MerinoTestUtils.checkAndClearHistograms({
    histograms,
    response: "success",
    latencyRecorded: true,
    client: gClient,
  });

  MerinoTestUtils.server.reset();
  gClient.resetSession();
});

// When the Merino suggestion does not include a score, the remote settings
// suggestion should be used.
add_task(async function noMerinoScore() {
  UrlbarPrefs.set(PREF_DATA_COLLECTION_ENABLED, true);

  let histograms = MerinoTestUtils.getAndClearHistograms();

  Assert.equal(
    typeof MerinoTestUtils.server.response.body.suggestions[0].score,
    "number",
    "Sanity check: First suggestion has a score"
  );
  delete MerinoTestUtils.server.response.body.suggestions[0].score;

  let context = createContext(SEARCH_STRING, {
    providers: [UrlbarProviderQuickSuggest.name],
    isPrivate: false,
  });
  await check_results({
    context,
    matches: [EXPECTED_REMOTE_SETTINGS_URLBAR_RESULT],
  });

  MerinoTestUtils.checkAndClearHistograms({
    histograms,
    response: "success",
    latencyRecorded: true,
    client: gClient,
  });

  MerinoTestUtils.server.reset();
  gClient.resetSession();
});

// When remote settings doesn't return a suggestion but Merino does, the Merino
// suggestion should be used.
add_task(async function noSuggestion_remoteSettings() {
  UrlbarPrefs.set(PREF_DATA_COLLECTION_ENABLED, true);

  let histograms = MerinoTestUtils.getAndClearHistograms();

  let context = createContext("this doesn't match remote settings", {
    providers: [UrlbarProviderQuickSuggest.name],
    isPrivate: false,
  });
  await check_results({
    context,
    matches: [EXPECTED_MERINO_URLBAR_RESULT],
  });

  MerinoTestUtils.checkAndClearHistograms({
    histograms,
    response: "success",
    latencyRecorded: true,
    client: gClient,
  });

  MerinoTestUtils.server.reset();
  gClient.resetSession();
});

// When Merino doesn't return a suggestion but remote settings does, the remote
// settings suggestion should be used.
add_task(async function noSuggestion_merino() {
  UrlbarPrefs.set(PREF_DATA_COLLECTION_ENABLED, true);

  let histograms = MerinoTestUtils.getAndClearHistograms();

  MerinoTestUtils.server.response.body.suggestions = [];

  let context = createContext(SEARCH_STRING, {
    providers: [UrlbarProviderQuickSuggest.name],
    isPrivate: false,
  });
  await check_results({
    context,
    matches: [EXPECTED_REMOTE_SETTINGS_URLBAR_RESULT],
  });

  MerinoTestUtils.checkAndClearHistograms({
    histograms,
    response: "no_suggestion",
    latencyRecorded: true,
    client: gClient,
  });

  MerinoTestUtils.server.reset();
  gClient.resetSession();
});

// When Merino returns multiple suggestions, the one with the largest score
// should be used.
add_task(async function multipleMerinoSuggestions() {
  UrlbarPrefs.set(PREF_DATA_COLLECTION_ENABLED, true);

  let histograms = MerinoTestUtils.getAndClearHistograms();

  MerinoTestUtils.server.response.body.suggestions = [
    {
      provider: "adm",
      full_keyword: "multipleMerinoSuggestions 0 full_keyword",
      title: "multipleMerinoSuggestions 0 title",
      url: "multipleMerinoSuggestions 0 url",
      icon: "multipleMerinoSuggestions 0 icon",
      impression_url: "multipleMerinoSuggestions 0 impression_url",
      click_url: "multipleMerinoSuggestions 0 click_url",
      block_id: 0,
      advertiser: "multipleMerinoSuggestions 0 advertiser",
      is_sponsored: true,
      score: 0.1,
    },
    {
      provider: "adm",
      full_keyword: "multipleMerinoSuggestions 1 full_keyword",
      title: "multipleMerinoSuggestions 1 title",
      url: "multipleMerinoSuggestions 1 url",
      icon: "multipleMerinoSuggestions 1 icon",
      impression_url: "multipleMerinoSuggestions 1 impression_url",
      click_url: "multipleMerinoSuggestions 1 click_url",
      block_id: 1,
      advertiser: "multipleMerinoSuggestions 1 advertiser",
      is_sponsored: true,
      score: 1,
    },
    {
      provider: "adm",
      full_keyword: "multipleMerinoSuggestions 2 full_keyword",
      title: "multipleMerinoSuggestions 2 title",
      url: "multipleMerinoSuggestions 2 url",
      icon: "multipleMerinoSuggestions 2 icon",
      impression_url: "multipleMerinoSuggestions 2 impression_url",
      click_url: "multipleMerinoSuggestions 2 click_url",
      block_id: 2,
      advertiser: "multipleMerinoSuggestions 2 advertiser",
      is_sponsored: true,
      score: 0.2,
    },
  ];

  let context = createContext("test", {
    providers: [UrlbarProviderQuickSuggest.name],
    isPrivate: false,
  });
  await check_results({
    context,
    matches: [
      {
        type: UrlbarUtils.RESULT_TYPE.URL,
        source: UrlbarUtils.RESULT_SOURCE.SEARCH,
        heuristic: false,
        payload: {
          telemetryType: "adm_sponsored",
          qsSuggestion: "multipleMerinoSuggestions 1 full_keyword",
          title: "multipleMerinoSuggestions 1 title",
          url: "multipleMerinoSuggestions 1 url",
          originalUrl: "multipleMerinoSuggestions 1 url",
          icon: "multipleMerinoSuggestions 1 icon",
          sponsoredImpressionUrl: "multipleMerinoSuggestions 1 impression_url",
          sponsoredClickUrl: "multipleMerinoSuggestions 1 click_url",
          sponsoredBlockId: 1,
          sponsoredAdvertiser: "multipleMerinoSuggestions 1 advertiser",
          isSponsored: true,
          descriptionL10n: { id: "urlbar-result-action-sponsored" },
          helpUrl: QuickSuggest.HELP_URL,
          helpL10n: {
            id: "urlbar-result-menu-learn-more-about-firefox-suggest",
          },
          isBlockable: true,
          blockL10n: {
            id: "urlbar-result-menu-dismiss-firefox-suggest",
          },
          displayUrl: "multipleMerinoSuggestions 1 url",
          requestId: "request_id",
          source: "merino",
          provider: "adm",
        },
      },
    ],
  });

  MerinoTestUtils.checkAndClearHistograms({
    histograms,
    response: "success",
    latencyRecorded: true,
    client: gClient,
  });

  MerinoTestUtils.server.reset();
  gClient.resetSession();
});

// Timestamp templates in URLs should be replaced with real timestamps.
add_task(async function timestamps() {
  UrlbarPrefs.set(PREF_DATA_COLLECTION_ENABLED, true);

  // Set up the Merino response with template URLs.
  let suggestion = MerinoTestUtils.server.response.body.suggestions[0];
  let { TIMESTAMP_TEMPLATE } = QuickSuggest;

  suggestion.url = `http://example.com/time-${TIMESTAMP_TEMPLATE}`;
  suggestion.click_url = `http://example.com/time-${TIMESTAMP_TEMPLATE}-foo`;

  // Do a search.
  let context = createContext("test", {
    providers: [UrlbarProviderQuickSuggest.name],
    isPrivate: false,
  });
  let controller = UrlbarTestUtils.newMockController({
    input: {
      isPrivate: context.isPrivate,
      onFirstResult() {
        return false;
      },
      getSearchSource() {
        return "dummy-search-source";
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
    url: suggestion.click_url,
    sponsoredClickUrl: suggestion.click_url,
  });

  MerinoTestUtils.server.reset();
  gClient.resetSession();
});

// When both suggestion types are disabled but data collection is enabled, we
// should still send requests to Merino, and the requests should include an
// empty `providers` to tell Merino not to fetch any suggestions.
add_task(async function suggestedDisabled_dataCollectionEnabled() {
  UrlbarPrefs.set(PREF_DATA_COLLECTION_ENABLED, true);
  UrlbarPrefs.set("suggest.quicksuggest.nonsponsored", false);
  UrlbarPrefs.set("suggest.quicksuggest.sponsored", false);

  let histograms = MerinoTestUtils.getAndClearHistograms();

  let context = createContext("test", {
    providers: [UrlbarProviderQuickSuggest.name],
    isPrivate: false,
  });
  await check_results({
    context,
    matches: [],
  });

  // Check that the request is received and includes an empty `providers`.
  MerinoTestUtils.server.checkAndClearRequests([
    {
      params: {
        [MerinoTestUtils.SEARCH_PARAMS.QUERY]: "test",
        [MerinoTestUtils.SEARCH_PARAMS.SEQUENCE_NUMBER]: 0,
        [MerinoTestUtils.SEARCH_PARAMS.PROVIDERS]: "",
      },
    },
  ]);

  MerinoTestUtils.checkAndClearHistograms({
    histograms,
    response: "success",
    latencyRecorded: true,
    client: gClient,
  });

  UrlbarPrefs.set("suggest.quicksuggest.nonsponsored", true);
  UrlbarPrefs.set("suggest.quicksuggest.sponsored", true);
  await QuickSuggestTestUtils.forceSync();
  gClient.resetSession();
});

// Test whether the blocking for Merino results works.
add_task(async function block() {
  UrlbarPrefs.set(PREF_DATA_COLLECTION_ENABLED, true);

  for (const suggestion of MerinoTestUtils.server.response.body.suggestions) {
    await QuickSuggest.blockedSuggestions.add(suggestion.url);
  }

  const context = createContext(SEARCH_STRING, {
    providers: [UrlbarProviderQuickSuggest.name],
    isPrivate: false,
  });

  await check_results({
    context,
    matches: [EXPECTED_REMOTE_SETTINGS_URLBAR_RESULT],
  });

  await QuickSuggest.blockedSuggestions.clear();
  MerinoTestUtils.server.reset();
  gClient.resetSession();
});

// Tests a Merino suggestion that is a top pick/best match.
add_task(async function bestMatch() {
  UrlbarPrefs.set(PREF_DATA_COLLECTION_ENABLED, true);

  // Set up a suggestion with `is_top_pick` and an unknown provider so that
  // UrlbarProviderQuickSuggest will make a default result for it.
  MerinoTestUtils.server.response.body.suggestions = [
    {
      is_top_pick: true,
      provider: "some_top_pick_provider",
      full_keyword: "full_keyword",
      title: "title",
      url: "url",
      icon: null,
      score: 1,
    },
  ];

  let context = createContext(SEARCH_STRING, {
    providers: [UrlbarProviderQuickSuggest.name],
    isPrivate: false,
  });
  await check_results({
    context,
    matches: [
      {
        isBestMatch: true,
        type: UrlbarUtils.RESULT_TYPE.URL,
        source: UrlbarUtils.RESULT_SOURCE.SEARCH,
        heuristic: false,
        payload: {
          telemetryType: "some_top_pick_provider",
          title: "title",
          url: "url",
          icon: null,
          qsSuggestion: "full_keyword",
          helpUrl: QuickSuggest.HELP_URL,
          helpL10n: {
            id: "urlbar-result-menu-learn-more-about-firefox-suggest",
          },
          isBlockable: true,
          blockL10n: {
            id: "urlbar-result-menu-dismiss-firefox-suggest",
          },
          displayUrl: "url",
          source: "merino",
          provider: "some_top_pick_provider",
        },
      },
    ],
  });

  // This isn't necessary since `check_results()` checks `isBestMatch`, but
  // check it here explicitly for good measure.
  Assert.ok(context.results[0].isBestMatch, "Result is a best match");

  MerinoTestUtils.server.reset();
  gClient.resetSession();
});
