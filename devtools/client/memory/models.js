/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

const { assert } = require("devtools/shared/DevToolsUtils");
const { MemoryFront } = require("devtools/server/actors/memory");
const HeapAnalysesClient = require("devtools/shared/heapsnapshot/HeapAnalysesClient");
const { PropTypes } = require("devtools/client/shared/vendor/react");
const {
  snapshotState: states,
  diffingState,
  dominatorTreeState,
  viewState
} = require("./constants");

/**
 * ONLY USE THIS FOR MODEL VALIDATORS IN CONJUCTION WITH assert()!
 *
 * React checks that the returned values from validator functions are instances
 * of Error, but because React is loaded in its own global, that check is always
 * false and always results in a warning.
 *
 * To work around this and still get model validation, just call assert() inside
 * a function passed to catchAndIgnore. The assert() function will still report
 * assertion failures, but this funciton will swallow the errors so that React
 * doesn't go crazy and drown out the real error in irrelevant and incorrect
 * warnings.
 *
 * Example usage:
 *
 *     const MyModel = PropTypes.shape({
 *       someProperty: catchAndIgnore(function (model) {
 *         assert(someInvariant(model.someProperty), "Should blah blah");
 *       })
 *     });
 */
function catchAndIgnore(fn) {
  return function (...args) {
    try {
      fn(...args);
    } catch (err) { }

    return null;
  };
}

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
  // The parent map for the report.
  parentMap: PropTypes.object,
  // The breakdown used to generate the current census
  breakdown: breakdownModel,
  // Whether the currently cached report tree is inverted or not.
  inverted: PropTypes.bool,
  // If present, the currently cached report's filter string used for pruning
  // the tree items.
  filter: PropTypes.string,
  // The Set<CensusTreeNode.id> of expanded node ids in the report tree.
  expanded: catchAndIgnore(function (census) {
    if (census.report) {
      assert(census.expanded,
             "If we have a report, we should also have the set of expanded nodes");
    }
  }),
  // If a node is currently focused in the report tree, then this is it.
  focused: PropTypes.object,
});

/**
 * Dominator tree model.
 */
let dominatorTreeModel = exports.dominatorTreeModel = PropTypes.shape({
  // The id of this dominator tree.
  dominatorTreeId: PropTypes.number,

  // The root DominatorTreeNode of this dominator tree.
  root: PropTypes.object,

  // The Set<NodeId> of expanded nodes in this dominator tree.
  expanded: PropTypes.object,

  // If a node is currently focused in the dominator tree, then this is it.
  focused: PropTypes.object,

  // If an error was thrown while getting this dominator tree, the `Error`
  // instance (or an error string message) is attached here.
  error: PropTypes.oneOfType([
    PropTypes.string,
    PropTypes.object,
  ]),

  // The breakdown used to generate descriptive labels of nodes in this
  // dominator tree.
  breakdown: breakdownModel,

  // The number of active requests to incrementally fetch subtrees. This should
  // only be non-zero when the state is INCREMENTAL_FETCHING.
  activeFetchRequestCount: PropTypes.number,

  // The dominatorTreeState that this domintor tree is currently in.
  state: catchAndIgnore(function (dominatorTree) {
    switch (dominatorTree.state) {
      case dominatorTreeState.COMPUTING:
        assert(dominatorTree.dominatorTreeId == null,
                "Should not have a dominator tree id yet");
        assert(!dominatorTree.root,
               "Should not have the root of the tree yet");
        assert(!dominatorTree.error,
               "Should not have an error");
        break;

      case dominatorTreeState.COMPUTED:
      case dominatorTreeState.FETCHING:
        assert(dominatorTree.dominatorTreeId != null,
               "Should have a dominator tree id");
        assert(!dominatorTree.root,
               "Should not have the root of the tree yet");
        assert(!dominatorTree.error,
               "Should not have an error");
        break;

      case dominatorTreeState.INCREMENTAL_FETCHING:
        assert(typeof dominatorTree.activeFetchRequestCount === "number",
               "The active fetch request count is a number when we are in the " +
               "INCREMENTAL_FETCHING state");
        assert(dominatorTree.activeFetchRequestCount > 0,
               "We are keeping track of how many active requests are in flight.");
        // Fall through...
      case dominatorTreeState.LOADED:
        assert(dominatorTree.dominatorTreeId != null,
               "Should have a dominator tree id");
        assert(dominatorTree.root,
               "Should have the root of the tree");
        assert(dominatorTree.expanded,
               "Should have an expanded set");
        assert(!dominatorTree.error,
               "Should not have an error");
        break;

      case dominatorTreeState.ERROR:
        assert(dominatorTree.error, "Should have an error");
        break;

      default:
        assert(false,
               `Unexpected dominator tree state: ${dominatorTree.state}`);
    }
  }),
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
  // Current dominator tree data for this snapshot.
  dominatorTree: dominatorTreeModel,
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
  state: catchAndIgnore(function (snapshot, propName) {
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
  }),
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
  secondSnapshotId: catchAndIgnore(function (diffing, propName) {
    if (diffing.secondSnapshotId && !diffing.firstSnapshotId) {
      throw new Error("Cannot have second snapshot without already having " +
                      "first snapshot");
    }
    return snapshotId(diffing, propName);
  }),

  // The current census data for the diffing.
  census: censusModel,

  // If an error was thrown while diffing, the `Error` instance is attached
  // here.
  error: PropTypes.object,

  // The current state the diffing is in.
  // @see ./constants.js
  state: catchAndIgnore(function (diffing) {
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
  }),
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

  // The breakdown object DSL describing how we want
  // the dominator tree labels to be computed.
  // @see `js/src/doc/Debugger/Debugger.Memory.md`
  dominatorTreeBreakdown: breakdownModel.isRequired,

  // List of reference to all snapshots taken
  snapshots: PropTypes.arrayOf(snapshotModel).isRequired,

  // True iff we want the tree displayed inverted.
  inverted: PropTypes.bool.isRequired,

  // If present, a filter string for pruning the tree items.
  filter: PropTypes.string,

  // If present, the current diffing state.
  diffing: diffingModel,

  // The current type of view.
  view: catchAndIgnore(function (app) {
    switch (app.view) {
      case viewState.CENSUS:
        assert(!app.diffing, "Should not be diffing");
        break;

      case viewState.DIFFING:
        assert(app.diffing, "Should be diffing");
        break;

      case viewState.DOMINATOR_TREE:
        assert(!app.diffing, "Should not be diffing");
        break;

      default:
        assert(false, `Unexpected type of view: ${app.view}`);
    }
  }),
};
