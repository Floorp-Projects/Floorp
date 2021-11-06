/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

// Tests Merino integration with the quick suggest feature/provider.

"use strict";

XPCOMUtils.defineLazyModuleGetters(this, {
  UrlbarProviderQuickSuggest:
    "resource:///modules/UrlbarProviderQuickSuggest.jsm",
  UrlbarQuickSuggest: "resource:///modules/UrlbarQuickSuggest.jsm",
});

// relative to `browser.urlbar`
const PREF_MERINO_ENABLED = "merino.enabled";
const PREF_REMOTE_SETTINGS_ENABLED = "quicksuggest.remoteSettings.enabled";
const PREF_MERINO_ENDPOINT_URL = "merino.endpointURL";

const TELEMETRY_MERINO_LATENCY = "FX_URLBAR_MERINO_LATENCY_MS";

const REMOTE_SETTINGS_SEARCH_STRING = "frab";

const REMOTE_SETTINGS_DATA = [
  {
    id: 1,
    url: "http://test.com/q=frabbits",
    title: "frabbits",
    keywords: [REMOTE_SETTINGS_SEARCH_STRING],
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
    qsSuggestion: REMOTE_SETTINGS_SEARCH_STRING,
    title: "frabbits",
    url: "http://test.com/q=frabbits",
    icon: null,
    sponsoredImpressionUrl: "http://impression.reporting.test.com/",
    sponsoredClickUrl: "http://click.reporting.test.com/",
    sponsoredBlockId: 1,
    sponsoredAdvertiser: "testadvertiser",
    isSponsored: true,
    helpUrl: UrlbarProviderQuickSuggest.helpUrl,
    helpL10nId: "firefox-suggest-urlbar-learn-more",
    displayUrl: "http://test.com/q=frabbits",
    source: "remote-settings",
  },
};

let gMerinoResponse;

add_task(async function init() {
  UrlbarPrefs.set("quicksuggest.enabled", true);
  UrlbarPrefs.set("suggest.quicksuggest", true);
  UrlbarPrefs.set("suggest.quicksuggest.sponsored", true);
  UrlbarPrefs.set("quicksuggest.shouldShowOnboardingDialog", false);

  // Set up the Merino server.
  let path = "/merino";
  let server = makeMerinoServer(path);
  let url = new URL("http://localhost/");
  url.pathname = path;
  url.port = server.identity.primaryPort;
  UrlbarPrefs.set(PREF_MERINO_ENDPOINT_URL, url.toString());

  // Set up the remote settings client with the test data.
  await QuickSuggestTestUtils.ensureQuickSuggestInit(REMOTE_SETTINGS_DATA);

  Assert.equal(
    typeof UrlbarQuickSuggest.SUGGESTION_SCORE,
    "number",
    "Sanity check: UrlbarQuickSuggest.SUGGESTION_SCORE is defined"
  );
});

// Tests with Merino enabled and remote settings disabled.
add_task(async function oneEnabled_merino() {
  UrlbarPrefs.set(PREF_MERINO_ENABLED, true);
  UrlbarPrefs.set(PREF_REMOTE_SETTINGS_ENABLED, false);

  setMerinoResponse({
    body: {
      request_id: "merinoOnly request_id",
      suggestions: [
        {
          full_keyword: "merinoOnly full_keyword",
          title: "merinoOnly title",
          url: "merinoOnly url",
          icon: "merinoOnly icon",
          impression_url: "merinoOnly impression_url",
          click_url: "merinoOnly click_url",
          block_id: 111,
          advertiser: "merinoOnly advertiser",
          is_sponsored: true,
          // Use a score lower than the remote settings score to make sure this
          // suggestion is included regardless.
          score: UrlbarQuickSuggest.SUGGESTION_SCORE / 2,
        },
      ],
    },
  });

  let context = createContext(REMOTE_SETTINGS_SEARCH_STRING, {
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
          qsSuggestion: "merinoOnly full_keyword",
          title: "merinoOnly title",
          url: "merinoOnly url",
          icon: "merinoOnly icon",
          sponsoredImpressionUrl: "merinoOnly impression_url",
          sponsoredClickUrl: "merinoOnly click_url",
          sponsoredBlockId: 111,
          sponsoredAdvertiser: "merinoOnly advertiser",
          isSponsored: true,
          helpUrl: UrlbarProviderQuickSuggest.helpUrl,
          helpL10nId: "firefox-suggest-urlbar-learn-more",
          displayUrl: "merinoOnly url",
          requestId: "merinoOnly request_id",
          source: "merino",
        },
      },
    ],
  });
});

