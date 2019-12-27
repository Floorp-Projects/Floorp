/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */

"use strict";

const {
  WorkerDispatcher,
} = require("devtools/client/netmonitor/src/workers/worker-utils");

let startArgs;
let dispatcher;

function getDispatcher() {
  if (!dispatcher) {
    dispatcher = new WorkerDispatcher();
    dispatcher.start(...startArgs);
  }
  return dispatcher;
}

function start(...args) {
  startArgs = args;
}

function stop() {
  if (dispatcher) {
    dispatcher.stop();
    dispatcher = null;
    startArgs = null;
  }
}

// The search worker support just one task at this point,
// which is searching through specified resource.
function searchInResource(...args) {
  return getDispatcher().invoke("searchInResource", ...args);
}

module.exports = {
  start,
  stop,
  searchInResource,
};
