/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

// Tests the keywords and zero-prefix behavior of quick suggest weather.

"use strict";

ChromeUtils.defineESModuleGetters(this, {
  UrlbarProviderWeather: "resource:///modules/UrlbarProviderWeather.sys.mjs",
});

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
});

// * Settings data: none
// * Nimbus values: none
// * Min keyword length pref: none
// * Expected: no suggestion
add_task(async function () {
  await doKeywordsTest({
    desc: "No data",
    tests: {
      "": false,
      w: false,
      we: false,
      wea: false,
      weat: false,
      weath: false,
      weathe: false,
      weather: false,
      f: false,
      fo: false,
      for: false,
      fore: false,
      forec: false,
      foreca: false,
      forecas: false,
      forecast: false,
    },
  });
});

// * Settings data: empty
// * Nimbus values: none
// * Min keyword length pref: none
// * Expected: no suggestion
add_task(async function () {
  await doKeywordsTest({
    desc: "Empty settings",
    settingsData: {},
    tests: {
      "": false,
      w: false,
      we: false,
      wea: false,
      weat: false,
      weath: false,
      weathe: false,
      weather: false,
      f: false,
      fo: false,
      for: false,
      fore: false,
      forec: false,
      foreca: false,
      forecas: false,
      forecast: false,
    },
  });
});

// * Settings data: keywords only
// * Nimbus values: none
// * Min keyword length pref: none
// * Expected: full keywords only
add_task(async function () {
  await doKeywordsTest({
    desc: "Settings only, keywords only",
    settingsData: {
      keywords: ["weather", "forecast"],
    },
    tests: {
      "": false,
      w: false,
      we: false,
      wea: false,
      weat: false,
      weath: false,
      weathe: false,
      weather: true,
      f: false,
      fo: false,
      for: false,
      fore: false,
      forec: false,
      foreca: false,
      forecas: false,
      forecast: true,
    },
  });
});

// * Settings data: keywords and min keyword length = 0
// * Nimbus values: none
// * Min keyword length pref: none
// * Expected: full keywords only
add_task(async function () {
  await doKeywordsTest({
    desc: "Settings only, min keyword length = 0",
    settingsData: {
      keywords: ["weather", "forecast"],
      min_keyword_length: 0,
    },
    tests: {
      "": false,
      w: false,
      we: false,
      wea: false,
      weat: false,
      weath: false,
      weathe: false,
      weather: true,
      f: false,
      fo: false,
      for: false,
      fore: false,
      forec: false,
      foreca: false,
      forecas: false,
      forecast: true,
    },
  });
});

// * Settings data: keywords and min keyword length > 0
// * Nimbus values: none
// * Min keyword length pref: none
// * Expected: use settings data
add_task(async function () {
  await doKeywordsTest({
    desc: "Settings only, min keyword length > 0",
    settingsData: {
      keywords: ["weather", "forecast"],
      min_keyword_length: 3,
    },
    tests: {
      "": false,
      w: false,
      we: false,
      wea: true,
      weat: true,
      weath: true,
      weathe: true,
      weather: true,
      f: false,
      fo: false,
      for: true,
      fore: true,
      forec: true,
      foreca: true,
      forecas: true,
      forecast: true,
    },
  });
});

// * Settings data: keywords and min keyword length = 0
// * Nimbus values: none
// * Min keyword length pref: 6
// * Expected: use settings keywords and min keyword length pref
add_task(async function () {
  await doKeywordsTest({
    desc: "Settings only, min keyword length = 0, pref exists",
    settingsData: {
      keywords: ["weather", "forecast"],
      min_keyword_length: 0,
    },
    minKeywordLength: 6,
    tests: {
      "": false,
      w: false,
      we: false,
      wea: false,
      weat: false,
      weath: false,
      weathe: true,
      weather: true,
      f: false,
      fo: false,
      for: false,
      fore: false,
      forec: false,
      foreca: true,
      forecas: true,
      forecast: true,
    },
  });
});

