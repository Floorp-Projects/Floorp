/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

// Tests impression frequency capping for quick suggest results.

"use strict";

ChromeUtils.defineESModuleGetters(this, {
  AsyncShutdown: "resource://gre/modules/AsyncShutdown.sys.mjs",
  setTimeout: "resource://gre/modules/Timer.sys.mjs",
});

const REMOTE_SETTINGS_RESULTS = [
  {
    id: 1,
    url: "http://example.com/sponsored",
    title: "Sponsored suggestion",
    keywords: ["sponsored"],
    click_url: "http://example.com/click",
    impression_url: "http://example.com/impression",
    advertiser: "TestAdvertiser",
    iab_category: "22 - Shopping",
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

const EXPECTED_SPONSORED_URLBAR_RESULT = {
  type: UrlbarUtils.RESULT_TYPE.URL,
  source: UrlbarUtils.RESULT_SOURCE.SEARCH,
  heuristic: false,
  payload: {
    telemetryType: "adm_sponsored",
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
    sponsoredIabCategory: "22 - Shopping",
    descriptionL10n: { id: "urlbar-result-action-sponsored" },
    helpUrl: QuickSuggest.HELP_URL,
    helpL10n: {
      id: "urlbar-result-menu-learn-more-about-firefox-suggest",
    },
    isBlockable: true,
    blockL10n: {
      id: "urlbar-result-menu-dismiss-firefox-suggest",
    },
    source: "remote-settings",
    provider: "AdmWikipedia",
  },
};

const EXPECTED_NONSPONSORED_URLBAR_RESULT = {
  type: UrlbarUtils.RESULT_TYPE.URL,
  source: UrlbarUtils.RESULT_SOURCE.SEARCH,
  heuristic: false,
  payload: {
    telemetryType: "adm_nonsponsored",
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
    sponsoredIabCategory: "5 - Education",
    helpUrl: QuickSuggest.HELP_URL,
    helpL10n: {
      id: "urlbar-result-menu-learn-more-about-firefox-suggest",
    },
    isBlockable: true,
    blockL10n: {
      id: "urlbar-result-menu-dismiss-firefox-suggest",
    },
    source: "remote-settings",
    provider: "AdmWikipedia",
  },
};

let gSandbox;
let gDateNowStub;
let gStartupDateMsStub;

add_setup(async () => {
  // Disable search suggestions so we don't hit the network.
  Services.prefs.setBoolPref("browser.search.suggest.enabled", false);

  await QuickSuggestTestUtils.ensureQuickSuggestInit({
    remoteSettingsRecords: [
      {
        type: "data",
        attachment: REMOTE_SETTINGS_RESULTS,
      },
    ],
    prefs: [
      ["quicksuggest.impressionCaps.sponsoredEnabled", true],
      ["quicksuggest.impressionCaps.nonSponsoredEnabled", true],
      ["suggest.quicksuggest.nonsponsored", true],
      ["suggest.quicksuggest.sponsored", true],
    ],
  });

  // Set up a sinon stub for the `Date.now()` implementation inside of
  // UrlbarProviderQuickSuggest. This lets us test searches performed at
  // specific times. See `doTimedCallbacks()` for more info.
  gSandbox = sinon.createSandbox();
  gDateNowStub = gSandbox.stub(
    Cu.getGlobalForObject(UrlbarProviderQuickSuggest).Date,
    "now"
  );

  // Set up a sinon stub for `UrlbarProviderQuickSuggest._getStartupDateMs()` to
  // let the test override the startup date.
  gStartupDateMsStub = gSandbox.stub(
    QuickSuggest.impressionCaps,
    "_getStartupDateMs"
  );
  gStartupDateMsStub.returns(0);
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
        0: {
          results: [[EXPECTED_SPONSORED_URLBAR_RESULT], []],
          telemetry: {
            events: [
              {
                object: "hit",
                extra: {
                  eventDate: "0",
                  intervalSeconds: "3",
                  maxCount: "1",
                  startDate: "0",
                  impressionDate: "0",
                  count: "1",
                  type: "sponsored",
                  eventCount: "1",
                },
              },
            ],
          },
        },
        1: {
          results: [[]],
        },
        2: {
          results: [[]],
        },
        3: {
          results: [[EXPECTED_SPONSORED_URLBAR_RESULT], []],
          telemetry: {
            events: [
              {
                object: "reset",
                extra: {
                  eventDate: "3000",
                  intervalSeconds: "3",
                  maxCount: "1",
                  startDate: "0",
                  impressionDate: "0",
                  count: "1",
                  type: "sponsored",
                  eventCount: "1",
                },
              },
              {
                object: "hit",
                extra: {
                  eventDate: "3000",
                  intervalSeconds: "3",
                  maxCount: "1",
                  startDate: "3000",
                  impressionDate: "3000",
                  count: "1",
                  type: "sponsored",
                  eventCount: "1",
                },
              },
            ],
          },
        },
        4: {
          results: [[]],
        },
        5: {
          results: [[]],
        },
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
        // 0s: 1 new impression; 1 impression total
        0: {
          results: [[EXPECTED_SPONSORED_URLBAR_RESULT], []],
          telemetry: {
            events: [
              // hit: interval_s: 1, max_count: 1
              {
                object: "hit",
                extra: {
                  eventDate: "0",
                  intervalSeconds: "1",
                  maxCount: "1",
                  startDate: "0",
                  impressionDate: "0",
                  count: "1",
                  type: "sponsored",
                  eventCount: "1",
                },
              },
            ],
          },
        },
        // 1s: 1 new impression; 2 impressions total
        1: {
          results: [[EXPECTED_SPONSORED_URLBAR_RESULT], []],
          telemetry: {
            events: [
              // reset: interval_s: 1, max_count: 1
              {
                object: "reset",
                extra: {
                  eventDate: "1000",
                  intervalSeconds: "1",
                  maxCount: "1",
                  startDate: "0",
                  impressionDate: "0",
                  count: "1",
                  type: "sponsored",
                  eventCount: "1",
                },
              },
              // hit: interval_s: 1, max_count: 1
              {
                object: "hit",
                extra: {
                  eventDate: "1000",
                  intervalSeconds: "1",
                  maxCount: "1",
                  startDate: "1000",
                  impressionDate: "1000",
                  count: "1",
                  type: "sponsored",
                  eventCount: "1",
                },
              },
            ],
          },
        },
        // 2s: 1 new impression; 3 impressions total
        2: {
          results: [[EXPECTED_SPONSORED_URLBAR_RESULT], []],
          telemetry: {
            events: [
              // reset: interval_s: 1, max_count: 1
              {
                object: "reset",
                extra: {
                  eventDate: "2000",
                  intervalSeconds: "1",
                  maxCount: "1",
                  startDate: "1000",
                  impressionDate: "1000",
                  count: "1",
                  type: "sponsored",
                  eventCount: "1",
                },
              },
              // hit: interval_s: 1, max_count: 1
              {
                object: "hit",
                extra: {
                  eventDate: "2000",
                  intervalSeconds: "1",
                  maxCount: "1",
                  startDate: "2000",
                  impressionDate: "2000",
                  count: "1",
                  type: "sponsored",
                  eventCount: "1",
                },
              },
              // hit: interval_s: 5, max_count: 3
              {
                object: "hit",
                extra: {
                  eventDate: "2000",
                  intervalSeconds: "5",
                  maxCount: "3",
                  startDate: "0",
                  impressionDate: "2000",
                  count: "3",
                  type: "sponsored",
                  eventCount: "1",
                },
              },
            ],
          },
        },
        // 3s: no new impressions; 3 impressions total
        3: {
          results: [[]],
          telemetry: {
            events: [
              // reset: interval_s: 1, max_count: 1
              {
                object: "reset",
                extra: {
                  eventDate: "3000",
                  intervalSeconds: "1",
                  maxCount: "1",
                  startDate: "2000",
                  impressionDate: "2000",
                  count: "1",
                  type: "sponsored",
                  eventCount: "1",
                },
              },
            ],
          },
        },
        // 4s: no new impressions; 3 impressions total
        4: {
          results: [[]],
          telemetry: {
            events: [
              // reset: interval_s: 1, max_count: 1
              {
                object: "reset",
                extra: {
                  eventDate: "4000",
                  intervalSeconds: "1",
                  maxCount: "1",
                  startDate: "3000",
                  impressionDate: "2000",
                  count: "0",
                  type: "sponsored",
                  eventCount: "1",
                },
              },
            ],
          },
        },
        // 5s: 1 new impression; 4 impressions total
        5: {
          results: [[EXPECTED_SPONSORED_URLBAR_RESULT], []],
          telemetry: {
            events: [
              // reset: interval_s: 1, max_count: 1
              {
                object: "reset",
                extra: {
                  eventDate: "5000",
                  intervalSeconds: "1",
                  maxCount: "1",
                  startDate: "4000",
                  impressionDate: "2000",
                  count: "0",
                  type: "sponsored",
                  eventCount: "1",
                },
              },
              // reset: interval_s: 5, max_count: 3
              {
                object: "reset",
                extra: {
                  eventDate: "5000",
                  intervalSeconds: "5",
                  maxCount: "3",
                  startDate: "0",
                  impressionDate: "2000",
                  count: "3",
                  type: "sponsored",
                  eventCount: "1",
                },
              },
              // hit: interval_s: 1, max_count: 1
              {
                object: "hit",
                extra: {
                  eventDate: "5000",
                  intervalSeconds: "1",
                  maxCount: "1",
                  startDate: "5000",
                  impressionDate: "5000",
                  count: "1",
                  type: "sponsored",
                  eventCount: "1",
                },
              },
            ],
          },
        },
        // 6s: 1 new impression; 5 impressions total
        6: {
          results: [[EXPECTED_SPONSORED_URLBAR_RESULT], []],
          telemetry: {
            events: [
              // reset: interval_s: 1, max_count: 1
              {
                object: "reset",
                extra: {
                  eventDate: "6000",
                  intervalSeconds: "1",
                  maxCount: "1",
                  startDate: "5000",
                  impressionDate: "5000",
                  count: "1",
                  type: "sponsored",
                  eventCount: "1",
                },
              },
              // hit: interval_s: 1, max_count: 1
              {
                object: "hit",
                extra: {
                  eventDate: "6000",
                  intervalSeconds: "1",
                  maxCount: "1",
                  startDate: "6000",
                  impressionDate: "6000",
                  count: "1",
                  type: "sponsored",
                  eventCount: "1",
                },
              },
              // hit: interval_s: 10, max_count: 5
              {
                object: "hit",
                extra: {
                  eventDate: "6000",
                  intervalSeconds: "10",
                  maxCount: "5",
                  startDate: "0",
                  impressionDate: "6000",
                  count: "5",
                  type: "sponsored",
                  eventCount: "1",
                },
              },
            ],
          },
        },
        // 7s: no new impressions; 5 impressions total
        7: {
          results: [[]],
          telemetry: {
            events: [
              // reset: interval_s: 1, max_count: 1
              {
                object: "reset",
                extra: {
                  eventDate: "7000",
                  intervalSeconds: "1",
                  maxCount: "1",
                  startDate: "6000",
                  impressionDate: "6000",
                  count: "1",
                  type: "sponsored",
                  eventCount: "1",
                },
              },
            ],
          },
        },
        // 8s: no new impressions; 5 impressions total
        8: {
          results: [[]],
          telemetry: {
            events: [
              // reset: interval_s: 1, max_count: 1
              {
                object: "reset",
                extra: {
                  eventDate: "8000",
                  intervalSeconds: "1",
                  maxCount: "1",
                  startDate: "7000",
                  impressionDate: "6000",
                  count: "0",
                  type: "sponsored",
                  eventCount: "1",
                },
              },
            ],
          },
        },
        // 9s: no new impressions; 5 impressions total
        9: {
          results: [[]],
          telemetry: {
            events: [
              // reset: interval_s: 1, max_count: 1
              {
                object: "reset",
                extra: {
                  eventDate: "9000",
                  intervalSeconds: "1",
                  maxCount: "1",
                  startDate: "8000",
                  impressionDate: "6000",
                  count: "0",
                  type: "sponsored",
                  eventCount: "1",
                },
              },
            ],
          },
        },
        // 10s: 1 new impression; 6 impressions total
        10: {
          results: [[EXPECTED_SPONSORED_URLBAR_RESULT], []],
          telemetry: {
            events: [
              // reset: interval_s: 1, max_count: 1
              {
                object: "reset",
                extra: {
                  eventDate: "10000",
                  intervalSeconds: "1",
                  maxCount: "1",
                  startDate: "9000",
                  impressionDate: "6000",
                  count: "0",
                  type: "sponsored",
                  eventCount: "1",
                },
              },
              // reset: interval_s: 5, max_count: 3
              {
                object: "reset",
                extra: {
                  eventDate: "10000",
                  intervalSeconds: "5",
                  maxCount: "3",
                  startDate: "5000",
                  impressionDate: "6000",
                  count: "2",
                  type: "sponsored",
                  eventCount: "1",
                },
              },
              // reset: interval_s: 10, max_count: 5
              {
                object: "reset",
                extra: {
                  eventDate: "10000",
                  intervalSeconds: "10",
                  maxCount: "5",
                  startDate: "0",
                  impressionDate: "6000",
                  count: "5",
                  type: "sponsored",
                  eventCount: "1",
                },
              },
              // hit: interval_s: 1, max_count: 1
              {
                object: "hit",
                extra: {
                  eventDate: "10000",
                  intervalSeconds: "1",
                  maxCount: "1",
                  startDate: "10000",
                  impressionDate: "10000",
                  count: "1",
                  type: "sponsored",
                  eventCount: "1",
                },
              },
            ],
          },
        },
        // 11s: 1 new impression; 7 impressions total
        11: {
          results: [[EXPECTED_SPONSORED_URLBAR_RESULT], []],
          telemetry: {
            events: [
              // reset: interval_s: 1, max_count: 1
              {
                object: "reset",
                extra: {
                  eventDate: "11000",
                  intervalSeconds: "1",
                  maxCount: "1",
                  startDate: "10000",
                  impressionDate: "10000",
                  count: "1",
                  type: "sponsored",
                  eventCount: "1",
                },
              },
              // hit: interval_s: 1, max_count: 1
              {
                object: "hit",
                extra: {
                  eventDate: "11000",
                  intervalSeconds: "1",
                  maxCount: "1",
                  startDate: "11000",
                  impressionDate: "11000",
                  count: "1",
                  type: "sponsored",
                  eventCount: "1",
                },
              },
            ],
          },
        },
        // 12s: 1 new impression; 8 impressions total
        12: {
          results: [[EXPECTED_SPONSORED_URLBAR_RESULT], []],
          telemetry: {
            events: [
              // reset: interval_s: 1, max_count: 1
              {
                object: "reset",
                extra: {
                  eventDate: "12000",
                  intervalSeconds: "1",
                  maxCount: "1",
                  startDate: "11000",
                  impressionDate: "11000",
                  count: "1",
                  type: "sponsored",
                  eventCount: "1",
                },
              },
              // hit: interval_s: 1, max_count: 1
              {
                object: "hit",
                extra: {
                  eventDate: "12000",
                  intervalSeconds: "1",
                  maxCount: "1",
                  startDate: "12000",
                  impressionDate: "12000",
                  count: "1",
                  type: "sponsored",
                  eventCount: "1",
                },
              },
              // hit: interval_s: 5, max_count: 3
              {
                object: "hit",
                extra: {
                  eventDate: "12000",
                  intervalSeconds: "5",
                  maxCount: "3",
                  startDate: "10000",
                  impressionDate: "12000",
                  count: "3",
                  type: "sponsored",
                  eventCount: "1",
                },
              },
            ],
          },
        },
        // 13s: no new impressions; 8 impressions total
        13: {
          results: [[]],
          telemetry: {
            events: [
              // reset: interval_s: 1, max_count: 1
              {
                object: "reset",
                extra: {
                  eventDate: "13000",
                  intervalSeconds: "1",
                  maxCount: "1",
                  startDate: "12000",
                  impressionDate: "12000",
                  count: "1",
                  type: "sponsored",
                  eventCount: "1",
                },
              },
            ],
          },
        },
        // 14s: no new impressions; 8 impressions total
        14: {
          results: [[]],
          telemetry: {
            events: [
              // reset: interval_s: 1, max_count: 1
              {
                object: "reset",
                extra: {
                  eventDate: "14000",
                  intervalSeconds: "1",
                  maxCount: "1",
                  startDate: "13000",
                  impressionDate: "12000",
                  count: "0",
                  type: "sponsored",
                  eventCount: "1",
                },
              },
            ],
          },
        },
        // 15s: 1 new impression; 9 impressions total
        15: {
          results: [[EXPECTED_SPONSORED_URLBAR_RESULT], []],
          telemetry: {
            events: [
              // reset: interval_s: 1, max_count: 1
              {
                object: "reset",
                extra: {
                  eventDate: "15000",
                  intervalSeconds: "1",
                  maxCount: "1",
                  startDate: "14000",
                  impressionDate: "12000",
                  count: "0",
                  type: "sponsored",
                  eventCount: "1",
                },
              },
              // reset: interval_s: 5, max_count: 3
              {
                object: "reset",
                extra: {
                  eventDate: "15000",
                  intervalSeconds: "5",
                  maxCount: "3",
                  startDate: "10000",
                  impressionDate: "12000",
                  count: "3",
                  type: "sponsored",
                  eventCount: "1",
                },
              },
              // hit: interval_s: 1, max_count: 1
              {
                object: "hit",
                extra: {
                  eventDate: "15000",
                  intervalSeconds: "1",
                  maxCount: "1",
                  startDate: "15000",
                  impressionDate: "15000",
                  count: "1",
                  type: "sponsored",
                  eventCount: "1",
                },
              },
            ],
          },
        },
        // 16s: 1 new impression; 10 impressions total
        16: {
          results: [[EXPECTED_SPONSORED_URLBAR_RESULT], []],
          telemetry: {
            events: [
              // reset: interval_s: 1, max_count: 1
              {
                object: "reset",
                extra: {
                  eventDate: "16000",
                  intervalSeconds: "1",
                  maxCount: "1",
                  startDate: "15000",
                  impressionDate: "15000",
                  count: "1",
                  type: "sponsored",
                  eventCount: "1",
                },
              },
              // hit: interval_s: 1, max_count: 1
              {
                object: "hit",
                extra: {
                  eventDate: "16000",
                  intervalSeconds: "1",
                  maxCount: "1",
                  startDate: "16000",
                  impressionDate: "16000",
                  count: "1",
                  type: "sponsored",
                  eventCount: "1",
                },
              },
              // hit: interval_s: 10, max_count: 5
              {
                object: "hit",
                extra: {
                  eventDate: "16000",
                  intervalSeconds: "10",
                  maxCount: "5",
                  startDate: "10000",
                  impressionDate: "16000",
                  count: "5",
                  type: "sponsored",
                  eventCount: "1",
                },
              },
            ],
          },
        },
        // 17s: no new impressions; 10 impressions total
        17: {
          results: [[]],
          telemetry: {
            events: [
              // reset: interval_s: 1, max_count: 1
              {
                object: "reset",
                extra: {
                  eventDate: "17000",
                  intervalSeconds: "1",
                  maxCount: "1",
                  startDate: "16000",
                  impressionDate: "16000",
                  count: "1",
                  type: "sponsored",
                  eventCount: "1",
                },
              },
            ],
          },
        },
        // 18s: no new impressions; 10 impressions total
        18: {
          results: [[]],
          telemetry: {
            events: [
              // reset: interval_s: 1, max_count: 1
              {
                object: "reset",
                extra: {
                  eventDate: "18000",
                  intervalSeconds: "1",
                  maxCount: "1",
                  startDate: "17000",
                  impressionDate: "16000",
                  count: "0",
                  type: "sponsored",
                  eventCount: "1",
                },
              },
            ],
          },
        },
        // 19s: no new impressions; 10 impressions total
        19: {
          results: [[]],
          telemetry: {
            events: [
              // reset: interval_s: 1, max_count: 1
              {
                object: "reset",
                extra: {
                  eventDate: "19000",
                  intervalSeconds: "1",
                  maxCount: "1",
                  startDate: "18000",
                  impressionDate: "16000",
                  count: "0",
                  type: "sponsored",
                  eventCount: "1",
                },
              },
            ],
          },
        },
        // 20s: 1 new impression; 11 impressions total
        20: {
          results: [[EXPECTED_SPONSORED_URLBAR_RESULT], []],
          telemetry: {
            events: [
              // reset: interval_s: 1, max_count: 1
              {
                object: "reset",
                extra: {
                  eventDate: "20000",
                  intervalSeconds: "1",
                  maxCount: "1",
                  startDate: "19000",
                  impressionDate: "16000",
                  count: "0",
                  type: "sponsored",
                  eventCount: "1",
                },
              },
              // reset: interval_s: 5, max_count: 3
              {
                object: "reset",
                extra: {
                  eventDate: "20000",
                  intervalSeconds: "5",
                  maxCount: "3",
                  startDate: "15000",
                  impressionDate: "16000",
                  count: "2",
                  type: "sponsored",
                  eventCount: "1",
                },
              },
              // reset: interval_s: 10, max_count: 5
              {
                object: "reset",
                extra: {
                  eventDate: "20000",
                  intervalSeconds: "10",
                  maxCount: "5",
                  startDate: "10000",
                  impressionDate: "16000",
                  count: "5",
                  type: "sponsored",
                  eventCount: "1",
                },
              },
              // hit: interval_s: 1, max_count: 1
              {
                object: "hit",
                extra: {
                  eventDate: "20000",
                  intervalSeconds: "1",
                  maxCount: "1",
                  startDate: "20000",
                  impressionDate: "20000",
                  count: "1",
                  type: "sponsored",
                  eventCount: "1",
                },
              },
            ],
          },
        },
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
      await doTimedSearches("sponsored", {
        0: {
          results: [
            [EXPECTED_SPONSORED_URLBAR_RESULT],
            [EXPECTED_SPONSORED_URLBAR_RESULT],
            [EXPECTED_SPONSORED_URLBAR_RESULT],
            [],
          ],
          telemetry: {
            events: [
              {
                object: "hit",
                extra: {
                  eventDate: "0",
                  intervalSeconds: "Infinity",
                  maxCount: "3",
                  startDate: "0",
                  impressionDate: "0",
                  count: "3",
                  type: "sponsored",
                  eventCount: "1",
                },
              },
            ],
          },
        },
        1: {
          results: [[]],
        },
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
        // 0s: 1 new impression; 1 impression total
        0: {
          results: [[EXPECTED_SPONSORED_URLBAR_RESULT], []],
          telemetry: {
            events: [
              // hit: interval_s: 1, max_count: 1
              {
                object: "hit",
                extra: {
                  eventDate: "0",
                  intervalSeconds: "1",
                  maxCount: "1",
                  startDate: "0",
                  impressionDate: "0",
                  count: "1",
                  type: "sponsored",
                  eventCount: "1",
                },
              },
            ],
          },
        },
        // 1s: 1 new impression; 2 impressions total
        1: {
          results: [[EXPECTED_SPONSORED_URLBAR_RESULT], []],
          telemetry: {
            events: [
              // reset: interval_s: 1, max_count: 1
              {
                object: "reset",
                extra: {
                  eventDate: "1000",
                  intervalSeconds: "1",
                  maxCount: "1",
                  startDate: "0",
                  impressionDate: "0",
                  count: "1",
                  type: "sponsored",
                  eventCount: "1",
                },
              },
              // hit: interval_s: 1, max_count: 1
              {
                object: "hit",
                extra: {
                  eventDate: "1000",
                  intervalSeconds: "1",
                  maxCount: "1",
                  startDate: "1000",
                  impressionDate: "1000",
                  count: "1",
                  type: "sponsored",
                  eventCount: "1",
                },
              },
            ],
          },
        },
        // 2s: 1 new impression; 3 impressions total
        2: {
          results: [[EXPECTED_SPONSORED_URLBAR_RESULT], []],
          telemetry: {
            events: [
              // reset: interval_s: 1, max_count: 1
              {
                object: "reset",
                extra: {
                  eventDate: "2000",
                  intervalSeconds: "1",
                  maxCount: "1",
                  startDate: "1000",
                  impressionDate: "1000",
                  count: "1",
                  type: "sponsored",
                  eventCount: "1",
                },
              },
              // hit: interval_s: 1, max_count: 1
              {
                object: "hit",
                extra: {
                  eventDate: "2000",
                  intervalSeconds: "1",
                  maxCount: "1",
                  startDate: "2000",
                  impressionDate: "2000",
                  count: "1",
                  type: "sponsored",
                  eventCount: "1",
                },
              },
              // hit: interval_s: Infinity, max_count: 3
              {
                object: "hit",
                extra: {
                  eventDate: "2000",
                  intervalSeconds: "Infinity",
                  maxCount: "3",
                  startDate: "0",
                  impressionDate: "2000",
                  count: "3",
                  type: "sponsored",
                  eventCount: "1",
                },
              },
            ],
          },
        },
        3: {
          results: [[]],
          telemetry: {
            events: [
              // reset: interval_s: 1, max_count: 1
              {
                object: "reset",
                extra: {
                  eventDate: "3000",
                  intervalSeconds: "1",
                  maxCount: "1",
                  startDate: "2000",
                  impressionDate: "2000",
                  count: "1",
                  type: "sponsored",
                  eventCount: "1",
                },
              },
            ],
          },
        },
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
        // 0s: 1 new impression; 1 impression total
        0: {
          results: [[EXPECTED_SPONSORED_URLBAR_RESULT], []],
          telemetry: {
            events: [
              // hit: interval_s: 1, max_count: 1
              {
                object: "hit",
                extra: {
                  eventDate: "0",
                  intervalSeconds: "1",
                  maxCount: "1",
                  startDate: "0",
                  impressionDate: "0",
                  count: "1",
                  type: "sponsored",
                  eventCount: "1",
                },
              },
            ],
          },
        },
        // 1s: 1 new impression; 2 impressions total
        1: {
          results: [[EXPECTED_SPONSORED_URLBAR_RESULT], []],
          telemetry: {
            events: [
              // reset: interval_s: 1, max_count: 1
              {
                object: "reset",
                extra: {
                  eventDate: "1000",
                  intervalSeconds: "1",
                  maxCount: "1",
                  startDate: "0",
                  impressionDate: "0",
                  count: "1",
                  type: "sponsored",
                  eventCount: "1",
                },
              },
              // hit: interval_s: 1, max_count: 1
              {
                object: "hit",
                extra: {
                  eventDate: "1000",
                  intervalSeconds: "1",
                  maxCount: "1",
                  startDate: "1000",
                  impressionDate: "1000",
                  count: "1",
                  type: "sponsored",
                  eventCount: "1",
                },
              },
            ],
          },
        },
        // 2s: 1 new impression; 3 impressions total
        2: {
          results: [[EXPECTED_SPONSORED_URLBAR_RESULT], []],
          telemetry: {
            events: [
              // reset: interval_s: 1, max_count: 1
              {
                object: "reset",
                extra: {
                  eventDate: "2000",
                  intervalSeconds: "1",
                  maxCount: "1",
                  startDate: "1000",
                  impressionDate: "1000",
                  count: "1",
                  type: "sponsored",
                  eventCount: "1",
                },
              },
              // hit: interval_s: 1, max_count: 1
              {
                object: "hit",
                extra: {
                  eventDate: "2000",
                  intervalSeconds: "1",
                  maxCount: "1",
                  startDate: "2000",
                  impressionDate: "2000",
                  count: "1",
                  type: "sponsored",
                  eventCount: "1",
                },
              },
              // hit: interval_s: 5, max_count: 3
              {
                object: "hit",
                extra: {
                  eventDate: "2000",
                  intervalSeconds: "5",
                  maxCount: "3",
                  startDate: "0",
                  impressionDate: "2000",
                  count: "3",
                  type: "sponsored",
                  eventCount: "1",
                },
              },
            ],
          },
        },
        // 3s: no new impressions; 3 impressions total
        3: {
          results: [[]],
          telemetry: {
            events: [
              // reset: interval_s: 1, max_count: 1
              {
                object: "reset",
                extra: {
                  eventDate: "3000",
                  intervalSeconds: "1",
                  maxCount: "1",
                  startDate: "2000",
                  impressionDate: "2000",
                  count: "1",
                  type: "sponsored",
                  eventCount: "1",
                },
              },
            ],
          },
        },
        // 4s: no new impressions; 3 impressions total
        4: {
          results: [[]],
          telemetry: {
            events: [
              // reset: interval_s: 1, max_count: 1
              {
                object: "reset",
                extra: {
                  eventDate: "4000",
                  intervalSeconds: "1",
                  maxCount: "1",
                  startDate: "3000",
                  impressionDate: "2000",
                  count: "0",
                  type: "sponsored",
                  eventCount: "1",
                },
              },
            ],
          },
        },
        // 5s: 1 new impression; 4 impressions total
        5: {
          results: [[EXPECTED_SPONSORED_URLBAR_RESULT], []],
          telemetry: {
            events: [
              // reset: interval_s: 1, max_count: 1
              {
                object: "reset",
                extra: {
                  eventDate: "5000",
                  intervalSeconds: "1",
                  maxCount: "1",
                  startDate: "4000",
                  impressionDate: "2000",
                  count: "0",
                  type: "sponsored",
                  eventCount: "1",
                },
              },
              // reset: interval_s: 5, max_count: 3
              {
                object: "reset",
                extra: {
                  eventDate: "5000",
                  intervalSeconds: "5",
                  maxCount: "3",
                  startDate: "0",
                  impressionDate: "2000",
                  count: "3",
                  type: "sponsored",
                  eventCount: "1",
                },
              },
              // hit: interval_s: 1, max_count: 1
              {
                object: "hit",
                extra: {
                  eventDate: "5000",
                  intervalSeconds: "1",
                  maxCount: "1",
                  startDate: "5000",
                  impressionDate: "5000",
                  count: "1",
                  type: "sponsored",
                  eventCount: "1",
                },
              },
              // hit: interval_s: Infinity, max_count: 4
              {
                object: "hit",
                extra: {
                  eventDate: "5000",
                  intervalSeconds: "Infinity",
                  maxCount: "4",
                  startDate: "0",
                  impressionDate: "5000",
                  count: "4",
                  type: "sponsored",
                  eventCount: "1",
                },
              },
            ],
          },
        },
        // 6s: no new impressions; 4 impressions total
        6: {
          results: [[]],
          telemetry: {
            events: [
              // reset: interval_s: 1, max_count: 1
              {
                object: "reset",
                extra: {
                  eventDate: "6000",
                  intervalSeconds: "1",
                  maxCount: "1",
                  startDate: "5000",
                  impressionDate: "5000",
                  count: "1",
                  type: "sponsored",
                  eventCount: "1",
                },
              },
            ],
          },
        },
        // 7s: no new impressions; 4 impressions total
        7: {
          results: [[]],
          telemetry: {
            events: [
              // reset: interval_s: 1, max_count: 1
              {
                object: "reset",
                extra: {
                  eventDate: "7000",
                  intervalSeconds: "1",
                  maxCount: "1",
                  startDate: "6000",
                  impressionDate: "5000",
                  count: "0",
                  type: "sponsored",
                  eventCount: "1",
                },
              },
            ],
          },
        },
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
        // 0s: 1 new impression; 1 impression total
        0: {
          results: [[EXPECTED_NONSPONSORED_URLBAR_RESULT], []],
          telemetry: {
            events: [
              // hit: interval_s: 1, max_count: 1
              {
                object: "hit",
                extra: {
                  eventDate: "0",
                  intervalSeconds: "1",
                  maxCount: "1",
                  startDate: "0",
                  impressionDate: "0",
                  count: "1",
                  type: "nonsponsored",
                  eventCount: "1",
                },
              },
            ],
          },
        },
        // 1s: 1 new impression; 2 impressions total
        1: {
          results: [[EXPECTED_NONSPONSORED_URLBAR_RESULT], []],
          telemetry: {
            events: [
              // reset: interval_s: 1, max_count: 1
              {
                object: "reset",
                extra: {
                  eventDate: "1000",
                  intervalSeconds: "1",
                  maxCount: "1",
                  startDate: "0",
                  impressionDate: "0",
                  count: "1",
                  type: "nonsponsored",
                  eventCount: "1",
                },
              },
              // hit: interval_s: 1, max_count: 1
              {
                object: "hit",
                extra: {
                  eventDate: "1000",
                  intervalSeconds: "1",
                  maxCount: "1",
                  startDate: "1000",
                  impressionDate: "1000",
                  count: "1",
                  type: "nonsponsored",
                  eventCount: "1",
                },
              },
            ],
          },
        },
        // 2s: 1 new impression; 3 impressions total
        2: {
          results: [[EXPECTED_NONSPONSORED_URLBAR_RESULT], []],
          telemetry: {
            events: [
              // reset: interval_s: 1, max_count: 1
              {
                object: "reset",
                extra: {
                  eventDate: "2000",
                  intervalSeconds: "1",
                  maxCount: "1",
                  startDate: "1000",
                  impressionDate: "1000",
                  count: "1",
                  type: "nonsponsored",
                  eventCount: "1",
                },
              },
              // hit: interval_s: 1, max_count: 1
              {
                object: "hit",
                extra: {
                  eventDate: "2000",
                  intervalSeconds: "1",
                  maxCount: "1",
                  startDate: "2000",
                  impressionDate: "2000",
                  count: "1",
                  type: "nonsponsored",
                  eventCount: "1",
                },
              },
              // hit: interval_s: 5, max_count: 3
              {
                object: "hit",
                extra: {
                  eventDate: "2000",
                  intervalSeconds: "5",
                  maxCount: "3",
                  startDate: "0",
                  impressionDate: "2000",
                  count: "3",
                  type: "nonsponsored",
                  eventCount: "1",
                },
              },
            ],
          },
        },
        // 3s: no new impressions; 3 impressions total
        3: {
          results: [[]],
          telemetry: {
            events: [
              // reset: interval_s: 1, max_count: 1
              {
                object: "reset",
                extra: {
                  eventDate: "3000",
                  intervalSeconds: "1",
                  maxCount: "1",
                  startDate: "2000",
                  impressionDate: "2000",
                  count: "1",
                  type: "nonsponsored",
                  eventCount: "1",
                },
              },
            ],
          },
        },
        // 4s: no new impressions; 3 impressions total
        4: {
          results: [[]],
          telemetry: {
            events: [
              // reset: interval_s: 1, max_count: 1
              {
                object: "reset",
                extra: {
                  eventDate: "4000",
                  intervalSeconds: "1",
                  maxCount: "1",
                  startDate: "3000",
                  impressionDate: "2000",
                  count: "0",
                  type: "nonsponsored",
                  eventCount: "1",
                },
              },
            ],
          },
        },
        // 5s: 1 new impression; 4 impressions total
        5: {
          results: [[EXPECTED_NONSPONSORED_URLBAR_RESULT], []],
          telemetry: {
            events: [
              // reset: interval_s: 1, max_count: 1
              {
                object: "reset",
                extra: {
                  eventDate: "5000",
                  intervalSeconds: "1",
                  maxCount: "1",
                  startDate: "4000",
                  impressionDate: "2000",
                  count: "0",
                  type: "nonsponsored",
                  eventCount: "1",
                },
              },
              // reset: interval_s: 5, max_count: 3
              {
                object: "reset",
                extra: {
                  eventDate: "5000",
                  intervalSeconds: "5",
                  maxCount: "3",
                  startDate: "0",
                  impressionDate: "2000",
                  count: "3",
                  type: "nonsponsored",
                  eventCount: "1",
                },
              },
              // hit: interval_s: 1, max_count: 1
              {
                object: "hit",
                extra: {
                  eventDate: "5000",
                  intervalSeconds: "1",
                  maxCount: "1",
                  startDate: "5000",
                  impressionDate: "5000",
                  count: "1",
                  type: "nonsponsored",
                  eventCount: "1",
                },
              },
              // hit: interval_s: Infinity, max_count: 4
              {
                object: "hit",
                extra: {
                  eventDate: "5000",
                  intervalSeconds: "Infinity",
                  maxCount: "4",
                  startDate: "0",
                  impressionDate: "5000",
                  count: "4",
                  type: "nonsponsored",
                  eventCount: "1",
                },
              },
            ],
          },
        },
        // 6s: no new impressions; 4 impressions total
        6: {
          results: [[]],
          telemetry: {
            events: [
              // reset: interval_s: 1, max_count: 1
              {
                object: "reset",
                extra: {
                  eventDate: "6000",
                  intervalSeconds: "1",
                  maxCount: "1",
                  startDate: "5000",
                  impressionDate: "5000",
                  count: "1",
                  type: "nonsponsored",
                  eventCount: "1",
                },
              },
            ],
          },
        },
        // 7s: no new impressions; 4 impressions total
        7: {
          results: [[]],
          telemetry: {
            events: [
              // reset: interval_s: 1, max_count: 1
              {
                object: "reset",
                extra: {
                  eventDate: "7000",
                  intervalSeconds: "1",
                  maxCount: "1",
                  startDate: "6000",
                  impressionDate: "5000",
                  count: "0",
                  type: "nonsponsored",
                  eventCount: "1",
                },
              },
            ],
          },
        },
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
      // 1st searches
      await checkSearch({
        name: "sponsored 1",
        searchString: "sponsored",
        expectedResults: [EXPECTED_SPONSORED_URLBAR_RESULT],
      });
      await checkSearch({
        name: "nonsponsored 1",
        searchString: "nonsponsored",
        expectedResults: [EXPECTED_NONSPONSORED_URLBAR_RESULT],
      });
      await checkTelemetryEvents([]);

      // 2nd searches
      await checkSearch({
        name: "sponsored 2",
        searchString: "sponsored",
        expectedResults: [EXPECTED_SPONSORED_URLBAR_RESULT],
      });
      await checkSearch({
        name: "nonsponsored 2",
        searchString: "nonsponsored",
        expectedResults: [EXPECTED_NONSPONSORED_URLBAR_RESULT],
      });
      await checkTelemetryEvents([
        {
          object: "hit",
          extra: {
            eventDate: "0",
            intervalSeconds: "Infinity",
            maxCount: "2",
            startDate: "0",
            impressionDate: "0",
            count: "2",
            type: "sponsored",
            eventCount: "1",
          },
        },
      ]);

      // 3rd searches
      await checkSearch({
        name: "sponsored 3",
        searchString: "sponsored",
        expectedResults: [],
      });
      await checkSearch({
        name: "nonsponsored 3",
        searchString: "nonsponsored",
        expectedResults: [EXPECTED_NONSPONSORED_URLBAR_RESULT],
      });
      await checkTelemetryEvents([
        {
          object: "hit",
          extra: {
            eventDate: "0",
            intervalSeconds: "Infinity",
            maxCount: "3",
            startDate: "0",
            impressionDate: "0",
            count: "3",
            type: "nonsponsored",
            eventCount: "1",
          },
        },
      ]);

      // 4th searches
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
      await checkTelemetryEvents([]);
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
          expectedResults: [EXPECTED_SPONSORED_URLBAR_RESULT],
        });
        await checkSearch({
          name: "nonsponsored " + i,
          searchString: "nonsponsored",
          expectedResults: [EXPECTED_NONSPONSORED_URLBAR_RESULT],
        });
      }
      await checkTelemetryEvents([]);
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
          expectedResults: [EXPECTED_SPONSORED_URLBAR_RESULT],
        });
        await checkSearch({
          name: "nonsponsored " + i,
          searchString: "nonsponsored",
          expectedResults: [EXPECTED_NONSPONSORED_URLBAR_RESULT],
        });
      }
      await checkTelemetryEvents([
        {
          object: "hit",
          extra: {
            eventDate: "0",
            intervalSeconds: "Infinity",
            maxCount: "3",
            startDate: "0",
            impressionDate: "0",
            count: "3",
            type: "nonsponsored",
            eventCount: "1",
          },
        },
      ]);

      await checkSearch({
        name: "sponsored additional",
        searchString: "sponsored",
        expectedResults: [EXPECTED_SPONSORED_URLBAR_RESULT],
      });
      await checkSearch({
        name: "nonsponsored additional",
        searchString: "nonsponsored",
        expectedResults: [],
      });
      await checkTelemetryEvents([]);
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
          expectedResults: [EXPECTED_SPONSORED_URLBAR_RESULT],
        });
        await checkSearch({
          name: "nonsponsored " + i,
          searchString: "nonsponsored",
          expectedResults: [EXPECTED_NONSPONSORED_URLBAR_RESULT],
        });
      }
      await checkTelemetryEvents([
        {
          object: "hit",
          extra: {
            eventDate: "0",
            intervalSeconds: "Infinity",
            maxCount: "3",
            startDate: "0",
            impressionDate: "0",
            count: "3",
            type: "sponsored",
            eventCount: "1",
          },
        },
      ]);

      await checkSearch({
        name: "sponsored additional",
        searchString: "sponsored",
        expectedResults: [],
      });
      await checkSearch({
        name: "nonsponsored additional",
        searchString: "nonsponsored",
        expectedResults: [EXPECTED_NONSPONSORED_URLBAR_RESULT],
      });
      await checkTelemetryEvents([]);
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
              expectedResults: [EXPECTED_SPONSORED_URLBAR_RESULT],
            });
          }
          await checkSearch({
            name: "0s additional",
            searchString: "sponsored",
            expectedResults: [],
          });
          await checkTelemetryEvents([
            {
              object: "hit",
              extra: {
                eventDate: "0",
                intervalSeconds: "3",
                maxCount: "3",
                startDate: "0",
                impressionDate: "0",
                count: "3",
                type: "sponsored",
                eventCount: "1",
              },
            },
          ]);
          await QuickSuggestTestUtils.setConfig({
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
          await checkTelemetryEvents([]);
        },
        3: async () => {
          await checkSearch({
            name: "3s 0",
            searchString: "sponsored",
            expectedResults: [EXPECTED_SPONSORED_URLBAR_RESULT],
          });
          await checkSearch({
            name: "3s additional",
            searchString: "sponsored",
            expectedResults: [],
          });
          await checkTelemetryEvents([
            {
              object: "reset",
              extra: {
                eventDate: "3000",
                intervalSeconds: "3",
                maxCount: "1",
                startDate: "0",
                impressionDate: "0",
                count: "3",
                type: "sponsored",
                eventCount: "1",
              },
            },
            {
              object: "hit",
              extra: {
                eventDate: "3000",
                intervalSeconds: "3",
                maxCount: "1",
                startDate: "3000",
                impressionDate: "3000",
                count: "1",
                type: "sponsored",
                eventCount: "1",
              },
            },
          ]);
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
              expectedResults: [EXPECTED_SPONSORED_URLBAR_RESULT],
            });
          }
          await checkTelemetryEvents([]);
          await QuickSuggestTestUtils.setConfig({
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
            expectedResults: [EXPECTED_SPONSORED_URLBAR_RESULT],
          });
          await checkSearch({
            name: "3s additional",
            searchString: "sponsored",
            expectedResults: [],
          });
          await checkTelemetryEvents([
            {
              object: "reset",
              extra: {
                eventDate: "3000",
                intervalSeconds: "3",
                maxCount: "1",
                startDate: "0",
                impressionDate: "0",
                count: "2",
                type: "sponsored",
                eventCount: "1",
              },
            },
            {
              object: "hit",
              extra: {
                eventDate: "3000",
                intervalSeconds: "3",
                maxCount: "1",
                startDate: "3000",
                impressionDate: "3000",
                count: "1",
                type: "sponsored",
                eventCount: "1",
              },
            },
          ]);
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
              expectedResults: [EXPECTED_SPONSORED_URLBAR_RESULT],
            });
          }
          await checkSearch({
            name: "0s additional",
            searchString: "sponsored",
            expectedResults: [],
          });
          await checkTelemetryEvents([
            {
              object: "hit",
              extra: {
                eventDate: "0",
                intervalSeconds: "3",
                maxCount: "3",
                startDate: "0",
                impressionDate: "0",
                count: "3",
                type: "sponsored",
                eventCount: "1",
              },
            },
          ]);
          await QuickSuggestTestUtils.setConfig({
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
              expectedResults: [EXPECTED_SPONSORED_URLBAR_RESULT],
            });
          }
          await checkSearch({
            name: "1s additional",
            searchString: "sponsored",
            expectedResults: [],
          });
          await checkTelemetryEvents([
            {
              object: "hit",
              extra: {
                eventDate: "1000",
                intervalSeconds: "3",
                maxCount: "5",
                startDate: "0",
                impressionDate: "1000",
                count: "5",
                type: "sponsored",
                eventCount: "1",
              },
            },
          ]);
        },
        3: async () => {
          for (let i = 0; i < 5; i++) {
            await checkSearch({
              name: "3s " + i,
              searchString: "sponsored",
              expectedResults: [EXPECTED_SPONSORED_URLBAR_RESULT],
            });
          }
          await checkSearch({
            name: "3s additional",
            searchString: "sponsored",
            expectedResults: [],
          });
          await checkTelemetryEvents([
            {
              object: "reset",
              extra: {
                eventDate: "3000",
                intervalSeconds: "3",
                maxCount: "5",
                startDate: "0",
                impressionDate: "1000",
                count: "5",
                type: "sponsored",
                eventCount: "1",
              },
            },
            {
              object: "hit",
              extra: {
                eventDate: "3000",
                intervalSeconds: "3",
                maxCount: "5",
                startDate: "3000",
                impressionDate: "3000",
                count: "5",
                type: "sponsored",
                eventCount: "1",
              },
            },
          ]);
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
              expectedResults: [EXPECTED_SPONSORED_URLBAR_RESULT],
            });
          }
          await checkTelemetryEvents([
            {
              object: "hit",
              extra: {
                eventDate: "0",
                intervalSeconds: "3",
                maxCount: "3",
                startDate: "0",
                impressionDate: "0",
                count: "3",
                type: "sponsored",
                eventCount: "1",
              },
            },
          ]);
          await QuickSuggestTestUtils.setConfig({
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
          await checkTelemetryEvents([]);
        },
        4: async () => {
          await checkSearch({
            name: "4s",
            searchString: "sponsored",
            expectedResults: [],
          });
          await checkTelemetryEvents([]);
        },
        5: async () => {
          for (let i = 0; i < 2; i++) {
            await checkSearch({
              name: "5s " + i,
              searchString: "sponsored",
              expectedResults: [EXPECTED_SPONSORED_URLBAR_RESULT],
            });
          }
          await checkSearch({
            name: "5s additional",
            searchString: "sponsored",
            expectedResults: [],
          });
          await checkTelemetryEvents([
            {
              object: "reset",
              extra: {
                eventDate: "5000",
                intervalSeconds: "5",
                maxCount: "3",
                startDate: "0",
                impressionDate: "0",
                count: "3",
                type: "sponsored",
                eventCount: "1",
              },
            },
            {
              object: "hit",
              extra: {
                eventDate: "5000",
                intervalSeconds: "10",
                maxCount: "5",
                startDate: "0",
                impressionDate: "5000",
                count: "5",
                type: "sponsored",
                eventCount: "1",
              },
            },
          ]);
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
              expectedResults: [EXPECTED_SPONSORED_URLBAR_RESULT],
            });
          }
          await checkTelemetryEvents([
            {
              object: "hit",
              extra: {
                eventDate: "0",
                intervalSeconds: "2",
                maxCount: "2",
                startDate: "0",
                impressionDate: "0",
                count: "2",
                type: "sponsored",
                eventCount: "1",
              },
            },
          ]);
        },
        2: async () => {
          for (let i = 0; i < 2; i++) {
            await checkSearch({
              name: "2s " + i,
              searchString: "sponsored",
              expectedResults: [EXPECTED_SPONSORED_URLBAR_RESULT],
            });
          }
          await checkTelemetryEvents([
            {
              object: "reset",
              extra: {
                eventDate: "2000",
                intervalSeconds: "2",
                maxCount: "2",
                startDate: "0",
                impressionDate: "0",
                count: "2",
                type: "sponsored",
                eventCount: "1",
              },
            },
            {
              object: "hit",
              extra: {
                eventDate: "2000",
                intervalSeconds: "2",
                maxCount: "2",
                startDate: "2000",
                impressionDate: "2000",
                count: "2",
                type: "sponsored",
                eventCount: "1",
              },
            },
            {
              object: "hit",
              extra: {
                eventDate: "2000",
                intervalSeconds: "4",
                maxCount: "4",
                startDate: "0",
                impressionDate: "2000",
                count: "4",
                type: "sponsored",
                eventCount: "1",
              },
            },
          ]);
          await QuickSuggestTestUtils.setConfig({
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
            expectedResults: [EXPECTED_SPONSORED_URLBAR_RESULT],
          });
          await checkSearch({
            name: "4s 1",
            searchString: "sponsored",
            expectedResults: [],
          });
          await checkTelemetryEvents([
            {
              object: "hit",
              extra: {
                eventDate: "4000",
                intervalSeconds: "6",
                maxCount: "5",
                startDate: "0",
                impressionDate: "4000",
                count: "5",
                type: "sponsored",
                eventCount: "1",
              },
            },
          ]);
        },
        5: async () => {
          await checkSearch({
            name: "5s",
            searchString: "sponsored",
            expectedResults: [],
          });
          await checkTelemetryEvents([]);
        },
        6: async () => {
          for (let i = 0; i < 5; i++) {
            await checkSearch({
              name: "6s " + i,
              searchString: "sponsored",
              expectedResults: [EXPECTED_SPONSORED_URLBAR_RESULT],
            });
          }
          await checkSearch({
            name: "6s additional",
            searchString: "sponsored",
            expectedResults: [],
          });
          await checkTelemetryEvents([
            {
              object: "reset",
              extra: {
                eventDate: "6000",
                intervalSeconds: "6",
                maxCount: "5",
                startDate: "0",
                impressionDate: "4000",
                count: "5",
                type: "sponsored",
                eventCount: "1",
              },
            },
            {
              object: "hit",
              extra: {
                eventDate: "6000",
                intervalSeconds: "6",
                maxCount: "5",
                startDate: "6000",
                impressionDate: "6000",
                count: "5",
                type: "sponsored",
                eventCount: "1",
              },
            },
          ]);
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
              expectedResults: [EXPECTED_SPONSORED_URLBAR_RESULT],
            });
          }
          await checkTelemetryEvents([
            {
              object: "hit",
              extra: {
                eventDate: "0",
                intervalSeconds: "5",
                maxCount: "3",
                startDate: "0",
                impressionDate: "0",
                count: "3",
                type: "sponsored",
                eventCount: "1",
              },
            },
          ]);
          await QuickSuggestTestUtils.setConfig({
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
              expectedResults: [EXPECTED_SPONSORED_URLBAR_RESULT],
            });
          }
          await checkSearch({
            name: "3s additional",
            searchString: "sponsored",
            expectedResults: [],
          });
          await checkTelemetryEvents([
            {
              object: "hit",
              extra: {
                eventDate: "1000",
                intervalSeconds: "3",
                maxCount: "3",
                startDate: "0",
                impressionDate: "1000",
                count: "3",
                type: "sponsored",
                eventCount: "1",
              },
            },
          ]);
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
              expectedResults: [EXPECTED_SPONSORED_URLBAR_RESULT],
            });
          }
          await checkTelemetryEvents([
            {
              object: "hit",
              extra: {
                eventDate: "0",
                intervalSeconds: "3",
                maxCount: "3",
                startDate: "0",
                impressionDate: "0",
                count: "3",
                type: "sponsored",
                eventCount: "1",
              },
            },
          ]);
          await QuickSuggestTestUtils.setConfig({
            impression_caps: {
              sponsored: {
                lifetime: 3,
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
          await checkTelemetryEvents([]);
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
              expectedResults: [EXPECTED_SPONSORED_URLBAR_RESULT],
            });
          }
          await checkSearch({
            name: "0s additional",
            searchString: "sponsored",
            expectedResults: [],
          });
          await checkTelemetryEvents([
            {
              object: "hit",
              extra: {
                eventDate: "0",
                intervalSeconds: "Infinity",
                maxCount: "3",
                startDate: "0",
                impressionDate: "0",
                count: "3",
                type: "sponsored",
                eventCount: "1",
              },
            },
          ]);
          await QuickSuggestTestUtils.setConfig({
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
              expectedResults: [EXPECTED_SPONSORED_URLBAR_RESULT],
            });
          }
          await checkSearch({
            name: "1s additional",
            searchString: "sponsored",
            expectedResults: [],
          });
          await checkTelemetryEvents([
            {
              object: "hit",
              extra: {
                eventDate: "1000",
                intervalSeconds: "Infinity",
                maxCount: "5",
                startDate: "0",
                impressionDate: "1000",
                count: "5",
                type: "sponsored",
                eventCount: "1",
              },
            },
          ]);
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
              expectedResults: [EXPECTED_SPONSORED_URLBAR_RESULT],
            });
          }
          await checkSearch({
            name: "0s additional",
            searchString: "sponsored",
            expectedResults: [],
          });
          await checkTelemetryEvents([
            {
              object: "hit",
              extra: {
                eventDate: "0",
                intervalSeconds: "Infinity",
                maxCount: "3",
                startDate: "0",
                impressionDate: "0",
                count: "3",
                type: "sponsored",
                eventCount: "1",
              },
            },
          ]);
          await QuickSuggestTestUtils.setConfig({
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
          await checkTelemetryEvents([]);
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
          expectedResults: [EXPECTED_SPONSORED_URLBAR_RESULT],
        });
      }

      let json = UrlbarPrefs.get("quicksuggest.impressionCaps.stats");
      Assert.ok(json, "JSON is non-empty");
      Assert.deepEqual(
        JSON.parse(json),
        {
          sponsored: [
            {
              intervalSeconds: 3,
              count: 2,
              maxCount: 2,
              startDateMs: 0,
              impressionDateMs: 0,
            },
            {
              intervalSeconds: 5,
              count: 2,
              maxCount: 4,
              startDateMs: 0,
              impressionDateMs: 0,
            },
            {
              intervalSeconds: null,
              count: 2,
              maxCount: 5,
              startDateMs: 0,
              impressionDateMs: 0,
            },
          ],
        },
        "JSON is correct"
      );

      QuickSuggest.impressionCaps._test_reloadStats();
      Assert.deepEqual(
        QuickSuggest.impressionCaps._test_stats,
        {
          sponsored: [
            {
              intervalSeconds: 3,
              count: 2,
              maxCount: 2,
              startDateMs: 0,
              impressionDateMs: 0,
            },
            {
              intervalSeconds: 5,
              count: 2,
              maxCount: 4,
              startDateMs: 0,
              impressionDateMs: 0,
            },
            {
              intervalSeconds: Infinity,
              count: 2,
              maxCount: 5,
              startDateMs: 0,
              impressionDateMs: 0,
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
          {
            intervalSeconds: 3,
            count: 0,
            maxCount: 3,
            startDateMs: 0,
            impressionDateMs: 0,
          },
          {
            intervalSeconds: Infinity,
            count: 0,
            maxCount: 5,
            startDateMs: 0,
            impressionDateMs: 0,
          },
        ],
      };

      UrlbarPrefs.set("quicksuggest.impressionCaps.stats", "bogus");
      Assert.deepEqual(
        QuickSuggest.impressionCaps._test_stats,
        expectedStats,
        "Expected stats for 'bogus'"
      );

      UrlbarPrefs.set("quicksuggest.impressionCaps.stats", JSON.stringify({}));
      Assert.deepEqual(
        QuickSuggest.impressionCaps._test_stats,
        expectedStats,
        "Expected stats for {}"
      );

      UrlbarPrefs.set(
        "quicksuggest.impressionCaps.stats",
        JSON.stringify({ sponsored: "bogus" })
      );
      Assert.deepEqual(
        QuickSuggest.impressionCaps._test_stats,
        expectedStats,
        "Expected stats for { sponsored: 'bogus' }"
      );

      UrlbarPrefs.set(
        "quicksuggest.impressionCaps.stats",
        JSON.stringify({
          sponsored: [
            {
              intervalSeconds: 3,
              count: 0,
              maxCount: 3,
              startDateMs: 0,
              impressionDateMs: 0,
            },
            {
              intervalSeconds: "bogus",
              count: 0,
              maxCount: 99,
              startDateMs: 0,
              impressionDateMs: 0,
            },
            {
              intervalSeconds: Infinity,
              count: 0,
              maxCount: 5,
              startDateMs: 0,
              impressionDateMs: 0,
            },
          ],
        })
      );
      Assert.deepEqual(
        QuickSuggest.impressionCaps._test_stats,
        expectedStats,
        "Expected stats with intervalSeconds: 'bogus'"
      );

      UrlbarPrefs.set(
        "quicksuggest.impressionCaps.stats",
        JSON.stringify({
          sponsored: [
            {
              intervalSeconds: 3,
              count: 0,
              maxCount: 123,
              startDateMs: 0,
              impressionDateMs: 0,
            },
            {
              intervalSeconds: Infinity,
              count: 0,
              maxCount: 456,
              startDateMs: 0,
              impressionDateMs: 0,
            },
          ],
        })
      );
      Assert.deepEqual(
        QuickSuggest.impressionCaps._test_stats,
        expectedStats,
        "Expected stats with `maxCount` values different from caps"
      );

      let stats = {
        sponsored: [
          {
            intervalSeconds: 3,
            count: 1,
            maxCount: 3,
            startDateMs: 99,
            impressionDateMs: 99,
          },
          {
            intervalSeconds: Infinity,
            count: 7,
            maxCount: 5,
            startDateMs: 1337,
            impressionDateMs: 1337,
          },
        ],
      };
      UrlbarPrefs.set(
        "quicksuggest.impressionCaps.stats",
        JSON.stringify(stats)
      );
      Assert.deepEqual(
        QuickSuggest.impressionCaps._test_stats,
        stats,
        "Expected stats with valid JSON"
      );
    },
  });
});

