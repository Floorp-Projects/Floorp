/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

// Tests Merino integration with the quick suggest feature/provider.

"use strict";

XPCOMUtils.defineLazyModuleGetters(this, {
  TelemetryTestUtils: "resource://testing-common/TelemetryTestUtils.jsm",
});

// We set the Merino timeout to a large value to avoid intermittent failures in
// CI, especially TV tests, where the Merino fetch unexpectedly doesn't finish
// before the default timeout.
const TEST_MERINO_TIMEOUT_MS = 1000;

// relative to `browser.urlbar`
const PREF_DATA_COLLECTION_ENABLED = "quicksuggest.dataCollection.enabled";
const PREF_MERINO_CLIENT_VARIANTS = "merino.clientVariants";
const PREF_MERINO_ENABLED = "merino.enabled";
const PREF_MERINO_ENDPOINT_URL = "merino.endpointURL";
const PREF_MERINO_PROVIDERS = "merino.providers";
const PREF_REMOTE_SETTINGS_ENABLED = "quicksuggest.remoteSettings.enabled";

const TELEMETRY_MERINO_LATENCY = "FX_URLBAR_MERINO_LATENCY_MS";
const TELEMETRY_MERINO_RESPONSE = "FX_URLBAR_MERINO_RESPONSE";
const FETCH_RESPONSE = {
  none: -1,
  success: 0,
  timeout: 1,
  network_error: 2,
  http_error: 3,
};

const SEARCH_STRING = "frab";