// * Settings data: keywords and min keyword length > 0
// * Nimbus values: none
// * Min keyword length pref: 6
// * Expected: use settings keywords and min keyword length pref
add_task(async function () {
  await doKeywordsTest({
    desc: "Settings only, min keyword length > 0, pref exists",
    settingsData: {
      keywords: ["weather", "forecast"],
      min_keyword_length: 3,
    },
    minKeywordLength: 6,
    tests: {
      "": false,
      w: false,
      we: false,
      wea: false,
      weat: false,
      weath: false,
      weathe: true,
      weather: true,
      f: false,
      fo: false,
      for: false,
      fore: false,
      forec: false,
      foreca: true,
      forecas: true,
      forecast: true,
    },
  });
});

// * Settings data: empty
// * Nimbus values: empty
// * Min keyword length pref: none
// * Expected: no suggestion
add_task(async function () {
  await doKeywordsTest({
    desc: "Settings: empty; Nimbus: empty",
    settingsData: {},
    nimbusValues: {},
    tests: {
      "": false,
      w: false,
      we: false,
      wea: false,
      weat: false,
      weath: false,
      weathe: false,
      weather: false,
      f: false,
      fo: false,
      for: false,
      fore: false,
      forec: false,
      foreca: false,
      forecas: false,
      forecast: false,
    },
  });
});

// * Settings data: keywords only
// * Nimbus values: keywords only
// * Min keyword length pref: none
// * Expected: full keywords in Nimbus
add_task(async function () {
  await doKeywordsTest({
    desc: "Settings: keywords; Nimbus: keywords",
    settingsData: {
      keywords: ["weather"],
    },
    nimbusValues: {
      weatherKeywords: ["forecast"],
    },
    tests: {
      "": false,
      w: false,
      we: false,
      wea: false,
      weat: false,
      weath: false,
      weathe: false,
      weather: false,
      f: false,
      fo: false,
      for: false,
      fore: false,
      forec: false,
      foreca: false,
      forecas: false,
      forecast: true,
    },
  });
});

// * Settings data: keywords and min keyword length = 0
// * Nimbus values: keywords only
// * Min keyword length pref: none
// * Expected: full keywords in Nimbus
add_task(async function () {
  await doKeywordsTest({
    desc: "Settings: keywords, min keyword length = 0; Nimbus: keywords",
    settingsData: {
      keywords: ["weather"],
      min_keyword_length: 0,
    },
    nimbusValues: {
      weatherKeywords: ["forecast"],
    },
    tests: {
      "": false,
      w: false,
      we: false,
      wea: false,
      weat: false,
      weath: false,
      weathe: false,
      weather: false,
      f: false,
      fo: false,
      for: false,
      fore: false,
      forec: false,
      foreca: false,
      forecas: false,
      forecast: true,
    },
  });
});

// * Settings data: keywords and min keyword length > 0
// * Nimbus values: keywords only
// * Min keyword length pref: none
// * Expected: Nimbus keywords with settings min keyword length
add_task(async function () {
  await doKeywordsTest({
    desc: "Settings: keywords, min keyword length > 0; Nimbus: keywords",
    settingsData: {
      keywords: ["weather"],
      min_keyword_length: 3,
    },
    nimbusValues: {
      weatherKeywords: ["forecast"],
    },
    tests: {
      "": false,
      w: false,
      we: false,
      wea: false,
      weat: false,
      weath: false,
      weathe: false,
      weather: false,
      f: false,
      fo: false,
      for: true,
      fore: true,
      forec: true,
      foreca: true,
      forecas: true,
      forecast: true,
    },
  });
});

// * Settings data: keywords and min keyword length > 0
// * Nimbus values: keywords and min keyword length = 0
// * Min keyword length pref: none
// * Expected: Nimbus keywords with settings min keyword length
add_task(async function () {
  await doKeywordsTest({
    desc: "Settings: keywords, min keyword length > 0; Nimbus: keywords, min keyword length = 0",
    settingsData: {
      keywords: ["weather"],
      min_keyword_length: 3,
    },
    nimbusValues: {
      weatherKeywords: ["forecast"],
      weatherKeywordsMinimumLength: 0,
    },
    tests: {
      "": false,
      w: false,
      we: false,
      wea: false,
      weat: false,
      weath: false,
      weathe: false,
      weather: false,
      f: false,
      fo: false,
      for: true,
      fore: true,
      forec: true,
      foreca: true,
      forecas: true,
      forecast: true,
    },
  });
});

