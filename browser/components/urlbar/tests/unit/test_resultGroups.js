/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

// Tests the muxer's result groups composition logic: child groups,
// `availableSpan`, `maxResultCount`, flex, etc. The purpose of this test is to
// check the composition logic, not every possible result type or group.

"use strict";

const lazy = {};

ChromeUtils.defineESModuleGetters(lazy, {
  sinon: "resource://testing-common/Sinon.sys.mjs",
});

// The possible limit-related properties in result groups.
const LIMIT_KEYS = ["availableSpan", "maxResultCount"];

// Most of this test adds tasks using `add_resultGroupsLimit_tasks`. It works
// like this. Instead of defining `maxResultCount` or `availableSpan` in their
// result groups, tasks define a `limit` property. The value of this property is
// a number just like any of the values for the limit-related properties. At
// runtime, `add_resultGroupsLimit_tasks` adds multiple tasks, one for each key
// in `LIMIT_KEYS`. In each of these tasks, the `limit` property is replaced
// with the actual limit key. This allows us to run checks against each of the
// limit keys using essentially the same task.

const MAX_RICH_RESULTS_PREF = "browser.urlbar.maxRichResults";

// For simplicity, most of the flex tests below assume that this is 10, so
// you'll need to update them if you change this.
const MAX_RESULTS = 10;

let sandbox;

add_setup(async function () {
  // Set a specific maxRichResults for sanity's sake.
  Services.prefs.setIntPref(MAX_RICH_RESULTS_PREF, MAX_RESULTS);

  sandbox = lazy.sinon.createSandbox();
  registerCleanupFunction(() => {
    sandbox.restore();
  });
});

add_resultGroupsLimit_tasks({
  testName: "empty root",
  resultGroups: {},
  providerResults: [...makeHistoryResults(1)],
  expectedResultIndexes: [],
});

add_resultGroupsLimit_tasks({
  testName: "root with empty children",
  resultGroups: {
    children: [],
  },
  providerResults: [...makeHistoryResults(1)],
  expectedResultIndexes: [],
});

add_resultGroupsLimit_tasks({
  testName: "root no match",
  resultGroups: {
    group: UrlbarUtils.RESULT_GROUP.REMOTE_SUGGESTION,
  },
  providerResults: [...makeHistoryResults(1)],
  expectedResultIndexes: [],
});

add_resultGroupsLimit_tasks({
  testName: "children no match",
  resultGroups: {
    children: [{ group: UrlbarUtils.RESULT_GROUP.REMOTE_SUGGESTION }],
  },
  providerResults: [...makeHistoryResults(1)],
  expectedResultIndexes: [],
});

add_resultGroupsLimit_tasks({
  // The actual max result count on the root is always context.maxResults and
  // limit is ignored, so we expect the result in this case.
  testName: "root limit: 0",
  resultGroups: {
    limit: 0,
    group: UrlbarUtils.RESULT_GROUP.GENERAL,
  },
  providerResults: [...makeHistoryResults(1)],
  expectedResultIndexes: [0],
});

add_resultGroupsLimit_tasks({
  // The actual max result count on the root is always context.maxResults and
  // limit is ignored, so we expect the result in this case.
  testName: "root limit: 0 with children",
  resultGroups: {
    limit: 0,
    children: [{ group: UrlbarUtils.RESULT_GROUP.GENERAL }],
  },
  providerResults: [...makeHistoryResults(1)],
  expectedResultIndexes: [0],
});

add_resultGroupsLimit_tasks({
  testName: "child limit: 0",
  resultGroups: {
    children: [
      {
        limit: 0,
        group: UrlbarUtils.RESULT_GROUP.GENERAL,
      },
    ],
  },
  providerResults: [...makeHistoryResults(1)],
  expectedResultIndexes: [],
});

add_resultGroupsLimit_tasks({
  testName: "root group",
  resultGroups: {
    group: UrlbarUtils.RESULT_GROUP.GENERAL,
  },
  providerResults: [...makeHistoryResults(1)],
  expectedResultIndexes: [...makeIndexRange(0, 1)],
});

