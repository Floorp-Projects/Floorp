/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const { DebuggerClient } = require("devtools/shared/client/debugger-client");
const { DebuggerServer } = require("devtools/server/main");

const Actions = require("./index");

const {
  getCurrentClient,
  findRuntimeById,
} = require("devtools/client/aboutdebugging-new/src/modules/runtimes-state-helper");

const {
  CONNECT_RUNTIME_FAILURE,
  CONNECT_RUNTIME_START,
  CONNECT_RUNTIME_SUCCESS,
  DISCONNECT_RUNTIME_FAILURE,
  DISCONNECT_RUNTIME_START,
  DISCONNECT_RUNTIME_SUCCESS,
  RUNTIMES,
  UNWATCH_RUNTIME_FAILURE,
  UNWATCH_RUNTIME_START,
  UNWATCH_RUNTIME_SUCCESS,
  WATCH_RUNTIME_FAILURE,
  WATCH_RUNTIME_START,
  WATCH_RUNTIME_SUCCESS,
} = require("../constants");

async function createLocalClient() {
  DebuggerServer.init();
  DebuggerServer.registerAllActors();
  const client = new DebuggerClient(DebuggerServer.connectPipe());
  await client.connect();
  return client;
}

async function createNetworkClient(host, port) {
  const transport = await DebuggerClient.socketConnect({ host, port });
  const client = new DebuggerClient(transport);
  await client.connect();
  return client;
}

async function createClientForRuntime(id, type) {
  if (type === RUNTIMES.THIS_FIREFOX) {
    return createLocalClient();
  } else if (type === RUNTIMES.NETWORK) {
    const [host, port] = id.split(":");
    return createNetworkClient(host, port);
  }

  return null;
}

function connectRuntime(id) {
  return async (dispatch, getState) => {
    dispatch({ type: CONNECT_RUNTIME_START });
    try {
      const runtime = findRuntimeById(id, getState().runtimes);
      const client = await createClientForRuntime(id, runtime.type);

      dispatch({
        type: CONNECT_RUNTIME_SUCCESS,
        runtime: {
          id,
          client,
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
      const client = runtime.client;

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

function watchRuntime() {
  return async (dispatch, getState) => {
    dispatch({ type: WATCH_RUNTIME_START });

    try {
      // THIS_FIREFOX connects and disconnects on the fly when opening the page.
      // XXX: watchRuntime is only called when opening THIS_FIREFOX page for now.
      await dispatch(connectRuntime(RUNTIMES.THIS_FIREFOX));

      const runtime = findRuntimeById(RUNTIMES.THIS_FIREFOX, getState().runtimes);

      dispatch({ type: WATCH_RUNTIME_SUCCESS, client: runtime.client });
      dispatch(Actions.requestExtensions());
      dispatch(Actions.requestTabs());
      dispatch(Actions.requestWorkers());
    } catch (e) {
      dispatch({ type: WATCH_RUNTIME_FAILURE, error: e.message });
    }
  };
}

function unwatchRuntime() {
  return async (dispatch, getState) => {
    // XXX: unwatchRuntime is only called when opening THIS_FIREFOX page for now.
    const runtime = findRuntimeById(RUNTIMES.THIS_FIREFOX, getState().runtimes);
    const client = runtime.client;

    dispatch({ type: UNWATCH_RUNTIME_START, client });

    try {
      // THIS_FIREFOX connects and disconnects on the fly when opening the page.
      // XXX: unwatchRuntime is only called when closing THIS_FIREFOX page for now.
      await dispatch(disconnectRuntime(RUNTIMES.THIS_FIREFOX));

      dispatch({ type: UNWATCH_RUNTIME_SUCCESS });
    } catch (e) {
      dispatch({ type: UNWATCH_RUNTIME_FAILURE, error: e.message });
    }
  };
}

module.exports = {
  connectRuntime,
  disconnectRuntime,
  unwatchRuntime,
  watchRuntime,
};
