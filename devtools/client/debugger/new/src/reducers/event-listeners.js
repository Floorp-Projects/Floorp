"use strict";

Object.defineProperty(exports, "__esModule", {
  value: true
});
exports.getEventListeners = getEventListeners;

/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */

/**
 * Event listeners reducer
 * @module reducers/event-listeners
 */
const initialEventListenersState = {
  activeEventNames: [],
  listeners: [],
  fetchingListeners: false
};

function update(state = initialEventListenersState, action, emit) {
  switch (action.type) {
    case "UPDATE_EVENT_BREAKPOINTS":
      state.activeEventNames = action.eventNames; // emit("activeEventNames", state.activeEventNames);

      break;

    case "FETCH_EVENT_LISTENERS":
      if (action.status === "begin") {
        state.fetchingListeners = true;
      } else if (action.status === "done") {
        state.fetchingListeners = false;
        state.listeners = action.listeners;
      }

      break;

    case "NAVIGATE":
      return initialEventListenersState;
  }

  return state;
}

function getEventListeners(state) {
  return state.eventListeners.listeners;
}

exports.default = update;