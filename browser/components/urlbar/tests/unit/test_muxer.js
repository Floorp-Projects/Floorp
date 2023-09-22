/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

const lazy = {};

ChromeUtils.defineESModuleGetters(lazy, {
  sinon: "resource://testing-common/Sinon.sys.mjs",
});

let sandbox;

add_setup(async function () {
  sandbox = lazy.sinon.createSandbox();
  registerCleanupFunction(() => {
    sandbox.restore();
  });
});

add_task(async function test_muxer() {
  Assert.throws(
    () => UrlbarProvidersManager.registerMuxer(),
    /invalid muxer/,
    "Should throw with no arguments"
  );
  Assert.throws(
    () => UrlbarProvidersManager.registerMuxer({}),
    /invalid muxer/,
    "Should throw with empty object"
  );
  Assert.throws(
    () =>
      UrlbarProvidersManager.registerMuxer({
        name: "",
      }),
    /invalid muxer/,
    "Should throw with empty name"
  );
  Assert.throws(
    () =>
      UrlbarProvidersManager.registerMuxer({
        name: "test",
        sort: "no",
      }),
    /invalid muxer/,
    "Should throw with invalid sort"
  );

  let matches = [
    new UrlbarResult(
      UrlbarUtils.RESULT_TYPE.TAB_SWITCH,
      UrlbarUtils.RESULT_SOURCE.TABS,
      { url: "http://mozilla.org/tab/" }
    ),
    new UrlbarResult(
      UrlbarUtils.RESULT_TYPE.URL,
      UrlbarUtils.RESULT_SOURCE.BOOKMARKS,
      { url: "http://mozilla.org/bookmark/" }
    ),
    new UrlbarResult(
      UrlbarUtils.RESULT_TYPE.URL,
      UrlbarUtils.RESULT_SOURCE.HISTORY,
      { url: "http://mozilla.org/history/" }
    ),
  ];

  let provider = registerBasicTestProvider(matches);
  let context = createContext(undefined, { providers: [provider.name] });
  let controller = UrlbarTestUtils.newMockController();
  /**
   * A test muxer.
   */
  class TestMuxer extends UrlbarMuxer {
    get name() {
      return "TestMuxer";
    }
    sort(queryContext, unsortedResults) {
      queryContext.results = [...unsortedResults].sort((a, b) => {
        if (b.source == UrlbarUtils.RESULT_SOURCE.TABS) {
          return -1;
        }
        if (b.source == UrlbarUtils.RESULT_SOURCE.BOOKMARKS) {
          return 1;
        }
        return a.source == UrlbarUtils.RESULT_SOURCE.BOOKMARKS ? -1 : 1;
      });
    }
  }
  let muxer = new TestMuxer();

  UrlbarProvidersManager.registerMuxer(muxer);
  context.muxer = "TestMuxer";

  info("Check results, the order should be: bookmark, history, tab");
  await UrlbarProvidersManager.startQuery(context, controller);
  Assert.deepEqual(context.results, [matches[1], matches[2], matches[0]]);

  // Sanity check, should not throw.
  UrlbarProvidersManager.unregisterMuxer(muxer);
  UrlbarProvidersManager.unregisterMuxer("TestMuxer"); // no-op.
});

add_task(async function test_preselectedHeuristic_singleProvider() {
  let matches = [
    new UrlbarResult(
      UrlbarUtils.RESULT_TYPE.URL,
      UrlbarUtils.RESULT_SOURCE.HISTORY,
      { url: "http://mozilla.org/a" }
    ),
    new UrlbarResult(
      UrlbarUtils.RESULT_TYPE.URL,
      UrlbarUtils.RESULT_SOURCE.HISTORY,
      { url: "http://mozilla.org/b" }
    ),
    new UrlbarResult(
      UrlbarUtils.RESULT_TYPE.URL,
      UrlbarUtils.RESULT_SOURCE.HISTORY,
      { url: "http://mozilla.org/c" }
    ),
  ];
  matches[1].heuristic = true;

  let provider = registerBasicTestProvider(matches);
  let context = createContext(undefined, {
    providers: [provider.name],
  });
  let controller = UrlbarTestUtils.newMockController();

  info("Check results, the order should be: b (heuristic), a, c");
  await UrlbarProvidersManager.startQuery(context, controller);
  Assert.deepEqual(context.results, [matches[1], matches[0], matches[2]]);
});

