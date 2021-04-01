/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

// This test checks row visibility during view updates when rows with suggested
// indexes are added and removed. Each task performs two searches: Search 1
// returns 4 results with search suggestions, and search 2 returns 4 results
// with URL results.

"use strict";

// Search 1:
//   4 results, no suggestedIndex
// Search 2:
//   4 results including suggestedIndex = 1
// Expected visible rows during update:
//   4 original rows with no changes
add_suggestedIndex_task({
  search1: {
    otherCount: 3,
    viewCount: 4,
  },
  search2: {
    otherCount: 2,
    suggestedIndex: 1,
    viewCount: 4,
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
    { count: 2, type: UrlbarUtils.RESULT_TYPE.URL, hidden: true },
  ],
});

// Search 1:
//   4 results, no suggestedIndex
// Search 2:
//   4 results including suggestedIndex = 2
// Expected visible rows during update:
//   4 original rows + 1 new row (the one before the suggestedIndex row)
add_suggestedIndex_task({
  search1: {
    otherCount: 3,
    viewCount: 4,
  },
  search2: {
    otherCount: 2,
    suggestedIndex: 2,
    viewCount: 4,
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
    { count: 1, type: UrlbarUtils.RESULT_TYPE.URL, hidden: true },
  ],
});

// Search 1:
//   4 results, no suggestedIndex
// Search 2:
//   4 results including suggestedIndex = 9
// Expected visible rows during update:
//   4 original rows + 2 new rows (the ones before the suggestedIndex row)
add_suggestedIndex_task({
  search1: {
    otherCount: 3,
    viewCount: 4,
  },
  search2: {
    otherCount: 2,
    suggestedIndex: 9,
    viewCount: 4,
  },
  duringUpdate: [
    { count: 1 },
    { count: 3, type: UrlbarUtils.RESULT_TYPE.SEARCH, stale: true },
    { count: 2, type: UrlbarUtils.RESULT_TYPE.URL },
    {
      count: 1,
      type: UrlbarUtils.RESULT_TYPE.URL,
      suggestedIndex: 9,
    },
  ],
});

