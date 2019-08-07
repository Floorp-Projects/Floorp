/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const {
  ADD_SEARCH_QUERY,
  ADD_SEARCH_RESULT,
  CLEAR_SEARCH_RESULTS,
  ADD_ONGOING_SEARCH,
  OPEN_SEARCH,
  CLOSE_SEARCH,
  SEARCH_STATUS,
  UPDATE_SEARCH_STATUS,
} = require("../constants");

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
      panelOpen: false,
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
    case CLOSE_SEARCH:
      return onCloseSearch(state);
    case OPEN_SEARCH:
      return onOpenSearch(state);
    case UPDATE_SEARCH_STATUS:
      return onUpdateSearchStatus(state, action);
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
  const results = state.results.slice();
  results.push({
    resource: action.resource,
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

function onCloseSearch(state) {
  return {
    ...state,
    panelOpen: false,
  };
}

function onOpenSearch(state) {
  return {
    ...state,
    panelOpen: true,
  };
}

function onUpdateSearchStatus(state, action) {
  return {
    ...state,
    status: action.status,
  };
}

module.exports = {
  Search,
  search,
};
