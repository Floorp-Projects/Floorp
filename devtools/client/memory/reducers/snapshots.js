/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
"use strict";

const Immutable = require("devtools/client/shared/vendor/immutable");
const { immutableUpdate, assert } = require("devtools/shared/DevToolsUtils");
const {
  actions,
  snapshotState: states,
  censusState,
  treeMapState,
  dominatorTreeState,
  viewState,
} = require("../constants");
const DominatorTreeNode = require("devtools/shared/heapsnapshot/DominatorTreeNode");

const handlers = Object.create(null);

handlers[actions.SNAPSHOT_ERROR] = function (snapshots, { id, error }) {
  return snapshots.map(snapshot => {
    return snapshot.id === id
      ? immutableUpdate(snapshot, { state: states.ERROR, error })
      : snapshot;
  });
};

handlers[actions.TAKE_SNAPSHOT_START] = function (snapshots, { snapshot }) {
  return [...snapshots, snapshot];
};

handlers[actions.TAKE_SNAPSHOT_END] = function (snapshots, { id, path }) {
  return snapshots.map(snapshot => {
    return snapshot.id === id
      ? immutableUpdate(snapshot, { state: states.SAVED, path })
      : snapshot;
  });
};

handlers[actions.IMPORT_SNAPSHOT_START] = handlers[actions.TAKE_SNAPSHOT_START];

handlers[actions.READ_SNAPSHOT_START] = function (snapshots, { id }) {
  return snapshots.map(snapshot => {
    return snapshot.id === id
      ? immutableUpdate(snapshot, { state: states.READING })
      : snapshot;
  });
};

handlers[actions.READ_SNAPSHOT_END] = function (snapshots, { id, creationTime }) {
  return snapshots.map(snapshot => {
    return snapshot.id === id
      ? immutableUpdate(snapshot, { state: states.READ, creationTime })
      : snapshot;
  });
};

handlers[actions.TAKE_CENSUS_START] = function (snapshots, { id, display, filter }) {
  const census = {
    report: null,
    display,
    filter,
    state: censusState.SAVING
  };

  return snapshots.map(snapshot => {
    return snapshot.id === id
      ? immutableUpdate(snapshot, { census })
      : snapshot;
  });
};

handlers[actions.TAKE_CENSUS_END] = function (snapshots, { id,
                                                           report,
                                                           parentMap,
                                                           display,
                                                           filter }) {
  const census = {
    report,
    parentMap,
    expanded: Immutable.Set(),
    display,
    filter,
    state: censusState.SAVED
  };

  return snapshots.map(snapshot => {
    return snapshot.id === id
      ? immutableUpdate(snapshot, { census })
      : snapshot;
  });
};

handlers[actions.TAKE_CENSUS_ERROR] = function (snapshots, { id, error }) {
  assert(error, "actions with TAKE_CENSUS_ERROR should have an error");

  return snapshots.map(snapshot => {
    if (snapshot.id !== id) {
      return snapshot;
    }

    const census = Object.freeze({
      state: censusState.ERROR,
      error,
    });

    return immutableUpdate(snapshot, { census });
  });
};

handlers[actions.TAKE_TREE_MAP_START] = function (snapshots, { id, display }) {
  const treeMap = {
    report: null,
    display,
    state: treeMapState.SAVING
  };

  return snapshots.map(snapshot => {
    return snapshot.id === id
      ? immutableUpdate(snapshot, { treeMap })
      : snapshot;
  });
};

handlers[actions.TAKE_TREE_MAP_END] = function (snapshots, action) {
  const { id, report, display } = action;
  const treeMap = {
    report,
    display,
    state: treeMapState.SAVED
  };

  return snapshots.map(snapshot => {
    return snapshot.id === id
      ? immutableUpdate(snapshot, { treeMap })
      : snapshot;
  });
};

handlers[actions.TAKE_TREE_MAP_ERROR] = function (snapshots, { id, error }) {
  assert(error, "actions with TAKE_TREE_MAP_ERROR should have an error");

  return snapshots.map(snapshot => {
    if (snapshot.id !== id) {
      return snapshot;
    }

    const treeMap = Object.freeze({
      state: treeMapState.ERROR,
      error,
    });

    return immutableUpdate(snapshot, { treeMap });
  });
};

handlers[actions.EXPAND_CENSUS_NODE] = function (snapshots, { id, node }) {
  return snapshots.map(snapshot => {
    if (snapshot.id !== id) {
      return snapshot;
    }

    assert(snapshot.census, "Should have a census");
    assert(snapshot.census.report, "Should have a census report");
    assert(snapshot.census.expanded, "Should have a census's expanded set");

    const expanded = snapshot.census.expanded.add(node.id);
    const census = immutableUpdate(snapshot.census, { expanded });
    return immutableUpdate(snapshot, { census });
  });
};

