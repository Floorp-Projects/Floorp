/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */

"use strict";

const {
  WorkerDispatcher,
} = require("resource://devtools/client/shared/worker-utils.js");

const SEARCH_WORKER_URL =
  "resource://devtools/client/netmonitor/src/workers/search/worker.js";

let dispatcher;

function getDispatcher() {
  if (!dispatcher) {
    dispatcher = new WorkerDispatcher();
    dispatcher.start(SEARCH_WORKER_URL);
  }
  return dispatcher;
}

function stop() {
  if (dispatcher) {
    dispatcher.stop();
    dispatcher = null;
  }
}

// The search worker support just one task at this point,
// which is searching through specified resource.
function searchInResource(...args) {
  return getDispatcher().invoke("searchInResource", ...args);
}

module.exports = {
  stop,
  searchInResource,
};
