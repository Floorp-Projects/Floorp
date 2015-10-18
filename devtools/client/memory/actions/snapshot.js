/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */
"use strict";

// @TODO 1215606
// Use this assert instead of utils when fixed.
// const { assert } = require("devtools/shared/DevToolsUtils");
const { createSnapshot, assert } = require("../utils");
const { actions, snapshotState: states } = require("../constants");

/**
 * A series of actions are fired from this task to save, read and generate the initial
 * census from a snapshot.
 *
 * @param {MemoryFront}
 * @param {HeapAnalysesClient}
 * @param {Object}
 */
const takeSnapshotAndCensus = exports.takeSnapshotAndCensus = function takeSnapshotAndCensus (front, heapWorker) {
  return function *(dispatch, getStore) {
    let snapshot = yield dispatch(takeSnapshot(front));
    yield dispatch(readSnapshot(heapWorker, snapshot));
    yield dispatch(takeCensus(heapWorker, snapshot));
  };
};

/**
 * @param {MemoryFront}
 */
const takeSnapshot = exports.takeSnapshot = function takeSnapshot (front) {
  return function *(dispatch, getStore) {
    let snapshot = createSnapshot();
    dispatch({ type: actions.TAKE_SNAPSHOT_START, snapshot });
    dispatch(selectSnapshot(snapshot));

    let path = yield front.saveHeapSnapshot();
    dispatch({ type: actions.TAKE_SNAPSHOT_END, snapshot, path });

    return snapshot;
  };
};

/**
 * Reads a snapshot into memory; necessary to do before taking
 * a census on the snapshot. May only be called once per snapshot.
 *
 * @param {HeapAnalysesClient}
 * @param {Snapshot} snapshot,
 */
const readSnapshot = exports.readSnapshot = function readSnapshot (heapWorker, snapshot) {
  return function *(dispatch, getStore) {
    // @TODO 1215606
    assert(snapshot.state === states.SAVED,
      "Should only read a snapshot once");

    dispatch({ type: actions.READ_SNAPSHOT_START, snapshot });
    yield heapWorker.readHeapSnapshot(snapshot.path);
    dispatch({ type: actions.READ_SNAPSHOT_END, snapshot });
  };
};

/**
 * @param {HeapAnalysesClient} heapWorker
 * @param {Snapshot} snapshot,
 *
 * @see {Snapshot} model defined in devtools/client/memory/app.js
 * @see `devtools/shared/heapsnapshot/HeapAnalysesClient.js`
 * @see `js/src/doc/Debugger/Debugger.Memory.md` for breakdown details
 */
const takeCensus = exports.takeCensus = function takeCensus (heapWorker, snapshot) {
  return function *(dispatch, getStore) {
    // @TODO 1215606
    assert([states.READ, states.SAVED_CENSUS].includes(snapshot.state),
      "Can only take census of snapshots in READ or SAVED_CENSUS state");

    let breakdown = getStore().breakdown;
    dispatch({ type: actions.TAKE_CENSUS_START, snapshot, breakdown });

    let census = yield heapWorker.takeCensus(snapshot.path, { breakdown }, { asTreeNode: true });
    dispatch({ type: actions.TAKE_CENSUS_END, snapshot, census });
  };
};

/**
 * @param {Snapshot}
 * @see {Snapshot} model defined in devtools/client/memory/app.js
 */
const selectSnapshot = exports.selectSnapshot = function takeSnapshot (snapshot) {
  return {
    type: actions.SELECT_SNAPSHOT,
    snapshot
  };
};

