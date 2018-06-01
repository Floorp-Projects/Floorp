/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const {
  ADD_REQUEST,
  CLEAR_REQUESTS,
  CLONE_SELECTED_REQUEST,
  REMOVE_SELECTED_CUSTOM_REQUEST,
  SEND_CUSTOM_REQUEST,
  TOGGLE_RECORDING,
  UPDATE_REQUEST,
} = require("../constants");
const { getSelectedRequest } = require("../selectors/index");

function addRequest(id, data, batch) {
  return {
    type: ADD_REQUEST,
    id,
    data,
    meta: { batch },
  };
}

function updateRequest(id, data, batch) {
  return {
    type: UPDATE_REQUEST,
    id,
    data,
    meta: { batch },
  };
}

/**
 * Clone the currently selected request, set the "isCustom" attribute.
 * Used by the "Edit and Resend" feature.
 */
function cloneSelectedRequest() {
  return {
    type: CLONE_SELECTED_REQUEST
  };
}

/**
 * Send a new HTTP request using the data in the custom request form.
 */
function sendCustomRequest(connector) {
  return (dispatch, getState) => {
    const selected = getSelectedRequest(getState());

    if (!selected) {
      return;
    }

    // Send a new HTTP request using the data in the custom request form
    const data = {
      url: selected.url,
      method: selected.method,
      httpVersion: selected.httpVersion,
    };
    if (selected.requestHeaders) {
      data.headers = selected.requestHeaders.headers;
    }
    if (selected.requestPostData) {
      data.body = selected.requestPostData.postData.text;
    }

    connector.sendHTTPRequest(data, (response) => {
      return dispatch({
        type: SEND_CUSTOM_REQUEST,
        id: response.eventActor.actor,
      });
    });
  };
}

/**
 * Remove a request from the list. Supports removing only cloned requests with a
 * "isCustom" attribute. Other requests never need to be removed.
 */
function removeSelectedCustomRequest() {
  return {
    type: REMOVE_SELECTED_CUSTOM_REQUEST
  };
}

function clearRequests() {
  return {
    type: CLEAR_REQUESTS
  };
}

/**
 * Toggle monitoring
 */
function toggleRecording() {
  return {
    type: TOGGLE_RECORDING
  };
}

module.exports = {
  addRequest,
  clearRequests,
  cloneSelectedRequest,
  removeSelectedCustomRequest,
  sendCustomRequest,
  toggleRecording,
  updateRequest,
};
