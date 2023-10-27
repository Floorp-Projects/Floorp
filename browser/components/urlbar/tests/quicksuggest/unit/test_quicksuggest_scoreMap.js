/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

// Tests the `quickSuggestScoreMap` Nimbus variable that assigns scores to
// specified types of quick suggest suggestions. The scores in the map should
// override the scores in the individual suggestion objects so that experiments
// can fully control the relative ranking of suggestions.

"use strict";

const { DEFAULT_SUGGESTION_SCORE } = UrlbarProviderQuickSuggest;

const REMOTE_SETTINGS_RECORDS = [
  {
    type: "data",
    attachment: [
      // sponsored without score
      {
        iab_category: "22 - Shopping",
        keywords: [
          "sponsored without score",
          "sponsored without score, nonsponsored without score",
          "sponsored without score, nonsponsored with score",
          "sponsored without score, addon without score",
        ],
        id: 1,
        url: "https://example.com/sponsored-without-score",
        title: "Sponsored without score",
        click_url: "https://example.com/click",
        impression_url: "https://example.com/impression",
        advertiser: "TestAdvertiser",
        icon: null,
      },
      // sponsored with score
      {
        iab_category: "22 - Shopping",
        score: 2 * DEFAULT_SUGGESTION_SCORE,
        keywords: [
          "sponsored with score",
          "sponsored with score, nonsponsored without score",
          "sponsored with score, nonsponsored with score",
          "sponsored with score, addon with score",
        ],
        id: 2,
        url: "https://example.com/sponsored-with-score",
        title: "Sponsored with score",
        click_url: "https://example.com/click",
        impression_url: "https://example.com/impression",
        advertiser: "TestAdvertiser",
        icon: null,
      },
      // nonsponsored without score
      {
        iab_category: "5 - Education",
        keywords: [
          "nonsponsored without score",
          "sponsored without score, nonsponsored without score",
          "sponsored with score, nonsponsored without score",
        ],
        id: 3,
        url: "https://example.com/nonsponsored-without-score",
        title: "Nonsponsored without score",
        click_url: "https://example.com/click",
        impression_url: "https://example.com/impression",
        advertiser: "TestAdvertiser",
        icon: null,
      },
      // nonsponsored with score
      {
        iab_category: "5 - Education",
        score: 2 * DEFAULT_SUGGESTION_SCORE,
        keywords: [
          "nonsponsored with score",
          "sponsored without score, nonsponsored with score",
          "sponsored with score, nonsponsored with score",
        ],
        id: 4,
        url: "https://example.com/nonsponsored-with-score",
        title: "Nonsponsored with score",
        click_url: "https://example.com/click",
        impression_url: "https://example.com/impression",
        advertiser: "TestAdvertiser",
        icon: null,
      },
    ],
  },
  {
    type: "amo-suggestions",
    attachment: [
      // addon without score
      {
        keywords: [
          "addon without score",
          "sponsored without score, addon without score",
        ],
        url: "https://example.com/addon-without-score",
        guid: "addon-without-score@example.com",
        icon: "https://example.com/addon.svg",
        title: "Addon without score",
        rating: "4.7",
        description: "Addon without score",
        number_of_ratings: 1256,
        is_top_pick: true,
      },
      // addon with score
      {
        score: 2 * DEFAULT_SUGGESTION_SCORE,
        keywords: [
          "addon with score",
          "sponsored with score, addon with score",
        ],
        url: "https://example.com/addon-with-score",
        guid: "addon-with-score@example.com",
        icon: "https://example.com/addon.svg",
        title: "Addon with score",
        rating: "4.7",
        description: "Addon with score",
        number_of_ratings: 1256,
        is_top_pick: true,
      },
    ],
  },
];

const ADM_RECORD = REMOTE_SETTINGS_RECORDS[0];
const SPONSORED_WITHOUT_SCORE = ADM_RECORD.attachment[0];
const SPONSORED_WITH_SCORE = ADM_RECORD.attachment[1];
const NONSPONSORED_WITHOUT_SCORE = ADM_RECORD.attachment[2];
const NONSPONSORED_WITH_SCORE = ADM_RECORD.attachment[3];

const ADDON_RECORD = REMOTE_SETTINGS_RECORDS[1];
const ADDON_WITHOUT_SCORE = ADDON_RECORD.attachment[0];
const ADDON_WITH_SCORE = ADDON_RECORD.attachment[1];