const REMOTE_SETTINGS_DATA = [
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

const EXPECTED_REMOTE_SETTINGS_RESULT = {
  type: UrlbarUtils.RESULT_TYPE.URL,
  source: UrlbarUtils.RESULT_SOURCE.SEARCH,
  heuristic: false,
  payload: {
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
    helpUrl: UrlbarProviderQuickSuggest.helpUrl,
    helpL10nId: "firefox-suggest-urlbar-learn-more",
    displayUrl: "http://test.com/q=frabbits",
    source: "remote-settings",
  },
};

const MERINO_RESPONSE = {
  body: {
    request_id: "request_id",
    suggestions: [
      {
        full_keyword: "full_keyword",
        title: "title",
        url: "url",
        icon: "icon",
        impression_url: "impression_url",
        click_url: "click_url",
        block_id: 1,
        advertiser: "advertiser",
        is_sponsored: true,
        score: 1,
      },
    ],
  },
};

const EXPECTED_MERINO_RESULT = {
  type: UrlbarUtils.RESULT_TYPE.URL,
  source: UrlbarUtils.RESULT_SOURCE.SEARCH,
  heuristic: false,
  payload: {
    qsSuggestion: "full_keyword",
    title: "title",
    url: "url",
    originalUrl: "url",
    icon: "icon",
    sponsoredImpressionUrl: "impression_url",
    sponsoredClickUrl: "click_url",
    sponsoredBlockId: 1,
    sponsoredAdvertiser: "advertiser",
    isSponsored: true,
    helpUrl: UrlbarProviderQuickSuggest.helpUrl,
    helpL10nId: "firefox-suggest-urlbar-learn-more",
    displayUrl: "url",
    requestId: "request_id",
    source: "merino",
  },
};

let gMerinoResponse;

add_task(async function init() {
  UrlbarPrefs.set("quicksuggest.enabled", true);
  UrlbarPrefs.set("suggest.quicksuggest.nonsponsored", true);
  UrlbarPrefs.set("suggest.quicksuggest.sponsored", true);
  UrlbarPrefs.set("quicksuggest.shouldShowOnboardingDialog", false);

  // Set up the Merino server.
  let path = "/merino";
  let server = makeMerinoServer(path);
  let url = new URL("http://localhost/");
  url.pathname = path;
  url.port = server.identity.primaryPort;
  UrlbarPrefs.set(PREF_MERINO_ENDPOINT_URL, url.toString());

  UrlbarPrefs.set("merino.timeoutMs", TEST_MERINO_TIMEOUT_MS);

  // Set up the remote settings client with the test data.
  await QuickSuggestTestUtils.ensureQuickSuggestInit(REMOTE_SETTINGS_DATA);

  Assert.equal(
    typeof UrlbarQuickSuggest.DEFAULT_SUGGESTION_SCORE,
    "number",
    "Sanity check: UrlbarQuickSuggest.DEFAULT_SUGGESTION_SCORE is defined"
  );
});

// Tests with Merino enabled and remote settings disabled.
add_task(async function oneEnabled_merino() {
  UrlbarPrefs.set(PREF_MERINO_ENABLED, true);
  UrlbarPrefs.set(PREF_REMOTE_SETTINGS_ENABLED, false);
  UrlbarPrefs.set(PREF_DATA_COLLECTION_ENABLED, true);

  let histograms = getAndClearHistograms();

  // Use a score lower than the remote settings score to make sure the
  // suggestion is included regardless.
  setMerinoResponse().body.suggestions[0].score =
    UrlbarQuickSuggest.DEFAULT_SUGGESTION_SCORE / 2;

  let context = createContext(SEARCH_STRING, {
    providers: [UrlbarProviderQuickSuggest.name],
    isPrivate: false,
  });
  await check_results({
    context,
    matches: [EXPECTED_MERINO_RESULT],
  });

  assertAndClearHistograms({
    histograms,
    context,
    response: FETCH_RESPONSE.success,
    latencyRecorded: true,
  });
});

// Tests with Merino disabled and remote settings enabled.
add_task(async function oneEnabled_remoteSettings() {
  UrlbarPrefs.set(PREF_MERINO_ENABLED, false);
  UrlbarPrefs.set(PREF_REMOTE_SETTINGS_ENABLED, true);
  UrlbarPrefs.set(PREF_DATA_COLLECTION_ENABLED, true);

  let histograms = getAndClearHistograms();

  // Make sure the server is prepared to return a response so we can make sure
  // we don't fetch it.
  setMerinoResponse();

  let context = createContext(SEARCH_STRING, {
    providers: [UrlbarProviderQuickSuggest.name],
    isPrivate: false,
  });
  await check_results({
    context,
    matches: [EXPECTED_REMOTE_SETTINGS_RESULT],
  });

  assertAndClearHistograms({
    histograms,
    context,
    response: FETCH_RESPONSE.none,
    latencyRecorded: false,
  });
});

// Tests with Merino enabled but with data collection disabled. Results should
// not be fetched from Merino in that case. Also tests with remote settings
// enabled.
add_task(async function dataCollectionDisabled() {
  UrlbarPrefs.set(PREF_MERINO_ENABLED, true);
  UrlbarPrefs.set(PREF_REMOTE_SETTINGS_ENABLED, true);
  UrlbarPrefs.set(PREF_DATA_COLLECTION_ENABLED, false);

  // Make sure the server is prepared to return a response so we can make sure
  // we don't fetch it.
  setMerinoResponse();

  let context = createContext("frab", {
    providers: [UrlbarProviderQuickSuggest.name],
    isPrivate: false,
  });
  await check_results({
    context,
    matches: [EXPECTED_REMOTE_SETTINGS_RESULT],
  });
});

// When the Merino suggestion has a higher score than the remote settings
// suggestion, the Merino suggestion should be used.
add_task(async function higherScore() {
  UrlbarPrefs.set(PREF_MERINO_ENABLED, true);
  UrlbarPrefs.set(PREF_REMOTE_SETTINGS_ENABLED, true);
  UrlbarPrefs.set(PREF_DATA_COLLECTION_ENABLED, true);

  let histograms = getAndClearHistograms();

  setMerinoResponse().body.suggestions[0].score =
    2 * UrlbarQuickSuggest.DEFAULT_SUGGESTION_SCORE;

  let context = createContext(SEARCH_STRING, {
    providers: [UrlbarProviderQuickSuggest.name],
    isPrivate: false,
  });
  await check_results({
    context,
    matches: [EXPECTED_MERINO_RESULT],
  });

  assertAndClearHistograms({
    histograms,
    context,
    response: FETCH_RESPONSE.success,
    latencyRecorded: true,
  });
});

// When the Merino suggestion has a lower score than the remote settings
// suggestion, the remote settings suggestion should be used.
add_task(async function lowerScore() {
  UrlbarPrefs.set(PREF_MERINO_ENABLED, true);
  UrlbarPrefs.set(PREF_REMOTE_SETTINGS_ENABLED, true);
  UrlbarPrefs.set(PREF_DATA_COLLECTION_ENABLED, true);

  let histograms = getAndClearHistograms();

  setMerinoResponse().body.suggestions[0].score =
    UrlbarQuickSuggest.DEFAULT_SUGGESTION_SCORE / 2;

  let context = createContext(SEARCH_STRING, {
    providers: [UrlbarProviderQuickSuggest.name],
    isPrivate: false,
  });
  await check_results({
    context,
    matches: [EXPECTED_REMOTE_SETTINGS_RESULT],
  });

  assertAndClearHistograms({
    histograms,
    context,
    response: FETCH_RESPONSE.success,
    latencyRecorded: true,
  });
});

// When the Merino and remote settings suggestions have the same score, the
// remote settings suggestion should be used.
add_task(async function sameScore() {
  UrlbarPrefs.set(PREF_MERINO_ENABLED, true);
  UrlbarPrefs.set(PREF_REMOTE_SETTINGS_ENABLED, true);
  UrlbarPrefs.set(PREF_DATA_COLLECTION_ENABLED, true);

  let histograms = getAndClearHistograms();

  setMerinoResponse().body.suggestions[0].score =
    UrlbarQuickSuggest.DEFAULT_SUGGESTION_SCORE;

  let context = createContext(SEARCH_STRING, {
    providers: [UrlbarProviderQuickSuggest.name],
    isPrivate: false,
  });
  await check_results({
    context,
    matches: [EXPECTED_REMOTE_SETTINGS_RESULT],
  });

  assertAndClearHistograms({
    histograms,
    context,
    response: FETCH_RESPONSE.success,
    latencyRecorded: true,
  });
});

// When the Merino suggestion does not include a score, the remote settings
// suggestion should be used.
add_task(async function noMerinoScore() {
  UrlbarPrefs.set(PREF_MERINO_ENABLED, true);
  UrlbarPrefs.set(PREF_REMOTE_SETTINGS_ENABLED, true);
  UrlbarPrefs.set(PREF_DATA_COLLECTION_ENABLED, true);

  let histograms = getAndClearHistograms();

  let resp = setMerinoResponse();
  Assert.equal(
    typeof resp.body.suggestions[0].score,
    "number",
    "Sanity check: First suggestion has a score"
  );
  delete resp.body.suggestions[0].score;

  let context = createContext(SEARCH_STRING, {
    providers: [UrlbarProviderQuickSuggest.name],
    isPrivate: false,
  });
  await check_results({
    context,
    matches: [EXPECTED_REMOTE_SETTINGS_RESULT],
  });

  assertAndClearHistograms({
    histograms,
    context,
    response: FETCH_RESPONSE.success,
    latencyRecorded: true,
  });
});

// When remote settings doesn't return a suggestion but Merino does, the Merino
// suggestion should be used.
add_task(async function noSuggestion_remoteSettings() {
  UrlbarPrefs.set(PREF_MERINO_ENABLED, true);
  UrlbarPrefs.set(PREF_REMOTE_SETTINGS_ENABLED, true);
  UrlbarPrefs.set(PREF_DATA_COLLECTION_ENABLED, true);

  let histograms = getAndClearHistograms();

  setMerinoResponse();

  let context = createContext("this doesn't match remote settings", {
    providers: [UrlbarProviderQuickSuggest.name],
    isPrivate: false,
  });
  await check_results({
    context,
    matches: [EXPECTED_MERINO_RESULT],
  });

  assertAndClearHistograms({
    histograms,
    context,
    response: FETCH_RESPONSE.success,
    latencyRecorded: true,
  });
});

// When Merino doesn't return a suggestion but remote settings does, the remote
// settings suggestion should be used.
add_task(async function noSuggestion_merino() {
  UrlbarPrefs.set(PREF_MERINO_ENABLED, true);
  UrlbarPrefs.set(PREF_REMOTE_SETTINGS_ENABLED, true);
  UrlbarPrefs.set(PREF_DATA_COLLECTION_ENABLED, true);

  let histograms = getAndClearHistograms();

  setMerinoResponse({
    body: {
      request_id: "request_id",
      suggestions: [],
    },
  });

  let context = createContext(SEARCH_STRING, {
    providers: [UrlbarProviderQuickSuggest.name],
    isPrivate: false,
  });
  await check_results({
    context,
    matches: [EXPECTED_REMOTE_SETTINGS_RESULT],
  });

  assertAndClearHistograms({
    histograms,
    context,
    response: FETCH_RESPONSE.success,
    latencyRecorded: true,
  });
});

// Tests with both Merino and remote settings disabled.
add_task(async function bothDisabled() {
  UrlbarPrefs.set(PREF_MERINO_ENABLED, false);
  UrlbarPrefs.set(PREF_REMOTE_SETTINGS_ENABLED, false);
  UrlbarPrefs.set(PREF_DATA_COLLECTION_ENABLED, true);

  let histograms = getAndClearHistograms();

  // Make sure the server is prepared to return a response so we can make sure
  // we don't fetch it.
  setMerinoResponse();

  let context = createContext(SEARCH_STRING, {
    providers: [UrlbarProviderQuickSuggest.name],
    isPrivate: false,
  });
  await check_results({ context, matches: [] });

  assertAndClearHistograms({
    histograms,
    context,
    response: FETCH_RESPONSE.none,
    latencyRecorded: false,
  });
});

// When Merino returns multiple suggestions, the one with the largest score
// should be used.
add_task(async function multipleMerinoSuggestions() {
  UrlbarPrefs.set(PREF_MERINO_ENABLED, true);
  UrlbarPrefs.set(PREF_REMOTE_SETTINGS_ENABLED, false);
  UrlbarPrefs.set(PREF_DATA_COLLECTION_ENABLED, true);

  let histograms = getAndClearHistograms();

  setMerinoResponse({
    body: {
      request_id: "request_id",
      suggestions: [
        {
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
      ],
    },
  });

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
          helpUrl: UrlbarProviderQuickSuggest.helpUrl,
          helpL10nId: "firefox-suggest-urlbar-learn-more",
          displayUrl: "multipleMerinoSuggestions 1 url",
          requestId: "request_id",
          source: "merino",
        },
      },
    ],
  });

  assertAndClearHistograms({
    histograms,
    context,
    response: FETCH_RESPONSE.success,
    latencyRecorded: true,
  });
});

