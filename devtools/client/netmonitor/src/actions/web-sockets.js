/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const {
  WS_ADD_FRAME,
  WS_SELECT_FRAME,
  WS_OPEN_FRAME_DETAILS,
  WS_CLEAR_FRAMES,
  WS_TOGGLE_FRAME_FILTER_TYPE,
  WS_SET_REQUEST_FILTER_TEXT,
  WS_TOGGLE_COLUMN,
  WS_RESET_COLUMNS,
} = require("../constants");

/**
 * Add frame into state.
 */
function addFrame(httpChannelId, data, batch) {
  return {
    type: WS_ADD_FRAME,
    httpChannelId,
    data,
    meta: { batch },
  };
}

/**
 * Select frame.
 */
function selectFrame(frame) {
  return {
    type: WS_SELECT_FRAME,
    open: true,
    frame,
  };
}

/**
 * Open frame details panel.
 *
 * @param {boolean} open - expected frame details panel open state
 */
function openFrameDetails(open) {
  return {
    type: WS_OPEN_FRAME_DETAILS,
    open,
  };
}

/**
 * Clear all frames from the FrameListContent
 * component belonging to the current channelId
 */
function clearFrames() {
  return {
    type: WS_CLEAR_FRAMES,
  };
}

/**
 * Show filtered frames from the FrameListContent
 * component belonging to the current channelId
 */
function toggleFrameFilterType(filter) {
  return {
    type: WS_TOGGLE_FRAME_FILTER_TYPE,
    filter,
  };
}

/**
 * Set filter text in toolbar.
 *
 */
function setFrameFilterText(text) {
  return {
    type: WS_SET_REQUEST_FILTER_TEXT,
    text,
  };
}

/**
 * Resets all WebSockets columns to their default state.
 *
 */
function resetWebSocketsColumns() {
  return {
    type: WS_RESET_COLUMNS,
  };
}

/**
 * Toggles a WebSockets column
 *
 * @param {string} column - The column that is going to be toggled
 */
function toggleWebSocketsColumn(column) {
  return {
    type: WS_TOGGLE_COLUMN,
    column,
  };
}

module.exports = {
  addFrame,
  selectFrame,
  openFrameDetails,
  clearFrames,
  toggleFrameFilterType,
  setFrameFilterText,
  resetWebSocketsColumns,
  toggleWebSocketsColumn,
};
