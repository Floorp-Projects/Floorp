/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

// Tests impression frequency capping for quick suggest results.

"use strict";

const SUGGESTIONS = [
  {
    id: 1,
    url: "http://example.com/sponsored",
    title: "Sponsored suggestion",
    keywords: ["sponsored"],
    click_url: "http://example.com/click",
    impression_url: "http://example.com/impression",
    advertiser: "TestAdvertiser",
  },
  {
    id: 2,
    url: "http://example.com/nonsponsored",
    title: "Non-sponsored suggestion",
    keywords: ["nonsponsored"],
    click_url: "http://example.com/click",
    impression_url: "http://example.com/impression",
    advertiser: "TestAdvertiser",
    iab_category: "5 - Education",
  },
];

const EXPECTED_SPONSORED_RESULT = {
  type: UrlbarUtils.RESULT_TYPE.URL,
  source: UrlbarUtils.RESULT_SOURCE.SEARCH,
  heuristic: false,
  payload: {
    url: "http://example.com/sponsored",
    originalUrl: "http://example.com/sponsored",
    displayUrl: "http://example.com/sponsored",
    title: "Sponsored suggestion",
    qsSuggestion: "sponsored",
    icon: null,
    isSponsored: true,
    sponsoredImpressionUrl: "http://example.com/impression",
    sponsoredClickUrl: "http://example.com/click",
    sponsoredBlockId: 1,
    sponsoredAdvertiser: "TestAdvertiser",
    helpUrl: UrlbarProviderQuickSuggest.helpUrl,
    helpL10nId: "firefox-suggest-urlbar-learn-more",
    source: "remote-settings",
  },
};

const EXPECTED_NONSPONSORED_RESULT = {
  type: UrlbarUtils.RESULT_TYPE.URL,
  source: UrlbarUtils.RESULT_SOURCE.SEARCH,
  heuristic: false,
  payload: {
    url: "http://example.com/nonsponsored",
    originalUrl: "http://example.com/nonsponsored",
    displayUrl: "http://example.com/nonsponsored",
    title: "Non-sponsored suggestion",
    qsSuggestion: "nonsponsored",
    icon: null,
    isSponsored: false,
    sponsoredImpressionUrl: "http://example.com/impression",
    sponsoredClickUrl: "http://example.com/click",
    sponsoredBlockId: 2,
    sponsoredAdvertiser: "TestAdvertiser",
    helpUrl: UrlbarProviderQuickSuggest.helpUrl,
    helpL10nId: "firefox-suggest-urlbar-learn-more",
    source: "remote-settings",
  },
};

let gSandbox;
let gDateNowStub;

add_task(async function init() {
  UrlbarPrefs.set("quicksuggest.enabled", true);
  UrlbarPrefs.set("quicksuggest.impressionCaps.sponsoredEnabled", true);
  UrlbarPrefs.set("quicksuggest.impressionCaps.nonSponsoredEnabled", true);
  UrlbarPrefs.set("suggest.quicksuggest.nonsponsored", true);
  UrlbarPrefs.set("suggest.quicksuggest.sponsored", true);
  UrlbarPrefs.set("bestMatch.enabled", false);

  // Disable search suggestions so we don't hit the network.
  Services.prefs.setBoolPref("browser.search.suggest.enabled", false);

  await QuickSuggestTestUtils.ensureQuickSuggestInit(SUGGESTIONS);

  // Set up a sinon stub for the `Date.now()` implementation inside of
  // UrlbarProviderQuickSuggest. This lets us test searches performed at
  // specific times. See `doTimedCallbacks()` for more info.
  gSandbox = sinon.createSandbox();
  gDateNowStub = gSandbox.stub(
    Cu.getGlobalForObject(UrlbarProviderQuickSuggest).Date,
    "now"
  );
});

// Tests a single interval.
add_task(async function oneInterval() {
  await doTest({
    config: {
      impression_caps: {
        sponsored: {
          custom: [{ interval_s: 3, max_count: 1 }],
        },
      },
    },
    callback: async () => {
      await doTimedSearches("sponsored", {
        0: [[EXPECTED_SPONSORED_RESULT], []],
        1: [[]],
        2: [[]],
        3: [[EXPECTED_SPONSORED_RESULT], []],
        4: [[]],
        5: [[]],
      });
    },
  });
});

