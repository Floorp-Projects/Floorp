const DevToolsUtils = require("devtools/shared/DevToolsUtils");
const { snapshotState: states } = require("./constants");
const SAVING_SNAPSHOT_TEXT = "Saving snapshot...";
const READING_SNAPSHOT_TEXT = "Reading snapshot...";
const SAVING_CENSUS_TEXT = "Taking heap census...";

// @TODO 1215606
// Use DevToolsUtils.assert when fixed.
exports.assert = function (condition, message) {
  if (!condition) {
    const err = new Error("Assertion failure: " + message);
    DevToolsUtils.reportException("DevToolsUtils.assert", err);
    throw err;
  }
};

/**
 * Returns a string representing a readable form of the snapshot's state.
 *
 * @param {Snapshot} snapshot
 * @return {String}
 */
exports.getSnapshotStatusText = function (snapshot) {
  switch (snapshot && snapshot.state) {
    case states.SAVING:
      return SAVING_SNAPSHOT_TEXT;
    case states.SAVED:
    case states.READING:
      return READING_SNAPSHOT_TEXT;
    case states.READ:
    case states.SAVING_CENSUS:
      return SAVING_CENSUS_TEXT;
  }
  return "";
}

/**
 * Takes an array of snapshots and a snapshot and returns
 * the snapshot instance in `snapshots` that matches
 * the snapshot passed in.
 *
 * @param {Array<Snapshot>} snapshots
 * @param {Snapshot}
 * @return ?Snapshot
 */
exports.getSnapshot = function getSnapshot (snapshots, snapshot) {
  let found = snapshots.find(s => s.id === snapshot.id);
  if (!found) {
    DevToolsUtils.reportException(`No matching snapshot found for ${snapshot.id}`);
  }
  return found || null;
};

/**
 * Creates a new snapshot object.
 *
 * @return {Snapshot}
 */
let INC_ID = 0;
exports.createSnapshot = function createSnapshot () {
  let id = ++INC_ID;
  return {
    id,
    state: states.SAVING,
    census: null,
    path: null,
  };
};
