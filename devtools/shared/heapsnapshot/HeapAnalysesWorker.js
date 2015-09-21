/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
/*global ThreadSafeChromeUtils*/

// This is a worker which reads offline heap snapshots into memory and performs
// heavyweight analyses on them without blocking the main thread. A
// HeapAnalysesWorker is owned and communicated with by a HeapAnalysesClient
// instance. See HeapAnalysesClient.js.

"use strict";

importScripts("resource://gre/modules/devtools/shared/shared/worker-helper.js");

// The set of HeapSnapshot instances this worker has read into memory. Keyed by
// snapshot file path.
const snapshots = Object.create(null);

/**
 * @see HeapAnalysesClient.prototype.readHeapSnapshot
 */
workerHelper.createTask(self, "readHeapSnapshot", ({ snapshotFilePath }) => {
  snapshots[snapshotFilePath] =
    ThreadSafeChromeUtils.readHeapSnapshot(snapshotFilePath);
  return true;
});

/**
 * @see HeapAnalysesClient.prototype.takeCensus
 */
workerHelper.createTask(self, "takeCensus", ({ snapshotFilePath, censusOptions }) => {
  if (!snapshots[snapshotFilePath]) {
    throw new Error(`No known heap snapshot for '${snapshotFilePath}'`);
  }

  return snapshots[snapshotFilePath].takeCensus(censusOptions);
});
