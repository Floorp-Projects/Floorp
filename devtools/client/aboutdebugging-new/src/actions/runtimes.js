/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const Actions = require("./index");

const {
  getCurrentRuntime,
  findRuntimeById,
} = require("../modules/runtimes-state-helper");
const { isSupportedDebugTarget } = require("../modules/debug-target-support");

const { createClientForRuntime } = require("../modules/runtime-client-factory");

const { remoteClientManager } =
  require("devtools/client/shared/remote-debugging/remote-client-manager");

const {
  CONNECT_RUNTIME_FAILURE,
  CONNECT_RUNTIME_START,
  CONNECT_RUNTIME_SUCCESS,
  DEBUG_TARGETS,
  DISCONNECT_RUNTIME_FAILURE,
  DISCONNECT_RUNTIME_START,
  DISCONNECT_RUNTIME_SUCCESS,
  RUNTIME_PREFERENCE,
  RUNTIMES,
  UNWATCH_RUNTIME_FAILURE,
  UNWATCH_RUNTIME_START,
  UNWATCH_RUNTIME_SUCCESS,
  UPDATE_CONNECTION_PROMPT_SETTING_FAILURE,
  UPDATE_CONNECTION_PROMPT_SETTING_START,
  UPDATE_CONNECTION_PROMPT_SETTING_SUCCESS,
  USB_RUNTIMES_UPDATED,
  WATCH_RUNTIME_FAILURE,
  WATCH_RUNTIME_START,
  WATCH_RUNTIME_SUCCESS,
} = require("../constants");

async function getRuntimeInfo(runtime, clientWrapper) {
  const { type } = runtime;
  const { name, channel, deviceName, version } =
    await clientWrapper.getDeviceDescription();
  const icon =
    (channel === "release" || channel === "beta" || channel === "aurora")
      ? `chrome://devtools/skin/images/aboutdebugging-firefox-${ channel }.svg`
      : "chrome://devtools/skin/images/aboutdebugging-firefox-nightly.svg";

  return {
    icon,
    deviceName,
    name,
    type,
    version,
  };
}

function onUSBDebuggerClientClosed() {
  // After scanUSBRuntimes action, updateUSBRuntimes action is called.
  // The closed runtime will be unwatched and disconnected explicitly in the action
  // if needed.
  window.AboutDebugging.store.dispatch(Actions.scanUSBRuntimes());
}

function connectRuntime(id) {
  return async (dispatch, getState) => {
    dispatch({ type: CONNECT_RUNTIME_START });
    try {
      const runtime = findRuntimeById(id, getState().runtimes);
      const clientWrapper = await createClientForRuntime(runtime);
      const info = await getRuntimeInfo(runtime, clientWrapper);

      const promptPrefName = RUNTIME_PREFERENCE.CONNECTION_PROMPT;
      const connectionPromptEnabled = await clientWrapper.getPreference(promptPrefName);
      const runtimeDetails = {
        clientWrapper,
        connectionPromptEnabled,
        info,
      };

      if (runtime.type === RUNTIMES.USB) {
        // `closed` event will be emitted when disabling remote debugging
        // on the connected USB runtime.
        clientWrapper.addOneTimeListener("closed", onUSBDebuggerClientClosed);
      }

      dispatch({
        type: CONNECT_RUNTIME_SUCCESS,
        runtime: {
          id,
          runtimeDetails,
          type: runtime.type,
        },
      });
    } catch (e) {
      dispatch({ type: CONNECT_RUNTIME_FAILURE, error: e });
    }
  };
}

function disconnectRuntime(id) {
  return async (dispatch, getState) => {
    dispatch({ type: DISCONNECT_RUNTIME_START });
    try {
      const runtime = findRuntimeById(id, getState().runtimes);
      const { clientWrapper } = runtime.runtimeDetails;

      if (runtime.type === RUNTIMES.USB) {
        clientWrapper.removeListener("closed", onUSBDebuggerClientClosed);
      }

      await clientWrapper.close();

      dispatch({
        type: DISCONNECT_RUNTIME_SUCCESS,
        runtime: {
          id,
          type: runtime.type,
        },
      });
    } catch (e) {
      dispatch({ type: DISCONNECT_RUNTIME_FAILURE, error: e });
    }
  };
}

function updateConnectionPromptSetting(connectionPromptEnabled) {
  return async (dispatch, getState) => {
    dispatch({ type: UPDATE_CONNECTION_PROMPT_SETTING_START });
    try {
      const runtime = getCurrentRuntime(getState().runtimes);
      const { clientWrapper } = runtime.runtimeDetails;
      const promptPrefName = RUNTIME_PREFERENCE.CONNECTION_PROMPT;
      await clientWrapper.setPreference(promptPrefName, connectionPromptEnabled);
      // Re-get actual value from the runtime.
      connectionPromptEnabled = await clientWrapper.getPreference(promptPrefName);

      dispatch({ type: UPDATE_CONNECTION_PROMPT_SETTING_SUCCESS,
                 runtime, connectionPromptEnabled });
    } catch (e) {
      dispatch({ type: UPDATE_CONNECTION_PROMPT_SETTING_FAILURE, error: e });
    }
  };
}

