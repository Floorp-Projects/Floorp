/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */

export function getTextSearchOperation(state) {
  return state.projectTextSearch.ongoingSearch;
}

export function getTextSearchResults(state) {
  return state.projectTextSearch.results;
}

export function getTextSearchStatus(state) {
  return state.projectTextSearch.status;
}

export function getTextSearchQuery(state) {
  return state.projectTextSearch.query;
}