// Checks a response that's valid but also has some unexpected properties.
add_task(async function unexpectedResponseProperties() {
  UrlbarPrefs.set(PREF_MERINO_ENABLED, true);
  UrlbarPrefs.set(PREF_REMOTE_SETTINGS_ENABLED, false);
  UrlbarPrefs.set(PREF_DATA_COLLECTION_ENABLED, true);

  let histograms = getAndClearHistograms();

  let resp = setMerinoResponse();
  resp.body.unexpectedString = "some value";
  resp.body.unexpectedArray = ["a", "b", "c"];
  resp.body.unexpectedObject = { foo: "bar" };

  let context = createContext("test", {
    providers: [UrlbarProviderQuickSuggest.name],
    isPrivate: false,
  });
  await check_results({
    context,
    matches: [EXPECTED_MERINO_RESULT],
  });

  assertAndClearHistograms({
    histograms,
    context,
    response: FETCH_RESPONSE.success,
    latencyRecorded: true,
  });
});

// Checks some responses with unexpected response bodies.
add_task(async function unexpectedResponseBody() {
  UrlbarPrefs.set(PREF_MERINO_ENABLED, true);
  UrlbarPrefs.set(PREF_REMOTE_SETTINGS_ENABLED, false);
  UrlbarPrefs.set(PREF_DATA_COLLECTION_ENABLED, true);

  let histograms = getAndClearHistograms();

  let context;
  let contextArgs = [
    "test",
    { providers: [UrlbarProviderQuickSuggest.name], isPrivate: false },
  ];

  setMerinoResponse({
    body: {},
  });
  context = createContext(...contextArgs);
  await check_results({ context, matches: [] });
  assertAndClearHistograms({
    histograms,
    context,
    response: FETCH_RESPONSE.success,
    latencyRecorded: true,
  });

  setMerinoResponse({
    body: { bogus: [] },
  });
  context = createContext(...contextArgs);
  await check_results({ context, matches: [] });
  assertAndClearHistograms({
    histograms,
    context,
    response: FETCH_RESPONSE.success,
    latencyRecorded: true,
  });

  setMerinoResponse({
    body: { suggestions: {} },
  });
  context = createContext(...contextArgs);
  await check_results({ context, matches: [] });
  assertAndClearHistograms({
    histograms,
    context,
    response: FETCH_RESPONSE.success,
    latencyRecorded: true,
  });

  setMerinoResponse({
    body: { suggestions: [] },
  });
  context = createContext(...contextArgs);
  await check_results({ context, matches: [] });
  assertAndClearHistograms({
    histograms,
    context,
    response: FETCH_RESPONSE.success,
    latencyRecorded: true,
  });

  setMerinoResponse({
    body: "",
  });
  context = createContext(...contextArgs);
  await check_results({ context, matches: [] });
  assertAndClearHistograms({
    histograms,
    context,
    response: FETCH_RESPONSE.success,
    latencyRecorded: true,
  });

  setMerinoResponse({
    contentType: "text/html",
    body: "bogus",
  });
  context = createContext(...contextArgs);
  await check_results({ context, matches: [] });
  assertAndClearHistograms({
    histograms,
    context,
    response: FETCH_RESPONSE.success,
    latencyRecorded: true,
  });
});