// Tests with Merino disabled and remote settings enabled.
add_task(async function oneEnabled_remoteSettings() {
  UrlbarPrefs.set(PREF_MERINO_ENABLED, false);
  UrlbarPrefs.set(PREF_REMOTE_SETTINGS_ENABLED, true);

  // Make sure the server is prepared to return a response so we can make sure
  // we don't fetch it.
  setMerinoResponse({
    body: {
      request_id: "merinoOnly request_id",
      suggestions: [
        {
          full_keyword: "remoteSettingsOnly full_keyword",
          title: "remoteSettingsOnly title",
          url: "remoteSettingsOnly url",
          icon: "remoteSettingsOnly icon",
          impression_url: "remoteSettingsOnly impression_url",
          click_url: "remoteSettingsOnly click_url",
          block_id: 111,
          advertiser: "remoteSettingsOnly advertiser",
          is_sponsored: true,
          score: 1,
        },
      ],
    },
  });

  let context = createContext(REMOTE_SETTINGS_SEARCH_STRING, {
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
add_task(async function higherScore_merino() {
  UrlbarPrefs.set(PREF_MERINO_ENABLED, true);
  UrlbarPrefs.set(PREF_REMOTE_SETTINGS_ENABLED, true);

  setMerinoResponse({
    body: {
      request_id: "merinoOnly request_id",
      suggestions: [
        {
          full_keyword: "higherScore full_keyword",
          title: "higherScore title",
          url: "higherScore url",
          icon: "higherScore icon",
          impression_url: "higherScore impression_url",
          click_url: "higherScore click_url",
          block_id: 111,
          advertiser: "higherScore advertiser",
          is_sponsored: true,
          score: 1,
        },
      ],
    },
  });

  let context = createContext(REMOTE_SETTINGS_SEARCH_STRING, {
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
          qsSuggestion: "higherScore full_keyword",
          title: "higherScore title",
          url: "higherScore url",
          icon: "higherScore icon",
          sponsoredImpressionUrl: "higherScore impression_url",
          sponsoredClickUrl: "higherScore click_url",
          sponsoredBlockId: 111,
          sponsoredAdvertiser: "higherScore advertiser",
          isSponsored: true,
          helpUrl: UrlbarProviderQuickSuggest.helpUrl,
          helpL10nId: "firefox-suggest-urlbar-learn-more",
          displayUrl: "higherScore url",
          requestId: "merinoOnly request_id",
          source: "merino",
        },
      },
    ],
  });
});

// When the remote settings suggestion has a higher score than the Merino
// suggestion, the remote settings suggestion should be used.
add_task(async function higherScore_remoteSettings() {
  UrlbarPrefs.set(PREF_MERINO_ENABLED, true);
  UrlbarPrefs.set(PREF_REMOTE_SETTINGS_ENABLED, true);

  setMerinoResponse({
    body: {
      request_id: "merinoOnly request_id",
      suggestions: [
        {
          full_keyword: "higherScore full_keyword",
          title: "higherScore title",
          url: "higherScore url",
          icon: "higherScore icon",
          impression_url: "higherScore impression_url",
          click_url: "higherScore click_url",
          block_id: 111,
          advertiser: "higherScore advertiser",
          is_sponsored: true,
          score: UrlbarQuickSuggest.SUGGESTION_SCORE / 2,
        },
      ],
    },
  });

  let context = createContext(REMOTE_SETTINGS_SEARCH_STRING, {
    providers: [UrlbarProviderQuickSuggest.name],
    isPrivate: false,
  });
  await check_results({
    context,
    matches: [EXPECTED_REMOTE_SETTINGS_RESULT],
  });
});

// When the Merino and remote settings suggestions have the same score, the
// remote settings suggestion should be used.
add_task(async function sameScore() {
  UrlbarPrefs.set(PREF_MERINO_ENABLED, true);
  UrlbarPrefs.set(PREF_REMOTE_SETTINGS_ENABLED, true);

  setMerinoResponse({
    body: {
      request_id: "merinoOnly request_id",
      suggestions: [
        {
          full_keyword: "sameScore full_keyword",
          title: "sameScore title",
          url: "sameScore url",
          icon: "sameScore icon",
          impression_url: "sameScore impression_url",
          click_url: "sameScore click_url",
          block_id: 111,
          advertiser: "sameScore advertiser",
          is_sponsored: true,
          score: UrlbarQuickSuggest.SUGGESTION_SCORE,
        },
      ],
    },
  });

  let context = createContext(REMOTE_SETTINGS_SEARCH_STRING, {
    providers: [UrlbarProviderQuickSuggest.name],
    isPrivate: false,
  });
  await check_results({
    context,
    matches: [EXPECTED_REMOTE_SETTINGS_RESULT],
  });
});

// When the Merino suggestion does not include a score, the remote settings
// suggestion should be used.
add_task(async function noMerinoScore() {
  UrlbarPrefs.set(PREF_MERINO_ENABLED, true);
  UrlbarPrefs.set(PREF_REMOTE_SETTINGS_ENABLED, true);

  setMerinoResponse({
    body: {
      request_id: "merinoOnly request_id",
      suggestions: [
        {
          full_keyword: "noMerinoScore full_keyword",
          title: "noMerinoScore title",
          url: "noMerinoScore url",
          icon: "noMerinoScore icon",
          impression_url: "noMerinoScore impression_url",
          click_url: "noMerinoScore click_url",
          block_id: 111,
          advertiser: "noMerinoScore advertiser",
          is_sponsored: true,
          // no score
        },
      ],
    },
  });

  let context = createContext(REMOTE_SETTINGS_SEARCH_STRING, {
    providers: [UrlbarProviderQuickSuggest.name],
    isPrivate: false,
  });
  await check_results({
    context,
    matches: [EXPECTED_REMOTE_SETTINGS_RESULT],
  });
});

// When remote settings doesn't return a suggestion but Merino does, the Merino
// suggestion should be used.
add_task(async function noSuggestion_remoteSettings() {
  UrlbarPrefs.set(PREF_MERINO_ENABLED, true);
  UrlbarPrefs.set(PREF_REMOTE_SETTINGS_ENABLED, true);

  setMerinoResponse({
    body: {
      request_id: "merinoOnly request_id",
      suggestions: [
        {
          full_keyword: "noSuggestion full_keyword",
          title: "noSuggestion title",
          url: "noSuggestion url",
          icon: "noSuggestion icon",
          impression_url: "noSuggestion impression_url",
          click_url: "noSuggestion click_url",
          block_id: 111,
          advertiser: "noSuggestion advertiser",
          is_sponsored: true,
          score: UrlbarQuickSuggest.SUGGESTION_SCORE / 2,
        },
      ],
    },
  });

  let context = createContext("this doesn't match remote settings", {
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
          qsSuggestion: "noSuggestion full_keyword",
          title: "noSuggestion title",
          url: "noSuggestion url",
          icon: "noSuggestion icon",
          sponsoredImpressionUrl: "noSuggestion impression_url",
          sponsoredClickUrl: "noSuggestion click_url",
          sponsoredBlockId: 111,
          sponsoredAdvertiser: "noSuggestion advertiser",
          isSponsored: true,
          helpUrl: UrlbarProviderQuickSuggest.helpUrl,
          helpL10nId: "firefox-suggest-urlbar-learn-more",
          displayUrl: "noSuggestion url",
          requestId: "merinoOnly request_id",
          source: "merino",
        },
      },
    ],
  });
});