handlers[actions.COLLAPSE_CENSUS_NODE] = function (snapshots, { id, node }) {
  return snapshots.map(snapshot => {
    if (snapshot.id !== id) {
      return snapshot;
    }

    assert(snapshot.census, "Should have a census");
    assert(snapshot.census.report, "Should have a census report");
    assert(snapshot.census.expanded, "Should have a census's expanded set");

    const expanded = snapshot.census.expanded.delete(node.id);
    const census = immutableUpdate(snapshot.census, { expanded });
    return immutableUpdate(snapshot, { census });
  });
};

handlers[actions.FOCUS_CENSUS_NODE] = function (snapshots, { id, node }) {
  return snapshots.map(snapshot => {
    if (snapshot.id !== id) {
      return snapshot;
    }

    assert(snapshot.census, "Should have a census");
    const census = immutableUpdate(snapshot.census, { focused: node });
    return immutableUpdate(snapshot, { census });
  });
};

handlers[actions.SELECT_SNAPSHOT] = function (snapshots, { id }) {
  return snapshots.map(s => immutableUpdate(s, { selected: s.id === id }));
};

handlers[actions.DELETE_SNAPSHOTS_START] = function (snapshots, { ids }) {
  return snapshots.filter(s => ids.indexOf(s.id) === -1);
};

handlers[actions.DELETE_SNAPSHOTS_END] = function (snapshots) {
  return snapshots;
};

handlers[actions.CHANGE_VIEW] = function (snapshots, { newViewState }) {
  return newViewState === viewState.DIFFING
    ? snapshots.map(s => immutableUpdate(s, { selected: false }))
    : snapshots;
};

handlers[actions.POP_VIEW] = function (snapshots, { previousView }) {
  return snapshots.map(s => immutableUpdate(s, {
    selected: s.id === previousView.selected
  }));
};

handlers[actions.COMPUTE_DOMINATOR_TREE_START] = function (snapshots, { id }) {
  const dominatorTree = Object.freeze({
    state: dominatorTreeState.COMPUTING,
    dominatorTreeId: undefined,
    root: undefined,
  });

  return snapshots.map(snapshot => {
    if (snapshot.id !== id) {
      return snapshot;
    }

    assert(!snapshot.dominatorTree,
           "Should not have a dominator tree model");
    return immutableUpdate(snapshot, { dominatorTree });
  });
};

handlers[actions.COMPUTE_DOMINATOR_TREE_END] = function (snapshots, { id, dominatorTreeId }) {
  return snapshots.map(snapshot => {
    if (snapshot.id !== id) {
      return snapshot;
    }

    assert(snapshot.dominatorTree, "Should have a dominator tree model");
    assert(snapshot.dominatorTree.state == dominatorTreeState.COMPUTING,
           "Should be in the COMPUTING state");

    const dominatorTree = immutableUpdate(snapshot.dominatorTree, {
      state: dominatorTreeState.COMPUTED,
      dominatorTreeId,
    });
    return immutableUpdate(snapshot, { dominatorTree });
  });
};

handlers[actions.FETCH_DOMINATOR_TREE_START] = function (snapshots, { id, display }) {
  return snapshots.map(snapshot => {
    if (snapshot.id !== id) {
      return snapshot;
    }

    assert(snapshot.dominatorTree, "Should have a dominator tree model");
    assert(snapshot.dominatorTree.state !== dominatorTreeState.COMPUTING &&
           snapshot.dominatorTree.state !== dominatorTreeState.ERROR,
           `Should have already computed the dominator tree, found state = ${snapshot.dominatorTree.state}`);

    const dominatorTree = immutableUpdate(snapshot.dominatorTree, {
      state: dominatorTreeState.FETCHING,
      root: undefined,
      display,
    });
    return immutableUpdate(snapshot, { dominatorTree });
  });
};

handlers[actions.FETCH_DOMINATOR_TREE_END] = function (snapshots, { id, root }) {
  return snapshots.map(snapshot => {
    if (snapshot.id !== id) {
      return snapshot;
    }

    assert(snapshot.dominatorTree, "Should have a dominator tree model");
    assert(snapshot.dominatorTree.state == dominatorTreeState.FETCHING,
           "Should be in the FETCHING state");

    let focused;
    if (snapshot.dominatorTree.focused) {
      focused = (function findFocused(node) {
        if (node.nodeId === snapshot.dominatorTree.focused.nodeId) {
          return node;
        }

        if (node.children) {
          const length = node.children.length;
          for (let i = 0; i < length; i++) {
            const result = findFocused(node.children[i]);
            if (result) {
              return result;
            }
          }
        }

        return undefined;
      }(root));
    }

    const dominatorTree = immutableUpdate(snapshot.dominatorTree, {
      state: dominatorTreeState.LOADED,
      root,
      expanded: Immutable.Set(),
      focused,
    });

    return immutableUpdate(snapshot, { dominatorTree });
  });
};

