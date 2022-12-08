/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

// Tests the quick suggest weather feature.

"use strict";

// We set the fetch interval to an absurdly large value so it absolutely will
// not fire during the test.
const TEST_FETCH_INTERVAL_MS = 24 * 60 * 60 * 1000; // 24 hours

const MERINO_SUGGESTION = {
  title: "Weather for San Francisco",
  url:
    "http://www.accuweather.com/en/us/san-francisco-ca/94103/current-weather/39376_pc?lang=en-us",
  provider: "accuweather",
  is_sponsored: false,
  score: 0.2,
  icon: null,
  city_name: "San Francisco",
  current_conditions: {
    url:
      "http://www.accuweather.com/en/us/san-francisco-ca/94103/current-weather/39376_pc?lang=en-us",
    summary: "Mostly cloudy",
    icon_id: 6,
    temperature: { c: 15.5, f: 60.0 },
  },
  forecast: {
    url:
      "http://www.accuweather.com/en/us/san-francisco-ca/94103/daily-weather-forecast/39376_pc?lang=en-us",
    summary: "Pleasant Saturday",
    high: { c: 21.1, f: 70.0 },
    low: { c: 13.9, f: 57.0 },
  },
};

const EXPECTED_RESULT = makeExpectedResult("f");

add_task(async function init() {
  await QuickSuggestTestUtils.ensureQuickSuggestInit();
  await MerinoTestUtils.server.start();
  MerinoTestUtils.server.response.body.suggestions = [MERINO_SUGGESTION];

  QuickSuggest.weather._test_setFetchIntervalMs(TEST_FETCH_INTERVAL_MS);

  // Enabling weather will trigger a fetch. Wait for it to finish so the
  // suggestion is ready when the remaining tasks begin.
  let fetchPromise = QuickSuggest.weather.waitForFetches();
  UrlbarPrefs.set("quicksuggest.enabled", true);
  UrlbarPrefs.set("weather.featureGate", true);
  UrlbarPrefs.set("suggest.weather", true);
  await fetchPromise;

  Assert.equal(
    QuickSuggest.weather._test_pendingFetchCount,
    0,
    "No pending fetches after awaiting initial fetch"
  );
});

// The feature should be properly uninitialized when it's disabled and then
// re-initialized when it's re-enabled. This task disables the feature using the
// feature gate pref.
add_task(async function disableAndEnable_featureGate() {
  await doBasicDisableAndEnableTest("weather.featureGate");
});

// The feature should be properly uninitialized when it's disabled and then
// re-initialized when it's re-enabled. This task disables the feature using the
// suggest pref.
add_task(async function disableAndEnable_suggestPref() {
  await doBasicDisableAndEnableTest("suggest.weather");
});

async function doBasicDisableAndEnableTest(pref) {
  // Sanity check initial state.
  assertEnabled({
    message: "Sanity check initial state",
    hasSuggestion: true,
    pendingFetchCount: 0,
  });

  // Disable the feature. It should be immediately uninitialized.
  UrlbarPrefs.set(pref, false);
  assertDisabled({
    message: "After disabling",
    pendingFetchCount: 0,
  });

  // No suggestion should be returned for a search.
  let context = createContext("", {
    providers: [UrlbarProviderQuickSuggest.name],
    isPrivate: false,
  });
  await check_results({
    context,
    matches: [],
  });

  // Re-enable the feature. It should be immediately initialized and a fetch
  // should start.
  info("Re-enable the feature");
  let fetchPromise = QuickSuggest.weather.waitForFetches();
  UrlbarPrefs.set(pref, true);
  assertEnabled({
    message: "Immediately after re-enabling",
    hasSuggestion: false,
    pendingFetchCount: 1,
  });

  await fetchPromise;
  assertEnabled({
    message: "After awaiting fetch",
    hasSuggestion: true,
    pendingFetchCount: 0,
  });

  // The suggestion should be returned for a search.
  context = createContext("", {
    providers: [UrlbarProviderQuickSuggest.name],
    isPrivate: false,
  });
  await check_results({
    context,
    matches: [EXPECTED_RESULT],
  });
}

