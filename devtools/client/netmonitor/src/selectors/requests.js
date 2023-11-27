/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const {
  createSelector,
} = require("resource://devtools/client/shared/vendor/reselect.js");
const {
  Filters,
  isFreetextMatch,
} = require("resource://devtools/client/netmonitor/src/utils/filter-predicates.js");
const {
  Sorters,
} = require("resource://devtools/client/netmonitor/src/utils/sort-predicates.js");

/**
 * Take clones into account when sorting.
 * If a request is a clone, use the original request for comparison.
 * If one of the compared request is a clone of the other, sort them next to each other.
 */
function sortWithClones(requests, sorter, a, b) {
  const aId = a.id,
    bId = b.id;

  if (aId.endsWith("-clone")) {
    const aOrigId = aId.replace(/-clone$/, "");
    if (aOrigId === bId) {
      return +1;
    }
    a = requests.find(item => item.id === aOrigId);
  }

  if (bId.endsWith("-clone")) {
    const bOrigId = bId.replace(/-clone$/, "");
    if (bOrigId === aId) {
      return -1;
    }
    b = requests.find(item => item.id === bOrigId);
  }

  const defaultSorter = () => false;
  return sorter ? sorter(a, b) : defaultSorter;
}

/**
 * Take clones into account when filtering. If a request is
 * a clone, it's not filtered out.
 */
const getFilterWithCloneFn = createSelector(
  state => state.filters,
  filters => r => {
    const matchesType = Object.keys(filters.requestFilterTypes).some(filter => {
      if (r.id.endsWith("-clone")) {
        return true;
      }
      return (
        filters.requestFilterTypes[filter] &&
        Filters[filter] &&
        Filters[filter](r)
      );
    });
    return matchesType && isFreetextMatch(r, filters.requestFilterText);
  }
);

const getTypeFilterFn = createSelector(
  state => state.filters,
  filters => r => {
    return Object.keys(filters.requestFilterTypes).some(filter => {
      return (
        filters.requestFilterTypes[filter] &&
        Filters[filter] &&
        Filters[filter](r)
      );
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
  ({ requests }, sortFn) => [...requests].sort(sortFn)
);

const getDisplayedRequests = createSelector(
  state => state.requests,
  getFilterWithCloneFn,
  getSortFn,
  ({ requests }, filterFn, sortFn) => requests.filter(filterFn).sort(sortFn)
);

const getTypeFilteredRequests = createSelector(
  state => state.requests,
  getTypeFilterFn,
  ({ requests }, filterFn) => requests.filter(filterFn)
);

const getDisplayedRequestsSummary = createSelector(
  getDisplayedRequests,
  state => state.requests.lastEndedMs - state.requests.firstStartedMs,
  (requests, totalMs) => {
    if (requests.length === 0) {
      return { count: 0, bytes: 0, ms: 0 };
    }

    const totalBytes = requests.reduce(
      (totals, item) => {
        if (typeof item.contentSize == "number") {
          totals.contentSize += item.contentSize;
        }

        if (
          typeof item.transferredSize == "number" &&
          !(item.fromCache || item.fromServiceWorker || item.status === "304")
        ) {
          totals.transferredSize += item.transferredSize;
        }

        return totals;
      },
      { contentSize: 0, transferredSize: 0 }
    );

    return {
      count: requests.length,
      contentSize: totalBytes.contentSize,
      ms: totalMs,
      transferredSize: totalBytes.transferredSize,
    };
  }
);

const getSelectedRequest = createSelector(
  state => state.requests,
  ({ selectedId, requests }) =>
    selectedId ? requests.find(item => item.id === selectedId) : undefined
);

const isSelectedRequestVisible = createSelector(
  state => state.requests,
  getDisplayedRequests,
  ({ selectedId }, displayedRequests) =>
    displayedRequests.some(r => r.id === selectedId)
);

function getRequestById(state, id) {
  return state.requests.requests.find(item => item.id === id);
}

function getRequestByChannelId(state, channelId) {
  return [...state.requests.requests.values()].find(
    r => r.resourceId == channelId
  );
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

const getClickedRequest = createSelector(
  state => state.requests,
  ({ requests, clickedRequestId }) =>
    requests.find(request => request.id == clickedRequestId)
);

module.exports = {
  getClickedRequest,
  getDisplayedRequestById,
  getDisplayedRequests,
  getDisplayedRequestsSummary,
  getRecordingState,
  getRequestById,
  getRequestByChannelId,
  getSelectedRequest,
  getSortedRequests,
  getTypeFilteredRequests,
  isSelectedRequestVisible,
};
