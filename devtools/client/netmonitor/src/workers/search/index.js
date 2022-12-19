/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */

"use strict";

const {
  WorkerDispatcher,
} = require("resource://devtools/client/shared/worker-utils.js");

const SEARCH_WORKER_URL =
  "resource://devtools/client/netmonitor/src/workers/search/worker.js";

class SearchDispatcher extends WorkerDispatcher {
  constructor() {
    super(SEARCH_WORKER_URL);
  }

  // The search worker support just one task at this point,
  // which is searching through specified resource.
  searchInResource = this.task("searchInResource");
}

// Compared to debugger, we instantiate a singleton within the dispatcher module
module.exports = new SearchDispatcher();
