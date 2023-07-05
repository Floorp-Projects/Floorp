/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

// Tests addon quick suggest results.

"use strict";

ChromeUtils.defineESModuleGetters(this, {
  AddonManager: "resource://gre/modules/AddonManager.sys.mjs",
  ExtensionTestCommon: "resource://testing-common/ExtensionTestCommon.sys.mjs",
});

const MERINO_SUGGESTIONS = [
  {
    provider: "amo",
    icon: "icon",
    url: "url",
    title: "title",
    description: "description",
    is_top_pick: true,
    custom_details: {
      amo: {
        rating: "5",
        number_of_ratings: "1234567",
        guid: "test@addon",
      },
    },
  },
];

const REMOTE_SETTINGS_RESULTS = [
  {
    type: "amo-suggestions",
    attachment: [
      {
        url: "https://example.com/first-addon",
        guid: "first@addon",
        icon: "https://example.com/first-addon.svg",
        title: "First Addon",
        rating: "4.7",
        keywords: ["first", "1st", "two words", "a b c"],
        description: "Description for the First Addon",
        number_of_ratings: 1256,
        is_top_pick: true,
      },
      {
        url: "https://example.com/second-addon",
        guid: "second@addon",
        icon: "https://example.com/second-addon.svg",
        title: "Second Addon",
        rating: "1.7",
        keywords: ["second", "2nd"],
        description: "Description for the Second Addon",
        number_of_ratings: 256,
        is_top_pick: false,
      },
      {
        url: "https://example.com/third-addon",
        guid: "third@addon",
        icon: "https://example.com/third-addon.svg",
        title: "Third Addon",
        rating: "3.7",
        keywords: ["third", "3rd"],
        description: "Description for the Third Addon",
        number_of_ratings: 3,
      },
    ],
  },
];

add_setup(async function init() {
  UrlbarPrefs.set("quicksuggest.enabled", true);
  UrlbarPrefs.set("bestMatch.enabled", true);
  UrlbarPrefs.set("suggest.quicksuggest.nonsponsored", true);
  UrlbarPrefs.set("addons.featureGate", true);

  // Disable search suggestions so we don't hit the network.
  Services.prefs.setBoolPref("browser.search.suggest.enabled", false);

  await QuickSuggestTestUtils.ensureQuickSuggestInit({
    remoteSettingsResults: REMOTE_SETTINGS_RESULTS,
    merinoSuggestions: MERINO_SUGGESTIONS,
  });
});

add_task(async function telemetryType() {
  Assert.equal(
    QuickSuggest.getFeature("AddonSuggestions").getSuggestionTelemetryType({}),
    "amo",
    "Telemetry type should be 'amo'"
  );
});

// When non-sponsored suggestions are disabled, addon suggestions should be
// disabled.
add_task(async function nonsponsoredDisabled() {
  // Disable sponsored suggestions. Addon suggestions are non-sponsored, so
  // doing this should not prevent them from being enabled.
  UrlbarPrefs.set("suggest.quicksuggest.sponsored", false);

  // First make sure the suggestion is added when non-sponsored suggestions are
  // enabled.
  await check_results({
    context: createContext("test", {
      providers: [UrlbarProviderQuickSuggest.name],
      isPrivate: false,
    }),
    matches: [
      makeExpectedResult({
        suggestion: MERINO_SUGGESTIONS[0],
        source: "merino",
        isTopPick: true,
      }),
    ],
  });

  // Now disable them.
  UrlbarPrefs.set("suggest.quicksuggest.nonsponsored", false);
  await check_results({
    context: createContext("test", {
      providers: [UrlbarProviderQuickSuggest.name],
      isPrivate: false,
    }),
    matches: [],
  });

  UrlbarPrefs.set("suggest.quicksuggest.nonsponsored", true);
  UrlbarPrefs.clear("suggest.quicksuggest.sponsored");
});

// When addon suggestions specific preference is disabled, addon suggestions
// should not be added.
add_task(async function addonSuggestionsSpecificPrefDisabled() {
  const prefs = ["suggest.addons", "addons.featureGate"];
  for (const pref of prefs) {
    // First make sure the suggestion is added.
    await check_results({
      context: createContext("test", {
        providers: [UrlbarProviderQuickSuggest.name],
        isPrivate: false,
      }),
      matches: [
        makeExpectedResult({
          suggestion: MERINO_SUGGESTIONS[0],
          source: "merino",
          isTopPick: true,
        }),
      ],
    });

    // Now disable the pref.
    UrlbarPrefs.set(pref, false);
    await check_results({
      context: createContext("test", {
        providers: [UrlbarProviderQuickSuggest.name],
        isPrivate: false,
      }),
      matches: [],
    });

    // Revert.
    UrlbarPrefs.set(pref, true);
  }
});

