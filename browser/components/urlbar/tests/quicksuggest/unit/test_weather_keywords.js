/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

// Tests the keywords behavior of quick suggest weather.

"use strict";

ChromeUtils.defineESModuleGetters(this, {
  UrlbarProviderWeather: "resource:///modules/UrlbarProviderWeather.sys.mjs",
});

const { WEATHER_RS_DATA } = MerinoTestUtils;

add_setup(async () => {
  await QuickSuggestTestUtils.ensureQuickSuggestInit({
    remoteSettingsRecords: [
      {
        type: "weather",
        weather: WEATHER_RS_DATA,
      },
    ],
    prefs: [["suggest.quicksuggest.nonsponsored", true]],
  });
  await MerinoTestUtils.initWeather();

  // When `add_tasks_with_rust()` disables the Rust backend and forces sync, the
  // JS backend will sync `Weather` with remote settings. Since keywords are
  // present in remote settings at that point (we added them above), `Weather`
  // will then start fetching. The fetch may or may not be done before our test
  // task starts. To make sure it's done, queue another fetch and await it.
  registerAddTasksWithRustSetup(async () => {
    await QuickSuggest.weather._test_fetch();
  });
});

// * Settings data: none
// * Nimbus values: none
// * Min keyword length pref: none
// * Expected: no suggestion
add_tasks_with_rust(async function () {
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
add_tasks_with_rust(async function () {
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
//
// JS backend only. The Rust component expects settings data to contain
// min_keyword_length.
add_tasks_with_rust(
  {
    skip_if_rust_enabled: true,
  },
  async function () {
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
  }
);

// * Settings data: keywords and min keyword length = 0
// * Nimbus values: none
// * Min keyword length pref: none
// * Expected: full keywords only
//
// JS backend only. The Rust component doesn't treat minKeywordLength == 0 as a
// special case.
add_tasks_with_rust(
  {
    skip_if_rust_enabled: true,
  },
  async function () {
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
  }
);

// * Settings data: keywords and min keyword length > 0
// * Nimbus values: none
// * Min keyword length pref: none
// * Expected: use settings data
add_tasks_with_rust(async function () {
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
add_tasks_with_rust(async function () {
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
add_tasks_with_rust(async function () {
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
add_tasks_with_rust(async function () {
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
//
// JS backend only. The Rust component expects settings data to contain
// min_keyword_length.
add_tasks_with_rust(
  {
    skip_if_rust_enabled: true,
  },
  async function () {
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
  }
);

// * Settings data: keywords and min keyword length = 0
// * Nimbus values: keywords only
// * Min keyword length pref: none
// * Expected: full keywords in Nimbus
//
// JS backend only. The Rust component doesn't treat minKeywordLength == 0 as a
// special case.
add_tasks_with_rust(
  {
    skip_if_rust_enabled: true,
  },
  async function () {
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
  }
);

// * Settings data: keywords and min keyword length > 0
// * Nimbus values: keywords only
// * Min keyword length pref: none
// * Expected: Nimbus keywords with settings min keyword length.
//     Even when Rust is enabled, UrlbarProviderWeather should serve the
//     suggestion since the keywords come from Nimbus.
add_tasks_with_rust(async function () {
  await doKeywordsTest({
    desc: "Settings: keywords, min keyword length > 0; Nimbus: keywords",
    settingsData: {
      keywords: ["weather"],
      min_keyword_length: 3,
    },
    nimbusValues: {
      weatherKeywords: ["forecast"],
    },
    alwaysExpectMerinoResult: true,
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
// * Expected: Nimbus keywords with settings min keyword length.
//     Even when Rust is enabled, UrlbarProviderWeather should serve the
//     suggestion since the keywords come from Nimbus.
add_tasks_with_rust(async function () {
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
    alwaysExpectMerinoResult: true,
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
// * Expected: use Nimbus values.
//     Even when Rust is enabled, UrlbarProviderWeather should serve the
//     suggestion since the keywords come from Nimbus.
add_tasks_with_rust(async function () {
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
    alwaysExpectMerinoResult: true,
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
// * Expected: Nimbus keywords with min keyword length pref.
//     Even when Rust is enabled, UrlbarProviderWeather should serve the
//     suggestion since the keywords come from Nimbus.
add_tasks_with_rust(async function () {
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
    alwaysExpectMerinoResult: true,
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
//     Even when Rust is enabled, UrlbarProviderWeather should serve the
//     suggestion since the keywords come from Nimbus.
add_tasks_with_rust(async function () {
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
    alwaysExpectMerinoResult: true,
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
//
// TODO bug 1879209: This doesn't work with the Rust backend because if
// min_keyword_length isn't specified on ingest, the Rust database will retain
// the last known good min_keyword_length, which interferes with this task.
add_tasks_with_rust(
  {
    skip_if_rust_enabled: true,
  },
  async function () {
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
  }
);

// * Settings data: none
// * Nimbus values: keywords and min keyword length = 0
// * Min keyword length pref: none
// * Expected: full keywords
//
// TODO bug 1879209: This doesn't work with the Rust backend because if
// min_keyword_length isn't specified on ingest, the Rust database will retain
// the last known good min_keyword_length, which interferes with this task.
add_tasks_with_rust(
  {
    skip_if_rust_enabled: true,
  },
  async function () {
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
  }
);

// * Settings data: none
// * Nimbus values: keywords and min keyword length > 0
// * Min keyword length pref: none
// * Expected: use Nimbus values
//     Even when Rust is enabled, UrlbarProviderWeather should serve the
//     suggestion since the keywords come from Nimbus.
add_tasks_with_rust(async function () {
  await doKeywordsTest({
    desc: "Settings: none; Nimbus: keywords, min keyword length > 0",
    nimbusValues: {
      weatherKeywords: ["weather", "forecast"],
      weatherKeywordsMinimumLength: 3,
    },
    alwaysExpectMerinoResult: true,
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
//     Even when Rust is enabled, UrlbarProviderWeather should serve the
//     suggestion since the keywords come from Nimbus.
add_tasks_with_rust(async function () {
  await doKeywordsTest({
    desc: "Settings: none; Nimbus: keywords, min keyword length > 0; pref exists",
    nimbusValues: {
      weatherKeywords: ["weather", "forecast"],
      weatherKeywordsMinimumLength: 3,
    },
    minKeywordLength: 6,
    alwaysExpectMerinoResult: true,
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
//     Even when Rust is enabled, UrlbarProviderWeather should serve the
//     suggestion since the keywords come from Nimbus.
add_tasks_with_rust(async function minLength_large() {
  await doKeywordsTest({
    desc: "Large min length",
    nimbusValues: {
      weatherKeywords: ["weather", "forecast"],
      weatherKeywordsMinimumLength: 999,
    },
    alwaysExpectMerinoResult: true,
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
add_tasks_with_rust(async function leadingAndTrailingSpaces() {
  await doKeywordsTest({
    settingsData: {
      keywords: ["weather"],
      min_keyword_length: 3,
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

add_tasks_with_rust(async function caseInsensitive() {
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
  alwaysExpectMerinoResult = false,
}) {
  info("Doing keywords test: " + desc);
  info(JSON.stringify({ nimbusValues, settingsData, minKeywordLength }));

  // If the JS backend is enabled, a suggestion hasn't already been fetched, and
  // the data contains keywords, a fetch will start. Wait for it to finish later
  // below.
  let fetchPromise;
  if (
    !QuickSuggest.weather.suggestion &&
    !UrlbarPrefs.get("quickSuggestRustEnabled") &&
    (nimbusValues?.weatherKeywords || settingsData?.keywords)
  ) {
    fetchPromise = waitForNewWeatherFetch();
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
    await fetchPromise;
    info("Got fetch");
  }

  let expectedResult = makeWeatherResult(
    !alwaysExpectMerinoResult
      ? undefined
      : { source: "merino", provider: "accuweather", telemetryType: null }
  );

  for (let [searchString, expected] of Object.entries(tests)) {
    info(
      "Doing keywords test search: " +
        JSON.stringify({
          searchString,
          expected,
        })
    );

    await check_results({
      context: createContext(searchString, {
        providers: [
          UrlbarProviderQuickSuggest.name,
          UrlbarProviderWeather.name,
        ],
        isPrivate: false,
      }),
      matches: expected ? [expectedResult] : [],
    });
  }

  await nimbusCleanup?.();

  if (!QuickSuggest.weather.suggestion) {
    fetchPromise = waitForNewWeatherFetch();
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
add_tasks_with_rust(async function matchingQuickSuggest_sponsored() {
  await doMatchingQuickSuggestTest("suggest.quicksuggest.sponsored", true);
});

// When a non-sponsored quick suggest result matches the same keyword as the
// weather result, the weather result should be shown and the quick suggest
// result should not be shown.
add_tasks_with_rust(async function matchingQuickSuggest_nonsponsored() {
  await doMatchingQuickSuggestTest("suggest.quicksuggest.nonsponsored", false);
});

async function doMatchingQuickSuggestTest(pref, isSponsored) {
  let keyword = "test";

  let attachment = isSponsored
    ? QuickSuggestTestUtils.ampRemoteSettings({ keywords: [keyword] })
    : QuickSuggestTestUtils.wikipediaRemoteSettings({ keywords: [keyword] });

  // Add a remote settings result to quick suggest.
  let oldPrefValue = UrlbarPrefs.get(pref);
  UrlbarPrefs.set(pref, true);
  await QuickSuggestTestUtils.setRemoteSettingsRecords([
    {
      type: "data",
      attachment: [attachment],
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
      isSponsored
        ? makeAmpResult({ keyword })
        : makeWikipediaResult({ keyword }),
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
    // The result should always come from Merino.
    matches: [
      makeWeatherResult({
        source: "merino",
        provider: "accuweather",
        telemetryType: null,
      }),
    ],
  });
  await cleanup();

  UrlbarPrefs.set(pref, oldPrefValue);
}

add_tasks_with_rust(async function () {
  await doIncrementTest({
    desc: "Settings only without cap",
    setup: {
      settingsData: {
        weather: {
          keywords: ["forecast", "wind"],
          min_keyword_length: 3,
        },
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

add_tasks_with_rust(async function () {
  await doIncrementTest({
    desc: "Settings only with cap",
    setup: {
      settingsData: {
        weather: {
          keywords: ["forecast", "wind"],
          min_keyword_length: 3,
        },
        configuration: {
          show_less_frequently_cap: 3,
        },
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

add_tasks_with_rust(async function () {
  await doIncrementTest({
    desc: "Settings and Nimbus without cap",
    setup: {
      settingsData: {
        weather: {
          keywords: ["weather"],
          min_keyword_length: 5,
        },
      },
      nimbusValues: {
        weatherKeywords: ["forecast", "wind"],
        weatherKeywordsMinimumLength: 3,
      },
    },
    // The suggestion should be served by UrlbarProviderWeather and therefore
    // be from Merino.
    alwaysExpectMerinoResult: true,
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
        weather: {
          keywords: ["weather"],
          min_keyword_length: 5,
        },
      },
      nimbusValues: {
        weatherKeywords: ["forecast", "wind"],
        weatherKeywordsMinimumLength: 3,
        weatherKeywordsMinimumLengthCap: 6,
      },
    },
    // The suggestion should be served by UrlbarProviderWeather and therefore
    // be from Merino.
    alwaysExpectMerinoResult: true,
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

async function doIncrementTest({
  desc,
  setup,
  tests,
  alwaysExpectMerinoResult = false,
}) {
  info("Doing increment test: " + desc);
  info(JSON.stringify({ setup }));

  let { nimbusValues, settingsData } = setup;

  // If the JS backend is enabled, a suggestion hasn't already been fetched, and
  // the data contains keywords, a fetch will start. Wait for it to finish later
  // below.
  let fetchPromise;
  if (
    !QuickSuggest.weather.suggestion &&
    !UrlbarPrefs.get("quickSuggestRustEnabled") &&
    (nimbusValues?.weatherKeywords || settingsData?.weather?.keywords)
  ) {
    fetchPromise = waitForNewWeatherFetch();
  }

  let nimbusCleanup;
  if (nimbusValues) {
    nimbusCleanup = await UrlbarTestUtils.initNimbusFeature(nimbusValues);
  }

  await QuickSuggestTestUtils.setRemoteSettingsRecords([
    {
      type: "weather",
      weather: settingsData?.weather,
    },
    {
      type: "configuration",
      configuration: settingsData?.configuration,
    },
  ]);

  if (fetchPromise) {
    info("Waiting for fetch");
    await fetchPromise;
    info("Got fetch");
  }

  let expectedResult = makeWeatherResult(
    !alwaysExpectMerinoResult
      ? undefined
      : { source: "merino", provider: "accuweather", telemetryType: null }
  );

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
      await check_results({
        context: createContext(searchString, {
          providers: [
            UrlbarProviderQuickSuggest.name,
            UrlbarProviderWeather.name,
          ],
          isPrivate: false,
        }),
        matches: expected ? [expectedResult] : [],
      });
    }

    QuickSuggest.weather.incrementMinKeywordLength();
    info(
      "Incremented min keyword length, new value is: " +
        QuickSuggest.weather.minKeywordLength
    );
  }

  await nimbusCleanup?.();

  if (!QuickSuggest.weather.suggestion) {
    fetchPromise = waitForNewWeatherFetch();
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
