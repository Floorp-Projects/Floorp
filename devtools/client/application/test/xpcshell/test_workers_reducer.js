/* Any copyright is dedicated to the Public Domain.
http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

const { Ci } = require("chrome");

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

  const rawData = [
    {
      registration: {
        scope: "lorem-ipsum",
        lastUpdateTime: 42,
      },
      workers: [
        {
          id: 1,
          state: Ci.nsIServiceWorkerInfo.STATE_ACTIVATED,
          url: "https://example.com",
          workerTargetFront: { foo: "bar" },
          stateText: "activated",
        },
      ],
    },
  ];

  const expectedData = [
    {
      id: 1,
      isActive: true,
      scope: "lorem-ipsum",
      lastUpdateTime: 42,
      url: "https://example.com",
      registrationFront: rawData[0].registration,
      workerTargetFront: rawData[0].workers[0].workerTargetFront,
      stateText: "activated",
    },
  ];

  const action = updateWorkers(rawData);
  const newState = workersReducer(state, action);
  deepEqual(newState.list, expectedData, "workers contains the expected list");
});