// Check wheather the addon suggestions will be shown by the setup of Nimbus
// variable.
add_task(async function nimbus() {
  // Disable the fature gate.
  UrlbarPrefs.set("addons.featureGate", false);
  await check_results({
    context: createContext("test", {
      providers: [UrlbarProviderQuickSuggest.name],
      isPrivate: false,
    }),
    matches: [],
  });

  // Enable by Nimbus.
  const cleanUpNimbusEnable = await UrlbarTestUtils.initNimbusFeature({
    addonsFeatureGate: true,
  });
  await check_results({
    context: createContext("test", {
      providers: [UrlbarProviderQuickSuggest.name],
      isPrivate: false,
    }),
    matches: [
      makeExpectedResult({
        suggestion: MERINO_SUGGESTIONS[0],
        source: "merino",
        isTopPick: true,
      }),
    ],
  });
  await cleanUpNimbusEnable();

  // Enable locally.
  UrlbarPrefs.set("addons.featureGate", true);

  // Disable by Nimbus.
  const cleanUpNimbusDisable = await UrlbarTestUtils.initNimbusFeature({
    addonsFeatureGate: false,
  });
  await check_results({
    context: createContext("test", {
      providers: [UrlbarProviderQuickSuggest.name],
      isPrivate: false,
    }),
    matches: [],
  });
  await cleanUpNimbusDisable();

  // Revert.
  UrlbarPrefs.set("addons.featureGate", true);
});

add_task(async function hideIfAlreadyInstalled() {
  // Show suggestion.
  await check_results({
    context: createContext("test", {
      providers: [UrlbarProviderQuickSuggest.name],
      isPrivate: false,
    }),
    matches: [
      makeExpectedResult({
        suggestion: MERINO_SUGGESTIONS[0],
        source: "merino",
        isTopPick: true,
      }),
    ],
  });

  // Install an addon for the suggestion.
  const xpi = ExtensionTestCommon.generateXPI({
    manifest: {
      browser_specific_settings: {
        gecko: { id: "test@addon" },
      },
    },
  });
  const addon = await AddonManager.installTemporaryAddon(xpi);

  // Show suggestion for the addon installed.
  await check_results({
    context: createContext("test", {
      providers: [UrlbarProviderQuickSuggest.name],
      isPrivate: false,
    }),
    matches: [],
  });

  await addon.uninstall();
  xpi.remove(false);
});

add_task(async function remoteSettings() {
  const testCases = [
    {
      input: "f",
      expected: null,
    },
    {
      input: "fi",
      expected: null,
    },
    {
      input: "fir",
      expected: null,
    },
    {
      input: "firs",
      expected: null,
    },
    {
      input: "first",
      expected: makeExpectedResult({
        suggestion: REMOTE_SETTINGS_RESULTS[0].attachment[0],
        source: "remote-settings",
        isTopPick: true,
      }),
    },
    {
      input: "1st",
      expected: makeExpectedResult({
        suggestion: REMOTE_SETTINGS_RESULTS[0].attachment[0],
        source: "remote-settings",
        isTopPick: true,
      }),
    },
    {
      input: "t",
      expected: null,
    },
    {
      input: "tw",
      expected: null,
    },
    {
      input: "two",
      expected: makeExpectedResult({
        suggestion: REMOTE_SETTINGS_RESULTS[0].attachment[0],
        source: "remote-settings",
        isTopPick: true,
      }),
    },
    {
      input: "two ",
      expected: makeExpectedResult({
        suggestion: REMOTE_SETTINGS_RESULTS[0].attachment[0],
        source: "remote-settings",
        isTopPick: true,
      }),
    },
    {
      input: "two w",
      expected: makeExpectedResult({
        suggestion: REMOTE_SETTINGS_RESULTS[0].attachment[0],
        source: "remote-settings",
        isTopPick: true,
      }),
    },
    {
      input: "two wo",
      expected: makeExpectedResult({
        suggestion: REMOTE_SETTINGS_RESULTS[0].attachment[0],
        source: "remote-settings",
        isTopPick: true,
      }),
    },
    {
      input: "two wor",
      expected: makeExpectedResult({
        suggestion: REMOTE_SETTINGS_RESULTS[0].attachment[0],
        source: "remote-settings",
        isTopPick: true,
      }),
    },
    {
      input: "two word",
      expected: makeExpectedResult({
        suggestion: REMOTE_SETTINGS_RESULTS[0].attachment[0],
        source: "remote-settings",
        isTopPick: true,
      }),
    },
    {
      input: "two words",
      expected: makeExpectedResult({
        suggestion: REMOTE_SETTINGS_RESULTS[0].attachment[0],
        source: "remote-settings",
        isTopPick: true,
      }),
    },
    {
      input: "a",
      expected: makeExpectedResult({
        suggestion: REMOTE_SETTINGS_RESULTS[0].attachment[0],
        source: "remote-settings",
        isTopPick: true,
      }),
    },
    {
      input: "a ",
      expected: makeExpectedResult({
        suggestion: REMOTE_SETTINGS_RESULTS[0].attachment[0],
        source: "remote-settings",
        isTopPick: true,
      }),
    },
    {
      input: "a b",
      expected: makeExpectedResult({
        suggestion: REMOTE_SETTINGS_RESULTS[0].attachment[0],
        source: "remote-settings",
        isTopPick: true,
      }),
    },
    {
      input: "a b ",
      expected: makeExpectedResult({
        suggestion: REMOTE_SETTINGS_RESULTS[0].attachment[0],
        source: "remote-settings",
        isTopPick: true,
      }),
    },
    {
      input: "a b c",
      expected: makeExpectedResult({
        suggestion: REMOTE_SETTINGS_RESULTS[0].attachment[0],
        source: "remote-settings",
        isTopPick: true,
      }),
    },
    {
      input: "second",
      expected: makeExpectedResult({
        suggestion: REMOTE_SETTINGS_RESULTS[0].attachment[1],
        source: "remote-settings",
        isTopPick: false,
      }),
    },
    {
      input: "2nd",
      expected: makeExpectedResult({
        suggestion: REMOTE_SETTINGS_RESULTS[0].attachment[1],
        source: "remote-settings",
        isTopPick: false,
      }),
    },
    {
      input: "third",
      expected: makeExpectedResult({
        suggestion: REMOTE_SETTINGS_RESULTS[0].attachment[2],
        source: "remote-settings",
        isTopPick: true,
      }),
    },
    {
      input: "3rd",
      expected: makeExpectedResult({
        suggestion: REMOTE_SETTINGS_RESULTS[0].attachment[2],
        source: "remote-settings",
        isTopPick: true,
      }),
    },
  ];

  // Disable Merino so we trigger only remote settings suggestions.
  UrlbarPrefs.set("quicksuggest.dataCollection.enabled", false);

  for (const { input, expected } of testCases) {
    await check_results({
      context: createContext(input, {
        providers: [UrlbarProviderQuickSuggest.name],
        isPrivate: false,
      }),
      matches: expected ? [expected] : [],
    });
  }

  UrlbarPrefs.set("quicksuggest.dataCollection.enabled", true);
});