// * Settings data: keywords and min keyword length > 0
// * Nimbus values: keywords and min keyword length > 0
// * Min keyword length pref: none
// * Expected: use Nimbus values
add_task(async function () {
  await doKeywordsTest({
    desc: "Settings: keywords, min keyword length > 0; Nimbus: keywords, min keyword length > 0",
    settingsData: {
      keywords: ["weather"],
      min_keyword_length: 3,
    },
    nimbusValues: {
      weatherKeywords: ["forecast"],
      weatherKeywordsMinimumLength: 4,
    },
    tests: {
      "": false,
      w: false,
      we: false,
      wea: false,
      weat: false,
      weath: false,
      weathe: false,
      weather: false,
      f: false,
      fo: false,
      for: false,
      fore: true,
      forec: true,
      foreca: true,
      forecas: true,
      forecast: true,
    },
  });
});

// * Settings data: keywords and min keyword length > 0
// * Nimbus values: keywords and min keyword length = 0
// * Min keyword length pref: exists
// * Expected: Nimbus keywords with min keyword length pref
add_task(async function () {
  await doKeywordsTest({
    desc: "Settings: keywords, min keyword length > 0; Nimbus: keywords, min keyword length = 0; pref exists",
    settingsData: {
      keywords: ["weather"],
      min_keyword_length: 3,
    },
    nimbusValues: {
      weatherKeywords: ["forecast"],
      weatherKeywordsMinimumLength: 0,
    },
    minKeywordLength: 6,
    tests: {
      "": false,
      w: false,
      we: false,
      wea: false,
      weat: false,
      weath: false,
      weathe: false,
      weather: false,
      f: false,
      fo: false,
      for: false,
      fore: false,
      forec: false,
      foreca: true,
      forecas: true,
      forecast: true,
    },
  });
});

// * Settings data: keywords and min keyword length > 0
// * Nimbus values: keywords and min keyword length > 0
// * Min keyword length pref: exists
// * Expected: Nimbus keywords with min keyword length pref
add_task(async function () {
  await doKeywordsTest({
    desc: "Settings: keywords, min keyword length > 0; Nimbus: keywords, min keyword length > 0; pref exists",
    settingsData: {
      keywords: ["weather", "forecast"],
      min_keyword_length: 3,
    },
    nimbusValues: {
      weatherKeywords: ["forecast"],
      weatherKeywordsMinimumLength: 4,
    },
    minKeywordLength: 6,
    tests: {
      "": false,
      w: false,
      we: false,
      wea: false,
      weat: false,
      weath: false,
      weathe: false,
      weather: false,
      f: false,
      fo: false,
      for: false,
      fore: false,
      forec: false,
      foreca: true,
      forecas: true,
      forecast: true,
    },
  });
});

// * Settings data: none
// * Nimbus values: keywords only
// * Min keyword length pref: none
// * Expected: full keywords
add_task(async function () {
  await doKeywordsTest({
    desc: "Settings: none; Nimbus: keywords",
    nimbusValues: {
      weatherKeywords: ["weather", "forecast"],
    },
    tests: {
      "": false,
      w: false,
      we: false,
      wea: false,
      weat: false,
      weath: false,
      weathe: false,
      weather: true,
      f: false,
      fo: false,
      for: false,
      fore: false,
      forec: false,
      foreca: false,
      forecas: false,
      forecast: true,
    },
  });
});

// * Settings data: none
// * Nimbus values: keywords and min keyword length = 0
// * Min keyword length pref: none
// * Expected: full keywords
add_task(async function () {
  await doKeywordsTest({
    desc: "Settings: none; Nimbus: keywords, min keyword length = 0",
    nimbusValues: {
      weatherKeywords: ["weather", "forecast"],
      weatherKeywordsMinimumLength: 0,
    },
    tests: {
      "": false,
      w: false,
      we: false,
      wea: false,
      weat: false,
      weath: false,
      weathe: false,
      weather: true,
      f: false,
      fo: false,
      for: false,
      fore: false,
      forec: false,
      foreca: false,
      forecas: false,
      forecast: true,
    },
  });
});

