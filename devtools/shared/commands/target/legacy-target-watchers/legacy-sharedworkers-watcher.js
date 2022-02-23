/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const {
  LegacyWorkersWatcher,
} = require("devtools/shared/commands/target/legacy-target-watchers/legacy-workers-watcher");

class LegacySharedWorkersWatcher extends LegacyWorkersWatcher {
  // Flag used from the parent class to listen to process targets.
  // Decision tree is complicated, keep all logic in the parent methods.
  _isSharedWorkerWatcher = true;

  _supportWorkerTarget(workerTarget) {
    return workerTarget.isSharedWorker;
  }
}

module.exports = { LegacySharedWorkersWatcher };