const MERINO_SPONSORED_SUGGESTION = {
  provider: "adm",
  score: DEFAULT_SUGGESTION_SCORE,
  iab_category: "22 - Shopping",
  is_sponsored: true,
  keywords: ["test"],
  full_keyword: "test",
  block_id: 1,
  url: "https://example.com/merino-sponsored",
  title: "Merino sponsored",
  click_url: "https://example.com/click",
  impression_url: "https://example.com/impression",
  advertiser: "TestAdvertiser",
  icon: null,
};

const MERINO_ADDON_SUGGESTION = {
  provider: "amo",
  score: DEFAULT_SUGGESTION_SCORE,
  keywords: ["test"],
  icon: "https://example.com/addon.svg",
  url: "https://example.com/merino-addon",
  title: "Merino addon",
  description: "Merino addon",
  is_top_pick: true,
  custom_details: {
    amo: {
      guid: "merino-addon@example.com",
      rating: "4.7",
      number_of_ratings: "1256",
    },
  },
};

const MERINO_UNKNOWN_SUGGESTION = {
  provider: "some_unknown_provider",
  score: DEFAULT_SUGGESTION_SCORE,
  keywords: ["test"],
  url: "https://example.com/merino-unknown",
  title: "Merino unknown",
};

add_setup(async function init() {
  // Disable search suggestions so we don't hit the network.
  Services.prefs.setBoolPref("browser.search.suggest.enabled", false);

  await QuickSuggestTestUtils.ensureQuickSuggestInit({
    remoteSettingsRecords: REMOTE_SETTINGS_RECORDS,
    merinoSuggestions: [],
    prefs: [
      ["suggest.quicksuggest.sponsored", true],
      ["suggest.quicksuggest.nonsponsored", true],
    ],
  });
});

add_task(async function sponsoredWithout_nonsponsoredWithout_sponsoredWins() {
  let keyword = "sponsored without score, nonsponsored without score";
  let score = 10 * DEFAULT_SUGGESTION_SCORE;
  await doTest({
    keyword,
    scoreMap: {
      adm_sponsored: score,
    },
    expectedFeatureName: "AdmWikipedia",
    expectedScore: score,
    expectedResult: makeExpectedAdmResult({
      keyword,
      suggestion: SPONSORED_WITHOUT_SCORE,
    }),
  });
});

add_task(
  async function sponsoredWithout_nonsponsoredWithout_nonsponsoredWins() {
    let keyword = "sponsored without score, nonsponsored without score";
    let score = 10 * DEFAULT_SUGGESTION_SCORE;
    await doTest({
      keyword,
      scoreMap: {
        adm_nonsponsored: score,
      },
      expectedFeatureName: "AdmWikipedia",
      expectedScore: score,
      expectedResult: makeExpectedAdmResult({
        keyword,
        suggestion: NONSPONSORED_WITHOUT_SCORE,
      }),
    });
  }
);
add_task(
  async function sponsoredWithout_nonsponsoredWithout_sponsoredWins_both() {
    let keyword = "sponsored without score, nonsponsored without score";
    let score = 10 * DEFAULT_SUGGESTION_SCORE;
    await doTest({
      keyword,
      scoreMap: {
        adm_sponsored: score,
        adm_nonsponsored: score / 2,
      },
      expectedFeatureName: "AdmWikipedia",
      expectedScore: score,
      expectedResult: makeExpectedAdmResult({
        keyword,
        suggestion: SPONSORED_WITHOUT_SCORE,
      }),
    });
  }
);

add_task(
  async function sponsoredWithout_nonsponsoredWithout_nonsponsoredWins_both() {
    let keyword = "sponsored without score, nonsponsored without score";
    let score = 10 * DEFAULT_SUGGESTION_SCORE;
    await doTest({
      keyword,
      scoreMap: {
        adm_nonsponsored: score,
        adm_sponsored: score / 2,
      },
      expectedFeatureName: "AdmWikipedia",
      expectedScore: score,
      expectedResult: makeExpectedAdmResult({
        keyword,
        suggestion: NONSPONSORED_WITHOUT_SCORE,
      }),
    });
  }
);

