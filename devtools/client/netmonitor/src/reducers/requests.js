/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const {
  processNetworkUpdates,
} = require("devtools/client/netmonitor/src/utils/request-utils");
const {
  ADD_REQUEST,
  SET_EVENT_STREAM_FLAG,
  CLEAR_REQUESTS,
  CLONE_REQUEST,
  CLONE_SELECTED_REQUEST,
  OPEN_NETWORK_DETAILS,
  REMOVE_SELECTED_CUSTOM_REQUEST,
  RIGHT_CLICK_REQUEST,
  SELECT_REQUEST,
  PRESELECT_REQUEST,
  SEND_CUSTOM_REQUEST,
  SET_RECORDING_STATE,
  UPDATE_REQUEST,
} = require("devtools/client/netmonitor/src/constants");

/**
 * This structure stores list of all HTTP requests received
 * from the backend. It's using plain JS structures to store
 * data instead of ImmutableJS, which is performance expensive.
 */
function Requests() {
  return {
    // Map with all requests (key = actor ID, value = request object)
    requests: [],
    // Selected request ID
    selectedId: null,
    // Right click request represents the last request that was clicked
    clickedRequestId: null,
    // @backward-compact { version 85 } The preselectedId can either be
    // the actor id on old servers, or the resourceId on new ones.
    preselectedId: null,
    // True if the monitor is recording HTTP traffic
    recording: true,
    // Auxiliary fields to hold requests stats
    firstStartedMs: +Infinity,
    lastEndedMs: -Infinity,
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
      return addRequest(state, action);
    }

    // Update an existing request (with received data).
    case UPDATE_REQUEST: {
      return updateRequest(state, action);
    }

    // Add isEventStream flag to a request.
    case SET_EVENT_STREAM_FLAG: {
      return setEventStreamFlag(state, action);
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
        clickedRequestId: action.id,
        selectedId: action.id,
      };
    }

    // Clone selected request for re-send.
    case CLONE_REQUEST: {
      return cloneRequest(state, action.id);
    }

    case CLONE_SELECTED_REQUEST: {
      return cloneRequest(state, state.selectedId);
    }

    case RIGHT_CLICK_REQUEST: {
      return {
        ...state,
        clickedRequestId: action.id,
      };
    }

    case PRESELECT_REQUEST: {
      return {
        ...state,
        preselectedId: action.id,
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
      return closeCustomRequest({ ...state, preselectedId: action.id });
    }

    // Pause/resume button clicked.
    case SET_RECORDING_STATE: {
      return {
        ...state,
        recording: action.recording,
      };
    }

    // Side bar with request details opened.
    case OPEN_NETWORK_DETAILS: {
      const nextState = { ...state };
      if (!action.open) {
        nextState.selectedId = null;
        return nextState;
      }

      if (!state.selectedId && action.defaultSelectedId) {
        nextState.selectedId = action.defaultSelectedId;
        return nextState;
      }

      return state;
    }

    default:
      return state;
  }
}

// Helpers

function addRequest(state, action) {
  const nextState = { ...state };
  // The target front is not used and cannot be serialized by redux
  // eslint-disable-next-line no-unused-vars
  const { targetFront, ...requestData } = action.data;
  const newRequest = {
    id: action.id,
    ...requestData,
  };

  nextState.requests = [...state.requests, newRequest];

  // Update the started/ended timestamps.
  const { startedMs } = action.data;
  if (startedMs < state.firstStartedMs) {
    nextState.firstStartedMs = startedMs;
  }
  if (startedMs > state.lastEndedMs) {
    nextState.lastEndedMs = startedMs;
  }

  // Select the request if it was preselected and there is no other selection.
  if (state.preselectedId) {
    if (state.preselectedId === action.id) {
      nextState.selectedId = state.selectedId || state.preselectedId;
    }
    // @backward-compact { version 85 } The preselectedId can be resourceId
    // instead of actor id when a custom request is created, and could not be
    // selected immediately because it was not yet in the request map.
    else if (state.preselectedId === newRequest.resourceId) {
      nextState.selectedId = action.id;
    }
    nextState.preselectedId = null;
  }

  return nextState;
}

function updateRequest(state, action) {
  const { requests, lastEndedMs } = state;

  const { id } = action;
  const index = requests.findIndex(needle => needle.id === id);
  if (index === -1) {
    return state;
  }
  const request = requests[index];

  const nextRequest = {
    ...request,
    ...processNetworkUpdates(action.data),
  };
  const requestEndTime =
    nextRequest.startedMs +
    (nextRequest.eventTimings ? nextRequest.eventTimings.totalTime : 0);

  const nextRequests = [...requests];
  nextRequests[index] = nextRequest;
  return {
    ...state,
    requests: nextRequests,
    lastEndedMs: requestEndTime > lastEndedMs ? requestEndTime : lastEndedMs,
  };
}

function setEventStreamFlag(state, action) {
  const { requests } = state;
  const { id } = action;
  const index = requests.findIndex(needle => needle.id === id);
  if (index === -1) {
    return state;
  }

  const request = requests[index];

  const nextRequest = {
    ...request,
    isEventStream: true,
  };

  const nextRequests = [...requests];
  nextRequests[index] = nextRequest;
  return {
    ...state,
    requests: nextRequests,
  };
}

function cloneRequest(state, id) {
  const { requests } = state;

  if (!id) {
    return state;
  }

  const clonedRequest = requests.find(needle => needle.id === id);
  if (!clonedRequest) {
    return state;
  }

  const newRequest = {
    id: clonedRequest.id + "-clone",
    method: clonedRequest.method,
    cause: clonedRequest.cause,
    url: clonedRequest.url,
    urlDetails: clonedRequest.urlDetails,
    requestHeaders: clonedRequest.requestHeaders,
    requestPostData: clonedRequest.requestPostData,
    requestPostDataAvailable: clonedRequest.requestPostDataAvailable,
    requestHeadersAvailable: clonedRequest.requestHeadersAvailable,
    isCustom: true,
  };

  return {
    ...state,
    requests: [...requests, newRequest],
    selectedId: newRequest.id,
    preselectedId: id,
  };
}

/**
 * Remove the currently selected custom request.
 */
function closeCustomRequest(state) {
  const { requests, selectedId, preselectedId } = state;

  if (!selectedId) {
    return state;
  }

  // Find the cloned requests to be removed
  const removedRequest = requests.find(needle => needle.id === selectedId);

  // If the custom request is already in the Map, select it immediately,
  // and reset `preselectedId` attribute.
  // @backward-compact { version 85 } The preselectId can also be a resourceId
  // or an actor id.
  const customRequest = requests.find(
    needle => needle.id === preselectedId || needle.resourceId === preselectedId
  );
  const hasPreselectedId = preselectedId && customRequest;

  return {
    ...state,
    // Only custom requests can be removed
    [removedRequest?.isCustom && "requests"]: requests.filter(
      item => item.id !== selectedId
    ),
    preselectedId: hasPreselectedId ? null : preselectedId,
    selectedId: hasPreselectedId ? customRequest.id : null,
  };
}

module.exports = {
  Requests,
  requestsReducer,
};
