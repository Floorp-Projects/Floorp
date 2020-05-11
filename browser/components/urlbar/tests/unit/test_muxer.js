/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

add_task(async function setup() {
  // These two lines are necessary because UrlbarMuxerUnifiedComplete.sort calls
  // PlacesSearchAutocompleteProvider.parseSubmissionURL, so we need engines and
  // PlacesSearchAutocompleteProvider.
  await AddonTestUtils.promiseStartupManager();
  await PlacesSearchAutocompleteProvider.ensureReady();
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

  let providerName = registerBasicTestProvider(matches);
  let context = createContext(undefined, { providers: [providerName] });
  let controller = UrlbarTestUtils.newMockController();
  /**
   * A test muxer.
   */
  class TestMuxer extends UrlbarMuxer {
    get name() {
      return "TestMuxer";
    }
    sort(queryContext) {
      queryContext.results.sort((a, b) => {
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

  let providerName = registerBasicTestProvider(matches);
  let context = createContext(undefined, {
    providers: [providerName],
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

  let provider1Name = registerBasicTestProvider(matches1);
  let provider2Name = registerBasicTestProvider(matches2);

  let context = createContext(undefined, {
    providers: [provider1Name, provider2Name],
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
      UrlbarUtils.RESULT_SOURCE.SEARCH,
      {
        engine: "mozSearch",
        query: "moz",
        suggestion: "mozilla",
      }
    ),
    new UrlbarResult(
      UrlbarUtils.RESULT_TYPE.SEARCH,
      UrlbarUtils.RESULT_SOURCE.SEARCH,
      {
        engine: "mozSearch",
        query: "moz",
        keywordOffer: UrlbarUtils.KEYWORD_OFFER.SHOW,
        keyword: "@moz",
      }
    ),
    new UrlbarResult(
      UrlbarUtils.RESULT_TYPE.URL,
      UrlbarUtils.RESULT_SOURCE.HISTORY,
      { url: "http://mozilla.org/c" }
    ),
  ];

  let providerName = registerBasicTestProvider(matches);

  let context = createContext(undefined, {
    providers: [providerName],
  });
  let controller = UrlbarTestUtils.newMockController();

  info("Check results, the order should be: moz, a, b, @moz, c");
  await UrlbarProvidersManager.startQuery(context, controller);
  Assert.deepEqual(context.results, [
    matches[2],
    matches[0],
    matches[1],
    matches[3],
    matches[4],
  ]);
});
