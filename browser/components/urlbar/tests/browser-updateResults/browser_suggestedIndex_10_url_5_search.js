/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

// This test checks row visibility during view updates when rows with suggested
// indexes are added and removed. Each task performs two searches: Search 1
// returns 10 results where the first result is a search suggestion and the
// remainder are URL results, and search 2 returns 5 results with search
// suggestions. This tests the view-update logic that allows search suggestions
// to replace other results once an existing suggestion row is encountered.

"use strict";

// Search 1:
//   10 results, no suggestedIndex
// Search 2:
//   5 results including suggestedIndex = 1
// Expected visible rows during update:
//   10 original rows with no changes
add_suggestedIndex_task({
  search1: {
    other: [
      { count: 1, type: UrlbarUtils.RESULT_TYPE.SEARCH },
      { count: 9, type: UrlbarUtils.RESULT_TYPE.URL },
    ],
    viewCount: 10,
  },
  search2: {
    otherCount: 3,
    otherType: UrlbarUtils.RESULT_TYPE.SEARCH,
    suggestedIndex: 1,
    viewCount: 5,
  },
  duringUpdate: [
    { count: 1 },
    { count: 1, type: UrlbarUtils.RESULT_TYPE.SEARCH, stale: true },
    { count: 8, type: UrlbarUtils.RESULT_TYPE.URL, stale: true },
    {
      count: 1,
      type: UrlbarUtils.RESULT_TYPE.URL,
      suggestedIndex: 1,
      hidden: true,
    },
    { count: 3, type: UrlbarUtils.RESULT_TYPE.SEARCH, hidden: true },
  ],
});

// Search 1:
//   10 results, no suggestedIndex
// Search 2:
//   5 results including suggestedIndex = 2
// Expected visible rows during update:
//   10 original rows with no changes
add_suggestedIndex_task({
  search1: {
    other: [
      { count: 1, type: UrlbarUtils.RESULT_TYPE.SEARCH },
      { count: 9, type: UrlbarUtils.RESULT_TYPE.URL },
    ],
    viewCount: 10,
  },
  search2: {
    otherCount: 3,
    otherType: UrlbarUtils.RESULT_TYPE.SEARCH,
    suggestedIndex: 2,
    viewCount: 5,
  },
  duringUpdate: [
    { count: 1 },
    { count: 1, type: UrlbarUtils.RESULT_TYPE.SEARCH },
    { count: 8, type: UrlbarUtils.RESULT_TYPE.URL, stale: true },
    {
      count: 1,
      type: UrlbarUtils.RESULT_TYPE.URL,
      suggestedIndex: 2,
      hidden: true,
    },
    { count: 2, type: UrlbarUtils.RESULT_TYPE.SEARCH, hidden: true },
  ],
});