add_task(async function merinoIsTopPick() {
  const suggestion = JSON.parse(JSON.stringify(MERINO_SUGGESTIONS[0]));

  // is_top_pick is specified as false.
  suggestion.is_top_pick = false;
  MerinoTestUtils.server.response.body.suggestions = [suggestion];
  await check_results({
    context: createContext("test", {
      providers: [UrlbarProviderQuickSuggest.name],
      isPrivate: false,
    }),
    matches: [
      makeExpectedResult({
        suggestion,
        source: "merino",
        isTopPick: false,
      }),
    ],
  });

  // is_top_pick is undefined.
  delete suggestion.is_top_pick;
  MerinoTestUtils.server.response.body.suggestions = [suggestion];
  await check_results({
    context: createContext("test", {
      providers: [UrlbarProviderQuickSuggest.name],
      isPrivate: false,
    }),
    matches: [
      makeExpectedResult({
        suggestion,
        source: "merino",
        isTopPick: true,
      }),
    ],
  });
});

// Tests the "show less frequently" behavior.
add_task(async function showLessFrequently() {
  await doShowLessFrequentlyTests({
    feature: QuickSuggest.getFeature("AddonSuggestions"),
    showLessFrequentlyCountPref: "addons.showLessFrequentlyCount",
    nimbusCapVariable: "addonsShowLessFrequentlyCap",
    expectedResult: makeExpectedResult({
      suggestion: REMOTE_SETTINGS_RESULTS[0].attachment[0],
      source: "remote-settings",
      isTopPick: true,
    }),
    keyword: "two words",
  });
});

function makeExpectedResult({ suggestion, source, isTopPick }) {
  let provider;
  let rating;
  let number_of_ratings;
  if (source === "remote-settings") {
    provider = "AddonSuggestions";
    rating = suggestion.rating;
    number_of_ratings = suggestion.number_of_ratings;
  } else {
    provider = "amo";
    rating = suggestion.custom_details.amo.rating;
    number_of_ratings = suggestion.custom_details.amo.number_of_ratings;
  }

  return {
    isBestMatch: isTopPick,
    suggestedIndex: isTopPick ? 1 : -1,
    type: UrlbarUtils.RESULT_TYPE.DYNAMIC,
    source: UrlbarUtils.RESULT_SOURCE.SEARCH,
    heuristic: false,
    payload: {
      telemetryType: "amo",
      dynamicType: "addons",
      title: suggestion.title,
      url: suggestion.url,
      displayUrl: suggestion.url.replace(/^https:\/\//, ""),
      icon: suggestion.icon,
      description: suggestion.description,
      rating: Number(rating),
      reviews: Number(number_of_ratings),
      shouldNavigate: true,
      helpUrl: QuickSuggest.HELP_URL,
      source,
      provider,
    },
  };
}
