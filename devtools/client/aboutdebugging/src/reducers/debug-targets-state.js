/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const {
  EXTENSION_BGSCRIPT_STATUS_UPDATED,
  REQUEST_EXTENSIONS_SUCCESS,
  REQUEST_PROCESSES_SUCCESS,
  REQUEST_TABS_SUCCESS,
  REQUEST_WORKERS_SUCCESS,
  TEMPORARY_EXTENSION_RELOAD_FAILURE,
  TEMPORARY_EXTENSION_RELOAD_START,
  TERMINATE_EXTENSION_BGSCRIPT_FAILURE,
  TERMINATE_EXTENSION_BGSCRIPT_START,
  UNWATCH_RUNTIME_SUCCESS,
} = require("resource://devtools/client/aboutdebugging/src/constants.js");

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

function updateExtensionDetails(extensions, id, updatedDetails) {
  // extensions is meant to be either state.installExtensions or state.temporaryExtensions.
  return extensions.map(extension => {
    if (extension.id === id) {
      extension = Object.assign({}, extension);

      extension.details = Object.assign({}, extension.details, updatedDetails);
    }
    return extension;
  });
}

function updateTemporaryExtension(state, id, updatedDetails) {
  return updateExtensionDetails(state.temporaryExtensions, id, updatedDetails);
}

function updateInstalledExtension(state, id, updatedDetails) {
  return updateExtensionDetails(state.installedExtensions, id, updatedDetails);
}

function updateExtension(state, id, updatedDetails) {
  return {
    installedExtensions: updateInstalledExtension(state, id, updatedDetails),
    temporaryExtensions: updateTemporaryExtension(state, id, updatedDetails),
  };
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
    case TERMINATE_EXTENSION_BGSCRIPT_START: {
      const { id } = action;
      const { installedExtensions, temporaryExtensions } = updateExtension(
        state,
        id,
        {
          // Clear the last error if one was still set.
          lastTerminateBackgroundScriptError: null,
        }
      );
      return Object.assign({}, state, {
        installedExtensions,
        temporaryExtensions,
      });
    }
    case TERMINATE_EXTENSION_BGSCRIPT_FAILURE: {
      const { id, error } = action;
      const { installedExtensions, temporaryExtensions } = updateExtension(
        state,
        id,
        {
          lastTerminateBackgroundScriptError: error.message,
        }
      );
      return Object.assign({}, state, {
        installedExtensions,
        temporaryExtensions,
      });
    }
    case EXTENSION_BGSCRIPT_STATUS_UPDATED: {
      const { id, backgroundScriptStatus } = action;
      const { installedExtensions, temporaryExtensions } = updateExtension(
        state,
        id,
        {
          backgroundScriptStatus,
          // Clear the last error if one was still set.
          lastTerminateBackgroundScriptError: null,
        }
      );
      return Object.assign({}, state, {
        installedExtensions,
        temporaryExtensions,
      });
    }

    default:
      return state;
  }
}

module.exports = {
  DebugTargetsState,
  debugTargetsReducer,
};
