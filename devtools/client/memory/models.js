/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

/* global treeMapState, censusState */
/* eslint no-shadow: ["error", { "allow": ["app"] }] */

"use strict";

const { assert } = require("devtools/shared/DevToolsUtils");
const { MemoryFront } = require("devtools/client/fronts/memory");
const HeapAnalysesClient = require("devtools/shared/heapsnapshot/HeapAnalysesClient");
const PropTypes = require("devtools/client/shared/vendor/react-prop-types");
const {
  snapshotState: states,
  diffingState,
  dominatorTreeState,
  viewState,
  individualsState,
} = require("devtools/client/memory/constants");

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
  return function(...args) {
    try {
      fn(...args);
    } catch (err) {
      // continue regardless of error
    }

    return null;
  };
}

/**
 * The data describing the census report's shape, and its associated metadata.
 *
 * @see `js/src/doc/Debugger/Debugger.Memory.md`
 */
const censusDisplayModel = (exports.censusDisplay = PropTypes.shape({
  displayName: PropTypes.string.isRequired,
  tooltip: PropTypes.string.isRequired,
  inverted: PropTypes.bool.isRequired,
  breakdown: PropTypes.shape({
    by: PropTypes.string.isRequired,
  }),
}));

/**
 * How we want to label nodes in the dominator tree, and associated
 * metadata. The notable difference from `censusDisplayModel` is the lack of
 * an `inverted` property.
 *
 * @see `js/src/doc/Debugger/Debugger.Memory.md`
 */
const labelDisplayModel = (exports.labelDisplay = PropTypes.shape({
  displayName: PropTypes.string.isRequired,
  tooltip: PropTypes.string.isRequired,
  breakdown: PropTypes.shape({
    by: PropTypes.string.isRequired,
  }),
}));

/**
 * The data describing the tree map's shape, and its associated metadata.
 *
 * @see `js/src/doc/Debugger/Debugger.Memory.md`
 */
const treeMapDisplayModel = (exports.treeMapDisplay = PropTypes.shape({
  displayName: PropTypes.string.isRequired,
  tooltip: PropTypes.string.isRequired,
  inverted: PropTypes.bool.isRequired,
  breakdown: PropTypes.shape({
    by: PropTypes.string.isRequired,
  }),
}));

/**
 * Tree map model.
 */
const treeMapModel = (exports.treeMapModel = PropTypes.shape({
  // The current census report data.
  report: PropTypes.object,
  // The display data used to generate the current census.
  display: treeMapDisplayModel,
  // The current treeMapState this is in
  state: catchAndIgnore(function(treeMap) {
    switch (treeMap.state) {
      case treeMapState.SAVING:
        assert(!treeMap.report, "Should not have a report");
        assert(!treeMap.error, "Should not have an error");
        break;

      case treeMapState.SAVED:
        assert(treeMap.report, "Should have a report");
        assert(!treeMap.error, "Should not have an error");
        break;

      case treeMapState.ERROR:
        assert(treeMap.error, "Should have an error");
        break;

      default:
        assert(false, `Unexpected treeMap state: ${treeMap.state}`);
    }
  }),
}));

const censusModel = (exports.censusModel = PropTypes.shape({
  // The current census report data.
  report: PropTypes.object,
  // The parent map for the report.
  parentMap: PropTypes.object,
  // The display data used to generate the current census.
  display: censusDisplayModel,
  // If present, the currently cached report's filter string used for pruning
  // the tree items.
  filter: PropTypes.string,
  // The Immutable.Set<CensusTreeNode.id> of expanded node ids in the report
  // tree.
  expanded: catchAndIgnore(function(census) {
    if (census.report) {
      assert(
        census.expanded,
        "If we have a report, we should also have the set of expanded nodes"
      );
    }
  }),
  // If a node is currently focused in the report tree, then this is it.
  focused: PropTypes.object,
  // The censusModelState that this census is currently in.
  state: catchAndIgnore(function(census) {
    switch (census.state) {
      case censusState.SAVING:
        assert(!census.report, "Should not have a report");
        assert(!census.parentMap, "Should not have a parent map");
        assert(census.expanded, "Should not have an expanded set");
        assert(!census.error, "Should not have an error");
        break;

      case censusState.SAVED:
        assert(census.report, "Should have a report");
        assert(census.parentMap, "Should have a parent map");
        assert(census.expanded, "Should have an expanded set");
        assert(!census.error, "Should not have an error");
        break;

      case censusState.ERROR:
        assert(!census.report, "Should not have a report");
        assert(census.error, "Should have an error");
        break;

      default:
        assert(false, `Unexpected census state: ${census.state}`);
    }
  }),
}));

