/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
/*global ThreadSafeChromeUtils*/

// This is a worker which reads offline heap snapshots into memory and performs
// heavyweight analyses on them without blocking the main thread. A
// HeapAnalysesWorker is owned and communicated with by a HeapAnalysesClient
// instance. See HeapAnalysesClient.js.

"use strict";

importScripts("resource://gre/modules/workers/require.js");
importScripts("resource://devtools/shared/worker/helper.js");
const { censusReportToCensusTreeNode } = require("resource://devtools/shared/heapsnapshot/census-tree-node.js");
const CensusUtils = require("resource://devtools/shared/heapsnapshot/CensusUtils.js");

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
workerHelper.createTask(self, "takeCensus", ({ snapshotFilePath, censusOptions, requestOptions }) => {
  if (!snapshots[snapshotFilePath]) {
    throw new Error(`No known heap snapshot for '${snapshotFilePath}'`);
  }

  let report = snapshots[snapshotFilePath].takeCensus(censusOptions);

  if (requestOptions.asTreeNode) {
    return censusReportToCensusTreeNode(censusOptions.breakdown, report);
  } else if (requestOptions.asInvertedTreeNode) {
    return censusReportToCensusTreeNode(censusOptions.breakdown, report, { invert: true });
  } else {
    return report;
  }
});

/**
 * @see HeapAnalysesClient.prototype.takeCensusDiff
 */
workerHelper.createTask(self, "takeCensusDiff", request => {
  const {
    firstSnapshotFilePath,
    secondSnapshotFilePath,
    censusOptions,
    requestOptions
  } = request;

  if (!snapshots[firstSnapshotFilePath]) {
    throw new Error(`No known heap snapshot for '${firstSnapshotFilePath}'`);
  }

  if (!snapshots[secondSnapshotFilePath]) {
    throw new Error(`No known heap snapshot for '${secondSnapshotFilePath}'`);
  }

  const first = snapshots[firstSnapshotFilePath].takeCensus(censusOptions);
  const second = snapshots[secondSnapshotFilePath].takeCensus(censusOptions);
  const delta = CensusUtils.diff(censusOptions.breakdown, first, second);

  if (requestOptions.asTreeNode) {
    return censusReportToCensusTreeNode(censusOptions.breakdown, delta);
  } else if (requestOptions.asInvertedTreeNode) {
    return censusReportToCensusTreeNode(censusOptions.breakdown, delta, { invert: true });
  } else {
    return delta;
  }
});

/**
 * @see HeapAnalysesClient.prototype.getCreationTime
 */
workerHelper.createTask(self, "getCreationTime", (snapshotFilePath) => {
  let snapshot = snapshots[snapshotFilePath];
  return snapshot ? snapshot.creationTime : null;
});
