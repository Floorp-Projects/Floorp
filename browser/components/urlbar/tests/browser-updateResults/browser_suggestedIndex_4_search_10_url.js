/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

// This test checks row visibility during view updates when rows with suggested
// indexes are added and removed. Each task performs two searches: Search 1
// returns 4 results with search suggestions, and search 2 returns 10 results
// with URL results.

"use strict";

// Search 1:
//   4 results, no suggestedIndex
// Search 2:
//   10 results including suggestedIndex = 1
// Expected visible rows during update:
//   4 original rows with no changes
add_suggestedIndex_task({
  search1: {
    otherCount: 3,
    viewCount: 4,
  },
  search2: {
    otherCount: 10,
    suggestedIndex: 1,
    viewCount: 10,
  },
  duringUpdate: [
    { count: 1 },
    { count: 3, type: UrlbarUtils.RESULT_TYPE.SEARCH, stale: true },
    {
      count: 1,
      type: UrlbarUtils.RESULT_TYPE.URL,
      suggestedIndex: 1,
      hidden: true,
    },
    { count: 8, type: UrlbarUtils.RESULT_TYPE.URL, hidden: true },
  ],
});

// Search 1:
//   4 results, no suggestedIndex
// Search 2:
//   10 results including suggestedIndex = 2
// Expected visible rows during update:
//   4 original rows + 1 new row (the one before the suggestedIndex row)
add_suggestedIndex_task({
  search1: {
    otherCount: 3,
    viewCount: 4,
  },
  search2: {
    otherCount: 10,
    suggestedIndex: 2,
    viewCount: 10,
  },
  duringUpdate: [
    { count: 1 },
    { count: 3, type: UrlbarUtils.RESULT_TYPE.SEARCH, stale: true },
    { count: 1, type: UrlbarUtils.RESULT_TYPE.URL },
    {
      count: 1,
      type: UrlbarUtils.RESULT_TYPE.URL,
      suggestedIndex: 2,
      hidden: true,
    },
    { count: 7, type: UrlbarUtils.RESULT_TYPE.URL, hidden: true },
  ],
});

// Search 1:
//   4 results, no suggestedIndex
// Search 2:
//   10 results including suggestedIndex = 4
// Expected visible rows during update:
//   4 original rows + 3 new rows (the ones before the suggestedIndex row)
add_suggestedIndex_task({
  search1: {
    otherCount: 3,
    viewCount: 4,
  },
  search2: {
    otherCount: 10,
    suggestedIndex: 4,
    viewCount: 10,
  },
  duringUpdate: [
    { count: 1 },
    { count: 3, type: UrlbarUtils.RESULT_TYPE.SEARCH, stale: true },
    { count: 3, type: UrlbarUtils.RESULT_TYPE.URL },
    {
      count: 1,
      type: UrlbarUtils.RESULT_TYPE.URL,
      suggestedIndex: 4,
      hidden: true,
    },
    { count: 5, type: UrlbarUtils.RESULT_TYPE.URL, hidden: true },
  ],
});

// Search 1:
//   4 results, no suggestedIndex
// Search 2:
//   10 results including suggestedIndex = 6
// Expected visible rows during update:
//   4 original rows + 5 new rows (the ones before the suggestedIndex row)
add_suggestedIndex_task({
  search1: {
    otherCount: 3,
    viewCount: 4,
  },
  search2: {
    otherCount: 10,
    suggestedIndex: 6,
    viewCount: 10,
  },
  duringUpdate: [
    { count: 1 },
    { count: 3, type: UrlbarUtils.RESULT_TYPE.SEARCH, stale: true },
    { count: 5, type: UrlbarUtils.RESULT_TYPE.URL },
    {
      count: 1,
      type: UrlbarUtils.RESULT_TYPE.URL,
      suggestedIndex: 6,
      hidden: true,
    },
    { count: 3, type: UrlbarUtils.RESULT_TYPE.URL, hidden: true },
  ],
});