/**
 * Dominator tree model.
 */
const dominatorTreeModel = (exports.dominatorTreeModel = PropTypes.shape({
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
  error: PropTypes.oneOfType([PropTypes.string, PropTypes.object]),

  // The display used to generate descriptive labels of nodes in this dominator
  // tree.
  display: labelDisplayModel,

  // The number of active requests to incrementally fetch subtrees. This should
  // only be non-zero when the state is INCREMENTAL_FETCHING.
  activeFetchRequestCount: PropTypes.number,

  // The dominatorTreeState that this domintor tree is currently in.
  state: catchAndIgnore(function(dominatorTree) {
    switch (dominatorTree.state) {
      case dominatorTreeState.COMPUTING:
        assert(
          dominatorTree.dominatorTreeId == null,
          "Should not have a dominator tree id yet"
        );
        assert(!dominatorTree.root, "Should not have the root of the tree yet");
        assert(!dominatorTree.error, "Should not have an error");
        break;

      case dominatorTreeState.COMPUTED:
      case dominatorTreeState.FETCHING:
        assert(
          dominatorTree.dominatorTreeId != null,
          "Should have a dominator tree id"
        );
        assert(!dominatorTree.root, "Should not have the root of the tree yet");
        assert(!dominatorTree.error, "Should not have an error");
        break;

      case dominatorTreeState.INCREMENTAL_FETCHING:
        assert(
          typeof dominatorTree.activeFetchRequestCount === "number",
          "The active fetch request count is a number when we are in the " +
            "INCREMENTAL_FETCHING state"
        );
        assert(
          dominatorTree.activeFetchRequestCount > 0,
          "We are keeping track of how many active requests are in flight."
        );
      // Fall through...
      case dominatorTreeState.LOADED:
        assert(
          dominatorTree.dominatorTreeId != null,
          "Should have a dominator tree id"
        );
        assert(dominatorTree.root, "Should have the root of the tree");
        assert(dominatorTree.expanded, "Should have an expanded set");
        assert(!dominatorTree.error, "Should not have an error");
        break;

      case dominatorTreeState.ERROR:
        assert(dominatorTree.error, "Should have an error");
        break;

      default:
        assert(
          false,
          `Unexpected dominator tree state: ${dominatorTree.state}`
        );
    }
  }),
}));

/**
 * Snapshot model.
 */
const stateKeys = Object.keys(states).map(state => states[state]);
const snapshotId = PropTypes.number;
const snapshotModel = (exports.snapshot = PropTypes.shape({
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
  // Current tree map data for this snapshot.
  treeMap: treeMapModel,
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
  state: catchAndIgnore(function(snapshot, propName) {
    const current = snapshot.state;
    const shouldHavePath = [states.IMPORTING, states.SAVED, states.READ];
    const shouldHaveCreationTime = [states.READ];

    if (!stateKeys.includes(current)) {
      throw new Error(`Snapshot state must be one of ${stateKeys}.`);
    }
    if (shouldHavePath.includes(current) && !snapshot.path) {
      throw new Error(
        `Snapshots in state ${current} must have a snapshot path.`
      );
    }
    if (shouldHaveCreationTime.includes(current) && !snapshot.creationTime) {
      throw new Error(
        `Snapshots in state ${current} must have a creation time.`
      );
    }
  }),
}));

const allocationsModel = (exports.allocations = PropTypes.shape({
  // True iff we are recording allocation stacks right now.
  recording: PropTypes.bool.isRequired,
  // True iff we are in the process of toggling the recording of allocation
  // stacks on or off right now.
  togglingInProgress: PropTypes.bool.isRequired,
}));