// Tests multiple interval periods where the cap is not hit. Telemetry should be
// recorded for these periods.
add_task(async function intervalsElapsedButCapNotHit() {
  await doTest({
    config: {
      impression_caps: {
        sponsored: {
          custom: [{ interval_s: 1, max_count: 3 }],
        },
      },
    },
    callback: async () => {
      await doTimedCallbacks({
        // 1s
        1: async () => {
          await checkSearch({
            name: "1s",
            searchString: "sponsored",
            expectedResults: [EXPECTED_SPONSORED_URLBAR_RESULT],
          });
        },
        // 10s
        10: async () => {
          QuickSuggest.impressionCaps._test_resetElapsedCounters();
          let expectedEvents = [
            // 1s: reset with count = 0
            {
              object: "reset",
              extra: {
                eventDate: "1000",
                intervalSeconds: "1",
                maxCount: "3",
                startDate: "0",
                impressionDate: "0",
                count: "0",
                type: "sponsored",
                eventCount: "1",
              },
            },
            // 2-10s: reset with count = 1, eventCount = 9
            {
              object: "reset",
              extra: {
                eventDate: "10000",
                intervalSeconds: "1",
                maxCount: "3",
                startDate: "1000",
                impressionDate: "1000",
                count: "1",
                type: "sponsored",
                eventCount: "9",
              },
            },
          ];
          await checkTelemetryEvents(expectedEvents);
        },
      });
    },
  });
});

