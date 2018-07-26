/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const {
  UPDATE_SELECTED_RUNTIME,
} = require("../constants");

function RuntimesState() {
  return {
    selectedRuntime: null,
  };
}

function runtimesReducer(state = RuntimesState(), action) {
  switch (action.type) {
    case UPDATE_SELECTED_RUNTIME: {
      return Object.assign({}, state, {
        selectedRuntime: action.runtime,
      });
    }

    default:
      return state;
  }
}

module.exports = {
  RuntimesState,
  runtimesReducer,
};