// Search 1:
//   10 results, no suggestedIndex
// Search 2:
//   5 results including suggestedIndex = 9
// Expected visible rows during update:
//   Indexes 2 and 3 replaced with search suggestions, no other changes
add_suggestedIndex_task({
  search1: {
    other: [
      { count: 1, type: UrlbarUtils.RESULT_TYPE.SEARCH },
      { count: 9, type: UrlbarUtils.RESULT_TYPE.URL },
    ],
    viewCount: 10,
  },
  search2: {
    otherCount: 3,
    otherType: UrlbarUtils.RESULT_TYPE.SEARCH,
    suggestedIndex: 9,
    viewCount: 5,
  },
  duringUpdate: [
    { count: 1 },
    { count: 3, type: UrlbarUtils.RESULT_TYPE.SEARCH },
    { count: 6, type: UrlbarUtils.RESULT_TYPE.URL, stale: true },
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
//   5 results including suggestedIndex = -1
// Expected visible rows during update:
//   Indexes 2 and 3 replaced with search suggestions, no other changes
add_suggestedIndex_task({
  search1: {
    other: [
      { count: 1, type: UrlbarUtils.RESULT_TYPE.SEARCH },
      { count: 9, type: UrlbarUtils.RESULT_TYPE.URL },
    ],
    viewCount: 10,
  },
  search2: {
    otherCount: 3,
    otherType: UrlbarUtils.RESULT_TYPE.SEARCH,
    suggestedIndex: -1,
    viewCount: 5,
  },
  duringUpdate: [
    { count: 1 },
    { count: 3, type: UrlbarUtils.RESULT_TYPE.SEARCH },
    { count: 6, type: UrlbarUtils.RESULT_TYPE.URL, stale: true },
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
//   5 results including suggestedIndex = -2
// Expected visible rows during update:
//   Indexes 1 and 2 replaced with search suggestions, no other changes
add_suggestedIndex_task({
  search1: {
    other: [
      { count: 1, type: UrlbarUtils.RESULT_TYPE.SEARCH },
      { count: 9, type: UrlbarUtils.RESULT_TYPE.URL },
    ],
    viewCount: 10,
  },
  search2: {
    otherCount: 3,
    otherType: UrlbarUtils.RESULT_TYPE.SEARCH,
    suggestedIndex: -2,
    viewCount: 5,
  },
  duringUpdate: [
    { count: 1 },
    { count: 2, type: UrlbarUtils.RESULT_TYPE.SEARCH },
    { count: 7, type: UrlbarUtils.RESULT_TYPE.URL, stale: true },
    {
      count: 1,
      type: UrlbarUtils.RESULT_TYPE.URL,
      suggestedIndex: -2,
      hidden: true,
    },
    { count: 1, type: UrlbarUtils.RESULT_TYPE.SEARCH, hidden: true },
  ],
});

// Search 1:
//   10 results including suggestedIndex = 1
// Search 2:
//   5 results including suggestedIndex = 1
// Expected visible rows during update:
//   Indexes 3 and 4 replaced with search suggestions, no other changes
add_suggestedIndex_task({
  search1: {
    other: [
      { count: 1, type: UrlbarUtils.RESULT_TYPE.SEARCH },
      { count: 9, type: UrlbarUtils.RESULT_TYPE.URL },
    ],
    suggestedIndex: 1,
    viewCount: 10,
  },
  search2: {
    otherCount: 3,
    otherType: UrlbarUtils.RESULT_TYPE.SEARCH,
    suggestedIndex: 1,
    viewCount: 5,
  },
  duringUpdate: [
    { count: 1 },
    {
      count: 1,
      type: UrlbarUtils.RESULT_TYPE.URL,
      suggestedIndex: 1,
    },
    { count: 3, type: UrlbarUtils.RESULT_TYPE.SEARCH },
    { count: 5, type: UrlbarUtils.RESULT_TYPE.URL, stale: true },
  ],
});

// Search 1:
//   10 results including suggestedIndex = 1
// Search 2:
//   5 results including suggestedIndex = 9
// Expected visible rows during update:
//   10 original rows with no changes
add_suggestedIndex_task({
  search1: {
    other: [
      { count: 1, type: UrlbarUtils.RESULT_TYPE.SEARCH },
      { count: 9, type: UrlbarUtils.RESULT_TYPE.URL },
    ],
    suggestedIndex: 1,
    viewCount: 10,
  },
  search2: {
    otherCount: 3,
    otherType: UrlbarUtils.RESULT_TYPE.SEARCH,
    suggestedIndex: 9,
    viewCount: 5,
  },
  duringUpdate: [
    { count: 1 },
    {
      count: 1,
      type: UrlbarUtils.RESULT_TYPE.URL,
      suggestedIndex: 1,
      stale: true,
    },
    { count: 1, type: UrlbarUtils.RESULT_TYPE.SEARCH, stale: true },
    { count: 7, type: UrlbarUtils.RESULT_TYPE.URL, stale: true },
    { count: 3, type: UrlbarUtils.RESULT_TYPE.SEARCH, hidden: true },
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
//   5 results including suggestedIndex = -1
// Expected visible rows during update:
//   10 original rows with no changes
add_suggestedIndex_task({
  search1: {
    other: [
      { count: 1, type: UrlbarUtils.RESULT_TYPE.SEARCH },
      { count: 9, type: UrlbarUtils.RESULT_TYPE.URL },
    ],
    suggestedIndex: 1,
    viewCount: 10,
  },
  search2: {
    otherCount: 3,
    otherType: UrlbarUtils.RESULT_TYPE.SEARCH,
    suggestedIndex: -1,
    viewCount: 5,
  },
  duringUpdate: [
    { count: 1 },
    {
      count: 1,
      type: UrlbarUtils.RESULT_TYPE.URL,
      suggestedIndex: 1,
      stale: true,
    },
    { count: 1, type: UrlbarUtils.RESULT_TYPE.SEARCH, stale: true },
    { count: 7, type: UrlbarUtils.RESULT_TYPE.URL, stale: true },
    { count: 3, type: UrlbarUtils.RESULT_TYPE.SEARCH, hidden: true },
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
//   5 results including suggestedIndex = -3
// Expected visible rows during update:
//   10 original rows with no changes
add_suggestedIndex_task({
  search1: {
    other: [
      { count: 1, type: UrlbarUtils.RESULT_TYPE.SEARCH },
      { count: 9, type: UrlbarUtils.RESULT_TYPE.URL },
    ],
    suggestedIndex: 1,
    viewCount: 10,
  },
  search2: {
    otherCount: 3,
    otherType: UrlbarUtils.RESULT_TYPE.SEARCH,
    suggestedIndex: -3,
    viewCount: 5,
  },
  duringUpdate: [
    { count: 1 },
    {
      count: 1,
      type: UrlbarUtils.RESULT_TYPE.URL,
      suggestedIndex: 1,
      stale: true,
    },
    { count: 1, type: UrlbarUtils.RESULT_TYPE.SEARCH, stale: true },
    { count: 7, type: UrlbarUtils.RESULT_TYPE.URL, stale: true },
    { count: 1, type: UrlbarUtils.RESULT_TYPE.SEARCH, hidden: true },
    {
      count: 1,
      type: UrlbarUtils.RESULT_TYPE.URL,
      suggestedIndex: -3,
      hidden: true,
    },
    { count: 2, type: UrlbarUtils.RESULT_TYPE.SEARCH, hidden: true },
  ],
});

// Search 1:
//   10 results including suggestedIndex = 9
// Search 2:
//   5 results including suggestedIndex = 1
// Expected visible rows during update:
//   10 original rows with no changes
add_suggestedIndex_task({
  search1: {
    other: [
      { count: 1, type: UrlbarUtils.RESULT_TYPE.SEARCH },
      { count: 9, type: UrlbarUtils.RESULT_TYPE.URL },
    ],
    suggestedIndex: 9,
    viewCount: 10,
  },
  search2: {
    otherCount: 3,
    otherType: UrlbarUtils.RESULT_TYPE.SEARCH,
    suggestedIndex: 1,
    viewCount: 5,
  },
  duringUpdate: [
    { count: 1 },
    { count: 1, type: UrlbarUtils.RESULT_TYPE.SEARCH, stale: true },
    { count: 7, type: UrlbarUtils.RESULT_TYPE.URL, stale: true },
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
    { count: 3, type: UrlbarUtils.RESULT_TYPE.SEARCH, hidden: true },
  ],
});

// Search 1:
//   10 results including suggestedIndex = 9
// Search 2:
//   5 results including suggestedIndex = 9
// Expected visible rows during update:
//   Indexes 2 and 3 replaced with search suggestions, no other changes
add_suggestedIndex_task({
  search1: {
    other: [
      { count: 1, type: UrlbarUtils.RESULT_TYPE.SEARCH },
      { count: 9, type: UrlbarUtils.RESULT_TYPE.URL },
    ],
    suggestedIndex: 9,
    viewCount: 10,
  },
  search2: {
    otherCount: 3,
    otherType: UrlbarUtils.RESULT_TYPE.SEARCH,
    suggestedIndex: 9,
    viewCount: 5,
  },
  duringUpdate: [
    { count: 1 },
    { count: 3, type: UrlbarUtils.RESULT_TYPE.SEARCH },
    { count: 5, type: UrlbarUtils.RESULT_TYPE.URL, stale: true },
    {
      count: 1,
      type: UrlbarUtils.RESULT_TYPE.URL,
      suggestedIndex: 9,
      stale: true,
    },
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
//   5 results including suggestedIndex = 1
// Expected visible rows during update:
//   10 original rows with no changes
add_suggestedIndex_task({
  search1: {
    other: [
      { count: 1, type: UrlbarUtils.RESULT_TYPE.SEARCH },
      { count: 9, type: UrlbarUtils.RESULT_TYPE.URL },
    ],
    suggestedIndex: -1,
    viewCount: 10,
  },
  search2: {
    otherCount: 3,
    otherType: UrlbarUtils.RESULT_TYPE.SEARCH,
    suggestedIndex: 1,
    viewCount: 5,
  },
  duringUpdate: [
    { count: 1 },
    { count: 1, type: UrlbarUtils.RESULT_TYPE.SEARCH, stale: true },
    { count: 7, type: UrlbarUtils.RESULT_TYPE.URL, stale: true },
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
    { count: 3, type: UrlbarUtils.RESULT_TYPE.SEARCH, hidden: true },
  ],
});

// Search 1:
//   10 results including suggestedIndex = -1
// Search 2:
//   5 results including suggestedIndex = 9
// Expected visible rows during update:
//   Indexes 2 and 3 replaced with search suggestions, no other changes
add_suggestedIndex_task({
  search1: {
    other: [
      { count: 1, type: UrlbarUtils.RESULT_TYPE.SEARCH },
      { count: 9, type: UrlbarUtils.RESULT_TYPE.URL },
    ],
    suggestedIndex: -1,
    viewCount: 10,
  },
  search2: {
    otherCount: 3,
    otherType: UrlbarUtils.RESULT_TYPE.SEARCH,
    suggestedIndex: 9,
    viewCount: 5,
  },
  duringUpdate: [
    { count: 1 },
    { count: 3, type: UrlbarUtils.RESULT_TYPE.SEARCH },
    { count: 5, type: UrlbarUtils.RESULT_TYPE.URL, stale: true },
    {
      count: 1,
      type: UrlbarUtils.RESULT_TYPE.URL,
      suggestedIndex: -1,
      stale: true,
    },
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
//   5 results including suggestedIndex = -1
// Expected visible rows during update:
//   Indexes 2 and 3 replaced with search suggestions, no other changes
add_suggestedIndex_task({
  search1: {
    other: [
      { count: 1, type: UrlbarUtils.RESULT_TYPE.SEARCH },
      { count: 9, type: UrlbarUtils.RESULT_TYPE.URL },
    ],
    suggestedIndex: -1,
    viewCount: 10,
  },
  search2: {
    otherCount: 3,
    otherType: UrlbarUtils.RESULT_TYPE.SEARCH,
    suggestedIndex: -1,
    viewCount: 5,
  },
  duringUpdate: [
    { count: 1 },
    { count: 3, type: UrlbarUtils.RESULT_TYPE.SEARCH },
    { count: 5, type: UrlbarUtils.RESULT_TYPE.URL, stale: true },
    {
      count: 1,
      type: UrlbarUtils.RESULT_TYPE.URL,
      suggestedIndex: -1,
      stale: true,
    },
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
//   5 results including suggestedIndex = 1 and suggestedIndex = -1
// Expected visible rows during update:
//   10 original rows with no changes
add_suggestedIndex_task({
  search1: {
    other: [
      { count: 1, type: UrlbarUtils.RESULT_TYPE.SEARCH },
      { count: 9, type: UrlbarUtils.RESULT_TYPE.URL },
    ],
    viewCount: 10,
  },
  search2: {
    otherCount: 2,
    otherType: UrlbarUtils.RESULT_TYPE.SEARCH,
    suggestedIndexes: [1, -1],
    viewCount: 5,
  },
  duringUpdate: [
    { count: 1 },
    { count: 1, type: UrlbarUtils.RESULT_TYPE.SEARCH, stale: true },
    { count: 8, type: UrlbarUtils.RESULT_TYPE.URL, stale: true },
    {
      count: 1,
      type: UrlbarUtils.RESULT_TYPE.URL,
      suggestedIndex: 1,
      hidden: true,
    },
    { count: 2, type: UrlbarUtils.RESULT_TYPE.SEARCH, hidden: true },
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
//   5 results including suggestedIndex = 1 and suggestedIndex = -1
// Expected visible rows during update:
//   Index 3 replaced with search suggestion, no other changes
add_suggestedIndex_task({
  search1: {
    other: [
      { count: 1, type: UrlbarUtils.RESULT_TYPE.SEARCH },
      { count: 9, type: UrlbarUtils.RESULT_TYPE.URL },
    ],
    suggestedIndex: 1,
    viewCount: 10,
  },
  search2: {
    otherCount: 2,
    otherType: UrlbarUtils.RESULT_TYPE.SEARCH,
    suggestedIndexes: [1, -1],
    viewCount: 5,
  },
  duringUpdate: [
    { count: 1 },
    {
      count: 1,
      type: UrlbarUtils.RESULT_TYPE.URL,
      suggestedIndex: 1,
    },
    { count: 2, type: UrlbarUtils.RESULT_TYPE.SEARCH },
    { count: 6, type: UrlbarUtils.RESULT_TYPE.URL, stale: true },
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
//   5 results including suggestedIndex = 1 and suggestedIndex = -1
// Expected visible rows during update:
//   10 original rows with no changes
add_suggestedIndex_task({
  search1: {
    other: [
      { count: 1, type: UrlbarUtils.RESULT_TYPE.SEARCH },
      { count: 9, type: UrlbarUtils.RESULT_TYPE.URL },
    ],
    suggestedIndex: -1,
    viewCount: 10,
  },
  search2: {
    otherCount: 2,
    otherType: UrlbarUtils.RESULT_TYPE.SEARCH,
    suggestedIndexes: [1, -1],
    viewCount: 5,
  },
  duringUpdate: [
    { count: 1 },
    { count: 1, type: UrlbarUtils.RESULT_TYPE.SEARCH, stale: true },
    { count: 7, type: UrlbarUtils.RESULT_TYPE.URL, stale: true },
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
    { count: 2, type: UrlbarUtils.RESULT_TYPE.SEARCH, hidden: true },
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
//   5 results including:
//     suggestedIndex = 1 with resultSpan = 2
//     suggestedIndex = -1
// Expected visible rows during update:
//   Index 3 replaced with search suggestion, no other changes
add_suggestedIndex_task({
  search1: {
    other: [
      { count: 1, type: UrlbarUtils.RESULT_TYPE.SEARCH },
      { count: 9, type: UrlbarUtils.RESULT_TYPE.URL },
    ],
    suggestedIndexes: [[1, 2]],
    viewCount: 9,
  },
  search2: {
    otherCount: 2,
    otherType: UrlbarUtils.RESULT_TYPE.SEARCH,
    suggestedIndexes: [[1, 2], -1],
    viewCount: 5,
  },
  duringUpdate: [
    { count: 1 },
    {
      count: 1,
      type: UrlbarUtils.RESULT_TYPE.URL,
      suggestedIndex: 1,
      resultSpan: 2,
    },
    { count: 2, type: UrlbarUtils.RESULT_TYPE.SEARCH },
    { count: 5, type: UrlbarUtils.RESULT_TYPE.URL, stale: true },
    {
      count: 1,
      type: UrlbarUtils.RESULT_TYPE.URL,
      suggestedIndex: -1,
      hidden: true,
    },
  ],
});