// When there's a network error and remote settings is disabled, no results
// should be returned.
add_task(async function networkError_merinoOnly() {
  UrlbarPrefs.set(PREF_MERINO_ENABLED, true);
  UrlbarPrefs.set(PREF_REMOTE_SETTINGS_ENABLED, false);
  UrlbarPrefs.set(PREF_DATA_COLLECTION_ENABLED, true);
  await doNetworkErrorTest([]);
});

// When there's a network error and remote settings is enabled, the remote
// settings result should be returned.
add_task(async function networkError_withRemoteSettings() {
  UrlbarPrefs.set(PREF_MERINO_ENABLED, true);
  UrlbarPrefs.set(PREF_REMOTE_SETTINGS_ENABLED, true);
  UrlbarPrefs.set(PREF_DATA_COLLECTION_ENABLED, true);
  await doNetworkErrorTest([EXPECTED_REMOTE_SETTINGS_RESULT]);
});

async function doNetworkErrorTest(expectedResults) {
  // Set the endpoint to a valid, unreachable URL.
  let originalURL = UrlbarPrefs.get(PREF_MERINO_ENDPOINT_URL);
  UrlbarPrefs.set(
    PREF_MERINO_ENDPOINT_URL,
    "http://localhost/test_quicksuggest_merino"
  );

  // Set the timeout high enough that the network error exception will happen
  // first. On Mac and Linux the fetch naturally times out fairly quickly but on
  // Windows it seems to take 5s, so set our artificial timeout to 10s.
  UrlbarPrefs.set("merino.timeoutMs", 10000);

  let histograms = getAndClearHistograms();

  let context = createContext(SEARCH_STRING, {
    providers: [UrlbarProviderQuickSuggest.name],
    isPrivate: false,
  });
  await check_results({
    context,
    matches: expectedResults,
  });

  // The provider should have nulled out the timeout timer.
  Assert.ok(
    !UrlbarProviderQuickSuggest._merinoTimeoutTimer,
    "_merinoTimeoutTimer does not exist after search finished"
  );

  // Wait for the fetch to finish. The provider will null out the fetch
  // controller when that happens. Wait for 10s as above.
  await TestUtils.waitForCondition(
    () => !UrlbarProviderQuickSuggest._merinoFetchController,
    "Waiting for fetch to finish",
    100, // interval between tries (ms)
    100 // number of tries
  );

  assertAndClearHistograms({
    histograms,
    context,
    response: FETCH_RESPONSE.network_error,
    latencyRecorded: false,
  });

  UrlbarPrefs.set(PREF_MERINO_ENDPOINT_URL, originalURL);
  UrlbarPrefs.set("merino.timeoutMs", TEST_MERINO_TIMEOUT_MS);
}

// When there's an HTTP error and remote settings is disabled, no results should
// be returned.
add_task(async function httpError_merinoOnly() {
  UrlbarPrefs.set(PREF_MERINO_ENABLED, true);
  UrlbarPrefs.set(PREF_REMOTE_SETTINGS_ENABLED, false);
  UrlbarPrefs.set(PREF_DATA_COLLECTION_ENABLED, true);
  await doHTTPErrorTest([]);
});

