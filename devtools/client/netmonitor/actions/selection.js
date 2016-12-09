/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const { getDisplayedRequests } = require("../selectors/index");
const { SELECT_REQUEST, PRESELECT_REQUEST } = require("../constants");

/**
 * When a new request with a given id is added in future, select it immediately.
 * Used by the "Edit and Resend" feature, where we know in advance the ID of the
 * request, at a time when it wasn't sent yet.
 */
function preselectRequest(id) {
  return {
    type: PRESELECT_REQUEST,
    id
  };
}

/**
 * Select request with a given id.
 */
function selectRequest(id) {
  return {
    type: SELECT_REQUEST,
    id
  };
}

const PAGE_SIZE_ITEM_COUNT_RATIO = 5;

/**
 * Move the selection up to down according to the "delta" parameter. Possible values:
 * - Number: positive or negative, move up or down by specified distance
 * - "PAGE_UP" | "PAGE_DOWN" (String): page up or page down
 * - +Infinity | -Infinity: move to the start or end of the list
 */
function selectDelta(delta) {
  return (dispatch, getState) => {
    const state = getState();
    const requests = getDisplayedRequests(state);

    if (requests.isEmpty()) {
      return;
    }

    const selIndex = requests.findIndex(r => r.id === state.requests.selectedId);

    if (delta === "PAGE_DOWN") {
      delta = Math.ceil(requests.size / PAGE_SIZE_ITEM_COUNT_RATIO);
    } else if (delta === "PAGE_UP") {
      delta = -Math.ceil(requests.size / PAGE_SIZE_ITEM_COUNT_RATIO);
    }

    const newIndex = Math.min(Math.max(0, selIndex + delta), requests.size - 1);
    const newItem = requests.get(newIndex);
    dispatch(selectRequest(newItem.id));
  };
}

module.exports = {
  preselectRequest,
  selectRequest,
  selectDelta,
};