// When Merino doesn't return a suggestion but remote settings does, the remote
// settings suggestion should be used.
add_task(async function noSuggestion_merino() {
  UrlbarPrefs.set(PREF_MERINO_ENABLED, true);
  UrlbarPrefs.set(PREF_REMOTE_SETTINGS_ENABLED, true);

  setMerinoResponse({
    body: {
      request_id: "merinoOnly request_id",
      suggestions: [],
    },
  });

  let context = createContext(REMOTE_SETTINGS_SEARCH_STRING, {
    providers: [UrlbarProviderQuickSuggest.name],
    isPrivate: false,
  });
  await check_results({
    context,
    matches: [EXPECTED_REMOTE_SETTINGS_RESULT],
  });
});

// Tests with both Merino and remote settings disabled.
add_task(async function bothDisabled() {
  UrlbarPrefs.set(PREF_MERINO_ENABLED, false);
  UrlbarPrefs.set(PREF_REMOTE_SETTINGS_ENABLED, false);

  // Make sure the server is prepared to return a response so we can make sure
  // we don't fetch it.
  setMerinoResponse({
    body: {
      request_id: "merinoOnly request_id",
      suggestions: [
        {
          full_keyword: "bothDisabled full_keyword",
          title: "bothDisabled title",
          url: "bothDisabled url",
          icon: "bothDisabled icon",
          impression_url: "bothDisabled impression_url",
          click_url: "bothDisabled click_url",
          block_id: 111,
          advertiser: "bothDisabled advertiser",
          is_sponsored: true,
          score: 1,
        },
      ],
    },
  });

  let context = createContext(REMOTE_SETTINGS_SEARCH_STRING, {
    providers: [UrlbarProviderQuickSuggest.name],
    isPrivate: false,
  });
  await check_results({ context, matches: [] });
});