// Search 1:
//   4 results, no suggestedIndex
// Search 2:
//   10 results including suggestedIndex = 8
// Expected visible rows during update:
//   4 original rows + 6 new rows (some of the ones before the suggestedIndex)
add_suggestedIndex_task({
  search1: {
    otherCount: 3,
    viewCount: 4,
  },
  search2: {
    otherCount: 10,
    suggestedIndex: 8,
    viewCount: 10,
  },
  duringUpdate: [
    { count: 1 },
    { count: 3, type: UrlbarUtils.RESULT_TYPE.SEARCH, stale: true },
    { count: 6, type: UrlbarUtils.RESULT_TYPE.URL },
    { count: 1, type: UrlbarUtils.RESULT_TYPE.URL, hidden: true },
    {
      count: 1,
      type: UrlbarUtils.RESULT_TYPE.URL,
      suggestedIndex: 8,
      hidden: true,
    },
    { count: 1, type: UrlbarUtils.RESULT_TYPE.URL, hidden: true },
  ],
});

// Search 1:
//   4 results, no suggestedIndex
// Search 2:
//   10 results including suggestedIndex = 9
// Expected visible rows during update:
//   4 original rows + 6 new rows (some of the ones before the suggestedIndex)
add_suggestedIndex_task({
  search1: {
    otherCount: 3,
    viewCount: 4,
  },
  search2: {
    otherCount: 10,
    suggestedIndex: 9,
    viewCount: 10,
  },
  duringUpdate: [
    { count: 1 },
    { count: 3, type: UrlbarUtils.RESULT_TYPE.SEARCH, stale: true },
    { count: 6, type: UrlbarUtils.RESULT_TYPE.URL },
    { count: 2, type: UrlbarUtils.RESULT_TYPE.URL, hidden: true },
    {
      count: 1,
      type: UrlbarUtils.RESULT_TYPE.URL,
      suggestedIndex: 9,
      hidden: true,
    },
  ],
});

// Search 1:
//   4 results, no suggestedIndex
// Search 2:
//   10 results including suggestedIndex = -1
// Expected visible rows during update:
//   4 original rows + 6 new rows
add_suggestedIndex_task({
  search1: {
    otherCount: 3,
    viewCount: 4,
  },
  search2: {
    otherCount: 10,
    suggestedIndex: -1,
    viewCount: 10,
  },
  duringUpdate: [
    { count: 1 },
    { count: 3, type: UrlbarUtils.RESULT_TYPE.SEARCH, stale: true },
    { count: 6, type: UrlbarUtils.RESULT_TYPE.URL },
    { count: 2, type: UrlbarUtils.RESULT_TYPE.URL, hidden: true },
    {
      count: 1,
      type: UrlbarUtils.RESULT_TYPE.URL,
      suggestedIndex: -1,
      hidden: true,
    },
  ],
});

// Search 1:
//   4 results, no suggestedIndex
// Search 2:
//   10 results including suggestedIndex = -2
// Expected visible rows during update:
//   4 original rows + 6 new rows
add_suggestedIndex_task({
  search1: {
    otherCount: 3,
    viewCount: 4,
  },
  search2: {
    otherCount: 10,
    suggestedIndex: -2,
    viewCount: 10,
  },
  duringUpdate: [
    { count: 1 },
    { count: 3, type: UrlbarUtils.RESULT_TYPE.SEARCH, stale: true },
    { count: 6, type: UrlbarUtils.RESULT_TYPE.URL },
    { count: 1, type: UrlbarUtils.RESULT_TYPE.URL, hidden: true },
    {
      count: 1,
      type: UrlbarUtils.RESULT_TYPE.URL,
      suggestedIndex: -2,
      hidden: true,
    },
    { count: 1, type: UrlbarUtils.RESULT_TYPE.URL, hidden: true },
  ],
});

