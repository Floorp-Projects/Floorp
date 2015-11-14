/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

const { assert } = require("devtools/shared/DevToolsUtils");
const { MemoryFront } = require("devtools/server/actors/memory");
const HeapAnalysesClient = require("devtools/shared/heapsnapshot/HeapAnalysesClient");
const { PropTypes } = require("devtools/client/shared/vendor/react");
const { snapshotState: states, diffingState } = require("./constants");

/**
 * The breakdown object DSL describing how we want
 * the census data to be.
 * @see `js/src/doc/Debugger/Debugger.Memory.md`
 */
let breakdownModel = exports.breakdown = PropTypes.shape({
  by: PropTypes.oneOf(["coarseType", "allocationStack", "objectClass", "internalType"]).isRequired,
});

let censusModel = exports.censusModel = PropTypes.shape({
  // The current census report data.
  report: PropTypes.object,
  // The breakdown used to generate the current census
  breakdown: breakdownModel,
  // Whether the currently cached report tree is inverted or not.
  inverted: PropTypes.bool,
  // If present, the currently cached report's filter string used for pruning
  // the tree items.
  filter: PropTypes.string,
});

/**
 * Snapshot model.
 */
let stateKeys = Object.keys(states).map(state => states[state]);
const snapshotId = PropTypes.number;
let snapshotModel = exports.snapshot = PropTypes.shape({
  // Unique ID for a snapshot
  id: snapshotId.isRequired,
  // Whether or not this snapshot is currently selected.
  selected: PropTypes.bool.isRequired,
  // Filesystem path to where the snapshot is stored; used to identify the
  // snapshot for HeapAnalysesClient.
  path: PropTypes.string,
  // Current census data for this snapshot.
  census: censusModel,
  // If an error was thrown while processing this snapshot, the `Error` instance
  // is attached here.
  error: PropTypes.object,
  // Boolean indicating whether or not this snapshot was imported.
  imported: PropTypes.bool.isRequired,
  // The creation time of the snapshot; required after the snapshot has been
  // read.
  creationTime: PropTypes.number,
  // The current state the snapshot is in.
  // @see ./constants.js
  state: function (snapshot, propName) {
    let current = snapshot.state;
    let shouldHavePath = [states.IMPORTING, states.SAVED, states.READ, states.SAVING_CENSUS, states.SAVED_CENSUS];
    let shouldHaveCreationTime = [states.READ, states.SAVING_CENSUS, states.SAVED_CENSUS];
    let shouldHaveCensus = [states.SAVED_CENSUS];

    if (!stateKeys.includes(current)) {
      throw new Error(`Snapshot state must be one of ${stateKeys}.`);
    }
    if (shouldHavePath.includes(current) && !snapshot.path) {
      throw new Error(`Snapshots in state ${current} must have a snapshot path.`);
    }
    if (shouldHaveCensus.includes(current) && (!snapshot.census || !snapshot.census.breakdown)) {
      throw new Error(`Snapshots in state ${current} must have a census and breakdown.`);
    }
    if (shouldHaveCreationTime.includes(current) && !snapshot.creationTime) {
      throw new Error(`Snapshots in state ${current} must have a creation time.`);
    }
  },
});

let allocationsModel = exports.allocations = PropTypes.shape({
  // True iff we are recording allocation stacks right now.
  recording: PropTypes.bool.isRequired,
  // True iff we are in the process of toggling the recording of allocation
  // stacks on or off right now.
  togglingInProgress: PropTypes.bool.isRequired,
});

let diffingModel = exports.diffingModel = PropTypes.shape({
  // The id of the first snapshot to diff.
  firstSnapshotId: snapshotId,

  // The id of the second snapshot to diff.
  secondSnapshotId: function (diffing, propName) {
    if (diffing.secondSnapshotId && !diffing.firstSnapshotId) {
      throw new Error("Cannot have second snapshot without already having " +
                      "first snapshot");
    }
    return snapshotId(diffing, propName);
  },

  // The current census data for the diffing.
  census: censusModel,

  // If an error was thrown while diffing, the `Error` instance is attached
  // here.
  error: PropTypes.object,

  // The current state the diffing is in.
  // @see ./constants.js
  state: function (diffing) {
    switch (diffing.state) {
      case diffingState.TOOK_DIFF:
        assert(diffing.census, "If we took a diff, we should have a census");
        // Fall through...
      case diffingState.TAKING_DIFF:
        assert(diffing.firstSnapshotId, "Should have first snapshot");
        assert(diffing.secondSnapshotId, "Should have second snapshot");
        break;

      case diffingState.SELECTING:
        break;

      case diffingState.ERROR:
        assert(diffing.error, "Should have error");
        break;

      default:
        assert(false, `Bad diffing state: ${diffing.state}`);
    }
  }
});

let appModel = exports.app = {
  // {MemoryFront} Used to communicate with platform
  front: PropTypes.instanceOf(MemoryFront),
  // Allocations recording related data.
  allocations: allocationsModel.isRequired,
  // {HeapAnalysesClient} Used to interface with snapshots
  heapWorker: PropTypes.instanceOf(HeapAnalysesClient),
  // The breakdown object DSL describing how we want
  // the census data to be.
  // @see `js/src/doc/Debugger/Debugger.Memory.md`
  breakdown: breakdownModel.isRequired,
  // List of reference to all snapshots taken
  snapshots: PropTypes.arrayOf(snapshotModel).isRequired,
  // True iff we want the tree displayed inverted.
  inverted: PropTypes.bool.isRequired,
  // If present, a filter string for pruning the tree items.
  filter: PropTypes.string,
  // If present, the current diffing state.
  diffing: diffingModel,
};
