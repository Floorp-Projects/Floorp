/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const {
  MSG_ADD,
  MSG_SELECT,
  MSG_OPEN_DETAILS,
  MSG_CLEAR,
  MSG_TOGGLE_FILTER_TYPE,
  MSG_TOGGLE_CONTROL,
  MSG_SET_FILTER_TEXT,
  MSG_TOGGLE_COLUMN,
  MSG_RESET_COLUMNS,
  MSG_CLOSE_CONNECTION,
} = require("resource://devtools/client/netmonitor/src/constants.js");

const {
  getDisplayedMessages,
} = require("resource://devtools/client/netmonitor/src/selectors/index.js");
const PAGE_SIZE_ITEM_COUNT_RATIO = 5;

/**
 * Add message into state.
 */
function addMessage(httpChannelId, data, batch) {
  return {
    type: MSG_ADD,
    httpChannelId,
    data,
    meta: { batch },
  };
}

/**
 * Select message.
 */
function selectMessage(message) {
  return {
    type: MSG_SELECT,
    open: true,
    message,
  };
}

/**
 * Open message details panel.
 *
 * @param {boolean} open - expected message details panel open state
 */
function openMessageDetails(open) {
  return {
    type: MSG_OPEN_DETAILS,
    open,
  };
}

/**
 * Clear all messages from the MessageListContent
 * component belonging to the current channelId
 */
function clearMessages() {
  return {
    type: MSG_CLEAR,
  };
}

/**
 * Show filtered messages from the MessageListContent
 * component belonging to the current channelId
 */
function toggleMessageFilterType(filter) {
  return {
    type: MSG_TOGGLE_FILTER_TYPE,
    filter,
  };
}

/**
 * Show control frames from the MessageListContent
 * component belonging to the current channelId
 */
function toggleControlFrames() {
  return {
    type: MSG_TOGGLE_CONTROL,
  };
}

/**
 * Set filter text in toolbar.
 *
 */
function setMessageFilterText(text) {
  return {
    type: MSG_SET_FILTER_TEXT,
    text,
  };
}

/**
 * Resets all Messages columns to their default state.
 *
 */
function resetMessageColumns() {
  return {
    type: MSG_RESET_COLUMNS,
  };
}

/**
 * Toggles a Message column
 *
 * @param {string} column - The column that is going to be toggled
 */
function toggleMessageColumn(column) {
  return {
    type: MSG_TOGGLE_COLUMN,
    column,
  };
}

/**
 * Sets current connection status to closed
 *
 * @param {number} httpChannelId - Unique id identifying the channel
 * @param {boolean} wasClean - False if connection terminated due to error
 * @param {number} code - Error code
 * @param {string} reason
 */
function closeConnection(httpChannelId, wasClean, code, reason) {
  return {
    type: MSG_CLOSE_CONNECTION,
    httpChannelId,
    wasClean,
    code,
    reason,
  };
}

/**
 * Move the selection up to down according to the "delta" parameter. Possible values:
 * - Number: positive or negative, move up or down by specified distance
 * - "PAGE_UP" | "PAGE_DOWN" (String): page up or page down
 * - +Infinity | -Infinity: move to the start or end of the list
 */
function selectMessageDelta(delta) {
  return ({ dispatch, getState }) => {
    const state = getState();
    const messages = getDisplayedMessages(state);

    if (messages.length === 0) {
      return;
    }

    const selIndex = messages.findIndex(
      r => r === state.messages.selectedMessage
    );

    if (delta === "PAGE_DOWN") {
      delta = Math.ceil(messages.length / PAGE_SIZE_ITEM_COUNT_RATIO);
    } else if (delta === "PAGE_UP") {
      delta = -Math.ceil(messages.length / PAGE_SIZE_ITEM_COUNT_RATIO);
    }

    const newIndex = Math.min(
      Math.max(0, selIndex + delta),
      messages.length - 1
    );
    const newItem = messages[newIndex];
    dispatch(selectMessage(newItem));
  };
}

module.exports = {
  addMessage,
  clearMessages,
  closeConnection,
  openMessageDetails,
  resetMessageColumns,
  selectMessage,
  selectMessageDelta,
  setMessageFilterText,
  toggleControlFrames,
  toggleMessageColumn,
  toggleMessageFilterType,
};