// Search 1:
//   4 results, no suggestedIndex
// Search 2:
//   10 results including suggestedIndex = -4
// Expected visible rows during update:
//   4 original rows + 5 new rows (the ones before the suggestedIndex row)
add_suggestedIndex_task({
  search1: {
    otherCount: 3,
    viewCount: 4,
  },
  search2: {
    otherCount: 10,
    suggestedIndex: -4,
    viewCount: 10,
  },
  duringUpdate: [
    { count: 1 },
    { count: 3, type: UrlbarUtils.RESULT_TYPE.SEARCH, stale: true },
    { count: 5, type: UrlbarUtils.RESULT_TYPE.URL },
    {
      count: 1,
      type: UrlbarUtils.RESULT_TYPE.URL,
      suggestedIndex: -4,
      hidden: true,
    },
    { count: 3, type: UrlbarUtils.RESULT_TYPE.URL, hidden: true },
  ],
});

// Search 1:
//   4 results, no suggestedIndex
// Search 2:
//   10 results including suggestedIndex = -6
// Expected visible rows during update:
//   4 original rows + 3 new rows (the ones before the suggestedIndex row)
add_suggestedIndex_task({
  search1: {
    otherCount: 3,
    viewCount: 4,
  },
  search2: {
    otherCount: 10,
    suggestedIndex: -6,
    viewCount: 10,
  },
  duringUpdate: [
    { count: 1 },
    { count: 3, type: UrlbarUtils.RESULT_TYPE.SEARCH, stale: true },
    { count: 3, type: UrlbarUtils.RESULT_TYPE.URL },
    {
      count: 1,
      type: UrlbarUtils.RESULT_TYPE.URL,
      suggestedIndex: -6,
      hidden: true,
    },
    { count: 5, type: UrlbarUtils.RESULT_TYPE.URL, hidden: true },
  ],
});

// Search 1:
//   4 results, no suggestedIndex
// Search 2:
//   10 results including suggestedIndex = -8
// Expected visible rows during update:
//   4 original rows + 1 new rows (the ones before the suggestedIndex row)
add_suggestedIndex_task({
  search1: {
    otherCount: 3,
    viewCount: 4,
  },
  search2: {
    otherCount: 10,
    suggestedIndex: -8,
    viewCount: 10,
  },
  duringUpdate: [
    { count: 1 },
    { count: 3, type: UrlbarUtils.RESULT_TYPE.SEARCH, stale: true },
    { count: 1, type: UrlbarUtils.RESULT_TYPE.URL },
    {
      count: 1,
      type: UrlbarUtils.RESULT_TYPE.URL,
      suggestedIndex: -8,
      hidden: true,
    },
    { count: 7, type: UrlbarUtils.RESULT_TYPE.URL, hidden: true },
  ],
});

// Search 1:
//   4 results including suggestedIndex = 1
// Search 2:
//   10 results including suggestedIndex = 1
// Expected visible rows during update:
//   4 original rows + 6 new rows
add_suggestedIndex_task({
  search1: {
    otherCount: 2,
    suggestedIndex: 1,
    viewCount: 4,
  },
  search2: {
    otherCount: 10,
    suggestedIndex: 1,
    viewCount: 10,
  },
  duringUpdate: [
    { count: 1 },
    {
      count: 1,
      type: UrlbarUtils.RESULT_TYPE.URL,
      suggestedIndex: 1,
    },
    { count: 2, type: UrlbarUtils.RESULT_TYPE.SEARCH, stale: true },
    { count: 6, type: UrlbarUtils.RESULT_TYPE.URL },
    { count: 2, type: UrlbarUtils.RESULT_TYPE.URL, hidden: true },
  ],
});

// Search 1:
//   4 results including suggestedIndex = 1
// Search 2:
//   10 results including suggestedIndex = 2
// Expected visible rows during update:
//   4 original rows with no changes
add_suggestedIndex_task({
  search1: {
    otherCount: 2,
    suggestedIndex: 1,
    viewCount: 4,
  },
  search2: {
    otherCount: 10,
    suggestedIndex: 2,
    viewCount: 10,
  },
  duringUpdate: [
    { count: 1 },
    {
      count: 1,
      type: UrlbarUtils.RESULT_TYPE.URL,
      suggestedIndex: 1,
      stale: true,
    },
    { count: 2, type: UrlbarUtils.RESULT_TYPE.SEARCH, stale: true },
    { count: 1, type: UrlbarUtils.RESULT_TYPE.URL, hidden: true },
    {
      count: 1,
      type: UrlbarUtils.RESULT_TYPE.URL,
      suggestedIndex: 2,
      hidden: true,
    },
    { count: 7, type: UrlbarUtils.RESULT_TYPE.URL, hidden: true },
  ],
});

