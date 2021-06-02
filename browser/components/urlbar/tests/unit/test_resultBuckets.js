/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

// Tests result buckets composition logic in the muxer -- child buckets,
// maxResultCount, flex, etc.  The purpose of this test is to check the
// composition logic, not every possible result type or group.  There are other
// tests for that.

"use strict";

const RESULT_BUCKETS_PREF = "browser.urlbar.resultBuckets";
const MAX_RICH_RESULTS_PREF = "browser.urlbar.maxRichResults";

// For simplicity, most of the flex tests below assume that this is 10, so
// you'll need to update them if you change this.
const MAX_RESULTS = 10;

add_task(async function setup() {
  // Set a specific maxRichResults for sanity's sake.
  Services.prefs.setIntPref(MAX_RICH_RESULTS_PREF, MAX_RESULTS);
});

add_resultBuckets_task({
  testName: "empty root",
  resultBuckets: {},
  providerResults: [...makeHistoryResults(1)],
  expectedResultIndexes: [],
});

add_resultBuckets_task({
  testName: "root with empty children",
  resultBuckets: {
    children: [],
  },
  providerResults: [...makeHistoryResults(1)],
  expectedResultIndexes: [],
});

add_resultBuckets_task({
  testName: "root no match",
  resultBuckets: {
    group: UrlbarUtils.RESULT_GROUP.REMOTE_SUGGESTION,
  },
  providerResults: [...makeHistoryResults(1)],
  expectedResultIndexes: [],
});

add_resultBuckets_task({
  testName: "children no match",
  resultBuckets: {
    children: [{ group: UrlbarUtils.RESULT_GROUP.REMOTE_SUGGESTION }],
  },
  providerResults: [...makeHistoryResults(1)],
  expectedResultIndexes: [],
});

add_resultBuckets_task({
  // The actual max result count on the root is always context.maxResults and
  // maxResultCount is ignored, so we expect the result in this case.
  testName: "root maxResultCount: 0",
  resultBuckets: {
    maxResultCount: 0,
    group: UrlbarUtils.RESULT_GROUP.GENERAL,
  },
  providerResults: [...makeHistoryResults(1)],
  expectedResultIndexes: [0],
});

add_resultBuckets_task({
  // The actual max result count on the root is always context.maxResults and
  // maxResultCount is ignored, so we expect the result in this case.
  testName: "root maxResultCount: 0 with children",
  resultBuckets: {
    maxResultCount: 0,
    children: [{ group: UrlbarUtils.RESULT_GROUP.GENERAL }],
  },
  providerResults: [...makeHistoryResults(1)],
  expectedResultIndexes: [0],
});

add_resultBuckets_task({
  testName: "child maxResultCount: 0",
  resultBuckets: {
    children: [
      {
        maxResultCount: 0,
        group: UrlbarUtils.RESULT_GROUP.GENERAL,
      },
    ],
  },
  providerResults: [...makeHistoryResults(1)],
  expectedResultIndexes: [],
});

add_resultBuckets_task({
  testName: "root group",
  resultBuckets: {
    group: UrlbarUtils.RESULT_GROUP.GENERAL,
  },
  providerResults: [...makeHistoryResults(1)],
  expectedResultIndexes: [...makeIndexRange(0, 1)],
});

add_resultBuckets_task({
  testName: "root group multiple",
  resultBuckets: {
    group: UrlbarUtils.RESULT_GROUP.GENERAL,
  },
  providerResults: [...makeHistoryResults(2)],
  expectedResultIndexes: [...makeIndexRange(0, 2)],
});

add_resultBuckets_task({
  testName: "child group multiple",
  resultBuckets: {
    children: [{ group: UrlbarUtils.RESULT_GROUP.GENERAL }],
  },
  providerResults: [...makeHistoryResults(2)],
  expectedResultIndexes: [0, 1],
});

add_resultBuckets_task({
  testName: "maxResultCount",
  resultBuckets: {
    children: [
      {
        maxResultCount: 1,
        group: UrlbarUtils.RESULT_GROUP.GENERAL,
      },
    ],
  },
  providerResults: [...makeHistoryResults(2)],
  expectedResultIndexes: [0],
});

add_resultBuckets_task({
  testName: "maxResultCount siblings",
  resultBuckets: {
    children: [
      {
        maxResultCount: 1,
        group: UrlbarUtils.RESULT_GROUP.GENERAL,
      },
      { group: UrlbarUtils.RESULT_GROUP.GENERAL },
    ],
  },
  providerResults: [...makeHistoryResults(2)],
  expectedResultIndexes: [0, 1],
});

add_resultBuckets_task({
  testName: "maxResultCount nested",
  resultBuckets: {
    children: [
      {
        maxResultCount: 1,
        children: [{ group: UrlbarUtils.RESULT_GROUP.GENERAL }],
      },
    ],
  },
  providerResults: [...makeHistoryResults(2)],
  expectedResultIndexes: [0],
});

add_resultBuckets_task({
  testName: "maxResultCount nested siblings",
  resultBuckets: {
    children: [
      {
        maxResultCount: 1,
        children: [
          { group: UrlbarUtils.RESULT_GROUP.GENERAL },
          { group: UrlbarUtils.RESULT_GROUP.GENERAL },
        ],
      },
    ],
  },
  providerResults: [...makeHistoryResults(2)],
  expectedResultIndexes: [0],
});

