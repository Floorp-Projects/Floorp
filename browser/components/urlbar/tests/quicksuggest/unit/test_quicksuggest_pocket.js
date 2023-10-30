/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

// Tests Pocket quick suggest results.

"use strict";

const LOW_KEYWORD = "low one two";
const HIGH_KEYWORD = "high three";

const REMOTE_SETTINGS_DATA = [
  {
    type: "pocket-suggestions",
    attachment: [
      {
        url: "https://example.com/pocket-0",
        title: "Pocket Suggestion 0",
        description: "Pocket description 0",
        lowConfidenceKeywords: [LOW_KEYWORD, "how to low"],
        highConfidenceKeywords: [HIGH_KEYWORD],
        score: 0.25,
      },
      {
        url: "https://example.com/pocket-1",
        title: "Pocket Suggestion 1",
        description: "Pocket description 1",
        lowConfidenceKeywords: ["other low"],
        highConfidenceKeywords: ["another high"],
        score: 0.25,
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
      ["pocket.featureGate", true],
    ],
  });
});

add_task(async function telemetryType() {
  Assert.equal(
    QuickSuggest.getFeature("PocketSuggestions").getSuggestionTelemetryType({}),
    "pocket",
    "Telemetry type should be 'pocket'"
  );
});

// When non-sponsored suggestions are disabled, Pocket suggestions should be
// disabled.
add_tasks_with_rust(async function nonsponsoredDisabled() {
  // Disable sponsored suggestions. Pocket suggestions are non-sponsored, so
  // doing this should not prevent them from being enabled.
  UrlbarPrefs.set("suggest.quicksuggest.sponsored", false);

  // First make sure the suggestion is added when non-sponsored suggestions are
  // enabled.
  await check_results({
    context: createContext(LOW_KEYWORD, {
      providers: [UrlbarProviderQuickSuggest.name],
      isPrivate: false,
    }),
    matches: [makeExpectedResult({ searchString: LOW_KEYWORD })],
  });

  // Now disable them.
  UrlbarPrefs.set("suggest.quicksuggest.nonsponsored", false);
  await check_results({
    context: createContext(LOW_KEYWORD, {
      providers: [UrlbarProviderQuickSuggest.name],
      isPrivate: false,
    }),
    matches: [],
  });

  UrlbarPrefs.set("suggest.quicksuggest.nonsponsored", true);
  UrlbarPrefs.clear("suggest.quicksuggest.sponsored");
  await QuickSuggestTestUtils.forceSync();
});

