/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */
"use strict";

const {
  immutableUpdate,
  reportException,
  assert,
} = require("resource://devtools/shared/DevToolsUtils.js");
const {
  snapshotState: states,
  actions,
} = require("resource://devtools/client/memory/constants.js");
const {
  L10N,
  openFilePicker,
  createSnapshot,
} = require("resource://devtools/client/memory/utils.js");
const {
  selectSnapshot,
  computeSnapshotData,
  readSnapshot,
} = require("resource://devtools/client/memory/actions/snapshot.js");
const VALID_EXPORT_STATES = [states.SAVED, states.READ];

exports.pickFileAndExportSnapshot = function (snapshot) {
  return async function ({ dispatch, getState }) {
    const outputFile = await openFilePicker({
      title: L10N.getFormatStr("snapshot.io.save.window"),
      defaultName: PathUtils.filename(snapshot.path),
      filters: [[L10N.getFormatStr("snapshot.io.filter"), "*.fxsnapshot"]],
      mode: "save",
    });

    if (!outputFile) {
      return;
    }

    await dispatch(exportSnapshot(snapshot, outputFile.path));
  };
};

const exportSnapshot = (exports.exportSnapshot = function (snapshot, dest) {
  return async function ({ dispatch, getState }) {
    dispatch({ type: actions.EXPORT_SNAPSHOT_START, snapshot });

    assert(
      VALID_EXPORT_STATES.includes(snapshot.state),
      `Snapshot is in invalid state for exporting: ${snapshot.state}`
    );

    try {
      await IOUtils.copy(snapshot.path, dest);
    } catch (error) {
      reportException("exportSnapshot", error);
      dispatch({ type: actions.EXPORT_SNAPSHOT_ERROR, snapshot, error });
    }

    dispatch({ type: actions.EXPORT_SNAPSHOT_END, snapshot });
  };
});

exports.pickFileAndImportSnapshotAndCensus = function (heapWorker) {
  return async function ({ dispatch, getState }) {
    const input = await openFilePicker({
      title: L10N.getFormatStr("snapshot.io.import.window"),
      filters: [[L10N.getFormatStr("snapshot.io.filter"), "*.fxsnapshot"]],
      mode: "open",
    });

    if (!input) {
      return;
    }

    await dispatch(importSnapshotAndCensus(heapWorker, input.path));
  };
};

const importSnapshotAndCensus = function (heapWorker, path) {
  return async function ({ dispatch, getState }) {
    const snapshot = immutableUpdate(createSnapshot(getState()), {
      path,
      state: states.IMPORTING,
      imported: true,
    });
    const id = snapshot.id;

    dispatch({ type: actions.IMPORT_SNAPSHOT_START, snapshot });
    dispatch(selectSnapshot(snapshot.id));

    try {
      await dispatch(readSnapshot(heapWorker, id));
      await dispatch(computeSnapshotData(heapWorker, id));
    } catch (error) {
      reportException("importSnapshot", error);
      dispatch({ type: actions.IMPORT_SNAPSHOT_ERROR, error, id });
    }

    dispatch({ type: actions.IMPORT_SNAPSHOT_END, id });
  };
};
exports.importSnapshotAndCensus = importSnapshotAndCensus;