// Simulates reset events across a restart with the following:
//
//   S                      S                          R
//   >----|----|----|----|----|----|----|----|----|----|
//   0s   1    2    3    4    5    6    7    8    9   10
//
//   1. Startup at 0s
//   2. Caps and stats initialized with interval_s: 1
//   3. Startup at 4.5s
//   4. Reset triggered at 10s
//
// Expected:
//   At 10s: 6 batched resets for periods starting at 4s
add_task(async function restart_1() {
  await doTest({
    config: {
      impression_caps: {
        sponsored: {
          custom: [{ interval_s: 1, max_count: 1 }],
        },
      },
    },
    callback: async () => {
      gStartupDateMsStub.returns(4500);
      await doTimedCallbacks({
        // 10s: 6 batched resets for periods starting at 4s
        10: async () => {
          QuickSuggest.impressionCaps._test_resetElapsedCounters();
          await checkTelemetryEvents([
            {
              object: "reset",
              extra: {
                eventDate: "10000",
                intervalSeconds: "1",
                maxCount: "1",
                startDate: "4000",
                impressionDate: "0",
                count: "0",
                type: "sponsored",
                eventCount: "6",
              },
            },
          ]);
        },
      });
    },
  });
  gStartupDateMsStub.returns(0);
});

// Simulates reset events across a restart with the following:
//
//   S                        S                        R
//   >----|----|----|----|----|----|----|----|----|----|
//   0s   1    2    3    4    5    6    7    8    9   10
//
//   1. Startup at 0s
//   2. Caps and stats initialized with interval_s: 1
//   3. Startup at 5s
//   4. Reset triggered at 10s
//
// Expected:
//   At 10s: 5 batched resets for periods starting at 5s
add_task(async function restart_2() {
  await doTest({
    config: {
      impression_caps: {
        sponsored: {
          custom: [{ interval_s: 1, max_count: 1 }],
        },
      },
    },
    callback: async () => {
      gStartupDateMsStub.returns(5000);
      await doTimedCallbacks({
        // 10s: 5 batched resets for periods starting at 5s
        10: async () => {
          QuickSuggest.impressionCaps._test_resetElapsedCounters();
          await checkTelemetryEvents([
            {
              object: "reset",
              extra: {
                eventDate: "10000",
                intervalSeconds: "1",
                maxCount: "1",
                startDate: "5000",
                impressionDate: "0",
                count: "0",
                type: "sponsored",
                eventCount: "5",
              },
            },
          ]);
        },
      });
    },
  });
  gStartupDateMsStub.returns(0);
});