// Tests multiple intervals.
add_task(async function multipleIntervals() {
  await doTest({
    config: {
      impression_caps: {
        sponsored: {
          custom: [
            { interval_s: 1, max_count: 1 },
            { interval_s: 5, max_count: 3 },
            { interval_s: 10, max_count: 5 },
          ],
        },
      },
    },
    callback: async () => {
      await doTimedSearches("sponsored", {
        0: [[EXPECTED_SPONSORED_RESULT], []],
        1: [[EXPECTED_SPONSORED_RESULT], []],
        2: [[EXPECTED_SPONSORED_RESULT], []],
        3: [[]],
        4: [[]],
        5: [[EXPECTED_SPONSORED_RESULT], []],
        6: [[EXPECTED_SPONSORED_RESULT], []],
        7: [[]],
        8: [[]],
        9: [[]],
        10: [[EXPECTED_SPONSORED_RESULT], []],
        11: [[EXPECTED_SPONSORED_RESULT], []],
        12: [[EXPECTED_SPONSORED_RESULT], []],
        13: [[]],
        14: [[]],
        15: [[EXPECTED_SPONSORED_RESULT], []],
        16: [[EXPECTED_SPONSORED_RESULT], []],
        17: [[]],
        18: [[]],
        19: [[]],
        20: [[EXPECTED_SPONSORED_RESULT], []],
      });
    },
  });
});

// Tests a lifetime cap.
add_task(async function lifetime() {
  await doTest({
    config: {
      impression_caps: {
        sponsored: {
          lifetime: 3,
        },
      },
    },
    callback: async () => {
      for (let i = 0; i < 3; i++) {
        await checkSearch({
          name: i,
          searchString: "sponsored",
          expectedResults: [EXPECTED_SPONSORED_RESULT],
        });
      }
      await checkSearch({
        name: "additional",
        searchString: "sponsored",
        expectedResults: [],
      });
    },
  });
});

// Tests one interval and a lifetime cap together.
add_task(async function intervalAndLifetime() {
  await doTest({
    config: {
      impression_caps: {
        sponsored: {
          lifetime: 3,
          custom: [{ interval_s: 1, max_count: 1 }],
        },
      },
    },
    callback: async () => {
      await doTimedSearches("sponsored", {
        0: [[EXPECTED_SPONSORED_RESULT], []],
        1: [[EXPECTED_SPONSORED_RESULT], []],
        2: [[EXPECTED_SPONSORED_RESULT], []],
        3: [[]],
        4: [[]],
      });
    },
  });
});

// Tests multiple intervals and a lifetime cap together.
add_task(async function multipleIntervalsAndLifetime() {
  await doTest({
    config: {
      impression_caps: {
        sponsored: {
          lifetime: 4,
          custom: [
            { interval_s: 1, max_count: 1 },
            { interval_s: 5, max_count: 3 },
          ],
        },
      },
    },
    callback: async () => {
      await doTimedSearches("sponsored", {
        0: [[EXPECTED_SPONSORED_RESULT], []],
        1: [[EXPECTED_SPONSORED_RESULT], []],
        2: [[EXPECTED_SPONSORED_RESULT], []],
        3: [[]],
        4: [[]],
        5: [[EXPECTED_SPONSORED_RESULT], []],
        6: [[]],
        7: [[]],
      });
    },
  });
});

// Smoke test for non-sponsored caps. Most tasks use sponsored results and caps,
// but sponsored and non-sponsored should behave the same since they use the
// same code paths.
add_task(async function nonsponsored() {
  await doTest({
    config: {
      impression_caps: {
        nonsponsored: {
          lifetime: 4,
          custom: [
            { interval_s: 1, max_count: 1 },
            { interval_s: 5, max_count: 3 },
          ],
        },
      },
    },
    callback: async () => {
      await doTimedSearches("nonsponsored", {
        0: [[EXPECTED_NONSPONSORED_RESULT], []],
        1: [[EXPECTED_NONSPONSORED_RESULT], []],
        2: [[EXPECTED_NONSPONSORED_RESULT], []],
        3: [[]],
        4: [[]],
        5: [[EXPECTED_NONSPONSORED_RESULT], []],
        6: [[]],
        7: [[]],
      });
    },
  });
});