// When Merino returns multiple suggestions, the one with the largest score
// should be used.
add_task(async function multipleMerinoSuggestions() {
  UrlbarPrefs.set(PREF_MERINO_ENABLED, true);
  UrlbarPrefs.set(PREF_REMOTE_SETTINGS_ENABLED, false);

  setMerinoResponse({
    body: {
      request_id: "merinoOnly request_id",
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
          icon: "multipleMerinoSuggestions 1 icon",
          sponsoredImpressionUrl: "multipleMerinoSuggestions 1 impression_url",
          sponsoredClickUrl: "multipleMerinoSuggestions 1 click_url",
          sponsoredBlockId: 1,
          sponsoredAdvertiser: "multipleMerinoSuggestions 1 advertiser",
          isSponsored: true,
          helpUrl: UrlbarProviderQuickSuggest.helpUrl,
          helpL10nId: "firefox-suggest-urlbar-learn-more",
          displayUrl: "multipleMerinoSuggestions 1 url",
          requestId: "merinoOnly request_id",
          source: "merino",
        },
      },
    ],
  });
});

// Checks a response that's valid but also has some unexpected properties.
add_task(async function unexpectedResponseProperties() {
  UrlbarPrefs.set(PREF_MERINO_ENABLED, true);
  UrlbarPrefs.set(PREF_REMOTE_SETTINGS_ENABLED, false);

  setMerinoResponse({
    body: {
      unexpectedString: "some value",
      unexpectedArray: ["a", "b", "c"],
      unexpectedObject: { foo: "bar" },
      request_id: "merinoOnly request_id",
      suggestions: [
        {
          full_keyword: "unexpected full_keyword",
          title: "unexpected title",
          url: "unexpected url",
          icon: "unexpected icon",
          impression_url: "unexpected impression_url",
          click_url: "unexpected click_url",
          block_id: 1234,
          advertiser: "unexpected advertiser",
          is_sponsored: true,
          score: 1,
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
          qsSuggestion: "unexpected full_keyword",
          title: "unexpected title",
          url: "unexpected url",
          icon: "unexpected icon",
          sponsoredImpressionUrl: "unexpected impression_url",
          sponsoredClickUrl: "unexpected click_url",
          sponsoredBlockId: 1234,
          sponsoredAdvertiser: "unexpected advertiser",
          isSponsored: true,
          helpUrl: UrlbarProviderQuickSuggest.helpUrl,
          helpL10nId: "firefox-suggest-urlbar-learn-more",
          displayUrl: "unexpected url",
          requestId: "merinoOnly request_id",
          source: "merino",
        },
      },
    ],
  });
});

