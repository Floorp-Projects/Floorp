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
  UPDATE_SEARCH_STATUS,
  SEARCH_STATUS,
  SET_TARGET_SEARCH_RESULT,
  TOGGLE_SEARCH_CASE_SENSITIVE_SEARCH,
} = require("../constants");

const {
  getDisplayedRequests,
  getOngoingSearch,
  getSearchStatus,
  getRequestById,
} = require("../selectors/index");

const { selectRequest } = require("./selection");
const { selectDetailsPanelTab } = require("./ui");
const { fetchNetworkUpdatePacket } = require("../utils/request-utils");
const { searchInResource } = require("../workers/search/index");

/**
 * Search through all resources. This is the main action exported
 * from this module and consumed by Network panel UI.
 */
function search(connector, query) {
  let canceled = false;

  // Instantiate an `ongoingSearch` function/object. It's responsible
  // for triggering set of asynchronous steps like fetching
  // data from the backend and performing search over it.
  // This `ongoingSearch` is stored in the Search reducer, so it can
  // be canceled if needed (e.g. when new search is executed).
  const newOngoingSearch = async (dispatch, getState) => {
    const state = getState();

    dispatch(stopOngoingSearch());

    await dispatch(addOngoingSearch(newOngoingSearch));
    await dispatch(clearSearchResults());
    await dispatch(addSearchQuery(query));

    dispatch(updateSearchStatus(SEARCH_STATUS.FETCHING));

    // Loop over all displayed resources (in the sorted order),
    // fetch all the details data and run search worker that
    // search through the resource structure.
    const requests = getDisplayedRequests(state);
    for (const request of requests) {
      if (canceled) {
        return;
      }

      // Fetch all data for the resource.
      await loadResource(connector, request);
      if (canceled) {
        return;
      }

      // The state changed, so make sure to get fresh new reference
      // to the updated resource object.
      const updatedResource = getRequestById(getState(), request.id);
      await dispatch(searchResource(updatedResource, query));
    }

    dispatch(updateSearchStatus(SEARCH_STATUS.DONE));
  };

  // Implement support for canceling (used e.g. when a new search
  // is executed or the user stops the searching manually).
  newOngoingSearch.cancel = () => {
    canceled = true;
  };

  newOngoingSearch.isCanceled = () => {
    return canceled;
  };

  return newOngoingSearch;
}

/**
 * Fetch all data related to the specified resource from the backend.
 */
async function loadResource(connector, resource) {
  const updateTypes = [
    "responseHeaders",
    "requestHeaders",
    "responseCookies",
    "requestCookies",
    "requestPostData",
    "responseContent",
    "responseCache",
    "stackTrace",
    "securityInfo",
  ];

  return fetchNetworkUpdatePacket(connector.requestData, resource, updateTypes);
}

/**
 * Search through all data within the specified resource.
 */
function searchResource(resource, query) {
  return async (dispatch, getState) => {
    const state = getState();
    const ongoingSearch = getOngoingSearch(state);

    const modifiers = {
      caseSensitive: state.search.caseSensitive,
    };

    // Run search in a worker and wait for the results. The return
    // value is an array with search occurrences.
    const result = await searchInResource(resource, query, modifiers);

    if (!result.length || ongoingSearch.isCanceled()) {
      return;
    }

    dispatch(addSearchResult(resource, result));
  };
}

/**
 * Add search query to the reducer.
 */
function addSearchResult(resource, result) {
  return {
    type: ADD_SEARCH_RESULT,
    resource,
    result,
  };
}

/**
 * Add search query to the reducer.
 */
function addSearchQuery(query) {
  return {
    type: ADD_SEARCH_QUERY,
    query,
  };
}

/**
 * Clear all search results.
 */
function clearSearchResults() {
  return {
    type: CLEAR_SEARCH_RESULTS,
  };
}

/**
 * Used to clear and cancel an ongoing search.
 * @returns {Function}
 */
function clearSearchResultAndCancel() {
  return (dispatch, getState) => {
    dispatch(stopOngoingSearch());
    dispatch(clearSearchResults());
  };
}

/**
 * Update status of the current search.
 */
function updateSearchStatus(status) {
  return {
    type: UPDATE_SEARCH_STATUS,
    status,
  };
}

/**
 * Close the entire search panel.
 */
function closeSearch() {
  return (dispatch, getState) => {
    dispatch(stopOngoingSearch());
    dispatch({ type: CLOSE_SEARCH });
  };
}

/**
 * Open the entire search panel
 * @returns {Function}
 */
function openSearch() {
  return (dispatch, getState) => {
    dispatch({ type: OPEN_SEARCH });
  };
}

/**
 * Toggles case sensitive search
 * @returns {Function}
 */
function toggleCaseSensitiveSearch() {
  return (dispatch, getState) => {
    dispatch({ type: TOGGLE_SEARCH_CASE_SENSITIVE_SEARCH });
  };
}

/**
 * Toggle visibility of search panel in network panel
 */
function toggleSearchPanel() {
  return (dispatch, getState) => {
    const state = getState();
    state.search.panelOpen
      ? dispatch({ type: CLOSE_SEARCH })
      : dispatch({ type: OPEN_SEARCH });
  };
}

/**
 * Append new search object into the reducer. The search object
 * is cancellable and so, it implements `cancel` method.
 */
function addOngoingSearch(ongoingSearch) {
  return {
    type: ADD_ONGOING_SEARCH,
    ongoingSearch,
  };
}

/**
 * Cancel the current ongoing search.
 */
function stopOngoingSearch() {
  return (dispatch, getState) => {
    const state = getState();
    const ongoingSearch = getOngoingSearch(state);
    const status = getSearchStatus(state);

    if (ongoingSearch && status !== SEARCH_STATUS.DONE) {
      ongoingSearch.cancel();
      dispatch(updateSearchStatus(SEARCH_STATUS.CANCELED));
    }
  };
}

/**
 * This action is fired when the user selects a search result
 * within the Search panel. It opens the details side bar and
 * selects the right side panel to show the context of the
 * clicked search result.
 */
function navigate(searchResult) {
  return (dispatch, getState) => {
    // Store target search result in Search reducer. It's used
    // for search result navigation within the side panels.
    dispatch(setTargetSearchResult(searchResult));

    // Preselect the right side panel.
    dispatch(selectDetailsPanelTab(searchResult.panel));

    // Select related request in the UI (it also opens the
    // right side bar automatically).
    dispatch(selectRequest(searchResult.parentResource.id));
  };
}

function setTargetSearchResult(searchResult) {
  return {
    type: SET_TARGET_SEARCH_RESULT,
    searchResult,
  };
}

module.exports = {
  search,
  closeSearch,
  openSearch,
  clearSearchResults,
  addSearchQuery,
  toggleSearchPanel,
  navigate,
  setTargetSearchResult,
  toggleCaseSensitiveSearch,
  clearSearchResultAndCancel,
};
