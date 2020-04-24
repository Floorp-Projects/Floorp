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
        id: "r1",
      },
      workers: [
        {
          id: "w1",
          state: Ci.nsIServiceWorkerInfo.STATE_ACTIVATED,
          url: "https://example.com/w1.js",
          workerTargetFront: { foo: "bar" },
          stateText: "activated",
        },
        {
          id: "w2",
          state: Ci.nsIServiceWorkerInfo.STATE_INSTALLED,
          url: "https://example.com/w2.js",
          workerTargetFront: undefined,
          stateText: "installed",
        },
      ],
    },
  ];

  const expectedData = [
    {
      id: "r1",
      lastUpdateTime: 42,
      registrationFront: rawData[0].registration,
      scope: "lorem-ipsum",
      workers: [
        {
          id: "w1",
          isActive: true,
          url: "https://example.com/w1.js",
          workerTargetFront: rawData[0].workers[0].workerTargetFront,
          registrationFront: rawData[0].registration,
          stateText: "activated",
        },
        {
          id: "w2",
          isActive: false,
          url: "https://example.com/w2.js",
          workerTargetFront: undefined,
          registrationFront: rawData[0].registration,
          stateText: "installed",
        },
      ],
    },
  ];

  const action = updateWorkers(rawData);
  const newState = workersReducer(state, action);
  deepEqual(newState.list, expectedData, "workers contains the expected list");
});