// Simulates reset events across a restart with the following:
//
//   S                           S                     R
//   >----|----|----|----|----|----|----|----|----|----|
//   0s   1    2    3    4    5    6    7    8    9   10
//
//   1. Startup at 0s
//   2. Caps and stats initialized with interval_s: 1
//   3. Startup at 5.5s
//   4. Reset triggered at 10s
//
// Expected:
//   At 10s: 5 batched resets for periods starting at 5s
add_task(async function restart_3() {
  await doTest({
    config: {
      impression_caps: {
        sponsored: {
          custom: [{ interval_s: 1, max_count: 1 }],
        },
      },
    },
    callback: async () => {
      gStartupDateMsStub.returns(5500);
      await doTimedCallbacks({
        // 10s: 5 batched resets for periods starting at 5s
        10: async () => {
          QuickSuggest.impressionCaps._test_resetElapsedCounters();
          await checkTelemetryEvents([
            {
              object: "reset",
              extra: {
                eventDate: "10000",
                intervalSeconds: "1",
                maxCount: "1",
                startDate: "5000",
                impressionDate: "0",
                count: "0",
                type: "sponsored",
                eventCount: "5",
              },
            },
          ]);
        },
      });
    },
  });
  gStartupDateMsStub.returns(0);
});

// Simulates reset events across a restart with the following:
//
//   S    S   RR        RR
//   >---------|---------|
//   0s       10        20
//
//   1. Startup at 0s
//   2. Caps and stats initialized with interval_s: 10
//   3. Startup at 5s
//   4. Resets triggered at 9s, 10s, 19s, 20s
//
// Expected:
//   At 10s: 1 reset for period starting at 0s
//   At 20s: 1 reset for period starting at 10s
add_task(async function restart_4() {
  await doTest({
    config: {
      impression_caps: {
        sponsored: {
          custom: [{ interval_s: 10, max_count: 1 }],
        },
      },
    },
    callback: async () => {
      gStartupDateMsStub.returns(5000);
      await doTimedCallbacks({
        // 9s: no resets
        9: async () => {
          QuickSuggest.impressionCaps._test_resetElapsedCounters();
          await checkTelemetryEvents([]);
        },
        // 10s: 1 reset for period starting at 0s
        10: async () => {
          QuickSuggest.impressionCaps._test_resetElapsedCounters();
          await checkTelemetryEvents([
            {
              object: "reset",
              extra: {
                eventDate: "10000",
                intervalSeconds: "10",
                maxCount: "1",
                startDate: "0",
                impressionDate: "0",
                count: "0",
                type: "sponsored",
                eventCount: "1",
              },
            },
          ]);
        },
        // 19s: no resets
        19: async () => {
          QuickSuggest.impressionCaps._test_resetElapsedCounters();
          await checkTelemetryEvents([]);
        },
        // 20s: 1 reset for period starting at 10s
        20: async () => {
          QuickSuggest.impressionCaps._test_resetElapsedCounters();
          await checkTelemetryEvents([
            {
              object: "reset",
              extra: {
                eventDate: "20000",
                intervalSeconds: "10",
                maxCount: "1",
                startDate: "10000",
                impressionDate: "0",
                count: "0",
                type: "sponsored",
                eventCount: "1",
              },
            },
          ]);
        },
      });
    },
  });
  gStartupDateMsStub.returns(0);
});

