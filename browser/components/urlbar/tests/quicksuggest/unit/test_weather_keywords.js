/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

// Tests the keywords and zero-prefix behavior of quick suggest weather.

"use strict";

ChromeUtils.defineESModuleGetters(this, {
  UrlbarProviderWeather: "resource:///modules/UrlbarProviderWeather.sys.mjs",
});

const { WEATHER_SUGGESTION } = MerinoTestUtils;

add_task(async function init() {
  await QuickSuggestTestUtils.ensureQuickSuggestInit();
  UrlbarPrefs.set("quicksuggest.enabled", true);
  await MerinoTestUtils.initWeather();
});

// * Settings data: none
// * Nimbus values: none
// * Expected: zero prefix
add_task(async function() {
  await doKeywordsTest({
    tests: {
      "": true,
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
// * Expected: zero prefix
add_task(async function() {
  await doKeywordsTest({
    settingsData: {},
    tests: {
      "": true,
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
// * Expected: zero prefix
add_task(async function() {
  await doKeywordsTest({
    settingsData: {
      keywords: ["weather", "forecast"],
    },
    tests: {
      "": true,
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

// * Settings data: keywords and min keyword length = 0
// * Nimbus values: none
// * Expected: zero prefix
add_task(async function() {
  await doKeywordsTest({
    settingsData: {
      keywords: ["weather", "forecast"],
      min_keyword_length: 0,
    },
    tests: {
      "": true,
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

// * Settings data: keywords and min keyword length > 0
// * Nimbus values: none
// * Expected: use settings data
add_task(async function() {
  await doKeywordsTest({
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

// * Settings data: empty
// * Nimbus values: empty
// * Expected: zero prefix
add_task(async function() {
  await doKeywordsTest({
    settingsData: {},
    nimbusValues: {},
    tests: {
      "": true,
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
// * Expected: zero prefix
add_task(async function() {
  await doKeywordsTest({
    settingsData: {
      keywords: ["weather"],
    },
    nimbusValues: {
      weatherKeywords: ["forecast"],
    },
    tests: {
      "": true,
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

// * Settings data: keywords and min keyword length = 0
// * Nimbus values: keywords only
// * Expected: zero prefix
add_task(async function() {
  await doKeywordsTest({
    settingsData: {
      keywords: ["weather"],
      min_keyword_length: 0,
    },
    nimbusValues: {
      weatherKeywords: ["forecast"],
    },
    tests: {
      "": true,
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

// * Settings data: keywords and min keyword length > 0
// * Nimbus values: keywords only
// * Expected: Nimbus keywords with settings min keyword length
add_task(async function() {
  await doKeywordsTest({
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
// * Expected: Nimbus keywords with settings min keyword length
add_task(async function() {
  await doKeywordsTest({
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
// * Expected: use Nimbus values
add_task(async function() {
  await doKeywordsTest({
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

// * Settings data: none
// * Nimbus values: keywords only
// * Expected: zero prefix
add_task(async function() {
  await doKeywordsTest({
    nimbusValues: {
      weatherKeywords: ["weather", "forecast"],
    },
    tests: {
      "": true,
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

// * Settings data: none
// * Nimbus values: keywords and min keyword length = 0
// * Expected: zero prefix
add_task(async function() {
  await doKeywordsTest({
    nimbusValues: {
      weatherKeywords: ["weather", "forecast"],
      weatherKeywordsMinimumLength: 0,
    },
    tests: {
      "": true,
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

// * Settings data: none
// * Nimbus values: keywords and min keyword length > 0
// * Expected: use Nimbus values
add_task(async function() {
  await doKeywordsTest({
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

// When `weatherKeywords` is non-null and `weatherKeywordsMinimumLength` is
// larger than the length of all keywords, the suggestion should be triggered by
// typing a full keyword.
add_task(async function minLength_large() {
  await doKeywordsTest({
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

async function doKeywordsTest({
  tests,
  nimbusValues = null,
  settingsData = null,
}) {
  let nimbusCleanup;
  if (nimbusValues) {
    nimbusCleanup = await UrlbarTestUtils.initNimbusFeature(nimbusValues);
  }

  QuickSuggest.weather._test_setRsData(settingsData);

  for (let [searchString, expected] of Object.entries(tests)) {
    info(
      "Doing search: " +
        JSON.stringify({
          nimbusValues,
          settingsData,
          searchString,
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
  QuickSuggest.weather._test_setRsData(null);
}

// When a Nimbus experiment isn't active, the suggestion should be triggered on
// zero prefix. Installing and uninstalling an experiment should update the
// keywords.
add_task(async function zeroPrefix_withoutNimbus() {
  info("1. Doing searches before installing experiment and setting keyword");
  await check_results({
    context: createContext("", {
      providers: [UrlbarProviderWeather.name],
      isPrivate: false,
    }),
    matches: [makeExpectedResult()],
  });
  await check_results({
    context: createContext("weather", {
      providers: [UrlbarProviderWeather.name],
      isPrivate: false,
    }),
    matches: [],
  });

  let cleanup = await UrlbarTestUtils.initNimbusFeature({
    weatherKeywords: ["weather"],
    weatherKeywordsMinimumLength: 1,
  });

  info("2. Doing searches after installing experiment and setting keyword");
  await check_results({
    context: createContext("", {
      providers: [UrlbarProviderWeather.name],
      isPrivate: false,
    }),
    matches: [],
  });
  await check_results({
    context: createContext("weather", {
      providers: [UrlbarProviderWeather.name],
      isPrivate: false,
    }),
    matches: [makeExpectedResult({ suggestedIndex: 1 })],
  });

  await cleanup();

  info("3. Doing searches after uninstalling experiment");
  await check_results({
    context: createContext("", {
      providers: [UrlbarProviderWeather.name],
      isPrivate: false,
    }),
    matches: [makeExpectedResult()],
  });
  await check_results({
    context: createContext("weather", {
      providers: [UrlbarProviderWeather.name],
      isPrivate: false,
    }),
    matches: [],
  });
});

// When the zero-prefix suggestion is enabled, search strings with only spaces
// or that start with spaces should not trigger a weather suggestion.
add_task(async function zeroPrefix_spacesInSearchString() {
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

// When the zero-prefix suggestion is enabled, a weather suggestion should not
// be returned for a non-empty search string.
add_task(async function zeroPrefix_nonEmptySearchString() {
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
          helpUrl: QuickSuggest.HELP_URL,
          helpL10n: {
            id: UrlbarPrefs.get("resultMenu")
              ? "urlbar-result-menu-learn-more-about-firefox-suggest"
              : "firefox-suggest-urlbar-learn-more",
          },
          isBlockable: UrlbarPrefs.get("quickSuggestBlockingEnabled"),
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