// Search 1:
//   4 results including suggestedIndex = 1
// Search 2:
//   10 results including suggestedIndex = 9
// Expected visible rows during update:
//   4 original rows with no changes
add_suggestedIndex_task({
  search1: {
    otherCount: 2,
    suggestedIndex: 1,
    viewCount: 4,
  },
  search2: {
    otherCount: 10,
    suggestedIndex: 9,
    viewCount: 10,
  },
  duringUpdate: [
    { count: 1 },
    {
      count: 1,
      type: UrlbarUtils.RESULT_TYPE.URL,
      suggestedIndex: 1,
      stale: true,
    },
    { count: 2, type: UrlbarUtils.RESULT_TYPE.SEARCH, stale: true },
    { count: 8, type: UrlbarUtils.RESULT_TYPE.URL, hidden: true },
    {
      count: 1,
      type: UrlbarUtils.RESULT_TYPE.URL,
      suggestedIndex: 9,
      hidden: true,
    },
  ],
});

// Search 1:
//   4 results including suggestedIndex = 1
// Search 2:
//   10 results including suggestedIndex = -1
// Expected visible rows during update:
//   4 original rows with no changes
add_suggestedIndex_task({
  search1: {
    otherCount: 2,
    suggestedIndex: 1,
    viewCount: 4,
  },
  search2: {
    otherCount: 10,
    suggestedIndex: -1,
    viewCount: 10,
  },
  duringUpdate: [
    { count: 1 },
    {
      count: 1,
      type: UrlbarUtils.RESULT_TYPE.URL,
      suggestedIndex: 1,
      stale: true,
    },
    { count: 2, type: UrlbarUtils.RESULT_TYPE.SEARCH, stale: true },
    { count: 8, type: UrlbarUtils.RESULT_TYPE.URL, hidden: true },
    {
      count: 1,
      type: UrlbarUtils.RESULT_TYPE.URL,
      suggestedIndex: -1,
      hidden: true,
    },
  ],
});

// Search 1:
//   4 results including suggestedIndex = 1
// Search 2:
//   10 results including suggestedIndex = -2
// Expected visible rows during update:
//   4 original rows with no changes
add_suggestedIndex_task({
  search1: {
    otherCount: 2,
    suggestedIndex: 1,
    viewCount: 4,
  },
  search2: {
    otherCount: 10,
    suggestedIndex: -2,
    viewCount: 10,
  },
  duringUpdate: [
    { count: 1 },
    {
      count: 1,
      type: UrlbarUtils.RESULT_TYPE.URL,
      suggestedIndex: 1,
      stale: true,
    },
    { count: 2, type: UrlbarUtils.RESULT_TYPE.SEARCH, stale: true },
    { count: 7, type: UrlbarUtils.RESULT_TYPE.URL, hidden: true },
    {
      count: 1,
      type: UrlbarUtils.RESULT_TYPE.URL,
      suggestedIndex: -2,
      hidden: true,
    },
    { count: 1, type: UrlbarUtils.RESULT_TYPE.URL, hidden: true },
  ],
});

// Search 1:
//   4 results including suggestedIndex = 1
// Search 2:
//   10 results including suggestedIndex = -9
// Expected visible rows during update:
//   4 original rows with no changes
add_suggestedIndex_task({
  search1: {
    otherCount: 2,
    suggestedIndex: 1,
    viewCount: 4,
  },
  search2: {
    otherCount: 10,
    suggestedIndex: -9,
    viewCount: 10,
  },
  duringUpdate: [
    { count: 1 },
    {
      count: 1,
      type: UrlbarUtils.RESULT_TYPE.URL,
      suggestedIndex: 1,
      stale: true,
    },
    { count: 2, type: UrlbarUtils.RESULT_TYPE.SEARCH, stale: true },
    {
      count: 1,
      type: UrlbarUtils.RESULT_TYPE.URL,
      suggestedIndex: -9,
      hidden: true,
    },
    { count: 8, type: UrlbarUtils.RESULT_TYPE.URL, hidden: true },
  ],
});