// Smoke test for sponsored and non-sponsored caps together. Most tasks use only
// sponsored results and caps, but sponsored and non-sponsored should behave the
// same since they use the same code paths.
add_task(async function sponsoredAndNonsponsored() {
  await doTest({
    config: {
      impression_caps: {
        sponsored: {
          lifetime: 2,
        },
        nonsponsored: {
          lifetime: 3,
        },
      },
    },
    callback: async () => {
      await checkSearch({
        name: "sponsored 1",
        searchString: "sponsored",
        expectedResults: [EXPECTED_SPONSORED_RESULT],
      });
      await checkSearch({
        name: "nonsponsored 1",
        searchString: "nonsponsored",
        expectedResults: [EXPECTED_NONSPONSORED_RESULT],
      });

      await checkSearch({
        name: "sponsored 2",
        searchString: "sponsored",
        expectedResults: [EXPECTED_SPONSORED_RESULT],
      });
      await checkSearch({
        name: "nonsponsored 2",
        searchString: "nonsponsored",
        expectedResults: [EXPECTED_NONSPONSORED_RESULT],
      });

      await checkSearch({
        name: "sponsored 3",
        searchString: "sponsored",
        expectedResults: [],
      });
      await checkSearch({
        name: "nonsponsored 3",
        searchString: "nonsponsored",
        expectedResults: [EXPECTED_NONSPONSORED_RESULT],
      });

      await checkSearch({
        name: "sponsored 4",
        searchString: "sponsored",
        expectedResults: [],
      });
      await checkSearch({
        name: "nonsponsored 4",
        searchString: "nonsponsored",
        expectedResults: [],
      });
    },
  });
});

// Tests with an empty config to make sure results are not capped.
add_task(async function emptyConfig() {
  await doTest({
    config: {},
    callback: async () => {
      for (let i = 0; i < 2; i++) {
        await checkSearch({
          name: "sponsored " + i,
          searchString: "sponsored",
          expectedResults: [EXPECTED_SPONSORED_RESULT],
        });
        await checkSearch({
          name: "nonsponsored " + i,
          searchString: "nonsponsored",
          expectedResults: [EXPECTED_NONSPONSORED_RESULT],
        });
      }
    },
  });
});

// Tests with sponsored caps disabled. Non-sponsored should still be capped.
add_task(async function sponsoredCapsDisabled() {
  UrlbarPrefs.set("quicksuggest.impressionCaps.sponsoredEnabled", false);
  await doTest({
    config: {
      impression_caps: {
        sponsored: {
          lifetime: 0,
        },
        nonsponsored: {
          lifetime: 3,
        },
      },
    },
    callback: async () => {
      for (let i = 0; i < 3; i++) {
        await checkSearch({
          name: "sponsored " + i,
          searchString: "sponsored",
          expectedResults: [EXPECTED_SPONSORED_RESULT],
        });
        await checkSearch({
          name: "nonsponsored " + i,
          searchString: "nonsponsored",
          expectedResults: [EXPECTED_NONSPONSORED_RESULT],
        });
      }
      await checkSearch({
        name: "sponsored additional",
        searchString: "sponsored",
        expectedResults: [EXPECTED_SPONSORED_RESULT],
      });
      await checkSearch({
        name: "nonsponsored additional",
        searchString: "nonsponsored",
        expectedResults: [],
      });
    },
  });
  UrlbarPrefs.set("quicksuggest.impressionCaps.sponsoredEnabled", true);
});

// Tests with non-sponsored caps disabled. Sponsored should still be capped.
add_task(async function nonsponsoredCapsDisabled() {
  UrlbarPrefs.set("quicksuggest.impressionCaps.nonSponsoredEnabled", false);
  await doTest({
    config: {
      impression_caps: {
        sponsored: {
          lifetime: 3,
        },
        nonsponsored: {
          lifetime: 0,
        },
      },
    },
    callback: async () => {
      for (let i = 0; i < 3; i++) {
        await checkSearch({
          name: "sponsored " + i,
          searchString: "sponsored",
          expectedResults: [EXPECTED_SPONSORED_RESULT],
        });
        await checkSearch({
          name: "nonsponsored " + i,
          searchString: "nonsponsored",
          expectedResults: [EXPECTED_NONSPONSORED_RESULT],
        });
      }
      await checkSearch({
        name: "sponsored additional",
        searchString: "sponsored",
        expectedResults: [],
      });
      await checkSearch({
        name: "nonsponsored additional",
        searchString: "nonsponsored",
        expectedResults: [EXPECTED_NONSPONSORED_RESULT],
      });
    },
  });
  UrlbarPrefs.set("quicksuggest.impressionCaps.nonSponsoredEnabled", true);
});