// When there's an HTTP error and remote settings is enabled, the remote
// settings result should be returned.
add_task(async function httpError_withRemoteSettings() {
  UrlbarPrefs.set(PREF_MERINO_ENABLED, true);
  UrlbarPrefs.set(PREF_REMOTE_SETTINGS_ENABLED, true);
  UrlbarPrefs.set(PREF_DATA_COLLECTION_ENABLED, true);
  await doHTTPErrorTest([EXPECTED_REMOTE_SETTINGS_RESULT]);
});

async function doHTTPErrorTest(expectedResults) {
  let histograms = getAndClearHistograms();

  setMerinoResponse({
    status: 500,
  });

  let context = createContext(SEARCH_STRING, {
    providers: [UrlbarProviderQuickSuggest.name],
    isPrivate: false,
  });
  await check_results({
    context,
    matches: expectedResults,
  });

  assertAndClearHistograms({
    histograms,
    context,
    response: FETCH_RESPONSE.http_error,
    latencyRecorded: true,
  });
}

// When Merino times out and remote settings is disabled, no results should be
// returned.
add_task(async function timeout_merinoOnly() {
  UrlbarPrefs.set(PREF_MERINO_ENABLED, true);
  UrlbarPrefs.set(PREF_REMOTE_SETTINGS_ENABLED, false);
  UrlbarPrefs.set(PREF_DATA_COLLECTION_ENABLED, true);
  setMerinoResponse();
  await doSimpleTimeoutTest([]);
});

// When Merino times out and remote settings is enabled, the remote settings
// result should be returned.
add_task(async function timeout_withRemoteSettings() {
  UrlbarPrefs.set(PREF_MERINO_ENABLED, true);
  UrlbarPrefs.set(PREF_REMOTE_SETTINGS_ENABLED, true);
  UrlbarPrefs.set(PREF_DATA_COLLECTION_ENABLED, true);
  setMerinoResponse();
  await doSimpleTimeoutTest([EXPECTED_REMOTE_SETTINGS_RESULT]);
});

// When Merino times out but then responds with an HTTP error, only the timeout
// should be recorded.
add_task(async function timeout_followedByHTTPError() {
  UrlbarPrefs.set(PREF_MERINO_ENABLED, true);
  UrlbarPrefs.set(PREF_REMOTE_SETTINGS_ENABLED, false);
  UrlbarPrefs.set(PREF_DATA_COLLECTION_ENABLED, true);
  let resp = setMerinoResponse();
  resp.status = 500;
  delete resp.body;
  await doSimpleTimeoutTest([]);
});

async function doSimpleTimeoutTest(expectedResults) {
  let histograms = getAndClearHistograms();

  // Make the server return a delayed response so it times out.
  gMerinoResponse.delay = 2 * UrlbarPrefs.get("merinoTimeoutMs");

  let context = createContext(SEARCH_STRING, {
    providers: [UrlbarProviderQuickSuggest.name],
    isPrivate: false,
  });
  await check_results({
    context,
    matches: expectedResults,
  });

  // The provider should have nulled out the timeout timer.
  Assert.ok(
    !UrlbarProviderQuickSuggest._merinoTimeoutTimer,
    "_merinoTimeoutTimer does not exist after search finished"
  );

  // The fetch controller should still exist because the fetch should remain
  // ongoing.
  Assert.ok(
    UrlbarProviderQuickSuggest._merinoFetchController,
    "_merinoFetchController still exists after search finished"
  );
  Assert.ok(
    !UrlbarProviderQuickSuggest._merinoFetchController.signal.aborted,
    "_merinoFetchController is not aborted"
  );

  // The latency histogram should not be updated since the fetch is still
  // ongoing.
  assertAndClearHistograms({
    histograms,
    context,
    response: FETCH_RESPONSE.timeout,
    latencyRecorded: false,
    latencyStopwatchRunning: true,
  });

  // Wait for the fetch to finish. The provider will null out the fetch
  // controller when that happens.
  await TestUtils.waitForCondition(
    () => !UrlbarProviderQuickSuggest._merinoFetchController,
    "Waiting for fetch to finish"
  );

  assertAndClearHistograms({
    histograms,
    context,
    // The assertAndClearHistograms() call above cleared the histograms. After
    // that call, nothing else should have been recorded for the response.
    response: FETCH_RESPONSE.none,
    latencyRecorded: true,
  });
}

