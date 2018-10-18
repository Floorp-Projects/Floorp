/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const { ADB } = require("devtools/shared/adb/adb");
const { DebuggerClient } = require("devtools/shared/client/debugger-client");
const { DebuggerServer } = require("devtools/server/main");

const Actions = require("./index");

const {
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
  RUNTIMES,
  UNWATCH_RUNTIME_FAILURE,
  UNWATCH_RUNTIME_START,
  UNWATCH_RUNTIME_SUCCESS,
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

function connectRuntime(id) {
  return async (dispatch, getState) => {
    dispatch({ type: CONNECT_RUNTIME_START });
    try {
      const runtime = findRuntimeById(id, getState().runtimes);
      const { client, transportDetails } = await createClientForRuntime(runtime);
      const info = await getRuntimeInfo(runtime, client);
      const connection = { client, info, transportDetails };

      dispatch({
        type: CONNECT_RUNTIME_SUCCESS,
        runtime: {
          id,
          connection,
          type: runtime.type,
        }
      });
    } catch (e) {
      dispatch({ type: CONNECT_RUNTIME_FAILURE, error: e.message });
    }
  };
}

function disconnectRuntime(id) {
  return async (dispatch, getState) => {
    dispatch({ type: DISCONNECT_RUNTIME_START });
    try {
      const runtime = findRuntimeById(id, getState().runtimes);
      const client = runtime.connection.client;

      await client.close();
      DebuggerServer.destroy();

      dispatch({
        type: DISCONNECT_RUNTIME_SUCCESS,
        runtime: {
          id,
          type: runtime.type,
        }
      });
    } catch (e) {
      dispatch({ type: DISCONNECT_RUNTIME_FAILURE, error: e.message });
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
      dispatch({ type: WATCH_RUNTIME_FAILURE, error: e.message });
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
      dispatch({ type: UNWATCH_RUNTIME_FAILURE, error: e.message });
    }
  };
}

function updateUSBRuntimes(runtimes) {
  return { type: USB_RUNTIMES_UPDATED, runtimes };
}

module.exports = {
  connectRuntime,
  disconnectRuntime,
  unwatchRuntime,
  updateUSBRuntimes,
  watchRuntime,
};