// Tests a config change: 1 interval -> same interval with lower cap, with the
// old cap already reached
add_task(async function configChange_sameIntervalLowerCap_1() {
  await doTest({
    config: {
      impression_caps: {
        sponsored: {
          custom: [{ interval_s: 3, max_count: 3 }],
        },
      },
    },
    callback: async () => {
      await doTimedCallbacks({
        0: async () => {
          for (let i = 0; i < 3; i++) {
            await checkSearch({
              name: "0s " + i,
              searchString: "sponsored",
              expectedResults: [EXPECTED_SPONSORED_RESULT],
            });
          }
          await checkSearch({
            name: "0s additional",
            searchString: "sponsored",
            expectedResults: [],
          });
          QuickSuggestTestUtils.setConfig({
            impression_caps: {
              sponsored: {
                custom: [{ interval_s: 3, max_count: 1 }],
              },
            },
          });
        },
        1: async () => {
          await checkSearch({
            name: "1s",
            searchString: "sponsored",
            expectedResults: [],
          });
        },
        3: async () => {
          await checkSearch({
            name: "3s 0",
            searchString: "sponsored",
            expectedResults: [EXPECTED_SPONSORED_RESULT],
          });
          await checkSearch({
            name: "3s additional",
            searchString: "sponsored",
            expectedResults: [],
          });
        },
      });
    },
  });
});

// Tests a config change: 1 interval -> same interval with lower cap, with the
// old cap not reached
add_task(async function configChange_sameIntervalLowerCap_2() {
  await doTest({
    config: {
      impression_caps: {
        sponsored: {
          custom: [{ interval_s: 3, max_count: 3 }],
        },
      },
    },
    callback: async () => {
      await doTimedCallbacks({
        0: async () => {
          for (let i = 0; i < 2; i++) {
            await checkSearch({
              name: "0s " + i,
              searchString: "sponsored",
              expectedResults: [EXPECTED_SPONSORED_RESULT],
            });
          }
          QuickSuggestTestUtils.setConfig({
            impression_caps: {
              sponsored: {
                custom: [{ interval_s: 3, max_count: 1 }],
              },
            },
          });
        },
        1: async () => {
          await checkSearch({
            name: "1s",
            searchString: "sponsored",
            expectedResults: [],
          });
        },
        3: async () => {
          await checkSearch({
            name: "3s 0",
            searchString: "sponsored",
            expectedResults: [EXPECTED_SPONSORED_RESULT],
          });
          await checkSearch({
            name: "3s additional",
            searchString: "sponsored",
            expectedResults: [],
          });
        },
      });
    },
  });
});

// Tests a config change: 1 interval -> same interval with higher cap
add_task(async function configChange_sameIntervalHigherCap() {
  await doTest({
    config: {
      impression_caps: {
        sponsored: {
          custom: [{ interval_s: 3, max_count: 3 }],
        },
      },
    },
    callback: async () => {
      await doTimedCallbacks({
        0: async () => {
          for (let i = 0; i < 3; i++) {
            await checkSearch({
              name: "0s " + i,
              searchString: "sponsored",
              expectedResults: [EXPECTED_SPONSORED_RESULT],
            });
          }
          await checkSearch({
            name: "0s additional",
            searchString: "sponsored",
            expectedResults: [],
          });
          QuickSuggestTestUtils.setConfig({
            impression_caps: {
              sponsored: {
                custom: [{ interval_s: 3, max_count: 5 }],
              },
            },
          });
        },
        1: async () => {
          for (let i = 0; i < 2; i++) {
            await checkSearch({
              name: "1s " + i,
              searchString: "sponsored",
              expectedResults: [EXPECTED_SPONSORED_RESULT],
            });
          }
          await checkSearch({
            name: "1s additional",
            searchString: "sponsored",
            expectedResults: [],
          });
        },
        3: async () => {
          for (let i = 0; i < 5; i++) {
            await checkSearch({
              name: "3s " + i,
              searchString: "sponsored",
              expectedResults: [EXPECTED_SPONSORED_RESULT],
            });
          }
          await checkSearch({
            name: "3s additional",
            searchString: "sponsored",
            expectedResults: [],
          });
        },
      });
    },
  });
});