const diffingModel = (exports.diffingModel = PropTypes.shape({
  // The id of the first snapshot to diff.
  firstSnapshotId: snapshotId,

  // The id of the second snapshot to diff.
  secondSnapshotId: catchAndIgnore(function(diffing, propName) {
    if (diffing.secondSnapshotId && !diffing.firstSnapshotId) {
      throw new Error(
        "Cannot have second snapshot without already having " + "first snapshot"
      );
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
  state: catchAndIgnore(function(diffing) {
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
}));

const previousViewModel = (exports.previousView = PropTypes.shape({
  state: catchAndIgnore(function(previous) {
    switch (previous.state) {
      case viewState.DIFFING:
        assert(previous.diffing, "Should have previous diffing state.");
        assert(
          !previous.selected,
          "Should not have a previously selected snapshot."
        );
        break;

      case viewState.CENSUS:
      case viewState.DOMINATOR_TREE:
      case viewState.TREE_MAP:
        assert(
          previous.selected,
          "Should have a previously selected snapshot."
        );
        break;

      case viewState.INDIVIDUALS:
      default:
        assert(false, `Unexpected previous view state: ${previous.state}.`);
    }
  }),

  // The previous diffing state, if any.
  diffing: diffingModel,

  // The previously selected snapshot, if any.
  selected: snapshotId,
}));

exports.view = PropTypes.shape({
  // The current view state.
  state: catchAndIgnore(function(view) {
    switch (view.state) {
      case viewState.DIFFING:
      case viewState.CENSUS:
      case viewState.DOMINATOR_TREE:
      case viewState.INDIVIDUALS:
      case viewState.TREE_MAP:
        break;

      default:
        assert(false, `Unexpected type of view: ${view.state}`);
    }
  }),

  // The previous view state.
  previous: previousViewModel,
});

const individualsModel = (exports.individuals = PropTypes.shape({
  error: PropTypes.object,

  nodes: PropTypes.arrayOf(PropTypes.object),

  dominatorTree: dominatorTreeModel,

  id: snapshotId,

  censusBreakdown: PropTypes.object,

  indices: PropTypes.object,

  labelDisplay: labelDisplayModel,

  focused: PropTypes.object,

  state: catchAndIgnore(function(individuals) {
    switch (individuals.state) {
      case individualsState.COMPUTING_DOMINATOR_TREE:
      case individualsState.FETCHING:
        assert(!individuals.nodes, "Should not have individual nodes");
        assert(!individuals.dominatorTree, "Should not have dominator tree");
        assert(!individuals.id, "Should not have an id");
        assert(
          !individuals.censusBreakdown,
          "Should not have a censusBreakdown"
        );
        assert(!individuals.indices, "Should not have indices");
        assert(!individuals.labelDisplay, "Should not have a labelDisplay");
        break;

      case individualsState.FETCHED:
        assert(individuals.nodes, "Should have individual nodes");
        assert(individuals.dominatorTree, "Should have dominator tree");
        assert(individuals.id, "Should have an id");
        assert(individuals.censusBreakdown, "Should have a censusBreakdown");
        assert(individuals.indices, "Should have indices");
        assert(individuals.labelDisplay, "Should have a labelDisplay");
        break;

      case individualsState.ERROR:
        assert(individuals.error, "Should have an error object");
        break;

      default:
        assert(false, `Unexpected individuals state: ${individuals.state}`);
        break;
    }
  }),
}));

exports.app = {
  // {Commands} Used to communicate with the backend
  commands: PropTypes.object,

  // {MemoryFront} Used to communicate with platform
  front: PropTypes.instanceOf(MemoryFront),

  // Allocations recording related data.
  allocations: allocationsModel.isRequired,

  // {HeapAnalysesClient} Used to interface with snapshots
  heapWorker: PropTypes.instanceOf(HeapAnalysesClient),

  // The display data describing how we want the census data to be.
  censusDisplay: censusDisplayModel.isRequired,

  // The display data describing how we want the dominator tree labels to be
  // computed.
  labelDisplay: labelDisplayModel.isRequired,

  // The display data describing how we want the dominator tree labels to be
  // computed.
  treeMapDisplay: treeMapDisplayModel.isRequired,

  // List of reference to all snapshots taken
  snapshots: PropTypes.arrayOf(snapshotModel).isRequired,

  // If present, a filter string for pruning the tree items.
  filter: PropTypes.string,

  // If present, the current diffing state.
  diffing: diffingModel,

  // If present, the current individuals state.
  individuals: individualsModel,

  // The current type of view.
  view(app) {
    catchAndIgnore(function(app) {
      switch (app.view.state) {
        case viewState.DIFFING:
          assert(app.diffing, "Should be diffing");
          break;

        case viewState.INDIVIDUALS:
        case viewState.CENSUS:
        case viewState.DOMINATOR_TREE:
        case viewState.TREE_MAP:
          assert(!app.diffing, "Should not be diffing");
          break;

        default:
          assert(false, `Unexpected type of view: ${app.view.state}`);
      }
    })(app);

    catchAndIgnore(function(app) {
      switch (app.view.state) {
        case viewState.INDIVIDUALS:
          assert(app.individuals, "Should have individuals state");
          break;

        case viewState.DIFFING:
        case viewState.CENSUS:
        case viewState.DOMINATOR_TREE:
        case viewState.TREE_MAP:
          assert(!app.individuals, "Should not have individuals state");
          break;

        default:
          assert(false, `Unexpected type of view: ${app.view.state}`);
      }
    })(app);
  },
};