add_task(async function sponsoredWith_nonsponsoredWith_sponsoredWins() {
  let keyword = "sponsored with score, nonsponsored with score";
  let score = 10 * DEFAULT_SUGGESTION_SCORE;
  await doTest({
    keyword,
    scoreMap: {
      adm_sponsored: score,
    },
    expectedFeatureName: "AdmWikipedia",
    expectedScore: score,
    expectedResult: makeExpectedAdmResult({
      keyword,
      suggestion: SPONSORED_WITH_SCORE,
    }),
  });
});

add_task(async function sponsoredWith_nonsponsoredWith_nonsponsoredWins() {
  let keyword = "sponsored with score, nonsponsored with score";
  let score = 10 * DEFAULT_SUGGESTION_SCORE;
  await doTest({
    keyword,
    scoreMap: {
      adm_nonsponsored: score,
    },
    expectedFeatureName: "AdmWikipedia",
    expectedScore: score,
    expectedResult: makeExpectedAdmResult({
      keyword,
      suggestion: NONSPONSORED_WITH_SCORE,
    }),
  });
});

add_task(async function sponsoredWith_nonsponsoredWith_sponsoredWins_both() {
  let keyword = "sponsored with score, nonsponsored with score";
  let score = 10 * DEFAULT_SUGGESTION_SCORE;
  await doTest({
    keyword,
    scoreMap: {
      adm_sponsored: score,
      adm_nonsponsored: score / 2,
    },
    expectedFeatureName: "AdmWikipedia",
    expectedScore: score,
    expectedResult: makeExpectedAdmResult({
      keyword,
      suggestion: SPONSORED_WITH_SCORE,
    }),
  });
});

add_task(async function sponsoredWith_nonsponsoredWith_nonsponsoredWins_both() {
  let keyword = "sponsored with score, nonsponsored with score";
  let score = 10 * DEFAULT_SUGGESTION_SCORE;
  await doTest({
    keyword,
    scoreMap: {
      adm_nonsponsored: score,
      adm_sponsored: score / 2,
    },
    expectedFeatureName: "AdmWikipedia",
    expectedScore: score,
    expectedResult: makeExpectedAdmResult({
      keyword,
      suggestion: NONSPONSORED_WITH_SCORE,
    }),
  });
});

add_task(async function sponsoredWithout_addonWithout_sponsoredWins() {
  let keyword = "sponsored without score, addon without score";
  let score = 10 * DEFAULT_SUGGESTION_SCORE;
  await doTest({
    keyword,
    scoreMap: {
      adm_sponsored: score,
    },
    expectedFeatureName: "AdmWikipedia",
    expectedScore: score,
    expectedResult: makeExpectedAdmResult({
      keyword,
      suggestion: SPONSORED_WITHOUT_SCORE,
    }),
  });
});

add_task(async function sponsoredWithout_addonWithout_addonWins() {
  let keyword = "sponsored without score, addon without score";
  let score = 10 * DEFAULT_SUGGESTION_SCORE;
  await doTest({
    keyword,
    scoreMap: {
      amo: score,
    },
    expectedFeatureName: "AddonSuggestions",
    expectedScore: score,
    expectedResult: makeExpectedAddonResult({
      suggestion: ADDON_WITHOUT_SCORE,
    }),
  });
});

add_task(async function sponsoredWithout_addonWithout_sponsoredWins_both() {
  let keyword = "sponsored without score, addon without score";
  let score = 10 * DEFAULT_SUGGESTION_SCORE;
  await doTest({
    keyword,
    scoreMap: {
      adm_sponsored: score,
      amo: score / 2,
    },
    expectedFeatureName: "AdmWikipedia",
    expectedScore: score,
    expectedResult: makeExpectedAdmResult({
      keyword,
      suggestion: SPONSORED_WITHOUT_SCORE,
    }),
  });
});

add_task(async function sponsoredWithout_addonWithout_addonWins_both() {
  let keyword = "sponsored without score, addon without score";
  let score = 10 * DEFAULT_SUGGESTION_SCORE;
  await doTest({
    keyword,
    scoreMap: {
      amo: score,
      adm_sponsored: score / 2,
    },
    expectedFeatureName: "AddonSuggestions",
    expectedScore: score,
    expectedResult: makeExpectedAddonResult({
      suggestion: ADDON_WITHOUT_SCORE,
    }),
  });
});