// Checks some bad/unexpected responses.
add_task(async function badResponses() {
  UrlbarPrefs.set(PREF_MERINO_ENABLED, true);
  UrlbarPrefs.set(PREF_REMOTE_SETTINGS_ENABLED, false);

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

  setMerinoResponse({
    body: { bogus: [] },
  });
  context = createContext(...contextArgs);
  await check_results({ context, matches: [] });

  setMerinoResponse({
    body: { suggestions: {} },
  });
  context = createContext(...contextArgs);
  await check_results({ context, matches: [] });

  setMerinoResponse({
    body: { suggestions: [] },
  });
  context = createContext(...contextArgs);
  await check_results({ context, matches: [] });

  setMerinoResponse({
    body: "",
  });
  context = createContext(...contextArgs);
  await check_results({ context, matches: [] });

  setMerinoResponse({
    contentType: "text/html",
    body: "bogus",
  });
  context = createContext(...contextArgs);
  await check_results({ context, matches: [] });
});

// Tests the Merino latency stopwatch histogram.
add_task(async function latencyTelemetry() {
  UrlbarPrefs.set(PREF_MERINO_ENABLED, true);
  UrlbarPrefs.set(PREF_REMOTE_SETTINGS_ENABLED, false);

  let histogram = Services.telemetry.getHistogramById(TELEMETRY_MERINO_LATENCY);
  histogram.clear();

  setMerinoResponse({
    body: {
      request_id: "merinoOnly request_id",
      suggestions: [
        {
          full_keyword: "latencyTelemetry full_keyword",
          title: "latencyTelemetry title",
          url: "latencyTelemetry url",
          icon: "latencyTelemetry icon",
          impression_url: "latencyTelemetry impression_url",
          click_url: "latencyTelemetry click_url",
          block_id: 111,
          advertiser: "latencyTelemetry advertiser",
          is_sponsored: true,
        },
      ],
    },
  });

  let context = createContext("test", {
    providers: [UrlbarProviderQuickSuggest.name],
    isPrivate: false,
  });

  let snapshot = histogram.snapshot();
  Assert.equal(
    Object.values(snapshot.values).reduce((sum, value) => sum + value, 0),
    0,
    "Sanity check: No telemetry recorded before search"
  );

  await check_results({
    context,
    matches: [
      {
        type: UrlbarUtils.RESULT_TYPE.URL,
        source: UrlbarUtils.RESULT_SOURCE.SEARCH,
        heuristic: false,
        payload: {
          qsSuggestion: "latencyTelemetry full_keyword",
          title: "latencyTelemetry title",
          url: "latencyTelemetry url",
          icon: "latencyTelemetry icon",
          sponsoredImpressionUrl: "latencyTelemetry impression_url",
          sponsoredClickUrl: "latencyTelemetry click_url",
          sponsoredBlockId: 111,
          sponsoredAdvertiser: "latencyTelemetry advertiser",
          isSponsored: true,
          helpUrl: UrlbarProviderQuickSuggest.helpUrl,
          helpL10nId: "firefox-suggest-urlbar-learn-more",
          displayUrl: "latencyTelemetry url",
          requestId: "merinoOnly request_id",
          source: "merino",
        },
      },
    ],
  });

  snapshot = histogram.snapshot();
  Assert.greater(
    Object.values(snapshot.values).reduce((sum, value) => sum + value, 0),
    0,
    "Telemetry recorded after search"
  );
  Assert.ok(
    !TelemetryStopwatch.running(TELEMETRY_MERINO_LATENCY, context),
    "Stopwatch not running after search"
  );
});