// Simulates reset events across a restart with the following:
//
//   S    S              R
//   >---------|---------|
//   0s       10        20
//
//   1. Startup at 0s
//   2. Caps and stats initialized with interval_s: 10
//   3. Startup at 5s
//   4. Reset triggered at 20s
//
// Expected:
//   At 20s: 2 batched resets for periods starting at 0s
add_task(async function restart_5() {
  await doTest({
    config: {
      impression_caps: {
        sponsored: {
          custom: [{ interval_s: 10, max_count: 1 }],
        },
      },
    },
    callback: async () => {
      gStartupDateMsStub.returns(5000);
      await doTimedCallbacks({
        // 20s: 2 batches resets for periods starting at 0s
        20: async () => {
          QuickSuggest.impressionCaps._test_resetElapsedCounters();
          await checkTelemetryEvents([
            {
              object: "reset",
              extra: {
                eventDate: "20000",
                intervalSeconds: "10",
                maxCount: "1",
                startDate: "0",
                impressionDate: "0",
                count: "0",
                type: "sponsored",
                eventCount: "2",
              },
            },
          ]);
        },
      });
    },
  });
  gStartupDateMsStub.returns(0);
});

// Simulates reset events across a restart with the following:
//
//   S              S   RR        RR
//   >---------|---------|---------|
//   0s       10        20        30
//
//   1. Startup at 0s
//   2. Caps and stats initialized with interval_s: 10
//   3. Startup at 15s
//   4. Resets triggered at 19s, 20s, 29s, 30s
//
// Expected:
//   At 20s: 1 reset for period starting at 10s
//   At 30s: 1 reset for period starting at 20s
add_task(async function restart_6() {
  await doTest({
    config: {
      impression_caps: {
        sponsored: {
          custom: [{ interval_s: 10, max_count: 1 }],
        },
      },
    },
    callback: async () => {
      gStartupDateMsStub.returns(15000);
      await doTimedCallbacks({
        // 19s: no resets
        19: async () => {
          QuickSuggest.impressionCaps._test_resetElapsedCounters();
          await checkTelemetryEvents([]);
        },
        // 20s: 1 reset for period starting at 10s
        20: async () => {
          QuickSuggest.impressionCaps._test_resetElapsedCounters();
          await checkTelemetryEvents([
            {
              object: "reset",
              extra: {
                eventDate: "20000",
                intervalSeconds: "10",
                maxCount: "1",
                startDate: "10000",
                impressionDate: "0",
                count: "0",
                type: "sponsored",
                eventCount: "1",
              },
            },
          ]);
        },
        // 29s: no resets
        29: async () => {
          QuickSuggest.impressionCaps._test_resetElapsedCounters();
          await checkTelemetryEvents([]);
        },
        // 30s: 1 reset for period starting at 20s
        30: async () => {
          QuickSuggest.impressionCaps._test_resetElapsedCounters();
          await checkTelemetryEvents([
            {
              object: "reset",
              extra: {
                eventDate: "30000",
                intervalSeconds: "10",
                maxCount: "1",
                startDate: "20000",
                impressionDate: "0",
                count: "0",
                type: "sponsored",
                eventCount: "1",
              },
            },
          ]);
        },
      });
    },
  });
  gStartupDateMsStub.returns(0);
});

