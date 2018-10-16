/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const {
  CONNECT_RUNTIME_SUCCESS,
  DISCONNECT_RUNTIME_SUCCESS,
  NETWORK_LOCATIONS_UPDATED,
  RUNTIMES,
  UNWATCH_RUNTIME_SUCCESS,
  USB_RUNTIMES_UPDATED,
  WATCH_RUNTIME_SUCCESS,
} = require("../constants");

const {
  findRuntimeById,
} = require("../modules/runtimes-state-helper");

// Map between known runtime types and nodes in the runtimes state.
const TYPE_TO_RUNTIMES_KEY = {
  [RUNTIMES.THIS_FIREFOX]: "thisFirefoxRuntimes",
  [RUNTIMES.NETWORK]: "networkRuntimes",
  [RUNTIMES.USB]: "usbRuntimes",
};

function RuntimesState() {
  return {
    networkRuntimes: [],
    selectedRuntimeId: null,
    thisFirefoxRuntimes: [{
      id: RUNTIMES.THIS_FIREFOX,
      type: RUNTIMES.THIS_FIREFOX,
    }],
    usbRuntimes: [],
  };
}

/**
 * Update the runtime matching the provided runtimeId with the content of updatedRuntime,
 * and return the new state.
 *
 * @param  {String} runtimeId
 *         The id of the runtime to update
 * @param  {Object} updatedRuntime
 *         Object used to update the runtime matching the idea using Object.assign.
 * @param  {Object} state
 *         Current runtimes state.
 * @return {Object} The updated state
 */
function _updateRuntimeById(runtimeId, updatedRuntime, state) {
  // Find the array of runtimes that contains the updated runtime.
  const runtime = findRuntimeById(runtimeId, state);
  const key = TYPE_TO_RUNTIMES_KEY[runtime.type];
  const runtimesToUpdate = state[key];

  // Update the runtime with the provided updatedRuntime.
  const updatedRuntimes = runtimesToUpdate.map(r => {
    if (r.id === runtimeId) {
      return Object.assign({}, r, updatedRuntime);
    }
    return r;
  });
  return Object.assign({}, state, { [key]: updatedRuntimes });
}

function runtimesReducer(state = RuntimesState(), action) {
  switch (action.type) {
    case CONNECT_RUNTIME_SUCCESS: {
      const { id, connection } = action.runtime;
      return _updateRuntimeById(id, { connection }, state);
    }

    case DISCONNECT_RUNTIME_SUCCESS: {
      const { id } = action.runtime;
      return _updateRuntimeById(id, { connection: null }, state);
    }

    case NETWORK_LOCATIONS_UPDATED: {
      const { locations } = action;
      const networkRuntimes = locations.map(location => {
        const [ host, port ] = location.split(":");
        return {
          id: location,
          extra: {
            connectionParameters: { host, port },
          },
          name: location,
          type: RUNTIMES.NETWORK,
        };
      });
      return Object.assign({}, state, { networkRuntimes });
    }

    case UNWATCH_RUNTIME_SUCCESS: {
      return Object.assign({}, state, { selectedRuntimeId: null });
    }

    case USB_RUNTIMES_UPDATED: {
      const { runtimes } = action;
      const usbRuntimes = runtimes.map(runtime => {
        return {
          id: runtime.id,
          extra: {
            connectionParameters: { socketPath: runtime._socketPath },
            deviceName: runtime.deviceName,
          },
          name: runtime.shortName,
          type: RUNTIMES.USB,
        };
      });
      return Object.assign({}, state, { usbRuntimes });
    }

    case WATCH_RUNTIME_SUCCESS: {
      const { id } = action.runtime;
      return Object.assign({}, state, { selectedRuntimeId: id });
    }

    default:
      return state;
  }
}

module.exports = {
  RuntimesState,
  runtimesReducer,
};