// When Pocket-specific preferences are disabled, suggestions should not be
// added.
add_tasks_with_rust(async function pocketSpecificPrefsDisabled() {
  const prefs = ["suggest.pocket", "pocket.featureGate"];
  for (const pref of prefs) {
    // First make sure the suggestion is added.
    await check_results({
      context: createContext(LOW_KEYWORD, {
        providers: [UrlbarProviderQuickSuggest.name],
        isPrivate: false,
      }),
      matches: [makeExpectedResult({ searchString: LOW_KEYWORD })],
    });

    // Now disable the pref.
    UrlbarPrefs.set(pref, false);
    await check_results({
      context: createContext(LOW_KEYWORD, {
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

// Check wheather the Pocket suggestions will be shown by the setup of Nimbus
// variable.
add_tasks_with_rust(async function nimbus() {
  // Disable the fature gate.
  UrlbarPrefs.set("pocket.featureGate", false);
  await check_results({
    context: createContext(LOW_KEYWORD, {
      providers: [UrlbarProviderQuickSuggest.name],
      isPrivate: false,
    }),
    matches: [],
  });

  // Enable by Nimbus.
  const cleanUpNimbusEnable = await UrlbarTestUtils.initNimbusFeature({
    pocketFeatureGate: true,
  });
  await QuickSuggestTestUtils.forceSync();
  await check_results({
    context: createContext(LOW_KEYWORD, {
      providers: [UrlbarProviderQuickSuggest.name],
      isPrivate: false,
    }),
    matches: [makeExpectedResult({ searchString: LOW_KEYWORD })],
  });
  await cleanUpNimbusEnable();

  // Enable locally.
  UrlbarPrefs.set("pocket.featureGate", true);
  await QuickSuggestTestUtils.forceSync();

  // Disable by Nimbus.
  const cleanUpNimbusDisable = await UrlbarTestUtils.initNimbusFeature({
    pocketFeatureGate: false,
  });
  await check_results({
    context: createContext(LOW_KEYWORD, {
      providers: [UrlbarProviderQuickSuggest.name],
      isPrivate: false,
    }),
    matches: [],
  });
  await cleanUpNimbusDisable();

  // Revert.
  UrlbarPrefs.set("pocket.featureGate", true);
  await QuickSuggestTestUtils.forceSync();
});

// The suggestion should be shown as a top pick when a high-confidence keyword
// is matched.
add_tasks_with_rust(async function topPick() {
  await check_results({
    context: createContext(HIGH_KEYWORD, {
      providers: [UrlbarProviderQuickSuggest.name],
      isPrivate: false,
    }),
    matches: [
      makeExpectedResult({ searchString: HIGH_KEYWORD, isTopPick: true }),
    ],
  });
});

// Low-confidence keywords should do prefix matching starting at the first word.
add_tasks_with_rust(async function lowPrefixes() {
  // search string -> should match
  let tests = {
    l: false,
    lo: false,
    low: true,
    "low ": true,
    "low o": true,
    "low on": true,
    "low one": true,
    "low one ": true,
    "low one t": true,
    "low one tw": true,
    "low one two": true,
    "low one two ": false,
  };
  for (let [searchString, shouldMatch] of Object.entries(tests)) {
    info("Doing search: " + JSON.stringify({ searchString, shouldMatch }));
    await check_results({
      context: createContext(searchString, {
        providers: [UrlbarProviderQuickSuggest.name],
        isPrivate: false,
      }),
      matches: shouldMatch
        ? [makeExpectedResult({ searchString, fullKeyword: LOW_KEYWORD })]
        : [],
    });
  }
});

// Low-confidence keywords that start with "how to" should do prefix matching
// starting at "how to" instead of the first word.
//
// Note: The Rust implementation doesn't support this.
add_task(async function lowPrefixes_howTo() {
  Assert.ok(
    !UrlbarPrefs.get("quicksuggest.rustEnabled"),
    "The Rust implementation doesn't support the 'how to' special case"
  );

  // search string -> should match
  let tests = {
    h: false,
    ho: false,
    how: false,
    "how ": false,
    "how t": false,
    "how to": true,
    "how to ": true,
    "how to l": true,
    "how to lo": true,
    "how to low": true,
  };
  for (let [searchString, shouldMatch] of Object.entries(tests)) {
    info("Doing search: " + JSON.stringify({ searchString, shouldMatch }));
    await check_results({
      context: createContext(searchString, {
        providers: [UrlbarProviderQuickSuggest.name],
        isPrivate: false,
      }),
      matches: shouldMatch
        ? [makeExpectedResult({ searchString, fullKeyword: "how to low" })]
        : [],
    });
  }
});

// High-confidence keywords should not do prefix matching at all.
add_tasks_with_rust(async function highPrefixes() {
  // search string -> should match
  let tests = {
    h: false,
    hi: false,
    hig: false,
    high: false,
    "high ": false,
    "high t": false,
    "high th": false,
    "high thr": false,
    "high thre": false,
    "high three": true,
    "high three ": false,
  };
  for (let [searchString, shouldMatch] of Object.entries(tests)) {
    info("Doing search: " + JSON.stringify({ searchString, shouldMatch }));
    await check_results({
      context: createContext(searchString, {
        providers: [UrlbarProviderQuickSuggest.name],
        isPrivate: false,
      }),
      matches: shouldMatch
        ? [
            makeExpectedResult({
              searchString,
              fullKeyword: HIGH_KEYWORD,
              isTopPick: true,
            }),
          ]
        : [],
    });
  }
});

// Keyword matching should be case insenstive.
add_tasks_with_rust(async function uppercase() {
  await check_results({
    context: createContext(LOW_KEYWORD.toUpperCase(), {
      providers: [UrlbarProviderQuickSuggest.name],
      isPrivate: false,
    }),
    matches: [
      makeExpectedResult({
        searchString: LOW_KEYWORD.toUpperCase(),
        fullKeyword: LOW_KEYWORD,
      }),
    ],
  });
  await check_results({
    context: createContext(HIGH_KEYWORD.toUpperCase(), {
      providers: [UrlbarProviderQuickSuggest.name],
      isPrivate: false,
    }),
    matches: [
      makeExpectedResult({
        searchString: HIGH_KEYWORD.toUpperCase(),
        fullKeyword: HIGH_KEYWORD,
        isTopPick: true,
      }),
    ],
  });
});

// Tests the "Not relevant" command: a dismissed suggestion shouldn't be added.
add_tasks_with_rust(async function notRelevant() {
  let result = makeExpectedResult({ searchString: LOW_KEYWORD });

  info("Triggering the 'Not relevant' command");
  QuickSuggest.getFeature("PocketSuggestions").handleCommand(
    {
      controller: { removeResult() {} },
    },
    result,
    "not_relevant"
  );
  await QuickSuggest.blockedSuggestions._test_readyPromise;

  Assert.ok(
    await QuickSuggest.blockedSuggestions.has(result.payload.originalUrl),
    "The result's URL should be blocked"
  );

  info("Doing search for blocked suggestion");
  await check_results({
    context: createContext(LOW_KEYWORD, {
      providers: [UrlbarProviderQuickSuggest.name],
      isPrivate: false,
    }),
    matches: [],
  });

  info("Doing search for blocked suggestion using high-confidence keyword");
  await check_results({
    context: createContext(HIGH_KEYWORD, {
      providers: [UrlbarProviderQuickSuggest.name],
      isPrivate: false,
    }),
    matches: [],
  });

  info("Doing search for a suggestion that wasn't blocked");
  await check_results({
    context: createContext("other low", {
      providers: [UrlbarProviderQuickSuggest.name],
      isPrivate: false,
    }),
    matches: [
      makeExpectedResult({
        searchString: "other low",
        suggestion: REMOTE_SETTINGS_DATA[0].attachment[1],
      }),
    ],
  });

  info("Clearing blocked suggestions");
  await QuickSuggest.blockedSuggestions.clear();

  info("Doing search for unblocked suggestion");
  await check_results({
    context: createContext(LOW_KEYWORD, {
      providers: [UrlbarProviderQuickSuggest.name],
      isPrivate: false,
    }),
    matches: [result],
  });
});

// Tests the "Not interested" command: all Pocket suggestions should be disabled
// and not added anymore.
add_tasks_with_rust(async function notInterested() {
  let result = makeExpectedResult({ searchString: LOW_KEYWORD });

  info("Triggering the 'Not interested' command");
  QuickSuggest.getFeature("PocketSuggestions").handleCommand(
    {
      controller: { removeResult() {} },
    },
    result,
    "not_interested"
  );

  Assert.ok(
    !UrlbarPrefs.get("suggest.pocket"),
    "Pocket suggestions should be disabled"
  );

  info("Doing search for the suggestion the command was used on");
  await check_results({
    context: createContext(LOW_KEYWORD, {
      providers: [UrlbarProviderQuickSuggest.name],
      isPrivate: false,
    }),
    matches: [],
  });

  info("Doing search for another Pocket suggestion");
  await check_results({
    context: createContext("other low", {
      providers: [UrlbarProviderQuickSuggest.name],
      isPrivate: false,
    }),
    matches: [],
  });

  UrlbarPrefs.clear("suggest.pocket");
  await QuickSuggestTestUtils.forceSync();
});

// Tests the "show less frequently" behavior.
add_tasks_with_rust(async function showLessFrequently() {
  await doShowLessFrequentlyTests({
    feature: QuickSuggest.getFeature("PocketSuggestions"),
    showLessFrequentlyCountPref: "pocket.showLessFrequentlyCount",
    nimbusCapVariable: "pocketShowLessFrequentlyCap",
    expectedResult: searchString =>
      makeExpectedResult({ searchString, fullKeyword: LOW_KEYWORD }),
    keyword: LOW_KEYWORD,
  });
});

function makeExpectedResult({
  searchString,
  fullKeyword = searchString,
  suggestion = REMOTE_SETTINGS_DATA[0].attachment[0],
  source = "remote-settings",
  isTopPick = false,
} = {}) {
  if (
    source == "remote-settings" &&
    UrlbarPrefs.get("quicksuggest.rustEnabled")
  ) {
    source = "rust";
  }

  let provider;
  let keywordSubstringNotTyped = fullKeyword.substring(searchString.length);
  let description = suggestion.description;
  switch (source) {
    case "remote-settings":
      provider = "PocketSuggestions";
      break;
    case "rust":
      provider = "Pocket";
      // Rust suggestions currently do not include full keyword or description.
      keywordSubstringNotTyped = "";
      description = suggestion.title;
      break;
    case "merino":
      provider = "pocket";
      break;
  }

  let url = new URL(suggestion.url);
  url.searchParams.set("utm_medium", "firefox-desktop");
  url.searchParams.set("utm_source", "firefox-suggest");
  url.searchParams.set("utm_campaign", "pocket-collections-in-the-address-bar");
  url.searchParams.set("utm_content", "treatment");

  return {
    isBestMatch: isTopPick,
    suggestedIndex: isTopPick ? 1 : -1,
    type: UrlbarUtils.RESULT_TYPE.URL,
    source: UrlbarUtils.RESULT_SOURCE.OTHER_NETWORK,
    heuristic: false,
    payload: {
      source,
      provider,
      telemetryType: "pocket",
      title: suggestion.title,
      url: url.href,
      displayUrl: url.href.replace(/^https:\/\//, ""),
      originalUrl: suggestion.url,
      description: isTopPick ? description : "",
      icon: isTopPick
        ? "chrome://global/skin/icons/pocket.svg"
        : "chrome://global/skin/icons/pocket-favicon.ico",
      helpUrl: QuickSuggest.HELP_URL,
      shouldShowUrl: true,
      bottomTextL10n: {
        id: "firefox-suggest-pocket-bottom-text",
        args: {
          keywordSubstringTyped: searchString,
          keywordSubstringNotTyped,
        },
      },
    },
  };
}