// Search 1:
//   4 results including suggestedIndex = 9
// Search 2:
//   10 results including suggestedIndex = 1
// Expected visible rows during update:
//   4 original rows with no changes
add_suggestedIndex_task({
  search1: {
    otherCount: 2,
    suggestedIndex: 9,
    viewCount: 4,
  },
  search2: {
    otherCount: 10,
    suggestedIndex: 1,
    viewCount: 10,
  },
  duringUpdate: [
    { count: 1 },
    { count: 2, type: UrlbarUtils.RESULT_TYPE.SEARCH, stale: true },
    {
      count: 1,
      type: UrlbarUtils.RESULT_TYPE.URL,
      suggestedIndex: 9,
      stale: true,
    },
    {
      count: 1,
      type: UrlbarUtils.RESULT_TYPE.URL,
      suggestedIndex: 1,
      hidden: true,
    },
    { count: 8, type: UrlbarUtils.RESULT_TYPE.URL, hidden: true },
  ],
});

// Search 1:
//   4 results including suggestedIndex = 9
// Search 2:
//   10 results including suggestedIndex = 3
// Expected visible rows during update:
//   4 original rows with no changes
add_suggestedIndex_task({
  search1: {
    otherCount: 2,
    suggestedIndex: 9,
    viewCount: 4,
  },
  search2: {
    otherCount: 10,
    suggestedIndex: 3,
    viewCount: 10,
  },
  duringUpdate: [
    { count: 1 },
    { count: 2, type: UrlbarUtils.RESULT_TYPE.SEARCH, stale: true },
    {
      count: 1,
      type: UrlbarUtils.RESULT_TYPE.URL,
      suggestedIndex: 9,
      stale: true,
    },
    { count: 2, type: UrlbarUtils.RESULT_TYPE.URL, hidden: true },
    {
      count: 1,
      type: UrlbarUtils.RESULT_TYPE.URL,
      suggestedIndex: 3,
      hidden: true,
    },
    { count: 6, type: UrlbarUtils.RESULT_TYPE.URL, hidden: true },
  ],
});

// Search 1:
//   4 results including suggestedIndex = 9
// Search 2:
//   10 results including suggestedIndex = 9
// Expected visible rows during update:
//   4 original rows with no changes
add_suggestedIndex_task({
  search1: {
    otherCount: 2,
    suggestedIndex: 9,
    viewCount: 4,
  },
  search2: {
    otherCount: 10,
    suggestedIndex: 9,
    viewCount: 10,
  },
  duringUpdate: [
    { count: 1 },
    { count: 2, type: UrlbarUtils.RESULT_TYPE.SEARCH, stale: true },
    {
      count: 1,
      type: UrlbarUtils.RESULT_TYPE.URL,
      suggestedIndex: 9,
      stale: true,
    },
    { count: 8, type: UrlbarUtils.RESULT_TYPE.URL, hidden: true },
    {
      count: 1,
      type: UrlbarUtils.RESULT_TYPE.URL,
      suggestedIndex: 9,
      hidden: true,
    },
  ],
});

// Search 1:
//   4 results including suggestedIndex = 9
// Search 2:
//   10 results including suggestedIndex = -1
// Expected visible rows during update:
//   4 original rows with no changes
add_suggestedIndex_task({
  search1: {
    otherCount: 2,
    suggestedIndex: 9,
    viewCount: 4,
  },
  search2: {
    otherCount: 10,
    suggestedIndex: -1,
    viewCount: 10,
  },
  duringUpdate: [
    { count: 1 },
    { count: 2, type: UrlbarUtils.RESULT_TYPE.SEARCH, stale: true },
    {
      count: 1,
      type: UrlbarUtils.RESULT_TYPE.URL,
      suggestedIndex: 9,
      stale: true,
    },
    { count: 8, type: UrlbarUtils.RESULT_TYPE.URL, hidden: true },
    {
      count: 1,
      type: UrlbarUtils.RESULT_TYPE.URL,
      suggestedIndex: -1,
      hidden: true,
    },
  ],
});