function watchRuntime(id) {
  return async (dispatch, getState) => {
    dispatch({ type: WATCH_RUNTIME_START });

    try {
      if (id === RUNTIMES.THIS_FIREFOX) {
        // THIS_FIREFOX connects and disconnects on the fly when opening the page.
        await dispatch(connectRuntime(RUNTIMES.THIS_FIREFOX));
      }

      // The selected runtime should already have a connected client assigned.
      const runtime = findRuntimeById(id, getState().runtimes);
      await dispatch({ type: WATCH_RUNTIME_SUCCESS, runtime });

      if (isSupportedDebugTarget(runtime.type, DEBUG_TARGETS.EXTENSION)) {
        dispatch(Actions.requestExtensions());
      }

      if (isSupportedDebugTarget(runtime.type, DEBUG_TARGETS.TAB)) {
        dispatch(Actions.requestTabs());
      }

      if (isSupportedDebugTarget(runtime.type, DEBUG_TARGETS.WORKER)) {
        dispatch(Actions.requestWorkers());
      }
    } catch (e) {
      dispatch({ type: WATCH_RUNTIME_FAILURE, error: e });
    }
  };
}

function unwatchRuntime(id) {
  return async (dispatch, getState) => {
    const runtime = findRuntimeById(id, getState().runtimes);

    dispatch({ type: UNWATCH_RUNTIME_START, runtime });

    try {
      if (id === RUNTIMES.THIS_FIREFOX) {
        // THIS_FIREFOX connects and disconnects on the fly when opening the page.
        await dispatch(disconnectRuntime(RUNTIMES.THIS_FIREFOX));
      }

      dispatch({ type: UNWATCH_RUNTIME_SUCCESS });
    } catch (e) {
      dispatch({ type: UNWATCH_RUNTIME_FAILURE, error: e });
    }
  };
}

function updateUSBRuntimes(runtimes) {
  return async (dispatch, getState) => {
    const currentRuntime = getCurrentRuntime(getState().runtimes);

    if (currentRuntime &&
        currentRuntime.type === RUNTIMES.USB &&
        !runtimes.find(runtime => currentRuntime.id === runtime.id)) {
      // Since current USB runtime was invalid, move to this firefox page.
      // This case is considered as followings and so on:
      // * Remove ADB addon
      // * (Physically) Disconnect USB runtime
      //
      // The reason why we call selectPage before USB_RUNTIMES_UPDATED was fired is below.
      // Current runtime can not be retrieved after USB_RUNTIMES_UPDATED action, since
      // that updates runtime state. So, before that we fire selectPage action so that to
      // transact unwatchRuntime correctly.

      await dispatch(Actions.selectPage(RUNTIMES.THIS_FIREFOX, RUNTIMES.THIS_FIREFOX));
    }

    // Disconnect runtimes that were no longer valid
    const validIds = runtimes.map(r => r.id);
    const existingRuntimes = getState().runtimes.usbRuntimes;
    const invalidRuntimes = existingRuntimes.filter(r => !validIds.includes(r.id));

    for (const invalidRuntime of invalidRuntimes) {
      await dispatch(disconnectRuntime(invalidRuntime.id));
    }

    dispatch({ type: USB_RUNTIMES_UPDATED, runtimes });

    for (const runtime of getState().runtimes.usbRuntimes) {
      const isConnected = !!runtime.runtimeDetails;
      const hasConnectedClient = remoteClientManager.hasClient(runtime.id, runtime.type);
      if (!isConnected && hasConnectedClient) {
        await dispatch(connectRuntime(runtime.id));
      }
    }
  };
}

/**
 * Remove all the listeners added on client objects. Since those objects are persisted
 * regardless of the about:debugging lifecycle, all the added events should be removed
 * before leaving about:debugging.
 */
function removeRuntimeListeners() {
  return (dispatch, getState) => {
    const { usbRuntimes } = getState().runtimes;
    for (const runtime of usbRuntimes) {
      if (runtime.runtimeDetails) {
        const { clientWrapper } = runtime.runtimeDetails;
        clientWrapper.removeListener("closed", onUSBDebuggerClientClosed);
      }
    }
  };
}

module.exports = {
  connectRuntime,
  disconnectRuntime,
  removeRuntimeListeners,
  unwatchRuntime,
  updateConnectionPromptSetting,
  updateUSBRuntimes,
  watchRuntime,
};
