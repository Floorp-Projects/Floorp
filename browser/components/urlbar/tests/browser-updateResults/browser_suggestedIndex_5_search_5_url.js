/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

// This test checks row visibility during view updates when rows with suggested
// indexes are added and removed. Each task performs two searches: Search 1
// returns 5 results with search suggestions, and search 2 returns 5 results
// with URL results.

"use strict";

// Search 1:
//   5 results, no suggestedIndex
// Search 2:
//   5 results including suggestedIndex = 1
// Expected visible rows during update:
//   5 original rows with no changes
add_suggestedIndex_task({
  search1: {
    otherCount: 4,
    otherType: UrlbarUtils.RESULT_TYPE.SEARCH,
    viewCount: 5,
  },
  search2: {
    otherCount: 3,
    otherType: UrlbarUtils.RESULT_TYPE.URL,
    suggestedIndex: 1,
    viewCount: 5,
  },
  duringUpdate: [
    { count: 1 },
    { count: 4, type: UrlbarUtils.RESULT_TYPE.SEARCH, stale: true },
    {
      count: 1,
      type: UrlbarUtils.RESULT_TYPE.URL,
      suggestedIndex: 1,
      hidden: true,
    },
    { count: 3, type: UrlbarUtils.RESULT_TYPE.URL, hidden: true },
  ],
});

// Search 1:
//   5 results, no suggestedIndex
// Search 2:
//   5 results including suggestedIndex = 2
// Expected visible rows during update:
//   5 search-1 rows + 1 search-2 row (the one before the suggestedIndex row)
add_suggestedIndex_task({
  search1: {
    otherCount: 4,
    otherType: UrlbarUtils.RESULT_TYPE.SEARCH,
    viewCount: 5,
  },
  search2: {
    otherCount: 3,
    otherType: UrlbarUtils.RESULT_TYPE.URL,
    suggestedIndex: 2,
    viewCount: 5,
  },
  duringUpdate: [
    { count: 1 },
    { count: 4, type: UrlbarUtils.RESULT_TYPE.SEARCH, stale: true },
    { count: 1, type: UrlbarUtils.RESULT_TYPE.URL },
    {
      count: 1,
      type: UrlbarUtils.RESULT_TYPE.URL,
      suggestedIndex: 2,
      hidden: true,
    },
    { count: 2, type: UrlbarUtils.RESULT_TYPE.URL, hidden: true },
  ],
});

// Search 1:
//   5 results, no suggestedIndex
// Search 2:
//   5 results including suggestedIndex = 9
// Expected visible rows during update:
//   5 search-1 rows + 3 search-2 rows (the ones before the suggestedIndex)
add_suggestedIndex_task({
  search1: {
    otherCount: 4,
    otherType: UrlbarUtils.RESULT_TYPE.SEARCH,
    viewCount: 5,
  },
  search2: {
    otherCount: 3,
    otherType: UrlbarUtils.RESULT_TYPE.URL,
    suggestedIndex: 9,
    viewCount: 5,
  },
  duringUpdate: [
    { count: 1 },
    { count: 4, type: UrlbarUtils.RESULT_TYPE.SEARCH, stale: true },
    { count: 3, type: UrlbarUtils.RESULT_TYPE.URL },
    {
      count: 1,
      type: UrlbarUtils.RESULT_TYPE.URL,
      suggestedIndex: 9,
      hidden: true,
    },
  ],
});

// Search 1:
//   5 results, no suggestedIndex
// Search 2:
//   5 results including suggestedIndex = -1
// Expected visible rows during update:
//   5 search-1 rows + 3 search-2 rows (the ones before the suggestedIndex row)
add_suggestedIndex_task({
  search1: {
    otherCount: 4,
    otherType: UrlbarUtils.RESULT_TYPE.SEARCH,
    viewCount: 5,
  },
  search2: {
    otherCount: 3,
    otherType: UrlbarUtils.RESULT_TYPE.URL,
    suggestedIndex: -1,
    viewCount: 5,
  },
  duringUpdate: [
    { count: 1 },
    { count: 4, type: UrlbarUtils.RESULT_TYPE.SEARCH, stale: true },
    { count: 3, type: UrlbarUtils.RESULT_TYPE.URL },
    {
      count: 1,
      type: UrlbarUtils.RESULT_TYPE.URL,
      suggestedIndex: -1,
      hidden: true,
    },
  ],
});

