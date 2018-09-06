/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const { DebuggerClient } = require("devtools/shared/client/debugger-client");
const { DebuggerServer } = require("devtools/server/main");

const Actions = require("./index");

const {
  getCurrentClient
} = require("devtools/client/aboutdebugging-new/src/modules/runtimes-state-helper");

const {
  UNWATCH_RUNTIME_FAILURE,
  UNWATCH_RUNTIME_START,
  UNWATCH_RUNTIME_SUCCESS,
  WATCH_RUNTIME_FAILURE,
  WATCH_RUNTIME_START,
  WATCH_RUNTIME_SUCCESS,
} = require("../constants");

function watchRuntime() {
  return async (dispatch, getState) => {
    dispatch({ type: WATCH_RUNTIME_START });

    DebuggerServer.init();
    DebuggerServer.registerAllActors();
    const client = new DebuggerClient(DebuggerServer.connectPipe());

    try {
      await client.connect();

      dispatch({ type: WATCH_RUNTIME_SUCCESS, client });
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
    const client = getCurrentClient(getState());

    dispatch({ type: UNWATCH_RUNTIME_START, client });

    try {
      await client.close();
      DebuggerServer.destroy();

      dispatch({ type: UNWATCH_RUNTIME_SUCCESS });
    } catch (e) {
      dispatch({ type: UNWATCH_RUNTIME_FAILURE, error: e.message });
    }
  };
}

module.exports = {
  watchRuntime,
  unwatchRuntime,
};
