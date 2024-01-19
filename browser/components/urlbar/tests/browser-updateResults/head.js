/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

// The files in this directory test UrlbarView.#updateResults().

"use strict";

ChromeUtils.defineESModuleGetters(this, {
  PlacesTestUtils: "resource://testing-common/PlacesTestUtils.sys.mjs",
  UrlbarProvidersManager: "resource:///modules/UrlbarProvidersManager.sys.mjs",
  UrlbarResult: "resource:///modules/UrlbarResult.sys.mjs",
  UrlbarUtils: "resource:///modules/UrlbarUtils.sys.mjs",
  UrlbarView: "resource:///modules/UrlbarView.sys.mjs",
});

ChromeUtils.defineLazyGetter(this, "UrlbarTestUtils", () => {
  const { UrlbarTestUtils: module } = ChromeUtils.importESModule(
    "resource://testing-common/UrlbarTestUtils.sys.mjs"
  );
  module.init(this);
  return module;
});

add_setup(async function headInit() {
  await PlacesUtils.history.clear();
  await PlacesUtils.bookmarks.eraseEverything();

  await SpecialPowers.pushPrefEnv({
    set: [
      // Make absolutely sure the panel stays open during the test. There are
      // spurious blurs on WebRender TV tests as the test starts that cause the
      // panel to close and the query to be canceled, resulting in intermittent
      // failures without this.
      ["ui.popup.disable_autohide", true],

      // Make sure maxRichResults is 10 for sanity.
      ["browser.urlbar.maxRichResults", 10],
    ],
  });

  // Increase the timeout of the remove-stale-rows timer so that it doesn't
  // interfere with the tests.
  let originalRemoveStaleRowsTimeout = UrlbarView.removeStaleRowsTimeout;
  UrlbarView.removeStaleRowsTimeout = 30000;
  registerCleanupFunction(() => {
    UrlbarView.removeStaleRowsTimeout = originalRemoveStaleRowsTimeout;
  });
});

/**
 * A test provider that doesn't finish startQuery() until `finishQueryPromise`
 * is resolved.
 */
class DelayingTestProvider extends UrlbarTestUtils.TestProvider {
  finishQueryPromise = null;
  async startQuery(context, addCallback) {
    for (let result of this._results) {
      addCallback(this, result);
    }
    await this.finishQueryPromise;
  }
}

/**
 * Makes a result with a suggested index.
 *
 * @param {number} suggestedIndex
 *   The preferred index of the result.
 * @param {number} resultSpan
 *   The result will have this span.
 * @returns {UrlbarResult}
 */
function makeSuggestedIndexResult(suggestedIndex, resultSpan = 1) {
  return Object.assign(
    new UrlbarResult(
      UrlbarUtils.RESULT_TYPE.URL,
      UrlbarUtils.RESULT_SOURCE.HISTORY,
      {
        url: "http://example.com/si",
        displayUrl: "http://example.com/si",
        title: "suggested index",
        helpUrl: "http://example.com/",
        isBlockable: true,
        blockL10n: { id: "urlbar-result-menu-remove-from-history" },
      }
    ),
    { suggestedIndex, resultSpan }
  );
}

/**
 * Makes an array of results for the suggestedIndex tests. The array will
 * include a heuristic followed by the specified results.
 *
 * @param {object} options
 *   The options object
 * @param {number} [options.count]
 *   The number of results to return other than the heuristic. This and
 *   `type` must be given together.
 * @param {UrlbarUtils.RESULT_TYPE} [options.type]
 *   The type of results to return other than the heuristic. This and `count`
 *   must be given together.
 * @param {Array} [options.specs]
 *   If you want a mix of result types instead of only one type, then use this
 *   param instead of `count` and `type`. Each item in this array must be an
 *   object with the following properties:
 *   {number} count
 *     The number of results to return for the given `type`.
 *   {UrlbarUtils.RESULT_TYPE} type
 *     The type of results.
 * @returns {Array}
 *   An array of results.
 */
