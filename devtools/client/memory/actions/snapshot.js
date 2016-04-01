/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */
"use strict";

const { assert, reportException } = require("devtools/shared/DevToolsUtils");
const {
  censusIsUpToDate,
  getSnapshot,
  createSnapshot,
  dominatorTreeIsComputed,
  canTakeCensus
} = require("../utils");
const {
  actions,
  snapshotState: states,
  viewState,
  censusState,
  treeMapState,
  dominatorTreeState
} = require("../constants");
const telemetry = require("../telemetry");
const view = require("./view");
const refresh = require("./refresh");
const diffing = require("./diffing");

/**
 * A series of actions are fired from this task to save, read and generate the
 * initial census from a snapshot.
 *
 * @param {MemoryFront}
 * @param {HeapAnalysesClient}
 * @param {Object}
 */
const takeSnapshotAndCensus = exports.takeSnapshotAndCensus = function (front, heapWorker) {
  return function *(dispatch, getState) {
    const id = yield dispatch(takeSnapshot(front));
    if (id === null) {
      return;
    }

    yield dispatch(readSnapshot(heapWorker, id));
    yield dispatch(computeSnapshotData(heapWorker, id));
  };
};

/**
 * Create the census for the snapshot with the provided snapshot id. If the
 * current view is the DOMINATOR_TREE view, create the dominator tree for this
 * snapshot as well.
 *
 * @param {HeapAnalysesClient} heapWorker
 * @param {snapshotId} id
 */
const computeSnapshotData = exports.computeSnapshotData = function(heapWorker, id) {
  return function* (dispatch, getState) {
    if (getSnapshot(getState(), id).state !== states.READ) {
      return;
    }

    // Decide which type of census to take.
    const censusTaker = getCurrentCensusTaker(getState().view);
    yield dispatch(censusTaker(heapWorker, id));

    if (getState().view === viewState.DOMINATOR_TREE) {
      yield dispatch(computeAndFetchDominatorTree(heapWorker, id));
    }
  };
};

/**
 * Selects a snapshot and if the snapshot's census is using a different
 * display, take a new census.
 *
 * @param {HeapAnalysesClient} heapWorker
 * @param {snapshotId} id
 */
const selectSnapshotAndRefresh = exports.selectSnapshotAndRefresh = function (heapWorker, id) {
  return function *(dispatch, getState) {
    if (getState().diffing) {
      dispatch(view.changeView(viewState.CENSUS));
    }

    dispatch(selectSnapshot(id));
    yield dispatch(refresh.refresh(heapWorker));
  };
};

/**
 * Take a snapshot and return its id on success, or null on failure.
 *
 * @param {MemoryFront} front
 * @returns {Number|null}
 */
const takeSnapshot = exports.takeSnapshot = function (front) {
  return function *(dispatch, getState) {
    telemetry.countTakeSnapshot();

    if (getState().diffing) {
      dispatch(view.changeView(viewState.CENSUS));
    }

    const snapshot = createSnapshot(getState());
    const id = snapshot.id;
    dispatch({ type: actions.TAKE_SNAPSHOT_START, snapshot });
    dispatch(selectSnapshot(id));

    let path;
    try {
      path = yield front.saveHeapSnapshot();
    } catch (error) {
      reportException("takeSnapshot", error);
      dispatch({ type: actions.SNAPSHOT_ERROR, id, error });
      return null;
    }

    dispatch({ type: actions.TAKE_SNAPSHOT_END, id, path });
    return snapshot.id;
  };
};

/**
 * Reads a snapshot into memory; necessary to do before taking
 * a census on the snapshot. May only be called once per snapshot.
 *
 * @param {HeapAnalysesClient} heapWorker
 * @param {snapshotId} id
 */
