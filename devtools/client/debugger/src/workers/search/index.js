/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */

import { WorkerDispatcher } from "devtools/client/shared/worker-utils";

const WORKER_URL = "resource://devtools/client/debugger/dist/search-worker.js";

export class SearchDispatcher extends WorkerDispatcher {
  constructor(jestUrl) {
    super(jestUrl || WORKER_URL);
  }

  getMatches = this.task("getMatches");

  findSourceMatches = this.task("findSourceMatches");
}