add_resultBuckets_task({
  testName: "maxResultCount nested uncle",
  resultBuckets: {
    children: [
      {
        maxResultCount: 1,
        children: [{ group: UrlbarUtils.RESULT_GROUP.GENERAL }],
      },
      { group: UrlbarUtils.RESULT_GROUP.GENERAL },
    ],
  },
  providerResults: [...makeHistoryResults(2)],
  expectedResultIndexes: [0, 1],
});

add_resultBuckets_task({
  testName: "maxResultCount nested override bad",
  resultBuckets: {
    children: [
      {
        maxResultCount: 1,
        children: [
          {
            maxResultCount: 99,
            group: UrlbarUtils.RESULT_GROUP.GENERAL,
          },
        ],
      },
    ],
  },
  providerResults: [...makeHistoryResults(2)],
  expectedResultIndexes: [0],
});

add_resultBuckets_task({
  testName: "maxResultCount nested override good",
  resultBuckets: {
    children: [
      {
        maxResultCount: 99,
        children: [
          {
            maxResultCount: 1,
            group: UrlbarUtils.RESULT_GROUP.GENERAL,
          },
        ],
      },
    ],
  },
  providerResults: [...makeHistoryResults(2)],
  expectedResultIndexes: [0],
});

add_resultBuckets_task({
  testName: "multiple groups",
  resultBuckets: {
    children: [
      { group: UrlbarUtils.RESULT_GROUP.GENERAL },
      { group: UrlbarUtils.RESULT_GROUP.REMOTE_SUGGESTION },
    ],
  },
  providerResults: [
    ...makeRemoteSuggestionResults(2),
    ...makeHistoryResults(2),
  ],
  expectedResultIndexes: [...makeIndexRange(2, 2), ...makeIndexRange(0, 2)],
});

add_resultBuckets_task({
  testName: "multiple groups maxResultCount 1",
  resultBuckets: {
    children: [
      {
        maxResultCount: 1,
        group: UrlbarUtils.RESULT_GROUP.GENERAL,
      },
      { group: UrlbarUtils.RESULT_GROUP.REMOTE_SUGGESTION },
    ],
  },
  providerResults: [
    ...makeRemoteSuggestionResults(2),
    ...makeHistoryResults(2),
  ],
  expectedResultIndexes: [...makeIndexRange(2, 1), ...makeIndexRange(0, 2)],
});

add_resultBuckets_task({
  testName: "multiple groups maxResultCount 2",
  resultBuckets: {
    children: [
      { group: UrlbarUtils.RESULT_GROUP.GENERAL },
      {
        maxResultCount: 1,
        group: UrlbarUtils.RESULT_GROUP.REMOTE_SUGGESTION,
      },
    ],
  },
  providerResults: [
    ...makeRemoteSuggestionResults(2),
    ...makeHistoryResults(2),
  ],
  expectedResultIndexes: [...makeIndexRange(2, 2), ...makeIndexRange(0, 1)],
});

add_resultBuckets_task({
  testName: "multiple groups maxResultCount 3",
  resultBuckets: {
    children: [
      {
        maxResultCount: 1,
        group: UrlbarUtils.RESULT_GROUP.GENERAL,
      },
      { group: UrlbarUtils.RESULT_GROUP.REMOTE_SUGGESTION },
      { group: UrlbarUtils.RESULT_GROUP.GENERAL },
    ],
  },
  providerResults: [
    ...makeRemoteSuggestionResults(2),
    ...makeHistoryResults(2),
  ],
  expectedResultIndexes: [
    ...makeIndexRange(2, 1),
    ...makeIndexRange(0, 2),
    ...makeIndexRange(3, 1),
  ],
});

add_resultBuckets_task({
  testName: "multiple groups maxResultCount 4",
  resultBuckets: {
    children: [
      { group: UrlbarUtils.RESULT_GROUP.GENERAL },
      {
        maxResultCount: 1,
        group: UrlbarUtils.RESULT_GROUP.REMOTE_SUGGESTION,
      },
      { group: UrlbarUtils.RESULT_GROUP.GENERAL },
    ],
  },
  providerResults: [
    ...makeRemoteSuggestionResults(2),
    ...makeHistoryResults(2),
  ],
  expectedResultIndexes: [...makeIndexRange(2, 2), ...makeIndexRange(0, 1)],
});

add_resultBuckets_task({
  testName: "multiple groups nested 1",
  resultBuckets: {
    children: [
      {
        children: [
          { group: UrlbarUtils.RESULT_GROUP.GENERAL },
          { group: UrlbarUtils.RESULT_GROUP.REMOTE_SUGGESTION },
        ],
      },
    ],
  },
  providerResults: [
    ...makeRemoteSuggestionResults(2),
    ...makeHistoryResults(2),
  ],
  expectedResultIndexes: [...makeIndexRange(2, 2), ...makeIndexRange(0, 2)],
});

add_resultBuckets_task({
  testName: "multiple groups nested 2",
  resultBuckets: {
    children: [
      {
        children: [{ group: UrlbarUtils.RESULT_GROUP.GENERAL }],
      },
      {
        children: [{ group: UrlbarUtils.RESULT_GROUP.REMOTE_SUGGESTION }],
      },
    ],
  },
  providerResults: [
    ...makeRemoteSuggestionResults(2),
    ...makeHistoryResults(2),
  ],
  expectedResultIndexes: [...makeIndexRange(2, 2), ...makeIndexRange(0, 2)],
});