add_task(async function test_preselectedHeuristic_multiProviders() {
  let matches1 = [
    new UrlbarResult(
      UrlbarUtils.RESULT_TYPE.URL,
      UrlbarUtils.RESULT_SOURCE.HISTORY,
      { url: "http://mozilla.org/a" }
    ),
    new UrlbarResult(
      UrlbarUtils.RESULT_TYPE.URL,
      UrlbarUtils.RESULT_SOURCE.HISTORY,
      { url: "http://mozilla.org/b" }
    ),
    new UrlbarResult(
      UrlbarUtils.RESULT_TYPE.URL,
      UrlbarUtils.RESULT_SOURCE.HISTORY,
      { url: "http://mozilla.org/c" }
    ),
  ];

  let matches2 = [
    new UrlbarResult(
      UrlbarUtils.RESULT_TYPE.URL,
      UrlbarUtils.RESULT_SOURCE.HISTORY,
      { url: "http://mozilla.org/d" }
    ),
    new UrlbarResult(
      UrlbarUtils.RESULT_TYPE.URL,
      UrlbarUtils.RESULT_SOURCE.HISTORY,
      { url: "http://mozilla.org/e" }
    ),
    new UrlbarResult(
      UrlbarUtils.RESULT_TYPE.URL,
      UrlbarUtils.RESULT_SOURCE.HISTORY,
      { url: "http://mozilla.org/f" }
    ),
  ];
  matches2[1].heuristic = true;

  let provider1 = registerBasicTestProvider(matches1);
  let provider2 = registerBasicTestProvider(matches2);

  let context = createContext(undefined, {
    providers: [provider1.name, provider2.name],
  });
  let controller = UrlbarTestUtils.newMockController();

  info("Check results, the order should be: e (heuristic), a, b, c, d, f");
  await UrlbarProvidersManager.startQuery(context, controller);
  Assert.deepEqual(context.results, [
    matches2[1],
    ...matches1,
    matches2[0],
    matches2[2],
  ]);
});

add_task(async function test_suggestions() {
  Services.prefs.setIntPref("browser.urlbar.maxHistoricalSearchSuggestions", 1);

  let matches = [
    new UrlbarResult(
      UrlbarUtils.RESULT_TYPE.URL,
      UrlbarUtils.RESULT_SOURCE.HISTORY,
      { url: "http://mozilla.org/a" }
    ),
    new UrlbarResult(
      UrlbarUtils.RESULT_TYPE.URL,
      UrlbarUtils.RESULT_SOURCE.HISTORY,
      { url: "http://mozilla.org/b" }
    ),
    new UrlbarResult(
      UrlbarUtils.RESULT_TYPE.SEARCH,
      UrlbarUtils.RESULT_SOURCE.HISTORY,
      {
        engine: "mozSearch",
        query: "moz",
        suggestion: "mozzarella",
        lowerCaseSuggestion: "mozzarella",
      }
    ),
    new UrlbarResult(
      UrlbarUtils.RESULT_TYPE.SEARCH,
      UrlbarUtils.RESULT_SOURCE.SEARCH,
      {
        engine: "mozSearch",
        query: "moz",
        suggestion: "mozilla",
        lowerCaseSuggestion: "mozilla",
      }
    ),
    new UrlbarResult(
      UrlbarUtils.RESULT_TYPE.SEARCH,
      UrlbarUtils.RESULT_SOURCE.SEARCH,
      {
        engine: "mozSearch",
        query: "moz",
        providesSearchMode: true,
        keyword: "@moz",
      }
    ),
    new UrlbarResult(
      UrlbarUtils.RESULT_TYPE.URL,
      UrlbarUtils.RESULT_SOURCE.HISTORY,
      { url: "http://mozilla.org/c" }
    ),
  ];

  let provider = registerBasicTestProvider(matches);

  let context = createContext(undefined, {
    providers: [provider.name],
  });
  let controller = UrlbarTestUtils.newMockController();

  info("Check results, the order should be: mozzarella, moz, a, b, @moz, c");
  await UrlbarProvidersManager.startQuery(context, controller);
  Assert.deepEqual(context.results, [
    matches[2],
    matches[3],
    matches[0],
    matches[1],
    matches[4],
    matches[5],
  ]);

  Services.prefs.clearUserPref("browser.urlbar.maxHistoricalSearchSuggestions");
});

