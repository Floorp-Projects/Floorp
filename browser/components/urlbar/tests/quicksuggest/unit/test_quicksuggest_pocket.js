/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

// Tests Pocket quick suggest results.

"use strict";

const REMOTE_SETTINGS_DATA = [
  {
    type: "pocket-suggestions",
    attachment: [
      {
        url: "https://example.com/suggestion",
        title: "Pocket Suggestion",
        keywords: ["test"],
      },
      {
        is_top_pick: true,
        url: "https://example.com/top-pick",
        title: "Pocket Top Pick",
        keywords: ["toppick"],
      },
      {
        is_top_pick: false,
        url: "https://example.com/not-a-top-pick",
        title: "Pocket Not a Top Pick",
        keywords: ["notatoppick"],
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
    context: createContext("test", {
      providers: [UrlbarProviderQuickSuggest.name],
      isPrivate: false,
    }),
    matches: [makeExpectedResult()],
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

// When Pocket-specific preferences are disabled, suggestions should not be
// added.
add_task(async function pocketSpecificPrefsDisabled() {
  const prefs = ["suggest.pocket", "pocket.featureGate"];
  for (const pref of prefs) {
    // First make sure the suggestion is added.
    await check_results({
      context: createContext("test", {
        providers: [UrlbarProviderQuickSuggest.name],
        isPrivate: false,
      }),
      matches: [makeExpectedResult()],
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

// Check wheather the Pocket suggestions will be shown by the setup of Nimbus
// variable.
add_task(async function nimbus() {
  // Disable the fature gate.
  UrlbarPrefs.set("pocket.featureGate", false);
  await check_results({
    context: createContext("test", {
      providers: [UrlbarProviderQuickSuggest.name],
      isPrivate: false,
    }),
    matches: [],
  });

  // Enable by Nimbus.
  const cleanUpNimbusEnable = await UrlbarTestUtils.initNimbusFeature({
    pocketFeatureGate: true,
  });
  await check_results({
    context: createContext("test", {
      providers: [UrlbarProviderQuickSuggest.name],
      isPrivate: false,
    }),
    matches: [makeExpectedResult()],
  });
  await cleanUpNimbusEnable();

  // Enable locally.
  UrlbarPrefs.set("pocket.featureGate", true);

  // Disable by Nimbus.
  const cleanUpNimbusDisable = await UrlbarTestUtils.initNimbusFeature({
    pocketFeatureGate: false,
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
  UrlbarPrefs.set("pocket.featureGate", true);
});

// The suggestion should be shown as a best match when it contains
// `is_top_pick: true`.
add_task(async function topPick() {
  await check_results({
    context: createContext("toppick", {
      providers: [UrlbarProviderQuickSuggest.name],
      isPrivate: false,
    }),
    matches: [
      makeExpectedResult({
        isTopPick: true,
        suggestion: REMOTE_SETTINGS_DATA[0].attachment[1],
      }),
    ],
  });
});

// The suggestion should not be shown as a best match when it contains
// `is_top_pick: false`.
add_task(async function notTopPick() {
  await check_results({
    context: createContext("notatoppick", {
      providers: [UrlbarProviderQuickSuggest.name],
      isPrivate: false,
    }),
    matches: [
      makeExpectedResult({
        isTopPick: false,
        suggestion: REMOTE_SETTINGS_DATA[0].attachment[2],
      }),
    ],
  });
});

// A blocked/dismissed suggestion shouldn't be added.
add_task(async function block() {
  let result = makeExpectedResult();
  let queryContext = {
    view: {
      acknowledgeDismissal() {},
    },
  };

  info("Blocking suggestion");
  QuickSuggest.getFeature("PocketSuggestions").handleCommand(
    queryContext,
    result,
    "dismiss"
  );
  await QuickSuggest.blockedSuggestions._test_readyPromise;

  Assert.ok(
    await QuickSuggest.blockedSuggestions.has(result.payload.url),
    "The result's URL should be blocked"
  );

  info("Doing search for blocked suggestion");
  await check_results({
    context: createContext("test", {
      providers: [UrlbarProviderQuickSuggest.name],
      isPrivate: false,
    }),
    matches: [],
  });

  info("Doing search for a suggestion that wasn't blocked");
  await check_results({
    context: createContext("toppick", {
      providers: [UrlbarProviderQuickSuggest.name],
      isPrivate: false,
    }),
    matches: [
      makeExpectedResult({
        isTopPick: true,
        suggestion: REMOTE_SETTINGS_DATA[0].attachment[1],
      }),
    ],
  });

  info("Clearing blocked suggestions");
  await QuickSuggest.blockedSuggestions.clear();

  info("Doing search for unblocked suggestion");
  await check_results({
    context: createContext("test", {
      providers: [UrlbarProviderQuickSuggest.name],
      isPrivate: false,
    }),
    matches: [result],
  });
});

function makeExpectedResult({
  suggestion = REMOTE_SETTINGS_DATA[0].attachment[0],
  source = "remote-settings",
  isTopPick = true,
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
      icon: "chrome://global/skin/icons/pocket.svg",
      helpUrl: QuickSuggest.HELP_URL,
    },
  };
}