add_task(async function sponsoredWith_addonWith_sponsoredWins() {
  let keyword = "sponsored with score, addon with score";
  let score = 10 * DEFAULT_SUGGESTION_SCORE;
  await doTest({
    keyword,
    scoreMap: {
      adm_sponsored: score,
    },
    expectedFeatureName: "AdmWikipedia",
    expectedScore: score,
    expectedResult: makeExpectedAdmResult({
      keyword,
      suggestion: SPONSORED_WITH_SCORE,
    }),
  });
});

add_task(async function sponsoredWith_addonWith_addonWins() {
  let keyword = "sponsored with score, addon with score";
  let score = 10 * DEFAULT_SUGGESTION_SCORE;
  await doTest({
    keyword,
    scoreMap: {
      amo: score,
    },
    expectedFeatureName: "AddonSuggestions",
    expectedScore: score,
    expectedResult: makeExpectedAddonResult({
      suggestion: ADDON_WITH_SCORE,
    }),
  });
});

add_task(async function sponsoredWith_addonWith_sponsoredWins_both() {
  let keyword = "sponsored with score, addon with score";
  let score = 10 * DEFAULT_SUGGESTION_SCORE;
  await doTest({
    keyword,
    scoreMap: {
      adm_sponsored: score,
      amo: score / 2,
    },
    expectedFeatureName: "AdmWikipedia",
    expectedScore: score,
    expectedResult: makeExpectedAdmResult({
      keyword,
      suggestion: SPONSORED_WITH_SCORE,
    }),
  });
});

add_task(async function sponsoredWith_addonWith_addonWins_both() {
  let keyword = "sponsored with score, addon with score";
  let score = 10 * DEFAULT_SUGGESTION_SCORE;
  await doTest({
    keyword,
    scoreMap: {
      amo: score,
      adm_sponsored: score / 2,
    },
    expectedFeatureName: "AddonSuggestions",
    expectedScore: score,
    expectedResult: makeExpectedAddonResult({
      suggestion: ADDON_WITH_SCORE,
    }),
  });
});

add_task(async function merino_sponsored_addon_sponsoredWins() {
  await QuickSuggestTestUtils.setRemoteSettingsRecords([]);

  MerinoTestUtils.server.response.body.suggestions = [
    MERINO_SPONSORED_SUGGESTION,
    MERINO_ADDON_SUGGESTION,
  ];

  let score = 10 * DEFAULT_SUGGESTION_SCORE;
  await doTest({
    keyword: "test",
    scoreMap: {
      adm_sponsored: score,
    },
    expectedFeatureName: "AdmWikipedia",
    expectedScore: score,
    expectedResult: makeExpectedAdmResult({
      keyword: "test",
      suggestion: MERINO_SPONSORED_SUGGESTION,
      source: "merino",
    }),
  });

  await QuickSuggestTestUtils.setRemoteSettingsRecords(REMOTE_SETTINGS_RECORDS);
});

add_task(async function merino_sponsored_addon_addonWins() {
  await QuickSuggestTestUtils.setRemoteSettingsRecords([]);

  MerinoTestUtils.server.response.body.suggestions = [
    MERINO_SPONSORED_SUGGESTION,
    MERINO_ADDON_SUGGESTION,
  ];

  let score = 10 * DEFAULT_SUGGESTION_SCORE;
  await doTest({
    keyword: "test",
    scoreMap: {
      amo: score,
    },
    expectedFeatureName: "AddonSuggestions",
    expectedScore: score,
    expectedResult: makeExpectedAddonResult({
      suggestion: MERINO_ADDON_SUGGESTION,
      source: "merino",
    }),
  });

  await QuickSuggestTestUtils.setRemoteSettingsRecords(REMOTE_SETTINGS_RECORDS);
});

add_task(async function merino_sponsored_unknown_sponsoredWins() {
  await QuickSuggestTestUtils.setRemoteSettingsRecords([]);

  MerinoTestUtils.server.response.body.suggestions = [
    MERINO_SPONSORED_SUGGESTION,
    MERINO_UNKNOWN_SUGGESTION,
  ];

  let score = 10 * DEFAULT_SUGGESTION_SCORE;
  await doTest({
    keyword: "test",
    scoreMap: {
      adm_sponsored: score,
    },
    expectedFeatureName: "AdmWikipedia",
    expectedScore: score,
    expectedResult: makeExpectedAdmResult({
      keyword: "test",
      suggestion: MERINO_SPONSORED_SUGGESTION,
      source: "merino",
    }),
  });

  await QuickSuggestTestUtils.setRemoteSettingsRecords(REMOTE_SETTINGS_RECORDS);
});

