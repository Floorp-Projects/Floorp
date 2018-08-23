/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const {
  CONNECT_RUNTIME_SUCCESS,
  DEBUG_TARGETS,
  DISCONNECT_RUNTIME_SUCCESS,
  REQUEST_EXTENSIONS_SUCCESS,
  REQUEST_TABS_SUCCESS,
  REQUEST_WORKERS_SUCCESS,
  SERVICE_WORKER_FETCH_STATES,
  SERVICE_WORKER_STATUSES,
} = require("../constants");

function RuntimeState() {
  return {
    client: null,
    installedExtensions: [],
    otherWorkers: [],
    serviceWorkers: [],
    sharedWorkers: [],
    tabs: [],
    temporaryExtensions: [],
  };
}

function runtimeReducer(state = RuntimeState(), action) {
  switch (action.type) {
    case CONNECT_RUNTIME_SUCCESS: {
      const { client } = action;
      return Object.assign({}, state, { client });
    }
    case DISCONNECT_RUNTIME_SUCCESS: {
      return RuntimeState();
    }
    case REQUEST_EXTENSIONS_SUCCESS: {
      const { installedExtensions, temporaryExtensions } = action;
      return Object.assign({}, state, { installedExtensions, temporaryExtensions });
    }
    case REQUEST_TABS_SUCCESS: {
      const { tabs } = action;
      return Object.assign({}, state, { tabs });
    }
    case REQUEST_WORKERS_SUCCESS: {
      const { otherWorkers, serviceWorkers, sharedWorkers } = action;
      return Object.assign({}, state, {
        otherWorkers: toWorkerComponentData(otherWorkers),
        serviceWorkers: toWorkerComponentData(serviceWorkers, true),
        sharedWorkers: toWorkerComponentData(sharedWorkers),
      });
    }

    default:
      return state;
  }
}

function getServiceWorkerStatus(isActive, isRunning) {
  if (isActive && isRunning) {
    return SERVICE_WORKER_STATUSES.RUNNING;
  } else if (isActive) {
    return SERVICE_WORKER_STATUSES.STOPPED;
  }
  // We cannot get service worker registrations unless the registration is in
  // ACTIVE state. Unable to know the actual state ("installing", "waiting"), we
  // display a custom state "registering" for now. See Bug 1153292.
  return SERVICE_WORKER_STATUSES.REGISTERING;
}

function toWorkerComponentData(workers, isServiceWorker) {
  return workers.map(worker => {
    const type = DEBUG_TARGETS.WORKER;
    const id = worker.workerTargetActor;
    const icon = "chrome://devtools/skin/images/debugging-workers.svg";
    let { fetch, name, registrationActor, scope } = worker;
    let isActive = false;
    let isRunning = false;
    let status = null;

    if (isServiceWorker) {
      fetch = fetch ? SERVICE_WORKER_FETCH_STATES.LISTENING
                    : SERVICE_WORKER_FETCH_STATES.NOT_LISTENING;
      isActive = worker.active;
      isRunning = !!worker.workerTargetActor;
      status = getServiceWorkerStatus(isActive, isRunning);
    }

    return {
      name,
      icon,
      id,
      type,
      details: {
        fetch,
        isActive,
        isRunning,
        registrationActor,
        scope,
        status,
      },
    };
  });
}

module.exports = {
  RuntimeState,
  runtimeReducer,
};
