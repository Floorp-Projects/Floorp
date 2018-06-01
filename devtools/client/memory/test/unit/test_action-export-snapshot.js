/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// Test exporting a snapshot to a user specified location on disk.

const { exportSnapshot } = require("devtools/client/memory/actions/io");
const { takeSnapshotAndCensus } = require("devtools/client/memory/actions/snapshot");
const { actions, treeMapState } = require("devtools/client/memory/constants");

add_task(async function() {
  const front = new StubbedMemoryFront();
  const heapWorker = new HeapAnalysesClient();
  await front.attach();
  const store = Store();
  const { getState, dispatch } = store;

  const destPath = await createTempFile();
  dispatch(takeSnapshotAndCensus(front, heapWorker));
  await waitUntilCensusState(store, snapshot => snapshot.treeMap,
                             [treeMapState.SAVED]);

  const exportEvents = Promise.all([
    waitUntilAction(store, actions.EXPORT_SNAPSHOT_START),
    waitUntilAction(store, actions.EXPORT_SNAPSHOT_END)
  ]);
  dispatch(exportSnapshot(getState().snapshots[0], destPath));
  await exportEvents;

  const stat = await OS.File.stat(destPath);
  info(stat.size);
  ok(stat.size > 0, "destination file is more than 0 bytes");

  heapWorker.destroy();
  await front.detach();
});