add_resultBuckets_task({
  testName: "multiple groups nested maxResultCount 1",
  resultBuckets: {
    children: [
      {
        maxResultCount: 1,
        children: [
          { group: UrlbarUtils.RESULT_GROUP.GENERAL },
          { group: UrlbarUtils.RESULT_GROUP.REMOTE_SUGGESTION },
        ],
      },
    ],
  },
  providerResults: [
    ...makeRemoteSuggestionResults(2),
    ...makeHistoryResults(2),
  ],
  expectedResultIndexes: [...makeIndexRange(2, 1)],
});

add_resultBuckets_task({
  testName: "multiple groups nested maxResultCount 2",
  resultBuckets: {
    children: [
      {
        children: [
          {
            maxResultCount: 1,
            group: UrlbarUtils.RESULT_GROUP.GENERAL,
          },
        ],
      },
      {
        children: [{ group: UrlbarUtils.RESULT_GROUP.REMOTE_SUGGESTION }],
      },
    ],
  },
  providerResults: [
    ...makeRemoteSuggestionResults(2),
    ...makeHistoryResults(2),
  ],
  expectedResultIndexes: [...makeIndexRange(2, 1), ...makeIndexRange(0, 2)],
});

add_resultBuckets_task({
  testName: "multiple groups nested maxResultCount 3",
  resultBuckets: {
    children: [
      {
        children: [{ group: UrlbarUtils.RESULT_GROUP.GENERAL }],
      },
      {
        children: [
          {
            maxResultCount: 1,
            group: UrlbarUtils.RESULT_GROUP.REMOTE_SUGGESTION,
          },
        ],
      },
    ],
  },
  providerResults: [
    ...makeRemoteSuggestionResults(2),
    ...makeHistoryResults(2),
  ],
  expectedResultIndexes: [...makeIndexRange(2, 2), ...makeIndexRange(0, 1)],
});

add_resultBuckets_task({
  testName: "multiple groups nested maxResultCount 4",
  resultBuckets: {
    children: [
      {
        children: [
          {
            maxResultCount: 1,
            group: UrlbarUtils.RESULT_GROUP.GENERAL,
          },
        ],
      },
      {
        children: [{ group: UrlbarUtils.RESULT_GROUP.REMOTE_SUGGESTION }],
      },
      {
        children: [{ group: UrlbarUtils.RESULT_GROUP.GENERAL }],
      },
    ],
  },
  providerResults: [
    ...makeRemoteSuggestionResults(2),
    ...makeHistoryResults(2),
  ],
  expectedResultIndexes: [
    ...makeIndexRange(2, 1),
    ...makeIndexRange(0, 2),
    ...makeIndexRange(3, 1),
  ],
});

add_resultBuckets_task({
  testName: "multiple groups nested maxResultCount 5",
  resultBuckets: {
    children: [
      {
        children: [{ group: UrlbarUtils.RESULT_GROUP.GENERAL }],
      },
      {
        children: [
          {
            maxResultCount: 1,
            group: UrlbarUtils.RESULT_GROUP.REMOTE_SUGGESTION,
          },
        ],
      },
      {
        children: [{ group: UrlbarUtils.RESULT_GROUP.GENERAL }],
      },
    ],
  },
  providerResults: [
    ...makeRemoteSuggestionResults(2),
    ...makeHistoryResults(2),
  ],
  expectedResultIndexes: [...makeIndexRange(2, 2), ...makeIndexRange(0, 1)],
});

add_resultBuckets_task({
  testName: "multiple groups nested maxResultCount 6",
  resultBuckets: {
    children: [
      {
        children: [
          {
            maxResultCount: 1,
            group: UrlbarUtils.RESULT_GROUP.GENERAL,
          },
        ],
      },
      { group: UrlbarUtils.RESULT_GROUP.REMOTE_SUGGESTION },
      { group: UrlbarUtils.RESULT_GROUP.GENERAL },
    ],
  },
  providerResults: [
    ...makeRemoteSuggestionResults(2),
    ...makeHistoryResults(2),
  ],
  expectedResultIndexes: [
    ...makeIndexRange(2, 1),
    ...makeIndexRange(0, 2),
    ...makeIndexRange(3, 1),
  ],
});