// * Settings data: none
// * Nimbus values: keywords and min keyword length > 0
// * Min keyword length pref: none
// * Expected: use Nimbus values
add_task(async function () {
  await doKeywordsTest({
    desc: "Settings: none; Nimbus: keywords, min keyword length > 0",
    nimbusValues: {
      weatherKeywords: ["weather", "forecast"],
      weatherKeywordsMinimumLength: 3,
    },
    tests: {
      "": false,
      w: false,
      we: false,
      wea: true,
      weat: true,
      weath: true,
      weathe: true,
      weather: true,
      f: false,
      fo: false,
      for: true,
      fore: true,
      forec: true,
      foreca: true,
      forecas: true,
      forecast: true,
    },
  });
});

// * Settings data: none
// * Nimbus values: keywords and min keyword length > 0
// * Min keyword length pref: exists
// * Expected: use Nimbus keywords and min keyword length pref
add_task(async function () {
  await doKeywordsTest({
    desc: "Settings: none; Nimbus: keywords, min keyword length > 0; pref exists",
    nimbusValues: {
      weatherKeywords: ["weather", "forecast"],
      weatherKeywordsMinimumLength: 3,
    },
    minKeywordLength: 6,
    tests: {
      "": false,
      w: false,
      we: false,
      wea: false,
      weat: false,
      weath: false,
      weathe: true,
      weather: true,
      f: false,
      fo: false,
      for: false,
      fore: false,
      forec: false,
      foreca: true,
      forecas: true,
      forecast: true,
    },
  });
});

// When `weatherKeywords` is non-null and `weatherKeywordsMinimumLength` is
// larger than the length of all keywords, the suggestion should not be
// triggered.
add_task(async function minLength_large() {
  await doKeywordsTest({
    desc: "Large min length",
    nimbusValues: {
      weatherKeywords: ["weather", "forecast"],
      weatherKeywordsMinimumLength: 999,
    },
    tests: {
      "": false,
      w: false,
      we: false,
      wea: false,
      weat: false,
      weath: false,
      weathe: false,
      weather: false,
      f: false,
      fo: false,
      for: false,
      fore: false,
      forec: false,
      foreca: false,
      forecas: false,
      forecast: false,
    },
  });
});

// Leading and trailing spaces should be ignored.
add_task(async function leadingAndTrailingSpaces() {
  await doKeywordsTest({
    nimbusValues: {
      weatherKeywords: ["weather"],
      weatherKeywordsMinimumLength: 3,
    },
    tests: {
      " wea": true,
      "  wea": true,
      "wea ": true,
      "wea  ": true,
      "  wea  ": true,
      " weat": true,
      "  weat": true,
      "weat ": true,
      "weat  ": true,
      "  weat  ": true,
    },
  });
});

add_task(async function caseInsensitive() {
  await doKeywordsTest({
    desc: "Case insensitive",
    settingsData: {
      keywords: ["weather"],
      min_keyword_length: 3,
    },
    tests: {
      wea: true,
      WEA: true,
      Wea: true,
      WeA: true,
      WEATHER: true,
      Weather: true,
      WeAtHeR: true,
    },
  });
});

