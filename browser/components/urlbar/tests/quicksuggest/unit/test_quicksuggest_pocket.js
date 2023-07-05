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
        lowConfidenceKeywords: [LOW_KEYWORD],
        highConfidenceKeywords: [HIGH_KEYWORD],
      },
      {
        url: "https://example.com/pocket-1",
        title: "Pocket Suggestion 1",
        description: "Pocket description 1",
        lowConfidenceKeywords: ["other low", "both low and high"],
        highConfidenceKeywords: ["both low and high"],
      },
    ],
  },
];

add_setup(async function init() {
  UrlbarPrefs.set("quicksuggest.enabled", true);
  UrlbarPrefs.set("bestMatch.enabled", true);
  UrlbarPrefs.set("suggest.quicksuggest.nonsponsored", true);
  UrlbarPrefs.set("pocket.featureGate", true);

  // Disable search suggestions so we don't hit the network.
  Services.prefs.setBoolPref("browser.search.suggest.enabled", false);

  await QuickSuggestTestUtils.ensureQuickSuggestInit({
    remoteSettingsResults: REMOTE_SETTINGS_DATA,
  });
  await waitForSuggestions();
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
add_task(async function nonsponsoredDisabled() {
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
  await waitForSuggestions();
});

// When Pocket-specific preferences are disabled, suggestions should not be
// added.
add_task(async function pocketSpecificPrefsDisabled() {
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
    await waitForSuggestions();
  }
});

// Check wheather the Pocket suggestions will be shown by the setup of Nimbus
// variable.
add_task(async function nimbus() {
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
  await waitForSuggestions();
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
  await waitForSuggestions();

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
  await waitForSuggestions();
});

// The suggestion should be shown as a top pick when a high-confidence keyword
// is matched.
add_task(async function topPick() {
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

// The suggestion should not be shown as a top pick when a best match pref is
// disabled even when a high-confidence keyword is matched.
add_task(async function topPickPrefsDisabled() {
  let prefs = ["bestMatch.enabled", "suggest.bestmatch"];
  for (let pref of prefs) {
    info("Disabling pref: " + pref);
    UrlbarPrefs.set(pref, false);
    await check_results({
      context: createContext(HIGH_KEYWORD, {
        providers: [UrlbarProviderQuickSuggest.name],
        isPrivate: false,
      }),
      matches: [
        makeExpectedResult({ searchString: HIGH_KEYWORD, isTopPick: false }),
      ],
    });
    UrlbarPrefs.set(pref, true);
  }
});

// Low-confidence keywords should do prefix matching starting at the first word.
add_task(async function lowPrefixes() {
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

// High-confidence keywords should not do prefix matching at all.
add_task(async function highPrefixes() {
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

// When a search matches both a low and high-confidence keyword, the suggestion
// should be shown as a top pick.
add_task(async function topPickLowAndHigh() {
  let suggestion = REMOTE_SETTINGS_DATA[0].attachment[1];
  let finalSearchString = "both low and high";
  Assert.ok(
    suggestion.lowConfidenceKeywords.includes(finalSearchString),
    "Sanity check: lowConfidenceKeywords includes the final search string"
  );
  Assert.ok(
    suggestion.highConfidenceKeywords.includes(finalSearchString),
    "Sanity check: highConfidenceKeywords includes the final search string"
  );

  // search string -> should be a top pick
  let tests = {
    "both low": false,
    "both low ": false,
    "both low a": false,
    "both low an": false,
    "both low and": false,
    "both low and ": false,
    "both low and h": false,
    "both low and hi": false,
    "both low and hig": false,
    "both low and high": true,
    [finalSearchString]: true,
  };
  for (let [searchString, isTopPick] of Object.entries(tests)) {
    info("Doing search: " + JSON.stringify({ searchString, isTopPick }));
    await check_results({
      context: createContext(searchString, {
        providers: [UrlbarProviderQuickSuggest.name],
        isPrivate: false,
      }),
      matches: [
        makeExpectedResult({
          searchString,
          fullKeyword: "both low and high",
          isTopPick,
          suggestion,
        }),
      ],
    });
  }
});

// Keyword matching should be case insenstive.
add_task(async function uppercase() {
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
add_task(async function notRelevant() {
  let result = makeExpectedResult({ searchString: LOW_KEYWORD });
  let queryContext = {
    view: {
      acknowledgeDismissal() {},
    },
  };

  info("Triggering the 'Not relevant' command");
  QuickSuggest.getFeature("PocketSuggestions").handleCommand(
    queryContext,
    result,
    "not_relevant"
  );
  await QuickSuggest.blockedSuggestions._test_readyPromise;

  Assert.ok(
    await QuickSuggest.blockedSuggestions.has(result.payload.url),
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
add_task(async function notInterested() {
  let result = makeExpectedResult({ searchString: LOW_KEYWORD });
  let queryContext = {
    view: {
      acknowledgeDismissal() {},
    },
  };

  info("Triggering the 'Not interested' command");
  QuickSuggest.getFeature("PocketSuggestions").handleCommand(
    queryContext,
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
  await waitForSuggestions();
});

// Tests the "show less frequently" behavior.
add_task(async function showLessFrequently() {
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
  return {
    isBestMatch: isTopPick,
    suggestedIndex: isTopPick ? 1 : -1,
    type: UrlbarUtils.RESULT_TYPE.URL,
    source: UrlbarUtils.RESULT_SOURCE.OTHER_NETWORK,
    heuristic: false,
    payload: {
      source,
      provider: source == "remote-settings" ? "PocketSuggestions" : "pocket",
      telemetryType: "pocket",
      title: suggestion.title,
      url: suggestion.url,
      displayUrl: suggestion.url.replace(/^https:\/\//, ""),
      description: isTopPick ? suggestion.description : "",
      icon: "chrome://global/skin/icons/pocket.svg",
      helpUrl: QuickSuggest.HELP_URL,
      shouldShowUrl: true,
      bottomTextL10n: {
        id: "firefox-suggest-pocket-bottom-text",
        args: {
          keywordSubstringTyped: searchString,
          keywordSubstringNotTyped: fullKeyword.substring(searchString.length),
        },
      },
    },
  };
}

async function waitForSuggestions(keyword = LOW_KEYWORD) {
  let feature = QuickSuggest.getFeature("PocketSuggestions");
  await TestUtils.waitForCondition(async () => {
    let suggestions = await feature.queryRemoteSettings(keyword);
    return !!suggestions.length;
  }, "Waiting for PocketSuggestions to serve remote settings suggestions");
}