add_resultGroupsLimit_tasks({
  testName: "root group multiple",
  resultGroups: {
    group: UrlbarUtils.RESULT_GROUP.GENERAL,
  },
  providerResults: [...makeHistoryResults(2)],
  expectedResultIndexes: [...makeIndexRange(0, 2)],
});

add_resultGroupsLimit_tasks({
  testName: "child group multiple",
  resultGroups: {
    children: [{ group: UrlbarUtils.RESULT_GROUP.GENERAL }],
  },
  providerResults: [...makeHistoryResults(2)],
  expectedResultIndexes: [0, 1],
});

add_resultGroupsLimit_tasks({
  testName: "simple limit",
  resultGroups: {
    children: [
      {
        limit: 1,
        group: UrlbarUtils.RESULT_GROUP.GENERAL,
      },
    ],
  },
  providerResults: [...makeHistoryResults(2)],
  expectedResultIndexes: [0],
});

add_resultGroupsLimit_tasks({
  testName: "limit siblings",
  resultGroups: {
    children: [
      {
        limit: 1,
        group: UrlbarUtils.RESULT_GROUP.GENERAL,
      },
      { group: UrlbarUtils.RESULT_GROUP.GENERAL },
    ],
  },
  providerResults: [...makeHistoryResults(2)],
  expectedResultIndexes: [0, 1],
});

add_resultGroupsLimit_tasks({
  testName: "limit nested",
  resultGroups: {
    children: [
      {
        limit: 1,
        children: [{ group: UrlbarUtils.RESULT_GROUP.GENERAL }],
      },
    ],
  },
  providerResults: [...makeHistoryResults(2)],
  expectedResultIndexes: [0],
});