// Tests a config change: 1 interval -> 2 new intervals with higher timeouts.
// Impression counts for the old interval should contribute to the new
// intervals.
add_task(async function configChange_1IntervalTo2NewIntervalsHigher() {
  await doTest({
    config: {
      impression_caps: {
        sponsored: {
          custom: [{ interval_s: 3, max_count: 3 }],
        },
      },
    },
    callback: async () => {
      await doTimedCallbacks({
        0: async () => {
          for (let i = 0; i < 3; i++) {
            await checkSearch({
              name: "0s " + i,
              searchString: "sponsored",
              expectedResults: [EXPECTED_SPONSORED_RESULT],
            });
          }
          QuickSuggestTestUtils.setConfig({
            impression_caps: {
              sponsored: {
                custom: [
                  { interval_s: 5, max_count: 3 },
                  { interval_s: 10, max_count: 5 },
                ],
              },
            },
          });
        },
        3: async () => {
          await checkSearch({
            name: "3s",
            searchString: "sponsored",
            expectedResults: [],
          });
        },
        4: async () => {
          await checkSearch({
            name: "4s",
            searchString: "sponsored",
            expectedResults: [],
          });
        },
        5: async () => {
          for (let i = 0; i < 2; i++) {
            await checkSearch({
              name: "5s " + i,
              searchString: "sponsored",
              expectedResults: [EXPECTED_SPONSORED_RESULT],
            });
          }
          await checkSearch({
            name: "5s additional",
            searchString: "sponsored",
            expectedResults: [],
          });
        },
      });
    },
  });
});

// Tests a config change: 2 intervals -> 1 new interval with higher timeout.
// Impression counts for the old intervals should contribute to the new
// interval.
add_task(async function configChange_2IntervalsTo1NewIntervalHigher() {
  await doTest({
    config: {
      impression_caps: {
        sponsored: {
          custom: [
            { interval_s: 2, max_count: 2 },
            { interval_s: 4, max_count: 4 },
          ],
        },
      },
    },
    callback: async () => {
      await doTimedCallbacks({
        0: async () => {
          for (let i = 0; i < 2; i++) {
            await checkSearch({
              name: "0s " + i,
              searchString: "sponsored",
              expectedResults: [EXPECTED_SPONSORED_RESULT],
            });
          }
        },
        2: async () => {
          for (let i = 0; i < 2; i++) {
            await checkSearch({
              name: "2s " + i,
              searchString: "sponsored",
              expectedResults: [EXPECTED_SPONSORED_RESULT],
            });
          }
          QuickSuggestTestUtils.setConfig({
            impression_caps: {
              sponsored: {
                custom: [{ interval_s: 6, max_count: 5 }],
              },
            },
          });
        },
        4: async () => {
          await checkSearch({
            name: "4s 0",
            searchString: "sponsored",
            expectedResults: [EXPECTED_SPONSORED_RESULT],
          });
          await checkSearch({
            name: "4s 1",
            searchString: "sponsored",
            expectedResults: [],
          });
        },
        5: async () => {
          await checkSearch({
            name: "5s",
            searchString: "sponsored",
            expectedResults: [],
          });
        },
        6: async () => {
          for (let i = 0; i < 5; i++) {
            await checkSearch({
              name: "6s " + i,
              searchString: "sponsored",
              expectedResults: [EXPECTED_SPONSORED_RESULT],
            });
          }
          await checkSearch({
            name: "6s additional",
            searchString: "sponsored",
            expectedResults: [],
          });
        },
      });
    },
  });
});

// Tests a config change: 1 interval -> 1 new interval with lower timeout.
// Impression counts for the old interval should not contribute to the new
// interval since the new interval has a lower timeout.
add_task(async function configChange_1IntervalTo1NewIntervalLower() {
  await doTest({
    config: {
      impression_caps: {
        sponsored: {
          custom: [{ interval_s: 5, max_count: 3 }],
        },
      },
    },
    callback: async () => {
      await doTimedCallbacks({
        0: async () => {
          for (let i = 0; i < 3; i++) {
            await checkSearch({
              name: "0s " + i,
              searchString: "sponsored",
              expectedResults: [EXPECTED_SPONSORED_RESULT],
            });
          }
          QuickSuggestTestUtils.setConfig({
            impression_caps: {
              sponsored: {
                custom: [{ interval_s: 3, max_count: 3 }],
              },
            },
          });
        },
        1: async () => {
          for (let i = 0; i < 3; i++) {
            await checkSearch({
              name: "3s " + i,
              searchString: "sponsored",
              expectedResults: [EXPECTED_SPONSORED_RESULT],
            });
          }
          await checkSearch({
            name: "3s additional",
            searchString: "sponsored",
            expectedResults: [],
          });
        },
      });
    },
  });
});

