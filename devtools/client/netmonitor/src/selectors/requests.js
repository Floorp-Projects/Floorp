/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const { createSelector } = require("devtools/client/shared/vendor/reselect");
const { Filters, isFreetextMatch } = require("../utils/filter-predicates");
const { Sorters } = require("../utils/sort-predicates");

/**
 * Take clones into account when sorting.
 * If a request is a clone, use the original request for comparison.
 * If one of the compared request is a clone of the other, sort them next to each other.
 */
function sortWithClones(requests, sorter, a, b) {
  const aId = a.id, bId = b.id;

  if (aId.endsWith("-clone")) {
    const aOrigId = aId.replace(/-clone$/, "");
    if (aOrigId === bId) {
      return +1;
    }
    a = requests.get(aOrigId);
  }

  if (bId.endsWith("-clone")) {
    const bOrigId = bId.replace(/-clone$/, "");
    if (bOrigId === aId) {
      return -1;
    }
    b = requests.get(bOrigId);
  }

  return sorter(a, b);
}

const getFilterFn = createSelector(
  state => state.filters,
  filters => r => {
    const matchesType = Object.keys(filters.requestFilterTypes).some(filter => {
      return filters.requestFilterTypes[filter] && Filters[filter] && Filters[filter](r);
    });
    return matchesType && isFreetextMatch(r, filters.requestFilterText);
  }
);

const getTypeFilterFn = createSelector(
  state => state.filters,
  filters => r => {
    return Object.keys(filters.requestFilterTypes).some(filter => {
      return filters.requestFilterTypes[filter] && Filters[filter] && Filters[filter](r);
    });
  }
);

const getSortFn = createSelector(
  state => state.requests,
  state => state.sort,
  ({ requests }, sort) => {
    const sorter = Sorters[sort.type || "waterfall"];
    const ascending = sort.ascending ? +1 : -1;
    return (a, b) => ascending * sortWithClones(requests, sorter, a, b);
  }
);

const getSortedRequests = createSelector(
  state => state.requests,
  getSortFn,
  ({ requests }, sortFn) => {
    let arr = requests.valueSeq().sort(sortFn);
    arr.get = index => arr[index];
    arr.isEmpty = () => this.length == 0;
    arr.size = arr.length;
    return arr;
  }
);

const getDisplayedRequests = createSelector(
  state => state.requests,
  getFilterFn,
  getSortFn,
  ({ requests }, filterFn, sortFn) => {
    let arr = requests.valueSeq().filter(filterFn).sort(sortFn);
    arr.get = index => arr[index];
    arr.isEmpty = () => this.length == 0;
    arr.size = arr.length;
    return arr;
  }
);

const getTypeFilteredRequests = createSelector(
  state => state.requests,
  getTypeFilterFn,
  ({ requests }, filterFn) => requests.valueSeq().filter(filterFn)
);

const getDisplayedRequestsSummary = createSelector(
  getDisplayedRequests,
  state => state.requests.lastEndedMillis - state.requests.firstStartedMillis,
  (requests, totalMillis) => {
    if (requests.size == 0) {
      return { count: 0, bytes: 0, millis: 0 };
    }

    const totalBytes = requests.reduce((totals, item) => {
      if (typeof item.contentSize == "number") {
        totals.contentSize += item.contentSize;
      }

      if (typeof item.transferredSize == "number" && !item.fromCache) {
        totals.transferredSize += item.transferredSize;
      }

      return totals;
    }, { contentSize: 0, transferredSize: 0 });

    return {
      count: requests.size,
      contentSize: totalBytes.contentSize,
      millis: totalMillis,
      transferredSize: totalBytes.transferredSize,
    };
  }
);

const getSelectedRequest = createSelector(
  state => state.requests,
  ({ selectedId, requests }) => selectedId ? requests.get(selectedId) : undefined
);

const isSelectedRequestVisible = createSelector(
  state => state.requests,
  getDisplayedRequests,
  ({ selectedId }, displayedRequests) =>
    displayedRequests.some(r => r.id === selectedId)
);

function getRequestById(state, id) {
  return state.requests.requests.get(id);
}

function getDisplayedRequestById(state, id) {
  return getDisplayedRequests(state).find(r => r.id === id);
}

/**
 * Returns the current recording boolean state (HTTP traffic is
 * monitored or not monitored)
 */
function getRecordingState(state) {
  return state.requests.recording;
}

module.exports = {
  getDisplayedRequestById,
  getDisplayedRequests,
  getDisplayedRequestsSummary,
  getRecordingState,
  getRequestById,
  getSelectedRequest,
  getSortedRequests,
  getTypeFilteredRequests,
  isSelectedRequestVisible,
};
