/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

// Tests results with suggestedIndex and resultSpan.

"use strict";

const MAX_RESULTS = 10;

add_task(async function suggestedIndex() {
  let tests = [
    // no result spans > 1
    {
      desc: "{ suggestedIndex: 0 }",
      suggestedIndexes: [0],
      expected: indexes([10, 1], [0, 9]),
    },
    {
      desc: "{ suggestedIndex: 1 }",
      suggestedIndexes: [1],
      expected: indexes([0, 1], [10, 1], [1, 8]),
    },
    {
      desc: "{ suggestedIndex: -1 }",
      suggestedIndexes: [-1],
      expected: indexes([0, 9], [10, 1]),
    },
    {
      desc: "{ suggestedIndex: -2 }",
      suggestedIndexes: [-2],
      expected: indexes([0, 8], [10, 1], [8, 1]),
    },
    {
      desc: "{ suggestedIndex: 0 }, { suggestedIndex: -1 }",
      suggestedIndexes: [0, -1],
      expected: indexes([10, 1], [0, 8], [11, 1]),
    },
    {
      desc: "{ suggestedIndex: 1 }, { suggestedIndex: -1 }",
      suggestedIndexes: [1, -1],
      expected: indexes([0, 1], [10, 1], [1, 7], [11, 1]),
    },
    {
      desc: "{ suggestedIndex: 1 }, { suggestedIndex: -2 }",
      suggestedIndexes: [1, -2],
      expected: indexes([0, 1], [10, 1], [1, 6], [11, 1], [7, 1]),
    },
    {
      desc: "{ suggestedIndex: 0 }, resultCount < max",
      suggestedIndexes: [0],
      resultCount: 5,
      expected: indexes([5, 1], [0, 5]),
    },
    {
      desc: "{ suggestedIndex: 1 }, resultCount < max",
      suggestedIndexes: [1],
      resultCount: 5,
      expected: indexes([0, 1], [5, 1], [1, 4]),
    },
    {
      desc: "{ suggestedIndex: -1 }, resultCount < max",
      suggestedIndexes: [-1],
      resultCount: 5,
      expected: indexes([0, 5], [5, 1]),
    },
    {
      desc: "{ suggestedIndex: -2 }, resultCount < max",
      suggestedIndexes: [-2],
      resultCount: 5,
      expected: indexes([0, 4], [5, 1], [4, 1]),
    },
    {
      desc: "{ suggestedIndex: 0 }, { suggestedIndex: -1 }, resultCount < max",
      suggestedIndexes: [0, -1],
      resultCount: 5,
      expected: indexes([5, 1], [0, 5], [6, 1]),
    },
    {
      desc: "{ suggestedIndex: 1 }, { suggestedIndex: -1 }, resultCount < max",
      suggestedIndexes: [1, -1],
      resultCount: 5,
      expected: indexes([0, 1], [5, 1], [1, 4], [6, 1]),
    },
    {
      desc: "{ suggestedIndex: 0 }, { suggestedIndex: -2 }, resultCount < max",
      suggestedIndexes: [0, -2],
      resultCount: 5,
      expected: indexes([5, 1], [0, 4], [6, 1], [4, 1]),
    },
    {
      desc: "{ suggestedIndex: 1 }, { suggestedIndex: -2 }, resultCount < max",
      suggestedIndexes: [1, -2],
      resultCount: 5,
      expected: indexes([0, 1], [5, 1], [1, 3], [6, 1], [4, 1]),
    },

    // one suggestedIndex with result span > 1
    {
      desc: "{ suggestedIndex: 0, resultSpan: 2 }",
      suggestedIndexes: [0],
      spansByIndex: { 10: 2 },
      expected: indexes([10, 1], [0, 8]),
    },
    {
      desc: "{ suggestedIndex: 0, resultSpan: 3 }",
      suggestedIndexes: [0],
      spansByIndex: { 10: 3 },
      expected: indexes([10, 1], [0, 7]),
    },
    {
      desc: "{ suggestedIndex: 1, resultSpan: 2 }",
      suggestedIndexes: [1],
      spansByIndex: { 10: 2 },
      expected: indexes([0, 1], [10, 1], [1, 7]),
    },
    {
      desc: "suggestedIndex: 1, resultSpan:: 3 }",
      suggestedIndexes: [1],
      spansByIndex: { 10: 3 },
      expected: indexes([0, 1], [10, 1], [1, 6]),
    },
    {
      desc: "{ suggestedIndex: -1, resultSpan 2 }",
      suggestedIndexes: [-1],
      spansByIndex: { 10: 2 },
      expected: indexes([0, 8], [10, 1]),
    },
    {
      desc: "{ suggestedIndex: -1, resultSpan: 3 }",
      suggestedIndexes: [-1],
      spansByIndex: { 10: 3 },
      expected: indexes([0, 7], [10, 1]),
    },
    {
      desc: "{ suggestedIndex: 0, resultSpan: 2 }, { suggestedIndex: -1 }",
      suggestedIndexes: [0, -1],
      spansByIndex: { 10: 2 },
      expected: indexes([10, 1], [0, 7], [11, 1]),
    },
    {
      desc: "{ suggestedIndex: 0, resultSpan: 3 }, { suggestedIndex: -1 }",
      suggestedIndexes: [0, -1],
      spansByIndex: { 10: 3 },
      expected: indexes([10, 1], [0, 6], [11, 1]),
    },
    {
      desc: "{ suggestedIndex: 0 }, { suggestedIndex: -1, resultSpan: 2 }",
      suggestedIndexes: [0, -1],
      spansByIndex: { 11: 2 },
      expected: indexes([10, 1], [0, 7], [11, 1]),
    },
    {
      desc: "{ suggestedIndex: 0 }, { suggestedIndex: -1, resultSpan: 3 }",
      suggestedIndexes: [0, -1],
      spansByIndex: { 11: 3 },
      expected: indexes([10, 1], [0, 6], [11, 1]),
    },
    {
      desc: "{ suggestedIndex: 1, resultSpan: 2 }, { suggestedIndex: -1 }",
      suggestedIndexes: [1, -1],
      spansByIndex: { 10: 2 },
      expected: indexes([0, 1], [10, 1], [1, 6], [11, 1]),
    },
    {
      desc: "{ suggestedIndex: 1, resultSpan: 3 }, { suggestedIndex: -1 }",
      suggestedIndexes: [1, -1],
      spansByIndex: { 10: 3 },
      expected: indexes([0, 1], [10, 1], [1, 5], [11, 1]),
    },
    {
      desc: "{ suggestedIndex: 1 }, { suggestedIndex: -1, resultSpan: 2 }",
      suggestedIndexes: [1, -1],
      spansByIndex: { 11: 2 },
      expected: indexes([0, 1], [10, 1], [1, 6], [11, 1]),
    },
    {
      desc: "{ suggestedIndex: 1 }, { suggestedIndex: -1, resultSpan: 3 }",
      suggestedIndexes: [1, -1],
      spansByIndex: { 11: 3 },
      expected: indexes([0, 1], [10, 1], [1, 5], [11, 1]),
    },
    {
      desc: "{ suggestedIndex: 0, resultSpan: 2 }, { suggestedIndex: -2 }",
      suggestedIndexes: [0, -2],
      spansByIndex: { 10: 2 },
      expected: indexes([10, 1], [0, 6], [11, 1], [6, 1]),
    },
    {
      desc: "{ suggestedIndex: 0, resultSpan: 3 }, { suggestedIndex: -2 }",
      suggestedIndexes: [0, -2],
      spansByIndex: { 10: 3 },
      expected: indexes([10, 1], [0, 5], [11, 1], [5, 1]),
    },
    {
      desc: "{ suggestedIndex: 1, resultSpan: 2 }, { suggestedIndex: -2 }",
      suggestedIndexes: [1, -2],
      spansByIndex: { 10: 2 },
      expected: indexes([0, 1], [10, 1], [1, 5], [11, 1], [6, 1]),
    },
    {
      desc: "{ suggestedIndex: 1, resultSpan: 3 }, { suggestedIndex: -2 }",
      suggestedIndexes: [1, -2],
      spansByIndex: { 10: 3 },
      expected: indexes([0, 1], [10, 1], [1, 4], [11, 1], [5, 1]),
    },
    {
      desc: "{ suggestedIndex: 0 }, { suggestedIndex: -2, resultSpan: 2 }",
      suggestedIndexes: [0, -2],
      spansByIndex: { 11: 2 },
      expected: indexes([10, 1], [0, 6], [11, 1], [6, 1]),
    },
    {
      desc: "{ suggestedIndex: 0 }, { suggestedIndex: -2, resultSpan: 3 }",
      suggestedIndexes: [0, -2],
      spansByIndex: { 11: 3 },
      expected: indexes([10, 1], [0, 5], [11, 1], [5, 1]),
    },
    {
      desc: "{ suggestedIndex: 1 }, { suggestedIndex: -2, resultSpan: 2 }",
      suggestedIndexes: [1, -2],
      spansByIndex: { 11: 2 },
      expected: indexes([0, 1], [10, 1], [1, 5], [11, 1], [6, 1]),
    },
    {
      desc: "{ suggestedIndex: 1 }, { suggestedIndex: -2, resultSpan: 3 }",
      suggestedIndexes: [1, -2],
      spansByIndex: { 11: 3 },
      expected: indexes([0, 1], [10, 1], [1, 4], [11, 1], [5, 1]),
    },
    {
      desc: "{ suggestedIndex: 0, resultSpan: 2 }, resultCount < max",
      suggestedIndexes: [0],
      spansByIndex: { 5: 2 },
      resultCount: 5,
      expected: indexes([5, 1], [0, 5]),
    },
    {
      desc: "{ suggestedIndex: 1, resultSpan: 2 }, resultCount < max",
      suggestedIndexes: [1],
      spansByIndex: { 5: 2 },
      resultCount: 5,
      expected: indexes([0, 1], [5, 1], [1, 4]),
    },
    {
      desc: "{ suggestedIndex: -1, resultSpan: 2 }, resultCount < max",
      suggestedIndexes: [-1],
      spansByIndex: { 5: 2 },
      resultCount: 5,
      expected: indexes([0, 5], [5, 1]),
    },
    {
      desc: "{ suggestedIndex: -2, resultSpan: 2 }, resultCount < max",
      suggestedIndexes: [-2],
      spansByIndex: { 5: 2 },
      resultCount: 5,
      expected: indexes([0, 4], [5, 1], [4, 1]),
    },
    {
      desc: "{ suggestedIndex: 0, resultSpan: 2 }, { suggestedIndex: -1 }, resultCount < max",
      suggestedIndexes: [0, -1],
      spansByIndex: { 5: 2 },
      resultCount: 5,
      expected: indexes([5, 1], [0, 5], [6, 1]),
    },
    {
      desc: "{ suggestedIndex: 1, resultSpan: 2 }, { suggestedIndex: -1 }, resultCount < max",
      suggestedIndexes: [1, -1],
      spansByIndex: { 5: 2 },
      resultCount: 5,
      expected: indexes([0, 1], [5, 1], [1, 4], [6, 1]),
    },
    {
      desc: "{ suggestedIndex: 0, resultSpan: 2 }, { suggestedIndex: -2 }, resultCount < max",
      suggestedIndexes: [0, -2],
      spansByIndex: { 5: 2 },
      resultCount: 5,
      expected: indexes([5, 1], [0, 4], [6, 1], [4, 1]),
    },
    {
      desc: "{ suggestedIndex: 0 }, { suggestedIndex: -1, resultSpan: 2 }, resultCount < max",
      suggestedIndexes: [0, -1],
      spansByIndex: { 6: 2 },
      resultCount: 5,
      expected: indexes([5, 1], [0, 5], [6, 1]),
    },
    {
      desc: "{ suggestedIndex: 0 }, { suggestedIndex: -2, resultSpan: 2 }, resultCount < max",
      suggestedIndexes: [0, -2],
      spansByIndex: { 6: 2 },
      resultCount: 5,
      expected: indexes([5, 1], [0, 4], [6, 1], [4, 1]),
    },

    // two suggestedIndexes with result span > 1
    {
      desc: "{ suggestedIndex: 0, resultSpan: 2 }, { suggestedIndex: -1, resultSpan: 2 }",
      suggestedIndexes: [0, -1],
      spansByIndex: { 10: 2, 11: 2 },
      expected: indexes([10, 1], [0, 6], [11, 1]),
    },
    {
      desc: "{ suggestedIndex: 0, resultSpan: 3 }, { suggestedIndex: -1, resultSpan: 2 }",
      suggestedIndexes: [0, -1],
      spansByIndex: { 10: 3, 11: 2 },
      expected: indexes([10, 1], [0, 5], [11, 1]),
    },
    {
      desc: "{ suggestedIndex: 0, resultSpan: 2 }, { suggestedIndex: -1, resultSpan: 3 }",
      suggestedIndexes: [0, -1],
      spansByIndex: { 10: 2, 11: 3 },
      expected: indexes([10, 1], [0, 5], [11, 1]),
    },
    {
      desc: "{ suggestedIndex: 1, resultSpan: 2 }, { suggestedIndex: -1, resultSpan: 2 }",
      suggestedIndexes: [1, -1],
      spansByIndex: { 10: 2, 11: 2 },
      expected: indexes([0, 1], [10, 1], [1, 5], [11, 1]),
    },
    {
      desc: "{ suggestedIndex: 1, resultSpan: 3 }, { suggestedIndex: -1, resultSpan: 2 }",
      suggestedIndexes: [1, -1],
      spansByIndex: { 10: 3, 11: 2 },
      expected: indexes([0, 1], [10, 1], [1, 4], [11, 1]),
    },
    {
      desc: "{ suggestedIndex: 1, resultSpan: 2 }, { suggestedIndex: -1, resultSpan: 3 }",
      suggestedIndexes: [1, -1],
      spansByIndex: { 10: 2, 11: 3 },
      expected: indexes([0, 1], [10, 1], [1, 4], [11, 1]),
    },
    {
      desc: "{ suggestedIndex: 0, resultSpan: 2 }, { suggestedIndex: -2, resultSpan: 2 }",
      suggestedIndexes: [0, -2],
      spansByIndex: { 10: 2, 11: 2 },
      expected: indexes([10, 1], [0, 5], [11, 1], [5, 1]),
    },
    {
      desc: "{ suggestedIndex: 0, resultSpan: 3 }, { suggestedIndex: -2, resultSpan: 2 }",
      suggestedIndexes: [0, -2],
      spansByIndex: { 10: 3, 11: 2 },
      expected: indexes([10, 1], [0, 4], [11, 1], [4, 1]),
    },
    {
      desc: "{ suggestedIndex: 0, resultSpan: 2 }, { suggestedIndex: -2, resultSpan: 3 }",
      suggestedIndexes: [0, -2],
      spansByIndex: { 10: 2, 11: 3 },
      expected: indexes([10, 1], [0, 4], [11, 1], [4, 1]),
    },
    {
      desc: "{ suggestedIndex: 1, resultSpan: 2 }, { suggestedIndex: -2, resultSpan: 2 }",
      suggestedIndexes: [1, -2],
      spansByIndex: { 10: 2, 11: 2 },
      expected: indexes([0, 1], [10, 1], [1, 4], [11, 1], [5, 1]),
    },
    {
      desc: "{ suggestedIndex: 1, resultSpan: 3 }, { suggestedIndex: -2, resultSpan: 2 }",
      suggestedIndexes: [1, -2],
      spansByIndex: { 10: 3, 11: 2 },
      expected: indexes([0, 1], [10, 1], [1, 3], [11, 1], [4, 1]),
    },
    {
      desc: "{ suggestedIndex: 1, resultSpan: 2 }, { suggestedIndex: -2, resultSpan: 3 }",
      suggestedIndexes: [1, -2],
      spansByIndex: { 10: 2, 11: 3 },
      expected: indexes([0, 1], [10, 1], [1, 3], [11, 1], [4, 1]),
    },
    {
      desc: "{ suggestedIndex: 0, resultSpan: 2 }, { suggestedIndex: -1, resultSpan: 2 }, resultCount < max",
      suggestedIndexes: [0, -1],
      spansByIndex: { 5: 2, 6: 2 },
      resultCount: 5,
      expected: indexes([5, 1], [0, 5], [6, 1]),
    },
    {
      desc: "{ suggestedIndex: 1, resultSpan: 2 }, { suggestedIndex: -1, resultSpan: 2 }, resultCount < max",
      suggestedIndexes: [1, -1],
      spansByIndex: { 5: 2, 6: 2 },
      resultCount: 5,
      expected: indexes([0, 1], [5, 1], [1, 4], [6, 1]),
    },
    {
      desc: "{ suggestedIndex: 0, resultSpan: 2 }, { suggestedIndex: -2, resultSpan: 2 }, resultCount < max",
      suggestedIndexes: [0, -2],
      spansByIndex: { 5: 2, 6: 2 },
      resultCount: 5,
      expected: indexes([5, 1], [0, 4], [6, 1], [4, 1]),
    },

    // one suggestedIndex plus other result with resultSpan > 1
    {
      desc: "{ suggestedIndex: 0 }, { resultSpan: 2 } A",
      suggestedIndexes: [0],
      spansByIndex: { 0: 2 },
      expected: indexes([10, 1], [0, 8]),
    },
    {
      desc: "{ suggestedIndex: 0 }, { resultSpan: 2 } B",
      suggestedIndexes: [0],
      spansByIndex: { 8: 2 },
      expected: indexes([10, 1], [0, 8]),
    },
    {
      desc: "{ suggestedIndex: 0 }, { resultSpan: 2 } C",
      suggestedIndexes: [0],
      spansByIndex: { 9: 2 },
      expected: indexes([10, 1], [0, 9]),
    },
    {
      desc: "{ suggestedIndex: 1 }, { resultSpan: 2 } A",
      suggestedIndexes: [1],
      spansByIndex: { 0: 2 },
      expected: indexes([0, 1], [10, 1], [1, 7]),
    },
    {
      desc: "{ suggestedIndex: 1 }, { resultSpan: 2 } B",
      suggestedIndexes: [1],
      spansByIndex: { 8: 2 },
      expected: indexes([0, 1], [10, 1], [1, 7]),
    },
    {
      desc: "{ suggestedIndex: -1 }, { resultSpan: 2 }",
      suggestedIndexes: [-1],
      spansByIndex: { 0: 2 },
      expected: indexes([0, 8], [10, 1]),
    },
    {
      desc: "{ suggestedIndex: -2 }, { resultSpan: 2 }",
      suggestedIndexes: [-2],
      spansByIndex: { 0: 2 },
      expected: indexes([0, 7], [10, 1], [7, 1]),
    },

    // miscellaneous
    {
      desc: "no suggestedIndex, last result has resultSpan = 2",
      suggestedIndexes: [],
      spansByIndex: { 9: 2 },
      expected: indexes([0, 9]),
    },
    {
      desc: "{ suggestedIndex: -1 }, last result has resultSpan = 2",
      suggestedIndexes: [-1],
      spansByIndex: { 9: 2 },
      expected: indexes([0, 9], [10, 1]),
    },
    {
      desc: "no suggestedIndex, index 8 result has resultSpan = 2",
      suggestedIndexes: [],
      spansByIndex: { 8: 2 },
      expected: indexes([0, 9]),
    },
    {
      desc: "{ suggestedIndex: -1 }, index 8 result has resultSpan = 2",
      suggestedIndexes: [-1],
      spansByIndex: { 8: 2 },
      expected: indexes([0, 8], [10, 1]),
    },
    {
      desc: "{ suggestedIndex: 0, maxRichResults: 0 }",
      maxRichResults: 0,
      suggestedIndexes: [0],
      expected: [],
    },
    {
      desc: "{ suggestedIndex: 1, maxRichResults: 0 }",
      maxRichResults: 0,
      suggestedIndexes: [1],
      expected: [],
    },
    {
      desc: "{ suggestedIndex: -1, maxRichResults: 0 }",
      maxRichResults: 0,
      suggestedIndexes: [-1],
      expected: [],
    },
    {
      desc: "{ suggestedIndex: 0, maxRichResults: 1 }",
      maxRichResults: 1,
      suggestedIndexes: [0],
      expected: indexes([10, 1]),
    },
    {
      desc: "{ suggestedIndex: 1, maxRichResults: 1 }",
      maxRichResults: 1,
      suggestedIndexes: [1],
      expected: indexes([10, 1]),
    },
    {
      desc: "{ suggestedIndex: -1, maxRichResults: 1 }",
      maxRichResults: 1,
      suggestedIndexes: [-1],
      expected: indexes([10, 1]),
    },
  ];

  for (let test of tests) {
    info("Running test: " + JSON.stringify(test));
    await doSuggestedIndexTest(test);
  }
});