// Tests a config change: 1 interval -> lifetime.
// Impression counts for the old interval should contribute to the new lifetime
// cap.
add_task(async function configChange_1IntervalToLifetime() {
  await doTest({
    config: {
      impression_caps: {
        sponsored: {
          custom: [{ interval_s: 3, max_count: 3 }],
        },
      },
    },
    callback: async () => {
      await doTimedCallbacks({
        0: async () => {
          for (let i = 0; i < 3; i++) {
            await checkSearch({
              name: "0s " + i,
              searchString: "sponsored",
              expectedResults: [EXPECTED_SPONSORED_RESULT],
            });
          }
          QuickSuggestTestUtils.setConfig({
            impression_caps: {
              sponsored: {
                lifetime: 3,
                custom: [{ interval_s: 1, max_count: 1 }],
              },
            },
          });
        },
        3: async () => {
          await checkSearch({
            name: "3s",
            searchString: "sponsored",
            expectedResults: [],
          });
        },
      });
    },
  });
});

// Tests a config change: lifetime cap -> higher lifetime cap
add_task(async function configChange_lifetimeCapHigher() {
  await doTest({
    config: {
      impression_caps: {
        sponsored: {
          lifetime: 3,
        },
      },
    },
    callback: async () => {
      await doTimedCallbacks({
        0: async () => {
          for (let i = 0; i < 3; i++) {
            await checkSearch({
              name: "0s " + i,
              searchString: "sponsored",
              expectedResults: [EXPECTED_SPONSORED_RESULT],
            });
          }
          await checkSearch({
            name: "0s additional",
            searchString: "sponsored",
            expectedResults: [],
          });
          QuickSuggestTestUtils.setConfig({
            impression_caps: {
              sponsored: {
                lifetime: 5,
              },
            },
          });
        },
        1: async () => {
          for (let i = 0; i < 2; i++) {
            await checkSearch({
              name: "1s " + i,
              searchString: "sponsored",
              expectedResults: [EXPECTED_SPONSORED_RESULT],
            });
          }
          await checkSearch({
            name: "1s additional",
            searchString: "sponsored",
            expectedResults: [],
          });
        },
      });
    },
  });
});

// Tests a config change: lifetime cap -> lower lifetime cap
add_task(async function configChange_lifetimeCapLower() {
  await doTest({
    config: {
      impression_caps: {
        sponsored: {
          lifetime: 3,
        },
      },
    },
    callback: async () => {
      await doTimedCallbacks({
        0: async () => {
          for (let i = 0; i < 3; i++) {
            await checkSearch({
              name: "0s " + i,
              searchString: "sponsored",
              expectedResults: [EXPECTED_SPONSORED_RESULT],
            });
          }
          await checkSearch({
            name: "0s additional",
            searchString: "sponsored",
            expectedResults: [],
          });
          QuickSuggestTestUtils.setConfig({
            impression_caps: {
              sponsored: {
                lifetime: 1,
              },
            },
          });
        },
        1: async () => {
          await checkSearch({
            name: "1s",
            searchString: "sponsored",
            expectedResults: [],
          });
        },
      });
    },
  });
});

// Makes sure stats are serialized to and from the pref correctly.
add_task(async function prefSync() {
  await doTest({
    config: {
      impression_caps: {
        sponsored: {
          lifetime: 5,
          custom: [
            { interval_s: 3, max_count: 2 },
            { interval_s: 5, max_count: 4 },
          ],
        },
      },
    },
    callback: async () => {
      for (let i = 0; i < 2; i++) {
        await checkSearch({
          name: i,
          searchString: "sponsored",
          expectedResults: [EXPECTED_SPONSORED_RESULT],
        });
      }

      let json = UrlbarPrefs.get("quicksuggest.impressionCaps.stats");
      Assert.ok(json, "JSON is non-empty");
      Assert.deepEqual(
        JSON.parse(json),
        {
          sponsored: [
            { intervalSeconds: 3, count: 2, maxCount: 2, startDateMs: 0 },
            { intervalSeconds: 5, count: 2, maxCount: 4, startDateMs: 0 },
            { intervalSeconds: null, count: 2, maxCount: 5, startDateMs: 0 },
          ],
        },
        "JSON is correct"
      );

      UrlbarProviderQuickSuggest._impressionStats = null;
      UrlbarProviderQuickSuggest._loadImpressionStats();
      Assert.deepEqual(
        UrlbarProviderQuickSuggest._impressionStats,
        {
          sponsored: [
            { intervalSeconds: 3, count: 2, maxCount: 2, startDateMs: 0 },
            { intervalSeconds: 5, count: 2, maxCount: 4, startDateMs: 0 },
            {
              intervalSeconds: Infinity,
              count: 2,
              maxCount: 5,
              startDateMs: 0,
            },
          ],
        },
        "Impression stats were properly restored from the pref"
      );
    },
  });
});

