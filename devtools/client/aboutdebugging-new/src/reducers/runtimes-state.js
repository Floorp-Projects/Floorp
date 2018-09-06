/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const {
  CONNECT_RUNTIME_SUCCESS,
  DISCONNECT_RUNTIME_SUCCESS,
  NETWORK_LOCATIONS_UPDATED,
  RUNTIMES,
} = require("../constants");

function RuntimesState(networkRuntimes = []) {
  return {
    networkRuntimes,
    thisFirefoxRuntimes: [{
      id: RUNTIMES.THIS_FIREFOX,
      type: RUNTIMES.THIS_FIREFOX,
    }]
  };
}

function runtimesReducer(state = RuntimesState(), action) {
  switch (action.type) {
    case CONNECT_RUNTIME_SUCCESS: {
      const { client } = action;
      const thisFirefoxRuntimes = [{
        id: RUNTIMES.THIS_FIREFOX,
        type: RUNTIMES.THIS_FIREFOX,
        client,
      }];
      return Object.assign({}, state, { thisFirefoxRuntimes });
    }

    case DISCONNECT_RUNTIME_SUCCESS: {
      const thisFirefoxRuntimes = [{
        id: RUNTIMES.THIS_FIREFOX,
        type: RUNTIMES.THIS_FIREFOX
      }];
      return Object.assign({}, state, { thisFirefoxRuntimes });
    }

    case NETWORK_LOCATIONS_UPDATED: {
      const { locations } = action;
      const networkRuntimes = locations.map(location => {
        return {
          id: location,
          type: RUNTIMES.NETWORK,
        };
      });
      return Object.assign({}, state, { networkRuntimes });
    }

    default:
      return state;
  }
}

module.exports = {
  RuntimesState,
  runtimesReducer,
};