/**
 * Sets up a provider with some results with suggested indexes and result spans,
 * performs a search, and then checks the results.
 *
 * @param {object} options
 *   Options for the test.
 * @param {Array} options.suggestedIndexes
 *   For each of the indexes in this array, a new result with the given
 *   suggestedIndex will be returned by the provider.
 * @param {Array} options.expected
 *   The indexes of the expected results within the array of results returned by
 *   the provider.
 * @param {object} [options.spansByIndex]
 *   Maps indexes within the array of results returned by the provider to result
 *   spans to set on those results.
 * @param {number} [options.resultCount]
 *   Aside from the results with suggested indexes, this is the number of
 *   results that the provider will return.
 * @param {number} [options.maxRichResults]
 *   The `maxRichResults` pref will be set to this value.
 */
async function doSuggestedIndexTest({
  suggestedIndexes,
  expected,
  spansByIndex = {},
  resultCount = MAX_RESULTS,
  maxRichResults = MAX_RESULTS,
}) {
  // Make resultCount history results.
  let results = [];
  for (let i = 0; i < resultCount; i++) {
    results.push(
      new UrlbarResult(
        UrlbarUtils.RESULT_TYPE.URL,
        UrlbarUtils.RESULT_SOURCE.HISTORY,
        {
          url: "http://example.com/" + i,
        }
      )
    );
  }

  // Make the suggested-index results.
  for (let suggestedIndex of suggestedIndexes) {
    results.push(
      Object.assign(
        new UrlbarResult(
          UrlbarUtils.RESULT_TYPE.URL,
          UrlbarUtils.RESULT_SOURCE.HISTORY,
          {
            url: "http://example.com/si " + suggestedIndex,
          }
        ),
        { suggestedIndex }
      )
    );
  }

  // Set resultSpan on each result as indicated by spansByIndex.
  for (let [index, span] of Object.entries(spansByIndex)) {
    results[index].resultSpan = span;
  }

  // Set up the provider, etc.
  UrlbarPrefs.set("maxRichResults", maxRichResults);
  let provider = registerBasicTestProvider(results);
  let context = createContext(undefined, { providers: [provider.name] });
  let controller = UrlbarTestUtils.newMockController();

  // Finally, search and check the results.
  let expectedResults = expected.map(i => results[i]);
  await UrlbarProvidersManager.startQuery(context, controller);
  Assert.deepEqual(context.results, expectedResults);
}

/**
 * Helper that generates an array of indexes.  Pass in [index, length] tuples.
 * Each tuple will produce the indexes starting from `index` to `index + length`
 * (not including the index at `index + length`).
 *
 * Examples:
 *
 *   indexes([0, 5]) => [0, 1, 2, 3, 4]
 *   indexes([0, 1], [4, 3], [8, 2]) => [0, 4, 5, 6, 8, 9]
 *
 * @param {Array} pairs
 *   [index, length] tuples as described above.
 * @returns {Array}
 *   An array of indexes.
 */
function indexes(...pairs) {
  return pairs.reduce((indexesArray, [start, len]) => {
    for (let i = start; i < start + len; i++) {
      indexesArray.push(i);
    }
    return indexesArray;
  }, []);
}