// By design, when a fetch times out, the provider finishes the search but it
// allows the fetch to continue so we can record its latency. When a new search
// starts immediately after that, the provider should abort the (previous) fetch
// so that there is at most one fetch at a time.
add_task(async function newFetchAbortsPrevious() {
  UrlbarPrefs.set(PREF_MERINO_ENABLED, true);
  UrlbarPrefs.set(PREF_REMOTE_SETTINGS_ENABLED, false);
  UrlbarPrefs.set(PREF_DATA_COLLECTION_ENABLED, true);

  let histograms = getAndClearHistograms();

  // Make the server return a very delayed response so that it would time out
  // and we can start a second search that will abort the first fetch.
  let resp = setMerinoResponse();
  resp.delay = 10000 * UrlbarPrefs.get("merinoTimeoutMs");

  // Do the first search.
  let context = createContext("first search", {
    providers: [UrlbarProviderQuickSuggest.name],
    isPrivate: false,
  });
  await check_results({
    context,
    matches: [],
  });

  // At this point, the timeout timer has fired, causing the provider to
  // finish. The fetch should still be ongoing.

  // The provider should have nulled out the timeout timer.
  Assert.ok(
    !UrlbarProviderQuickSuggest._merinoTimeoutTimer,
    "_merinoTimeoutTimer does not exist after first search finished"
  );

  // The fetch controller should still exist because the fetch should remain
  // ongoing.
  Assert.ok(
    UrlbarProviderQuickSuggest._merinoFetchController,
    "_merinoFetchController still exists after first search finished"
  );
  Assert.ok(
    !UrlbarProviderQuickSuggest._merinoFetchController.signal.aborted,
    "_merinoFetchController is not aborted"
  );

  // The latency histogram should not be updated since the fetch is still
  // ongoing.
  assertAndClearHistograms({
    histograms,
    context,
    response: FETCH_RESPONSE.timeout,
    latencyRecorded: false,
    latencyStopwatchRunning: true,
  });

  // Do the second search. This time don't delay the response.
  delete resp.delay;
  context = createContext("second search", {
    providers: [UrlbarProviderQuickSuggest.name],
    isPrivate: false,
  });
  await check_results({
    context,
    matches: [EXPECTED_MERINO_RESULT],
  });

  // The fetch was successful, so the provider should have nulled out both
  // properties.
  Assert.ok(
    !UrlbarProviderQuickSuggest._merinoFetchController,
    "_merinoFetchController does not exist after second search finished"
  );
  Assert.ok(
    !UrlbarProviderQuickSuggest._merinoTimeoutTimer,
    "_merinoTimeoutTimer does not exist after second search finished"
  );

  assertAndClearHistograms({
    histograms,
    context,
    response: FETCH_RESPONSE.success,
    latencyRecorded: true,
  });
});

// By design, canceling a search after the fetch has started should allow the
// fetch to finish so we can record its latency, as long as a new search doesn't
// start before the fetch finishes.
add_task(async function cancelDoesNotAbortFetch() {
  UrlbarPrefs.set(PREF_MERINO_ENABLED, true);
  UrlbarPrefs.set(PREF_REMOTE_SETTINGS_ENABLED, false);
  UrlbarPrefs.set(PREF_DATA_COLLECTION_ENABLED, true);

  let histograms = getAndClearHistograms();

  // Make the server return a delayed response so we can cancel it before the
  // search finishes.
  setMerinoResponse().delay = 1000;

  // Do a search but don't wait for it to finish.
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
      window: {
        location: {
          href: AppConstants.BROWSER_CHROME_URL,
        },
      },
    },
  });
  let searchPromise = controller.startQuery(context);

  // Wait for the fetch controller to be created so we know the fetch has
  // actually started.
  await TestUtils.waitForCondition(
    () => UrlbarProviderQuickSuggest._merinoFetchController,
    "Waiting for _merinoFetchController"
  );

  // Now cancel the search.
  controller.cancelQuery();
  await searchPromise;

  // The fetch controller should still exist because the fetch should remain
  // ongoing.
  Assert.ok(
    UrlbarProviderQuickSuggest._merinoFetchController,
    "_merinoFetchController still exists after search canceled"
  );
  Assert.ok(
    !UrlbarProviderQuickSuggest._merinoFetchController.signal.aborted,
    "_merinoFetchController is not aborted"
  );

  // The latency histogram should not be updated since the fetch is still
  // ongoing.
  assertAndClearHistograms({
    histograms,
    context,
    response: FETCH_RESPONSE.none,
    latencyRecorded: false,
    latencyStopwatchRunning: true,
  });

  // Wait for the fetch to finish. The provider will null out the fetch
  // controller when that happens.
  await TestUtils.waitForCondition(
    () => !UrlbarProviderQuickSuggest._merinoFetchController,
    "Waiting for provider to null out _merinoFetchController"
  );

  assertAndClearHistograms({
    histograms,
    context,
    response: FETCH_RESPONSE.none,
    latencyRecorded: true,
  });
});

// Timestamp templates in URLs should be replaced with real timestamps.
add_task(async function timestamps() {
  UrlbarPrefs.set(PREF_MERINO_ENABLED, true);
  UrlbarPrefs.set(PREF_REMOTE_SETTINGS_ENABLED, false);
  UrlbarPrefs.set(PREF_DATA_COLLECTION_ENABLED, true);

  // Set up the Merino response with template URLs.
  let resp = setMerinoResponse();
  let suggestion = resp.body.suggestions[0];
  let { TIMESTAMP_TEMPLATE } = UrlbarProviderQuickSuggest;

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
});