// Simulates reset events across a restart with the following:
//
//   S              S              R
//   >---------|---------|---------|
//   0s       10        20        30
//
//   1. Startup at 0s
//   2. Caps and stats initialized with interval_s: 10
//   3. Startup at 15s
//   4. Reset triggered at 30s
//
// Expected:
//   At 30s: 2 batched resets for periods starting at 10s
add_task(async function restart_7() {
  await doTest({
    config: {
      impression_caps: {
        sponsored: {
          custom: [{ interval_s: 10, max_count: 1 }],
        },
      },
    },
    callback: async () => {
      gStartupDateMsStub.returns(15000);
      await doTimedCallbacks({
        // 30s: 2 batched resets for periods starting at 10s
        30: async () => {
          QuickSuggest.impressionCaps._test_resetElapsedCounters();
          await checkTelemetryEvents([
            {
              object: "reset",
              extra: {
                eventDate: "30000",
                intervalSeconds: "10",
                maxCount: "1",
                startDate: "10000",
                impressionDate: "0",
                count: "0",
                type: "sponsored",
                eventCount: "2",
              },
            },
          ]);
        },
      });
    },
  });
  gStartupDateMsStub.returns(0);
});

// Tests reset telemetry recorded on shutdown.
add_task(async function shutdown() {
  await doTest({
    config: {
      impression_caps: {
        sponsored: {
          custom: [{ interval_s: 1, max_count: 1 }],
        },
      },
    },
    callback: async () => {
      // Make `Date.now()` return 10s. Since the cap's `interval_s` is 1s and
      // before this `Date.now()` returned 0s, 10 reset events should be
      // recorded on shutdown.
      gDateNowStub.returns(10000);

      // Simulate shutdown.
      Services.prefs.setBoolPref("toolkit.asyncshutdown.testing", true);
      AsyncShutdown.profileChangeTeardown._trigger();

      await checkTelemetryEvents([
        {
          object: "reset",
          extra: {
            eventDate: "10000",
            intervalSeconds: "1",
            maxCount: "1",
            startDate: "0",
            impressionDate: "0",
            count: "0",
            type: "sponsored",
            eventCount: "10",
          },
        },
      ]);

      gDateNowStub.returns(0);
      Services.prefs.clearUserPref("toolkit.asyncshutdown.testing");
    },
  });
});

