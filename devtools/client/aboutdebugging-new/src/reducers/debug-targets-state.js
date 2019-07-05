/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const {
  REQUEST_EXTENSIONS_SUCCESS,
  REQUEST_PROCESSES_SUCCESS,
  REQUEST_TABS_SUCCESS,
  REQUEST_WORKERS_SUCCESS,
  TEMPORARY_EXTENSION_RELOAD_FAILURE,
  TEMPORARY_EXTENSION_RELOAD_START,
  UNWATCH_RUNTIME_SUCCESS,
} = require("../constants");

function DebugTargetsState() {
  return {
    installedExtensions: [],
    otherWorkers: [],
    processes: [],
    serviceWorkers: [],
    sharedWorkers: [],
    tabs: [],
    temporaryExtensions: [],
  };
}

function updateTemporaryExtension(state, id, updatedDetails) {
  return state.temporaryExtensions.map(extension => {
    if (extension.id === id) {
      extension = Object.assign({}, extension);
      extension.details = Object.assign({}, extension.details, updatedDetails);
    }
    return extension;
  });
}

function debugTargetsReducer(state = DebugTargetsState(), action) {
  switch (action.type) {
    case UNWATCH_RUNTIME_SUCCESS: {
      return DebugTargetsState();
    }
    case REQUEST_EXTENSIONS_SUCCESS: {
      const { installedExtensions, temporaryExtensions } = action;
      return Object.assign({}, state, {
        installedExtensions,
        temporaryExtensions,
      });
    }
    case REQUEST_PROCESSES_SUCCESS: {
      const { processes } = action;
      return Object.assign({}, state, { processes });
    }
    case REQUEST_TABS_SUCCESS: {
      const { tabs } = action;
      return Object.assign({}, state, { tabs });
    }
    case REQUEST_WORKERS_SUCCESS: {
      const { otherWorkers, serviceWorkers, sharedWorkers } = action;
      return Object.assign({}, state, {
        otherWorkers,
        serviceWorkers,
        sharedWorkers,
      });
    }
    case TEMPORARY_EXTENSION_RELOAD_FAILURE: {
      const { id, error } = action;
      const temporaryExtensions = updateTemporaryExtension(state, id, {
        reloadError: error.message,
      });
      return Object.assign({}, state, { temporaryExtensions });
    }
    case TEMPORARY_EXTENSION_RELOAD_START: {
      const { id } = action;
      const temporaryExtensions = updateTemporaryExtension(state, id, {
        reloadError: null,
      });
      return Object.assign({}, state, { temporaryExtensions });
    }

    default:
      return state;
  }
}

module.exports = {
  DebugTargetsState,
  debugTargetsReducer,
};
