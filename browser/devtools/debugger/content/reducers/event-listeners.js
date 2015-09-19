/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
"use strict";

const constants = require('../constants');

const FETCH_EVENT_LISTENERS_DELAY = 200; // ms

const initialState = {
  activeEventNames: [],
  listeners: [],
  fetchingListeners: false,
};

function update(state = initialState, action, emit) {
  switch(action.type) {
  case constants.UPDATE_EVENT_BREAKPOINTS:
    state.activeEventNames = action.eventNames;
    emit("@redux:activeEventNames", state.activeEventNames);
    break;
  case constants.FETCH_EVENT_LISTENERS:
    if (action.status === "begin") {
      state.fetchingListeners = true;
    }
    else if (action.status === "done") {
      state.fetchingListeners = false;
      state.listeners = action.listeners;
      emit("@redux:listeners", state.listeners);
    }
    break;
  }

  return state;
}

module.exports = update;