// Tests the reset interval in realtime.
add_task(async function resetInterval() {
  // Remove the test stubs so we can test in realtime.
  gDateNowStub.restore();
  gStartupDateMsStub.restore();

  await doTest({
    config: {
      impression_caps: {
        sponsored: {
          custom: [{ interval_s: 0.1, max_count: 1 }],
        },
      },
    },
    callback: async () => {
      // Restart the reset interval now with a 1s period. Since the cap's
      // `interval_s` is 0.1s, at least 10 reset events should be recorded the
      // first time the reset interval fires. The exact number depends on timing
      // and the machine running the test: how many 0.1s intervals elapse
      // between when the config is set to when the reset interval fires. For
      // that reason, we allow some leeway when checking `eventCount` below to
      // avoid intermittent failures.
      QuickSuggest.impressionCaps._test_setCountersResetInterval(1000);

      // eslint-disable-next-line mozilla/no-arbitrary-setTimeout
      await new Promise(r => setTimeout(r, 1100));

      // Restore the reset interval to its default.
      QuickSuggest.impressionCaps._test_setCountersResetInterval();

      await checkTelemetryEvents([
        {
          object: "reset",
          extra: {
            eventDate: /^[0-9]+$/,
            intervalSeconds: "0.1",
            maxCount: "1",
            startDate: /^[0-9]+$/,
            impressionDate: "0",
            count: "0",
            type: "sponsored",
            // See comment above on allowing leeway for `eventCount`.
            eventCount: str => {
              info(`Checking 'eventCount': ${str}`);
              let count = parseInt(str);
              return 10 <= count && count < 20;
            },
          },
        },
      ]);
    },
  });

  // Recreate the test stubs.
  gDateNowStub = gSandbox.stub(
    Cu.getGlobalForObject(UrlbarProviderQuickSuggest).Date,
    "now"
  );
  gStartupDateMsStub = gSandbox.stub(
    QuickSuggest.impressionCaps,
    "_getStartupDateMs"
  );
  gStartupDateMsStub.returns(0);
});

