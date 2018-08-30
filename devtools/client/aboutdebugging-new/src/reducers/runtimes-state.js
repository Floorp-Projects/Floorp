/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const {
  NETWORK_LOCATIONS_UPDATED,
  RUNTIMES,
} = require("../constants");

function RuntimesState(networkRuntimes = []) {
  return {
    networkRuntimes
  };
}

function runtimesReducer(state = RuntimesState(), action) {
  switch (action.type) {
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
