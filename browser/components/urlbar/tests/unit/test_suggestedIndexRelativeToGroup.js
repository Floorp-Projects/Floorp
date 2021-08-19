/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

// Tests results with `suggestedIndex` and `isSuggestedIndexRelativeToGroup`.

"use strict";

const MAX_RESULTS = 10;
const RESULT_GROUPS_PREF = "browser.urlbar.resultGroups";
const MAX_RICH_RESULTS_PREF = "browser.urlbar.maxRichResults";

add_task(async function test() {
  // Set a specific maxRichResults for sanity's sake.
  Services.prefs.setIntPref(MAX_RICH_RESULTS_PREF, MAX_RESULTS);

  // Set the result groups we'll test with.
  setResultGroups({
    children: [
      {
        group: UrlbarUtils.RESULT_GROUP.GENERAL_PARENT,
        flexChildren: true,
        children: [
          {
            flex: 1,
            group: UrlbarUtils.RESULT_GROUP.FORM_HISTORY,
          },
          {
            flex: 1,
            group: UrlbarUtils.RESULT_GROUP.GENERAL,
          },
          {
            flex: 1,
            group: UrlbarUtils.RESULT_GROUP.REMOTE_SUGGESTION,
          },
        ],
      },
    ],
  });

  // Create the non-suggestedIndex results we'll use in all tests. For each test
  // we'll copy this array and append suggestedIndex results.
  let basicResults = [
    ...makeHistoryResults(),
    ...makeFormHistoryResults(),
    ...makeRemoteSuggestionResults(),
  ];

  // Test cases follow. Each object in `tests` has the following properties:
  //
  // * {string} desc
  // * {object} suggestedIndexResults
  //   Describes the suggestedIndex results the test provider should return.
  //   Properties:
  //     * {number} suggestedIndex
  //     * {UrlbarUtils.RESULT_GROUP} group
  //       This will force the result to have the given group.
  // * {array} expected
  //   Describes the expected results the muxer should return, i.e., the search
  //   results. Each object in the array describes a slice of expected results.
  //   The size of the slice is defined by the `count` property.
  //     * {UrlbarUtils.RESULT_GROUP} group
  //       The expected group of the results in the slice.
  //     * {number} count
  //       The number of results in the slice.
  //     * {number} [offset]
  //       Can be used to offset the starting index of the slice in the results.
  let tests = [
    {
      desc: "First result in GENERAL",
      suggestedIndexResults: [
        {
          suggestedIndex: 0,
          group: UrlbarUtils.RESULT_GROUP.GENERAL,
        },
      ],
      expected: [
        {
          group: UrlbarUtils.RESULT_GROUP.FORM_HISTORY,
          count: 4,
        },
        {
          group: UrlbarUtils.RESULT_GROUP.GENERAL,
          suggestedIndex: 0,
        },
        {
          group: UrlbarUtils.RESULT_GROUP.GENERAL,
          count: 2,
        },
        {
          group: UrlbarUtils.RESULT_GROUP.REMOTE_SUGGESTION,
          // The muxer will remove the first 4 remote suggestions because they
          // dupe the earlier form history.
          offset: 4,
          count: 3,
        },
      ],
    },

    {
      desc: "Last result in GENERAL",
      suggestedIndexResults: [
        {
          suggestedIndex: -1,
          group: UrlbarUtils.RESULT_GROUP.GENERAL,
        },
      ],
      expected: [
        {
          group: UrlbarUtils.RESULT_GROUP.FORM_HISTORY,
          count: 4,
        },
        {
          group: UrlbarUtils.RESULT_GROUP.GENERAL,
          count: 2,
        },
        {
          group: UrlbarUtils.RESULT_GROUP.GENERAL,
          suggestedIndex: -1,
        },
        {
          group: UrlbarUtils.RESULT_GROUP.REMOTE_SUGGESTION,
          // The muxer will remove the first 4 remote suggestions because they
          // dupe the earlier form history.
          offset: 4,
          count: 3,
        },
      ],
    },

    {
      desc: "First result in GENERAL_PARENT",
      suggestedIndexResults: [
        {
          suggestedIndex: 0,
          group: UrlbarUtils.RESULT_GROUP.GENERAL_PARENT,
        },
      ],
      expected: [
        {
          group: UrlbarUtils.RESULT_GROUP.GENERAL_PARENT,
          suggestedIndex: 0,
        },
        {
          group: UrlbarUtils.RESULT_GROUP.FORM_HISTORY,
          count: 3,
        },
        {
          group: UrlbarUtils.RESULT_GROUP.GENERAL,
          count: 3,
        },
        {
          group: UrlbarUtils.RESULT_GROUP.REMOTE_SUGGESTION,
          // The muxer will remove the first 3 remote suggestions because they
          // dupe the earlier form history.
          offset: 3,
          count: 3,
        },
      ],
    },

    {
      desc: "Last result in GENERAL_PARENT",
      suggestedIndexResults: [
        {
          suggestedIndex: -1,
          group: UrlbarUtils.RESULT_GROUP.GENERAL_PARENT,
        },
      ],
      expected: [
        {
          group: UrlbarUtils.RESULT_GROUP.FORM_HISTORY,
          count: 3,
        },
        {
          group: UrlbarUtils.RESULT_GROUP.GENERAL,
          count: 3,
        },
        {
          group: UrlbarUtils.RESULT_GROUP.REMOTE_SUGGESTION,
          // The muxer will remove the first 3 remote suggestions because they
          // dupe the earlier form history.
          offset: 3,
          count: 3,
        },
        {
          group: UrlbarUtils.RESULT_GROUP.GENERAL_PARENT,
          suggestedIndex: -1,
        },
      ],
    },

    {
      desc: "First and last results in GENERAL",
      suggestedIndexResults: [
        {
          suggestedIndex: 0,
          group: UrlbarUtils.RESULT_GROUP.GENERAL,
        },
        {
          suggestedIndex: -1,
          group: UrlbarUtils.RESULT_GROUP.GENERAL,
        },
      ],
      expected: [
        {
          group: UrlbarUtils.RESULT_GROUP.FORM_HISTORY,
          count: 4,
        },
        {
          group: UrlbarUtils.RESULT_GROUP.GENERAL,
          suggestedIndex: 0,
        },
        {
          group: UrlbarUtils.RESULT_GROUP.GENERAL,
          count: 1,
        },
        {
          group: UrlbarUtils.RESULT_GROUP.GENERAL,
          suggestedIndex: -1,
        },
        {
          group: UrlbarUtils.RESULT_GROUP.REMOTE_SUGGESTION,
          // The muxer will remove the first 4 remote suggestions because they
          // dupe the earlier form history.
          offset: 4,
          count: 3,
        },
      ],
    },

    {
      desc: "First and last results in GENERAL_PARENT",
      suggestedIndexResults: [
        {
          suggestedIndex: 0,
          group: UrlbarUtils.RESULT_GROUP.GENERAL_PARENT,
        },
        {
          suggestedIndex: -1,
          group: UrlbarUtils.RESULT_GROUP.GENERAL_PARENT,
        },
      ],
      expected: [
        {
          group: UrlbarUtils.RESULT_GROUP.GENERAL_PARENT,
          suggestedIndex: 0,
        },
        {
          group: UrlbarUtils.RESULT_GROUP.FORM_HISTORY,
          count: 3,
        },
        {
          group: UrlbarUtils.RESULT_GROUP.GENERAL,
          count: 3,
        },
        {
          group: UrlbarUtils.RESULT_GROUP.REMOTE_SUGGESTION,
          // The muxer will remove the first 3 remote suggestions because they
          // dupe the earlier form history.
          offset: 3,
          count: 2,
        },
        {
          group: UrlbarUtils.RESULT_GROUP.GENERAL_PARENT,
          suggestedIndex: -1,
        },
      ],
    },

    {
      desc: "First result in GENERAL_PARENT, first result in GENERAL",
      suggestedIndexResults: [
        {
          suggestedIndex: 0,
          group: UrlbarUtils.RESULT_GROUP.GENERAL_PARENT,
        },
        {
          suggestedIndex: 0,
          group: UrlbarUtils.RESULT_GROUP.GENERAL,
        },
      ],
      expected: [
        {
          group: UrlbarUtils.RESULT_GROUP.GENERAL_PARENT,
          suggestedIndex: 0,
        },
        {
          group: UrlbarUtils.RESULT_GROUP.FORM_HISTORY,
          count: 3,
        },
        {
          group: UrlbarUtils.RESULT_GROUP.GENERAL,
          suggestedIndex: 0,
        },
        {
          group: UrlbarUtils.RESULT_GROUP.GENERAL,
          count: 2,
        },
        {
          group: UrlbarUtils.RESULT_GROUP.REMOTE_SUGGESTION,
          // The muxer will remove the first 3 remote suggestions because they
          // dupe the earlier form history.
          offset: 3,
          count: 3,
        },
      ],
    },
  ];

  let controller = UrlbarTestUtils.newMockController();

  for (let { desc, suggestedIndexResults, expected } of tests) {
    info(`Running test: ${desc}`);

    // Make the array of all results and do a search.
    let results = basicResults.concat(
      makeSuggestedIndexResults(suggestedIndexResults)
    );
    let provider = registerBasicTestProvider(results);
    let context = createContext(undefined, { providers: [provider.name] });
    await UrlbarProvidersManager.startQuery(context, controller);

    // Make the list of expected results.
    let expectedResults = [];
    for (let { group, offset, count, suggestedIndex } of expected) {
      // Find the index in `results` of the expected result.
      let index = results.findIndex(
        r =>
          UrlbarUtils.getResultGroup(r) == group &&
          r.suggestedIndex === suggestedIndex
      );
      Assert.notEqual(
        index,
        -1,
        "Sanity check: Expected result is in `results`"
      );
      if (offset) {
        index += offset;
      }

      // Extract the expected number of results from `results` and append them
      // to the expected results array.
      count = count || 1;
      expectedResults.push(...results.slice(index, index + count));
    }

    Assert.deepEqual(context.results, expectedResults);

    UrlbarProvidersManager.unregisterProvider(provider);
  }
});

