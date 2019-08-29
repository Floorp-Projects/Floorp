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
} = require("../constants");

const {
  getDisplayedRequests,
  getOngoingSearch,
  getSearchStatus,
  getRequestById,
} = require("../selectors/index");

const { fetchNetworkUpdatePacket } = require("../utils/request-utils");

const { searchInResource } = require("../workers/search/index");

/**
 * Search through all resources. This is the main action exported
 * from this module and consumed by Network panel UI.
 */
function search(connector, query) {
  let cancelled = false;

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
      if (cancelled) {
        return;
      }

      // Fetch all data for the resource.
      await loadResource(connector, request);

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
    cancelled = true;
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
    // Run search in a worker and wait for the results. The return
    // value is an array with search occurrences.
    const result = await searchInResource(resource, query);

    if (!result.length) {
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

function openSearch() {
  return (dispatch, getState) => {
    dispatch({ type: OPEN_SEARCH });
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
      dispatch(updateSearchStatus(SEARCH_STATUS.CANCELLED));
    }
  };
}

module.exports = {
  search,
  closeSearch,
  openSearch,
  clearSearchResults,
  addSearchQuery,
  toggleSearchPanel,
};
