/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

// Tests Yelp suggestions.

"use strict";

const REMOTE_SETTINGS_RECORDS = [
  {
    type: "yelp-suggestions",
    attachment: {
      subjects: ["ramen"],
      preModifiers: ["best"],
      postModifiers: ["delivery"],
      locationSigns: [{ keyword: "in", needLocation: true }],
      yelpModifiers: [],
    },
  },
];

add_setup(async function () {
  Services.prefs.setBoolPref("browser.search.suggest.enabled", false);

  await QuickSuggestTestUtils.ensureQuickSuggestInit({
    remoteSettingsRecords: REMOTE_SETTINGS_RECORDS,
    prefs: [
      ["quicksuggest.rustEnabled", true],
      ["suggest.quicksuggest.sponsored", true],
      ["suggest.yelp", true],
      ["yelp.featureGate", true],
    ],
  });

  await MerinoTestUtils.initGeolocation();
});

add_task(async function basic() {
  const TEST_DATA = [
    {
      description: "Basic",
      query: "best ramen delivery in tokyo",
      expected: {
        url: "https://www.yelp.com/search?find_desc=best+ramen+delivery&find_loc=tokyo",
        title: "best ramen delivery in tokyo",
      },
    },
    {
      description: "With upper case",
      query: "BeSt RaMeN dElIvErY iN tOkYo",
      expected: {
        url: "https://www.yelp.com/search?find_desc=BeSt+RaMeN+dElIvErY&find_loc=tOkYo",
        title: "BeSt RaMeN dElIvErY iN tOkYo",
      },
    },
    {
      description: "No specific location",
      query: "ramen",
      expected: {
        url: "https://www.yelp.com/search?find_desc=ramen&find_loc=Yokohama%2C+Kanagawa",
        displayUrl:
          "yelp.com/search?find_desc=ramen&find_loc=Yokohama,+Kanagawa",
        title: "ramen in Yokohama, Kanagawa",
      },
    },
  ];

  for (let { query, expected } of TEST_DATA) {
    info(`Test for ${query}`);

    await check_results({
      context: createContext(query, {
        providers: [UrlbarProviderQuickSuggest.name],
        isPrivate: false,
      }),
      matches: expected ? [makeExpectedResult(expected)] : [],
    });
  }
});

add_task(async function telemetryType() {
  Assert.equal(
    QuickSuggest.getFeature("YelpSuggestions").getSuggestionTelemetryType({}),
    "yelp",
    "Telemetry type should be 'yelp'"
  );
});

// When sponsored suggestions are disabled, Yelp suggestions should be
// disabled.
add_task(async function sponsoredDisabled() {
  UrlbarPrefs.set("suggest.quicksuggest.nonsponsored", false);

  // First make sure the suggestion is added when non-sponsored
  // suggestions are enabled, if the rust is enabled.
  await check_results({
    context: createContext("ramen in tokyo", {
      providers: [UrlbarProviderQuickSuggest.name],
      isPrivate: false,
    }),
    matches: [
      makeExpectedResult({
        url: "https://www.yelp.com/search?find_desc=ramen&find_loc=tokyo",
        title: "ramen in tokyo",
      }),
    ],
  });

  // Now disable the pref.
  UrlbarPrefs.set("suggest.quicksuggest.sponsored", false);
  await check_results({
    context: createContext("ramen in tokyo", {
      providers: [UrlbarProviderQuickSuggest.name],
      isPrivate: false,
    }),
    matches: [],
  });

  UrlbarPrefs.set("suggest.quicksuggest.sponsored", true);
  UrlbarPrefs.clear("suggest.quicksuggest.nonsponsored");
  await QuickSuggestTestUtils.forceSync();
});

// When Yelp-specific preferences are disabled, suggestions should not be
// added.
add_task(async function yelpSpecificPrefsDisabled() {
  const prefs = ["suggest.yelp", "yelp.featureGate"];
  for (const pref of prefs) {
    // First make sure the suggestion is added, if the rust is enabled.
    await check_results({
      context: createContext("ramen in tokyo", {
        providers: [UrlbarProviderQuickSuggest.name],
        isPrivate: false,
      }),
      matches: [
        makeExpectedResult({
          url: "https://www.yelp.com/search?find_desc=ramen&find_loc=tokyo",
          title: "ramen in tokyo",
        }),
      ],
    });

    // Now disable the pref.
    UrlbarPrefs.set(pref, false);
    await check_results({
      context: createContext("ramen in tokyo", {
        providers: [UrlbarProviderQuickSuggest.name],
        isPrivate: false,
      }),
      matches: [],
    });

    // Revert.
    UrlbarPrefs.set(pref, true);
    await QuickSuggestTestUtils.forceSync();
  }
});