// Tests direct changes to the stats pref.
add_task(async function prefDirectlyChanged() {
  await doTest({
    config: {
      impression_caps: {
        sponsored: {
          lifetime: 5,
          custom: [{ interval_s: 3, max_count: 3 }],
        },
      },
    },
    callback: async () => {
      let expectedStats = {
        sponsored: [
          { intervalSeconds: 3, count: 0, maxCount: 3, startDateMs: 0 },
          { intervalSeconds: Infinity, count: 0, maxCount: 5, startDateMs: 0 },
        ],
      };

      UrlbarPrefs.set("quicksuggest.impressionCaps.stats", "bogus");
      Assert.deepEqual(
        UrlbarProviderQuickSuggest._impressionStats,
        expectedStats,
        "Expected stats for 'bogus'"
      );

      UrlbarPrefs.set("quicksuggest.impressionCaps.stats", JSON.stringify({}));
      Assert.deepEqual(
        UrlbarProviderQuickSuggest._impressionStats,
        expectedStats,
        "Expected stats for {}"
      );

      UrlbarPrefs.set(
        "quicksuggest.impressionCaps.stats",
        JSON.stringify({ sponsored: "bogus" })
      );
      Assert.deepEqual(
        UrlbarProviderQuickSuggest._impressionStats,
        expectedStats,
        "Expected stats for { sponsored: 'bogus' }"
      );

      UrlbarPrefs.set(
        "quicksuggest.impressionCaps.stats",
        JSON.stringify({
          sponsored: [
            { intervalSeconds: 3, count: 0, maxCount: 3, startDateMs: 0 },
            { intervalSeconds: "bogus", count: 0, startDateMs: 0 },
            {
              intervalSeconds: Infinity,
              count: 0,
              maxCount: 5,
              startDateMs: 0,
            },
          ],
        })
      );
      Assert.deepEqual(
        UrlbarProviderQuickSuggest._impressionStats,
        expectedStats,
        "Expected stats with intervalSeconds: 'bogus'"
      );

      UrlbarPrefs.set(
        "quicksuggest.impressionCaps.stats",
        JSON.stringify({
          sponsored: [
            { intervalSeconds: 3, count: 0, maxCount: 123, startDateMs: 0 },
            {
              intervalSeconds: Infinity,
              count: 0,
              maxCount: 456,
              startDateMs: 0,
            },
          ],
        })
      );
      Assert.deepEqual(
        UrlbarProviderQuickSuggest._impressionStats,
        expectedStats,
        "Expected stats with `maxCount` values different from caps"
      );

      let stats = {
        sponsored: [
          { intervalSeconds: 3, count: 1, maxCount: 3, startDateMs: 99 },
          {
            intervalSeconds: Infinity,
            count: 7,
            maxCount: 5,
            startDateMs: 1337,
          },
        ],
      };
      UrlbarPrefs.set(
        "quicksuggest.impressionCaps.stats",
        JSON.stringify(stats)
      );
      Assert.deepEqual(
        UrlbarProviderQuickSuggest._impressionStats,
        stats,
        "Expected stats with valid JSON"
      );
    },
  });
});

/**
 * Main test helper. Sets up state, calls your callback, and resets state.
 *
 * @param {object} config
 *   The quick suggest config to use during the test.
 * @param {function} callback
 */
async function doTest({ config, callback }) {
  // Make `Date.now()` return 0 to start with. It's necessary to do this before
  // calling `withConfig()` because when a new config is set, the provider
  // validates its impression stats, whose `startDateMs` values depend on
  // `Date.now()`.
  gDateNowStub.returns(0);

  await QuickSuggestTestUtils.withConfig({ config, callback });
  UrlbarPrefs.clear("quicksuggest.impressionCaps.stats");
}

/**
 * Does a series of timed searches and checks their results. This function
 * relies on `doTimedCallbacks()`, so it may be helpful to look at it too.
 *
 * @param {string} searchString
 * @param {object} expectedResultsListsBySecond
 *   An object that maps from seconds to an array of expected results arrays.
 *   For a given entry `S -> L` in this object, searches are performed S seconds
 *   after this function is called, one search per item in L. The item in L at
 *   index i is itself an array of expected results for the i'th search.
 *
 *   Example:
 *
 *     {
 *       0: [[R1], []],
 *       1: [[]],
 *     }
 *
 *     0 seconds after `doTimedSearches()` is called, two searches are
 *     performed. The first one is expected to return a single result R1, and
 *     the second search is expected to return no results.
 *
 *     1 second after `doTimedSearches()` is called, one search is performed.
 *     It's expected to return no results.
 */
