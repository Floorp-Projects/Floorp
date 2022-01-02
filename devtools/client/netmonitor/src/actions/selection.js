/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const { SELECT_REQUEST } = require("devtools/client/netmonitor/src/constants");
const {
  getDisplayedRequests,
  getSortedRequests,
} = require("devtools/client/netmonitor/src/selectors/index");

const PAGE_SIZE_ITEM_COUNT_RATIO = 5;

/**
 * Select request with a given id.
 */
function selectRequest(id, request) {
  return {
    type: SELECT_REQUEST,
    id,
    request,
  };
}

/**
 * Select request with a given index (sorted order)
 */
function selectRequestByIndex(index) {
  return ({ dispatch, getState }) => {
    const requests = getSortedRequests(getState());
    let itemId;
    if (index >= 0 && index < requests.length) {
      itemId = requests[index].id;
    }
    dispatch(selectRequest(itemId));
  };
}

/**
 * Move the selection up to down according to the "delta" parameter. Possible values:
 * - Number: positive or negative, move up or down by specified distance
 * - "PAGE_UP" | "PAGE_DOWN" (String): page up or page down
 * - +Infinity | -Infinity: move to the start or end of the list
 */
function selectDelta(delta) {
  return ({ dispatch, getState }) => {
    const state = getState();
    const requests = getDisplayedRequests(state);

    if (!requests.length) {
      return;
    }

    const selIndex = requests.findIndex(
      r => r.id === state.requests.selectedId
    );

    if (delta === "PAGE_DOWN") {
      delta = Math.ceil(requests.length / PAGE_SIZE_ITEM_COUNT_RATIO);
    } else if (delta === "PAGE_UP") {
      delta = -Math.ceil(requests.length / PAGE_SIZE_ITEM_COUNT_RATIO);
    }

    const newIndex = Math.min(
      Math.max(0, selIndex + delta),
      requests.length - 1
    );
    const newItem = requests[newIndex];
    dispatch(selectRequest(newItem.id, newItem));
  };
}

module.exports = {
  selectRequest,
  selectRequestByIndex,
  selectDelta,
};
