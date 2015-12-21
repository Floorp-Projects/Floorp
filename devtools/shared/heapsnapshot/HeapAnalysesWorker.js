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
const DominatorTreeNode = require("resource://devtools/shared/heapsnapshot/DominatorTreeNode.js");
const CensusUtils = require("resource://devtools/shared/heapsnapshot/CensusUtils.js");

const DEFAULT_START_INDEX = 0;
const DEFAULT_MAX_COUNT = 50;

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

  const report = snapshots[snapshotFilePath].takeCensus(censusOptions);

  if (requestOptions.asTreeNode || requestOptions.asInvertedTreeNode) {
    const opts = { filter: requestOptions.filter || null };
    if (requestOptions.asInvertedTreeNode) {
      opts.invert = true;
    }
    return censusReportToCensusTreeNode(censusOptions.breakdown, report, opts);
  }

  return report;
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

  if (requestOptions.asTreeNode || requestOptions.asInvertedTreeNode) {
    const opts = { filter: requestOptions.filter || null };
    if (requestOptions.asInvertedTreeNode) {
      opts.invert = true;
    }
    return censusReportToCensusTreeNode(censusOptions.breakdown, delta, opts);
  }

  return delta;
});

/**
 * @see HeapAnalysesClient.prototype.getCreationTime
 */
workerHelper.createTask(self, "getCreationTime", snapshotFilePath => {
  let snapshot = snapshots[snapshotFilePath];
  return snapshot ? snapshot.creationTime : null;
});

/**
 * The set of `DominatorTree`s that have been computed, mapped by their id (aka
 * the index into this array).
 *
 * @see /dom/webidl/DominatorTree.webidl
 */
const dominatorTrees = [];

/**
 * The i^th HeapSnapshot in this array is the snapshot used to generate the i^th
 * dominator tree in `dominatorTrees` above. This lets us map from a dominator
 * tree id to the snapshot it came from.
 */
const dominatorTreeSnapshots = [];

/**
 * @see HeapAnalysesClient.prototype.computeDominatorTree
 */
workerHelper.createTask(self, "computeDominatorTree", snapshotFilePath => {
  const snapshot = snapshots[snapshotFilePath];
  if (!snapshot) {
    throw new Error(`No known heap snapshot for '${snapshotFilePath}'`);
  }

  const id = dominatorTrees.length;
  dominatorTrees.push(snapshot.computeDominatorTree());
  dominatorTreeSnapshots.push(snapshot);
  return id;
});

/**
 * @see HeapAnalysesClient.prototype.getDominatorTree
 */
workerHelper.createTask(self, "getDominatorTree", request => {
  const {
    dominatorTreeId,
    breakdown,
    maxDepth,
    maxSiblings
  } = request;

  if (!(0 <= dominatorTreeId && dominatorTreeId < dominatorTrees.length)) {
    throw new Error(
      `There does not exist a DominatorTree with the id ${dominatorTreeId}`);
  }

  const dominatorTree = dominatorTrees[dominatorTreeId];
  const snapshot = dominatorTreeSnapshots[dominatorTreeId];

  return DominatorTreeNode.partialTraversal(dominatorTree,
                                            snapshot,
                                            breakdown,
                                            maxDepth,
                                            maxSiblings);
});

/**
 * @see HeapAnalysesClient.prototype.getImmediatelyDominated
 */
workerHelper.createTask(self, "getImmediatelyDominated", request => {
  const {
    dominatorTreeId,
    nodeId,
    breakdown,
    startIndex,
    maxCount
  } = request;

  if (!(0 <= dominatorTreeId && dominatorTreeId < dominatorTrees.length)) {
    throw new Error(
      `There does not exist a DominatorTree with the id ${dominatorTreeId}`);
  }

  const dominatorTree = dominatorTrees[dominatorTreeId];
  const snapshot = dominatorTreeSnapshots[dominatorTreeId];

  const childIds = dominatorTree.getImmediatelyDominated(nodeId);
  if (!childIds) {
    throw new Error(`${nodeId} is not a node id in the dominator tree`);
  }

  const start = startIndex || DEFAULT_START_INDEX;
  const count = maxCount || DEFAULT_MAX_COUNT;
  const end = start + count;

  const nodes = childIds
    .slice(start, end)
    .map(id => {
      const { label, shallowSize } =
        DominatorTreeNode.getLabelAndShallowSize(id, snapshot, breakdown);
      const retainedSize = dominatorTree.getRetainedSize(id);
      const node = new DominatorTreeNode(id, label, shallowSize, retainedSize);
      node.parentId = nodeId;
      // DominatorTree.getImmediatelyDominated will always return non-null here
      // because we got the id directly from the dominator tree.
      node.moreChildrenAvailable = dominatorTree.getImmediatelyDominated(id).length > 0;
      return node;
    });

  const moreChildrenAvailable = childIds.length > end;

  return { nodes, moreChildrenAvailable };
});