// Check wheather the Yelp suggestions will be shown by the setup of Nimbus
// variable.
add_task(async function featureGate() {
  // Disable the fature gate.
  UrlbarPrefs.set("yelp.featureGate", false);
  await check_results({
    context: createContext("ramem in tokyo", {
      providers: [UrlbarProviderQuickSuggest.name],
      isPrivate: false,
    }),
    matches: [],
  });

  // Enable by Nimbus.
  const cleanUpNimbusEnable = await UrlbarTestUtils.initNimbusFeature({
    yelpFeatureGate: true,
  });
  await QuickSuggestTestUtils.forceSync();
  await check_results({
    context: createContext("ramen in tokyo", {
      providers: [UrlbarProviderQuickSuggest.name],
      isPrivate: false,
    }),
    matches: [
      makeExpectedResult({
        url: "https://www.yelp.com/search?find_desc=ramen&find_loc=tokyo",
        title: "ramen in tokyo",
      }),
    ],
  });
  await cleanUpNimbusEnable();

  // Enable locally.
  UrlbarPrefs.set("yelp.featureGate", true);
  await QuickSuggestTestUtils.forceSync();

  // Disable by Nimbus.
  const cleanUpNimbusDisable = await UrlbarTestUtils.initNimbusFeature({
    yelpFeatureGate: false,
  });
  await check_results({
    context: createContext("ramen in tokyo", {
      providers: [UrlbarProviderQuickSuggest.name],
      isPrivate: false,
    }),
    matches: [],
  });
  await cleanUpNimbusDisable();

  // Revert.
  UrlbarPrefs.set("yelp.featureGate", true);
  await QuickSuggestTestUtils.forceSync();
});

// Check wheather the Yelp suggestions will be shown as top_pick by the Nimbus
// variable.
add_task(async function yelpSuggestPriority() {
  // Enable by Nimbus.
  const cleanUpNimbusEnable = await UrlbarTestUtils.initNimbusFeature({
    yelpSuggestPriority: true,
  });
  await QuickSuggestTestUtils.forceSync();

  await check_results({
    context: createContext("ramen in tokyo", {
      providers: [UrlbarProviderQuickSuggest.name],
      isPrivate: false,
    }),
    matches: [
      makeExpectedResult({
        url: "https://www.yelp.com/search?find_desc=ramen&find_loc=tokyo",
        title: "ramen in tokyo",
        isTopPick: true,
      }),
    ],
  });

  await cleanUpNimbusEnable();
  await QuickSuggestTestUtils.forceSync();

  await check_results({
    context: createContext("ramen in tokyo", {
      providers: [UrlbarProviderQuickSuggest.name],
      isPrivate: false,
    }),
    matches: [
      makeExpectedResult({
        url: "https://www.yelp.com/search?find_desc=ramen&find_loc=tokyo",
        title: "ramen in tokyo",
        isTopPick: false,
      }),
    ],
  });
});

// The `Yelp` Rust provider should be passed to the Rust component when
// querying depending on whether Yelp suggestions are enabled.
add_task(async function rustProviders() {
  await doRustProvidersTests({
    searchString: "ramen in tokyo",
    tests: [
      {
        prefs: {
          "suggest.yelp": true,
        },
        expectedUrls: [
          "https://www.yelp.com/search?find_desc=ramen&find_loc=tokyo",
        ],
      },
      {
        prefs: {
          "suggest.yelp": false,
        },
        expectedUrls: [],
      },
    ],
  });

  UrlbarPrefs.clear("suggest.yelp");
  await QuickSuggestTestUtils.forceSync();
});

function makeExpectedResult(expected) {
  const utmParameters = "&utm_medium=partner&utm_source=mozilla";

  let url = expected.url + utmParameters;
  let displayUrl =
    (expected.displayUrl ??
      expected.url.replace(/^https:\/\/www[.]/, "").replace("%20", " ")) +
    utmParameters;

  return {
    type: UrlbarUtils.RESULT_TYPE.URL,
    source: UrlbarUtils.RESULT_SOURCE.SEARCH,
    isBestMatch: expected.isTopPick ?? false,
    heuristic: false,
    payload: {
      source: "rust",
      provider: "Yelp",
      telemetryType: "yelp",
      shouldShowUrl: true,
      bottomTextL10n: { id: "firefox-suggest-yelp-bottom-text" },
      url,
      title: expected.title,
      displayUrl,
    },
  };
}