async function doKeywordsTest({
  desc,
  tests,
  nimbusValues = null,
  settingsData = null,
  minKeywordLength = undefined,
}) {
  info("Doing keywords test: " + desc);
  info(JSON.stringify({ nimbusValues, settingsData, minKeywordLength }));

  // If a suggestion hasn't already been fetched and the data contains keywords,
  // a fetch will start. Wait for it to finish below.
  let fetchPromise;
  if (
    !QuickSuggest.weather.suggestion &&
    (nimbusValues?.weatherKeywords || settingsData?.keywords)
  ) {
    fetchPromise = QuickSuggest.weather.waitForFetches();
  }

  let nimbusCleanup;
  if (nimbusValues) {
    nimbusCleanup = await UrlbarTestUtils.initNimbusFeature(nimbusValues);
  }

  await QuickSuggestTestUtils.setRemoteSettingsRecords([
    {
      type: "weather",
      weather: settingsData,
    },
  ]);

  if (minKeywordLength) {
    UrlbarPrefs.set("weather.minKeywordLength", minKeywordLength);
  }

  if (fetchPromise) {
    info("Waiting for fetch");
    assertFetchingStarted({ pendingFetchCount: 1 });
    await fetchPromise;
    info("Got fetch");
  }

  for (let [searchString, expected] of Object.entries(tests)) {
    info(
      "Doing search: " +
        JSON.stringify({
          searchString,
          expected,
        })
    );

    let suggestedIndex = searchString ? 1 : 0;
    await check_results({
      context: createContext(searchString, {
        providers: [UrlbarProviderWeather.name],
        isPrivate: false,
      }),
      matches: expected ? [makeExpectedResult({ suggestedIndex })] : [],
    });
  }

  await nimbusCleanup?.();

  fetchPromise = null;
  if (!QuickSuggest.weather.suggestion) {
    fetchPromise = QuickSuggest.weather.waitForFetches();
  }
  await QuickSuggestTestUtils.setRemoteSettingsRecords([
    {
      type: "weather",
      weather: MerinoTestUtils.WEATHER_RS_DATA,
    },
  ]);

  UrlbarPrefs.clear("weather.minKeywordLength");
  await fetchPromise;
}

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
  let keyword = "test";
  let iab_category = isSponsored ? "22 - Shopping" : "5 - Education";

  // Add a remote settings result to quick suggest.
  UrlbarPrefs.set(pref, true);
  await QuickSuggestTestUtils.setRemoteSettingsRecords([
    {
      type: "data",
      attachment: [
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
      ],
    },
    {
      type: "weather",
      weather: MerinoTestUtils.WEATHER_RS_DATA,
    },
  ]);

  // First do a search to verify the quick suggest result matches the keyword.
  info("Doing first search for quick suggest result");
  await check_results({
    context: createContext(keyword, {
      providers: [UrlbarProviderQuickSuggest.name, UrlbarProviderWeather.name],
      isPrivate: false,
    }),
    matches: [
      {
        type: UrlbarUtils.RESULT_TYPE.URL,
        source: UrlbarUtils.RESULT_SOURCE.SEARCH,
        heuristic: false,
        payload: {
          telemetryType: isSponsored ? "adm_sponsored" : "adm_nonsponsored",
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
          descriptionL10n: isSponsored
            ? { id: "urlbar-result-action-sponsored" }
            : undefined,
          helpUrl: QuickSuggest.HELP_URL,
          helpL10n: {
            id: "urlbar-result-menu-learn-more-about-firefox-suggest",
          },
          isBlockable: true,
          blockL10n: {
            id: "urlbar-result-menu-dismiss-firefox-suggest",
          },
          source: "remote-settings",
          provider: "AdmWikipedia",
        },
      },
    ],
  });

  // Set up the keyword for the weather suggestion and do a second search to
  // verify only the weather result matches.
  info("Doing second search for weather suggestion");
  let cleanup = await UrlbarTestUtils.initNimbusFeature({
    weatherKeywords: [keyword],
    weatherKeywordsMinimumLength: 1,
  });
  await check_results({
    context: createContext(keyword, {
      providers: [UrlbarProviderQuickSuggest.name, UrlbarProviderWeather.name],
      isPrivate: false,
    }),
    matches: [makeExpectedResult({ suggestedIndex: 1 })],
  });
  await cleanup();

  UrlbarPrefs.clear(pref);
}

add_task(async function () {
  await doIncrementTest({
    desc: "Settings only",
    setup: {
      settingsData: {
        keywords: ["forecast", "wind"],
        min_keyword_length: 3,
      },
    },
    tests: [
      {
        minKeywordLength: 3,
        canIncrement: true,
        searches: {
          fo: false,
          for: true,
          fore: true,
          forec: true,
          wi: false,
          win: true,
          wind: true,
        },
      },
      {
        minKeywordLength: 4,
        canIncrement: true,
        searches: {
          fo: false,
          for: false,
          fore: true,
          forec: true,
          wi: false,
          win: false,
          wind: true,
        },
      },
      {
        minKeywordLength: 5,
        canIncrement: true,
        searches: {
          fo: false,
          for: false,
          fore: false,
          forec: true,
          wi: false,
          win: false,
          wind: false,
        },
      },
    ],
  });
});

