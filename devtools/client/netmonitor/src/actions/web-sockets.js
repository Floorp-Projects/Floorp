/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const {
  WS_ADD_FRAME,
  WS_SELECT_FRAME,
  WS_OPEN_FRAME_DETAILS,
} = require("../constants");

function addFrame(httpChannelId, data) {
  return {
    type: WS_ADD_FRAME,
    httpChannelId,
    data,
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

module.exports = {
  addFrame,
  selectFrame,
  openFrameDetails,
};