// Search 1:
//   4 results including suggestedIndex = 9
// Search 2:
//   10 results including suggestedIndex = -7
// Expected visible rows during update:
//   4 original rows with no changes
add_suggestedIndex_task({
  search1: {
    otherCount: 2,
    suggestedIndex: 9,
    viewCount: 4,
  },
  search2: {
    otherCount: 10,
    suggestedIndex: -7,
    viewCount: 10,
  },
  duringUpdate: [
    { count: 1 },
    { count: 2, type: UrlbarUtils.RESULT_TYPE.SEARCH, stale: true },
    {
      count: 1,
      type: UrlbarUtils.RESULT_TYPE.URL,
      suggestedIndex: 9,
      stale: true,
    },
    { count: 2, type: UrlbarUtils.RESULT_TYPE.URL, hidden: true },
    {
      count: 1,
      type: UrlbarUtils.RESULT_TYPE.URL,
      suggestedIndex: -7,
      hidden: true,
    },
    { count: 6, type: UrlbarUtils.RESULT_TYPE.URL, hidden: true },
  ],
});

// Search 1:
//   4 results, no suggestedIndex
// Search 2:
//   10 results including suggestedIndex = 1 and suggestedIndex = -1
// Expected visible rows during update:
//   4 original rows with no changes
add_suggestedIndex_task({
  search1: {
    otherCount: 3,
    viewCount: 4,
  },
  search2: {
    otherCount: 10,
    suggestedIndexes: [1, -1],
    viewCount: 10,
  },
  duringUpdate: [
    { count: 1 },
    { count: 3, type: UrlbarUtils.RESULT_TYPE.SEARCH, stale: true },
    {
      count: 1,
      type: UrlbarUtils.RESULT_TYPE.URL,
      suggestedIndex: 1,
      hidden: true,
    },
    { count: 7, type: UrlbarUtils.RESULT_TYPE.URL, hidden: true },
    {
      count: 1,
      type: UrlbarUtils.RESULT_TYPE.URL,
      suggestedIndex: -1,
      hidden: true,
    },
  ],
});

// Search 1:
//   4 results including suggestedIndex = 1
// Search 2:
//   10 results including suggestedIndex = 1 and suggestedIndex = -1
// Expected visible rows during update:
//   4 original rows + 6 new rows (some of the ones before the suggestedIndex)
add_suggestedIndex_task({
  search1: {
    otherCount: 2,
    suggestedIndex: 1,
    viewCount: 4,
  },
  search2: {
    otherCount: 10,
    suggestedIndexes: [1, -1],
    viewCount: 10,
  },
  duringUpdate: [
    { count: 1 },
    {
      count: 1,
      type: UrlbarUtils.RESULT_TYPE.URL,
      suggestedIndex: 1,
    },
    { count: 2, type: UrlbarUtils.RESULT_TYPE.SEARCH, stale: true },
    { count: 6, type: UrlbarUtils.RESULT_TYPE.URL },
    { count: 1, type: UrlbarUtils.RESULT_TYPE.URL, hidden: true },
    {
      count: 1,
      type: UrlbarUtils.RESULT_TYPE.URL,
      suggestedIndex: -1,
      hidden: true,
    },
  ],
});

