/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

// Tests the quick suggest weather feature.

"use strict";

ChromeUtils.defineESModuleGetters(this, {
  MockRegistrar: "resource://testing-common/MockRegistrar.sys.mjs",
  UrlbarProviderWeather: "resource:///modules/UrlbarProviderWeather.sys.mjs",
});

const HISTOGRAM_LATENCY = "FX_URLBAR_MERINO_LATENCY_WEATHER_MS";
const HISTOGRAM_RESPONSE = "FX_URLBAR_MERINO_RESPONSE_WEATHER";

const { WEATHER_RS_DATA, WEATHER_SUGGESTION } = MerinoTestUtils;

add_task(async function init() {
  await QuickSuggestTestUtils.ensureQuickSuggestInit({
    remoteSettingsRecords: [
      {
        type: "weather",
        weather: WEATHER_RS_DATA,
      },
    ],
  });

  await MerinoTestUtils.initWeather();

  // Give this a small value so it doesn't delay the test too long. Choose a
  // value that's unlikely to be used anywhere else in the test so that when
  // `lastFetchTimeMs` is expected to be `fetchDelayAfterComingOnlineMs`, we can
  // be sure the value actually came from `fetchDelayAfterComingOnlineMs`.
  QuickSuggest.weather._test_fetchDelayAfterComingOnlineMs = 53;
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
  let context = createContext(MerinoTestUtils.WEATHER_KEYWORD, {
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

  // Wait for keywords to be re-synced from remote settings.
  await QuickSuggestTestUtils.forceSync();

  // The suggestion should be returned for a search.
  context = createContext(MerinoTestUtils.WEATHER_KEYWORD, {
    providers: [UrlbarProviderWeather.name],
    isPrivate: false,
  });
  await check_results({
    context,
    matches: [makeExpectedResult()],
  });
}

add_task(async function keywordsNotDefined() {
  // Sanity check initial state.
  assertEnabled({
    message: "Sanity check initial state",
    hasSuggestion: true,
    pendingFetchCount: 0,
  });

  // Set RS data without any keywords. Fetching should immediately stop.
  await QuickSuggestTestUtils.setRemoteSettingsRecords([
    {
      type: "weather",
      weather: {},
    },
  ]);
  assertDisabled({
    message: "After setting RS data without keywords",
    pendingFetchCount: 0,
  });

  // No suggestion should be returned for a search.
  let context = createContext(MerinoTestUtils.WEATHER_KEYWORD, {
    providers: [UrlbarProviderWeather.name],
    isPrivate: false,
  });
  await check_results({
    context,
    matches: [],
  });

  // Set keywords. Fetching should immediately start.
  info("Setting keywords");
  let fetchPromise = QuickSuggest.weather.waitForFetches();
  await QuickSuggestTestUtils.setRemoteSettingsRecords([
    {
      type: "weather",
      weather: MerinoTestUtils.WEATHER_RS_DATA,
    },
  ]);
  assertEnabled({
    message: "Immediately after setting keywords",
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

  // The suggestion should be returned for a search.
  context = createContext(MerinoTestUtils.WEATHER_KEYWORD, {
    providers: [UrlbarProviderWeather.name],
    isPrivate: false,
  });
  await check_results({
    context,
    matches: [makeExpectedResult()],
  });
});

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

  // Wait for keywords to be re-synced from remote settings.
  await QuickSuggestTestUtils.forceSync();

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

  // Wait for keywords to be re-synced from remote settings.
  await QuickSuggestTestUtils.forceSync();
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

  let context = createContext(MerinoTestUtils.WEATHER_KEYWORD, {
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

  let context = createContext(MerinoTestUtils.WEATHER_KEYWORD, {
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

  let context = createContext(MerinoTestUtils.WEATHER_KEYWORD, {
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

  let context = createContext(MerinoTestUtils.WEATHER_KEYWORD, {
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
        context: createContext(MerinoTestUtils.WEATHER_KEYWORD, {
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
        context: createContext(MerinoTestUtils.WEATHER_KEYWORD, {
          providers: [UrlbarProviderWeather.name],
          isPrivate: false,
        }),
        matches: [makeExpectedResult({ temperatureUnit: osUnit })],
      });
      Services.prefs.clearUserPref("intl.regional_prefs.use_os_locales");
    });
  }
}

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
  let context = createContext(MerinoTestUtils.WEATHER_KEYWORD, {
    providers: [UrlbarProviderWeather.name],
    isPrivate: false,
  });
  await check_results({
    context,
    matches: [makeExpectedResult()],
  });

  // Block the result.
  const controller = UrlbarTestUtils.newMockController();
  controller.setView({
    get visibleResults() {
      return context.results;
    },
    controller: {
      removeResult() {},
    },
  });
  UrlbarProviderWeather.onEngagement(
    "engagement",
    context,
    {
      result: context.results[0],
      selType: "dismiss",
      selIndex: context.results[0].rowIndex,
    },
    controller
  );
  Assert.ok(
    !UrlbarPrefs.get("suggest.weather"),
    "suggest.weather is false after blocking the result"
  );

  // Do a second search. Nothing should be returned.
  context = createContext(MerinoTestUtils.WEATHER_KEYWORD, {
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

  // Wait for keywords to be re-synced from remote settings.
  await QuickSuggestTestUtils.forceSync();

  assertEnabled({
    message: "On cleanup",
    hasSuggestion: true,
    pendingFetchCount: 0,
  });
});

// Simulates wake 100ms before the start of the next fetch period. A new fetch
// should not start.
add_task(async function wakeBeforeNextFetchPeriod() {
  await doWakeTest({
    sleepIntervalMs: QuickSuggest.weather._test_fetchIntervalMs - 100,
    shouldFetchOnWake: false,
    fetchTimerMsOnWake: 100,
  });
});

// Simulates wake 100ms after the start of the next fetch period. A new fetch
// should start.
add_task(async function wakeAfterNextFetchPeriod() {
  await doWakeTest({
    sleepIntervalMs: QuickSuggest.weather._test_fetchIntervalMs + 100,
    shouldFetchOnWake: true,
  });
});

// Simulates wake after many fetch periods + 100ms. A new fetch should start.
add_task(async function wakeAfterManyFetchPeriods() {
  await doWakeTest({
    sleepIntervalMs: 100 * QuickSuggest.weather._test_fetchIntervalMs + 100,
    shouldFetchOnWake: true,
  });
});

async function doWakeTest({
  sleepIntervalMs,
  shouldFetchOnWake,
  fetchTimerMsOnWake,
}) {
  // Make `Date.now()` return a value under our control, doesn't matter what it
  // is. This is the time the first fetch period will start.
  let nowOnStart = 100;
  let sandbox = sinon.createSandbox();
  let dateNowStub = sandbox.stub(
    Cu.getGlobalForObject(QuickSuggest.weather).Date,
    "now"
  );
  dateNowStub.returns(nowOnStart);

  // Start the first fetch period.
  info("Starting first fetch period");
  await QuickSuggest.weather._test_fetch();
  Assert.equal(
    QuickSuggest.weather._test_lastFetchTimeMs,
    nowOnStart,
    "Last fetch time should be updated after fetch"
  );
  Assert.equal(
    QuickSuggest.weather._test_fetchTimerMs,
    QuickSuggest.weather._test_fetchIntervalMs,
    "Timer period should be full fetch interval"
  );

  let timer = QuickSuggest.weather._test_fetchTimer;

  // Advance the clock and simulate wake.
  info("Sending wake notification");
  let nowOnWake = nowOnStart + sleepIntervalMs;
  dateNowStub.returns(nowOnWake);
  QuickSuggest.weather.observe(null, "wake_notification", "");

  Assert.equal(
    QuickSuggest.weather._test_pendingFetchCount,
    0,
    "After wake, next fetch should not have immediately started"
  );
  Assert.equal(
    QuickSuggest.weather._test_lastFetchTimeMs,
    nowOnStart,
    "After wake, last fetch time should be unchanged"
  );
  Assert.notEqual(
    QuickSuggest.weather._test_fetchTimer,
    0,
    "After wake, the timer should exist (be non-zero)"
  );
  Assert.notEqual(
    QuickSuggest.weather._test_fetchTimer,
    timer,
    "After wake, a new timer should have been created"
  );

  if (shouldFetchOnWake) {
    Assert.equal(
      QuickSuggest.weather._test_fetchTimerMs,
      QuickSuggest.weather._test_fetchDelayAfterComingOnlineMs,
      "After wake, timer period should be fetchDelayAfterComingOnlineMs"
    );
  } else {
    Assert.equal(
      QuickSuggest.weather._test_fetchTimerMs,
      fetchTimerMsOnWake,
      "After wake, timer period should be the remaining interval"
    );
  }

  // Wait for the fetch. If the wake didn't trigger it, then the caller should
  // have passed in a `sleepIntervalMs` that will make it start soon.
  info("Waiting for fetch after wake");
  await QuickSuggest.weather.waitForFetches();

  Assert.equal(
    QuickSuggest.weather._test_fetchTimerMs,
    QuickSuggest.weather._test_fetchIntervalMs,
    "After post-wake fetch, timer period should remain full fetch interval"
  );
  Assert.equal(
    QuickSuggest.weather._test_pendingFetchCount,
    0,
    "After post-wake fetch, no more fetches should be pending"
  );

  dateNowStub.restore();
}

// When network:link-status-changed is observed and the suggestion is non-null,
// a fetch should not start.
add_task(async function networkLinkStatusChanged_nonNull() {
  // See nsINetworkLinkService for possible data values.
  await doOnlineTestWithSuggestion({
    topic: "network:link-status-changed",
    dataValues: [
      "down",
      "up",
      "changed",
      "unknown",
      "this is not a valid data value",
    ],
  });
});

// When network:offline-status-changed is observed and the suggestion is
// non-null, a fetch should not start.
add_task(async function networkOfflineStatusChanged_nonNull() {
  // See nsIIOService for possible data values.
  await doOnlineTestWithSuggestion({
    topic: "network:offline-status-changed",
    dataValues: ["offline", "online", "this is not a valid data value"],
  });
});

// When captive-portal-login-success is observed and the suggestion is non-null,
// a fetch should not start.
add_task(async function captivePortalLoginSuccess_nonNull() {
  // See nsIIOService for possible data values.
  await doOnlineTestWithSuggestion({
    topic: "captive-portal-login-success",
    dataValues: [""],
  });
});

async function doOnlineTestWithSuggestion({ topic, dataValues }) {
  info("Starting fetch period");
  await QuickSuggest.weather._test_fetch();
  Assert.ok(
    QuickSuggest.weather.suggestion,
    "Suggestion should have been fetched"
  );

  let timer = QuickSuggest.weather._test_fetchTimer;

  for (let data of dataValues) {
    info("Sending notification: " + JSON.stringify({ topic, data }));
    QuickSuggest.weather.observe(null, topic, data);

    Assert.equal(
      QuickSuggest.weather._test_pendingFetchCount,
      0,
      "Fetch should not have started"
    );
    Assert.equal(
      QuickSuggest.weather._test_fetchTimer,
      timer,
      "Timer should not have been recreated"
    );
    Assert.equal(
      QuickSuggest.weather._test_fetchTimerMs,
      QuickSuggest.weather._test_fetchIntervalMs,
      "Timer period should be the full fetch interval"
    );
  }
}

// When network:link-status-changed is observed and the suggestion is null, a
// fetch should start unless the data indicates the status is offline.
add_task(async function networkLinkStatusChanged_null() {
  // See nsINetworkLinkService for possible data values.
  await doOnlineTestWithNullSuggestion({
    topic: "network:link-status-changed",
    offlineData: "down",
    otherDataValues: [
      "up",
      "changed",
      "unknown",
      "this is not a valid data value",
    ],
  });
});

// When network:offline-status-changed is observed and the suggestion is null, a
// fetch should start unless the data indicates the status is offline.
add_task(async function networkOfflineStatusChanged_null() {
  // See nsIIOService for possible data values.
  await doOnlineTestWithNullSuggestion({
    topic: "network:offline-status-changed",
    offlineData: "offline",
    otherDataValues: ["online", "this is not a valid data value"],
  });
});

// When captive-portal-login-success is observed and the suggestion is null, a
// fetch should start.
add_task(async function captivePortalLoginSuccess_null() {
  // See nsIIOService for possible data values.
  await doOnlineTestWithNullSuggestion({
    topic: "captive-portal-login-success",
    otherDataValues: [""],
  });
});

async function doOnlineTestWithNullSuggestion({
  topic,
  otherDataValues,
  offlineData = "",
}) {
  QuickSuggest.weather._test_setSuggestionToNull();
  Assert.ok(!QuickSuggest.weather.suggestion, "Suggestion should be null");

  let timer = QuickSuggest.weather._test_fetchTimer;

  // First, send the notification with the offline data value. Nothing should
  // happen.
  if (offlineData) {
    info("Sending notification: " + JSON.stringify({ topic, offlineData }));
    QuickSuggest.weather.observe(null, topic, offlineData);

    Assert.ok(
      !QuickSuggest.weather.suggestion,
      "Suggestion should remain null"
    );
    Assert.equal(
      QuickSuggest.weather._test_pendingFetchCount,
      0,
      "Fetch should not have started"
    );
    Assert.equal(
      QuickSuggest.weather._test_fetchTimer,
      timer,
      "Timer should not have been recreated"
    );
    Assert.equal(
      QuickSuggest.weather._test_fetchTimerMs,
      QuickSuggest.weather._test_fetchIntervalMs,
      "Timer period should be the full fetch interval"
    );
  }

  // Now send it with all other data values. Fetches should be triggered.
  for (let data of otherDataValues) {
    QuickSuggest.weather._test_setSuggestionToNull();
    Assert.ok(!QuickSuggest.weather.suggestion, "Suggestion should be null");

    info("Sending notification: " + JSON.stringify({ topic, data }));
    QuickSuggest.weather.observe(null, topic, data);

    Assert.equal(
      QuickSuggest.weather._test_pendingFetchCount,
      0,
      "Fetch should not have started yet"
    );
    Assert.notEqual(
      QuickSuggest.weather._test_fetchTimer,
      0,
      "Timer should exist"
    );
    Assert.notEqual(
      QuickSuggest.weather._test_fetchTimer,
      timer,
      "A new timer should have been created"
    );
    Assert.equal(
      QuickSuggest.weather._test_fetchTimerMs,
      QuickSuggest.weather._test_fetchDelayAfterComingOnlineMs,
      "Timer ms should be fetchDelayAfterComingOnlineMs"
    );

    timer = QuickSuggest.weather._test_fetchTimer;

    info("Waiting for fetch after notification");
    await QuickSuggest.weather.waitForFetches();

    Assert.equal(
      QuickSuggest.weather._test_pendingFetchCount,
      0,
      "Fetch should not be pending"
    );
    Assert.notEqual(
      QuickSuggest.weather._test_fetchTimer,
      0,
      "Timer should exist"
    );
    Assert.notEqual(
      QuickSuggest.weather._test_fetchTimer,
      timer,
      "A new timer should have been created"
    );
    Assert.equal(
      QuickSuggest.weather._test_fetchTimerMs,
      QuickSuggest.weather._test_fetchIntervalMs,
      "Timer period should be full fetch interval"
    );

    timer = QuickSuggest.weather._test_fetchTimer;
  }
}

// When many online notifications are received at once, only one fetch should
// start.
add_task(async function manyOnlineNotifications() {
  await doManyNotificationsTest([
    ["network:link-status-changed", "changed"],
    ["network:link-status-changed", "up"],
    ["network:offline-status-changed", "online"],
  ]);
});

// When wake and online notifications are received at once, only one fetch
// should start.
add_task(async function wakeAndOnlineNotifications() {
  await doManyNotificationsTest([
    ["wake_notification", ""],
    ["network:link-status-changed", "changed"],
    ["network:link-status-changed", "up"],
    ["network:offline-status-changed", "online"],
  ]);
});

async function doManyNotificationsTest(notifications) {
  // Make `Date.now()` return a value under our control, doesn't matter what it
  // is. This is the time the first fetch period will start.
  let nowOnStart = 100;
  let sandbox = sinon.createSandbox();
  let dateNowStub = sandbox.stub(
    Cu.getGlobalForObject(QuickSuggest.weather).Date,
    "now"
  );
  dateNowStub.returns(nowOnStart);

  // Start a first fetch period so that after we send the notifications below
  // the last fetch time will be in the past.
  info("Starting first fetch period");
  await QuickSuggest.weather._test_fetch();
  Assert.equal(
    QuickSuggest.weather._test_lastFetchTimeMs,
    nowOnStart,
    "Last fetch time should be updated after fetch"
  );

  // Now advance the clock by many fetch intervals.
  let nowOnWake = nowOnStart + 100 * QuickSuggest.weather._test_fetchIntervalMs;
  dateNowStub.returns(nowOnWake);

  // Set the suggestion to null so online notifications will trigger a fetch.
  QuickSuggest.weather._test_setSuggestionToNull();
  Assert.ok(!QuickSuggest.weather.suggestion, "Suggestion should be null");

  // Clear the server's list of received requests.
  MerinoTestUtils.server.reset();
  MerinoTestUtils.server.response.body.suggestions = [
    MerinoTestUtils.WEATHER_SUGGESTION,
  ];

  // Send the notifications.
  for (let [topic, data] of notifications) {
    info("Sending notification: " + JSON.stringify({ topic, data }));
    QuickSuggest.weather.observe(null, topic, data);
  }

  info("Waiting for fetch after notifications");
  await QuickSuggest.weather.waitForFetches();

  Assert.equal(
    QuickSuggest.weather._test_pendingFetchCount,
    0,
    "Fetch should not be pending"
  );
  Assert.notEqual(
    QuickSuggest.weather._test_fetchTimer,
    0,
    "Timer should exist"
  );
  Assert.equal(
    QuickSuggest.weather._test_fetchTimerMs,
    QuickSuggest.weather._test_fetchIntervalMs,
    "Timer period should be full fetch interval"
  );

  Assert.equal(
    MerinoTestUtils.server.requests.length,
    1,
    "Merino should have received only one request"
  );

  dateNowStub.restore();
}

// Fetching when a VPN is detected should set the suggestion to null, and
// turning off the VPN should trigger a re-fetch.
add_task(async function vpn() {
  // Register a mock object that implements nsINetworkLinkService.
  let mockLinkService = {
    isLinkUp: true,
    linkStatusKnown: true,
    linkType: Ci.nsINetworkLinkService.LINK_TYPE_WIFI,
    networkID: "abcd",
    dnsSuffixList: [],
    platformDNSIndications: Ci.nsINetworkLinkService.NONE_DETECTED,
    QueryInterface: ChromeUtils.generateQI(["nsINetworkLinkService"]),
  };
  let networkLinkServiceCID = MockRegistrar.register(
    "@mozilla.org/network/network-link-service;1",
    mockLinkService
  );
  QuickSuggest.weather._test_linkService = mockLinkService;

  // At this point no VPN is detected, so a fetch should complete successfully.
  await QuickSuggest.weather._test_fetch();
  Assert.ok(QuickSuggest.weather.suggestion, "Suggestion should exist");

  // Modify the mock link service to indicate a VPN is detected.
  mockLinkService.platformDNSIndications =
    Ci.nsINetworkLinkService.VPN_DETECTED;

  // Now a fetch should set the suggestion to null.
  await QuickSuggest.weather._test_fetch();
  Assert.ok(!QuickSuggest.weather.suggestion, "Suggestion should be null");

  // Set `weather.ignoreVPN` and fetch again. It should complete successfully.
  UrlbarPrefs.set("weather.ignoreVPN", true);
  await QuickSuggest.weather._test_fetch();
  Assert.ok(QuickSuggest.weather.suggestion, "Suggestion should be fetched");

  // Clear the pref and fetch again. It should set the suggestion back to null.
  UrlbarPrefs.clear("weather.ignoreVPN");
  await QuickSuggest.weather._test_fetch();
  Assert.ok(!QuickSuggest.weather.suggestion, "Suggestion should be null");

  // Simulate the link status changing. Since the mock link service still
  // indicates a VPN is detected, the suggestion should remain null.
  let fetchPromise = QuickSuggest.weather.waitForFetches();
  QuickSuggest.weather.observe(null, "network:link-status-changed", "changed");
  await fetchPromise;
  Assert.ok(!QuickSuggest.weather.suggestion, "Suggestion should remain null");

  // Modify the mock link service to indicate a VPN is no longer detected.
  mockLinkService.platformDNSIndications =
    Ci.nsINetworkLinkService.NONE_DETECTED;

  // Simulate the link status changing again. The suggestion should be fetched.
  fetchPromise = QuickSuggest.weather.waitForFetches();
  QuickSuggest.weather.observe(null, "network:link-status-changed", "changed");
  await fetchPromise;
  Assert.ok(QuickSuggest.weather.suggestion, "Suggestion should be fetched");

  MockRegistrar.unregister(networkLinkServiceCID);
  delete QuickSuggest.weather._test_linkService;
});

// When a Nimbus experiment is installed, it should override the remote settings
// weather record.
add_task(async function nimbusOverride() {
  // Sanity check initial state.
  assertEnabled({
    message: "Sanity check initial state",
    hasSuggestion: true,
    pendingFetchCount: 0,
  });

  // Verify a search works as expected with the default remote settings weather
  // record (which was added in the init task).
  await check_results({
    context: createContext(MerinoTestUtils.WEATHER_KEYWORD, {
      providers: [UrlbarProviderWeather.name],
      isPrivate: false,
    }),
    matches: [makeExpectedResult()],
  });

  // Install an experiment with a different keyword and min length.
  let nimbusCleanup = await UrlbarTestUtils.initNimbusFeature({
    weatherKeywords: ["nimbusoverride"],
    weatherKeywordsMinimumLength: "nimbus".length,
  });

  // The usual default keyword shouldn't match.
  await check_results({
    context: createContext(MerinoTestUtils.WEATHER_KEYWORD, {
      providers: [UrlbarProviderWeather.name],
      isPrivate: false,
    }),
    matches: [],
  });

  // The new keyword from Nimbus should match.
  await check_results({
    context: createContext("nimbusoverride", {
      providers: [UrlbarProviderWeather.name],
      isPrivate: false,
    }),
    matches: [makeExpectedResult()],
  });
  await check_results({
    context: createContext("nimbus", {
      providers: [UrlbarProviderWeather.name],
      isPrivate: false,
    }),
    matches: [makeExpectedResult()],
  });

  // Uninstall the experiment.
  await nimbusCleanup();

  // The usual default keyword should match again.
  await check_results({
    context: createContext(MerinoTestUtils.WEATHER_KEYWORD, {
      providers: [UrlbarProviderWeather.name],
      isPrivate: false,
    }),
    matches: [makeExpectedResult()],
  });

  // The keywords from Nimbus shouldn't match anymore.
  await check_results({
    context: createContext("nimbusoverride", {
      providers: [UrlbarProviderWeather.name],
      isPrivate: false,
    }),
    matches: [],
  });
  await check_results({
    context: createContext("nimbus", {
      providers: [UrlbarProviderWeather.name],
      isPrivate: false,
    }),
    matches: [],
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
    QuickSuggest.weather._test_fetchTimer,
    0,
    "Fetch timer is non-zero"
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
    QuickSuggest.weather._test_fetchTimer,
    0,
    "Fetch timer is zero"
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
  suggestedIndex = 1,
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
      requestId: MerinoTestUtils.server.response.body.request_id,
      source: "merino",
      provider: "accuweather",
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