function makeProviderResults({ count = 0, type = undefined, specs = [] }) {
  if (count) {
    specs.push({ count, type });
  }

  let query = "test";
  let results = [
    Object.assign(
      new UrlbarResult(
        UrlbarUtils.RESULT_TYPE.SEARCH,
        UrlbarUtils.RESULT_SOURCE.SEARCH,
        {
          query,
          engine: Services.search.defaultEngine.name,
        }
      ),
      { heuristic: true }
    ),
  ];

  for (let { count: specCount, type: specType } of specs) {
    for (let i = 0; i < specCount; i++) {
      let str = `${query} ${results.length}`;
      switch (specType) {
        case UrlbarUtils.RESULT_TYPE.SEARCH:
          results.push(
            new UrlbarResult(
              UrlbarUtils.RESULT_TYPE.SEARCH,
              UrlbarUtils.RESULT_SOURCE.SEARCH,
              {
                query,
                suggestion: str,
                lowerCaseSuggestion: str.toLowerCase(),
                engine: Services.search.defaultEngine.name,
              }
            )
          );
          break;
        case UrlbarUtils.RESULT_TYPE.URL:
          results.push(
            new UrlbarResult(
              UrlbarUtils.RESULT_TYPE.URL,
              UrlbarUtils.RESULT_SOURCE.HISTORY,
              {
                url: "http://example.com/" + i,
                displayUrl: "http://example.com/" + i,
                title: str,
                helpUrl: "http://example.com/",
                isBlockable: true,
                blockL10n: { id: "urlbar-result-menu-remove-from-history" },
              }
            )
          );
          break;
        default:
          throw new Error(`Unsupported makeProviderResults type: ${specType}`);
      }
    }
  }

  return results;
}

let gSuggestedIndexTaskIndex = 0;

/**
 * Adds a suggestedIndex test task. See doSuggestedIndexTest() for params.
 *
 * @param {object} options
 *   See doSuggestedIndexTest().
 */
function add_suggestedIndex_task(options) {
  if (!gSuggestedIndexTaskIndex) {
    initSuggestedIndexTest();
  }
  let testIndex = gSuggestedIndexTaskIndex++;
  let testName = "test_" + testIndex;
  let testDesc = JSON.stringify(options);
  let func = async () => {
    info(`Running task at index ${testIndex}: ${testDesc}`);
    await doSuggestedIndexTest(options);
  };
  Object.defineProperty(func, "name", { value: testName });
  add_task(func);
}

/**
 * Initializes suggestedIndex tests. You don't normally need to call this from
 * your test because add_suggestedIndex_task() calls it automatically.
 */
function initSuggestedIndexTest() {
  // These tests can time out on Mac TV WebRender just because they do so much,
  // so request a longer timeout.
  if (AppConstants.platform == "macosx") {
    requestLongerTimeout(3);
  }
  registerCleanupFunction(() => {
    gSuggestedIndexTaskIndex = 0;
  });
}

/**
 * @typedef {object} SuggestedIndexTestOptions
 * @property {number} [otherCount]
 *   The number of results other than the heuristic and suggestedIndex results
 *   that the provider should return for search 1. This and `otherType` must be
 *   given together.
 * @property {UrlbarUtils.RESULT_TYPE} [otherType]
 *   The type of results other than the heuristic and suggestedIndex results
 *   that the provider should return for search 1. This and `otherCount` must be
 *   given together.
 * @property {Array} [other]
 *   If you want the provider to return a mix of result types instead of only
 *   one type, then use this param instead of `otherCount` and `otherType`. Each
 *   item in this array must be an object with the following properties:
 *   {number} count
 *     The number of results to return for the given `type`.
 *   {UrlbarUtils.RESULT_TYPE} type
 *     The type of results.
 * @property {number} viewCount
 *   The total number of results expected in the view after search 1 finishes,
 *   including the heuristic and suggestedIndex results.
 * @param {number} [suggestedIndex]
 *   If given, the provider will return a result with this suggested index for
 *   search 1.
 * @property {number} [resultSpan]
 *   If this and `search1.suggestedIndex` are given, then the suggestedIndex
 *   result for search 1 will have this resultSpan.
 * @property {Array} [suggestedIndexes]
 *   If you want the provider to return more than one suggestedIndex result for
 *   search 1, then use this instead of `search1.suggestedIndex`. Each item in
 *   this array must be one of the following:
 *     suggestedIndex value
 *     [suggestedIndex, resultSpan] tuple
 */

/**
 * Runs a suggestedIndex test. Performs two searches and checks the results just
 * after the view update and after the second search finishes. The caller is
 * responsible for passing in a description of what the rows should look like
 * just after the view update finishes but before the second search finishes,
 * i.e., before stale rows are removed and hidden rows are shown -- this is the
 * `duringUpdate` param. The important thing this checks is that the rows with
 * suggested indexes don't move around or appear in the wrong places.
 *
 * @param {object} options
 *   The options object
 * @param {SuggestedIndexTestOptions} options.search1
 *   The first search options object
 * @param {SuggestedIndexTestOptions} options.search2
 *   This object has the same properties as the `search1` object but it applies
 *   to the second search.
 * @param {Array<{ count: number, type: UrlbarUtils.RESULT_TYPE, suggestedIndex: ?number, stale: ?boolean, hidden: ?boolean }>} options.duringUpdate
 *   An array of expected row states during the view update. Each item in the
 *   array must be an object with the following properties:
 *   {number} count
 *     The number of rows in the view to which this row state object applies.
 *   {UrlbarUtils.RESULT_TYPE} type
 *     The expected type of the rows.
 *   {number} [suggestedIndex]
 *     The expected suggestedIndex of the row.
 *   {boolean} [stale]
 *     Whether the rows are expected to be stale. Defaults to false.
 *   {boolean} [hidden]
 *     Whether the rows are expected to be hidden. Defaults to false.
 */