add_task(async function merino_sponsored_unknown_unknownWins() {
  await QuickSuggestTestUtils.setRemoteSettingsRecords([]);

  MerinoTestUtils.server.response.body.suggestions = [
    MERINO_SPONSORED_SUGGESTION,
    MERINO_UNKNOWN_SUGGESTION,
  ];

  let score = 10 * DEFAULT_SUGGESTION_SCORE;
  await doTest({
    keyword: "test",
    scoreMap: {
      [MERINO_UNKNOWN_SUGGESTION.provider]: score,
    },
    expectedFeatureName: null,
    expectedScore: score,
    expectedResult: makeExpectedDefaultResult({
      suggestion: MERINO_UNKNOWN_SUGGESTION,
    }),
  });

  await QuickSuggestTestUtils.setRemoteSettingsRecords(REMOTE_SETTINGS_RECORDS);
});

add_task(async function stringValue() {
  let keyword = "sponsored with score, nonsponsored with score";
  await doTest({
    keyword,
    scoreMap: {
      adm_sponsored: "123.456",
    },
    expectedFeatureName: "AdmWikipedia",
    expectedScore: 123.456,
    expectedResult: makeExpectedAdmResult({
      keyword,
      suggestion: SPONSORED_WITH_SCORE,
    }),
  });
});

add_task(async function nanValue_sponsoredWins() {
  let keyword = "sponsored with score, nonsponsored without score";
  await doTest({
    keyword,
    scoreMap: {
      adm_nonsponsored: "this is NaN",
    },
    expectedFeatureName: "AdmWikipedia",
    expectedScore: 2 * DEFAULT_SUGGESTION_SCORE,
    expectedResult: makeExpectedAdmResult({
      keyword,
      suggestion: SPONSORED_WITH_SCORE,
    }),
  });
});

add_task(async function nanValue_nonsponsoredWins() {
  let keyword = "sponsored without score, nonsponsored with score";
  await doTest({
    keyword,
    scoreMap: {
      adm_sponsored: "this is NaN",
    },
    expectedFeatureName: "AdmWikipedia",
    expectedScore: 2 * DEFAULT_SUGGESTION_SCORE,
    expectedResult: makeExpectedAdmResult({
      keyword,
      suggestion: NONSPONSORED_WITH_SCORE,
    }),
  });
});

/**
 * Sets up Nimbus with a `quickSuggestScoreMap` variable value, does a search,
 * and makes sure the expected result is shown and the expected score is set on
 * the suggestion.
 *
 * @param {object} options
 *   Options object.
 * @param {string} options.keyword
 *   The search string. This should be equal to a keyword from one or more
 *   suggestions.
 * @param {object} options.scoreMap
 *   The value to set for the `quickSuggestScoreMap` variable.
 * @param {string} options.expectedFeatureName
 *   The name of the `BaseFeature` instance that is expected to create the
 *   `UrlbarResult` that's shown. If the suggestion is intentionally from an
 *   unknown Merino provider and therefore the quick suggest provider is
 *   expected to create a default result for it, set this to null.
 * @param {UrlbarResultstring} options.expectedResult
 *   The `UrlbarResult` that's expected to be shown.
 * @param {number} options.expectedScore
 *   The final `score` value that's expected to be defined on the suggestion
 *   object.
 */
async function doTest({
  keyword,
  scoreMap,
  expectedFeatureName,
  expectedResult,
  expectedScore,
}) {
  let cleanUpNimbus = await UrlbarTestUtils.initNimbusFeature({
    quickSuggestScoreMap: scoreMap,
  });

  // Stub the expected feature's `makeResult()` so we can see the value of the
  // passed-in suggestion's score. If the suggestion's type is in the score map,
  // the provider will set its score before calling `makeResult()`.
  let actualScore;
  let sandbox;
  if (expectedFeatureName) {
    sandbox = sinon.createSandbox();
    let feature = QuickSuggest.getFeature(expectedFeatureName);
    let stub = sandbox
      .stub(feature, "makeResult")
      .callsFake((queryContext, suggestion, searchString) => {
        actualScore = suggestion.score;
        return stub.wrappedMethod.call(
          feature,
          queryContext,
          suggestion,
          searchString
        );
      });
  }

  await check_results({
    context: createContext(keyword, {
      providers: [UrlbarProviderQuickSuggest.name],
      isPrivate: false,
    }),
    matches: [expectedResult],
  });

  if (expectedFeatureName) {
    Assert.equal(
      actualScore,
      expectedScore,
      "Suggestion score should be set correctly"
    );
    sandbox.restore();
  }

  await cleanUpNimbus();
}

