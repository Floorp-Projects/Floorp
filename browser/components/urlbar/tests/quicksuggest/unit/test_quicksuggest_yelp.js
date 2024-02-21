/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

// Tests Yelp suggestions.

"use strict";

const REMOTE_SETTINGS_RECORDS = [
  {
    type: "yelp-suggestions",
    attachment: {
      subjects: ["ramen", "ab", "alongerkeyword"],
      preModifiers: ["best"],
      postModifiers: ["delivery"],
      locationSigns: [{ keyword: "in", needLocation: true }],
      yelpModifiers: [],
      icon: "1234",
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
      ["yelp.minKeywordLength", 5],
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
        originalUrl: "https://www.yelp.com/search?find_desc=ramen",
        displayUrl:
          "yelp.com/search?find_desc=ramen&find_loc=Yokohama,+Kanagawa",
        title: "ramen in Yokohama, Kanagawa",
      },
    },
    {
      description: "Query too short, no subject exact match: ra",
      query: "ra",
      expected: null,
    },
    {
      description: "Query too short, no subject not exact match: ram",
      query: "ram",
      expected: null,
    },
    {
      description: "Query too short, no subject exact match: rame",
      query: "rame",
      expected: null,
    },
    {
      description:
        "Query length == minKeywordLength, subject exact match: ramen",
      query: "ramen",
      expected: {
        url: "https://www.yelp.com/search?find_desc=ramen&find_loc=Yokohama%2C+Kanagawa",
        originalUrl: "https://www.yelp.com/search?find_desc=ramen",
        displayUrl:
          "yelp.com/search?find_desc=ramen&find_loc=Yokohama,+Kanagawa",
        title: "ramen in Yokohama, Kanagawa",
      },
    },
    {
      description: "Pre-modifier only",
      query: "best",
      expected: null,
    },
    {
      description: "Pre-modifier only with trailing space",
      query: "best ",
      expected: null,
    },
    {
      description: "Pre-modifier, subject too short",
      query: "best r",
      expected: null,
    },
    {
      description: "Pre-modifier, query long enough, subject long enough",
      query: "best ra",
      expected: {
        url: "https://www.yelp.com/search?find_desc=best+ramen&find_loc=Yokohama%2C+Kanagawa",
        originalUrl: "https://www.yelp.com/search?find_desc=best+ramen",
        displayUrl:
          "yelp.com/search?find_desc=best+ramen&find_loc=Yokohama,+Kanagawa",
        title: "best ramen in Yokohama, Kanagawa",
      },
    },
    {
      description: "Subject exact match with length < minKeywordLength",
      query: "ab",
      expected: {
        url: "https://www.yelp.com/search?find_desc=ab&find_loc=Yokohama%2C+Kanagawa",
        originalUrl: "https://www.yelp.com/search?find_desc=ab",
        displayUrl: "yelp.com/search?find_desc=ab&find_loc=Yokohama,+Kanagawa",
        title: "ab in Yokohama, Kanagawa",
      },
    },
    {
      description:
        "Subject exact match with length < minKeywordLength, showLessFrequentlyCount non-zero",
      query: "ab",
      showLessFrequentlyCount: 1,
      expected: null,
    },
    {
      description:
        "Subject exact match with length == minKeywordLength, showLessFrequentlyCount non-zero",
      query: "ramen",
      showLessFrequentlyCount: 1,
      expected: null,
    },
    {
      description: "Query too short: alon",
      query: "alon",
      expected: null,
    },
    {
      description: "Query length == minKeywordLength, subject not exact match",
      query: "along",
      expected: {
        url: "https://www.yelp.com/search?find_desc=alongerkeyword&find_loc=Yokohama%2C+Kanagawa",
        originalUrl: "https://www.yelp.com/search?find_desc=alongerkeyword",
        displayUrl:
          "yelp.com/search?find_desc=alongerkeyword&find_loc=Yokohama,+Kanagawa",
        title: "alongerkeyword in Yokohama, Kanagawa",
      },
    },
    {
      description:
        "Query length == minKeywordLength, subject not exact match, showLessFrequentlyCount non-zero",
      query: "along",
      showLessFrequentlyCount: 1,
      expected: null,
    },
    {
      description:
        "Query length == minKeywordLength + showLessFrequentlyCount, subject not exact match",
      query: "alonge",
      showLessFrequentlyCount: 1,
      expected: {
        url: "https://www.yelp.com/search?find_desc=alongerkeyword&find_loc=Yokohama%2C+Kanagawa",
        originalUrl: "https://www.yelp.com/search?find_desc=alongerkeyword",
        displayUrl:
          "yelp.com/search?find_desc=alongerkeyword&find_loc=Yokohama,+Kanagawa",
        title: "alongerkeyword in Yokohama, Kanagawa",
      },
    },
    {
      description:
        "Query length < minKeywordLength + showLessFrequentlyCount, subject not exact match",
      query: "alonge",
      showLessFrequentlyCount: 2,
      expected: null,
    },
    {
      description:
        "Query length == minKeywordLength + showLessFrequentlyCount, subject not exact match",
      query: "alonger",
      showLessFrequentlyCount: 2,
      expected: {
        url: "https://www.yelp.com/search?find_desc=alongerkeyword&find_loc=Yokohama%2C+Kanagawa",
        originalUrl: "https://www.yelp.com/search?find_desc=alongerkeyword",
        displayUrl:
          "yelp.com/search?find_desc=alongerkeyword&find_loc=Yokohama,+Kanagawa",
        title: "alongerkeyword in Yokohama, Kanagawa",
      },
    },
  ];

  for (let {
    description,
    query,
    showLessFrequentlyCount,
    expected,
  } of TEST_DATA) {
    info(
      "Doing basic subtest: " +
        JSON.stringify({
          description,
          query,
          showLessFrequentlyCount,
          expected,
        })
    );

    if (typeof showLessFrequentlyCount == "number") {
      UrlbarPrefs.set("yelp.showLessFrequentlyCount", showLessFrequentlyCount);
    }

    await check_results({
      context: createContext(query, {
        providers: [UrlbarProviderQuickSuggest.name],
        isPrivate: false,
      }),
      matches: expected ? [makeExpectedResult(expected)] : [],
    });

    UrlbarPrefs.clear("yelp.showLessFrequentlyCount");
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
  Assert.ok(
    !QuickSuggest.getFeature("YelpSuggestions").isEnabled,
    "Yelp should be disabled"
  );
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

  // Make sure Yelp is enabled again.
  Assert.ok(
    QuickSuggest.getFeature("YelpSuggestions").isEnabled,
    "Yelp should be re-enabled"
  );
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
    Assert.ok(
      !QuickSuggest.getFeature("YelpSuggestions").isEnabled,
      "Yelp should be disabled"
    );
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

    // Make sure Yelp is enabled again.
    Assert.ok(
      QuickSuggest.getFeature("YelpSuggestions").isEnabled,
      "Yelp should be re-enabled"
    );
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

// Tests the "Not relevant" command: a dismissed suggestion shouldn't be added.
add_task(async function notRelevant() {
  let result = makeExpectedResult({
    url: "https://www.yelp.com/search?find_desc=ramen&find_loc=tokyo",
    title: "ramen in tokyo",
  });

  info("Triggering the 'Not relevant' command");
  QuickSuggest.getFeature("YelpSuggestions").handleCommand(
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
    context: createContext("ramen in tokyo", {
      providers: [UrlbarProviderQuickSuggest.name],
      isPrivate: false,
    }),
    matches: [],
  });

  info("Doing search for a suggestion that wasn't blocked");
  await check_results({
    context: createContext("alongerkeyword in tokyo", {
      providers: [UrlbarProviderQuickSuggest.name],
      isPrivate: false,
    }),
    matches: [
      makeExpectedResult({
        url: "https://www.yelp.com/search?find_desc=alongerkeyword&find_loc=tokyo",
        title: "alongerkeyword in tokyo",
      }),
    ],
  });

  info("Clearing blocked suggestions");
  await QuickSuggest.blockedSuggestions.clear();

  info("Doing search for unblocked suggestion");
  await check_results({
    context: createContext("ramen in tokyo", {
      providers: [UrlbarProviderQuickSuggest.name],
      isPrivate: false,
    }),
    matches: [result],
  });
});