// Search 1:
//   5 results, no suggestedIndex
// Search 2:
//   5 results including suggestedIndex = -2
// Expected visible rows during update:
//   5 search-1 rows + 2 search-2 rows (the one before the suggestedIndex row)
add_suggestedIndex_task({
  search1: {
    otherCount: 4,
    otherType: UrlbarUtils.RESULT_TYPE.SEARCH,
    viewCount: 5,
  },
  search2: {
    otherCount: 3,
    otherType: UrlbarUtils.RESULT_TYPE.URL,
    suggestedIndex: -2,
    viewCount: 5,
  },
  duringUpdate: [
    { count: 1 },
    { count: 4, type: UrlbarUtils.RESULT_TYPE.SEARCH, stale: true },
    { count: 2, type: UrlbarUtils.RESULT_TYPE.URL },
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
//   5 results including suggestedIndex = 1
// Search 2:
//   5 results including suggestedIndex = 1
// Expected visible rows during update:
//   5 search-1 rows + 3 search-2 rows (i.e., all rows from both searches)
add_suggestedIndex_task({
  search1: {
    otherCount: 3,
    otherType: UrlbarUtils.RESULT_TYPE.SEARCH,
    suggestedIndex: 1,
    viewCount: 5,
  },
  search2: {
    otherCount: 3,
    otherType: UrlbarUtils.RESULT_TYPE.URL,
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
    { count: 3, type: UrlbarUtils.RESULT_TYPE.SEARCH, stale: true },
    { count: 3, type: UrlbarUtils.RESULT_TYPE.URL },
  ],
});

// Search 1:
//   5 results including suggestedIndex = 1
// Search 2:
//   5 results including suggestedIndex = 2
// Expected visible rows during update:
//   5 original rows with no changes
add_suggestedIndex_task({
  search1: {
    otherCount: 3,
    otherType: UrlbarUtils.RESULT_TYPE.SEARCH,
    suggestedIndex: 1,
    viewCount: 5,
  },
  search2: {
    otherCount: 3,
    otherType: UrlbarUtils.RESULT_TYPE.URL,
    suggestedIndex: 2,
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
    { count: 3, type: UrlbarUtils.RESULT_TYPE.SEARCH, stale: true },
    { count: 1, type: UrlbarUtils.RESULT_TYPE.URL, hidden: true },
    {
      count: 1,
      type: UrlbarUtils.RESULT_TYPE.URL,
      suggestedIndex: 2,
      hidden: true,
    },
    { count: 2, type: UrlbarUtils.RESULT_TYPE.URL, hidden: true },
  ],
});

// Search 1:
//   5 results including suggestedIndex = 1
// Search 2:
//   5 results including suggestedIndex = 9
// Expected visible rows during update:
//   5 original rows with no changes
add_suggestedIndex_task({
  search1: {
    otherCount: 3,
    otherType: UrlbarUtils.RESULT_TYPE.SEARCH,
    suggestedIndex: 1,
    viewCount: 5,
  },
  search2: {
    otherCount: 3,
    otherType: UrlbarUtils.RESULT_TYPE.URL,
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
    { count: 3, type: UrlbarUtils.RESULT_TYPE.SEARCH, stale: true },
    { count: 3, type: UrlbarUtils.RESULT_TYPE.URL, hidden: true },
    {
      count: 1,
      type: UrlbarUtils.RESULT_TYPE.URL,
      suggestedIndex: 9,
      hidden: true,
    },
  ],
});

// Search 1:
//   5 results including suggestedIndex = 1
// Search 2:
//   5 results including suggestedIndex = -1
// Expected visible rows during update:
//   5 original rows with no changes
add_suggestedIndex_task({
  search1: {
    otherCount: 3,
    otherType: UrlbarUtils.RESULT_TYPE.SEARCH,
    suggestedIndex: 1,
    viewCount: 5,
  },
  search2: {
    otherCount: 3,
    otherType: UrlbarUtils.RESULT_TYPE.URL,
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
    { count: 3, type: UrlbarUtils.RESULT_TYPE.SEARCH, stale: true },
    { count: 3, type: UrlbarUtils.RESULT_TYPE.URL, hidden: true },
    {
      count: 1,
      type: UrlbarUtils.RESULT_TYPE.URL,
      suggestedIndex: -1,
      hidden: true,
    },
  ],
});

// Search 1:
//   5 results including suggestedIndex = 1
// Search 2:
//   5 results including suggestedIndex = -2
// Expected visible rows during update:
//   5 original rows with no changes
add_suggestedIndex_task({
  search1: {
    otherCount: 3,
    otherType: UrlbarUtils.RESULT_TYPE.SEARCH,
    suggestedIndex: 1,
    viewCount: 5,
  },
  search2: {
    otherCount: 3,
    otherType: UrlbarUtils.RESULT_TYPE.URL,
    suggestedIndex: -2,
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
    { count: 3, type: UrlbarUtils.RESULT_TYPE.SEARCH, stale: true },
    { count: 2, type: UrlbarUtils.RESULT_TYPE.URL, hidden: true },
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
//   5 results including suggestedIndex = 1
// Search 2:
//   5 results including suggestedIndex = -3
// Expected visible rows during update:
//   5 original rows with no changes
add_suggestedIndex_task({
  search1: {
    otherCount: 3,
    otherType: UrlbarUtils.RESULT_TYPE.SEARCH,
    suggestedIndex: 1,
    viewCount: 5,
  },
  search2: {
    otherCount: 3,
    otherType: UrlbarUtils.RESULT_TYPE.URL,
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
    { count: 3, type: UrlbarUtils.RESULT_TYPE.SEARCH, stale: true },
    { count: 1, type: UrlbarUtils.RESULT_TYPE.URL, hidden: true },
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
//   5 results including suggestedIndex = 9
// Search 2:
//   5 results including suggestedIndex = 1
// Expected visible rows during update:
//   5 original rows with no changes
add_suggestedIndex_task({
  search1: {
    otherCount: 3,
    otherType: UrlbarUtils.RESULT_TYPE.SEARCH,
    suggestedIndex: 9,
    viewCount: 5,
  },
  search2: {
    otherCount: 3,
    otherType: UrlbarUtils.RESULT_TYPE.URL,
    suggestedIndex: 1,
    viewCount: 5,
  },
  duringUpdate: [
    { count: 1 },
    { count: 3, type: UrlbarUtils.RESULT_TYPE.SEARCH, stale: true },
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
    { count: 3, type: UrlbarUtils.RESULT_TYPE.URL, hidden: true },
  ],
});

// Search 1:
//   5 results including suggestedIndex = 9
// Search 2:
//   5 results including suggestedIndex = 2
// Expected visible rows during update:
//   5 original rows with no changes
add_suggestedIndex_task({
  search1: {
    otherCount: 3,
    otherType: UrlbarUtils.RESULT_TYPE.SEARCH,
    suggestedIndex: 9,
    viewCount: 5,
  },
  search2: {
    otherCount: 3,
    otherType: UrlbarUtils.RESULT_TYPE.URL,
    suggestedIndex: 2,
    viewCount: 5,
  },
  duringUpdate: [
    { count: 1 },
    { count: 3, type: UrlbarUtils.RESULT_TYPE.SEARCH, stale: true },
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
    { count: 2, type: UrlbarUtils.RESULT_TYPE.URL, hidden: true },
  ],
});

// Search 1:
//   5 results including suggestedIndex = 9
// Search 2:
//   5 results including suggestedIndex = 9
// Expected visible rows during update:
//   5 original rows with no changes (because the original search results can't
//   be replaced with URL results)
add_suggestedIndex_task({
  search1: {
    otherCount: 3,
    otherType: UrlbarUtils.RESULT_TYPE.SEARCH,
    suggestedIndex: 9,
    viewCount: 5,
  },
  search2: {
    otherCount: 3,
    otherType: UrlbarUtils.RESULT_TYPE.URL,
    suggestedIndex: 9,
    viewCount: 5,
  },
  duringUpdate: [
    { count: 1 },
    { count: 3, type: UrlbarUtils.RESULT_TYPE.SEARCH, stale: true },
    {
      count: 1,
      type: UrlbarUtils.RESULT_TYPE.URL,
      suggestedIndex: 9,
      stale: true,
    },
    { count: 3, type: UrlbarUtils.RESULT_TYPE.URL, hidden: true },
    {
      count: 1,
      type: UrlbarUtils.RESULT_TYPE.URL,
      suggestedIndex: 9,
      hidden: true,
    },
  ],
});

// Search 1:
//   5 results including suggestedIndex = 9
// Search 2:
//   5 results including suggestedIndex = -1
// Expected visible rows during update:
//   5 original rows with no changes
add_suggestedIndex_task({
  search1: {
    otherCount: 3,
    otherType: UrlbarUtils.RESULT_TYPE.SEARCH,
    suggestedIndex: 9,
    viewCount: 5,
  },
  search2: {
    otherCount: 3,
    otherType: UrlbarUtils.RESULT_TYPE.URL,
    suggestedIndex: -1,
    viewCount: 5,
  },
  duringUpdate: [
    { count: 1 },
    { count: 3, type: UrlbarUtils.RESULT_TYPE.SEARCH, stale: true },
    {
      count: 1,
      type: UrlbarUtils.RESULT_TYPE.URL,
      suggestedIndex: 9,
      stale: true,
    },
    { count: 3, type: UrlbarUtils.RESULT_TYPE.URL, hidden: true },
    {
      count: 1,
      type: UrlbarUtils.RESULT_TYPE.URL,
      suggestedIndex: -1,
      hidden: true,
    },
  ],
});

// Search 1:
//   5 results including suggestedIndex = 9
// Search 2:
//   5 results including suggestedIndex = -2
// Expected visible rows during update:
//   5 original rows with no changes
add_suggestedIndex_task({
  search1: {
    otherCount: 3,
    otherType: UrlbarUtils.RESULT_TYPE.SEARCH,
    suggestedIndex: 9,
    viewCount: 5,
  },
  search2: {
    otherCount: 3,
    otherType: UrlbarUtils.RESULT_TYPE.URL,
    suggestedIndex: -2,
    viewCount: 5,
  },
  duringUpdate: [
    { count: 1 },
    { count: 3, type: UrlbarUtils.RESULT_TYPE.SEARCH, stale: true },
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
      suggestedIndex: -2,
      hidden: true,
    },
    { count: 1, type: UrlbarUtils.RESULT_TYPE.URL, hidden: true },
  ],
});

// Search 1:
//   5 results including suggestedIndex = 9
// Search 2:
//   5 results including suggestedIndex = -3
// Expected visible rows during update:
//   5 original rows with no changes
add_suggestedIndex_task({
  search1: {
    otherCount: 3,
    otherType: UrlbarUtils.RESULT_TYPE.SEARCH,
    suggestedIndex: 9,
    viewCount: 5,
  },
  search2: {
    otherCount: 3,
    otherType: UrlbarUtils.RESULT_TYPE.URL,
    suggestedIndex: -3,
    viewCount: 5,
  },
  duringUpdate: [
    { count: 1 },
    { count: 3, type: UrlbarUtils.RESULT_TYPE.SEARCH, stale: true },
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
      suggestedIndex: -3,
      hidden: true,
    },
    { count: 2, type: UrlbarUtils.RESULT_TYPE.URL, hidden: true },
  ],
});

// Search 1:
//   5 results including suggestedIndex = -1
// Search 2:
//   5 results including suggestedIndex = 1
// Expected visible rows during update:
//   5 original rows with no changes
add_suggestedIndex_task({
  search1: {
    otherCount: 3,
    otherType: UrlbarUtils.RESULT_TYPE.SEARCH,
    suggestedIndex: -1,
    viewCount: 5,
  },
  search2: {
    otherCount: 3,
    otherType: UrlbarUtils.RESULT_TYPE.URL,
    suggestedIndex: 1,
    viewCount: 5,
  },
  duringUpdate: [
    { count: 1 },
    { count: 3, type: UrlbarUtils.RESULT_TYPE.SEARCH, stale: true },
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
    { count: 3, type: UrlbarUtils.RESULT_TYPE.URL, hidden: true },
  ],
});

// Search 1:
//   5 results including suggestedIndex = -1
// Search 2:
//   5 results including suggestedIndex = 2
// Expected visible rows during update:
//   5 original rows with no changes
add_suggestedIndex_task({
  search1: {
    otherCount: 3,
    otherType: UrlbarUtils.RESULT_TYPE.SEARCH,
    suggestedIndex: -1,
    viewCount: 5,
  },
  search2: {
    otherCount: 3,
    otherType: UrlbarUtils.RESULT_TYPE.URL,
    suggestedIndex: 2,
    viewCount: 5,
  },
  duringUpdate: [
    { count: 1 },
    { count: 3, type: UrlbarUtils.RESULT_TYPE.SEARCH, stale: true },
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
    { count: 2, type: UrlbarUtils.RESULT_TYPE.URL, hidden: true },
  ],
});

// Search 1:
//   5 results including suggestedIndex = -1
// Search 2:
//   5 results including suggestedIndex = 9
// Expected visible rows during update:
//   5 original rows with no changes
add_suggestedIndex_task({
  search1: {
    otherCount: 3,
    otherType: UrlbarUtils.RESULT_TYPE.SEARCH,
    suggestedIndex: -1,
    viewCount: 5,
  },
  search2: {
    otherCount: 3,
    otherType: UrlbarUtils.RESULT_TYPE.URL,
    suggestedIndex: 9,
    viewCount: 5,
  },
  duringUpdate: [
    { count: 1 },
    { count: 3, type: UrlbarUtils.RESULT_TYPE.SEARCH, stale: true },
    {
      count: 1,
      type: UrlbarUtils.RESULT_TYPE.URL,
      suggestedIndex: -1,
      stale: true,
    },
    { count: 3, type: UrlbarUtils.RESULT_TYPE.URL, hidden: true },
    {
      count: 1,
      type: UrlbarUtils.RESULT_TYPE.URL,
      suggestedIndex: 9,
      hidden: true,
    },
  ],
});

// Search 1:
//   5 results including suggestedIndex = -1
// Search 2:
//   5 results including suggestedIndex = -1
// Expected visible rows during update:
//   5 original rows with no changes
add_suggestedIndex_task({
  search1: {
    otherCount: 3,
    otherType: UrlbarUtils.RESULT_TYPE.SEARCH,
    suggestedIndex: -1,
    viewCount: 5,
  },
  search2: {
    otherCount: 3,
    otherType: UrlbarUtils.RESULT_TYPE.URL,
    suggestedIndex: -1,
    viewCount: 5,
  },
  duringUpdate: [
    { count: 1 },
    { count: 3, type: UrlbarUtils.RESULT_TYPE.SEARCH, stale: true },
    {
      count: 1,
      type: UrlbarUtils.RESULT_TYPE.URL,
      suggestedIndex: -1,
      stale: true,
    },
    { count: 3, type: UrlbarUtils.RESULT_TYPE.URL, hidden: true },
    {
      count: 1,
      type: UrlbarUtils.RESULT_TYPE.URL,
      suggestedIndex: -1,
      hidden: true,
    },
  ],
});

// Search 1:
//   5 results including suggestedIndex = -1
// Search 2:
//   5 results including suggestedIndex = -2
// Expected visible rows during update:
//   5 original rows with no changes
add_suggestedIndex_task({
  search1: {
    otherCount: 3,
    otherType: UrlbarUtils.RESULT_TYPE.SEARCH,
    suggestedIndex: -1,
    viewCount: 5,
  },
  search2: {
    otherCount: 3,
    otherType: UrlbarUtils.RESULT_TYPE.URL,
    suggestedIndex: -2,
    viewCount: 5,
  },
  duringUpdate: [
    { count: 1 },
    { count: 3, type: UrlbarUtils.RESULT_TYPE.SEARCH, stale: true },
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
      suggestedIndex: -2,
      hidden: true,
    },
    { count: 1, type: UrlbarUtils.RESULT_TYPE.URL, hidden: true },
  ],
});

// Search 1:
//   5 results including suggestedIndex = -2
// Search 2:
//   5 results including suggestedIndex = 1
// Expected visible rows during update:
//   5 original rows with no changes
add_suggestedIndex_task({
  search1: {
    otherCount: 3,
    otherType: UrlbarUtils.RESULT_TYPE.SEARCH,
    suggestedIndex: -2,
    viewCount: 5,
  },
  search2: {
    otherCount: 3,
    otherType: UrlbarUtils.RESULT_TYPE.URL,
    suggestedIndex: 1,
    viewCount: 5,
  },
  duringUpdate: [
    { count: 1 },
    { count: 2, type: UrlbarUtils.RESULT_TYPE.SEARCH, stale: true },
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
    { count: 3, type: UrlbarUtils.RESULT_TYPE.URL, hidden: true },
  ],
});

// Search 1:
//   5 results including suggestedIndex = -2
// Search 2:
//   5 results including suggestedIndex = 2
// Expected visible rows during update:
//   5 original rows with no changes
add_suggestedIndex_task({
  search1: {
    otherCount: 3,
    otherType: UrlbarUtils.RESULT_TYPE.SEARCH,
    suggestedIndex: -2,
    viewCount: 5,
  },
  search2: {
    otherCount: 3,
    otherType: UrlbarUtils.RESULT_TYPE.URL,
    suggestedIndex: 2,
    viewCount: 5,
  },
  duringUpdate: [
    { count: 1 },
    { count: 2, type: UrlbarUtils.RESULT_TYPE.SEARCH, stale: true },
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
    { count: 2, type: UrlbarUtils.RESULT_TYPE.URL, hidden: true },
  ],
});

// Search 1:
//   5 results including suggestedIndex = -2
// Search 2:
//   5 results including suggestedIndex = 9
// Expected visible rows during update:
//   5 original rows with no changes
add_suggestedIndex_task({
  search1: {
    otherCount: 3,
    otherType: UrlbarUtils.RESULT_TYPE.SEARCH,
    suggestedIndex: -2,
    viewCount: 5,
  },
  search2: {
    otherCount: 3,
    otherType: UrlbarUtils.RESULT_TYPE.URL,
    suggestedIndex: 9,
    viewCount: 5,
  },
  duringUpdate: [
    { count: 1 },
    { count: 2, type: UrlbarUtils.RESULT_TYPE.SEARCH, stale: true },
    {
      count: 1,
      type: UrlbarUtils.RESULT_TYPE.URL,
      suggestedIndex: -2,
      stale: true,
    },
    { count: 1, type: UrlbarUtils.RESULT_TYPE.SEARCH, stale: true },
    { count: 3, type: UrlbarUtils.RESULT_TYPE.URL, hidden: true },
    {
      count: 1,
      type: UrlbarUtils.RESULT_TYPE.URL,
      suggestedIndex: 9,
      hidden: true,
    },
  ],
});

// Search 1:
//   5 results including suggestedIndex = -2
// Search 2:
//   5 results including suggestedIndex = -1
// Expected visible rows during update:
//   5 original rows with no changes
add_suggestedIndex_task({
  search1: {
    otherCount: 3,
    otherType: UrlbarUtils.RESULT_TYPE.SEARCH,
    suggestedIndex: -2,
    viewCount: 5,
  },
  search2: {
    otherCount: 3,
    otherType: UrlbarUtils.RESULT_TYPE.URL,
    suggestedIndex: -1,
    viewCount: 5,
  },
  duringUpdate: [
    { count: 1 },
    { count: 2, type: UrlbarUtils.RESULT_TYPE.SEARCH, stale: true },
    {
      count: 1,
      type: UrlbarUtils.RESULT_TYPE.URL,
      suggestedIndex: -2,
      stale: true,
    },
    { count: 1, type: UrlbarUtils.RESULT_TYPE.SEARCH, stale: true },
    { count: 3, type: UrlbarUtils.RESULT_TYPE.URL, hidden: true },
    {
      count: 1,
      type: UrlbarUtils.RESULT_TYPE.URL,
      suggestedIndex: -1,
      hidden: true,
    },
  ],
});

// Search 1:
//   5 results including suggestedIndex = -2
// Search 2:
//   5 results including suggestedIndex = -2
// Expected visible rows during update:
//   5 original rows with no changes (because the original search results can't
//   be replaced with URL results)
add_suggestedIndex_task({
  search1: {
    otherCount: 3,
    otherType: UrlbarUtils.RESULT_TYPE.SEARCH,
    suggestedIndex: -2,
    viewCount: 5,
  },
  search2: {
    otherCount: 3,
    otherType: UrlbarUtils.RESULT_TYPE.URL,
    suggestedIndex: -2,
    viewCount: 5,
  },
  duringUpdate: [
    { count: 1 },
    { count: 2, type: UrlbarUtils.RESULT_TYPE.SEARCH, stale: true },
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
      suggestedIndex: -2,
      hidden: true,
    },
    { count: 1, type: UrlbarUtils.RESULT_TYPE.URL, hidden: true },
  ],
});

// Search 1:
//   5 results, no suggestedIndex
// Search 2:
//   5 results including suggestedIndex = 1 and suggestedIndex = -1
// Expected visible rows during update:
//   5 original rows with no changes
add_suggestedIndex_task({
  search1: {
    otherCount: 4,
    otherType: UrlbarUtils.RESULT_TYPE.SEARCH,
    viewCount: 5,
  },
  search2: {
    otherCount: 2,
    otherType: UrlbarUtils.RESULT_TYPE.URL,
    suggestedIndexes: [1, -1],
    viewCount: 5,
  },
  duringUpdate: [
    { count: 1 },
    { count: 4, type: UrlbarUtils.RESULT_TYPE.SEARCH, stale: true },
    {
      count: 1,
      type: UrlbarUtils.RESULT_TYPE.URL,
      suggestedIndex: 1,
      hidden: true,
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
//   5 results including suggestedIndex = 1
// Search 2:
//   5 results including suggestedIndex = 1 and suggestedIndex = -1
// Expected visible rows during update:
//   5 search-1 rows + 2 search-2 rows (the ones before the suggestedIndex row)
add_suggestedIndex_task({
  search1: {
    otherCount: 3,
    otherType: UrlbarUtils.RESULT_TYPE.SEARCH,
    suggestedIndex: 1,
    viewCount: 5,
  },
  search2: {
    otherCount: 2,
    otherType: UrlbarUtils.RESULT_TYPE.URL,
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
//   5 results including suggestedIndex = -1
// Search 2:
//   5 results including suggestedIndex = 1 and suggestedIndex = -1
// Expected visible rows during update:
//   5 original rows with no changes
add_suggestedIndex_task({
  search1: {
    otherCount: 3,
    otherType: UrlbarUtils.RESULT_TYPE.SEARCH,
    suggestedIndex: -1,
    viewCount: 5,
  },
  search2: {
    otherCount: 2,
    otherType: UrlbarUtils.RESULT_TYPE.URL,
    suggestedIndexes: [1, -1],
    viewCount: 5,
  },
  duringUpdate: [
    { count: 1 },
    { count: 3, type: UrlbarUtils.RESULT_TYPE.SEARCH, stale: true },
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
    {
      count: 1,
      type: UrlbarUtils.RESULT_TYPE.URL,
      suggestedIndex: -1,
      hidden: true,
    },
  ],
});