/**
 * Main test helper. Sets up state, calls your callback, and resets state.
 *
 * @param {object} options
 *   Options object.
 * @param {object} options.config
 *   The quick suggest config to use during the test.
 * @param {Function} options.callback
 *   The callback that will be run with the {@link config}
 */
async function doTest({ config, callback }) {
  Services.telemetry.clearEvents();

  // Make `Date.now()` return 0 to start with. It's necessary to do this before
  // calling `withConfig()` because when a new config is set, the provider
  // validates its impression stats, whose `startDateMs` values depend on
  // `Date.now()`.
  gDateNowStub.returns(0);

  info(`Clearing stats and setting config`);
  UrlbarPrefs.clear("quicksuggest.impressionCaps.stats");
  QuickSuggest.impressionCaps._test_reloadStats();
  await QuickSuggestTestUtils.withConfig({ config, callback });
}

/**
 * Does a series of timed searches and checks their results and telemetry. This
 * function relies on `doTimedCallbacks()`, so it may be helpful to look at it
 * too.
 *
 * @param {string} searchString
 *   The query that should be timed
 * @param {object} expectedBySecond
 *   An object that maps from seconds to objects that describe the searches to
 *   perform, their expected results, and the expected telemetry. For a given
 *   entry `S -> E` in this object, searches are performed S seconds after this
 *   function is called. `E` is an object that looks like this:
 *
 *     { results, telemetry }
 *
 *     {array} results
 *       An array of arrays. A search is performed for each sub-array in
 *       `results`, and the contents of the sub-array are the expected results
 *       for that search.
 *     {object} telemetry
 *       An object like this: { events }
 *       {array} events
 *         An array of expected telemetry events after all searches are done.
 *         Telemetry events are cleared after checking these. If not present,
 *         then it will be asserted that no events were recorded.
 *
 *   Example:
 *
 *     {
 *       0: {
 *         results: [[R1], []],
 *         telemetry: {
 *           events: [
 *             someExpectedEvent,
 *           ],
 *         },
 *       }
 *       1: {
 *         results: [[]],
 *       },
 *     }
 *
 *     0 seconds after `doTimedSearches()` is called, two searches are
 *     performed. The first one is expected to return a single result R1, and
 *     the second search is expected to return no results. After the searches
 *     are done, one telemetry event is expected to be recorded.
 *
 *     1 second after `doTimedSearches()` is called, one search is performed.
 *     It's expected to return no results, and no telemetry is expected to be
 *     recorded.
 */
async function doTimedSearches(searchString, expectedBySecond) {
  await doTimedCallbacks(
    Object.entries(expectedBySecond).reduce(
      (memo, [second, { results, telemetry }]) => {
        memo[second] = async () => {
          for (let i = 0; i < results.length; i++) {
            let expectedResults = results[i];
            await checkSearch({
              searchString,
              expectedResults,
              name: `${second}s search ${i + 1} of ${results.length}`,
            });
          }
          let { events } = telemetry || {};
          await checkTelemetryEvents(events || []);
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
// timer whose period is 1s (e.g., using `setInterval()` from Timer.sys.mjs).
// After ~33 ticks, the timer's period jumps to ~10s.
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
 * @param {object} options
 *   Options object.
 * @param {string} options.name
 *   This value is the name of the search and will be logged in messages to make
 *   debugging easier.
 * @param {string} options.searchString
 *   The query that should be searched.
 * @param {Array} options.expectedResults
 *   The results that are expected from the search.
 */
async function checkSearch({ name, searchString, expectedResults }) {
  info(`Preparing search "${name}" with search string "${searchString}"`);
  let context = createContext(searchString, {
    providers: [UrlbarProviderQuickSuggest.name],
    isPrivate: false,
  });
  info(`Doing search: ${name}`);
  await check_results({
    context,
    matches: expectedResults,
  });
  info(`Finished search: ${name}`);

  // Impression stats are updated only on engagement, so force one now.
  // `selIndex` doesn't really matter but since we're not trying to simulate a
  // click on the suggestion, pass in -1 to ensure we don't record a click.
  if (UrlbarProviderQuickSuggest._resultFromLastQuery) {
    UrlbarProviderQuickSuggest._resultFromLastQuery.isVisible = true;
  }
  const controller = UrlbarTestUtils.newMockController({
    input: {
      isPrivate: true,
      onFirstResult() {
        return false;
      },
      getSearchSource() {
        return "dummy-search-source";
      },
      window: {
        location: {
          href: AppConstants.BROWSER_CHROME_URL,
        },
      },
    },
  });
  controller.setView({
    get visibleResults() {
      return context.results;
    },
    controller: {
      removeResult() {},
    },
  });
  UrlbarProviderQuickSuggest.onEngagement(
    "engagement",
    context,
    {
      selIndex: -1,
    },
    controller
  );
}

async function checkTelemetryEvents(expectedEvents) {
  QuickSuggestTestUtils.assertEvents(
    expectedEvents.map(event => ({
      ...event,
      category: QuickSuggest.TELEMETRY_EVENT_CATEGORY,
      method: "impression_cap",
    })),
    // Filter in only impression_cap events.
    { method: "impression_cap" }
  );
}
