/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const {
  WS_ADD_FRAME,
  WS_SELECT_FRAME,
  WS_OPEN_FRAME_DETAILS,
} = require("../constants");

/**
 * This structure stores list of all WebSocket frames received
 * from the backend.
 */
function WebSockets() {
  return {
    // Map with all requests (key = channelId, value = array of frame objects)
    frames: new Map(),
    selectedFrame: null,
    frameDetailsOpen: false,
  };
}

// Appending new frame into the map.
function addFrame(state, action) {
  const nextState = { ...state };

  const newFrame = {
    httpChannelId: action.httpChannelId,
    ...action.data,
  };

  nextState.frames = mapSet(state.frames, newFrame.httpChannelId, newFrame);

  return nextState;
}

// Select specific frame.
function selectFrame(state, action) {
  return {
    ...state,
    selectedFrame: action.frame,
    frameDetailsOpen: action.open,
  };
}

function openFrameDetails(state, action) {
  return {
    ...state,
    frameDetailsOpen: action.open,
  };
}

/**
 * Append new item into existing map and return new map.
 */
function mapSet(map, key, value) {
  const newMap = new Map(map);
  if (newMap.has(key)) {
    const framesArray = [...newMap.get(key)];
    framesArray.push(value);
    newMap.set(key, framesArray);
    return newMap;
  }
  return newMap.set(key, [value]);
}

/**
 * This reducer is responsible for maintaining list of
 * WebSocket frames within the Network panel.
 */
function webSockets(state = WebSockets(), action) {
  switch (action.type) {
    case WS_ADD_FRAME:
      return addFrame(state, action);
    case WS_SELECT_FRAME:
      return selectFrame(state, action);
    case WS_OPEN_FRAME_DETAILS:
      return openFrameDetails(state, action);
    default:
      return state;
  }
}

module.exports = {
  WebSockets,
  webSockets,
};
