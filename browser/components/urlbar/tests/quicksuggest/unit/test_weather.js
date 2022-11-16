/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

// Tests quick suggest weather results.

"use strict";

ChromeUtils.defineESModuleGetters(this, {
  setTimeout: "resource://gre/modules/Timer.sys.mjs",
});

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

  // Enabling weather will trigger a fetch. Wait for it to finish so the
  // suggestion is ready when the remaining tasks begin.
  let fetchPromise = QuickSuggest.weather.waitForNextFetch();
  UrlbarPrefs.set("quicksuggest.enabled", true);
  UrlbarPrefs.set("weather.featureGate", true);
  UrlbarPrefs.set("suggest.weather", true);
  await fetchPromise;
});

// The feature should be properly uninitialized when it's disabled and then
// re-initialized when it's re-enabled. This task disables the feature using the
// feature gate pref.
add_task(async function disableAndEnable_featureGate() {
  await doDisableAndEnableTest("weather.featureGate");
});

// The feature should be properly uninitialized when it's disabled and then
// re-initialized when it's re-enabled. This task disables the feature using the
// suggest pref.
add_task(async function disableAndEnable_suggestPref() {
  await doDisableAndEnableTest("suggest.weather");
});

async function doDisableAndEnableTest(pref) {
  // A suggestion should have already been fetched initially.
  Assert.ok(
    QuickSuggest.weather.suggestion,
    "Suggestion is non-null initially"
  );
  Assert.notEqual(
    QuickSuggest.weather._test_fetchInterval,
    0,
    "Fetch interval is non-zero initially"
  );
  Assert.ok(
    QuickSuggest.weather._test_merino,
    "Merino client is non-null initially"
  );

  // Disable the feature. It should be immediately uninitialized.
  UrlbarPrefs.set(pref, false);
  Assert.strictEqual(
    QuickSuggest.weather.suggestion,
    null,
    "Suggestion is null after disabling"
  );
  Assert.strictEqual(
    QuickSuggest.weather._test_fetchInterval,
    0,
    "Fetch interval is zero after disabling"
  );
  Assert.strictEqual(
    QuickSuggest.weather._test_merino,
    null,
    "Merino client is null after disabling"
  );

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
  let fetchPromise = QuickSuggest.weather.waitForNextFetch();
  UrlbarPrefs.set(pref, true);
  Assert.notEqual(
    QuickSuggest.weather._test_fetchInterval,
    0,
    "Fetch interval is non-zero after re-enabling"
  );
  Assert.ok(
    QuickSuggest.weather._test_merino,
    "Merino client is non-null after re-enabling"
  );
  await fetchPromise;

  Assert.ok(
    QuickSuggest.weather.suggestion,
    "Suggestion is non-null after re-enabling"
  );

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

// A network error should cause the last-fetched suggestion to be discarded.
add_task(async function networkError() {
  Assert.ok(
    QuickSuggest.weather.suggestion,
    "Suggestion is non-null initially"
  );

  await MerinoTestUtils.server.withNetworkError(async () => {
    await QuickSuggest.weather._test_fetch();
    Assert.strictEqual(
      QuickSuggest.weather.suggestion,
      null,
      "Suggestion is null"
    );
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
  Assert.ok(QuickSuggest.weather.suggestion, "Suggestion is non-null");
});

// An HTTP error should cause the last-fetched suggestion to be discarded.
add_task(async function httpError() {
  Assert.ok(QuickSuggest.weather.suggestion, "Suggestion is non-null");

  MerinoTestUtils.server.response = { status: 500 };
  await QuickSuggest.weather._test_fetch();

  Assert.strictEqual(
    QuickSuggest.weather.suggestion,
    null,
    "Suggestion is null"
  );

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
  Assert.ok(QuickSuggest.weather.suggestion, "Suggestion is non-null");
});

// Tests the fetch interval in real time.
add_task(async function realTimeInterval() {
  // Disable the feature so the interval is cleared and a new interval period
  // can be set.
  let intervalMs = 100;
  UrlbarPrefs.set("weather.featureGate", false);
  QuickSuggest.weather._test_setFetchIntervalMs(intervalMs);

  // Re-enable it with the new interval period and wait for the initial fetch.
  UrlbarPrefs.set("weather.featureGate", true);

  // eslint-disable-next-line mozilla/no-arbitrary-setTimeout
  await new Promise(r => setTimeout(r, 2 * intervalMs));

  Assert.ok(QuickSuggest.weather.suggestion, "Suggestion is non-null");

  // Disable it again. The suggestion should be immediately nulled.
  UrlbarPrefs.set("weather.featureGate", false);
  Assert.strictEqual(
    QuickSuggest.weather.suggestion,
    null,
    "Suggestion is null"
  );

  // Wait until the interval period elapses. The interval should have been
  // cleared, so no suggestion should have been fetched.
  // eslint-disable-next-line mozilla/no-arbitrary-setTimeout
  await new Promise(r => setTimeout(r, 200));
  Assert.strictEqual(
    QuickSuggest.weather.suggestion,
    null,
    "Suggestion remains null"
  );

  // Clean up by resetting the interval period and forcing another fetch so the
  // suggestion is non-null for the remaining tasks.
  QuickSuggest.weather._test_setFetchIntervalMs(-1);
  let fetchPromise = QuickSuggest.weather.waitForNextFetch();
  UrlbarPrefs.set("weather.featureGate", true);
  await fetchPromise;
  Assert.ok(QuickSuggest.weather.suggestion, "Suggestion is non-null");
});

// The UrlbarResult should use the temperature unit appropriate for the user's
// locale: F for en-US and C for everything else.
add_task(async function localeTemperatureUnit() {
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
  // A suggestion should have already been fetched initially.
  Assert.ok(
    QuickSuggest.weather.suggestion,
    "Suggestion is non-null initially"
  );

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
      helpL10nId: "firefox-suggest-urlbar-learn-more",
      requestId: MerinoTestUtils.server.response.body.request_id,
      source: "merino",
    },
  };
}
