/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

// Tests MDN quick suggest results.

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
        keywords: ["array filter"],
        score: 0.24,
      },
      {
        url: "https://example.com/input",
        title: "<input>: The Input (Form Input) element",
        description:
          "The <input> HTML element is used to create interactive controls for web-based forms in order to accept data from the user; a wide variety of types of input data and control widgets are available, depending on the device and user agent. The <input> element is one of the most powerful and complex in all of HTML due to the sheer number of combinations of input types and attributes.",
        keywords: ["input"],
        score: 0.24,
      },
      {
        url: "https://example.com/grid",
        title: "CSS Grid Layout",
        description:
          "CSS Grid Layout excels at dividing a page into major regions or defining the relationship in terms of size, position, and layer, between parts of a control built from HTML primitives.",
        keywords: ["grid"],
        score: 0.24,
      },
    ],
  },
];

add_setup(async function init() {
  // Disable search suggestions so we don't hit the network.
  Services.prefs.setBoolPref("browser.search.suggest.enabled", false);

  await QuickSuggestTestUtils.ensureQuickSuggestInit({
    remoteSettingsRecords: REMOTE_SETTINGS_DATA,
    prefs: [
      ["suggest.quicksuggest.nonsponsored", true],
      ["suggest.quicksuggest.sponsored", false],
    ],
  });
});

add_tasks_with_rust(async function basic() {
  for (const suggestion of REMOTE_SETTINGS_DATA[0].attachment) {
    const fullKeyword = suggestion.keywords[0];
    const firstWord = fullKeyword.split(" ")[0];
    for (let i = 1; i < fullKeyword.length; i++) {
      const keyword = fullKeyword.substring(0, i);
      const shouldMatch = i >= firstWord.length;
      const matches = shouldMatch ? [makeMdnResult(suggestion)] : [];
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
      matches:
        UrlbarPrefs.get("quickSuggestRustEnabled") && !fullKeyword.includes(" ")
          ? [makeMdnResult(suggestion)]
          : [],
    });
  }
});

// Check wheather the MDN suggestions will be hidden by the pref.
add_tasks_with_rust(async function disableByLocalPref() {
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
      matches: [makeMdnResult(suggestion)],
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
    await QuickSuggestTestUtils.forceSync();
  }
});

// Check wheather the MDN suggestions will be shown by the setup of Nimbus
// variable.
add_tasks_with_rust(async function nimbus() {
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
  await QuickSuggestTestUtils.forceSync();
  await check_results({
    context: createContext(keyword, {
      providers: [UrlbarProviderQuickSuggest.name],
      isPrivate: false,
    }),
    matches: [makeMdnResult(suggestion)],
  });
  await cleanUpNimbusEnable();

  // Enable locally.
  defaultPrefs.setBoolPref("mdn.featureGate", true);
  await QuickSuggestTestUtils.forceSync();

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
  await QuickSuggestTestUtils.forceSync();
});

add_tasks_with_rust(async function mixedCaseQuery() {
  const suggestion = REMOTE_SETTINGS_DATA[0].attachment[1];
  const keyword = "InPuT";

  await check_results({
    context: createContext(keyword, {
      providers: [UrlbarProviderQuickSuggest.name],
      isPrivate: false,
    }),
    matches: [makeMdnResult(suggestion)],
  });
});
