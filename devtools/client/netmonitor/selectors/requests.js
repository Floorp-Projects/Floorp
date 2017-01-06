/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const { createSelector } = require("devtools/client/shared/vendor/reselect");
const { Filters, isFreetextMatch } = require("../filter-predicates");
const { Sorters } = require("../sort-predicates");

/**
 * Check if the given requests is a clone, find and return the original request if it is.
 * Cloned requests are sorted by comparing the original ones.
 */
function getOrigRequest(requests, req) {
  if (!req.id.endsWith("-clone")) {
    return req;
  }

  const origId = req.id.replace(/-clone$/, "");
  return requests.find(r => r.id === origId);
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
    let dataSorter = Sorters[sort.type || "waterfall"];

    function sortWithClones(a, b) {
      // If one request is a clone of the other, sort them next to each other
      if (a.id == b.id + "-clone") {
        return +1;
      } else if (a.id + "-clone" == b.id) {
        return -1;
      }

      // Otherwise, get the original requests and compare them
      return dataSorter(
        getOrigRequest(requests, a),
        getOrigRequest(requests, b)
      );
    }

    const ascending = sort.ascending ? +1 : -1;
    return (a, b) => ascending * sortWithClones(a, b, dataSorter);
  }
);

const getSortedRequests = createSelector(
  state => state.requests.requests,
  getSortFn,
  (requests, sortFn) => requests.sort(sortFn)
);

const getDisplayedRequests = createSelector(
  state => state.requests.requests,
  getFilterFn,
  getSortFn,
  (requests, filterFn, sortFn) => requests.filter(filterFn).sort(sortFn)
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
  requests => {
    if (!requests.selectedId) {
      return null;
    }

    return requests.requests.find(r => r.id === requests.selectedId);
  }
);

function getRequestById(state, id) {
  return state.requests.requests.find(r => r.id === id);
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