add_resultBuckets_task({
  testName: "flex 1",
  resultBuckets: {
    flexChildren: true,
    children: [
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
  providerResults: [
    ...makeRemoteSuggestionResults(MAX_RESULTS),
    ...makeHistoryResults(MAX_RESULTS),
  ],
  expectedResultIndexes: [
    // general/history: round(10 * (1 / (1 + 1))) = 5
    ...makeIndexRange(MAX_RESULTS, 5),
    // remote suggestions: round(10 * (1 / (1 + 1))) = 5
    ...makeIndexRange(0, 5),
  ],
});

add_resultBuckets_task({
  testName: "flex 2",
  resultBuckets: {
    flexChildren: true,
    children: [
      {
        flex: 2,
        group: UrlbarUtils.RESULT_GROUP.GENERAL,
      },
      {
        flex: 1,
        group: UrlbarUtils.RESULT_GROUP.REMOTE_SUGGESTION,
      },
    ],
  },
  providerResults: [
    ...makeRemoteSuggestionResults(MAX_RESULTS),
    ...makeHistoryResults(MAX_RESULTS),
  ],
  expectedResultIndexes: [
    // general/history: round(10 * (2 / 3)) = 7
    ...makeIndexRange(MAX_RESULTS, 7),
    // remote suggestions: round(10 * (1 / 3)) = 3
    ...makeIndexRange(0, 3),
  ],
});

add_resultBuckets_task({
  testName: "flex 3",
  resultBuckets: {
    flexChildren: true,
    children: [
      {
        flex: 1,
        group: UrlbarUtils.RESULT_GROUP.GENERAL,
      },
      {
        flex: 2,
        group: UrlbarUtils.RESULT_GROUP.REMOTE_SUGGESTION,
      },
    ],
  },
  providerResults: [
    ...makeRemoteSuggestionResults(MAX_RESULTS),
    ...makeHistoryResults(MAX_RESULTS),
  ],
  expectedResultIndexes: [
    // general/history: round(10 * (1 / 3)) = 3
    ...makeIndexRange(MAX_RESULTS, 3),
    // remote suggestions: round(10 * (2 / 3)) = 7
    ...makeIndexRange(0, 7),
  ],
});

add_resultBuckets_task({
  testName: "flex 4",
  resultBuckets: {
    flexChildren: true,
    children: [
      {
        flex: 1,
        group: UrlbarUtils.RESULT_GROUP.GENERAL,
      },
      {
        flex: 1,
        group: UrlbarUtils.RESULT_GROUP.REMOTE_SUGGESTION,
      },
      {
        flex: 1,
        group: UrlbarUtils.RESULT_GROUP.FORM_HISTORY,
      },
    ],
  },
  providerResults: [
    ...makeFormHistoryResults(MAX_RESULTS),
    ...makeRemoteSuggestionResults(MAX_RESULTS),
    ...makeHistoryResults(MAX_RESULTS),
  ],
  expectedResultIndexes: [
    // general/history: round(10 * (1 / 3)) = 3
    ...makeIndexRange(2 * MAX_RESULTS, 3),
    // remote suggestions: round(10 * (1 / 3)) = 3
    ...makeIndexRange(MAX_RESULTS, 3),
    // form history: round(10 * (1 / 3)) = 3
    // The first three form history results dupe the three remote suggestions,
    // so they should not be included.
    ...makeIndexRange(3, 3),
  ],
});

add_resultBuckets_task({
  testName: "flex 5",
  resultBuckets: {
    flexChildren: true,
    children: [
      {
        flex: 2,
        group: UrlbarUtils.RESULT_GROUP.GENERAL,
      },
      {
        flex: 1,
        group: UrlbarUtils.RESULT_GROUP.REMOTE_SUGGESTION,
      },
      {
        flex: 1,
        group: UrlbarUtils.RESULT_GROUP.FORM_HISTORY,
      },
    ],
  },
  providerResults: [
    ...makeFormHistoryResults(MAX_RESULTS),
    ...makeRemoteSuggestionResults(MAX_RESULTS),
    ...makeHistoryResults(MAX_RESULTS),
  ],
  expectedResultIndexes: [
    // general/history: round(10 * (2 / 4)) = 5
    ...makeIndexRange(2 * MAX_RESULTS, 5),
    // remote suggestions: round(10 * (1 / 4)) = 3
    ...makeIndexRange(MAX_RESULTS, 3),
    // form history: round(10 * (1 / 4)) = 3, but context.maxResults is 10, so 2
    // The first three form history results dupe the three remote suggestions,
    // so they should not be included.
    ...makeIndexRange(3, 2),
  ],
});

add_resultBuckets_task({
  testName: "flex 6",
  resultBuckets: {
    flexChildren: true,
    children: [
      {
        flex: 1,
        group: UrlbarUtils.RESULT_GROUP.GENERAL,
      },
      {
        flex: 2,
        group: UrlbarUtils.RESULT_GROUP.REMOTE_SUGGESTION,
      },
      {
        flex: 1,
        group: UrlbarUtils.RESULT_GROUP.FORM_HISTORY,
      },
    ],
  },
  providerResults: [
    ...makeFormHistoryResults(MAX_RESULTS),
    ...makeRemoteSuggestionResults(MAX_RESULTS),
    ...makeHistoryResults(MAX_RESULTS),
  ],
  expectedResultIndexes: [
    // general/history: round(10 * (1 / 4)) = 3
    ...makeIndexRange(2 * MAX_RESULTS, 3),
    // remote suggestions: round(10 * (2 / 4)) = 5
    ...makeIndexRange(MAX_RESULTS, 5),
    // form history: round(10 * (1 / 4)) = 3, but context.maxResults is 10, so 2
    // The first five form history results dupe the five remote suggestions, so
    // they should not be included.
    ...makeIndexRange(5, 2),
  ],
});

add_resultBuckets_task({
  testName: "flex 7",
  resultBuckets: {
    flexChildren: true,
    children: [
      {
        flex: 1,
        group: UrlbarUtils.RESULT_GROUP.GENERAL,
      },
      {
        flex: 1,
        group: UrlbarUtils.RESULT_GROUP.REMOTE_SUGGESTION,
      },
      {
        flex: 2,
        group: UrlbarUtils.RESULT_GROUP.FORM_HISTORY,
      },
    ],
  },
  providerResults: [
    ...makeFormHistoryResults(MAX_RESULTS),
    ...makeRemoteSuggestionResults(MAX_RESULTS),
    ...makeHistoryResults(MAX_RESULTS),
  ],
  expectedResultIndexes: [
    // general/history: round(10 * (1 / 4)) = 3
    ...makeIndexRange(2 * MAX_RESULTS, 3),
    // remote suggestions: round(10 * (1 / 4)) = 3
    ...makeIndexRange(MAX_RESULTS, 3),
    // form history: round(10 * (2 / 4)) = 5, but context.maxResults is 10, so 4
    // The first three form history results dupe the three remote suggestions,
    // so they should not be included.
    ...makeIndexRange(3, 4),
  ],
});

add_resultBuckets_task({
  testName: "flex zero implied",
  resultBuckets: {
    flexChildren: true,
    children: [
      {
        flex: 2,
        group: UrlbarUtils.RESULT_GROUP.GENERAL,
      },
      {
        flex: 1,
        group: UrlbarUtils.RESULT_GROUP.REMOTE_SUGGESTION,
      },
      {
        // `flex: 0` is implied
        group: UrlbarUtils.RESULT_GROUP.FORM_HISTORY,
      },
    ],
  },
  providerResults: [
    ...makeFormHistoryResults(MAX_RESULTS),
    ...makeRemoteSuggestionResults(1),
    ...makeHistoryResults(2),
  ],
  expectedResultIndexes: [
    // general/history
    ...makeIndexRange(MAX_RESULTS + 1, 2),
    // remote suggestions
    ...makeIndexRange(MAX_RESULTS, 1),
    // form history: 10 - (2 + 1) = 7
    // The first form history result dupes the remote suggestion, so it should
    // not be included.
    ...makeIndexRange(1, 7),
  ],
});

add_resultBuckets_task({
  testName: "flex zero explicit",
  resultBuckets: {
    flexChildren: true,
    children: [
      {
        flex: 2,
        group: UrlbarUtils.RESULT_GROUP.GENERAL,
      },
      {
        flex: 1,
        group: UrlbarUtils.RESULT_GROUP.REMOTE_SUGGESTION,
      },
      {
        flex: 0,
        group: UrlbarUtils.RESULT_GROUP.FORM_HISTORY,
      },
    ],
  },
  providerResults: [
    ...makeFormHistoryResults(MAX_RESULTS),
    ...makeRemoteSuggestionResults(1),
    ...makeHistoryResults(2),
  ],
  expectedResultIndexes: [
    // general/history
    ...makeIndexRange(MAX_RESULTS + 1, 2),
    // remote suggestions
    ...makeIndexRange(MAX_RESULTS, 1),
    // form history: 10 - (2 + 1) = 7
    // The first form history result dupes the remote suggestion, so it should
    // not be included.
    ...makeIndexRange(1, 7),
  ],
});

add_resultBuckets_task({
  testName: "flex overfill 1",
  resultBuckets: {
    flexChildren: true,
    children: [
      {
        flex: 2,
        group: UrlbarUtils.RESULT_GROUP.GENERAL,
      },
      {
        flex: 1,
        group: UrlbarUtils.RESULT_GROUP.REMOTE_SUGGESTION,
      },
      {
        flex: 1,
        group: UrlbarUtils.RESULT_GROUP.FORM_HISTORY,
      },
    ],
  },
  providerResults: [
    ...makeFormHistoryResults(MAX_RESULTS),
    ...makeHistoryResults(MAX_RESULTS),
  ],
  expectedResultIndexes: [
    // general/history: round(10 * (2 / (2 + 0 + 1))) = 7
    ...makeIndexRange(MAX_RESULTS, 7),
    // form history: round(10 * (1 / (2 + 0 + 1))) = 3
    ...makeIndexRange(0, 3),
  ],
});

add_resultBuckets_task({
  testName: "flex overfill 2",
  resultBuckets: {
    flexChildren: true,
    children: [
      {
        flex: 2,
        group: UrlbarUtils.RESULT_GROUP.GENERAL,
      },
      {
        flex: 2,
        group: UrlbarUtils.RESULT_GROUP.REMOTE_SUGGESTION,
      },
      {
        flex: 1,
        group: UrlbarUtils.RESULT_GROUP.FORM_HISTORY,
      },
    ],
  },
  providerResults: [
    ...makeFormHistoryResults(MAX_RESULTS),
    ...makeRemoteSuggestionResults(1),
    ...makeHistoryResults(MAX_RESULTS),
  ],
  expectedResultIndexes: [
    // general/history: round(9 * (2 / (2 + 0 + 1))) = 6
    ...makeIndexRange(MAX_RESULTS + 1, 6),
    // remote suggestions
    ...makeIndexRange(MAX_RESULTS, 1),
    // form history: round(9 * (1 / (2 + 0 + 1))) = 3
    // The first form history result dupes the remote suggestion, so it should
    // not be included.
    ...makeIndexRange(1, 3),
  ],
});

add_resultBuckets_task({
  testName: "flex nested maxResultCount 1",
  resultBuckets: {
    children: [
      {
        maxResultCount: 5,
        flexChildren: true,
        children: [
          {
            flex: 2,
            group: UrlbarUtils.RESULT_GROUP.GENERAL,
          },
          {
            flex: 1,
            group: UrlbarUtils.RESULT_GROUP.REMOTE_SUGGESTION,
          },
        ],
      },
    ],
  },
  providerResults: [
    ...makeRemoteSuggestionResults(MAX_RESULTS),
    ...makeHistoryResults(MAX_RESULTS),
  ],
  expectedResultIndexes: [
    // general/history: round(5 * (2 / (2 + 1))) = 3
    ...makeIndexRange(MAX_RESULTS, 3),
    // remote suggestions: round(5 * (1 / (2 + 1))) = 2
    ...makeIndexRange(0, 2),
  ],
});

add_resultBuckets_task({
  testName: "flex nested maxResultCount 2",
  resultBuckets: {
    children: [
      {
        maxResultCount: 7,
        flexChildren: true,
        children: [
          {
            flex: 1,
            group: UrlbarUtils.RESULT_GROUP.GENERAL,
          },
          {
            flex: 2,
            group: UrlbarUtils.RESULT_GROUP.REMOTE_SUGGESTION,
          },
        ],
      },
    ],
  },
  providerResults: [
    ...makeRemoteSuggestionResults(MAX_RESULTS),
    ...makeHistoryResults(MAX_RESULTS),
  ],
  expectedResultIndexes: [
    // general: round(7 * (1 / (2 + 1))) = 2
    ...makeIndexRange(MAX_RESULTS, 2),
    // remote suggestions: round(7 * (2 / (2 + 1))) = 5
    ...makeIndexRange(0, 5),
  ],
});

add_resultBuckets_task({
  testName: "flex nested maxResultCount 3",
  resultBuckets: {
    children: [
      {
        maxResultCount: 7,
        flexChildren: true,
        children: [
          {
            flex: 1,
            group: UrlbarUtils.RESULT_GROUP.GENERAL,
          },
          {
            flex: 2,
            group: UrlbarUtils.RESULT_GROUP.REMOTE_SUGGESTION,
          },
        ],
      },
      {
        maxResultCount: 3,
        flexChildren: true,
        children: [
          {
            flex: 2,
            group: UrlbarUtils.RESULT_GROUP.FORM_HISTORY,
          },
          {
            flex: 1,
            group: UrlbarUtils.RESULT_GROUP.GENERAL,
          },
        ],
      },
    ],
  },
  providerResults: [
    ...makeRemoteSuggestionResults(MAX_RESULTS),
    ...makeFormHistoryResults(MAX_RESULTS),
    ...makeHistoryResults(MAX_RESULTS),
  ],
  expectedResultIndexes: [
    // general: round(7 * (1 / (2 + 1))) = 2
    ...makeIndexRange(2 * MAX_RESULTS, 2),
    // remote suggestions: round(7 * (2 / (2 + 1))) = 5
    ...makeIndexRange(0, 5),
    // form history: round(3 * (2 / (2 + 1))) = 2
    // The first five form history results dupe the five remote suggestions, so
    // they should not be included.
    ...makeIndexRange(MAX_RESULTS + 5, 2),
    // general: round(3 * (1 / (2 + 1))) = 1
    ...makeIndexRange(2 * MAX_RESULTS + 2, 1),
  ],
});

add_resultBuckets_task({
  testName: "flex nested maxResultCount 4",
  resultBuckets: {
    children: [
      {
        maxResultCount: 7,
        flexChildren: true,
        children: [
          {
            flex: 1,
            group: UrlbarUtils.RESULT_GROUP.GENERAL,
          },
          {
            flex: 2,
            group: UrlbarUtils.RESULT_GROUP.REMOTE_SUGGESTION,
          },
        ],
      },
      {
        maxResultCount: 3,
        children: [
          { group: UrlbarUtils.RESULT_GROUP.FORM_HISTORY },
          { group: UrlbarUtils.RESULT_GROUP.GENERAL },
        ],
      },
    ],
  },
  providerResults: [
    ...makeRemoteSuggestionResults(MAX_RESULTS),
    ...makeFormHistoryResults(MAX_RESULTS),
    ...makeHistoryResults(MAX_RESULTS),
  ],
  expectedResultIndexes: [
    // general: round(7 * (1 / (2 + 1))) = 2
    ...makeIndexRange(2 * MAX_RESULTS, 2),
    // remote suggestions: round(7 * (2 / (2 + 1))) = 5
    ...makeIndexRange(0, 5),
    // form history: round(3 * (2 / (2 + 1))) = 2
    // The first five form history results dupe the five remote suggestions, so
    // they should not be included.
    ...makeIndexRange(MAX_RESULTS + 5, 3),
  ],
});

add_resultBuckets_task({
  testName: "flex nested maxResultCount 5",
  resultBuckets: {
    children: [
      {
        maxResultCount: 7,
        flexChildren: true,
        children: [
          {
            flex: 1,
            group: UrlbarUtils.RESULT_GROUP.GENERAL,
          },
          {
            flex: 2,
            group: UrlbarUtils.RESULT_GROUP.REMOTE_SUGGESTION,
          },
        ],
      },
      {
        maxResultCount: 3,
        group: UrlbarUtils.RESULT_GROUP.FORM_HISTORY,
      },
    ],
  },
  providerResults: [
    ...makeRemoteSuggestionResults(MAX_RESULTS),
    ...makeFormHistoryResults(MAX_RESULTS),
    ...makeHistoryResults(MAX_RESULTS),
  ],
  expectedResultIndexes: [
    // general: round(7 * (1 / (2 + 1))) = 2
    ...makeIndexRange(2 * MAX_RESULTS, 2),
    // remote suggestions: round(7 * (2 / (2 + 1))) = 5
    ...makeIndexRange(0, 5),
    // form history: round(3 * (2 / (2 + 1))) = 2
    // The first five form history results dupe the five remote suggestions, so
    // they should not be included.
    ...makeIndexRange(MAX_RESULTS + 5, 3),
  ],
});

add_resultBuckets_task({
  testName: "flex nested",
  resultBuckets: {
    flexChildren: true,
    children: [
      {
        flex: 2,
        flexChildren: true,
        children: [
          {
            flex: 1,
            group: UrlbarUtils.RESULT_GROUP.GENERAL,
          },
          {
            flex: 2,
            group: UrlbarUtils.RESULT_GROUP.REMOTE_SUGGESTION,
          },
        ],
      },
      {
        flex: 1,
        flexChildren: true,
        children: [
          {
            flex: 2,
            group: UrlbarUtils.RESULT_GROUP.FORM_HISTORY,
          },
          {
            flex: 1,
            group: UrlbarUtils.RESULT_GROUP.GENERAL,
          },
        ],
      },
    ],
  },
  providerResults: [
    ...makeRemoteSuggestionResults(MAX_RESULTS),
    ...makeFormHistoryResults(MAX_RESULTS),
    ...makeHistoryResults(MAX_RESULTS),
  ],
  expectedResultIndexes: [
    // outer 1: general & remote suggestions: round(10 * (2 / (2 + 1))) = 7
    // inner 1: general: round(7 * (1 / (2 + 1))) = 2
    ...makeIndexRange(2 * MAX_RESULTS, 2),
    // inner 2: remote suggestions: round(7 * (2 / (2 + 1))) = 5
    ...makeIndexRange(0, 5),

    // outer 2: form history & general: round(10 * (1 / (2 + 1))) = 3
    // inner 1: form history: round(3 * (2 / (2 + 1))) = 2
    // The first five form history results dupe the five remote suggestions, so
    // they should not be included.
    ...makeIndexRange(MAX_RESULTS + 5, 2),
    // inner 2: general: round(3 * (1 / (2 + 1))) = 1
    ...makeIndexRange(2 * MAX_RESULTS + 2, 1),
  ],
});

add_resultBuckets_task({
  testName: "flex nested overfill 1",
  resultBuckets: {
    flexChildren: true,
    children: [
      {
        flex: 2,
        flexChildren: true,
        children: [
          {
            flex: 1,
            group: UrlbarUtils.RESULT_GROUP.GENERAL,
          },
          {
            flex: 2,
            group: UrlbarUtils.RESULT_GROUP.REMOTE_SUGGESTION,
          },
        ],
      },
      {
        flex: 1,
        flexChildren: true,
        children: [
          {
            flex: 2,
            group: UrlbarUtils.RESULT_GROUP.FORM_HISTORY,
          },
          {
            flex: 1,
            group: UrlbarUtils.RESULT_GROUP.GENERAL,
          },
        ],
      },
    ],
  },
  providerResults: [
    ...makeRemoteSuggestionResults(MAX_RESULTS),
    ...makeFormHistoryResults(MAX_RESULTS),
  ],
  expectedResultIndexes: [
    // outer 1: general & remote suggestions: round(10 * (2 / (2 + 1))) = 7
    // inner 1: general: no results
    // inner 2: remote suggestions: round(7 * (2 / (2 + 0))) = 7
    ...makeIndexRange(0, 7),

    // outer 2: form history & general: round(10 * (1 / (2 + 1))) = 3
    // inner 1: form history: round(3 * (2 / (2 + 0))) = 3
    // The first seven form history results dupe the seven remote suggestions,
    // so they should not be included.
    ...makeIndexRange(MAX_RESULTS + 7, 3),
    // inner 2: general: no results
  ],
});

add_resultBuckets_task({
  testName: "flex nested overfill 2",
  resultBuckets: {
    flexChildren: true,
    children: [
      {
        flex: 2,
        flexChildren: true,
        children: [
          {
            flex: 1,
            group: UrlbarUtils.RESULT_GROUP.GENERAL,
          },
          {
            flex: 2,
            group: UrlbarUtils.RESULT_GROUP.REMOTE_SUGGESTION,
          },
        ],
      },
      {
        flex: 1,
        flexChildren: true,
        children: [
          {
            flex: 2,
            group: UrlbarUtils.RESULT_GROUP.FORM_HISTORY,
          },
          {
            flex: 1,
            group: UrlbarUtils.RESULT_GROUP.GENERAL,
          },
        ],
      },
    ],
  },
  providerResults: [...makeFormHistoryResults(MAX_RESULTS)],
  expectedResultIndexes: [
    // outer 1: general & remote suggestions: round(10 * (2 / (2 + 1))) = 7
    // inner 1: general: no results
    // inner 2: remote suggestions: no results

    // outer 2: form history & general: round(10 * (1 / (0 + 1))) = 10
    // inner 1: form history: round(10 * (2 / (2 + 0))) = 10
    ...makeIndexRange(0, MAX_RESULTS),
    // inner 2: general: no results
  ],
});

add_resultBuckets_task({
  testName: "flex nested overfill 3",
  resultBuckets: {
    flexChildren: true,
    children: [
      {
        flex: 2,
        flexChildren: true,
        children: [
          {
            flex: 1,
            group: UrlbarUtils.RESULT_GROUP.GENERAL,
          },
          {
            flex: 2,
            group: UrlbarUtils.RESULT_GROUP.REMOTE_SUGGESTION,
          },
        ],
      },
      {
        flex: 1,
        flexChildren: true,
        children: [
          {
            flex: 2,
            group: UrlbarUtils.RESULT_GROUP.FORM_HISTORY,
          },
          {
            flex: 1,
            group: UrlbarUtils.RESULT_GROUP.GENERAL,
          },
        ],
      },
    ],
  },
  providerResults: [...makeRemoteSuggestionResults(MAX_RESULTS)],
  expectedResultIndexes: [
    // outer 1: general & remote suggestions: round(10 * (2 / (2 + 0))) = 10
    // inner 1: general: no results
    // inner 2: remote suggestions: round(10 * (2 / (2 + 0))) = 10
    ...makeIndexRange(0, MAX_RESULTS),

    // outer 2: form history & general: round(10 * (1 / (2 + 1))) = 3
    // inner 1: form history: no results
    // inner 2: general: no results
  ],
});

add_resultBuckets_task({
  testName: "maxResultCount ignored with flex",
  resultBuckets: {
    flexChildren: true,
    children: [
      {
        maxResultCount: 1,
        flex: 2,
        group: UrlbarUtils.RESULT_GROUP.GENERAL,
      },
      {
        flex: 1,
        group: UrlbarUtils.RESULT_GROUP.REMOTE_SUGGESTION,
      },
    ],
  },
  providerResults: [
    ...makeRemoteSuggestionResults(MAX_RESULTS),
    ...makeHistoryResults(MAX_RESULTS),
  ],
  expectedResultIndexes: [
    // general/history: round(10 * (2 / (2 + 1))) = 7 -- maxResultCount ignored
    ...makeIndexRange(MAX_RESULTS, 7),
    // remote suggestions: round(10 * (1 / (2 + 1))) = 3
    ...makeIndexRange(0, 3),
  ],
});

add_resultBuckets_task({
  testName: "resultSpan = 3 followed by others",
  resultBuckets: {
    children: [
      {
        group: UrlbarUtils.RESULT_GROUP.GENERAL,
      },
      {
        group: UrlbarUtils.RESULT_GROUP.REMOTE_SUGGESTION,
      },
    ],
  },
  providerResults: [
    // max results remote suggestions
    ...makeRemoteSuggestionResults(MAX_RESULTS),
    // 1 history with resultSpan = 3
    Object.assign(
      makeHistoryResults(1)[0],
      { resultSpan: 3 },
    )
  ],
  expectedResultIndexes: [
    // general/history: 1
    ...makeIndexRange(MAX_RESULTS, 1),
    // remote suggestions: maxResults - resultSpan of 3 = 10 - 3 = 7
    ...makeIndexRange(0, 7),
  ],
});

/**
 * Adds a test task.
 *
 * @param {string} testName
 *   This name is logged with `info` as the task starts.
 * @param {object} resultBuckets
 *   browser.urlbar.resultBuckets is set to this value as the task starts.
 * @param {array} providerResults
 *   Array of result objects that the test provider will add.
 * @param {array} expectedResultIndexes
 *   Array of indexes in `providerResults` of the expected final results.
 */
function add_resultBuckets_task({
  testName,
  resultBuckets,
  providerResults,
  expectedResultIndexes,
}) {
  let func = async () => {
    info(`Running resultBuckest test: ${testName}`);
    setResultBuckets(resultBuckets);
    let provider = registerBasicTestProvider(providerResults);
    let context = createContext("foo", { providers: [provider.name] });
    let controller = UrlbarTestUtils.newMockController();
    await UrlbarProvidersManager.startQuery(context, controller);
    UrlbarProvidersManager.unregisterProvider(provider);
    let expectedResults = expectedResultIndexes.map(i => providerResults[i]);
    Assert.deepEqual(context.results, expectedResults);
    setResultBuckets(null);
  };
  Object.defineProperty(func, "name", { value: testName });
  add_task(func);
}

function makeHistoryResults(count) {
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

function makeRemoteSuggestionResults(count) {
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

function makeFormHistoryResults(count) {
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

function makeIndexRange(startIndex, count) {
  let indexes = [];
  for (let i = startIndex; i < startIndex + count; i++) {
    indexes.push(i);
  }
  return indexes;
}

function setResultBuckets(resultBuckets) {
  if (resultBuckets) {
    Services.prefs.setCharPref(
      RESULT_BUCKETS_PREF,
      JSON.stringify(resultBuckets)
    );
  } else {
    Services.prefs.clearUserPref(RESULT_BUCKETS_PREF);
  }
}