function makeExpectedAdmResult({
  suggestion,
  keyword,
  source = "remote-settings",
}) {
  let isSponsored = suggestion.iab_category != "5 - Education";
  let result = {
    type: UrlbarUtils.RESULT_TYPE.URL,
    source: UrlbarUtils.RESULT_SOURCE.SEARCH,
    heuristic: false,
    payload: {
      source,
      isSponsored,
      provider: source == "remote-settings" ? "AdmWikipedia" : "adm",
      telemetryType: isSponsored ? "adm_sponsored" : "adm_nonsponsored",
      title: suggestion.title,
      url: suggestion.url,
      originalUrl: suggestion.url,
      displayUrl: suggestion.url.replace(/^https:\/\//, ""),
      icon: suggestion.icon,
      sponsoredBlockId:
        source == "remote-settings" ? suggestion.id : suggestion.block_id,
      sponsoredImpressionUrl: suggestion.impression_url,
      sponsoredClickUrl: suggestion.click_url,
      sponsoredAdvertiser: suggestion.advertiser,
      sponsoredIabCategory: suggestion.iab_category,
      qsSuggestion: keyword,
      descriptionL10n: isSponsored
        ? { id: "urlbar-result-action-sponsored" }
        : undefined,
      helpUrl: QuickSuggest.HELP_URL,
      helpL10n: {
        id: "urlbar-result-menu-learn-more-about-firefox-suggest",
      },
      isBlockable: true,
      blockL10n: {
        id: "urlbar-result-menu-dismiss-firefox-suggest",
      },
    },
  };

  if (source == "merino") {
    result.payload.requestId = "request_id";
  }

  return result;
}

function makeExpectedAddonResult({ suggestion, source = "remote-settings" }) {
  let url = new URL(suggestion.url);
  url.searchParams.set("utm_medium", "firefox-desktop");
  url.searchParams.set("utm_source", "firefox-suggest");

  return {
    type: UrlbarUtils.RESULT_TYPE.URL,
    source: UrlbarUtils.RESULT_SOURCE.SEARCH,
    heuristic: false,
    isBestMatch: true,
    payload: {
      source,
      provider: source == "remote-settings" ? "AddonSuggestions" : "amo",
      telemetryType: "amo",
      title: suggestion.title,
      url: url.href,
      originalUrl: suggestion.url,
      displayUrl: url.href.replace(/^https:\/\//, ""),
      shouldShowUrl: true,
      icon: suggestion.icon,
      description: suggestion.description,
      bottomTextL10n: { id: "firefox-suggest-addons-recommended" },
      helpUrl: QuickSuggest.HELP_URL,
    },
  };
}

function makeExpectedDefaultResult({ suggestion }) {
  return {
    type: UrlbarUtils.RESULT_TYPE.URL,
    source: UrlbarUtils.RESULT_SOURCE.SEARCH,
    heuristic: false,
    payload: {
      source: "merino",
      provider: suggestion.provider,
      telemetryType: suggestion.provider,
      isSponsored: suggestion.is_sponsored,
      title: suggestion.title,
      url: suggestion.url,
      displayUrl: suggestion.url.replace(/^https:\/\//, ""),
      icon: suggestion.icon,
      descriptionL10n: suggestion.is_sponsored
        ? { id: "urlbar-result-action-sponsored" }
        : undefined,
      shouldShowUrl: true,
      helpUrl: QuickSuggest.HELP_URL,
      helpL10n: {
        id: "urlbar-result-menu-learn-more-about-firefox-suggest",
      },
      isBlockable: true,
      blockL10n: {
        id: "urlbar-result-menu-dismiss-firefox-suggest",
      },
    },
  };
}