const readSnapshot = exports.readSnapshot = function readSnapshot (heapWorker, id) {
  return function *(dispatch, getState) {
    const snapshot = getSnapshot(getState(), id);
    assert([states.SAVED, states.IMPORTING].includes(snapshot.state),
      `Should only read a snapshot once. Found snapshot in state ${snapshot.state}`);

    let creationTime;

    dispatch({ type: actions.READ_SNAPSHOT_START, id });
    try {
      yield heapWorker.readHeapSnapshot(snapshot.path);
      creationTime = yield heapWorker.getCreationTime(snapshot.path);
    } catch (error) {
      reportException("readSnapshot", error);
      dispatch({ type: actions.SNAPSHOT_ERROR, id, error });
      return;
    }

    dispatch({ type: actions.READ_SNAPSHOT_END, id, creationTime });
  };
};

/**
 * Census and tree maps both require snapshots. This function shares the logic
 * of creating snapshots, but is configurable with specific actions for the
 * individual census types.
 *
 * @param {getDisplay} Get the display object from the state.
 * @param {getCensus} Get the census from the snapshot.
 * @param {beginAction} Action to send at the beginning of a heap snapshot.
 * @param {endAction} Action to send at the end of a heap snapshot.
 * @param {errorAction} Action to send if a snapshot has an error.
 */
function makeTakeCensusTask({ getDisplay, getFilter, getCensus, beginAction,
                              endAction, errorAction }) {
  /**
   * @param {HeapAnalysesClient} heapWorker
   * @param {snapshotId} id
   *
   * @see {Snapshot} model defined in devtools/client/memory/models.js
   * @see `devtools/shared/heapsnapshot/HeapAnalysesClient.js`
   * @see `js/src/doc/Debugger/Debugger.Memory.md` for breakdown details
   */
  return function (heapWorker, id) {
    return function *(dispatch, getState) {
      const snapshot = getSnapshot(getState(), id);
      // Assert that snapshot is in a valid state

      assert(canTakeCensus(snapshot),
        `Attempting to take a census when the snapshot is not in a ready state.`);

      let report, parentMap;
      let display = getDisplay(getState());
      let filter = getFilter(getState());

      // If display, filter and inversion haven't changed, don't do anything.
      if (censusIsUpToDate(filter, display, getCensus(snapshot))) {
        return;
      }

      // Keep taking a census if the display changes while our request is in
      // flight. Recheck that the display used for the census is the same as the
      // state's display.
      do {
        display = getDisplay(getState());
        filter = getState().filter;

        dispatch({
          type: beginAction,
          id,
          filter,
          display
        });

        let opts = display.inverted
          ? { asInvertedTreeNode: true }
          : { asTreeNode: true };

        opts.filter = filter || null;

        try {
          ({ report, parentMap } = yield heapWorker.takeCensus(
            snapshot.path,
            { breakdown: display.breakdown },
            opts));
        } catch (error) {
          reportException("takeCensus", error);
          dispatch({ type: errorAction, id, error });
          return;
        }
      }
      while (filter !== getState().filter ||
             display !== getDisplay(getState()));

      dispatch({
        type: endAction,
        id,
        display,
        filter,
        report,
        parentMap
      });

      telemetry.countCensus({ filter, display });
    };
  };
}

/**
 * Take a census.
 */
const takeCensus = exports.takeCensus = makeTakeCensusTask({
  getDisplay: (state) => state.censusDisplay,
  getFilter: (state) => state.filter,
  getCensus: (snapshot) => snapshot.census,
  beginAction: actions.TAKE_CENSUS_START,
  endAction: actions.TAKE_CENSUS_END,
  errorAction: actions.TAKE_CENSUS_ERROR
});

/**
 * Take a census for the treemap.
 */
const takeTreeMap = exports.takeTreeMap = makeTakeCensusTask({
  getDisplay: (state) => state.treeMapDisplay,
  getFilter: () => null,
  getCensus: (snapshot) => snapshot.treeMap,
  beginAction: actions.TAKE_TREE_MAP_START,
  endAction: actions.TAKE_TREE_MAP_END,
  errorAction: actions.TAKE_TREE_MAP_ERROR
});

/**
 * Define what should be the default mode for taking a census based on the
 * default view of the tool.
 */
const defaultCensusTaker = takeTreeMap;