add_task(async function test_deduplicate_for_unitConversion() {
  const searchSuggestion = new UrlbarResult(
    UrlbarUtils.RESULT_TYPE.SEARCH,
    UrlbarUtils.RESULT_SOURCE.SEARCH,
    {
      engine: "Google",
      query: "10cm to m",
      suggestion: "= 0.1 meters",
    }
  );
  const searchProvider = registerBasicTestProvider(
    [searchSuggestion],
    null,
    UrlbarUtils.PROVIDER_TYPE.PROFILE
  );

  const unitConversionSuggestion = new UrlbarResult(
    UrlbarUtils.RESULT_TYPE.DYNAMIC,
    UrlbarUtils.RESULT_SOURCE.OTHER_LOCAL,
    {
      dynamicType: "unitConversion",
      output: "0.1 m",
      input: "10cm to m",
    }
  );
  unitConversionSuggestion.suggestedIndex = 1;

  const unitConversion = registerBasicTestProvider(
    [unitConversionSuggestion],
    null,
    UrlbarUtils.PROVIDER_TYPE.PROFILE,
    "UnitConversion"
  );

  const context = createContext(undefined, {
    providers: [searchProvider.name, unitConversion.name],
  });
  const controller = UrlbarTestUtils.newMockController();
  await UrlbarProvidersManager.startQuery(context, controller);
  Assert.deepEqual(context.results, [unitConversionSuggestion]);
});

// These results are used in the badHeuristicGroups tests below.  The order of
// the results in the array isn't important because they all get added at the
// same time.  It's the resultGroups in each test that is important.
const BAD_HEURISTIC_RESULTS = [
  // heuristic
  Object.assign(
    new UrlbarResult(
      UrlbarUtils.RESULT_TYPE.URL,
      UrlbarUtils.RESULT_SOURCE.HISTORY,
      { url: "http://mozilla.org/heuristic-0" }
    ),
    { heuristic: true }
  ),
  // heuristic
  Object.assign(
    new UrlbarResult(
      UrlbarUtils.RESULT_TYPE.URL,
      UrlbarUtils.RESULT_SOURCE.HISTORY,
      { url: "http://mozilla.org/heuristic-1" }
    ),
    { heuristic: true }
  ),
  // non-heuristic
  new UrlbarResult(
    UrlbarUtils.RESULT_TYPE.URL,
    UrlbarUtils.RESULT_SOURCE.HISTORY,
    { url: "http://mozilla.org/non-heuristic-0" }
  ),
  // non-heuristic
  new UrlbarResult(
    UrlbarUtils.RESULT_TYPE.URL,
    UrlbarUtils.RESULT_SOURCE.HISTORY,
    { url: "http://mozilla.org/non-heuristic-1" }
  ),
];

const BAD_HEURISTIC_RESULTS_FIRST_HEURISTIC = BAD_HEURISTIC_RESULTS[0];
const BAD_HEURISTIC_RESULTS_GENERAL = [
  BAD_HEURISTIC_RESULTS[2],
  BAD_HEURISTIC_RESULTS[3],
];