add_task(async function () {
  await doIncrementTest({
    desc: "Settings only with cap",
    setup: {
      settingsData: {
        keywords: ["forecast", "wind"],
        min_keyword_length: 3,
        min_keyword_length_cap: 6,
      },
    },
    tests: [
      {
        minKeywordLength: 3,
        canIncrement: true,
        searches: {
          fo: false,
          for: true,
          fore: true,
          forec: true,
          foreca: true,
          forecas: true,
          wi: false,
          win: true,
          wind: true,
        },
      },
      {
        minKeywordLength: 4,
        canIncrement: true,
        searches: {
          fo: false,
          for: false,
          fore: true,
          forec: true,
          foreca: true,
          forecas: true,
          wi: false,
          win: false,
          wind: true,
        },
      },
      {
        minKeywordLength: 5,
        canIncrement: true,
        searches: {
          fo: false,
          for: false,
          fore: false,
          forec: true,
          foreca: true,
          forecas: true,
          wi: false,
          win: false,
          wind: false,
          windy: false,
        },
      },
      {
        minKeywordLength: 6,
        canIncrement: false,
        searches: {
          fo: false,
          for: false,
          fore: false,
          forec: false,
          foreca: true,
          forecas: true,
          wi: false,
          win: false,
          wind: false,
          windy: false,
        },
      },
      {
        minKeywordLength: 6,
        canIncrement: false,
        searches: {
          fo: false,
          for: false,
          fore: false,
          forec: false,
          foreca: true,
          forecas: true,
          wi: false,
          win: false,
          wind: false,
          windy: false,
        },
      },
    ],
  });
});

add_task(async function () {
  await doIncrementTest({
    desc: "Settings and Nimbus",
    setup: {
      settingsData: {
        keywords: ["weather"],
        min_keyword_length: 5,
      },
      nimbusValues: {
        weatherKeywords: ["forecast", "wind"],
        weatherKeywordsMinimumLength: 3,
      },
    },
    tests: [
      {
        minKeywordLength: 3,
        canIncrement: true,
        searches: {
          we: false,
          wea: false,
          weat: false,
          weath: false,
          fo: false,
          for: true,
          fore: true,
          forec: true,
          wi: false,
          win: true,
          wind: true,
        },
      },
      {
        minKeywordLength: 4,
        canIncrement: true,
        searches: {
          we: false,
          wea: false,
          weat: false,
          weath: false,
          fo: false,
          for: false,
          fore: true,
          forec: true,
          wi: false,
          win: false,
          wind: true,
        },
      },
      {
        minKeywordLength: 5,
        canIncrement: true,
        searches: {
          we: false,
          wea: false,
          weat: false,
          weath: false,
          fo: false,
          for: false,
          fore: false,
          forec: true,
          wi: false,
          win: false,
          wind: false,
          windy: false,
        },
      },
    ],
  });
});