// Search 1:
//   4 results including suggestedIndex = -1
// Search 2:
//   10 results including suggestedIndex = 1 and suggestedIndex = -1
// Expected visible rows during update:
//   4 original rows with no changes
add_suggestedIndex_task({
  search1: {
    otherCount: 2,
    suggestedIndex: -1,
    viewCount: 4,
  },
  search2: {
    otherCount: 10,
    suggestedIndexes: [1, -1],
    viewCount: 10,
  },
  duringUpdate: [
    { count: 1 },
    { count: 2, type: UrlbarUtils.RESULT_TYPE.SEARCH, stale: true },
    {
      count: 1,
      type: UrlbarUtils.RESULT_TYPE.URL,
      suggestedIndex: -1,
      stale: true,
    },
    {
      count: 1,
      type: UrlbarUtils.RESULT_TYPE.URL,
      suggestedIndex: 1,
      hidden: true,
    },
    { count: 7, type: UrlbarUtils.RESULT_TYPE.URL, hidden: true },
    {
      count: 1,
      type: UrlbarUtils.RESULT_TYPE.URL,
      suggestedIndex: -1,
      hidden: true,
    },
  ],
});

// Search 1:
//   4 results, no suggestedIndex
// Search 2:
//   9 results including suggestedIndex = 1 with resultSpan = 2
// Expected visible rows during update:
//   4 original rows with no changes
add_suggestedIndex_task({
  search1: {
    otherCount: 3,
    viewCount: 4,
  },
  search2: {
    otherCount: 10,
    suggestedIndex: 1,
    resultSpan: 2,
    viewCount: 9,
  },
  duringUpdate: [
    { count: 1 },
    { count: 3, type: UrlbarUtils.RESULT_TYPE.SEARCH, stale: true },
    {
      count: 1,
      type: UrlbarUtils.RESULT_TYPE.URL,
      suggestedIndex: 1,
      resultSpan: 2,
      hidden: true,
    },
    { count: 7, type: UrlbarUtils.RESULT_TYPE.URL, hidden: true },
  ],
});

// Search 1:
//   4 results, no suggestedIndex
// Search 2:
//   9 results including:
//     suggestedIndex = 1 with resultSpan = 2
//     suggestedIndex = -1
// Expected visible rows during update:
//   4 original rows with no changes
add_suggestedIndex_task({
  search1: {
    otherCount: 3,
    viewCount: 4,
  },
  search2: {
    otherCount: 10,
    suggestedIndexes: [[1, 2], -1],
    viewCount: 9,
  },
  duringUpdate: [
    { count: 1 },
    { count: 3, type: UrlbarUtils.RESULT_TYPE.SEARCH, stale: true },
    {
      count: 1,
      type: UrlbarUtils.RESULT_TYPE.URL,
      suggestedIndex: 1,
      resultSpan: 2,
      hidden: true,
    },
    { count: 6, type: UrlbarUtils.RESULT_TYPE.URL, hidden: true },
    {
      count: 1,
      type: UrlbarUtils.RESULT_TYPE.URL,
      suggestedIndex: -1,
      hidden: true,
    },
  ],
});

// Search 1:
//   4 results including suggestedIndex = 1 with resultSpan = 2
// Search 2:
//   9 results including:
//     suggestedIndex = 1 with resultSpan = 2
//     suggestedIndex = -1
// Expected visible rows during update:
//   4 original rows + 5 new rows (some of the ones before the suggestedIndex)
add_suggestedIndex_task({
  search1: {
    otherCount: 2,
    suggestedIndexes: [[1, 2]],
    viewCount: 4,
  },
  search2: {
    otherCount: 10,
    suggestedIndexes: [[1, 2], -1],
    viewCount: 9,
  },
  duringUpdate: [
    { count: 1 },
    {
      count: 1,
      type: UrlbarUtils.RESULT_TYPE.URL,
      suggestedIndex: 1,
      resultSpan: 2,
    },
    { count: 2, type: UrlbarUtils.RESULT_TYPE.SEARCH, stale: true },
    { count: 5, type: UrlbarUtils.RESULT_TYPE.URL },
    { count: 1, type: UrlbarUtils.RESULT_TYPE.URL, hidden: true },
    {
      count: 1,
      type: UrlbarUtils.RESULT_TYPE.URL,
      suggestedIndex: -1,
      hidden: true,
    },
  ],
});