add_task(async function test_badHeuristicGroups_multiple_0() {
  await doBadHeuristicGroupsTest(
    [
      // 2 heuristics with child groups
      {
        maxResultCount: 2,
        children: [{ group: UrlbarUtils.RESULT_GROUP.HEURISTIC_TEST }],
      },
      // infinite general
      {
        group: UrlbarUtils.RESULT_GROUP.GENERAL,
      },
    ],
    [BAD_HEURISTIC_RESULTS_FIRST_HEURISTIC, ...BAD_HEURISTIC_RESULTS_GENERAL]
  );
});

add_task(async function test_badHeuristicGroups_multiple_1() {
  await doBadHeuristicGroupsTest(
    [
      // infinite heuristics with child groups
      {
        children: [{ group: UrlbarUtils.RESULT_GROUP.HEURISTIC_TEST }],
      },
      // infinite general
      {
        group: UrlbarUtils.RESULT_GROUP.GENERAL,
      },
    ],
    [BAD_HEURISTIC_RESULTS_FIRST_HEURISTIC, ...BAD_HEURISTIC_RESULTS_GENERAL]
  );
});

add_task(async function test_badHeuristicGroups_multiple_2() {
  await doBadHeuristicGroupsTest(
    [
      // 2 heuristics
      {
        maxResultCount: 2,
        group: UrlbarUtils.RESULT_GROUP.HEURISTIC_TEST,
      },
      // infinite general
      {
        group: UrlbarUtils.RESULT_GROUP.GENERAL,
      },
    ],
    [BAD_HEURISTIC_RESULTS_FIRST_HEURISTIC, ...BAD_HEURISTIC_RESULTS_GENERAL]
  );
});

add_task(async function test_badHeuristicGroups_multiple_3() {
  await doBadHeuristicGroupsTest(
    [
      // infinite heuristics
      {
        group: UrlbarUtils.RESULT_GROUP.HEURISTIC_TEST,
      },
      // infinite general
      {
        group: UrlbarUtils.RESULT_GROUP.GENERAL,
      },
    ],
    [BAD_HEURISTIC_RESULTS_FIRST_HEURISTIC, ...BAD_HEURISTIC_RESULTS_GENERAL]
  );
});

add_task(async function test_badHeuristicGroups_multiple_4() {
  await doBadHeuristicGroupsTest(
    [
      // 1 heuristic with child groups
      {
        maxResultCount: 1,
        children: [{ group: UrlbarUtils.RESULT_GROUP.HEURISTIC_TEST }],
      },
      // infinite general
      {
        group: UrlbarUtils.RESULT_GROUP.GENERAL,
      },
      // 1 heuristic with child groups
      {
        maxResultCount: 1,
        children: [{ group: UrlbarUtils.RESULT_GROUP.HEURISTIC_TEST }],
      },
    ],
    [BAD_HEURISTIC_RESULTS_FIRST_HEURISTIC, ...BAD_HEURISTIC_RESULTS_GENERAL]
  );
});

add_task(async function test_badHeuristicGroups_multiple_5() {
  await doBadHeuristicGroupsTest(
    [
      // infinite heuristics with child groups
      {
        children: [{ group: UrlbarUtils.RESULT_GROUP.HEURISTIC_TEST }],
      },
      // infinite general
      {
        group: UrlbarUtils.RESULT_GROUP.GENERAL,
      },
      // infinite heuristics with child groups
      {
        children: [{ group: UrlbarUtils.RESULT_GROUP.HEURISTIC_TEST }],
      },
    ],
    [BAD_HEURISTIC_RESULTS_FIRST_HEURISTIC, ...BAD_HEURISTIC_RESULTS_GENERAL]
  );
});

add_task(async function test_badHeuristicGroups_multiple_6() {
  await doBadHeuristicGroupsTest(
    [
      // 1 heuristic
      {
        maxResultCount: 1,
        group: UrlbarUtils.RESULT_GROUP.HEURISTIC_TEST,
      },
      // infinite general
      {
        group: UrlbarUtils.RESULT_GROUP.GENERAL,
      },
      // 1 heuristic
      {
        maxResultCount: 1,
        group: UrlbarUtils.RESULT_GROUP.HEURISTIC_TEST,
      },
    ],
    [BAD_HEURISTIC_RESULTS_FIRST_HEURISTIC, ...BAD_HEURISTIC_RESULTS_GENERAL]
  );
});

