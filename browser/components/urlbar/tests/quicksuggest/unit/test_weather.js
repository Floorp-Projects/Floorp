/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

// Tests the quick suggest weather feature.

"use strict";

ChromeUtils.defineESModuleGetters(this, {
  UrlbarProviderWeather: "resource:///modules/UrlbarProviderWeather.sys.mjs",
});

const HISTOGRAM_LATENCY = "FX_URLBAR_MERINO_LATENCY_WEATHER_MS";
const HISTOGRAM_RESPONSE = "FX_URLBAR_MERINO_RESPONSE_WEATHER";

const { WEATHER_SUGGESTION } = MerinoTestUtils;

add_task(async function init() {
  await QuickSuggestTestUtils.ensureQuickSuggestInit();
  UrlbarPrefs.set("quicksuggest.enabled", true);

  await MerinoTestUtils.initWeather();
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
    providers: [UrlbarProviderWeather.name],
    isPrivate: false,
  });
  await check_results({
    context,
    matches: [],
  });

  let histograms = MerinoTestUtils.getAndClearHistograms({
    extraLatency: HISTOGRAM_LATENCY,
    extraResponse: HISTOGRAM_RESPONSE,
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

  Assert.equal(
    QuickSuggest.weather._test_merino.lastFetchStatus,
    "success",
    "The request successfully finished"
  );
  MerinoTestUtils.checkAndClearHistograms({
    histograms,
    response: "success",
    latencyRecorded: true,
    client: QuickSuggest.weather._test_merino,
  });

  // The suggestion should be returned for a search.
  context = createContext("", {
    providers: [UrlbarProviderWeather.name],
    isPrivate: false,
  });
  await check_results({
    context,
    matches: [makeExpectedResult()],
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

// A fetch that doesn't return a suggestion should cause the last-fetched
// suggestion to be discarded.
add_task(async function noSuggestion() {
  assertEnabled({
    message: "Sanity check initial state",
    hasSuggestion: true,
    pendingFetchCount: 0,
  });

  let histograms = MerinoTestUtils.getAndClearHistograms({
    extraLatency: HISTOGRAM_LATENCY,
    extraResponse: HISTOGRAM_RESPONSE,
  });

  let { suggestions } = MerinoTestUtils.server.response.body;
  MerinoTestUtils.server.response.body.suggestions = [];

  await QuickSuggest.weather._test_fetch();

  assertEnabled({
    message: "After fetch",
    hasSuggestion: false,
    pendingFetchCount: 0,
  });
  Assert.equal(
    QuickSuggest.weather._test_merino.lastFetchStatus,
    "no_suggestion",
    "The request successfully finished without suggestions"
  );
  MerinoTestUtils.checkAndClearHistograms({
    histograms,
    response: "no_suggestion",
    latencyRecorded: true,
    client: QuickSuggest.weather._test_merino,
  });

  let context = createContext("", {
    providers: [UrlbarProviderWeather.name],
    isPrivate: false,
  });
  await check_results({
    context,
    matches: [],
  });

  MerinoTestUtils.server.response.body.suggestions = suggestions;

  // Clean up by forcing another fetch so the suggestion is non-null for the
  // remaining tasks.
  await QuickSuggest.weather._test_fetch();
  assertEnabled({
    message: "On cleanup",
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

  let histograms = MerinoTestUtils.getAndClearHistograms({
    extraLatency: HISTOGRAM_LATENCY,
    extraResponse: HISTOGRAM_RESPONSE,
  });

  // Set the weather fetch timeout high enough that the network error exception
  // will happen first. See `MerinoTestUtils.withNetworkError()`.
  QuickSuggest.weather._test_setTimeoutMs(10000);

  await MerinoTestUtils.server.withNetworkError(async () => {
    await QuickSuggest.weather._test_fetch();
  });

  QuickSuggest.weather._test_setTimeoutMs(-1);

  assertEnabled({
    message: "After fetch",
    hasSuggestion: false,
    pendingFetchCount: 0,
  });
  Assert.equal(
    QuickSuggest.weather._test_merino.lastFetchStatus,
    "network_error",
    "The request failed with a network error"
  );
  MerinoTestUtils.checkAndClearHistograms({
    histograms,
    response: "network_error",
    latencyRecorded: false,
    client: QuickSuggest.weather._test_merino,
  });

  let context = createContext("", {
    providers: [UrlbarProviderWeather.name],
    isPrivate: false,
  });
  await check_results({
    context,
    matches: [],
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

  let histograms = MerinoTestUtils.getAndClearHistograms({
    extraLatency: HISTOGRAM_LATENCY,
    extraResponse: HISTOGRAM_RESPONSE,
  });

  MerinoTestUtils.server.response = { status: 500 };
  await QuickSuggest.weather._test_fetch();

  assertEnabled({
    message: "After fetch",
    hasSuggestion: false,
    pendingFetchCount: 0,
  });
  Assert.equal(
    QuickSuggest.weather._test_merino.lastFetchStatus,
    "http_error",
    "The request failed with an HTTP error"
  );
  MerinoTestUtils.checkAndClearHistograms({
    histograms,
    response: "http_error",
    latencyRecorded: true,
    client: QuickSuggest.weather._test_merino,
  });

  let context = createContext("", {
    providers: [UrlbarProviderWeather.name],
    isPrivate: false,
  });
  await check_results({
    context,
    matches: [],
  });

  // Clean up by forcing another fetch so the suggestion is non-null for the
  // remaining tasks.
  MerinoTestUtils.server.reset();
  MerinoTestUtils.server.response.body.suggestions = [WEATHER_SUGGESTION];
  await QuickSuggest.weather._test_fetch();
  assertEnabled({
    message: "On cleanup",
    hasSuggestion: true,
    pendingFetchCount: 0,
  });
});

// A fetch that doesn't return a suggestion due to a client timeout should cause
// the last-fetched suggestion to be discarded.
add_task(async function clientTimeout() {
  assertEnabled({
    message: "Sanity check initial state",
    hasSuggestion: true,
    pendingFetchCount: 0,
  });

  let histograms = MerinoTestUtils.getAndClearHistograms({
    extraLatency: HISTOGRAM_LATENCY,
    extraResponse: HISTOGRAM_RESPONSE,
  });

  // Make the server return a delayed response so the Merino client times out
  // waiting for it.
  MerinoTestUtils.server.response.delay = 400;

  // Make the client time out immediately.
  QuickSuggest.weather._test_setTimeoutMs(1);

  // Set up a promise that will be resolved when the client finally receives the
  // response.
  let responsePromise = QuickSuggest.weather._test_merino.waitForNextResponse();

  await QuickSuggest.weather._test_fetch();

  assertEnabled({
    message: "After fetch",
    hasSuggestion: false,
    pendingFetchCount: 0,
  });
  Assert.equal(
    QuickSuggest.weather._test_merino.lastFetchStatus,
    "timeout",
    "The request timed out"
  );
  MerinoTestUtils.checkAndClearHistograms({
    histograms,
    response: "timeout",
    latencyRecorded: false,
    latencyStopwatchRunning: true,
    client: QuickSuggest.weather._test_merino,
  });

  let context = createContext("", {
    providers: [UrlbarProviderWeather.name],
    isPrivate: false,
  });
  await check_results({
    context,
    matches: [],
  });

  // Await the response.
  await responsePromise;

  // The `checkAndClearHistograms()` call above cleared the histograms. After
  // that, nothing else should have been recorded for the response.
  MerinoTestUtils.checkAndClearHistograms({
    histograms,
    response: null,
    latencyRecorded: true,
    client: QuickSuggest.weather._test_merino,
  });

  QuickSuggest.weather._test_setTimeoutMs(-1);
  delete MerinoTestUtils.server.response.delay;

  // Clean up by forcing another fetch so the suggestion is non-null for the
  // remaining tasks.
  await QuickSuggest.weather._test_fetch();
  assertEnabled({
    message: "On cleanup",
    hasSuggestion: true,
    pendingFetchCount: 0,
  });
});

// Locale task for when this test runs on an en-US OS.
add_task(async function locale_enUS() {
  await doLocaleTest({
    shouldRunTask: osLocale => osLocale == "en-US",
    osUnit: "f",
    unitsByLocale: {
      "en-US": "f",
      // When the app's locale is set to any en-* locale, F will be used because
      // `regionalPrefsLocales` will prefer the en-US OS locale.
      "en-CA": "f",
      "en-GB": "f",
      de: "c",
    },
  });
});

// Locale task for when this test runs on a non-US English OS.
add_task(async function locale_nonUSEnglish() {
  await doLocaleTest({
    shouldRunTask: osLocale => osLocale.startsWith("en") && osLocale != "en-US",
    osUnit: "c",
    unitsByLocale: {
      // When the app's locale is set to en-US, C will be used because
      // `regionalPrefsLocales` will prefer the non-US English OS locale.
      "en-US": "c",
      "en-CA": "c",
      "en-GB": "c",
      de: "c",
    },
  });
});

// Locale task for when this test runs on a non-English OS.
add_task(async function locale_nonEnglish() {
  await doLocaleTest({
    shouldRunTask: osLocale => !osLocale.startsWith("en"),
    osUnit: "c",
    unitsByLocale: {
      "en-US": "f",
      "en-CA": "c",
      "en-GB": "c",
      de: "c",
    },
  });
});

// Search strings with only spaces or that start with spaces should not trigger
// a weather suggestion.
add_task(async function spacesInSearchString() {
  // Sanity check initial state.
  assertEnabled({
    message: "Sanity check initial state",
    hasSuggestion: true,
    pendingFetchCount: 0,
  });

  for (let searchString of [" ", "  ", "   ", " doesn't match anything"]) {
    await check_results({
      context: createContext(searchString, {
        providers: [UrlbarProviderWeather.name],
        isPrivate: false,
      }),
      matches: [],
    });
  }
});

/**
 * Testing locales is tricky due to the weather feature's use of
 * `Services.locale.regionalPrefsLocales`. By default `regionalPrefsLocales`
 * prefers the OS locale if its language is the same as the app locale's
 * language; otherwise it prefers the app locale. For example, assuming the OS
 * locale is en-CA, then if the app locale is en-US it will prefer en-CA since
 * both are English, but if the app locale is de it will prefer de. If the pref
 * `intl.regional_prefs.use_os_locales` is set, then the OS locale is always
 * preferred.
 *
 * This function tests a given set of locales with and without
 * `intl.regional_prefs.use_os_locales` set.
 *
 * @param {object} options
 *   Options
 * @param {Function} options.shouldRunTask
 *   Called with the OS locale. Should return true if the function should run.
 *   Use this to skip tasks that don't target a desired OS locale.
 * @param {string} options.osUnit
 *   The expected "c" or "f" unit for the OS locale.
 * @param {object} options.unitsByLocale
 *   The expected "c" or "f" unit when the app's locale is set to particular
 *   locales. This should be an object that maps locales to expected units. For
 *   each locale in the object, the app's locale is set to that locale and the
 *   actual unit is expected to be the unit in the object.
 */
async function doLocaleTest({ shouldRunTask, osUnit, unitsByLocale }) {
  Services.prefs.setBoolPref("intl.regional_prefs.use_os_locales", true);
  let osLocale = Services.locale.regionalPrefsLocales[0];
  Services.prefs.clearUserPref("intl.regional_prefs.use_os_locales");

  if (!shouldRunTask(osLocale)) {
    info("Skipping task, should not run for this OS locale");
    return;
  }

  assertEnabled({
    message: "Sanity check initial state",
    hasSuggestion: true,
    pendingFetchCount: 0,
  });

  // Sanity check initial locale info.
  Assert.equal(
    Services.locale.appLocaleAsBCP47,
    "en-US",
    "Initial app locale should be en-US"
  );
  Assert.ok(
    !Services.prefs.getBoolPref("intl.regional_prefs.use_os_locales"),
    "intl.regional_prefs.use_os_locales should be false initially"
  );

  // Check locales.
  for (let [locale, temperatureUnit] of Object.entries(unitsByLocale)) {
    await QuickSuggestTestUtils.withLocales([locale], async () => {
      info("Checking locale: " + locale);
      await check_results({
        context: createContext("", {
          providers: [UrlbarProviderWeather.name],
          isPrivate: false,
        }),
        matches: [makeExpectedResult({ temperatureUnit })],
      });

      info(
        "Checking locale with intl.regional_prefs.use_os_locales: " + locale
      );
      Services.prefs.setBoolPref("intl.regional_prefs.use_os_locales", true);
      await check_results({
        context: createContext("", {
          providers: [UrlbarProviderWeather.name],
          isPrivate: false,
        }),
        matches: [makeExpectedResult({ temperatureUnit: osUnit })],
      });
      Services.prefs.clearUserPref("intl.regional_prefs.use_os_locales");
    });
  }
}

// A weather suggestion should not be returned for a non-empty search string.
add_task(async function nonEmptySearchString() {
  assertEnabled({
    message: "Sanity check initial state",
    hasSuggestion: true,
    pendingFetchCount: 0,
  });

  // Do a search.
  let context = createContext("this shouldn't match anything", {
    providers: [UrlbarProviderWeather.name],
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
    providers: [UrlbarProviderWeather.name],
    isPrivate: false,
  });
  await check_results({
    context,
    matches: [makeExpectedResult()],
  });

  // Block the result.
  UrlbarProviderWeather.blockResult(context, context.results[0]);
  Assert.ok(
    !UrlbarPrefs.get("suggest.weather"),
    "suggest.weather is false after blocking the result"
  );

  // Do a second search. Nothing should be returned.
  context = createContext("", {
    providers: [UrlbarProviderWeather.name],
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

// When `weather.zeroPrefix` is false, weather suggestions should be triggered
// by keywords.
add_task(async function keywords() {
  // Sanity check initial state.
  assertEnabled({
    message: "Sanity check initial state",
    hasSuggestion: true,
    pendingFetchCount: 0,
  });

  await QuickSuggestTestUtils.withConfig({
    config: {
      weather_keywords: ["wea", "weather"],
    },
    callback: async () => {
      // Map from search string -> whether the result should be triggered when
      // `weather.zeroPrefix` is false
      let tests = {
        wea: true,
        " wea": true,
        "     wea": true,
        weather: true,
        " weather": true,
        "     weather": true,

        "": false,
        " ": false,
        "     ": false,
        w: false,
        we: false,
        weat: false,
        weath: false,
        weathe: false,
        "weather ": false,
        " weather ": false,
      };

      for (let [searchString, expected] of Object.entries(tests)) {
        // First do a search with `weather.zeroPrefix` set to true. Only the
        // empty string should trigger the result even if the search string
        // matches a keyword in the config.
        info(
          "Doing first search with weather.zeroPrefix = true: " +
            JSON.stringify(searchString)
        );
        UrlbarPrefs.set("weather.zeroPrefix", true);
        await check_results({
          context: createContext(searchString, {
            providers: [UrlbarProviderWeather.name],
            isPrivate: false,
          }),
          matches: !searchString ? [makeExpectedResult()] : [],
        });

        // Do a second search with `weather.zeroPrefix` set to false and check
        // the test case.
        info(
          "Doing second search with weather.zeroPrefix = false: " +
            JSON.stringify(searchString)
        );
        UrlbarPrefs.set("weather.zeroPrefix", false);
        await check_results({
          context: createContext(searchString, {
            providers: [UrlbarProviderWeather.name],
            isPrivate: false,
          }),
          matches: expected ? [makeExpectedResult({ suggestedIndex: 1 })] : [],
        });
      }
    },
  });

  UrlbarPrefs.clear("weather.zeroPrefix");
});

// When a sponsored quick suggest result matches the same keyword as the weather
// result, the weather result should be shown and the quick suggest result
// should not be shown.
add_task(async function matchingQuickSuggest_sponsored() {
  await doMatchingQuickSuggestTest("suggest.quicksuggest.sponsored", true);
});

// When a non-sponsored quick suggest result matches the same keyword as the
// weather result, the weather result should be shown and the quick suggest
// result should not be shown.
add_task(async function matchingQuickSuggest_nonsponsored() {
  await doMatchingQuickSuggestTest("suggest.quicksuggest.nonsponsored", false);
});

async function doMatchingQuickSuggestTest(pref, isSponsored) {
  // Sanity check initial state.
  assertEnabled({
    message: "Sanity check initial state",
    hasSuggestion: true,
    pendingFetchCount: 0,
  });

  let keyword = "test";
  let iab_category = isSponsored ? "22 - Shopping" : "5 - Education";

  // Add a remote settings result to quick suggest.
  UrlbarPrefs.set(pref, true);
  await QuickSuggestTestUtils.setRemoteSettingsResults([
    {
      id: 1,
      url: "http://example.com/",
      title: "Suggestion",
      keywords: [keyword],
      click_url: "http://example.com/click",
      impression_url: "http://example.com/impression",
      advertiser: "TestAdvertiser",
      iab_category,
    },
  ]);

  await QuickSuggestTestUtils.withConfig({
    config: {
      weather_keywords: [keyword],
    },
    callback: async () => {
      // First do a search with `weather.zeroPrefix` set to true to verify the
      // quick suggest result matches the keyword.
      info("Doing first search with weather.zeroPrefix = true");
      UrlbarPrefs.set("weather.zeroPrefix", true);
      await check_results({
        context: createContext(keyword, {
          providers: [
            UrlbarProviderQuickSuggest.name,
            UrlbarProviderWeather.name,
          ],
          isPrivate: false,
        }),
        matches: [
          {
            type: UrlbarUtils.RESULT_TYPE.URL,
            source: UrlbarUtils.RESULT_SOURCE.SEARCH,
            heuristic: false,
            payload: {
              qsSuggestion: keyword,
              title: "Suggestion",
              url: "http://example.com/",
              displayUrl: "http://example.com",
              originalUrl: "http://example.com/",
              icon: null,
              sponsoredImpressionUrl: "http://example.com/impression",
              sponsoredClickUrl: "http://example.com/click",
              sponsoredBlockId: 1,
              sponsoredAdvertiser: "TestAdvertiser",
              sponsoredIabCategory: iab_category,
              isSponsored,
              helpUrl: QuickSuggest.HELP_URL,
              helpL10n: {
                id: UrlbarPrefs.get("resultMenu")
                  ? "urlbar-result-menu-learn-more-about-firefox-suggest"
                  : "firefox-suggest-urlbar-learn-more",
              },
              isBlockable: false,
              blockL10n: {
                id: UrlbarPrefs.get("resultMenu")
                  ? "urlbar-result-menu-dismiss-firefox-suggest"
                  : "firefox-suggest-urlbar-block",
              },
              source: "remote-settings",
            },
          },
        ],
      });

      // Do a second search with `weather.zeroPrefix` set to false to verify
      // only the weather result is shown.
      info("Doing second search with weather.zeroPrefix = false");
      UrlbarPrefs.set("weather.zeroPrefix", false);
      await check_results({
        context: createContext(keyword, {
          providers: [
            UrlbarProviderQuickSuggest.name,
            UrlbarProviderWeather.name,
          ],
          isPrivate: false,
        }),
        matches: [makeExpectedResult({ suggestedIndex: 1 })],
      });
    },
  });

  UrlbarPrefs.clear("weather.zeroPrefix");
  UrlbarPrefs.clear(pref);
}

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

function makeExpectedResult({
  suggestedIndex = 0,
  temperatureUnit = undefined,
} = {}) {
  if (!temperatureUnit) {
    temperatureUnit =
      Services.locale.regionalPrefsLocales[0] == "en-US" ? "f" : "c";
  }

  return {
    suggestedIndex,
    type: UrlbarUtils.RESULT_TYPE.DYNAMIC,
    source: UrlbarUtils.RESULT_SOURCE.SEARCH,
    heuristic: false,
    payload: {
      temperatureUnit,
      url: WEATHER_SUGGESTION.url,
      iconId: "6",
      helpUrl: QuickSuggest.HELP_URL,
      helpL10n: {
        id: UrlbarPrefs.get("resultMenu")
          ? "urlbar-result-menu-learn-more-about-firefox-suggest"
          : "firefox-suggest-urlbar-learn-more",
      },
      isBlockable: true,
      blockL10n: {
        id: UrlbarPrefs.get("resultMenu")
          ? "urlbar-result-menu-dismiss-firefox-suggest"
          : "firefox-suggest-urlbar-block",
      },
      requestId: MerinoTestUtils.server.response.body.request_id,
      source: "merino",
      merinoProvider: "accuweather",
      dynamicType: "weather",
      city: WEATHER_SUGGESTION.city_name,
      temperature:
        WEATHER_SUGGESTION.current_conditions.temperature[temperatureUnit],
      currentConditions: WEATHER_SUGGESTION.current_conditions.summary,
      forecast: WEATHER_SUGGESTION.forecast.summary,
      high: WEATHER_SUGGESTION.forecast.high[temperatureUnit],
      low: WEATHER_SUGGESTION.forecast.low[temperatureUnit],
      shouldNavigate: true,
    },
  };
}