async function doSuggestedIndexTest({ search1, search2, duringUpdate }) {
  // We use this test provider to test specific results. It has an Infinity
  // priority so that it provides all results in our test searches, including
  // the heuristic. That lets us avoid any potential races with the built-in
  // providers; testing them is not important here.
  let provider = new DelayingTestProvider({ priority: Infinity });
  UrlbarProvidersManager.registerProvider(provider);
  registerCleanupFunction(() => {
    UrlbarProvidersManager.unregisterProvider(provider);
  });

  // Set up the first search. First, add the non-suggestedIndex results to the
  // provider.
  provider._results = makeProviderResults({
    specs: search1.other,
    count: search1.otherCount,
    type: search1.otherType,
  });

  // Set up `suggestedIndexes`. It's an array with [suggestedIndex, resultSpan]
  // tuples.
  if (!search1.suggestedIndexes) {
    search1.suggestedIndexes = [];
  }
  search1.suggestedIndexes = search1.suggestedIndexes.map(value =>
    typeof value == "number" ? [value, 1] : value
  );
  if (typeof search1.suggestedIndex == "number") {
    search1.suggestedIndexes.push([
      search1.suggestedIndex,
      search1.resultSpan || 1,
    ]);
  }

  // Add the suggestedIndex results to the provider.
  for (let [suggestedIndex, resultSpan] of search1.suggestedIndexes) {
    provider._results.push(
      makeSuggestedIndexResult(suggestedIndex, resultSpan)
    );
  }

  // Do the first search.
  provider.finishQueryPromise = Promise.resolve();
  await UrlbarTestUtils.promiseAutocompleteResultPopup({
    window,
    value: "test",
  });

  // Sanity check the results.
  Assert.equal(
    UrlbarTestUtils.getResultCount(window),
    search1.viewCount,
    "Row count after first search"
  );
  for (let [suggestedIndex, resultSpan] of search1.suggestedIndexes) {
    let index =
      suggestedIndex >= 0
        ? Math.min(search1.viewCount - 1, suggestedIndex)
        : Math.max(0, search1.viewCount + suggestedIndex);
    let result = await UrlbarTestUtils.getDetailsOfResultAt(window, index);
    Assert.equal(
      result.element.row.result.suggestedIndex,
      suggestedIndex,
      "suggestedIndex after first search"
    );
    Assert.equal(
      UrlbarUtils.getSpanForResult(result.element.row.result),
      resultSpan,
      "resultSpan after first search"
    );
  }

  // Set up the second search. First, add the non-suggestedIndex results to the
  // provider.
  provider._results = makeProviderResults({
    specs: search2.other,
    count: search2.otherCount,
    type: search2.otherType,
  });

  // Set up `suggestedIndexes`. It's an array with [suggestedIndex, resultSpan]
  // tuples.
  if (!search2.suggestedIndexes) {
    search2.suggestedIndexes = [];
  }
  search2.suggestedIndexes = search2.suggestedIndexes.map(value =>
    typeof value == "number" ? [value, 1] : value
  );
  if (typeof search2.suggestedIndex == "number") {
    search2.suggestedIndexes.push([
      search2.suggestedIndex,
      search2.resultSpan || 1,
    ]);
  }

  // Add the suggestedIndex results to the provider.
  for (let [suggestedIndex, resultSpan] of search2.suggestedIndexes) {
    provider._results.push(
      makeSuggestedIndexResult(suggestedIndex, resultSpan)
    );
  }

  let rowCountDuringUpdate = duringUpdate.reduce(
    (count, rowState) => count + rowState.count,
    0
  );

  // Don't allow the search to finish until we check the updated rows. We'll
  // accomplish that by adding a mutation observer to observe completion of the
  // update and delaying resolving the provider's finishQueryPromise.
  let mutationPromise = new Promise(resolve => {
    let lastRowState = duringUpdate[duringUpdate.length - 1];
    let observer = new MutationObserver(mutations => {
      observer.disconnect();
      resolve();
    });
    if (lastRowState.stale) {
      // The last row during the update is expected to become stale. Wait for
      // the stale attribute to be set on it. We'll actually just wait for any
      // attribute.
      let { children } = UrlbarTestUtils.getResultsContainer(window);
      observer.observe(children[children.length - 1], { attributes: true });
    } else if (search1.viewCount == rowCountDuringUpdate) {
      // No rows are expected to be added during the view update, so it must be
      // the case that some rows will be updated for results in the the second
      // search. Wait for any change to an existing row.
      observer.observe(UrlbarTestUtils.getResultsContainer(window), {
        subtree: true,
        attributes: true,
        characterData: true,
      });
    } else {
      // Rows are expected to be added during the update. Wait for them.
      observer.observe(UrlbarTestUtils.getResultsContainer(window), {
        childList: true,
      });
    }
  });

  // Now do the second search but don't wait for it to finish.
  let resolveQuery;
  provider.finishQueryPromise = new Promise(
    resolve => (resolveQuery = resolve)
  );
  let queryPromise = UrlbarTestUtils.promiseAutocompleteResultPopup({
    window,
    value: "test",
  });

  // Wait for the update to finish.
  await mutationPromise;

  // Check the rows. We can't use UrlbarTestUtils.getDetailsOfResultAt() here
  // because it waits for the search to finish.
  Assert.equal(
    UrlbarTestUtils.getResultCount(window),
    rowCountDuringUpdate,
    "Row count during update"
  );
  let rows = UrlbarTestUtils.getResultsContainer(window).children;
  let rowIndex = 0;
  for (let rowState of duringUpdate) {
    for (let i = 0; i < rowState.count; i++) {
      let row = rows[rowIndex];

      // type
      if ("type" in rowState) {
        Assert.equal(
          row.result.type,
          rowState.type,
          `Type at index ${rowIndex} during update`
        );
      }

      // suggestedIndex
      if ("suggestedIndex" in rowState) {
        Assert.ok(
          row.result.hasSuggestedIndex,
          `Row at index ${rowIndex} has suggestedIndex during update`
        );
        Assert.equal(
          row.result.suggestedIndex,
          rowState.suggestedIndex,
          `suggestedIndex at index ${rowIndex} during update`
        );
      } else {
        Assert.ok(
          !row.result.hasSuggestedIndex,
          `Row at index ${rowIndex} does not have suggestedIndex during update`
        );
      }

      // resultSpan
      Assert.equal(
        UrlbarUtils.getSpanForResult(row.result),
        rowState.resultSpan || 1,
        `resultSpan at index ${rowIndex} during update`
      );

      // stale
      if (rowState.stale) {
        Assert.equal(
          row.getAttribute("stale"),
          "true",
          `Row at index ${rowIndex} is stale during update`
        );
      } else {
        Assert.ok(
          !row.hasAttribute("stale"),
          `Row at index ${rowIndex} is not stale during update`
        );
      }

      // visible
      Assert.equal(
        BrowserTestUtils.isVisible(row),
        !rowState.hidden,
        `Visible at index ${rowIndex} during update`
      );

      rowIndex++;
    }
  }

  // Finish the search.
  resolveQuery();
  await queryPromise;

  // Check the rows now that the second search is done. First, build a map from
  // real indexes to suggested index. e.g., if a suggestedIndex = -1, then the
  // real index = the result count - 1.
  let suggestedIndexesByRealIndex = new Map();
  for (let [suggestedIndex, resultSpan] of search2.suggestedIndexes) {
    let realIndex =
      suggestedIndex >= 0
        ? Math.min(suggestedIndex, search2.viewCount - 1)
        : Math.max(0, search2.viewCount + suggestedIndex);
    suggestedIndexesByRealIndex.set(realIndex, [suggestedIndex, resultSpan]);
  }

  Assert.equal(
    UrlbarTestUtils.getResultCount(window),
    search2.viewCount,
    "Row count after update"
  );
  for (let i = 0; i < search2.viewCount; i++) {
    let result = rows[i].result;
    let tuple = suggestedIndexesByRealIndex.get(i);
    if (tuple) {
      let [suggestedIndex, resultSpan] = tuple;
      Assert.ok(
        result.hasSuggestedIndex,
        `Row at index ${i} has suggestedIndex after update`
      );
      Assert.equal(
        result.suggestedIndex,
        suggestedIndex,
        `suggestedIndex at index ${i} after update`
      );
      Assert.equal(
        UrlbarUtils.getSpanForResult(result),
        resultSpan,
        `resultSpan at index ${i} after update`
      );
    } else {
      Assert.ok(
        !result.hasSuggestedIndex,
        `Row at index ${i} does not have suggestedIndex after update`
      );
    }
  }

  await UrlbarTestUtils.promisePopupClose(window);
  gURLBar.handleRevert();
  UrlbarProvidersManager.unregisterProvider(provider);
}