add_task(async function test_badHeuristicGroups_multiple_7() {
  await doBadHeuristicGroupsTest(
    [
      // infinite heuristics
      {
        group: UrlbarUtils.RESULT_GROUP.HEURISTIC_TEST,
      },
      // infinite general
      {
        group: UrlbarUtils.RESULT_GROUP.GENERAL,
      },
      // infinite heuristics
      {
        group: UrlbarUtils.RESULT_GROUP.HEURISTIC_TEST,
      },
    ],
    [BAD_HEURISTIC_RESULTS_FIRST_HEURISTIC, ...BAD_HEURISTIC_RESULTS_GENERAL]
  );
});

add_task(async function test_badHeuristicsGroups_notFirst_0() {
  await doBadHeuristicGroupsTest(
    [
      // infinite general first
      {
        group: UrlbarUtils.RESULT_GROUP.GENERAL,
      },
      // 1 heuristic with child groups second
      {
        maxResultCount: 1,
        children: [{ group: UrlbarUtils.RESULT_GROUP.HEURISTIC_TEST }],
      },
    ],
    [...BAD_HEURISTIC_RESULTS_GENERAL]
  );
});

add_task(async function test_badHeuristicsGroups_notFirst_1() {
  await doBadHeuristicGroupsTest(
    [
      // infinite general first
      {
        group: UrlbarUtils.RESULT_GROUP.GENERAL,
      },
      // infinite heuristics with child groups second
      {
        children: [{ group: UrlbarUtils.RESULT_GROUP.HEURISTIC_TEST }],
      },
    ],
    [...BAD_HEURISTIC_RESULTS_GENERAL]
  );
});

add_task(async function test_badHeuristicsGroups_notFirst_2() {
  await doBadHeuristicGroupsTest(
    [
      // infinite general first
      {
        group: UrlbarUtils.RESULT_GROUP.GENERAL,
      },
      // 1 heuristic second
      {
        maxResultCount: 1,
        group: UrlbarUtils.RESULT_GROUP.HEURISTIC_TEST,
      },
    ],
    [...BAD_HEURISTIC_RESULTS_GENERAL]
  );
});

add_task(async function test_badHeuristicsGroups_notFirst_3() {
  await doBadHeuristicGroupsTest(
    [
      // infinite general first
      {
        group: UrlbarUtils.RESULT_GROUP.GENERAL,
      },
      // infinite heuristics second
      {
        group: UrlbarUtils.RESULT_GROUP.HEURISTIC_TEST,
      },
    ],
    [...BAD_HEURISTIC_RESULTS_GENERAL]
  );
});

add_task(async function test_badHeuristicsGroups_notFirst_4() {
  await doBadHeuristicGroupsTest(
    [
      // 1 general first
      {
        maxResultCount: 1,
        group: UrlbarUtils.RESULT_GROUP.GENERAL,
      },
      // infinite heuristics second
      {
        group: UrlbarUtils.RESULT_GROUP.HEURISTIC_TEST,
      },
      // infinite general third
      {
        group: UrlbarUtils.RESULT_GROUP.GENERAL,
      },
    ],
    [...BAD_HEURISTIC_RESULTS_GENERAL]
  );
});

/**
 * Sets the resultGroups pref, performs a search, and then checks the results.
 * Regardless of the groups, the muxer should include at most one heuristic in
 * its results and it should always be the first result.
 *
 * @param {Array} resultGroups
 *   The result groups.
 * @param {Array} expectedResults
 *   The expected results.
 */
async function doBadHeuristicGroupsTest(resultGroups, expectedResults) {
  sandbox.stub(UrlbarPrefs, "resultGroups").get(() => {
    return { children: resultGroups };
  });

  let provider = registerBasicTestProvider(BAD_HEURISTIC_RESULTS);
  let context = createContext("foo", { providers: [provider.name] });
  let controller = UrlbarTestUtils.newMockController();
  await UrlbarProvidersManager.startQuery(context, controller);
  Assert.deepEqual(context.results, expectedResults);

  sandbox.restore();
}