// The Merino latency stopwatch histogram should not be updated when a search is
// canceled.
add_task(async function latencyTelemetryCancel() {
  UrlbarPrefs.set(PREF_MERINO_ENABLED, true);
  UrlbarPrefs.set(PREF_REMOTE_SETTINGS_ENABLED, false);

  let histogram = Services.telemetry.getHistogramById(TELEMETRY_MERINO_LATENCY);
  histogram.clear();

  // Make the server return a delayed response so we can cancel it before it
  // finishes.
  setMerinoResponse({
    delay: 3000,
    body: {
      request_id: "merinoOnly request_id",
      suggestions: [
        {
          full_keyword: "latencyTelemetryCancel full_keyword",
          title: "latencyTelemetryCancel title",
          url: "latencyTelemetryCancel url",
          icon: "latencyTelemetryCancel icon",
          impression_url: "latencyTelemetryCancel impression_url",
          click_url: "latencyTelemetryCancel click_url",
          block_id: 111,
          advertiser: "latencyTelemetryCancel advertiser",
          is_sponsored: true,
        },
      ],
    },
  });

  let context = createContext("test", {
    providers: [UrlbarProviderQuickSuggest.name],
    isPrivate: false,
  });

  let snapshot = histogram.snapshot();
  Assert.equal(
    Object.values(snapshot.values).reduce((sum, value) => sum + value, 0),
    0,
    "Sanity check: No telemetry recorded before search"
  );

  // Do a search but don't wait for it to finish.
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

  // Wait for the stopwatch to start running.
  await TestUtils.waitForCondition(
    () => TelemetryStopwatch.running(TELEMETRY_MERINO_LATENCY, context),
    "Waiting for stopwatch to start running"
  );

  // Now cancel the search.
  controller.cancelQuery();
  await searchPromise;

  // The telemetry should not be recorded.
  snapshot = histogram.snapshot();
  Assert.equal(
    Object.values(snapshot.values).reduce((sum, value) => sum + value, 0),
    0,
    "Telemetry not recorded after canceling search"
  );
  Assert.ok(
    !TelemetryStopwatch.running(TELEMETRY_MERINO_LATENCY, context),
    "Stopwatch not running after search"
  );
});

// The Merino latency stopwatch histogram should not be updated when there's an
// exception fetching the Merino server response. (This does *not* include 500
// responses from the server since in that case a response is still fetched.)
add_task(async function latencyTelemetryException() {
  UrlbarPrefs.set(PREF_MERINO_ENABLED, true);
  UrlbarPrefs.set(PREF_REMOTE_SETTINGS_ENABLED, false);

  // Set an invalid endpoint URL.
  let originalURL = UrlbarPrefs.get(PREF_MERINO_ENDPOINT_URL);
  UrlbarPrefs.set(PREF_MERINO_ENDPOINT_URL, "bogus");

  let histogram = Services.telemetry.getHistogramById(TELEMETRY_MERINO_LATENCY);
  histogram.clear();

  let context = createContext("test", {
    providers: [UrlbarProviderQuickSuggest.name],
    isPrivate: false,
  });

  let snapshot = histogram.snapshot();
  Assert.equal(
    Object.values(snapshot.values).reduce((sum, value) => sum + value, 0),
    0,
    "Sanity check: No telemetry recorded before search"
  );

  await check_results({
    context,
    matches: [],
  });

  // The telemetry should not be recorded.
  snapshot = histogram.snapshot();
  Assert.equal(
    Object.values(snapshot.values).reduce((sum, value) => sum + value, 0),
    0,
    "Telemetry not recorded after search with error"
  );
  Assert.ok(
    !TelemetryStopwatch.running(TELEMETRY_MERINO_LATENCY, context),
    "Stopwatch not running after search"
  );

  UrlbarPrefs.set(PREF_MERINO_ENDPOINT_URL, originalURL);
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
    resp.setHeader("Content-Type", gMerinoResponse.contentType, false);
    if (typeof gMerinoResponse.body == "string") {
      resp.write(gMerinoResponse.body);
    } else if (gMerinoResponse.body) {
      resp.write(JSON.stringify(gMerinoResponse.body));
    }
    resp.finish();
  });
  return server;
}

function setMerinoResponse(resp) {
  if (!resp.contentType) {
    resp.contentType = "application/json";
  }
  gMerinoResponse = resp;
}