function makeHistoryResults(count = MAX_RESULTS) {
  let results = [];
  for (let i = 0; i < count; i++) {
    results.push(
      new UrlbarResult(
        UrlbarUtils.RESULT_TYPE.URL,
        UrlbarUtils.RESULT_SOURCE.HISTORY,
        { url: "http://example.com/" + i }
      )
    );
  }
  return results;
}

function makeRemoteSuggestionResults(count = MAX_RESULTS) {
  let results = [];
  for (let i = 0; i < count; i++) {
    results.push(
      new UrlbarResult(
        UrlbarUtils.RESULT_TYPE.SEARCH,
        UrlbarUtils.RESULT_SOURCE.SEARCH,
        {
          engine: "test",
          query: "test",
          suggestion: "test " + i,
          lowerCaseSuggestion: "test " + i,
        }
      )
    );
  }
  return results;
}

function makeFormHistoryResults(count = MAX_RESULTS) {
  let results = [];
  for (let i = 0; i < count; i++) {
    results.push(
      new UrlbarResult(
        UrlbarUtils.RESULT_TYPE.SEARCH,
        UrlbarUtils.RESULT_SOURCE.HISTORY,
        {
          engine: "test",
          suggestion: "test " + i,
          lowerCaseSuggestion: "test " + i,
        }
      )
    );
  }
  return results;
}

function makeSuggestedIndexResults(objects) {
  return objects.map(({ suggestedIndex, group }) =>
    Object.assign(
      new UrlbarResult(
        UrlbarUtils.RESULT_TYPE.URL,
        UrlbarUtils.RESULT_SOURCE.OTHER_LOCAL,
        {
          url: "http://example.com/si " + suggestedIndex,
        }
      ),
      {
        group,
        suggestedIndex,
        isSuggestedIndexRelativeToGroup: true,
      }
    )
  );
}

function setResultGroups(resultGroups) {
  if (resultGroups) {
    Services.prefs.setCharPref(
      RESULT_GROUPS_PREF,
      JSON.stringify(resultGroups)
    );
  } else {
    Services.prefs.clearUserPref(RESULT_GROUPS_PREF);
  }
}
