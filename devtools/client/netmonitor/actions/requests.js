/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const {
  ADD_REQUEST,
  UPDATE_REQUEST,
  CLONE_SELECTED_REQUEST,
  REMOVE_SELECTED_CUSTOM_REQUEST,
  CLEAR_REQUESTS,
} = require("../constants");

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

module.exports = {
  addRequest,
  updateRequest,
  cloneSelectedRequest,
  removeSelectedCustomRequest,
  clearRequests,
};
