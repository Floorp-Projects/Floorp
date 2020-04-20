/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

function getOngoingSearch(state) {
  return state.search.ongoingSearch;
}

function getSearchStatus(state) {
  return state.search.status;
}

function getSearchResultCount(state) {
  const { results } = state.search;
  return (
    (results.length !== 0
      ? results.reduce((total, current) => total + current.results.length, 0)
      : 0) + ""
  );
}

function getSearchResourceCount(state) {
  return state.search.results.length + "";
}

module.exports = {
  getOngoingSearch,
  getSearchStatus,
  getSearchResultCount,
  getSearchResourceCount,
};
