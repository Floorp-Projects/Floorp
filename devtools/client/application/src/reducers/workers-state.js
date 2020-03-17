/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const { Ci } = require("chrome");

const {
  UPDATE_CAN_DEBUG_WORKERS,
  UPDATE_WORKERS,
} = require("devtools/client/application/src/constants");

function WorkersState() {
  return {
    // Array of all service workers
    list: [],
    canDebugWorkers: false,
  };
}

function buildWorkerDataFromFronts({ registration, workers }) {
  return workers.map(worker => ({
    id: worker.id,
    isActive: worker.state === Ci.nsIServiceWorkerInfo.STATE_ACTIVATED,
    scope: registration.scope,
    lastUpdateTime: registration.lastUpdateTime, // only available for active worker
    url: worker.url,
    registrationFront: registration,
    workerTargetFront: worker.workerTargetFront,
    stateText: worker.stateText,
  }));
}

function workersReducer(state = WorkersState(), action) {
  switch (action.type) {
    case UPDATE_CAN_DEBUG_WORKERS: {
      return Object.assign({}, state, {
        canDebugWorkers: action.canDebugWorkers,
      });
    }
    case UPDATE_WORKERS: {
      const { workers } = action;
      return Object.assign({}, state, {
        list: workers.map(buildWorkerDataFromFronts).flat(),
      });
    }
    default:
      return state;
  }
}

module.exports = {
  WorkersState,
  workersReducer,
};