// Tests that Merino includes the clientVariants and providers urls, if set.
add_task(async function clientVariants_providers() {
  UrlbarPrefs.set(PREF_MERINO_ENABLED, true);
  UrlbarPrefs.set(PREF_REMOTE_SETTINGS_ENABLED, false);
  UrlbarPrefs.set(PREF_DATA_COLLECTION_ENABLED, true);
  UrlbarPrefs.set(PREF_MERINO_CLIENT_VARIANTS, "");
  UrlbarPrefs.set(PREF_MERINO_PROVIDERS, "");

  function parseQueryString(queryString) {
    return Object.fromEntries(queryString.split("&").map(p => p.split("=")));
  }

  let checksCalled = 0;

  // Check that neither clientVariants or providers are added.
  setMerinoResponse().checkRequest = req => {
    let qs = parseQueryString(req.queryString);
    Assert.ok(
      !Object.hasOwn(qs, "providers"),
      "providers should not be specified"
    );
    Assert.ok(
      !Object.hasOwn(qs, "client_variants"),
      "client_variants should not be specified"
    );
    checksCalled += 1;
  };

  let context = createContext(SEARCH_STRING, {
    providers: [UrlbarProviderQuickSuggest.name],
    isPrivate: false,
  });
  await check_results({
    context,
    matches: [EXPECTED_MERINO_RESULT],
  });

  UrlbarPrefs.set(PREF_MERINO_CLIENT_VARIANTS, "green");
  UrlbarPrefs.set(PREF_MERINO_PROVIDERS, "pink");

  // Check that neither clientVariants or providers are added.
  setMerinoResponse().checkRequest = req => {
    let qs = parseQueryString(req.queryString);
    Assert.equal(qs.client_variants, "green", "client variants should be set");
    Assert.equal(qs.providers, "pink", "providers should be set");
    checksCalled += 1;
  };

  context = createContext(SEARCH_STRING, {
    providers: [UrlbarProviderQuickSuggest.name],
    isPrivate: false,
  });
  await check_results({
    context,
    matches: [EXPECTED_MERINO_RESULT],
  });

  Assert.equal(checksCalled, 2, "both checks should have been called");

  UrlbarPrefs.clear(PREF_MERINO_CLIENT_VARIANTS);
  UrlbarPrefs.clear(PREF_MERINO_PROVIDERS);
});

// When both suggestion types are disabled but data collection is enabled, we
// should still send requests to Merino, and the requests should include an
// empty `providers` to tell Merino not to fetch any suggestions.
add_task(async function suggestedDisabled_dataCollectionEnabled() {
  UrlbarPrefs.set(PREF_MERINO_ENABLED, true);
  UrlbarPrefs.set(PREF_REMOTE_SETTINGS_ENABLED, false);
  UrlbarPrefs.set(PREF_DATA_COLLECTION_ENABLED, true);
  UrlbarPrefs.set("suggest.quicksuggest.nonsponsored", false);
  UrlbarPrefs.set("suggest.quicksuggest.sponsored", false);

  let histograms = getAndClearHistograms();

  // Check that the request is received and includes an empty `providers`.
  let checkRequestCallCount = 0;
  setMerinoResponse().checkRequest = req => {
    checkRequestCallCount++;
    let params = new URLSearchParams(req.queryString);
    Assert.deepEqual(
      params.getAll("providers"),
      [""],
      "providers param is specified once and is an empty string"
    );
  };

  let context = createContext("test", {
    providers: [UrlbarProviderQuickSuggest.name],
    isPrivate: false,
  });
  await check_results({
    context,
    matches: [],
  });

  Assert.equal(checkRequestCallCount, 1, "Request received");

  assertAndClearHistograms({
    histograms,
    context,
    response: FETCH_RESPONSE.success,
    latencyRecorded: true,
  });

  UrlbarPrefs.set("suggest.quicksuggest.nonsponsored", true);
  UrlbarPrefs.set("suggest.quicksuggest.sponsored", true);
});

// When both suggestion types are disabled but data collection is enabled, we
// should still send requests to Merino, and when the `providers` pref is also
// set, it should be included in the request instead of an empty string.
add_task(async function suggestedDisabled_dataCollectionEnabled_providers() {
  UrlbarPrefs.set(PREF_MERINO_ENABLED, true);
  UrlbarPrefs.set(PREF_REMOTE_SETTINGS_ENABLED, false);
  UrlbarPrefs.set(PREF_DATA_COLLECTION_ENABLED, true);
  UrlbarPrefs.set("suggest.quicksuggest.nonsponsored", false);
  UrlbarPrefs.set("suggest.quicksuggest.sponsored", false);

  UrlbarPrefs.set(PREF_MERINO_PROVIDERS, "blue");

  let histograms = getAndClearHistograms();

  // Check that the request is received and includes the expected `providers`.
  let checkRequestCallCount = 0;
  setMerinoResponse().checkRequest = req => {
    checkRequestCallCount++;
    let params = new URLSearchParams(req.queryString);
    Assert.deepEqual(
      params.getAll("providers"),
      ["blue"],
      "providers param is specified once and is 'blue'"
    );
  };

  let context = createContext("test", {
    providers: [UrlbarProviderQuickSuggest.name],
    isPrivate: false,
  });
  await check_results({
    context,
    matches: [],
  });

  Assert.equal(checkRequestCallCount, 1, "Request received");

  assertAndClearHistograms({
    histograms,
    context,
    response: FETCH_RESPONSE.success,
    latencyRecorded: true,
  });

  UrlbarPrefs.set("suggest.quicksuggest.nonsponsored", true);
  UrlbarPrefs.set("suggest.quicksuggest.sponsored", true);
});

// Test whether the blocking for Merino results works.
add_task(async function block() {
  UrlbarPrefs.set(PREF_MERINO_ENABLED, true);
  UrlbarPrefs.set(PREF_REMOTE_SETTINGS_ENABLED, true);
  UrlbarPrefs.set(PREF_DATA_COLLECTION_ENABLED, true);

  for (const suggestion of MERINO_RESPONSE.body.suggestions) {
    await UrlbarProviderQuickSuggest.blockSuggestion(suggestion.url);
  }

  const context = createContext(SEARCH_STRING, {
    providers: [UrlbarProviderQuickSuggest.name],
    isPrivate: false,
  });

  await check_results({
    context,
    matches: [EXPECTED_REMOTE_SETTINGS_RESULT],
  });

  await UrlbarProviderQuickSuggest.clearBlockedSuggestions();
});

