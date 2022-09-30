/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const {
  ADD_SEARCH_QUERY,
  ADD_SEARCH_RESULT,
  CLEAR_SEARCH_RESULTS,
  ADD_ONGOING_SEARCH,
  SEARCH_STATUS,
  TOGGLE_SEARCH_CASE_SENSITIVE_SEARCH,
  UPDATE_SEARCH_STATUS,
  SET_TARGET_SEARCH_RESULT,
} = require("resource://devtools/client/netmonitor/src/constants.js");

/**
 * Search reducer stores the following data:
 * - query [String]: the string the user is looking for
 * - results [Object]: the list of search results
 * - ongoingSearch [Object]: the object representing the current search
 * - status [String]: status of the current search (see constants.js)
 */
function Search(overrideParams = {}) {
  return Object.assign(
    {
      query: "",
      results: [],
      ongoingSearch: null,
      status: SEARCH_STATUS.INITIAL,
      caseSensitive: false,
      targetSearchResult: null,
    },
    overrideParams
  );
}

function search(state = new Search(), action) {
  switch (action.type) {
    case ADD_SEARCH_QUERY:
      return onAddSearchQuery(state, action);
    case ADD_SEARCH_RESULT:
      return onAddSearchResult(state, action);
    case CLEAR_SEARCH_RESULTS:
      return onClearSearchResults(state);
    case ADD_ONGOING_SEARCH:
      return onAddOngoingSearch(state, action);
    case TOGGLE_SEARCH_CASE_SENSITIVE_SEARCH:
      return onToggleCaseSensitiveSearch(state);
    case UPDATE_SEARCH_STATUS:
      return onUpdateSearchStatus(state, action);
    case SET_TARGET_SEARCH_RESULT:
      return onSetTargetSearchResult(state, action);
  }
  return state;
}

function onAddSearchQuery(state, action) {
  return {
    ...state,
    query: action.query,
  };
}

function onAddSearchResult(state, action) {
  const { resource } = action;
  const results = state.results.slice();
  results.push({
    resource,
    results: action.result,
  });

  return {
    ...state,
    results,
  };
}

function onClearSearchResults(state) {
  return {
    ...state,
    results: [],
  };
}

function onAddOngoingSearch(state, action) {
  return {
    ...state,
    ongoingSearch: action.ongoingSearch,
  };
}

function onToggleCaseSensitiveSearch(state) {
  return {
    ...state,
    caseSensitive: !state.caseSensitive,
  };
}

function onUpdateSearchStatus(state, action) {
  return {
    ...state,
    status: action.status,
  };
}

function onSetTargetSearchResult(state, action) {
  return {
    ...state,
    targetSearchResult: action.searchResult,
  };
}

module.exports = {
  Search,
  search,
};