/**
 * Pick the default census taker when taking a snapshot. This should be
 * determined by the current view. If the view doesn't include a census, then
 * use the default one defined above. Some census information is always needed
 * to display some basic information about a snapshot.
 *
 * @param {string} value from viewState
 */
const getCurrentCensusTaker = exports.getCurrentCensusTaker = function (currentView) {
  switch (currentView) {
    case viewState.TREE_MAP:
      return takeTreeMap;
    break;
    case viewState.CENSUS:
      return takeCensus;
    break;
  }
  return defaultCensusTaker;
};

/**
 * Refresh the selected snapshot's census data, if need be (for example,
 * display configuration changed).
 *
 * @param {HeapAnalysesClient} heapWorker
 */
const refreshSelectedCensus = exports.refreshSelectedCensus = function (heapWorker) {
  return function*(dispatch, getState) {
    let snapshot = getState().snapshots.find(s => s.selected);

    // Intermediate snapshot states will get handled by the task action that is
    // orchestrating them. For example, if the snapshot census's state is
    // SAVING, then the takeCensus action will keep taking a census until
    // the inverted property matches the inverted state. If the snapshot is
    // still in the process of being saved or read, the takeSnapshotAndCensus
    // task action will follow through and ensure that a census is taken.
    if (snapshot &&
        (snapshot.census && snapshot.census.state === censusState.SAVED) ||
        !snapshot.census) {
      yield dispatch(takeCensus(heapWorker, snapshot.id));
    }
  };
};

/**
 * Refresh the selected snapshot's tree map data, if need be (for example,
 * display configuration changed).
 *
 * @param {HeapAnalysesClient} heapWorker
 */
const refreshSelectedTreeMap = exports.refreshSelectedTreeMap = function (heapWorker) {
  return function*(dispatch, getState) {
    let snapshot = getState().snapshots.find(s => s.selected);

    // Intermediate snapshot states will get handled by the task action that is
    // orchestrating them. For example, if the snapshot census's state is
    // SAVING, then the takeCensus action will keep taking a census until
    // the inverted property matches the inverted state. If the snapshot is
    // still in the process of being saved or read, the takeSnapshotAndCensus
    // task action will follow through and ensure that a census is taken.
    if (snapshot &&
        (snapshot.treeMap && snapshot.treeMap.state === treeMapState.SAVED) ||
        !snapshot.treeMap) {
      yield dispatch(takeTreeMap(heapWorker, snapshot.id));
    }
  };
};

/**
 * Request that the `HeapAnalysesWorker` compute the dominator tree for the
 * snapshot with the given `id`.
 *
 * @param {HeapAnalysesClient} heapWorker
 * @param {SnapshotId} id
 *
 * @returns {Promise<DominatorTreeId>}
 */
const computeDominatorTree = exports.computeDominatorTree = function (heapWorker, id) {
  return function*(dispatch, getState) {
    const snapshot = getSnapshot(getState(), id);
    assert(!(snapshot.dominatorTree && snapshot.dominatorTree.dominatorTreeId),
           "Should not re-compute dominator trees");

    dispatch({ type: actions.COMPUTE_DOMINATOR_TREE_START, id });

    let dominatorTreeId;
    try {
      dominatorTreeId = yield heapWorker.computeDominatorTree(snapshot.path);
    } catch (error) {
      reportException("actions/snapshot/computeDominatorTree", error);
      dispatch({ type: actions.DOMINATOR_TREE_ERROR, id, error });
      return null;
    }

    dispatch({ type: actions.COMPUTE_DOMINATOR_TREE_END, id, dominatorTreeId });
    return dominatorTreeId;
  };
};

/**
 * Get the partial subtree, starting from the root, of the
 * snapshot-with-the-given-id's dominator tree.
 *
 * @param {HeapAnalysesClient} heapWorker
 * @param {SnapshotId} id
 *
 * @returns {Promise<DominatorTreeNode>}
 */
