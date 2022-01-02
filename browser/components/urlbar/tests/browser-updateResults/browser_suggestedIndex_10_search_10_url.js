/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

// This test checks row visibility during view updates when rows with suggested
// indexes are added and removed. Each task performs two searches: Search 1
// returns 10 results with search suggestions, and search 2 returns 10 results
// with URL results.

"use strict";

// Search 1:
//   10 results, no suggestedIndex
// Search 2:
//   10 results including suggestedIndex = 1
// Expected visible rows during update:
//   10 original rows with no changes
add_suggestedIndex_task({
  search1: {
    otherCount: 10,
    otherType: UrlbarUtils.RESULT_TYPE.SEARCH,
    viewCount: 10,
  },
  search2: {
    otherCount: 10,
    otherType: UrlbarUtils.RESULT_TYPE.URL,
    suggestedIndex: 1,
    viewCount: 10,
  },
  duringUpdate: [
    { count: 1 },
    { count: 9, type: UrlbarUtils.RESULT_TYPE.SEARCH, stale: true },
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
//   10 results, no suggestedIndex
// Search 2:
//   10 results including suggestedIndex = 2
// Expected visible rows during update:
//   10 original rows with no changes
add_suggestedIndex_task({
  search1: {
    otherCount: 10,
    otherType: UrlbarUtils.RESULT_TYPE.SEARCH,
    viewCount: 10,
  },
  search2: {
    otherCount: 10,
    otherType: UrlbarUtils.RESULT_TYPE.URL,
    suggestedIndex: 2,
    viewCount: 10,
  },
  duringUpdate: [
    { count: 1 },
    { count: 9, type: UrlbarUtils.RESULT_TYPE.SEARCH, stale: true },
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
//   10 results, no suggestedIndex
// Search 2:
//   10 results including suggestedIndex = 9
// Expected visible rows during update:
//   10 original rows with no changes
add_suggestedIndex_task({
  search1: {
    otherCount: 10,
    otherType: UrlbarUtils.RESULT_TYPE.SEARCH,
    viewCount: 10,
  },
  search2: {
    otherCount: 10,
    otherType: UrlbarUtils.RESULT_TYPE.URL,
    suggestedIndex: 9,
    viewCount: 10,
  },
  duringUpdate: [
    { count: 1 },
    { count: 9, type: UrlbarUtils.RESULT_TYPE.SEARCH, stale: true },
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
//   10 results, no suggestedIndex
// Search 2:
//   10 results including suggestedIndex = -1
// Expected visible rows during update:
//   10 original rows with no changes
add_suggestedIndex_task({
  search1: {
    otherCount: 10,
    otherType: UrlbarUtils.RESULT_TYPE.SEARCH,
    viewCount: 10,
  },
  search2: {
    otherCount: 10,
    otherType: UrlbarUtils.RESULT_TYPE.URL,
    suggestedIndex: -1,
    viewCount: 10,
  },
  duringUpdate: [
    { count: 1 },
    { count: 9, type: UrlbarUtils.RESULT_TYPE.SEARCH, stale: true },
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
//   10 results, no suggestedIndex
// Search 2:
//   10 results including suggestedIndex = -2
// Expected visible rows during update:
//   10 original rows with no changes
add_suggestedIndex_task({
  search1: {
    otherCount: 10,
    otherType: UrlbarUtils.RESULT_TYPE.SEARCH,
    viewCount: 10,
  },
  search2: {
    otherCount: 10,
    otherType: UrlbarUtils.RESULT_TYPE.URL,
    suggestedIndex: -2,
    viewCount: 10,
  },
  duringUpdate: [
    { count: 1 },
    { count: 9, type: UrlbarUtils.RESULT_TYPE.SEARCH, stale: true },
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
//   10 results, no suggestedIndex
// Search 2:
//   10 results including suggestedIndex = -9
// Expected visible rows during update:
//   10 original rows with no changes
add_suggestedIndex_task({
  search1: {
    otherCount: 10,
    otherType: UrlbarUtils.RESULT_TYPE.SEARCH,
    viewCount: 10,
  },
  search2: {
    otherCount: 10,
    otherType: UrlbarUtils.RESULT_TYPE.URL,
    suggestedIndex: -9,
    viewCount: 10,
  },
  duringUpdate: [
    { count: 1 },
    { count: 9, type: UrlbarUtils.RESULT_TYPE.SEARCH, stale: true },
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
//   10 results including suggestedIndex = 1
// Search 2:
//   10 results including suggestedIndex = 1
// Expected visible rows during update:
//   10 original rows with no changes (because the original search results can't
//   be replaced with URL results)
add_suggestedIndex_task({
  search1: {
    otherCount: 10,
    otherType: UrlbarUtils.RESULT_TYPE.SEARCH,
    suggestedIndex: 1,
    viewCount: 10,
  },
  search2: {
    otherCount: 10,
    otherType: UrlbarUtils.RESULT_TYPE.URL,
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
    { count: 8, type: UrlbarUtils.RESULT_TYPE.SEARCH, stale: true },
    { count: 8, type: UrlbarUtils.RESULT_TYPE.URL, hidden: true },
  ],
});

// Search 1:
//   10 results including suggestedIndex = 1
// Search 2:
//   10 results including suggestedIndex = 2
// Expected visible rows during update:
//   10 original rows with no changes
add_suggestedIndex_task({
  search1: {
    otherCount: 10,
    otherType: UrlbarUtils.RESULT_TYPE.SEARCH,
    suggestedIndex: 1,
    viewCount: 10,
  },
  search2: {
    otherCount: 10,
    otherType: UrlbarUtils.RESULT_TYPE.URL,
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
    { count: 8, type: UrlbarUtils.RESULT_TYPE.SEARCH, stale: true },
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
//   10 results including suggestedIndex = 1
// Search 2:
//   10 results including suggestedIndex = 9
// Expected visible rows during update:
//   10 original rows with no changes
add_suggestedIndex_task({
  search1: {
    otherCount: 10,
    otherType: UrlbarUtils.RESULT_TYPE.SEARCH,
    suggestedIndex: 1,
    viewCount: 10,
  },
  search2: {
    otherCount: 10,
    otherType: UrlbarUtils.RESULT_TYPE.URL,
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
    { count: 8, type: UrlbarUtils.RESULT_TYPE.SEARCH, stale: true },
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
//   10 results including suggestedIndex = 1
// Search 2:
//   10 results including suggestedIndex = -1
// Expected visible rows during update:
//   10 original rows with no changes
add_suggestedIndex_task({
  search1: {
    otherCount: 10,
    otherType: UrlbarUtils.RESULT_TYPE.SEARCH,
    suggestedIndex: 1,
    viewCount: 10,
  },
  search2: {
    otherCount: 10,
    otherType: UrlbarUtils.RESULT_TYPE.URL,
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
    { count: 8, type: UrlbarUtils.RESULT_TYPE.SEARCH, stale: true },
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
//   10 results including suggestedIndex = 1
// Search 2:
//   10 results including suggestedIndex = -2
// Expected visible rows during update:
//   10 original rows with no changes
add_suggestedIndex_task({
  search1: {
    otherCount: 10,
    otherType: UrlbarUtils.RESULT_TYPE.SEARCH,
    suggestedIndex: 1,
    viewCount: 10,
  },
  search2: {
    otherCount: 10,
    otherType: UrlbarUtils.RESULT_TYPE.URL,
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
    { count: 8, type: UrlbarUtils.RESULT_TYPE.SEARCH, stale: true },
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
//   10 results including suggestedIndex = 1
// Search 2:
//   10 results including suggestedIndex = -9
// Expected visible rows during update:
//   10 original rows with no changes
add_suggestedIndex_task({
  search1: {
    otherCount: 10,
    otherType: UrlbarUtils.RESULT_TYPE.SEARCH,
    suggestedIndex: 1,
    viewCount: 10,
  },
  search2: {
    otherCount: 10,
    otherType: UrlbarUtils.RESULT_TYPE.URL,
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
    { count: 8, type: UrlbarUtils.RESULT_TYPE.SEARCH, stale: true },
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
//   10 results including suggestedIndex = 9
// Search 2:
//   10 results including suggestedIndex = 1
// Expected visible rows during update:
//   10 original rows with no changes
add_suggestedIndex_task({
  search1: {
    otherCount: 10,
    otherType: UrlbarUtils.RESULT_TYPE.SEARCH,
    suggestedIndex: 9,
    viewCount: 10,
  },
  search2: {
    otherCount: 10,
    otherType: UrlbarUtils.RESULT_TYPE.URL,
    suggestedIndex: 1,
    viewCount: 10,
  },
  duringUpdate: [
    { count: 1 },
    { count: 8, type: UrlbarUtils.RESULT_TYPE.SEARCH, stale: true },
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
//   10 results including suggestedIndex = 9
// Search 2:
//   10 results including suggestedIndex = 9
// Expected visible rows during update:
//   10 original rows with no changes
add_suggestedIndex_task({
  search1: {
    otherCount: 10,
    otherType: UrlbarUtils.RESULT_TYPE.SEARCH,
    suggestedIndex: 9,
    viewCount: 10,
  },
  search2: {
    otherCount: 10,
    otherType: UrlbarUtils.RESULT_TYPE.URL,
    suggestedIndex: 9,
    viewCount: 10,
  },
  duringUpdate: [
    { count: 1 },
    { count: 8, type: UrlbarUtils.RESULT_TYPE.SEARCH, stale: true },
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
//   10 results including suggestedIndex = 9
// Search 2:
//   10 results including suggestedIndex = -1
// Expected visible rows during update:
//   10 original rows with no changes
add_suggestedIndex_task({
  search1: {
    otherCount: 10,
    otherType: UrlbarUtils.RESULT_TYPE.SEARCH,
    suggestedIndex: 9,
    viewCount: 10,
  },
  search2: {
    otherCount: 10,
    otherType: UrlbarUtils.RESULT_TYPE.URL,
    suggestedIndex: -1,
    viewCount: 10,
  },
  duringUpdate: [
    { count: 1 },
    { count: 8, type: UrlbarUtils.RESULT_TYPE.SEARCH, stale: true },
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
//   10 results including suggestedIndex = 9
// Search 2:
//   10 results including suggestedIndex = -9
// Expected visible rows during update:
//   10 original rows with no changes
add_suggestedIndex_task({
  search1: {
    otherCount: 10,
    otherType: UrlbarUtils.RESULT_TYPE.SEARCH,
    suggestedIndex: 9,
    viewCount: 10,
  },
  search2: {
    otherCount: 10,
    otherType: UrlbarUtils.RESULT_TYPE.URL,
    suggestedIndex: -9,
    viewCount: 10,
  },
  duringUpdate: [
    { count: 1 },
    { count: 8, type: UrlbarUtils.RESULT_TYPE.SEARCH, stale: true },
    {
      count: 1,
      type: UrlbarUtils.RESULT_TYPE.URL,
      suggestedIndex: 9,
      stale: true,
    },
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
//   10 results including suggestedIndex = -1
// Search 2:
//   10 results including suggestedIndex = 1
// Expected visible rows during update:
//   10 original rows with no changes
add_suggestedIndex_task({
  search1: {
    otherCount: 10,
    otherType: UrlbarUtils.RESULT_TYPE.SEARCH,
    suggestedIndex: -1,
    viewCount: 10,
  },
  search2: {
    otherCount: 10,
    otherType: UrlbarUtils.RESULT_TYPE.URL,
    suggestedIndex: 1,
    viewCount: 10,
  },
  duringUpdate: [
    { count: 1 },
    { count: 8, type: UrlbarUtils.RESULT_TYPE.SEARCH, stale: true },
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
    { count: 8, type: UrlbarUtils.RESULT_TYPE.URL, hidden: true },
  ],
});

// Search 1:
//   10 results including suggestedIndex = -1
// Search 2:
//   10 results including suggestedIndex = 9
// Expected visible rows during update:
//   10 original rows with no changes
add_suggestedIndex_task({
  search1: {
    otherCount: 10,
    otherType: UrlbarUtils.RESULT_TYPE.SEARCH,
    suggestedIndex: -1,
    viewCount: 10,
  },
  search2: {
    otherCount: 10,
    otherType: UrlbarUtils.RESULT_TYPE.URL,
    suggestedIndex: 9,
    viewCount: 10,
  },
  duringUpdate: [
    { count: 1 },
    { count: 8, type: UrlbarUtils.RESULT_TYPE.SEARCH, stale: true },
    {
      count: 1,
      type: UrlbarUtils.RESULT_TYPE.URL,
      suggestedIndex: -1,
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
//   10 results including suggestedIndex = -1
// Search 2:
//   10 results including suggestedIndex = -1
// Expected visible rows during update:
//   10 original rows with no changes
add_suggestedIndex_task({
  search1: {
    otherCount: 10,
    otherType: UrlbarUtils.RESULT_TYPE.SEARCH,
    suggestedIndex: -1,
    viewCount: 10,
  },
  search2: {
    otherCount: 10,
    otherType: UrlbarUtils.RESULT_TYPE.URL,
    suggestedIndex: -1,
    viewCount: 10,
  },
  duringUpdate: [
    { count: 1 },
    { count: 8, type: UrlbarUtils.RESULT_TYPE.SEARCH, stale: true },
    {
      count: 1,
      type: UrlbarUtils.RESULT_TYPE.URL,
      suggestedIndex: -1,
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
//   10 results including suggestedIndex = -1
// Search 2:
//   10 results including suggestedIndex = -9
// Expected visible rows during update:
//   10 original rows with no changes
add_suggestedIndex_task({
  search1: {
    otherCount: 10,
    otherType: UrlbarUtils.RESULT_TYPE.SEARCH,
    suggestedIndex: -1,
    viewCount: 10,
  },
  search2: {
    otherCount: 10,
    otherType: UrlbarUtils.RESULT_TYPE.URL,
    suggestedIndex: -9,
    viewCount: 10,
  },
  duringUpdate: [
    { count: 1 },
    { count: 8, type: UrlbarUtils.RESULT_TYPE.SEARCH, stale: true },
    {
      count: 1,
      type: UrlbarUtils.RESULT_TYPE.URL,
      suggestedIndex: -1,
      stale: true,
    },
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
//   10 results including suggestedIndex = -9
// Search 2:
//   10 results including suggestedIndex = 1
// Expected visible rows during update:
//   10 original rows with no changes
add_suggestedIndex_task({
  search1: {
    otherCount: 10,
    otherType: UrlbarUtils.RESULT_TYPE.SEARCH,
    suggestedIndex: -9,
    viewCount: 10,
  },
  search2: {
    otherCount: 10,
    otherType: UrlbarUtils.RESULT_TYPE.URL,
    suggestedIndex: 1,
    viewCount: 10,
  },
  duringUpdate: [
    { count: 1 },
    {
      count: 1,
      type: UrlbarUtils.RESULT_TYPE.URL,
      suggestedIndex: -9,
      stale: true,
    },
    { count: 8, type: UrlbarUtils.RESULT_TYPE.SEARCH, stale: true },
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
//   10 results including suggestedIndex = -9
// Search 2:
//   10 results including suggestedIndex = 9
// Expected visible rows during update:
//   10 original rows with no changes
add_suggestedIndex_task({
  search1: {
    otherCount: 10,
    otherType: UrlbarUtils.RESULT_TYPE.SEARCH,
    suggestedIndex: -9,
    viewCount: 10,
  },
  search2: {
    otherCount: 10,
    otherType: UrlbarUtils.RESULT_TYPE.URL,
    suggestedIndex: 9,
    viewCount: 10,
  },
  duringUpdate: [
    { count: 1 },
    {
      count: 1,
      type: UrlbarUtils.RESULT_TYPE.URL,
      suggestedIndex: -9,
      stale: true,
    },
    { count: 8, type: UrlbarUtils.RESULT_TYPE.SEARCH, stale: true },
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
//   10 results including suggestedIndex = -9
// Search 2:
//   10 results including suggestedIndex = -1
// Expected visible rows during update:
//   10 original rows with no changes
add_suggestedIndex_task({
  search1: {
    otherCount: 10,
    otherType: UrlbarUtils.RESULT_TYPE.SEARCH,
    suggestedIndex: -9,
    viewCount: 10,
  },
  search2: {
    otherCount: 10,
    otherType: UrlbarUtils.RESULT_TYPE.URL,
    suggestedIndex: -1,
    viewCount: 10,
  },
  duringUpdate: [
    { count: 1 },
    {
      count: 1,
      type: UrlbarUtils.RESULT_TYPE.URL,
      suggestedIndex: -9,
      stale: true,
    },
    { count: 8, type: UrlbarUtils.RESULT_TYPE.SEARCH, stale: true },
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
//   10 results including suggestedIndex = -9
// Search 2:
//   10 results including suggestedIndex = -9
// Expected visible rows during update:
//   10 original rows with no changes
add_suggestedIndex_task({
  search1: {
    otherCount: 10,
    otherType: UrlbarUtils.RESULT_TYPE.SEARCH,
    suggestedIndex: -9,
    viewCount: 10,
  },
  search2: {
    otherCount: 10,
    otherType: UrlbarUtils.RESULT_TYPE.URL,
    suggestedIndex: -9,
    viewCount: 10,
  },
  duringUpdate: [
    { count: 1 },
    {
      count: 1,
      type: UrlbarUtils.RESULT_TYPE.URL,
      suggestedIndex: -9,
    },
    { count: 8, type: UrlbarUtils.RESULT_TYPE.SEARCH, stale: true },
    { count: 8, type: UrlbarUtils.RESULT_TYPE.URL, hidden: true },
  ],
});

// Search 1:
//   10 results, no suggestedIndex
// Search 2:
//   10 results including suggestedIndex = 1 and suggestedIndex = -1
// Expected visible rows during update:
//   10 original rows with no changes
add_suggestedIndex_task({
  search1: {
    otherCount: 10,
    otherType: UrlbarUtils.RESULT_TYPE.SEARCH,
    viewCount: 10,
  },
  search2: {
    otherCount: 10,
    otherType: UrlbarUtils.RESULT_TYPE.URL,
    suggestedIndexes: [1, -1],
    viewCount: 10,
  },
  duringUpdate: [
    { count: 1 },
    { count: 9, type: UrlbarUtils.RESULT_TYPE.SEARCH, stale: true },
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
//   10 results including suggestedIndex = 1
// Search 2:
//   10 results including suggestedIndex = 1 and suggestedIndex = -1
// Expected visible rows during update:
//   10 original rows with no changes
add_suggestedIndex_task({
  search1: {
    otherCount: 10,
    otherType: UrlbarUtils.RESULT_TYPE.SEARCH,
    suggestedIndex: 1,
    viewCount: 10,
  },
  search2: {
    otherCount: 10,
    otherType: UrlbarUtils.RESULT_TYPE.URL,
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
    { count: 8, type: UrlbarUtils.RESULT_TYPE.SEARCH, stale: true },
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
//   10 results including suggestedIndex = -1
// Search 2:
//   10 results including suggestedIndex = 1 and suggestedIndex = -1
// Expected visible rows during update:
//   10 original rows with no changes
add_suggestedIndex_task({
  search1: {
    otherCount: 10,
    otherType: UrlbarUtils.RESULT_TYPE.SEARCH,
    suggestedIndex: -1,
    viewCount: 10,
  },
  search2: {
    otherCount: 10,
    otherType: UrlbarUtils.RESULT_TYPE.URL,
    suggestedIndexes: [1, -1],
    viewCount: 10,
  },
  duringUpdate: [
    { count: 1 },
    { count: 8, type: UrlbarUtils.RESULT_TYPE.SEARCH, stale: true },
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
//   10 results, no suggestedIndex
// Search 2:
//   9 results including suggestedIndex = 1 with resultSpan = 2
// Expected visible rows during update:
//   10 original rows with no changes
add_suggestedIndex_task({
  search1: {
    otherCount: 10,
    otherType: UrlbarUtils.RESULT_TYPE.SEARCH,
    viewCount: 10,
  },
  search2: {
    otherCount: 10,
    otherType: UrlbarUtils.RESULT_TYPE.URL,
    suggestedIndex: 1,
    resultSpan: 2,
    viewCount: 9,
  },
  duringUpdate: [
    { count: 1 },
    { count: 9, type: UrlbarUtils.RESULT_TYPE.SEARCH, stale: true },
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
//   10 results, no suggestedIndex
// Search 2:
//   9 results including:
//     suggestedIndex = 1 with resultSpan = 2
//     suggestedIndex = -1
// Expected visible rows during update:
//   10 original rows with no changes
add_suggestedIndex_task({
  search1: {
    otherCount: 10,
    otherType: UrlbarUtils.RESULT_TYPE.SEARCH,
    viewCount: 10,
  },
  search2: {
    otherCount: 10,
    otherType: UrlbarUtils.RESULT_TYPE.URL,
    suggestedIndexes: [[1, 2], -1],
    viewCount: 9,
  },
  duringUpdate: [
    { count: 1 },
    { count: 9, type: UrlbarUtils.RESULT_TYPE.SEARCH, stale: true },
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
//   9 results including suggestedIndex = 1 with resultSpan = 2
// Search 2:
//   9 results including:
//     suggestedIndex = 1 with resultSpan = 2
//     suggestedIndex = -1
// Expected visible rows during update:
//   9 original rows with no changes
add_suggestedIndex_task({
  search1: {
    otherCount: 10,
    otherType: UrlbarUtils.RESULT_TYPE.SEARCH,
    suggestedIndexes: [[1, 2]],
    viewCount: 9,
  },
  search2: {
    otherCount: 10,
    otherType: UrlbarUtils.RESULT_TYPE.URL,
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
    { count: 7, type: UrlbarUtils.RESULT_TYPE.SEARCH, stale: true },
    { count: 6, type: UrlbarUtils.RESULT_TYPE.URL, hidden: true },
    {
      count: 1,
      type: UrlbarUtils.RESULT_TYPE.URL,
      suggestedIndex: -1,
      hidden: true,
    },
  ],
});