// Search 1:
//   4 results, no suggestedIndex
// Search 2:
//   4 results including suggestedIndex = -1
// Expected visible rows during update:
//   4 original rows + 2 new rows (the ones before the suggestedIndex row)
add_suggestedIndex_task({
  search1: {
    otherCount: 3,
    viewCount: 4,
  },
  search2: {
    otherCount: 2,
    suggestedIndex: -1,
    viewCount: 4,
  },
  duringUpdate: [
    { count: 1 },
    { count: 3, type: UrlbarUtils.RESULT_TYPE.SEARCH, stale: true },
    { count: 2, type: UrlbarUtils.RESULT_TYPE.URL },
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
//   4 results including suggestedIndex = -2
// Expected visible rows during update:
//   4 original rows + 1 new row (the one before the suggestedIndex row)
add_suggestedIndex_task({
  search1: {
    otherCount: 3,
    viewCount: 4,
  },
  search2: {
    otherCount: 2,
    suggestedIndex: -2,
    viewCount: 4,
  },
  duringUpdate: [
    { count: 1 },
    { count: 3, type: UrlbarUtils.RESULT_TYPE.SEARCH, stale: true },
    { count: 1, type: UrlbarUtils.RESULT_TYPE.URL },
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
//   4 results including suggestedIndex = 1
// Expected visible rows during update:
//   4 original rows + 2 new rows
add_suggestedIndex_task({
  search1: {
    otherCount: 2,
    suggestedIndex: 1,
    viewCount: 4,
  },
  search2: {
    otherCount: 2,
    suggestedIndex: 1,
    viewCount: 4,
  },
  duringUpdate: [
    { count: 1 },
    {
      count: 1,
      type: UrlbarUtils.RESULT_TYPE.URL,
      suggestedIndex: 1,
    },
    { count: 2, type: UrlbarUtils.RESULT_TYPE.SEARCH, stale: true },
    { count: 2, type: UrlbarUtils.RESULT_TYPE.URL },
  ],
});

// Search 1:
//   4 results including suggestedIndex = 1
// Search 2:
//   4 results including suggestedIndex = 2
// Expected visible rows during update:
//   4 original rows with no changes
add_suggestedIndex_task({
  search1: {
    otherCount: 2,
    suggestedIndex: 1,
    viewCount: 4,
  },
  search2: {
    otherCount: 2,
    suggestedIndex: 2,
    viewCount: 4,
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
    { count: 1, type: UrlbarUtils.RESULT_TYPE.URL, hidden: true },
  ],
});

// Search 1:
//   4 results including suggestedIndex = 1
// Search 2:
//   4 results including suggestedIndex = 9
// Expected visible rows during update:
//   4 original rows with no changes
add_suggestedIndex_task({
  search1: {
    otherCount: 2,
    suggestedIndex: 1,
    viewCount: 4,
  },
  search2: {
    otherCount: 2,
    suggestedIndex: 9,
    viewCount: 4,
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
//   4 results including suggestedIndex = 1
// Search 2:
//   4 results including suggestedIndex = -1
// Expected visible rows during update:
//   4 original rows with no changes
add_suggestedIndex_task({
  search1: {
    otherCount: 2,
    suggestedIndex: 1,
    viewCount: 4,
  },
  search2: {
    otherCount: 2,
    suggestedIndex: -1,
    viewCount: 4,
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
//   4 results including suggestedIndex = 1
// Search 2:
//   4 results including suggestedIndex = -2
// Expected visible rows during update:
//   4 original rows with no changes
add_suggestedIndex_task({
  search1: {
    otherCount: 2,
    suggestedIndex: 1,
    viewCount: 4,
  },
  search2: {
    otherCount: 2,
    suggestedIndex: -2,
    viewCount: 4,
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
      suggestedIndex: -2,
      hidden: true,
    },
    { count: 1, type: UrlbarUtils.RESULT_TYPE.URL, hidden: true },
  ],
});

// Search 1:
//   4 results including suggestedIndex = 1
// Search 2:
//   4 results including suggestedIndex = -3
// Expected visible rows during update:
//   4 original rows with no changes
add_suggestedIndex_task({
  search1: {
    otherCount: 2,
    suggestedIndex: 1,
    viewCount: 4,
  },
  search2: {
    otherCount: 2,
    suggestedIndex: -3,
    viewCount: 4,
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
      suggestedIndex: -3,
      hidden: true,
    },
    { count: 2, type: UrlbarUtils.RESULT_TYPE.URL, hidden: true },
  ],
});

// Search 1:
//   4 results including suggestedIndex = 9
// Search 2:
//   4 results including suggestedIndex = 1
// Expected visible rows during update:
//   4 original rows with no changes
add_suggestedIndex_task({
  search1: {
    otherCount: 2,
    suggestedIndex: 9,
    viewCount: 4,
  },
  search2: {
    otherCount: 2,
    suggestedIndex: 1,
    viewCount: 4,
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
    { count: 2, type: UrlbarUtils.RESULT_TYPE.URL, hidden: true },
  ],
});

// Search 1:
//   4 results including suggestedIndex = 9
// Search 2:
//   4 results including suggestedIndex = 2
// Expected visible rows during update:
//   4 original rows with no changes
add_suggestedIndex_task({
  search1: {
    otherCount: 2,
    suggestedIndex: 9,
    viewCount: 4,
  },
  search2: {
    otherCount: 2,
    suggestedIndex: 2,
    viewCount: 4,
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
    { count: 1, type: UrlbarUtils.RESULT_TYPE.URL, hidden: true },
    {
      count: 1,
      type: UrlbarUtils.RESULT_TYPE.URL,
      suggestedIndex: 2,
      hidden: true,
    },
    { count: 1, type: UrlbarUtils.RESULT_TYPE.URL, hidden: true },
  ],
});

// Search 1:
//   4 results including suggestedIndex = 9
// Search 2:
//   4 results including suggestedIndex = 9
// Expected visible rows during update:
//   4 original rows with no changes (because the original search results can't
//   be replaced with URL results)
add_suggestedIndex_task({
  search1: {
    otherCount: 2,
    suggestedIndex: 9,
    viewCount: 4,
  },
  search2: {
    otherCount: 2,
    suggestedIndex: 9,
    viewCount: 4,
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
      suggestedIndex: 9,
      hidden: true,
    },
  ],
});

// Search 1:
//   4 results including suggestedIndex = 9
// Search 2:
//   4 results including suggestedIndex = -1
// Expected visible rows during update:
//   4 original rows with no changes
add_suggestedIndex_task({
  search1: {
    otherCount: 2,
    suggestedIndex: 9,
    viewCount: 4,
  },
  search2: {
    otherCount: 2,
    suggestedIndex: -1,
    viewCount: 4,
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
      suggestedIndex: -1,
      hidden: true,
    },
  ],
});

// Search 1:
//   4 results including suggestedIndex = 9
// Search 2:
//   4 results including suggestedIndex = -2
// Expected visible rows during update:
//   4 original rows with no changes
add_suggestedIndex_task({
  search1: {
    otherCount: 2,
    suggestedIndex: 9,
    viewCount: 4,
  },
  search2: {
    otherCount: 2,
    suggestedIndex: -2,
    viewCount: 4,
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
//   4 results including suggestedIndex = 9
// Search 2:
//   4 results including suggestedIndex = -3
// Expected visible rows during update:
//   4 original rows with no changes
add_suggestedIndex_task({
  search1: {
    otherCount: 2,
    suggestedIndex: 9,
    viewCount: 4,
  },
  search2: {
    otherCount: 2,
    suggestedIndex: -3,
    viewCount: 4,
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
      suggestedIndex: -3,
      hidden: true,
    },
    { count: 2, type: UrlbarUtils.RESULT_TYPE.URL, hidden: true },
  ],
});

// Search 1:
//   4 results including suggestedIndex = -1
// Search 2:
//   4 results including suggestedIndex = 1
// Expected visible rows during update:
//   4 original rows with no changes
add_suggestedIndex_task({
  search1: {
    otherCount: 2,
    suggestedIndex: -1,
    viewCount: 4,
  },
  search2: {
    otherCount: 2,
    suggestedIndex: 1,
    viewCount: 4,
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
    { count: 2, type: UrlbarUtils.RESULT_TYPE.URL, hidden: true },
  ],
});

// Search 1:
//   4 results including suggestedIndex = -1
// Search 2:
//   4 results including suggestedIndex = 2
// Expected visible rows during update:
//   4 original rows with no changes
add_suggestedIndex_task({
  search1: {
    otherCount: 2,
    suggestedIndex: -1,
    viewCount: 4,
  },
  search2: {
    otherCount: 2,
    suggestedIndex: 2,
    viewCount: 4,
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
    { count: 1, type: UrlbarUtils.RESULT_TYPE.URL, hidden: true },
    {
      count: 1,
      type: UrlbarUtils.RESULT_TYPE.URL,
      suggestedIndex: 2,
      hidden: true,
    },
    { count: 1, type: UrlbarUtils.RESULT_TYPE.URL, hidden: true },
  ],
});

// Search 1:
//   4 results including suggestedIndex = -1
// Search 2:
//   4 results including suggestedIndex = 9
// Expected visible rows during update:
//   4 original rows with no changes
add_suggestedIndex_task({
  search1: {
    otherCount: 2,
    suggestedIndex: -1,
    viewCount: 4,
  },
  search2: {
    otherCount: 2,
    suggestedIndex: 9,
    viewCount: 4,
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
//   4 results including suggestedIndex = -1
// Search 2:
//   4 results including suggestedIndex = -1
// Expected visible rows during update:
//   4 original rows with no changes
add_suggestedIndex_task({
  search1: {
    otherCount: 2,
    suggestedIndex: -1,
    viewCount: 4,
  },
  search2: {
    otherCount: 2,
    suggestedIndex: -1,
    viewCount: 4,
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
//   4 results including suggestedIndex = -1
// Search 2:
//   4 results including suggestedIndex = -2
// Expected visible rows during update:
//   4 original rows with no changes
add_suggestedIndex_task({
  search1: {
    otherCount: 2,
    suggestedIndex: -1,
    viewCount: 4,
  },
  search2: {
    otherCount: 2,
    suggestedIndex: -2,
    viewCount: 4,
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
//   4 results including suggestedIndex = -2
// Search 2:
//   4 results including suggestedIndex = 1
// Expected visible rows during update:
//   4 original rows with no changes
add_suggestedIndex_task({
  search1: {
    otherCount: 2,
    suggestedIndex: -2,
    viewCount: 4,
  },
  search2: {
    otherCount: 2,
    suggestedIndex: 1,
    viewCount: 4,
  },
  duringUpdate: [
    { count: 1 },
    { count: 1, type: UrlbarUtils.RESULT_TYPE.SEARCH, stale: true },
    {
      count: 1,
      type: UrlbarUtils.RESULT_TYPE.URL,
      suggestedIndex: -2,
      stale: true,
    },
    { count: 1, type: UrlbarUtils.RESULT_TYPE.SEARCH, stale: true },
    {
      count: 1,
      type: UrlbarUtils.RESULT_TYPE.URL,
      suggestedIndex: 1,
      hidden: true,
    },
    { count: 2, type: UrlbarUtils.RESULT_TYPE.URL, hidden: true },
  ],
});

// Search 1:
//   4 results including suggestedIndex = -2
// Search 2:
//   4 results including suggestedIndex = 2
// Expected visible rows during update:
//   4 original rows with no changes
add_suggestedIndex_task({
  search1: {
    otherCount: 2,
    suggestedIndex: -2,
    viewCount: 4,
  },
  search2: {
    otherCount: 2,
    suggestedIndex: 2,
    viewCount: 4,
  },
  duringUpdate: [
    { count: 1 },
    { count: 1, type: UrlbarUtils.RESULT_TYPE.SEARCH, stale: true },
    {
      count: 1,
      type: UrlbarUtils.RESULT_TYPE.URL,
      suggestedIndex: -2,
      stale: true,
    },
    { count: 1, type: UrlbarUtils.RESULT_TYPE.SEARCH, stale: true },
    { count: 1, type: UrlbarUtils.RESULT_TYPE.URL, hidden: true },
    {
      count: 1,
      type: UrlbarUtils.RESULT_TYPE.URL,
      suggestedIndex: 2,
      hidden: true,
    },
    { count: 1, type: UrlbarUtils.RESULT_TYPE.URL, hidden: true },
  ],
});

// Search 1:
//   4 results including suggestedIndex = -2
// Search 2:
//   4 results including suggestedIndex = 9
// Expected visible rows during update:
//   4 original rows with no changes
add_suggestedIndex_task({
  search1: {
    otherCount: 2,
    suggestedIndex: -2,
    viewCount: 4,
  },
  search2: {
    otherCount: 2,
    suggestedIndex: 9,
    viewCount: 4,
  },
  duringUpdate: [
    { count: 1 },
    { count: 1, type: UrlbarUtils.RESULT_TYPE.SEARCH, stale: true },
    {
      count: 1,
      type: UrlbarUtils.RESULT_TYPE.URL,
      suggestedIndex: -2,
      stale: true,
    },
    { count: 1, type: UrlbarUtils.RESULT_TYPE.SEARCH, stale: true },
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
//   4 results including suggestedIndex = -2
// Search 2:
//   4 results including suggestedIndex = -1
// Expected visible rows during update:
//   4 original rows with no changes
add_suggestedIndex_task({
  search1: {
    otherCount: 2,
    suggestedIndex: -2,
    viewCount: 4,
  },
  search2: {
    otherCount: 2,
    suggestedIndex: -1,
    viewCount: 4,
  },
  duringUpdate: [
    { count: 1 },
    { count: 1, type: UrlbarUtils.RESULT_TYPE.SEARCH, stale: true },
    {
      count: 1,
      type: UrlbarUtils.RESULT_TYPE.URL,
      suggestedIndex: -2,
      stale: true,
    },
    { count: 1, type: UrlbarUtils.RESULT_TYPE.SEARCH, stale: true },
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
//   4 results including suggestedIndex = -2
// Search 2:
//   4 results including suggestedIndex = -2
// Expected visible rows during update:
//   4 original rows with no changes (because the original search results can't
//   be replaced with URL results)
add_suggestedIndex_task({
  search1: {
    otherCount: 2,
    suggestedIndex: -2,
    viewCount: 4,
  },
  search2: {
    otherCount: 2,
    suggestedIndex: -2,
    viewCount: 4,
  },
  duringUpdate: [
    { count: 1 },
    { count: 1, type: UrlbarUtils.RESULT_TYPE.SEARCH, stale: true },
    {
      count: 1,
      type: UrlbarUtils.RESULT_TYPE.URL,
      suggestedIndex: -2,
      stale: true,
    },
    { count: 1, type: UrlbarUtils.RESULT_TYPE.SEARCH, stale: true },
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
//   4 results including suggestedIndex = 1 and suggestedIndex = -1
// Expected visible rows during update:
//   4 original rows with no changes
add_suggestedIndex_task({
  search1: {
    otherCount: 3,
    viewCount: 4,
  },
  search2: {
    otherCount: 1,
    suggestedIndexes: [1, -1],
    viewCount: 4,
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
//   4 results including suggestedIndex = 1
// Search 2:
//   4 results including suggestedIndex = 1 and suggestedIndex = -1
// Expected visible rows during update:
//   4 original rows + 1 new row (the one before the suggestedIndex row)
add_suggestedIndex_task({
  search1: {
    otherCount: 2,
    suggestedIndex: 1,
    viewCount: 4,
  },
  search2: {
    otherCount: 1,
    suggestedIndexes: [1, -1],
    viewCount: 4,
  },
  duringUpdate: [
    { count: 1 },
    {
      count: 1,
      type: UrlbarUtils.RESULT_TYPE.URL,
      suggestedIndex: 1,
    },
    { count: 2, type: UrlbarUtils.RESULT_TYPE.SEARCH, stale: true },
    { count: 1, type: UrlbarUtils.RESULT_TYPE.URL },
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
//   4 results including suggestedIndex = 1 and suggestedIndex = -1
// Expected visible rows during update:
//   4 original rows with no changes
add_suggestedIndex_task({
  search1: {
    otherCount: 2,
    suggestedIndex: -1,
    viewCount: 4,
  },
  search2: {
    otherCount: 1,
    suggestedIndexes: [1, -1],
    viewCount: 4,
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
    { count: 1, type: UrlbarUtils.RESULT_TYPE.URL, hidden: true },
    {
      count: 1,
      type: UrlbarUtils.RESULT_TYPE.URL,
      suggestedIndex: -1,
      hidden: true,
    },
  ],
});