async function doTimedSearches(searchString, expectedResultsListsBySecond) {
  await doTimedCallbacks(
    Object.entries(expectedResultsListsBySecond).reduce(
      (memo, [second, expectedResultsList]) => {
        memo[second] = async () => {
          for (let i = 0; i < expectedResultsList.length; i++) {
            let expectedResults = expectedResultsList[i];
            await checkSearch({
              searchString,
              expectedResults,
              name: `${second}s search ${i + 1} of ${
                expectedResultsList.length
              }`,
            });
          }
        };
        return memo;
      },
      {}
    )
  );
}

/**
 * Takes a series a callbacks and times at which they should be called, and
 * calls them accordingly. This function is specifically designed for
 * UrlbarProviderQuickSuggest and its impression capping implementation because
 * it works by stubbing `Date.now()` within UrlbarProviderQuickSuggest. The
 * callbacks are not actually called at the given times but instead `Date.now()`
 * is stubbed so that UrlbarProviderQuickSuggest will think they are being
 * called at the given times.
 *
 * A more general implementation of this helper function that isn't tailored to
 * UrlbarProviderQuickSuggest is commented out below, and unfortunately it
 * doesn't work properly on macOS.
 *
 * @param {object} callbacksBySecond
 *   An object that maps from seconds to callback functions. For a given entry
 *   `S -> F` in this object, the callback F is called S seconds after
 *   `doTimedCallbacks()` is called.
 */
async function doTimedCallbacks(callbacksBySecond) {
  let entries = Object.entries(callbacksBySecond).sort(([t1], [t2]) => t1 - t2);
  for (let [timeoutSeconds, callback] of entries) {
    gDateNowStub.returns(1000 * timeoutSeconds);
    await callback();
  }
}

/*
// This is the original implementation of `doTimedCallbacks()`, left here for
// reference or in case the macOS problem described below is fixed. Instead of
// stubbing `Date.now()` within UrlbarProviderQuickSuggest, it starts parallel
// timers so that the callbacks are actually called at appropriate times. This
// version of `doTimedCallbacks()` is therefore more generally useful, but it
// has the drawback that your test has to run in real time. e.g., if one of your
// callbacks needs to run 10s from now, the test must actually wait 10s.
//
// Unfortunately macOS seems to have some kind of limit of ~33 total 1-second
// timers during any xpcshell test -- not 33 simultaneous timers but 33 total
// timers. After that, timers fire randomly and with huge timeout periods that
// are often exactly 10s greater than the specified period, as if some 10s
// timeout internal to macOS is being hit. This problem does not seem to happen
// when running the full browser, only during xpcshell tests. In fact the
// problem can be reproduced in an xpcshell test that simply creates an interval
// timer whose period is 1s (e.g., using `setInterval()` from Timer.jsm). After
// ~33 ticks, the timer's period jumps to ~10s.
async function doTimedCallbacks(callbacksBySecond) {
  await Promise.all(
    Object.entries(callbacksBySecond).map(
      ([timeoutSeconds, callback]) => new Promise(
        resolve => setTimeout(
          () => callback().then(resolve),
          1000 * parseInt(timeoutSeconds)
        )
      )
    )
  );
}
*/

/**
 * Does a search, triggers an engagement, and checks the results.
 *
 * @param {string} name
 *   This value is the name of the search and will be logged in messages to make
 *   debugging easier.
 * @param {string} searchString
 * @param {array} expectedResults
 */
async function checkSearch({ name, searchString, expectedResults }) {
  info(`Doing search "${name}" with search string "${searchString}"`);
  let context = createContext(searchString, {
    providers: [UrlbarProviderQuickSuggest.name],
    isPrivate: false,
  });
  info(`Checking results for search "${name}"`);
  await check_results({
    context,
    matches: expectedResults,
  });
  info(`Done checking results for search "${name}"`);

  // Impression stats are updated only on engagement, so force one now.
  // `selIndex` doesn't really matter but since we're not trying to simulate a
  // click on the suggestion, pass in -1 to ensure we don't record a click. Pass
  // in true for `isPrivate` so we don't attempt to record the impression ping
  // because otherwise the following PingCentre error is logged:
  // "Structured Ingestion ping failure with error: undefined"
  let isPrivate = true;
  UrlbarProviderQuickSuggest.onEngagement(isPrivate, "engagement", context, {
    selIndex: -1,
  });
}