add_task(async function () {
  await doIncrementTest({
    desc: "Settings and Nimbus with cap in Nimbus",
    setup: {
      settingsData: {
        keywords: ["weather"],
        min_keyword_length: 5,
      },
      nimbusValues: {
        weatherKeywords: ["forecast", "wind"],
        weatherKeywordsMinimumLength: 3,
        weatherKeywordsMinimumLengthCap: 6,
      },
    },
    tests: [
      {
        minKeywordLength: 3,
        canIncrement: true,
        searches: {
          we: false,
          wea: false,
          weat: false,
          weath: false,
          fo: false,
          for: true,
          fore: true,
          forec: true,
          foreca: true,
          forecas: true,
          wi: false,
          win: true,
          wind: true,
        },
      },
      {
        minKeywordLength: 4,
        canIncrement: true,
        searches: {
          we: false,
          wea: false,
          weat: false,
          weath: false,
          fo: false,
          for: false,
          fore: true,
          forec: true,
          foreca: true,
          forecas: true,
          wi: false,
          win: false,
          wind: true,
        },
      },
      {
        minKeywordLength: 5,
        canIncrement: true,
        searches: {
          we: false,
          wea: false,
          weat: false,
          weath: false,
          fo: false,
          for: false,
          fore: false,
          forec: true,
          foreca: true,
          forecas: true,
          wi: false,
          win: false,
          wind: false,
          windy: false,
        },
      },
      {
        minKeywordLength: 6,
        canIncrement: false,
        searches: {
          we: false,
          wea: false,
          weat: false,
          weath: false,
          fo: false,
          for: false,
          fore: false,
          forec: false,
          foreca: true,
          forecas: true,
          wi: false,
          win: false,
          wind: false,
          windy: false,
        },
      },
      {
        minKeywordLength: 6,
        canIncrement: false,
        searches: {
          we: false,
          wea: false,
          weat: false,
          weath: false,
          fo: false,
          for: false,
          fore: false,
          forec: false,
          foreca: true,
          forecas: true,
          wi: false,
          win: false,
          wind: false,
          windy: false,
        },
      },
    ],
  });
});

async function doIncrementTest({ desc, setup, tests }) {
  info("Doing increment test: " + desc);
  info(JSON.stringify({ setup }));

  let { nimbusValues, settingsData } = setup;

  let fetchPromise;
  if (
    !QuickSuggest.weather.suggestion &&
    (nimbusValues?.weatherKeywords || settingsData?.keywords)
  ) {
    fetchPromise = QuickSuggest.weather.waitForFetches();
  }

  let nimbusCleanup;
  if (nimbusValues) {
    nimbusCleanup = await UrlbarTestUtils.initNimbusFeature(nimbusValues);
  }

  await QuickSuggestTestUtils.setRemoteSettingsRecords([
    {
      type: "weather",
      weather: settingsData,
    },
  ]);

  if (fetchPromise) {
    info("Waiting for fetch");
    assertFetchingStarted({ pendingFetchCount: 1 });
    await fetchPromise;
    info("Got fetch");
  }

  for (let { minKeywordLength, canIncrement, searches } of tests) {
    info(
      "Doing increment test case: " +
        JSON.stringify({
          minKeywordLength,
          canIncrement,
        })
    );

    Assert.equal(
      QuickSuggest.weather.minKeywordLength,
      minKeywordLength,
      "minKeywordLength should be correct"
    );
    Assert.equal(
      QuickSuggest.weather.canIncrementMinKeywordLength,
      canIncrement,
      "canIncrement should be correct"
    );

    for (let [searchString, expected] of Object.entries(searches)) {
      Assert.equal(
        QuickSuggest.weather.keywords.has(searchString),
        expected,
        "Keyword should be present/absent as expected: " + searchString
      );

      await check_results({
        context: createContext(searchString, {
          providers: [UrlbarProviderWeather.name],
          isPrivate: false,
        }),
        matches: expected ? [makeExpectedResult({ suggestedIndex: 1 })] : [],
      });
    }

    QuickSuggest.weather.incrementMinKeywordLength();
    info(
      "Incremented min keyword length, new value is: " +
        QuickSuggest.weather.minKeywordLength
    );
  }

  await nimbusCleanup?.();

  fetchPromise = null;
  if (!QuickSuggest.weather.suggestion) {
    fetchPromise = QuickSuggest.weather.waitForFetches();
  }
  await QuickSuggestTestUtils.setRemoteSettingsRecords([
    {
      type: "weather",
      weather: MerinoTestUtils.WEATHER_RS_DATA,
    },
  ]);
  UrlbarPrefs.clear("weather.minKeywordLength");
  await fetchPromise;
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

function assertFetchingStarted() {
  info("Asserting fetching has started");

  Assert.notEqual(
    QuickSuggest.weather._test_fetchTimer,
    0,
    "Fetch timer is non-zero"
  );
  Assert.ok(QuickSuggest.weather._test_merino, "Merino client is non-null");
  Assert.equal(
    QuickSuggest.weather._test_pendingFetchCount,
    1,
    "Expected pending fetch count"
  );
}
