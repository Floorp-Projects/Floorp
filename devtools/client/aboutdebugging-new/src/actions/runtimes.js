/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const { ADB } = require("devtools/shared/adb/adb");
const { DebuggerClient } = require("devtools/shared/client/debugger-client");
const { DebuggerServer } = require("devtools/server/main");

const Actions = require("./index");

const {
  getCurrentRuntime,
  findRuntimeById,
} = require("../modules/runtimes-state-helper");
const { isSupportedDebugTarget } = require("../modules/debug-target-support");

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

async function createLocalClient() {
  DebuggerServer.init();
  DebuggerServer.registerAllActors();
  const client = new DebuggerClient(DebuggerServer.connectPipe());
  await client.connect();
  return { client };
}

async function createNetworkClient(host, port) {
  const transportDetails = { host, port };
  const transport = await DebuggerClient.socketConnect(transportDetails);
  const client = new DebuggerClient(transport);
  await client.connect();
  return { client, transportDetails };
}

async function createUSBClient(socketPath) {
  const port = await ADB.prepareTCPConnection(socketPath);
  return createNetworkClient("localhost", port);
}

async function createClientForRuntime(runtime) {
  const { extra, type } = runtime;

  if (type === RUNTIMES.THIS_FIREFOX) {
    return createLocalClient();
  } else if (type === RUNTIMES.NETWORK) {
    const { host, port } = extra.connectionParameters;
    return createNetworkClient(host, port);
  } else if (type === RUNTIMES.USB) {
    const { socketPath } = extra.connectionParameters;
    return createUSBClient(socketPath);
  }

  return null;
}

async function getRuntimeInfo(runtime, client) {
  const { extra, type } = runtime;
  const deviceFront = await client.mainRoot.getFront("device");
  const { brandName: name, channel, version } = await deviceFront.getDescription();
  const icon =
    (channel === "release" || channel === "beta" || channel === "aurora")
      ? `chrome://devtools/skin/images/aboutdebugging-firefox-${ channel }.svg`
      : "chrome://devtools/skin/images/aboutdebugging-firefox-nightly.svg";

  return {
    icon,
    deviceName: extra ? extra.deviceName : undefined,
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
      const { client, transportDetails } = await createClientForRuntime(runtime);
      const info = await getRuntimeInfo(runtime, client);
      const preferenceFront = await client.mainRoot.getFront("preference");
      const connectionPromptEnabled =
        await preferenceFront.getBoolPref(RUNTIME_PREFERENCE.CONNECTION_PROMPT);
      const runtimeDetails = { connectionPromptEnabled, client, info, transportDetails };

      if (runtime.type === RUNTIMES.USB) {
        // `closed` event will be emitted when disabling remote debugging
        // on the connected USB runtime.
        client.addOneTimeListener("closed", onUSBDebuggerClientClosed);
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
      const client = runtime.runtimeDetails.client;

      if (runtime.type === RUNTIMES.USB) {
        client.removeListener("closed", onUSBDebuggerClientClosed);
      }

      await client.close();

      if (runtime.type === RUNTIMES.THIS_FIREFOX) {
        DebuggerServer.destroy();
      }

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
      const client = runtime.runtimeDetails.client;
      const preferenceFront = await client.mainRoot.getFront("preference");
      await preferenceFront.setBoolPref(RUNTIME_PREFERENCE.CONNECTION_PROMPT,
                                        connectionPromptEnabled);
      // Re-get actual value from the runtime.
      connectionPromptEnabled =
        await preferenceFront.getBoolPref(RUNTIME_PREFERENCE.CONNECTION_PROMPT);

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
    const invalidRuntimes =
      getState().runtimes.usbRuntimes.filter(r => !runtimes.includes(r));
    for (const invalidRuntime of invalidRuntimes) {
      await dispatch(disconnectRuntime(invalidRuntime.id));
    }

    dispatch({ type: USB_RUNTIMES_UPDATED, runtimes });
  };
}

module.exports = {
  connectRuntime,
  disconnectRuntime,
  unwatchRuntime,
  updateConnectionPromptSetting,
  updateUSBRuntimes,
  watchRuntime,
};
