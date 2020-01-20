/* Any copyright is dedicated to the Public Domain.
http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

const {
  updateCanDebugWorkers,
  updateWorkers,
} = require("devtools/client/application/src/actions/workers.js");

const {
  workersReducer,
  WorkersState,
} = require("devtools/client/application/src/reducers/workers-state.js");

add_task(async function() {
  info("Test workers reducer: UPDATE_CAN_DEBUG_WORKERS action");

  function testUpdateCanDebugWorkers(flagValue) {
    const state = WorkersState();
    const action = updateCanDebugWorkers(flagValue);
    const newState = workersReducer(state, action);
    equal(
      newState.canDebugWorkers,
      flagValue,
      "canDebugWorkers contains the expected value"
    );
  }

  testUpdateCanDebugWorkers(false);
  testUpdateCanDebugWorkers(true);
});

add_task(async function() {
  info("Test workers reducer: UPDATE_WORKERS action");

  const state = WorkersState();
  const action = updateWorkers([{ foo: "bar" }, { lorem: "ipsum" }]);
  const newState = workersReducer(state, action);
  deepEqual(
    newState.list,
    [{ foo: "bar" }, { lorem: "ipsum" }],
    "workers contains the expected list"
  );
});
