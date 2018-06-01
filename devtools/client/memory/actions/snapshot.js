/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */
"use strict";

const { Preferences } = require("resource://gre/modules/Preferences.jsm");
const { assert, reportException, isSet } = require("devtools/shared/DevToolsUtils");
const {
  censusIsUpToDate,
  getSnapshot,
  createSnapshot,
  dominatorTreeIsComputed,
} = require("../utils");
const {
  actions,
  snapshotState: states,
  viewState,
  censusState,
  treeMapState,
  dominatorTreeState,
  individualsState,
} = require("../constants");
const view = require("./view");
const refresh = require("./refresh");
const diffing = require("./diffing");
const TaskCache = require("./task-cache");

/**
 * A series of actions are fired from this task to save, read and generate the
 * initial census from a snapshot.
 *
 * @param {MemoryFront}
 * @param {HeapAnalysesClient}
 * @param {Object}
 */
exports.takeSnapshotAndCensus = function(front, heapWorker) {
  return async function(dispatch, getState) {
    const id = await dispatch(takeSnapshot(front));
    if (id === null) {
      return;
    }

    await dispatch(readSnapshot(heapWorker, id));
    if (getSnapshot(getState(), id).state !== states.READ) {
      return;
    }

    await dispatch(computeSnapshotData(heapWorker, id));
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
  return async function(dispatch, getState) {
    if (getSnapshot(getState(), id).state !== states.READ) {
      return;
    }

    // Decide which type of census to take.
    const censusTaker = getCurrentCensusTaker(getState().view.state);
    await dispatch(censusTaker(heapWorker, id));

    if (getState().view.state === viewState.DOMINATOR_TREE &&
        !getSnapshot(getState(), id).dominatorTree) {
      await dispatch(computeAndFetchDominatorTree(heapWorker, id));
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
exports.selectSnapshotAndRefresh = function(heapWorker, id) {
  return async function(dispatch, getState) {
    if (getState().diffing || getState().individuals) {
      dispatch(view.changeView(viewState.CENSUS));
    }

    dispatch(selectSnapshot(id));
    await dispatch(refresh.refresh(heapWorker));
  };
};

/**
 * Take a snapshot and return its id on success, or null on failure.
 *
 * @param {MemoryFront} front
 * @returns {Number|null}
 */
const takeSnapshot = exports.takeSnapshot = function(front) {
  return async function(dispatch, getState) {
    if (getState().diffing || getState().individuals) {
      dispatch(view.changeView(viewState.CENSUS));
    }

    const snapshot = createSnapshot(getState());
    const id = snapshot.id;
    dispatch({ type: actions.TAKE_SNAPSHOT_START, snapshot });
    dispatch(selectSnapshot(id));

    let path;
    try {
      path = await front.saveHeapSnapshot();
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
const readSnapshot = exports.readSnapshot =
TaskCache.declareCacheableTask({
  getCacheKey(_, id) {
    return id;
  },

  task: async function(heapWorker, id, removeFromCache, dispatch, getState) {
    const snapshot = getSnapshot(getState(), id);
    assert([states.SAVED, states.IMPORTING].includes(snapshot.state),
           `Should only read a snapshot once. Found snapshot in state ${snapshot.state}`);

    let creationTime;

    dispatch({ type: actions.READ_SNAPSHOT_START, id });
    try {
      await heapWorker.readHeapSnapshot(snapshot.path);
      creationTime = await heapWorker.getCreationTime(snapshot.path);
    } catch (error) {
      removeFromCache();
      reportException("readSnapshot", error);
      dispatch({ type: actions.SNAPSHOT_ERROR, id, error });
      return;
    }

    removeFromCache();
    dispatch({ type: actions.READ_SNAPSHOT_END, id, creationTime });
  }
});

let takeCensusTaskCounter = 0;

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
                              endAction, errorAction, canTakeCensus }) {
  /**
   * @param {HeapAnalysesClient} heapWorker
   * @param {snapshotId} id
   *
   * @see {Snapshot} model defined in devtools/client/memory/models.js
   * @see `devtools/shared/heapsnapshot/HeapAnalysesClient.js`
   * @see `js/src/doc/Debugger/Debugger.Memory.md` for breakdown details
   */
  const thisTakeCensusTaskId = ++takeCensusTaskCounter;
  return TaskCache.declareCacheableTask({
    getCacheKey(_, id) {
      return `take-census-task-${thisTakeCensusTaskId}-${id}`;
    },

    task: async function(heapWorker, id, removeFromCache, dispatch, getState) {
      const snapshot = getSnapshot(getState(), id);
      if (!snapshot) {
        removeFromCache();
        return;
      }

      // Assert that snapshot is in a valid state
      assert(canTakeCensus(snapshot),
        "Attempting to take a census when the snapshot is not in a ready state. " +
        `snapshot.state = ${snapshot.state}, ` +
        `census.state = ${(getCensus(snapshot) || { state: null }).state}`);

      let report, parentMap;
      let display = getDisplay(getState());
      let filter = getFilter(getState());

      // If display, filter and inversion haven't changed, don't do anything.
      if (censusIsUpToDate(filter, display, getCensus(snapshot))) {
        removeFromCache();
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

        const opts = display.inverted
          ? { asInvertedTreeNode: true }
          : { asTreeNode: true };

        opts.filter = filter || null;

        try {
          ({ report, parentMap } = await heapWorker.takeCensus(
            snapshot.path,
            { breakdown: display.breakdown },
            opts));
        } catch (error) {
          removeFromCache();
          reportException("takeCensus", error);
          dispatch({ type: errorAction, id, error });
          return;
        }
      }
      while (filter !== getState().filter ||
             display !== getDisplay(getState()));

      removeFromCache();
      dispatch({
        type: endAction,
        id,
        display,
        filter,
        report,
        parentMap
      });
    }
  });
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
  errorAction: actions.TAKE_CENSUS_ERROR,
  canTakeCensus: snapshot =>
    snapshot.state === states.READ &&
    (!snapshot.census || snapshot.census.state === censusState.SAVED),
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
  errorAction: actions.TAKE_TREE_MAP_ERROR,
  canTakeCensus: snapshot =>
    snapshot.state === states.READ &&
    (!snapshot.treeMap || snapshot.treeMap.state === treeMapState.SAVED),
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
const getCurrentCensusTaker = exports.getCurrentCensusTaker = function(currentView) {
  switch (currentView) {
    case viewState.TREE_MAP:
      return takeTreeMap;
    case viewState.CENSUS:
      return takeCensus;
    default:
      return defaultCensusTaker;
  }
};

/**
 * Focus the given node in the individuals view.
 *
 * @param {DominatorTreeNode} node.
 */
exports.focusIndividual = function(node) {
  return {
    type: actions.FOCUS_INDIVIDUAL,
    node,
  };
};

/**
 * Fetch the individual `DominatorTreeNodes` for the census group specified by
 * `censusBreakdown` and `reportLeafIndex`.
 *
 * @param {HeapAnalysesClient} heapWorker
 * @param {SnapshotId} id
 * @param {Object} censusBreakdown
 * @param {Set<Number> | Number} reportLeafIndex
 */
const fetchIndividuals = exports.fetchIndividuals =
function(heapWorker, id, censusBreakdown, reportLeafIndex) {
  return async function(dispatch, getState) {
    if (getState().view.state !== viewState.INDIVIDUALS) {
      dispatch(view.changeView(viewState.INDIVIDUALS));
    }

    const snapshot = getSnapshot(getState(), id);
    assert(snapshot && snapshot.state === states.READ,
           "The snapshot should already be read into memory");

    if (!dominatorTreeIsComputed(snapshot)) {
      await dispatch(computeAndFetchDominatorTree(heapWorker, id));
    }

    const snapshot_ = getSnapshot(getState(), id);
    assert(snapshot_.dominatorTree && snapshot_.dominatorTree.root,
           "Should have a dominator tree with a root.");

    const dominatorTreeId = snapshot_.dominatorTree.dominatorTreeId;

    const indices = isSet(reportLeafIndex)
      ? reportLeafIndex
      : new Set([reportLeafIndex]);

    let labelDisplay;
    let nodes;
    do {
      labelDisplay = getState().labelDisplay;
      assert(labelDisplay && labelDisplay.breakdown && labelDisplay.breakdown.by,
             `Should have a breakdown to label nodes with, got: ${uneval(labelDisplay)}`);

      if (getState().view.state !== viewState.INDIVIDUALS) {
        // We switched views while in the process of fetching individuals -- any
        // further work is useless.
        return;
      }

      dispatch({ type: actions.FETCH_INDIVIDUALS_START });

      try {
        ({ nodes } = await heapWorker.getCensusIndividuals({
          dominatorTreeId,
          indices,
          censusBreakdown,
          labelBreakdown: labelDisplay.breakdown,
          maxRetainingPaths: Preferences.get("devtools.memory.max-retaining-paths"),
          maxIndividuals: Preferences.get("devtools.memory.max-individuals"),
        }));
      } catch (error) {
        reportException("actions/snapshot/fetchIndividuals", error);
        dispatch({ type: actions.INDIVIDUALS_ERROR, error });
        return;
      }
    }
    while (labelDisplay !== getState().labelDisplay);

    dispatch({
      type: actions.FETCH_INDIVIDUALS_END,
      id,
      censusBreakdown,
      indices,
      labelDisplay,
      nodes,
      dominatorTree: snapshot_.dominatorTree,
    });
  };
};

/**
 * Refresh the current individuals view.
 *
 * @param {HeapAnalysesClient} heapWorker
 */
exports.refreshIndividuals = function(heapWorker) {
  return async function(dispatch, getState) {
    assert(getState().view.state === viewState.INDIVIDUALS,
           "Should be in INDIVIDUALS view.");

    const { individuals } = getState();

    switch (individuals.state) {
      case individualsState.COMPUTING_DOMINATOR_TREE:
      case individualsState.FETCHING:
        // Nothing to do here.
        return;

      case individualsState.FETCHED:
        if (getState().individuals.labelDisplay === getState().labelDisplay) {
          return;
        }
        break;

      case individualsState.ERROR:
        // Doesn't hurt to retry: maybe we won't get an error this time around?
        break;

      default:
        assert(false, `Unexpected individuals state: ${individuals.state}`);
        return;
    }

    await dispatch(fetchIndividuals(heapWorker,
                                    individuals.id,
                                    individuals.censusBreakdown,
                                    individuals.indices));
  };
};

/**
 * Refresh the selected snapshot's census data, if need be (for example,
 * display configuration changed).
 *
 * @param {HeapAnalysesClient} heapWorker
 */
exports.refreshSelectedCensus = function(heapWorker) {
  return async function(dispatch, getState) {
    const snapshot = getState().snapshots.find(s => s.selected);
    if (!snapshot || snapshot.state !== states.READ) {
      return;
    }

    // Intermediate snapshot states will get handled by the task action that is
    // orchestrating them. For example, if the snapshot census's state is
    // SAVING, then the takeCensus action will keep taking a census until
    // the inverted property matches the inverted state. If the snapshot is
    // still in the process of being saved or read, the takeSnapshotAndCensus
    // task action will follow through and ensure that a census is taken.
    if ((snapshot.census && snapshot.census.state === censusState.SAVED) ||
        !snapshot.census) {
      await dispatch(takeCensus(heapWorker, snapshot.id));
    }
  };
};

/**
 * Refresh the selected snapshot's tree map data, if need be (for example,
 * display configuration changed).
 *
 * @param {HeapAnalysesClient} heapWorker
 */
exports.refreshSelectedTreeMap = function(heapWorker) {
  return async function(dispatch, getState) {
    const snapshot = getState().snapshots.find(s => s.selected);
    if (!snapshot || snapshot.state !== states.READ) {
      return;
    }

    // Intermediate snapshot states will get handled by the task action that is
    // orchestrating them. For example, if the snapshot census's state is
    // SAVING, then the takeCensus action will keep taking a census until
    // the inverted property matches the inverted state. If the snapshot is
    // still in the process of being saved or read, the takeSnapshotAndCensus
    // task action will follow through and ensure that a census is taken.
    if ((snapshot.treeMap && snapshot.treeMap.state === treeMapState.SAVED) ||
        !snapshot.treeMap) {
      await dispatch(takeTreeMap(heapWorker, snapshot.id));
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
const computeDominatorTree = exports.computeDominatorTree =
TaskCache.declareCacheableTask({
  getCacheKey(_, id) {
    return id;
  },

  task: async function(heapWorker, id, removeFromCache, dispatch, getState) {
    const snapshot = getSnapshot(getState(), id);
    assert(!(snapshot.dominatorTree && snapshot.dominatorTree.dominatorTreeId),
           "Should not re-compute dominator trees");

    dispatch({ type: actions.COMPUTE_DOMINATOR_TREE_START, id });

    let dominatorTreeId;
    try {
      dominatorTreeId = await heapWorker.computeDominatorTree(snapshot.path);
    } catch (error) {
      removeFromCache();
      reportException("actions/snapshot/computeDominatorTree", error);
      dispatch({ type: actions.DOMINATOR_TREE_ERROR, id, error });
      return null;
    }

    removeFromCache();
    dispatch({ type: actions.COMPUTE_DOMINATOR_TREE_END, id, dominatorTreeId });
    return dominatorTreeId;
  }
});

/**
 * Get the partial subtree, starting from the root, of the
 * snapshot-with-the-given-id's dominator tree.
 *
 * @param {HeapAnalysesClient} heapWorker
 * @param {SnapshotId} id
 *
 * @returns {Promise<DominatorTreeNode>}
 */
const fetchDominatorTree = exports.fetchDominatorTree =
TaskCache.declareCacheableTask({
  getCacheKey(_, id) {
    return id;
  },

  task: async function(heapWorker, id, removeFromCache, dispatch, getState) {
    const snapshot = getSnapshot(getState(), id);
    assert(dominatorTreeIsComputed(snapshot),
           "Should have dominator tree model and it should be computed");

    let display;
    let root;
    do {
      display = getState().labelDisplay;
      assert(display && display.breakdown,
             `Should have a breakdown to describe nodes with, got: ${uneval(display)}`);

      dispatch({ type: actions.FETCH_DOMINATOR_TREE_START, id, display });

      try {
        root = await heapWorker.getDominatorTree({
          dominatorTreeId: snapshot.dominatorTree.dominatorTreeId,
          breakdown: display.breakdown,
          maxRetainingPaths: Preferences.get("devtools.memory.max-retaining-paths"),
        });
      } catch (error) {
        removeFromCache();
        reportException("actions/snapshot/fetchDominatorTree", error);
        dispatch({ type: actions.DOMINATOR_TREE_ERROR, id, error });
        return null;
      }
    }
    while (display !== getState().labelDisplay);

    removeFromCache();
    dispatch({ type: actions.FETCH_DOMINATOR_TREE_END, id, root });
    return root;
  }
});

/**
 * Fetch the immediately dominated children represented by the placeholder
 * `lazyChildren` from snapshot-with-the-given-id's dominator tree.
 *
 * @param {HeapAnalysesClient} heapWorker
 * @param {SnapshotId} id
 * @param {DominatorTreeLazyChildren} lazyChildren
 */
exports.fetchImmediatelyDominated = TaskCache.declareCacheableTask({
  getCacheKey(_, id, lazyChildren) {
    return `${id}-${lazyChildren.key()}`;
  },

  task: async function(heapWorker, id, lazyChildren, removeFromCache, dispatch,
                        getState) {
    const snapshot = getSnapshot(getState(), id);
    assert(snapshot.dominatorTree, "Should have dominator tree model");
    assert(snapshot.dominatorTree.state === dominatorTreeState.LOADED ||
           snapshot.dominatorTree.state === dominatorTreeState.INCREMENTAL_FETCHING,
           "Cannot fetch immediately dominated nodes in a dominator tree unless " +
           " the dominator tree has already been computed");

    let display;
    let response;
    do {
      display = getState().labelDisplay;
      assert(display, "Should have a display to describe nodes with.");

      dispatch({ type: actions.FETCH_IMMEDIATELY_DOMINATED_START, id });

      try {
        response = await heapWorker.getImmediatelyDominated({
          dominatorTreeId: snapshot.dominatorTree.dominatorTreeId,
          breakdown: display.breakdown,
          nodeId: lazyChildren.parentNodeId(),
          startIndex: lazyChildren.siblingIndex(),
          maxRetainingPaths: Preferences.get("devtools.memory.max-retaining-paths"),
        });
      } catch (error) {
        removeFromCache();
        reportException("actions/snapshot/fetchImmediatelyDominated", error);
        dispatch({ type: actions.DOMINATOR_TREE_ERROR, id, error });
        return;
      }
    }
    while (display !== getState().labelDisplay);

    removeFromCache();
    dispatch({
      type: actions.FETCH_IMMEDIATELY_DOMINATED_END,
      id,
      path: response.path,
      nodes: response.nodes,
      moreChildrenAvailable: response.moreChildrenAvailable,
    });
  }
});

/**
 * Compute and then fetch the dominator tree of the snapshot with the given
 * `id`.
 *
 * @param {HeapAnalysesClient} heapWorker
 * @param {SnapshotId} id
 *
 * @returns {Promise<DominatorTreeNode>}
 */
const computeAndFetchDominatorTree = exports.computeAndFetchDominatorTree =
TaskCache.declareCacheableTask({
  getCacheKey(_, id) {
    return id;
  },

  task: async function(heapWorker, id, removeFromCache, dispatch, getState) {
    const dominatorTreeId = await dispatch(computeDominatorTree(heapWorker, id));
    if (dominatorTreeId === null) {
      removeFromCache();
      return null;
    }

    const root = await dispatch(fetchDominatorTree(heapWorker, id));
    removeFromCache();

    if (!root) {
      return null;
    }

    return root;
  }
});

/**
 * Update the currently selected snapshot's dominator tree.
 *
 * @param {HeapAnalysesClient} heapWorker
 */
exports.refreshSelectedDominatorTree = function(heapWorker) {
  return async function(dispatch, getState) {
    const snapshot = getState().snapshots.find(s => s.selected);
    if (!snapshot) {
      return;
    }

    if (snapshot.dominatorTree &&
        !(snapshot.dominatorTree.state === dominatorTreeState.COMPUTED ||
          snapshot.dominatorTree.state === dominatorTreeState.LOADED ||
          snapshot.dominatorTree.state === dominatorTreeState.INCREMENTAL_FETCHING)) {
      return;
    }

    // We need to check for the snapshot state because if there was an error,
    // we can't continue and if we are still saving or reading the snapshot,
    // then takeSnapshotAndCensus will finish the job for us
    if (snapshot.state === states.READ) {
      if (snapshot.dominatorTree) {
        await dispatch(fetchDominatorTree(heapWorker, snapshot.id));
      } else {
        await dispatch(computeAndFetchDominatorTree(heapWorker, snapshot.id));
      }
    }
  };
};

/**
 * Select the snapshot with the given id.
 *
 * @param {snapshotId} id
 * @see {Snapshot} model defined in devtools/client/memory/models.js
 */
const selectSnapshot = exports.selectSnapshot = function(id) {
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
exports.clearSnapshots = function(heapWorker) {
  return async function(dispatch, getState) {
    const snapshots = getState().snapshots.filter(s => {
      const snapshotReady = s.state === states.READ || s.state === states.ERROR;
      const censusReady = (s.treeMap && s.treeMap.state === treeMapState.SAVED) ||
                        (s.census && s.census.state === censusState.SAVED);

      return snapshotReady && censusReady;
    });

    const ids = snapshots.map(s => s.id);

    dispatch({ type: actions.DELETE_SNAPSHOTS_START, ids });

    if (getState().diffing) {
      dispatch(diffing.toggleDiffing());
    }
    if (getState().individuals) {
      dispatch(view.popView());
    }

    await Promise.all(snapshots.map(snapshot => {
      return heapWorker.deleteHeapSnapshot(snapshot.path).catch(error => {
        reportException("clearSnapshots", error);
        dispatch({ type: actions.SNAPSHOT_ERROR, id: snapshot.id, error });
      });
    }));

    dispatch({ type: actions.DELETE_SNAPSHOTS_END, ids });
  };
};

/**
 * Delete a snapshot
 *
 * @param {HeapAnalysesClient} heapWorker
 * @param {snapshotModel} snapshot
 */
exports.deleteSnapshot = function(heapWorker, snapshot) {
  return async function(dispatch, getState) {
    dispatch({ type: actions.DELETE_SNAPSHOTS_START, ids: [snapshot.id] });

    try {
      await heapWorker.deleteHeapSnapshot(snapshot.path);
    } catch (error) {
      reportException("deleteSnapshot", error);
      dispatch({ type: actions.SNAPSHOT_ERROR, id: snapshot.id, error });
    }

    dispatch({ type: actions.DELETE_SNAPSHOTS_END, ids: [snapshot.id] });
  };
};

/**
 * Expand the given node in the snapshot's census report.
 *
 * @param {CensusTreeNode} node
 */
exports.expandCensusNode = function(id, node) {
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
exports.collapseCensusNode = function(id, node) {
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
exports.focusCensusNode = function(id, node) {
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
exports.expandDominatorTreeNode = function(id, node) {
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
exports.collapseDominatorTreeNode = function(id, node) {
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
exports.focusDominatorTreeNode = function(id, node) {
  return {
    type: actions.FOCUS_DOMINATOR_TREE_NODE,
    id,
    node,
  };
};
