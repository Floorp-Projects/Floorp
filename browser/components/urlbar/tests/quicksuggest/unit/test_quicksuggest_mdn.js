/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

// Tests Pocket quick suggest results.

"use strict";

const REMOTE_SETTINGS_DATA = [
  {
    type: "mdn-suggestions",
    attachment: [
      {
        url: "https://example.com/array-filter",
        title: "Array.prototype.filter()",
        description:
          "The filter() method creates a shallow copy of a portion of a given array, filtered down to just the elements from the given array that pass the test implemented by the provided function.",
        is_top_pick: true,
        keywords: ["array filter"],
      },
      {
        url: "https://example.com/input",
        title: "<input>: The Input (Form Input) element",
        description:
          "The <input> HTML element is used to create interactive controls for web-based forms in order to accept data from the user; a wide variety of types of input data and control widgets are available, depending on the device and user agent. The <input> element is one of the most powerful and complex in all of HTML due to the sheer number of combinations of input types and attributes.",
        is_top_pick: false,
        keywords: ["input"],
      },
      {
        url: "https://example.com/grid",
        title: "CSS Grid Layout",
        description:
          "CSS Grid Layout excels at dividing a page into major regions or defining the relationship in terms of size, position, and layer, between parts of a control built from HTML primitives.",
        keywords: ["grid"],
      },
    ],
  },
];

add_setup(async function init() {
  UrlbarPrefs.set("quicksuggest.enabled", true);
  UrlbarPrefs.set("bestMatch.enabled", true);
  UrlbarPrefs.set("suggest.quicksuggest.nonsponsored", true);
  UrlbarPrefs.set("suggest.quicksuggest.sponsored", false);
  UrlbarPrefs.set("suggest.mdn", true);
  UrlbarPrefs.set("mdn.featureGate", true);

  // Disable search suggestions so we don't hit the network.
  Services.prefs.setBoolPref("browser.search.suggest.enabled", false);

  await QuickSuggestTestUtils.ensureQuickSuggestInit({
    remoteSettingsResults: REMOTE_SETTINGS_DATA,
  });
  await waitForSuggestions();
});

add_task(async function basic() {
  for (const suggestion of REMOTE_SETTINGS_DATA[0].attachment) {
    const fullKeyword = suggestion.keywords[0];
    const firstWord = fullKeyword.split(" ")[0];
    for (let i = 1; i < fullKeyword.length; i++) {
      const keyword = fullKeyword.substring(0, i);
      const shouldMatch = i >= firstWord.length;
      const matches = shouldMatch
        ? [makeExpectedResult({ searchString: keyword, suggestion })]
        : [];
      await check_results({
        context: createContext(keyword, {
          providers: [UrlbarProviderQuickSuggest.name],
          isPrivate: false,
        }),
        matches,
      });
    }

    await check_results({
      context: createContext(fullKeyword + " ", {
        providers: [UrlbarProviderQuickSuggest.name],
        isPrivate: false,
      }),
      matches: [],
    });
  }
});

// Check wheather the MDN suggestions will be hidden by the pref.
add_task(async function disableByLocalPref() {
  const suggestion = REMOTE_SETTINGS_DATA[0].attachment[0];
  const keyword = suggestion.keywords[0];

  const prefs = [
    "suggest.mdn",
    "quicksuggest.enabled",
    "suggest.quicksuggest.nonsponsored",
  ];

  for (const pref of prefs) {
    // First make sure the suggestion is added.
    await check_results({
      context: createContext(keyword, {
        providers: [UrlbarProviderQuickSuggest.name],
        isPrivate: false,
      }),
      matches: [
        makeExpectedResult({
          searchString: keyword,
          suggestion,
        }),
      ],
    });

    // Now disable them.
    UrlbarPrefs.set(pref, false);
    await check_results({
      context: createContext(keyword, {
        providers: [UrlbarProviderQuickSuggest.name],
        isPrivate: false,
      }),
      matches: [],
    });

    // Revert.
    UrlbarPrefs.set(pref, true);
    await waitForSuggestions();
  }
});

// Check wheather the MDN suggestions will be shown by the setup of Nimbus
// variable.
add_task(async function nimbus() {
  // Nimbus variable mdn.featureGate changes the pref in default branch
  // (by setPref in FeatureManifest). So, as it will not override the user branch
  // pref, should use default branch if the test needs Nimbus and needs to change
  // mdn.featureGate in local.
  UrlbarPrefs.clear("mdn.featureGate");
  const defaultPrefs = Services.prefs.getDefaultBranch("browser.urlbar.");

  const suggestion = REMOTE_SETTINGS_DATA[0].attachment[0];
  const keyword = suggestion.keywords[0];

  // Disable the fature gate.
  defaultPrefs.setBoolPref("mdn.featureGate", false);
  await check_results({
    context: createContext(keyword, {
      providers: [UrlbarProviderQuickSuggest.name],
      isPrivate: false,
    }),
    matches: [],
  });

  // Enable by Nimbus.
  const cleanUpNimbusEnable = await UrlbarTestUtils.initNimbusFeature(
    { mdnFeatureGate: true },
    "urlbar",
    "config"
  );
  await waitForSuggestions();
  await check_results({
    context: createContext(keyword, {
      providers: [UrlbarProviderQuickSuggest.name],
      isPrivate: false,
    }),
    matches: [makeExpectedResult({ searchString: keyword, suggestion })],
  });
  await cleanUpNimbusEnable();

  // Enable locally.
  defaultPrefs.setBoolPref("mdn.featureGate", true);
  await waitForSuggestions();

  // Disable by Nimbus.
  const cleanUpNimbusDisable = await UrlbarTestUtils.initNimbusFeature(
    { mdnFeatureGate: false },
    "urlbar",
    "config"
  );
  await check_results({
    context: createContext(keyword, {
      providers: [UrlbarProviderQuickSuggest.name],
      isPrivate: false,
    }),
    matches: [],
  });
  await cleanUpNimbusDisable();

  // Revert.
  defaultPrefs.setBoolPref("mdn.featureGate", true);
  await waitForSuggestions();
});

function makeExpectedResult({
  searchString,
  suggestion,
  source = "remote-settings",
} = {}) {
  const isTopPick = !!suggestion.is_top_pick;
  return {
    isBestMatch: isTopPick,
    suggestedIndex: isTopPick ? 1 : -1,
    type: UrlbarUtils.RESULT_TYPE.URL,
    source: UrlbarUtils.RESULT_SOURCE.OTHER_NETWORK,
    heuristic: false,
    payload: {
      source,
      provider: source == "remote-settings" ? "MDNSuggestions" : "mdn",
      telemetryType: "mdn",
      title: suggestion.title,
      url: suggestion.url,
      displayUrl: suggestion.url.replace(/^https:\/\//, ""),
      description: isTopPick ? suggestion.description : "",
      icon: "chrome://global/skin/icons/mdn.svg",
      shouldShowUrl: true,
    },
  };
}

async function waitForSuggestions() {
  let keyword = REMOTE_SETTINGS_DATA[0].attachment[0].keywords[0];
  let feature = QuickSuggest.getFeature("MDNSuggestions");
  await TestUtils.waitForCondition(async () => {
    let suggestions = await feature.queryRemoteSettings(keyword);
    return !!suggestions.length;
  }, "Waiting for MDNSuggestions to serve remote settings suggestions");
}