// Disables and re-enables the feature without waiting for any intermediate
// fetches to complete, using the following steps:
//
// 1. Disable
// 2. Enable
// 3. Disable again
//
// At this point, the fetch from step 2 will remain ongoing but once it finishes
// it should be discarded since the feature is disabled.
add_task(async function disableAndEnable_immediate1() {
  // Sanity check initial state.
  assertEnabled({
    message: "Sanity check initial state",
    hasSuggestion: true,
    pendingFetchCount: 0,
  });

  // Disable the feature. It should be immediately uninitialized.
  UrlbarPrefs.set("weather.featureGate", false);
  assertDisabled({
    message: "After disabling",
    pendingFetchCount: 0,
  });

  // Re-enable the feature. It should be immediately initialized and a fetch
  // should start.
  let fetchPromise = QuickSuggest.weather.waitForFetches();
  UrlbarPrefs.set("weather.featureGate", true);
  assertEnabled({
    message: "Immediately after re-enabling",
    hasSuggestion: false,
    pendingFetchCount: 1,
  });

  // Disable it again. The fetch will remain ongoing since pending fetches
  // aren't stopped when the feature is disabled.
  UrlbarPrefs.set("weather.featureGate", false);
  assertDisabled({
    message: "After disabling again",
    pendingFetchCount: 1,
  });

  // Wait for the fetch to finish.
  await fetchPromise;

  // The fetched suggestion should be discarded and the feature should remain
  // uninitialized.
  assertDisabled({
    message: "After awaiting fetch",
    pendingFetchCount: 0,
  });

  // Clean up by re-enabling the feature for the remaining tasks.
  fetchPromise = QuickSuggest.weather.waitForFetches();
  UrlbarPrefs.set("weather.featureGate", true);
  await fetchPromise;
  assertEnabled({
    message: "On cleanup",
    hasSuggestion: true,
    pendingFetchCount: 0,
  });
});

// Disables and re-enables the feature without waiting for any intermediate
// fetches to complete, using the following steps:
//
// 1. Disable
// 2. Enable
// 3. Disable again
// 4. Enable again
//
// At this point, the fetches from steps 2 and 4 will remain ongoing. The fetch
// from step 2 should be discarded.
add_task(async function disableAndEnable_immediate2() {
  // Sanity check initial state.
  assertEnabled({
    message: "Sanity check initial state",
    hasSuggestion: true,
    pendingFetchCount: 0,
  });

  // Disable the feature. It should be immediately uninitialized.
  UrlbarPrefs.set("weather.featureGate", false);
  assertDisabled({
    message: "After disabling",
    pendingFetchCount: 0,
  });

  // Re-enable the feature. It should be immediately initialized and a fetch
  // should start.
  UrlbarPrefs.set("weather.featureGate", true);
  assertEnabled({
    message: "Immediately after re-enabling",
    hasSuggestion: false,
    pendingFetchCount: 1,
  });

  // Disable it again. The fetch will remain ongoing since pending fetches
  // aren't stopped when the feature is disabled.
  UrlbarPrefs.set("weather.featureGate", false);
  assertDisabled({
    message: "After disabling again",
    pendingFetchCount: 1,
  });

  // Re-enable it. A new fetch should start, so now there will be two pending
  // fetches.
  let fetchPromise = QuickSuggest.weather.waitForFetches();
  UrlbarPrefs.set("weather.featureGate", true);
  assertEnabled({
    message: "Immediately after re-enabling again",
    hasSuggestion: false,
    pendingFetchCount: 2,
  });

  // Wait for both fetches to finish.
  await fetchPromise;
  assertEnabled({
    message: "Immediately after re-enabling again",
    hasSuggestion: true,
    pendingFetchCount: 0,
  });
});

// A network error should cause the last-fetched suggestion to be discarded.
add_task(async function networkError() {
  assertEnabled({
    message: "Sanity check initial state",
    hasSuggestion: true,
    pendingFetchCount: 0,
  });

  await MerinoTestUtils.server.withNetworkError(async () => {
    await QuickSuggest.weather._test_fetch();
    assertEnabled({
      message: "After fetch",
      hasSuggestion: false,
      pendingFetchCount: 0,
    });
    let context = createContext("", {
      providers: [UrlbarProviderQuickSuggest.name],
      isPrivate: false,
    });
    await check_results({
      context,
      matches: [],
    });
  });

  // Clean up by forcing another fetch so the suggestion is non-null for the
  // remaining tasks.
  await QuickSuggest.weather._test_fetch();
  assertEnabled({
    message: "On cleanup",
    hasSuggestion: true,
    pendingFetchCount: 0,
  });
});

// An HTTP error should cause the last-fetched suggestion to be discarded.
add_task(async function httpError() {
  assertEnabled({
    message: "Sanity check initial state",
    hasSuggestion: true,
    pendingFetchCount: 0,
  });

  MerinoTestUtils.server.response = { status: 500 };
  await QuickSuggest.weather._test_fetch();

  assertEnabled({
    message: "After fetch",
    hasSuggestion: false,
    pendingFetchCount: 0,
  });

  let context = createContext("", {
    providers: [UrlbarProviderQuickSuggest.name],
    isPrivate: false,
  });
  await check_results({
    context,
    matches: [],
  });

  // Clean up by forcing another fetch so the suggestion is non-null for the
  // remaining tasks.
  MerinoTestUtils.server.reset();
  MerinoTestUtils.server.response.body.suggestions = [MERINO_SUGGESTION];
  await QuickSuggest.weather._test_fetch();
  assertEnabled({
    message: "On cleanup",
    hasSuggestion: true,
    pendingFetchCount: 0,
  });
});