const fetchDominatorTree = exports.fetchDominatorTree = function (heapWorker, id) {
  return function*(dispatch, getState) {
    const snapshot = getSnapshot(getState(), id);
    assert(dominatorTreeIsComputed(snapshot),
           "Should have dominator tree model and it should be computed");

    let display;
    let root;
    do {
      display = getState().dominatorTreeDisplay;
      assert(display && display.breakdown,
             `Should have a breakdown to describe nodes with, got: ${uneval(display)}`);

      dispatch({ type: actions.FETCH_DOMINATOR_TREE_START, id, display });

      try {
        root = yield heapWorker.getDominatorTree({
          dominatorTreeId: snapshot.dominatorTree.dominatorTreeId,
          breakdown: display.breakdown,
        });
      } catch (error) {
        reportException("actions/snapshot/fetchDominatorTree", error);
        dispatch({ type: actions.DOMINATOR_TREE_ERROR, id, error });
        return null;
      }
    }
    while (display !== getState().dominatorTreeDisplay);

    dispatch({ type: actions.FETCH_DOMINATOR_TREE_END, id, root });
    telemetry.countDominatorTree({ display });
    return root;
  };
};

/**
 * Fetch the immediately dominated children represented by the placeholder
 * `lazyChildren` from snapshot-with-the-given-id's dominator tree.
 *
 * @param {HeapAnalysesClient} heapWorker
 * @param {SnapshotId} id
 * @param {DominatorTreeLazyChildren} lazyChildren
 */
const fetchImmediatelyDominated = exports.fetchImmediatelyDominated = function (heapWorker, id, lazyChildren) {
  return function*(dispatch, getState) {
    const snapshot = getSnapshot(getState(), id);
    assert(snapshot.dominatorTree, "Should have dominator tree model");
    assert(snapshot.dominatorTree.state === dominatorTreeState.LOADED ||
           snapshot.dominatorTree.state === dominatorTreeState.INCREMENTAL_FETCHING,
           "Cannot fetch immediately dominated nodes in a dominator tree unless " +
           " the dominator tree has already been computed");

    let display;
    let response;
    do {
      display = getState().dominatorTreeDisplay;
      assert(display, "Should have a display to describe nodes with.");

      dispatch({ type: actions.FETCH_IMMEDIATELY_DOMINATED_START, id });

      try {
        response = yield heapWorker.getImmediatelyDominated({
          dominatorTreeId: snapshot.dominatorTree.dominatorTreeId,
          breakdown: display.breakdown,
          nodeId: lazyChildren.parentNodeId(),
          startIndex: lazyChildren.siblingIndex(),
        });
      } catch (error) {
        reportException("actions/snapshot/fetchImmediatelyDominated", error);
        dispatch({ type: actions.DOMINATOR_TREE_ERROR, id, error });
        return null;
      }
    }
    while (display !== getState().dominatorTreeDisplay);

    dispatch({
      type: actions.FETCH_IMMEDIATELY_DOMINATED_END,
      id,
      path: response.path,
      nodes: response.nodes,
      moreChildrenAvailable: response.moreChildrenAvailable,
    });
  };
};

/**
 * Compute and then fetch the dominator tree of the snapshot with the given
 * `id`.
 *
 * @param {HeapAnalysesClient} heapWorker
 * @param {SnapshotId} id
 *
 * @returns {Promise<DominatorTreeNode>}
 */
const computeAndFetchDominatorTree = exports.computeAndFetchDominatorTree = function (heapWorker, id) {
  return function*(dispatch, getState) {
    const dominatorTreeId = yield dispatch(computeDominatorTree(heapWorker, id));
    if (dominatorTreeId === null) {
      return null;
    }

    const root = yield dispatch(fetchDominatorTree(heapWorker, id));
    if (!root) {
      return null;
    }

    return root;
  };
};

/**
 * Update the currently selected snapshot's dominator tree.
 *
 * @param {HeapAnalysesClient} heapWorker
 */