// When `maxRichResults` is positive and taken up by suggested-index result(s),
// both the heuristic and suggested-index results should be included because we
// (a) make room for the heuristic and (b) assume all suggested-index results
// should be included even if it means exceeding `maxRichResults`. The specified
// `maxRichResults` span will be exceeded in this case.
add_task(async function roomForHeuristic_suggestedIndex() {
  let results = [
    Object.assign(
      new UrlbarResult(
        UrlbarUtils.RESULT_TYPE.URL,
        UrlbarUtils.RESULT_SOURCE.HISTORY,
        { url: "http://example.com/heuristic" }
      ),
      { heuristic: true }
    ),
    Object.assign(
      new UrlbarResult(
        UrlbarUtils.RESULT_TYPE.URL,
        UrlbarUtils.RESULT_SOURCE.HISTORY,
        { url: "http://example.com/suggestedIndex" }
      ),
      { suggestedIndex: 1 }
    ),
  ];

  UrlbarPrefs.set("maxRichResults", 1);

  let provider = registerBasicTestProvider(results);
  let context = createContext(undefined, { providers: [provider.name] });
  await check_results({
    context,
    matches: results,
  });

  UrlbarPrefs.clear("maxRichResults");
});

// When `maxRichResults` is positive but less than the heuristic's result span,
// the heuristic should be included because we make room for it even if it means
// exceeding `maxRichResults`. The specified `maxRichResults` span will be
// exceeded in this case.
add_task(async function roomForHeuristic_largeResultSpan() {
  let results = [
    Object.assign(
      new UrlbarResult(
        UrlbarUtils.RESULT_TYPE.URL,
        UrlbarUtils.RESULT_SOURCE.HISTORY,
        { url: "http://example.com/heuristic" }
      ),
      { heuristic: true, resultSpan: 2 }
    ),
  ];

  UrlbarPrefs.set("maxRichResults", 1);

  let provider = registerBasicTestProvider(results);
  let context = createContext(undefined, { providers: [provider.name] });
  await check_results({
    context,
    matches: results,
  });

  UrlbarPrefs.clear("maxRichResults");
});

// When `maxRichResults` is zero and there are no suggested-index results, the
// heuristic should not be included.
add_task(async function roomForHeuristic_maxRichResultsZero() {
  let results = [
    Object.assign(
      new UrlbarResult(
        UrlbarUtils.RESULT_TYPE.URL,
        UrlbarUtils.RESULT_SOURCE.HISTORY,
        { url: "http://example.com/heuristic" }
      ),
      { heuristic: true }
    ),
  ];

  UrlbarPrefs.set("maxRichResults", 0);

  let provider = registerBasicTestProvider(results);
  let context = createContext(undefined, { providers: [provider.name] });
  await check_results({
    context,
    matches: [],
  });

  UrlbarPrefs.clear("maxRichResults");
});

// When `maxRichResults` is zero and suggested-index results are present,
// neither the heuristic nor the suggested-index results should be included.
add_task(async function roomForHeuristic_maxRichResultsZero_suggestedIndex() {
  let results = [
    Object.assign(
      new UrlbarResult(
        UrlbarUtils.RESULT_TYPE.URL,
        UrlbarUtils.RESULT_SOURCE.HISTORY,
        { url: "http://example.com/heuristic" }
      ),
      { heuristic: true }
    ),
    Object.assign(
      new UrlbarResult(
        UrlbarUtils.RESULT_TYPE.URL,
        UrlbarUtils.RESULT_SOURCE.HISTORY,
        { url: "http://example.com/suggestedIndex" }
      ),
      { suggestedIndex: 1 }
    ),
  ];

  UrlbarPrefs.set("maxRichResults", 0);

  let provider = registerBasicTestProvider(results);
  let context = createContext(undefined, { providers: [provider.name] });
  await check_results({
    context,
    matches: [],
  });

  UrlbarPrefs.clear("maxRichResults");
});
