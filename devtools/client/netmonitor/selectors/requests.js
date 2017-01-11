/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const { createSelector } = require("devtools/client/shared/vendor/reselect");
const { Filters, isFreetextMatch } = require("../filter-predicates");
const { Sorters } = require("../sort-predicates");

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
    const matchesType = filters.requestFilterTypes.some((enabled, filter) => {
      return enabled && Filters[filter] && Filters[filter](r);
    });
    return matchesType && isFreetextMatch(r, filters.requestFilterText);
  }
);

const getSortFn = createSelector(
  state => state.requests.requests,
  state => state.sort,
  (requests, sort) => {
    const sorter = Sorters[sort.type || "waterfall"];
    const ascending = sort.ascending ? +1 : -1;
    return (a, b) => ascending * sortWithClones(requests, sorter, a, b);
  }
);

const getSortedRequests = createSelector(
  state => state.requests.requests,
  getSortFn,
  (requests, sortFn) => requests.valueSeq().sort(sortFn).toList()
);

const getDisplayedRequests = createSelector(
  state => state.requests.requests,
  getFilterFn,
  getSortFn,
  (requests, filterFn, sortFn) => requests.valueSeq()
    .filter(filterFn).sort(sortFn).toList()
);

const getDisplayedRequestsSummary = createSelector(
  getDisplayedRequests,
  state => state.requests.lastEndedMillis - state.requests.firstStartedMillis,
  (requests, totalMillis) => {
    if (requests.size == 0) {
      return { count: 0, bytes: 0, millis: 0 };
    }

    const totalBytes = requests.reduce((total, item) => {
      if (typeof item.contentSize == "number") {
        total += item.contentSize;
      }
      return total;
    }, 0);

    return {
      count: requests.size,
      bytes: totalBytes,
      millis: totalMillis,
    };
  }
);

const getSelectedRequest = createSelector(
  state => state.requests,
  ({ selectedId, requests }) => selectedId ? requests.get(selectedId) : null
);

function getRequestById(state, id) {
  return state.requests.requests.get(id);
}

function getDisplayedRequestById(state, id) {
  return getDisplayedRequests(state).find(r => r.id === id);
}

module.exports = {
  getDisplayedRequestById,
  getDisplayedRequests,
  getDisplayedRequestsSummary,
  getRequestById,
  getSelectedRequest,
  getSortedRequests,
};
