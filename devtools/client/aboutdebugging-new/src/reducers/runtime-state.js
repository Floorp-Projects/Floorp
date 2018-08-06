/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const {
  CONNECT_RUNTIME_SUCCESS,
  DISCONNECT_RUNTIME_SUCCESS,
} = require("../constants");

function RuntimeState() {
  return {
    client: null,
    tabs: [],
  };
}

function runtimeReducer(state = RuntimeState(), action) {
  switch (action.type) {
    case CONNECT_RUNTIME_SUCCESS: {
      const { client, tabs } = action;
      return { client, tabs };
    }
    case DISCONNECT_RUNTIME_SUCCESS: {
      return RuntimeState();
    }

    default:
      return state;
  }
}

module.exports = {
  RuntimeState,
  runtimeReducer,
};
