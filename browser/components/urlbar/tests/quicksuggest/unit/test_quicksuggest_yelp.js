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
      ["suggest.quicksuggest.nonsponsored", true],
      ["suggest.yelp", true],
      ["yelp.featureGate", true],
    ],
  });
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

// When non-sponsored suggestions are disabled, Yelp suggestions should be
// disabled.
add_task(async function nonsponsoredDisabled() {
  UrlbarPrefs.set("suggest.quicksuggest.sponsored", false);

  // First make sure the suggestion is added when non-sponsored
  // suggestions are enabled, if the rust is enabled.
  await check_results({
    context: createContext("ramen", {
      providers: [UrlbarProviderQuickSuggest.name],
      isPrivate: false,
    }),
    matches: [
      makeExpectedResult({
        url: "https://www.yelp.com/search?find_desc=ramen",
        title: "ramen",
      }),
    ],
  });

  // Now disable the pref.
  UrlbarPrefs.set("suggest.quicksuggest.nonsponsored", false);
  await check_results({
    context: createContext("ramen", {
      providers: [UrlbarProviderQuickSuggest.name],
      isPrivate: false,
    }),
    matches: [],
  });

  UrlbarPrefs.set("suggest.quicksuggest.nonsponsored", true);
  UrlbarPrefs.clear("suggest.quicksuggest.sponsored");
  await QuickSuggestTestUtils.forceSync();
});

// When Yelp-specific preferences are disabled, suggestions should not be
// added.
add_task(async function yelpSpecificPrefsDisabled() {
  const prefs = ["suggest.yelp", "yelp.featureGate"];
  for (const pref of prefs) {
    // First make sure the suggestion is added, if the rust is enabled.
    await check_results({
      context: createContext("ramen", {
        providers: [UrlbarProviderQuickSuggest.name],
        isPrivate: false,
      }),
      matches: [
        makeExpectedResult({
          url: "https://www.yelp.com/search?find_desc=ramen",
          title: "ramen",
        }),
      ],
    });

    // Now disable the pref.
    UrlbarPrefs.set(pref, false);
    await check_results({
      context: createContext("ramen", {
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
add_task(async function nimbus() {
  // Disable the fature gate.
  UrlbarPrefs.set("yelp.featureGate", false);
  await check_results({
    context: createContext("ramem", {
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
    context: createContext("ramen", {
      providers: [UrlbarProviderQuickSuggest.name],
      isPrivate: false,
    }),
    matches: [
      makeExpectedResult({
        url: "https://www.yelp.com/search?find_desc=ramen",
        title: "ramen",
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
    context: createContext("ramen", {
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

// The `Yelp` Rust provider should be passed to the Rust component when
// querying depending on whether Yelp suggestions are enabled.
add_task(async function rustProviders() {
  await doRustProvidersTests({
    searchString: "ramen",
    tests: [
      {
        prefs: {
          "suggest.yelp": true,
        },
        expectedUrls: ["https://www.yelp.com/search?find_desc=ramen"],
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
  return {
    type: UrlbarUtils.RESULT_TYPE.URL,
    source: UrlbarUtils.RESULT_SOURCE.SEARCH,
    isBestMatch: true,
    heuristic: false,
    payload: {
      source: "rust",
      provider: "Yelp",
      telemetryType: "yelp",
      shouldShowUrl: true,
      bottomTextL10n: { id: "firefox-suggest-yelp-bottom-text" },
      url: expected.url,
      title: expected.title,
      displayUrl: expected.url
        .replace(/^https:\/\/www[.]/, "")
        .replace("%20", " "),
    },
  };
}