// Tests a Merino suggestion that is a best match.
add_task(async function bestMatch() {
  UrlbarPrefs.set(PREF_MERINO_ENABLED, true);
  UrlbarPrefs.set(PREF_REMOTE_SETTINGS_ENABLED, false);
  UrlbarPrefs.set(PREF_DATA_COLLECTION_ENABLED, true);

  // Simply enabling the best match feature should make the mock suggestion a
  // best match because the search string length is greater than the required
  // best match length.
  UrlbarPrefs.set("bestMatch.enabled", true);
  UrlbarPrefs.set("suggest.bestmatch", true);

  setMerinoResponse();

  let expectedResult = { ...EXPECTED_MERINO_RESULT };
  expectedResult.payload = { ...EXPECTED_MERINO_RESULT.payload };
  expectedResult.isBestMatch = true;
  delete expectedResult.payload.qsSuggestion;

  let context = createContext(SEARCH_STRING, {
    providers: [UrlbarProviderQuickSuggest.name],
    isPrivate: false,
  });
  await check_results({
    context,
    matches: [expectedResult],
  });

  // This isn't necessary since `check_results()` checks `isBestMatch`, but
  // check it here explicitly for good measure.
  Assert.ok(context.results[0].isBestMatch, "Result is a best match");

  UrlbarPrefs.clear("bestMatch.enabled");
  UrlbarPrefs.clear("suggest.bestmatch");
});

function makeMerinoServer(endpointPath) {
  let server = makeTestServer();
  server.registerPathHandler(endpointPath, async (req, resp) => {
    resp.processAsync();
    if (typeof gMerinoResponse.delay == "number") {
      await new Promise(resolve => {
        let timer = Cc["@mozilla.org/timer;1"].createInstance(Ci.nsITimer);
        timer.initWithCallback(
          resolve,
          gMerinoResponse.delay,
          Ci.nsITimer.TYPE_ONE_SHOT
        );
      });
    }
    if (typeof gMerinoResponse.status == "number") {
      resp.setStatusLine("", gMerinoResponse.status, gMerinoResponse.status);
    }
    resp.setHeader("Content-Type", gMerinoResponse.contentType, false);
    if (typeof gMerinoResponse.body == "string") {
      resp.write(gMerinoResponse.body);
    } else if (gMerinoResponse.body) {
      resp.write(JSON.stringify(gMerinoResponse.body));
    }
    if (typeof gMerinoResponse.checkRequest == "function") {
      gMerinoResponse.checkRequest(req);
    }
    resp.finish();
  });
  return server;
}

function deepCopy(obj) {
  if (Array.isArray(obj)) {
    return obj.map(item => deepCopy(item));
  }
  if (obj && typeof obj == "object") {
    return Object.entries(obj).reduce((memo, [key, value]) => {
      memo[key] = deepCopy(value);
      return memo;
    }, {});
  }
  return obj;
}

function setMerinoResponse(resp = deepCopy(MERINO_RESPONSE)) {
  if (!resp.contentType) {
    resp.contentType = "application/json";
  }
  gMerinoResponse = resp;

  info("Set Merino response: " + JSON.stringify(resp));
  return resp;
}

function getAndClearHistograms() {
  return {
    latency: TelemetryTestUtils.getAndClearHistogram(TELEMETRY_MERINO_LATENCY),
    response: TelemetryTestUtils.getAndClearHistogram(
      TELEMETRY_MERINO_RESPONSE
    ),
  };
}

/**
 * Asserts the Merino latency and response histograms are updated as expected.
 * Clears the histograms before returning.
 *
 * @param {object} histograms
 *   The histograms object returned from getAndClearHistograms().
 * @param {number} response
 *   The expected `FETCH_RESPONSE`.
 * @param {boolean} latencyRecorded
 *   Whether the Merino latency histogram is expected to contain a value.
 * @param {UrlbarQueryContext} context
 * @param {boolean} latencyStopwatchRunning
 *   Whether the Merino latency stopwatch is expected to be running.
 */
function assertAndClearHistograms({
  histograms,
  response,
  latencyRecorded,
  context,
  latencyStopwatchRunning = false,
}) {
  // Check the response histogram.
  Assert.equal(typeof response, "number", "Sanity check: response is defined");
  if (response != FETCH_RESPONSE.none) {
    TelemetryTestUtils.assertHistogram(histograms.response, response, 1);
  } else {
    Assert.strictEqual(
      histograms.response.snapshot().sum,
      0,
      "Response histogram not updated"
    );
  }

  // Check the latency histogram.
  if (latencyRecorded) {
    // There should be a single value across all buckets.
    Assert.deepEqual(
      Object.values(histograms.latency.snapshot().values).filter(v => v > 0),
      [1],
      "Latency histogram updated"
    );
  } else {
    Assert.strictEqual(
      histograms.latency.snapshot().sum,
      0,
      "Latency histogram not updated"
    );
  }

  // Check the latency stopwatch.
  Assert.equal(
    TelemetryStopwatch.running(TELEMETRY_MERINO_LATENCY, context),
    latencyStopwatchRunning,
    "Latency stopwatch running as expected"
  );

  // Clear histograms.
  for (let histogram of Object.values(histograms)) {
    histogram.clear();
  }
}