handlers[actions.EXPAND_DOMINATOR_TREE_NODE] = function (snapshots, { id, node }) {
  return snapshots.map(snapshot => {
    if (snapshot.id !== id) {
      return snapshot;
    }

    assert(snapshot.dominatorTree, "Should have a dominator tree");
    assert(snapshot.dominatorTree.expanded,
           "Should have the dominator tree's expanded set");

    const expanded = snapshot.dominatorTree.expanded.add(node.nodeId);
    const dominatorTree = immutableUpdate(snapshot.dominatorTree, { expanded });
    return immutableUpdate(snapshot, { dominatorTree });
  });
};

handlers[actions.COLLAPSE_DOMINATOR_TREE_NODE] = function (snapshots, { id, node }) {
  return snapshots.map(snapshot => {
    if (snapshot.id !== id) {
      return snapshot;
    }

    assert(snapshot.dominatorTree, "Should have a dominator tree");
    assert(snapshot.dominatorTree.expanded,
           "Should have the dominator tree's expanded set");

    const expanded = snapshot.dominatorTree.expanded.delete(node.nodeId);
    const dominatorTree = immutableUpdate(snapshot.dominatorTree, { expanded });
    return immutableUpdate(snapshot, { dominatorTree });
  });
};

handlers[actions.FOCUS_DOMINATOR_TREE_NODE] = function (snapshots, { id, node }) {
  return snapshots.map(snapshot => {
    if (snapshot.id !== id) {
      return snapshot;
    }

    assert(snapshot.dominatorTree, "Should have a dominator tree");
    const dominatorTree = immutableUpdate(snapshot.dominatorTree, { focused: node });
    return immutableUpdate(snapshot, { dominatorTree });
  });
};

handlers[actions.FETCH_IMMEDIATELY_DOMINATED_START] = function (snapshots, { id }) {
  return snapshots.map(snapshot => {
    if (snapshot.id !== id) {
      return snapshot;
    }

    assert(snapshot.dominatorTree, "Should have a dominator tree model");
    assert(snapshot.dominatorTree.state == dominatorTreeState.INCREMENTAL_FETCHING ||
           snapshot.dominatorTree.state == dominatorTreeState.LOADED,
           "The dominator tree should be loaded if we are going to " +
           "incrementally fetch children.");

    const activeFetchRequestCount = snapshot.dominatorTree.activeFetchRequestCount
      ? snapshot.dominatorTree.activeFetchRequestCount + 1
      : 1;

    const dominatorTree = immutableUpdate(snapshot.dominatorTree, {
      state: dominatorTreeState.INCREMENTAL_FETCHING,
      activeFetchRequestCount,
    });

    return immutableUpdate(snapshot, { dominatorTree });
  });
};

handlers[actions.FETCH_IMMEDIATELY_DOMINATED_END] =
  function (snapshots, { id, path, nodes, moreChildrenAvailable}) {
    return snapshots.map(snapshot => {
      if (snapshot.id !== id) {
        return snapshot;
      }

      assert(snapshot.dominatorTree, "Should have a dominator tree model");
      assert(snapshot.dominatorTree.root, "Should have a dominator tree model root");
      assert(snapshot.dominatorTree.state === dominatorTreeState.INCREMENTAL_FETCHING,
             "The dominator tree state should be INCREMENTAL_FETCHING");

      const root = DominatorTreeNode.insert(snapshot.dominatorTree.root,
                                            path,
                                            nodes,
                                            moreChildrenAvailable);

      const focused = snapshot.dominatorTree.focused
        ? DominatorTreeNode.getNodeByIdAlongPath(snapshot.dominatorTree.focused.nodeId,
                                                 root,
                                                 path)
        : undefined;

      const activeFetchRequestCount = snapshot.dominatorTree.activeFetchRequestCount === 1
        ? undefined
        : snapshot.dominatorTree.activeFetchRequestCount - 1;

      // If there are still outstanding requests, we need to stay in the
      // INCREMENTAL_FETCHING state until they complete.
      const state = activeFetchRequestCount
        ? dominatorTreeState.INCREMENTAL_FETCHING
        : dominatorTreeState.LOADED;

      const dominatorTree = immutableUpdate(snapshot.dominatorTree, {
        state,
        root,
        focused,
        activeFetchRequestCount,
      });

      return immutableUpdate(snapshot, { dominatorTree });
    });
  };

handlers[actions.DOMINATOR_TREE_ERROR] = function (snapshots, { id, error }) {
  assert(error, "actions with DOMINATOR_TREE_ERROR should have an error");

  return snapshots.map(snapshot => {
    if (snapshot.id !== id) {
      return snapshot;
    }

    const dominatorTree = Object.freeze({
      state: dominatorTreeState.ERROR,
      error,
    });

    return immutableUpdate(snapshot, { dominatorTree });
  });
};

module.exports = function (snapshots = [], action) {
  const handler = handlers[action.type];
  if (handler) {
    return handler(snapshots, action);
  }
  return snapshots;
};
