/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const {
  WS_ADD_FRAME,
} = require("../constants");

/**
 * This structure stores list of all WebSocket frames received
 * from the backend.
 */
function WebSockets() {
  return {
    // Map with all requests (key = channelId, value = array of frame objects)
    frames: new Map(),
  };
}

/**
 * This reducer is responsible for maintaining list of
 * WebSocket frames within the Network panel.
 */
function webSocketsReducer(state = WebSockets(), action) {
  switch (action.type) {
    // Appending new frame into the map.
    case WS_ADD_FRAME: {
      const nextState = { ...state };

      const newFrame = {
        httpChannelId: action.httpChannelId,
        ...action.data,
      };

      nextState.frames = mapSet(state.frames, newFrame.httpChannelId, newFrame);

      return nextState;
    }

    default:
      return state;
  }
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

module.exports = {
  WebSockets,
  webSocketsReducer,
};
