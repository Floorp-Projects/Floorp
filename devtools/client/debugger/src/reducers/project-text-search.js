/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */

// @format

/**
 * Project text search reducer
 * @module reducers/project-text-search
 */

export const statusType = {
  initial: "INITIAL",
  fetching: "FETCHING",
  cancelled: "CANCELLED",
  done: "DONE",
  error: "ERROR",
};

export function initialProjectTextSearchState() {
  return {
    query: "",
    results: [],
    ongoingSearch: null,
    status: statusType.initial,
  };
}

function update(state = initialProjectTextSearchState(), action) {
  switch (action.type) {
    case "ADD_QUERY":
      return { ...state, query: action.query };

    case "ADD_SEARCH_RESULT":
      if (action.result.matches.length === 0) {
        return state;
      }

      const result = {
        type: "RESULT",
        ...action.result,
        matches: action.result.matches.map(m => ({ type: "MATCH", ...m })),
      };
      return { ...state, results: [...state.results, result] };

    case "UPDATE_STATUS":
      const ongoingSearch =
        action.status == statusType.fetching ? state.ongoingSearch : null;
      return { ...state, status: action.status, ongoingSearch };

    case "CLEAR_SEARCH_RESULTS":
      return { ...state, results: [] };

    case "ADD_ONGOING_SEARCH":
      return { ...state, ongoingSearch: action.ongoingSearch };

    case "CLEAR_SEARCH":
    case "CLOSE_PROJECT_SEARCH":
    case "NAVIGATE":
      return initialProjectTextSearchState();
  }
  return state;
}

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

export default update;
