/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const I = require("devtools/client/shared/vendor/immutable");
const { getUrlDetails } = require("../request-utils");
const {
  ADD_REQUEST,
  UPDATE_REQUEST,
  CLEAR_REQUESTS,
  SELECT_REQUEST,
  PRESELECT_REQUEST,
  CLONE_SELECTED_REQUEST,
  REMOVE_SELECTED_CUSTOM_REQUEST,
  OPEN_SIDEBAR,
} = require("../constants");

const Request = I.Record({
  id: null,
  // Set to true in case of a request that's being edited as part of "edit and resend"
  isCustom: false,
  // Request properties - at the beginning, they are unknown and are gradually filled in
  startedMillis: undefined,
  method: undefined,
  url: undefined,
  urlDetails: undefined,
  remotePort: undefined,
  remoteAddress: undefined,
  isXHR: undefined,
  cause: undefined,
  fromCache: undefined,
  fromServiceWorker: undefined,
  status: undefined,
  statusText: undefined,
  httpVersion: undefined,
  securityState: undefined,
  securityInfo: undefined,
  mimeType: "text/plain",
  contentSize: undefined,
  transferredSize: undefined,
  totalTime: undefined,
  eventTimings: undefined,
  headersSize: undefined,
  requestHeaders: undefined,
  requestHeadersFromUploadStream: undefined,
  requestCookies: undefined,
  requestPostData: undefined,
  responseHeaders: undefined,
  responseCookies: undefined,
  responseContent: undefined,
  responseContentDataUri: undefined,
  formDataSections: undefined,
});

const Requests = I.Record({
  // The collection of requests (keyed by id)
  requests: I.Map(),
  // Selection state
  selectedId: null,
  preselectedId: null,
  // Auxiliary fields to hold requests stats
  firstStartedMillis: +Infinity,
  lastEndedMillis: -Infinity,
});

const UPDATE_PROPS = [
  "method",
  "url",
  "remotePort",
  "remoteAddress",
  "status",
  "statusText",
  "httpVersion",
  "securityState",
  "securityInfo",
  "mimeType",
  "contentSize",
  "transferredSize",
  "totalTime",
  "eventTimings",
  "headersSize",
  "requestHeaders",
  "requestHeadersFromUploadStream",
  "requestCookies",
  "requestPostData",
  "responseHeaders",
  "responseCookies",
  "responseContent",
  "responseContentDataUri",
  "formDataSections",
];

function requestsReducer(state = new Requests(), action) {
  switch (action.type) {
    case ADD_REQUEST: {
      return state.withMutations(st => {
        let newRequest = new Request(Object.assign(
          { id: action.id },
          action.data,
          { urlDetails: getUrlDetails(action.data.url) }
        ));
        st.requests = st.requests.set(newRequest.id, newRequest);

        // Update the started/ended timestamps
        let { startedMillis } = action.data;
        if (startedMillis < st.firstStartedMillis) {
          st.firstStartedMillis = startedMillis;
        }
        if (startedMillis > st.lastEndedMillis) {
          st.lastEndedMillis = startedMillis;
        }

        // Select the request if it was preselected and there is no other selection
        if (st.preselectedId && st.preselectedId === action.id) {
          st.selectedId = st.selectedId || st.preselectedId;
          st.preselectedId = null;
        }
      });
    }

    case UPDATE_REQUEST: {
      let { requests, lastEndedMillis } = state;

      let updatedRequest = requests.get(action.id);
      if (!updatedRequest) {
        return state;
      }

      updatedRequest = updatedRequest.withMutations(request => {
        for (let [key, value] of Object.entries(action.data)) {
          if (!UPDATE_PROPS.includes(key)) {
            continue;
          }

          request[key] = value;

          switch (key) {
            case "url":
              // Compute the additional URL details
              request.urlDetails = getUrlDetails(value);
              break;
            case "totalTime":
              const endedMillis = request.startedMillis + value;
              lastEndedMillis = Math.max(lastEndedMillis, endedMillis);
              break;
            case "requestPostData":
              request.requestHeadersFromUploadStream = {
                headers: [],
                headersSize: 0,
              };
              break;
          }
        }
      });

      return state.withMutations(st => {
        st.requests = requests.set(updatedRequest.id, updatedRequest);
        st.lastEndedMillis = lastEndedMillis;
      });
    }
    case CLEAR_REQUESTS: {
      return new Requests();
    }
    case SELECT_REQUEST: {
      return state.set("selectedId", action.id);
    }
    case PRESELECT_REQUEST: {
      return state.set("preselectedId", action.id);
    }
    case CLONE_SELECTED_REQUEST: {
      let { requests, selectedId } = state;

      if (!selectedId) {
        return state;
      }

      let clonedRequest = requests.get(selectedId);
      if (!clonedRequest) {
        return state;
      }

      let newRequest = new Request({
        id: clonedRequest.id + "-clone",
        method: clonedRequest.method,
        url: clonedRequest.url,
        urlDetails: clonedRequest.urlDetails,
        requestHeaders: clonedRequest.requestHeaders,
        requestPostData: clonedRequest.requestPostData,
        isCustom: true
      });

      return state.withMutations(st => {
        st.requests = requests.set(newRequest.id, newRequest);
        st.selectedId = newRequest.id;
      });
    }
    case REMOVE_SELECTED_CUSTOM_REQUEST: {
      let { requests, selectedId } = state;

      if (!selectedId) {
        return state;
      }

      // Only custom requests can be removed
      let removedRequest = requests.get(selectedId);
      if (!removedRequest || !removedRequest.isCustom) {
        return state;
      }

      return state.withMutations(st => {
        st.requests = requests.delete(selectedId);
        st.selectedId = null;
      });
    }
    case OPEN_SIDEBAR: {
      if (!action.open) {
        return state.set("selectedId", null);
      }

      if (!state.selectedId && !state.requests.isEmpty()) {
        return state.set("selectedId", state.requests.first().id);
      }

      return state;
    }

    default:
      return state;
  }
}

module.exports = requestsReducer;
