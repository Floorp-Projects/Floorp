/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const {
  CONNECT_RUNTIME_SUCCESS,
  DISCONNECT_RUNTIME_SUCCESS,
  REQUEST_EXTENSIONS_SUCCESS,
  REQUEST_TABS_SUCCESS,
  REQUEST_WORKERS_SUCCESS,
} = require("../constants");

function RuntimeState() {
  return {
    client: null,
    installedExtensions: [],
    otherWorkers: [],
    serviceWorkers: [],
    sharedWorkers: [],
    tabs: [],
    temporaryExtensions: [],
  };
}

function runtimeReducer(state = RuntimeState(), action) {
  switch (action.type) {
    case CONNECT_RUNTIME_SUCCESS: {
      const { client } = action;
      return Object.assign({}, state, { client });
    }
    case DISCONNECT_RUNTIME_SUCCESS: {
      return RuntimeState();
    }
    case REQUEST_EXTENSIONS_SUCCESS: {
      const { installedExtensions, temporaryExtensions } = action;
      return Object.assign({}, state, { installedExtensions, temporaryExtensions });
    }
    case REQUEST_TABS_SUCCESS: {
      const { tabs } = action;
      return Object.assign({}, state, { tabs });
    }
    case REQUEST_WORKERS_SUCCESS: {
      const { otherWorkers, serviceWorkers, sharedWorkers } = action;
      return Object.assign({}, state, { otherWorkers, serviceWorkers, sharedWorkers });
    }

    default:
      return state;
  }
}

module.exports = {
  RuntimeState,
  runtimeReducer,
};