// The UrlbarResult should use the temperature unit appropriate for the user's
// locale: F for en-US and C for everything else.
add_task(async function localeTemperatureUnit() {
  assertEnabled({
    message: "Sanity check initial state",
    hasSuggestion: true,
    pendingFetchCount: 0,
  });

  let unitsByLocale = {
    "en-US": "f",
    "en-CA": "c",
    "en-GB": "c",
    de: "c",
  };
  for (let [locale, expectedUnit] of Object.entries(unitsByLocale)) {
    await QuickSuggestTestUtils.withLocales([locale], async () => {
      let context = createContext("", {
        providers: [UrlbarProviderQuickSuggest.name],
        isPrivate: false,
      });
      await check_results({
        context,
        matches: [makeExpectedResult(expectedUnit)],
      });
    });
  }
});

// A weather suggestion should not be returned for a non-empty search string.
add_task(async function nonEmptySearchString() {
  assertEnabled({
    message: "Sanity check initial state",
    hasSuggestion: true,
    pendingFetchCount: 0,
  });

  // Do a search.
  let context = createContext("this shouldn't match anything", {
    providers: [UrlbarProviderQuickSuggest.name],
    isPrivate: false,
  });
  await check_results({
    context,
    matches: [],
  });
});

// Blocks a result and makes sure the weather pref is disabled.
add_task(async function block() {
  // Sanity check initial state.
  assertEnabled({
    message: "Sanity check initial state",
    hasSuggestion: true,
    pendingFetchCount: 0,
  });
  Assert.ok(
    UrlbarPrefs.get("suggest.weather"),
    "Sanity check: suggest.weather is true initially"
  );

  // Do a search so we can get an actual result.
  let context = createContext("", {
    providers: [UrlbarProviderQuickSuggest.name],
    isPrivate: false,
  });
  await check_results({
    context,
    matches: [EXPECTED_RESULT],
  });

  // Block the result.
  UrlbarProviderQuickSuggest.blockResult(context, context.results[0]);
  Assert.ok(
    !UrlbarPrefs.get("suggest.weather"),
    "suggest.weather is false after blocking the result"
  );

  // Do a second search. Nothing should be returned.
  context = createContext("", {
    providers: [UrlbarProviderQuickSuggest.name],
    isPrivate: false,
  });
  await check_results({
    context,
    matches: [],
  });

  // Re-enable the pref and clean up.
  let fetchPromise = QuickSuggest.weather.waitForFetches();
  UrlbarPrefs.set("suggest.weather", true);
  await fetchPromise;
  assertEnabled({
    message: "On cleanup",
    hasSuggestion: true,
    pendingFetchCount: 0,
  });
});

function assertEnabled({ message, hasSuggestion, pendingFetchCount }) {
  info("Asserting feature is enabled");
  if (message) {
    info(message);
  }

  Assert.equal(
    !!QuickSuggest.weather.suggestion,
    hasSuggestion,
    "Suggestion is null or non-null as expected"
  );
  Assert.notEqual(
    QuickSuggest.weather._test_fetchInterval,
    0,
    "Fetch interval is non-zero"
  );
  Assert.ok(QuickSuggest.weather._test_merino, "Merino client is non-null");
  Assert.equal(
    QuickSuggest.weather._test_pendingFetchCount,
    pendingFetchCount,
    "Expected pending fetch count"
  );
}

function assertDisabled({ message, pendingFetchCount }) {
  info("Asserting feature is disabled");
  if (message) {
    info(message);
  }

  Assert.strictEqual(
    QuickSuggest.weather.suggestion,
    null,
    "Suggestion is null"
  );
  Assert.strictEqual(
    QuickSuggest.weather._test_fetchInterval,
    0,
    "Fetch interval is zero"
  );
  Assert.strictEqual(
    QuickSuggest.weather._test_merino,
    null,
    "Merino client is null"
  );
  Assert.equal(
    QuickSuggest.weather._test_pendingFetchCount,
    pendingFetchCount,
    "Expected pending fetch count"
  );
}

function makeExpectedResult(temperatureUnit) {
  return {
    type: UrlbarUtils.RESULT_TYPE.URL,
    source: UrlbarUtils.RESULT_SOURCE.OTHER_NETWORK,
    heuristic: false,
    payload: {
      title:
        MERINO_SUGGESTION.city_name +
        " • " +
        MERINO_SUGGESTION.current_conditions.temperature[temperatureUnit] +
        "° " +
        MERINO_SUGGESTION.current_conditions.summary +
        " • " +
        MERINO_SUGGESTION.forecast.summary +
        " • H " +
        MERINO_SUGGESTION.forecast.high[temperatureUnit] +
        "° • L " +
        MERINO_SUGGESTION.forecast.low[temperatureUnit] +
        "°",
      url: MERINO_SUGGESTION.url,
      icon: "chrome://global/skin/icons/highlights.svg",
      helpUrl: QuickSuggest.HELP_URL,
      helpL10n: { id: "firefox-suggest-urlbar-learn-more" },
      isBlockable: true,
      blockL10n: { id: "firefox-suggest-urlbar-block" },
      requestId: MerinoTestUtils.server.response.body.request_id,
      source: "merino",
      merinoProvider: "accuweather",
    },
  };
}
