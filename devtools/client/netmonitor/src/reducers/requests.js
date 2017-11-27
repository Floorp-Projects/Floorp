/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const {
  getUrlDetails,
  processNetworkUpdates,
} = require("../utils/request-utils");
const {
  ADD_REQUEST,
  CLEAR_REQUESTS,
  CLONE_SELECTED_REQUEST,
  OPEN_NETWORK_DETAILS,
  REMOVE_SELECTED_CUSTOM_REQUEST,
  SELECT_REQUEST,
  SEND_CUSTOM_REQUEST,
  TOGGLE_RECORDING,
  UPDATE_REQUEST,
} = require("../constants");

/**
 * This structure stores list of all HTTP requests received
 * from the backend. It's using plain JS structures to store
 * data instead of ImmutableJS, which is performance expensive.
 */
function Requests() {
  return {
    // Map with all requests (key = actor ID, value = request object)
    requests: mapNew(),
    // Selected request ID
    selectedId: null,
    preselectedId: null,
    // True if the monitor is recording HTTP traffic
    recording: true,
    // Auxiliary fields to hold requests stats
    firstStartedMillis: +Infinity,
    lastEndedMillis: -Infinity,
  };
}

/**
 * This reducer is responsible for maintaining list of request
 * within the Network panel.
 */
function requestsReducer(state = Requests(), action) {
  switch (action.type) {
    // Appending new request into the list/map.
    case ADD_REQUEST: {
      let nextState = { ...state };

      let newRequest = {
        id: action.id,
        ...action.data,
        urlDetails: getUrlDetails(action.data.url),
      };

      nextState.requests = mapSet(state.requests, newRequest.id, newRequest);

      // Update the started/ended timestamps.
      let { startedMillis } = action.data;
      if (startedMillis < state.firstStartedMillis) {
        nextState.firstStartedMillis = startedMillis;
      }
      if (startedMillis > state.lastEndedMillis) {
        nextState.lastEndedMillis = startedMillis;
      }

      // Select the request if it was preselected and there is no other selection.
      if (state.preselectedId && state.preselectedId === action.id) {
        nextState.selectedId = state.selectedId || state.preselectedId;
        nextState.preselectedId = null;
      }

      return nextState;
    }

    // Update an existing request (with received data).
    case UPDATE_REQUEST: {
      let { requests, lastEndedMillis } = state;

      let request = requests.get(action.id);
      if (!request) {
        return state;
      }

      request = {
        ...request,
        ...processNetworkUpdates(action.data),
      };

      return {
        ...state,
        requests: mapSet(state.requests, action.id, request),
        lastEndedMillis: lastEndedMillis,
      };
    }

    // Remove all requests in the list. Create fresh new state
    // object, but keep value of the `recording` field.
    case CLEAR_REQUESTS: {
      return {
        ...Requests(),
        recording: state.recording,
      };
    }

    // Select specific request.
    case SELECT_REQUEST: {
      return {
        ...state,
        selectedId: action.id,
      };
    }

    // Clone selected request for re-send.
    case CLONE_SELECTED_REQUEST: {
      let { requests, selectedId } = state;

      if (!selectedId) {
        return state;
      }

      let clonedRequest = requests.get(selectedId);
      if (!clonedRequest) {
        return state;
      }

      let newRequest = {
        id: clonedRequest.id + "-clone",
        method: clonedRequest.method,
        url: clonedRequest.url,
        urlDetails: clonedRequest.urlDetails,
        requestHeaders: clonedRequest.requestHeaders,
        requestPostData: clonedRequest.requestPostData,
        isCustom: true
      };

      return {
        ...state,
        requests: mapSet(requests, newRequest.id, newRequest),
        selectedId: newRequest.id,
      };
    }

    // Removing temporary cloned request (created for re-send, but canceled).
    case REMOVE_SELECTED_CUSTOM_REQUEST: {
      return closeCustomRequest(state);
    }

    // Re-sending an existing request.
    case SEND_CUSTOM_REQUEST: {
      // When a new request with a given id is added in future, select it immediately.
      // where we know in advance the ID of the request, at a time when it
      // wasn't sent yet.
      return closeCustomRequest(state.set("preselectedId", action.id));
    }

    // Pause/resume button clicked.
    case TOGGLE_RECORDING: {
      return {
        ...state,
        recording: !state.recording,
      };
    }

    // Side bar with request details opened.
    case OPEN_NETWORK_DETAILS: {
      let nextState = { ...state };
      if (!action.open) {
        nextState.selectedId = null;
        return nextState;
      }

      if (!state.selectedId && !state.requests.isEmpty()) {
        nextState.selectedId = [...state.requests.values()][0].id;
        return nextState;
      }

      return state;
    }

    default:
      return state;
  }
}

// Helpers

/**
 * Remove the currently selected custom request.
 */
function closeCustomRequest(state) {
  let { requests, selectedId } = state;

  if (!selectedId) {
    return state;
  }

  let removedRequest = requests.get(selectedId);

  // Only custom requests can be removed
  if (!removedRequest || !removedRequest.isCustom) {
    return state;
  }

  return {
    ...state,
    requests: mapDelete(state.requests, selectedId),
    selectedId: null,
  };
}

// Immutability helpers
// FIXME The following helper API need refactoring, see bug 1418969.

/**
 * Clone an existing map.
 */
function mapNew(map) {
  let newMap = new Map(map);
  newMap.isEmpty = () => newMap.size == 0;
  newMap.valueSeq = () => [...newMap.values()];
  return newMap;
}

/**
 * Append new item into existing map and return new map.
 */
function mapSet(map, key, value) {
  let newMap = mapNew(map);
  return newMap.set(key, value);
}

/**
 * Remove an item from existing map and return new map.
 */
function mapDelete(map, key) {
  let newMap = mapNew(map);
  newMap.delete(key);
  return newMap;
}

module.exports = {
  Requests,
  requestsReducer,
};
