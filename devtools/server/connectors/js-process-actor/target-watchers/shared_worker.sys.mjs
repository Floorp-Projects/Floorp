/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

import { WorkerTargetWatcherClass } from "resource://devtools/server/connectors/js-process-actor/target-watchers/worker.sys.mjs";

class SharedWorkerTargetWatcherClass extends WorkerTargetWatcherClass {
  constructor() {
    super("shared_worker");
  }
}

export const SharedWorkerTargetWatcher = new SharedWorkerTargetWatcherClass();
