/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */

import { WorkerDispatcher } from "devtools/client/shared/worker-utils";

const WORKER_URL = "resource://devtools/client/debugger/dist/search-worker.js";

let dispatcher;
let jestWorkerUrl;

function getDispatcher() {
  if (!dispatcher) {
    dispatcher = new WorkerDispatcher();
    dispatcher.start(jestWorkerUrl || WORKER_URL);
  }

  return dispatcher;
}

export const start = jestUrl => {
  jestWorkerUrl = jestUrl;
};

export const stop = () => {
  if (dispatcher) {
    dispatcher.stop();
    dispatcher = null;
  }
};

export const getMatches = (...args) => {
  return getDispatcher().invoke("getMatches", ...args);
};

export const findSourceMatches = (...args) => {
  return getDispatcher().invoke("findSourceMatches", ...args);
};