const refreshSelectedDominatorTree = exports.refreshSelectedDominatorTree = function (heapWorker) {
  return function*(dispatch, getState) {
    let snapshot = getState().snapshots.find(s => s.selected);
    if (!snapshot) {
      return;
    }

    if (snapshot.dominatorTree &&
        !(snapshot.dominatorTree.state === dominatorTreeState.COMPUTED ||
          snapshot.dominatorTree.state === dominatorTreeState.LOADED ||
          snapshot.dominatorTree.state === dominatorTreeState.INCREMENTAL_FETCHING)) {
      return;
    }

    if (snapshot.state === states.READ) {
      if (snapshot.dominatorTree) {
        yield dispatch(fetchDominatorTree(heapWorker, snapshot.id));
      } else {
        yield dispatch(computeAndFetchDominatorTree(heapWorker, snapshot.id));
      }
    } else {
        // If there was an error, we can't continue. If we are still saving or
        // reading the snapshot, then takeSnapshotAndCensus will finish the job
        // for us.
        return;
    }
  };
};

/**
 * Select the snapshot with the given id.
 *
 * @param {snapshotId} id
 * @see {Snapshot} model defined in devtools/client/memory/models.js
 */
const selectSnapshot = exports.selectSnapshot = function (id) {
  return {
    type: actions.SELECT_SNAPSHOT,
    id
  };
};

/**
 * Delete all snapshots that are in the READ or ERROR state
 *
 * @param {HeapAnalysesClient} heapWorker
 */
const clearSnapshots = exports.clearSnapshots = function (heapWorker) {
  return function*(dispatch, getState) {
    let snapshots = getState().snapshots.filter(s => {
      let snapshotReady = s.state === states.READ || s.state === states.ERROR;
      let censusReady = (s.treeMap && s.treeMap.state === treeMapState.SAVED) ||
                        (s.census && s.census.state === censusState.SAVED);

      return snapshotReady && censusReady
    });

    let ids = snapshots.map(s => s.id);

    dispatch({ type: actions.DELETE_SNAPSHOTS_START, ids });

    if (getState().diffing) {
      dispatch(diffing.toggleDiffing());
    }

    yield Promise.all(snapshots.map(snapshot => {
      return heapWorker.deleteHeapSnapshot(snapshot.path).catch(error => {
        reportException("clearSnapshots", error);
        dispatch({ type: actions.SNAPSHOT_ERROR, id: snapshot.id, error });
      });
    }));

    dispatch({ type: actions.DELETE_SNAPSHOTS_END, ids });
  };
};

/**
 * Expand the given node in the snapshot's census report.
 *
 * @param {CensusTreeNode} node
 */
const expandCensusNode = exports.expandCensusNode = function (id, node) {
  return {
    type: actions.EXPAND_CENSUS_NODE,
    id,
    node,
  };
};

/**
 * Collapse the given node in the snapshot's census report.
 *
 * @param {CensusTreeNode} node
 */
const collapseCensusNode = exports.collapseCensusNode = function (id, node) {
  return {
    type: actions.COLLAPSE_CENSUS_NODE,
    id,
    node,
  };
};

/**
 * Focus the given node in the snapshot's census's report.
 *
 * @param {SnapshotId} id
 * @param {DominatorTreeNode} node
 */
const focusCensusNode = exports.focusCensusNode = function (id, node) {
  return {
    type: actions.FOCUS_CENSUS_NODE,
    id,
    node,
  };
};

/**
 * Expand the given node in the snapshot's dominator tree.
 *
 * @param {DominatorTreeTreeNode} node
 */
const expandDominatorTreeNode = exports.expandDominatorTreeNode = function (id, node) {
  return {
    type: actions.EXPAND_DOMINATOR_TREE_NODE,
    id,
    node,
  };
};

/**
 * Collapse the given node in the snapshot's dominator tree.
 *
 * @param {DominatorTreeTreeNode} node
 */
const collapseDominatorTreeNode = exports.collapseDominatorTreeNode = function (id, node) {
  return {
    type: actions.COLLAPSE_DOMINATOR_TREE_NODE,
    id,
    node,
  };
};

/**
 * Focus the given node in the snapshot's dominator tree.
 *
 * @param {SnapshotId} id
 * @param {DominatorTreeNode} node
 */
const focusDominatorTreeNode = exports.focusDominatorTreeNode = function (id, node) {
  return {
    type: actions.FOCUS_DOMINATOR_TREE_NODE,
    id,
    node,
  };
};
