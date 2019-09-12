/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */

// @flow

import { workerUtils } from "devtools-utils";
const { WorkerDispatcher } = workerUtils;

let startArgs;
let dispatcher;

function getDispatcher() {
  if (!dispatcher) {
    dispatcher = new WorkerDispatcher();
    dispatcher.start(...startArgs);
  }

  return dispatcher;
}

export const start = (...args) => {
  startArgs = args;
};

export const stop = () => {
  if (dispatcher) {
    dispatcher.stop();
    dispatcher = null;
    startArgs = null;
  }
};

export const getMatches = (...args) => {
  return getDispatcher().invoke("getMatches", ...args);
};

export const findSourceMatches = (...args) => {
  return getDispatcher().invoke("findSourceMatches", ...args);
};