// Tests the "Not interested" command: all Pocket suggestions should be disabled
// and not added anymore.
add_tasks_with_rust(async function notInterested() {
  let result = makeExpectedResult({
    url: "https://www.yelp.com/search?find_desc=ramen&find_loc=tokyo",
    title: "ramen in tokyo",
  });

  info("Triggering the 'Not interested' command");
  QuickSuggest.getFeature("YelpSuggestions").handleCommand(
    {
      controller: { removeResult() {} },
    },
    result,
    "not_interested"
  );

  Assert.ok(
    !UrlbarPrefs.get("suggest.yelp"),
    "Yelp suggestions should be disabled"
  );

  info("Doing search for the suggestion the command was used on");
  await check_results({
    context: createContext("ramen in tokyo", {
      providers: [UrlbarProviderQuickSuggest.name],
      isPrivate: false,
    }),
    matches: [],
  });

  info("Doing search for another Pocket suggestion");
  await check_results({
    context: createContext("alongerkeyword in tokyo", {
      providers: [UrlbarProviderQuickSuggest.name],
      isPrivate: false,
    }),
    matches: [],
  });

  UrlbarPrefs.clear("suggest.yelp");
  await QuickSuggestTestUtils.forceSync();
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
  let originalUrl = expected.originalUrl ?? expected.url;
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
      originalUrl,
      title: expected.title,
      displayUrl,
      icon: null,
    },
  };
}
