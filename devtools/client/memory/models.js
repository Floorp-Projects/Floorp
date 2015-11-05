/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

const { MemoryFront } = require("devtools/server/actors/memory");
const HeapAnalysesClient = require("devtools/shared/heapsnapshot/HeapAnalysesClient");
const { PropTypes } = require("devtools/client/shared/vendor/react");
const { snapshotState: states } = require("./constants");

/**
 * The breakdown object DSL describing how we want
 * the census data to be.
 * @see `js/src/doc/Debugger/Debugger.Memory.md`
 */
let breakdownModel = exports.breakdown = PropTypes.shape({
  by: PropTypes.oneOf(["coarseType", "allocationStack", "objectClass", "internalType"]).isRequired,
});

/**
 * Snapshot model.
 */
let stateKeys = Object.keys(states).map(state => states[state]);
let snapshotModel = exports.snapshot = PropTypes.shape({
  // Unique ID for a snapshot
  id: PropTypes.number.isRequired,
  // Whether or not this snapshot is currently selected.
  selected: PropTypes.bool.isRequired,
  // Filesystem path to where the snapshot is stored; used to identify the
  // snapshot for HeapAnalysesClient.
  path: PropTypes.string,
  // The current census report data.
  census: PropTypes.object,
  // The breakdown used to generate the current census
  breakdown: breakdownModel,
  // Whether the currently cached census tree is inverted or not.
  inverted: PropTypes.bool,
  // If present, the currently cached census's filter string used for pruning
  // the tree items.
  filter: PropTypes.string,
  // If an error was thrown while processing this snapshot, the `Error` instance is attached here.
  error: PropTypes.object,
  // The creation time of the snapshot; required after the snapshot has been read.
  creationTime: PropTypes.number,
  // The current state the snapshot is in.
  // @see ./constants.js
  state: function (snapshot, propName) {
    let current = snapshot.state;
    let shouldHavePath = [states.SAVED, states.READ, states.SAVING_CENSUS, states.SAVED_CENSUS];
    let shouldHaveCreationTime = [states.READ, states.SAVING_CENSUS, states.SAVED_CENSUS];
    let shouldHaveCensus = [states.SAVED_CENSUS];

    if (!stateKeys.includes(current)) {
      throw new Error(`Snapshot state must be one of ${stateKeys}.`);
    }
    if (shouldHavePath.includes(current) && !snapshot.path) {
      throw new Error(`Snapshots in state ${current} must have a snapshot path.`);
    }
    if (shouldHaveCensus.includes(current) && (!snapshot.census || !snapshot.breakdown)) {
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
};
