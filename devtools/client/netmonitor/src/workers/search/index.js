/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */

"use strict";

const { WorkerDispatcher } = require("../worker-utils");

const dispatcher = new WorkerDispatcher();
const start = dispatcher.start.bind(dispatcher);
const stop = dispatcher.stop.bind(dispatcher);

// The search worker support just one task at this point,
// which is searching through specified resource.
const searchInResource = dispatcher.task("searchInResource");

module.exports = {
  start,
  stop,
  searchInResource,
};
