/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const { DebuggerClient } = require("devtools/shared/client/debugger-client");
const { DebuggerServer } = require("devtools/server/main");

const {
  CONNECT_RUNTIME_FAILURE,
  CONNECT_RUNTIME_START,
  CONNECT_RUNTIME_SUCCESS,
  DEBUG_TARGETS,
  DISCONNECT_RUNTIME_FAILURE,
  DISCONNECT_RUNTIME_START,
  DISCONNECT_RUNTIME_SUCCESS,
  REQUEST_TABS_FAILURE,
  REQUEST_TABS_START,
  REQUEST_TABS_SUCCESS,
} = require("../constants");

function connectRuntime() {
  return async (dispatch, getState) => {
    dispatch({ type: CONNECT_RUNTIME_START });

    DebuggerServer.init();
    DebuggerServer.registerAllActors();
    const client = new DebuggerClient(DebuggerServer.connectPipe());

    try {
      await client.connect();

      dispatch({ type: CONNECT_RUNTIME_SUCCESS, client });
      dispatch(requestTabs());
    } catch (e) {
      dispatch({ type: CONNECT_RUNTIME_FAILURE, error: e.message });
    }
  };
}

function disconnectRuntime() {
  return async (dispatch, getState) => {
    dispatch({ type: DISCONNECT_RUNTIME_START });

    const client = getState().runtime.client;

    try {
      await client.close();
      DebuggerServer.destroy();

      dispatch({ type: DISCONNECT_RUNTIME_SUCCESS });
    } catch (e) {
      dispatch({ type: DISCONNECT_RUNTIME_FAILURE, error: e.message });
    }
  };
}

function inspectDebugTarget(type, id) {
  if (type === DEBUG_TARGETS.TAB) {
    window.open(`about:devtools-toolbox?type=tab&id=${ id }`);
  } else {
    console.error(`Failed to inspect the debug target of type: ${ type } id: ${ id }`);
  }

  // We cancel the redux flow here since the inspection does not need to update the state.
  return () => {};
}

function requestTabs() {
  return async (dispatch, getState) => {
    dispatch({ type: REQUEST_TABS_START });

    const client = getState().runtime.client;

    try {
      const { tabs } = await client.listTabs({ favicons: true });

      dispatch({ type: REQUEST_TABS_SUCCESS, tabs });
    } catch (e) {
      dispatch({ type: REQUEST_TABS_FAILURE, error: e.message });
    }
  };
}

module.exports = {
  connectRuntime,
  disconnectRuntime,
  inspectDebugTarget,
  requestTabs,
};