add_resultGroupsLimit_tasks({
  testName: "limit nested siblings",
  resultGroups: {
    children: [
      {
        limit: 1,
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

add_resultGroupsLimit_tasks({
  testName: "limit nested uncle",
  resultGroups: {
    children: [
      {
        limit: 1,
        children: [{ group: UrlbarUtils.RESULT_GROUP.GENERAL }],
      },
      { group: UrlbarUtils.RESULT_GROUP.GENERAL },
    ],
  },
  providerResults: [...makeHistoryResults(2)],
  expectedResultIndexes: [0, 1],
});

add_resultGroupsLimit_tasks({
  testName: "limit nested override bad",
  resultGroups: {
    children: [
      {
        limit: 1,
        children: [
          {
            limit: 99,
            group: UrlbarUtils.RESULT_GROUP.GENERAL,
          },
        ],
      },
    ],
  },
  providerResults: [...makeHistoryResults(2)],
  expectedResultIndexes: [0],
});

add_resultGroupsLimit_tasks({
  testName: "limit nested override good",
  resultGroups: {
    children: [
      {
        limit: 99,
        children: [
          {
            limit: 1,
            group: UrlbarUtils.RESULT_GROUP.GENERAL,
          },
        ],
      },
    ],
  },
  providerResults: [...makeHistoryResults(2)],
  expectedResultIndexes: [0],
});

add_resultGroupsLimit_tasks({
  testName: "multiple groups",
  resultGroups: {
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

add_resultGroupsLimit_tasks({
  testName: "multiple groups limit 1",
  resultGroups: {
    children: [
      {
        limit: 1,
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

add_resultGroupsLimit_tasks({
  testName: "multiple groups limit 2",
  resultGroups: {
    children: [
      { group: UrlbarUtils.RESULT_GROUP.GENERAL },
      {
        limit: 1,
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

add_resultGroupsLimit_tasks({
  testName: "multiple groups limit 3",
  resultGroups: {
    children: [
      {
        limit: 1,
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

add_resultGroupsLimit_tasks({
  testName: "multiple groups limit 4",
  resultGroups: {
    children: [
      { group: UrlbarUtils.RESULT_GROUP.GENERAL },
      {
        limit: 1,
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

add_resultGroupsLimit_tasks({
  testName: "multiple groups nested 1",
  resultGroups: {
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

add_resultGroupsLimit_tasks({
  testName: "multiple groups nested 2",
  resultGroups: {
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

add_resultGroupsLimit_tasks({
  testName: "multiple groups nested limit 1",
  resultGroups: {
    children: [
      {
        limit: 1,
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

add_resultGroupsLimit_tasks({
  testName: "multiple groups nested limit 2",
  resultGroups: {
    children: [
      {
        children: [
          {
            limit: 1,
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

add_resultGroupsLimit_tasks({
  testName: "multiple groups nested limit 3",
  resultGroups: {
    children: [
      {
        children: [{ group: UrlbarUtils.RESULT_GROUP.GENERAL }],
      },
      {
        children: [
          {
            limit: 1,
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

add_resultGroupsLimit_tasks({
  testName: "multiple groups nested limit 4",
  resultGroups: {
    children: [
      {
        children: [
          {
            limit: 1,
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

add_resultGroupsLimit_tasks({
  testName: "multiple groups nested limit 5",
  resultGroups: {
    children: [
      {
        children: [{ group: UrlbarUtils.RESULT_GROUP.GENERAL }],
      },
      {
        children: [
          {
            limit: 1,
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

add_resultGroupsLimit_tasks({
  testName: "multiple groups nested limit 6",
  resultGroups: {
    children: [
      {
        children: [
          {
            limit: 1,
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

add_resultGroupsLimit_tasks({
  testName: "flex 1",
  resultGroups: {
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

add_resultGroupsLimit_tasks({
  testName: "flex 2",
  resultGroups: {
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

add_resultGroupsLimit_tasks({
  testName: "flex 3",
  resultGroups: {
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

add_resultGroupsLimit_tasks({
  testName: "flex 4",
  resultGroups: {
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
    // general/history: round(10 * (1 / 3)) = 3, and then incremented to 4 so
    // that the total result span is 10 instead of 9. This group is incremented
    // because the fractional part of its unrounded ideal max result count is
    // 0.33 (since 10 * (1 / 3) = 3.33), the same as the other two groups, and
    // this group is first.
    ...makeIndexRange(2 * MAX_RESULTS, 4),
    // remote suggestions: round(10 * (1 / 3)) = 3
    ...makeIndexRange(MAX_RESULTS, 3),
    // form history: round(10 * (1 / 3)) = 3
    // The first three form history results dupe the three remote suggestions,
    // so they should not be included.
    ...makeIndexRange(3, 3),
  ],
});

add_resultGroupsLimit_tasks({
  testName: "flex 5",
  resultGroups: {
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

add_resultGroupsLimit_tasks({
  testName: "flex 6",
  resultGroups: {
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

add_resultGroupsLimit_tasks({
  testName: "flex 7",
  resultGroups: {
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
    // remote suggestions: round(10 * (1 / 4)) = 3, and then decremented to 2 so
    // that the total result span is 10 instead of 11. This group is decremented
    // because the fractional part of its unrounded ideal max result count is
    // 0.5 (since 10 * (1 / 4) = 2.5), the same as the previous group, and the
    // next group's fractional part is zero.
    ...makeIndexRange(MAX_RESULTS, 2),
    // form history: round(10 * (2 / 4)) = 5
    // The first 2 form history results dupe the three remote suggestions, so
    // they should not be included.
    ...makeIndexRange(2, 5),
  ],
});

add_resultGroupsLimit_tasks({
  testName: "flex overfill 1",
  resultGroups: {
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

add_resultGroupsLimit_tasks({
  testName: "flex overfill 2",
  resultGroups: {
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

add_resultGroupsLimit_tasks({
  testName: "flex nested limit 1",
  resultGroups: {
    children: [
      {
        limit: 5,
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

add_resultGroupsLimit_tasks({
  testName: "flex nested limit 2",
  resultGroups: {
    children: [
      {
        limit: 7,
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

add_resultGroupsLimit_tasks({
  testName: "flex nested limit 3",
  resultGroups: {
    children: [
      {
        limit: 7,
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
        limit: 3,
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

add_resultGroupsLimit_tasks({
  testName: "flex nested limit 4",
  resultGroups: {
    children: [
      {
        limit: 7,
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
        limit: 3,
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

add_resultGroupsLimit_tasks({
  testName: "flex nested limit 5",
  resultGroups: {
    children: [
      {
        limit: 7,
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
        limit: 3,
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

add_resultGroupsLimit_tasks({
  testName: "flex nested",
  resultGroups: {
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

add_resultGroupsLimit_tasks({
  testName: "flex nested overfill 1",
  resultGroups: {
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

add_resultGroupsLimit_tasks({
  testName: "flex nested overfill 2",
  resultGroups: {
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

add_resultGroupsLimit_tasks({
  testName: "flex nested overfill 3",
  resultGroups: {
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

add_resultGroupsLimit_tasks({
  testName: "limit ignored with flex",
  resultGroups: {
    flexChildren: true,
    children: [
      {
        limit: 1,
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
    // general/history: round(10 * (2 / (2 + 1))) = 7 -- limit ignored
    ...makeIndexRange(MAX_RESULTS, 7),
    // remote suggestions: round(10 * (1 / (2 + 1))) = 3
    ...makeIndexRange(0, 3),
  ],
});

add_resultGroupsLimit_tasks({
  testName: "resultSpan = 3 followed by others",
  resultGroups: {
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
    Object.assign(makeHistoryResults(1)[0], { resultSpan: 3 }),
  ],
  expectedResultIndexes: [
    // general/history: 1
    ...makeIndexRange(MAX_RESULTS, 1),
    // remote suggestions: maxResults - resultSpan of 3 = 10 - 3 = 7
    ...makeIndexRange(0, 7),
  ],
});

add_resultGroups_task({
  testName: "maxResultCount: 1, availableSpan: 3",
  resultGroups: {
    children: [
      {
        maxResultCount: 1,
        availableSpan: 3,
        group: UrlbarUtils.RESULT_GROUP.GENERAL,
      },
    ],
  },
  providerResults: [...makeHistoryResults(MAX_RESULTS)],
  expectedResultIndexes: [0],
});

add_resultGroups_task({
  testName: "maxResultCount: 1, availableSpan: 3, resultSpan = 3",
  resultGroups: {
    children: [
      {
        maxResultCount: 1,
        availableSpan: 3,
        group: UrlbarUtils.RESULT_GROUP.GENERAL,
      },
    ],
  },
  providerResults: [
    Object.assign(makeHistoryResults(1)[0], { resultSpan: 3 }),
    Object.assign(makeHistoryResults(1)[0], { resultSpan: 3 }),
    Object.assign(makeHistoryResults(1)[0], { resultSpan: 3 }),
  ],
  expectedResultIndexes: [0],
});

add_resultGroups_task({
  testName: "maxResultCount: 3, availableSpan: 1",
  resultGroups: {
    children: [
      {
        maxResultCount: 3,
        availableSpan: 1,
        group: UrlbarUtils.RESULT_GROUP.GENERAL,
      },
    ],
  },
  providerResults: [...makeHistoryResults(MAX_RESULTS)],
  expectedResultIndexes: [0],
});

add_resultGroups_task({
  testName: "maxResultCount: 3, availableSpan: 1, resultSpan = 3",
  resultGroups: {
    children: [
      {
        maxResultCount: 3,
        availableSpan: 1,
        group: UrlbarUtils.RESULT_GROUP.GENERAL,
      },
    ],
  },
  providerResults: [Object.assign(makeHistoryResults(1)[0], { resultSpan: 3 })],
  expectedResultIndexes: [],
});

add_resultGroups_task({
  testName: "availableSpan: 1",
  resultGroups: {
    children: [
      {
        availableSpan: 1,
        group: UrlbarUtils.RESULT_GROUP.GENERAL,
      },
    ],
  },
  providerResults: [...makeHistoryResults(MAX_RESULTS)],
  expectedResultIndexes: [0],
});

add_resultGroups_task({
  testName: "availableSpan: 1, resultSpan = 3",
  resultGroups: {
    children: [
      {
        availableSpan: 1,
        group: UrlbarUtils.RESULT_GROUP.GENERAL,
      },
    ],
  },
  providerResults: [Object.assign(makeHistoryResults(1)[0], { resultSpan: 3 })],
  expectedResultIndexes: [],
});

add_resultGroups_task({
  testName: "availableSpan: 3, resultSpan = 2 and resultSpan = 1",
  resultGroups: {
    children: [
      {
        availableSpan: 3,
        group: UrlbarUtils.RESULT_GROUP.GENERAL,
      },
    ],
  },
  providerResults: [
    makeHistoryResults(1)[0],
    Object.assign(makeHistoryResults(1)[0], { resultSpan: 2 }),
    makeHistoryResults(1)[0],
  ],
  expectedResultIndexes: [0, 1],
});

add_resultGroups_task({
  testName: "availableSpan: 3, resultSpan = 1 and resultSpan = 2",
  resultGroups: {
    children: [
      {
        availableSpan: 3,
        group: UrlbarUtils.RESULT_GROUP.GENERAL,
      },
    ],
  },
  providerResults: [
    Object.assign(makeHistoryResults(1)[0], { resultSpan: 2 }),
    makeHistoryResults(1)[0],
    makeHistoryResults(1)[0],
  ],
  expectedResultIndexes: [0, 1],
});

/**
 * Adds a single test task.
 *
 * @param {object} options
 *   The options for the test
 * @param {string} options.testName
 *   This name is logged with `info` as the task starts.
 * @param {object} options.resultGroups
 *   browser.urlbar.resultGroups is set to this value as the task starts.
 * @param {Array} options.providerResults
 *   Array of result objects that the test provider will add.
 * @param {Array} options.expectedResultIndexes
 *   Array of indexes in `providerResults` of the expected final results.
 */
function add_resultGroups_task({
  testName,
  resultGroups,
  providerResults,
  expectedResultIndexes,
}) {
  let func = async () => {
    info(`Running resultGroups test: ${testName}`);
    info(`Setting result groups: ` + JSON.stringify(resultGroups));
    setResultGroups(resultGroups);
    let provider = registerBasicTestProvider(providerResults);
    let context = createContext("foo", { providers: [provider.name] });
    let controller = UrlbarTestUtils.newMockController();
    await UrlbarProvidersManager.startQuery(context, controller);
    UrlbarProvidersManager.unregisterProvider(provider);
    let expectedResults = expectedResultIndexes.map(i => providerResults[i]);
    Assert.deepEqual(context.results, expectedResults);
    setResultGroups(null);
  };
  Object.defineProperty(func, "name", { value: testName });
  add_task(func);
}

/**
 * Adds test tasks for each of the keys in `LIMIT_KEYS`.
 *
 * @param {object} options
 *   The options for the test
 * @param {string} options.testName
 *   The name of the test.
 * @param {object} options.resultGroups
 *   The resultGroups object to set.
 * @param {Array} options.providerResults
 *   The results to return from the test
 * @param {Array} options.expectedResultIndexes
 *   Indexes of the expected results within {@link providerResults}
 */
function add_resultGroupsLimit_tasks({
  testName,
  resultGroups,
  providerResults,
  expectedResultIndexes,
}) {
  for (let key of LIMIT_KEYS) {
    add_resultGroups_task({
      testName: `${testName} (limit: ${key})`,
      resultGroups: replaceLimitWithKey(resultGroups, key),
      providerResults,
      expectedResultIndexes,
    });
  }
}

function replaceLimitWithKey(group, key) {
  group = { ...group };
  if ("limit" in group) {
    group[key] = group.limit;
    delete group.limit;
  }
  for (let i = 0; i < group.children?.length; i++) {
    group.children[i] = replaceLimitWithKey(group.children[i], key);
  }
  return group;
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

function setResultGroups(resultGroups) {
  sandbox.restore();
  if (resultGroups) {
    sandbox.stub(UrlbarPrefs, "resultGroups").get(() => resultGroups);
  }
}
