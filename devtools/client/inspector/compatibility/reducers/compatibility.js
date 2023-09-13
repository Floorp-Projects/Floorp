/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const {
  COMPATIBILITY_APPEND_NODE_FAILURE,
  COMPATIBILITY_CLEAR_DESTROYED_NODES,
  COMPATIBILITY_INIT_USER_SETTINGS_SUCCESS,
  COMPATIBILITY_INIT_USER_SETTINGS_FAILURE,
  COMPATIBILITY_INTERNAL_APPEND_NODE,
  COMPATIBILITY_INTERNAL_NODE_UPDATE,
  COMPATIBILITY_INTERNAL_REMOVE_NODE,
  COMPATIBILITY_INTERNAL_UPDATE_SELECTED_NODE_ISSUES,
  COMPATIBILITY_REMOVE_NODE_FAILURE,
  COMPATIBILITY_UPDATE_NODE_FAILURE,
  COMPATIBILITY_UPDATE_NODES_FAILURE,
  COMPATIBILITY_UPDATE_SELECTED_NODE_SUCCESS,
  COMPATIBILITY_UPDATE_SELECTED_NODE_FAILURE,
  COMPATIBILITY_UPDATE_SETTINGS_VISIBILITY,
  COMPATIBILITY_UPDATE_TARGET_BROWSERS_START,
  COMPATIBILITY_UPDATE_TARGET_BROWSERS_SUCCESS,
  COMPATIBILITY_UPDATE_TARGET_BROWSERS_FAILURE,
  COMPATIBILITY_UPDATE_TARGET_BROWSERS_COMPLETE,
  COMPATIBILITY_UPDATE_TOP_LEVEL_TARGET_START,
  COMPATIBILITY_UPDATE_TOP_LEVEL_TARGET_SUCCESS,
  COMPATIBILITY_UPDATE_TOP_LEVEL_TARGET_FAILURE,
  COMPATIBILITY_UPDATE_TOP_LEVEL_TARGET_COMPLETE,
} = require("resource://devtools/client/inspector/compatibility/actions/index.js");

const INITIAL_STATE = {
  defaultTargetBrowsers: [],
  isSettingsVisible: false,
  isTopLevelTargetProcessing: false,
  selectedNode: null,
  selectedNodeIssues: [],
  topLevelTarget: null,
  topLevelTargetIssues: [],
  targetBrowsers: [],
};

const reducers = {
  [COMPATIBILITY_APPEND_NODE_FAILURE](state, { error }) {
    _showError(COMPATIBILITY_APPEND_NODE_FAILURE, error);
    return state;
  },
  [COMPATIBILITY_CLEAR_DESTROYED_NODES](state) {
    const topLevelTargetIssues = _clearDestroyedNodes(
      state.topLevelTargetIssues
    );
    return Object.assign({}, state, { topLevelTargetIssues });
  },
  [COMPATIBILITY_INIT_USER_SETTINGS_SUCCESS](
    state,
    { defaultTargetBrowsers, targetBrowsers }
  ) {
    return Object.assign({}, state, { defaultTargetBrowsers, targetBrowsers });
  },
  [COMPATIBILITY_INIT_USER_SETTINGS_FAILURE](state, { error }) {
    _showError(COMPATIBILITY_INIT_USER_SETTINGS_FAILURE, error);
    return state;
  },
  [COMPATIBILITY_INTERNAL_APPEND_NODE](state, { node, issues }) {
    const topLevelTargetIssues = _appendTopLevelTargetIssues(
      state.topLevelTargetIssues,
      node,
      issues
    );
    return Object.assign({}, state, { topLevelTargetIssues });
  },
  [COMPATIBILITY_INTERNAL_NODE_UPDATE](state, { node, issues }) {
    const topLevelTargetIssues = _updateTopLevelTargetIssues(
      state.topLevelTargetIssues,
      node,
      issues
    );
    return Object.assign({}, state, { topLevelTargetIssues });
  },
  [COMPATIBILITY_INTERNAL_REMOVE_NODE](state, { node }) {
    const topLevelTargetIssues = _removeNodeOrIssues(
      state.topLevelTargetIssues,
      node,
      []
    );
    return Object.assign({}, state, { topLevelTargetIssues });
  },
  [COMPATIBILITY_INTERNAL_UPDATE_SELECTED_NODE_ISSUES](state, { issues }) {
    return Object.assign({}, state, { selectedNodeIssues: issues });
  },
  [COMPATIBILITY_REMOVE_NODE_FAILURE](state, { error }) {
    _showError(COMPATIBILITY_UPDATE_NODES_FAILURE, error);
    return state;
  },
  [COMPATIBILITY_UPDATE_NODE_FAILURE](state, { error }) {
    _showError(COMPATIBILITY_UPDATE_NODES_FAILURE, error);
    return state;
  },
  [COMPATIBILITY_UPDATE_NODES_FAILURE](state, { error }) {
    _showError(COMPATIBILITY_UPDATE_NODES_FAILURE, error);
    return state;
  },
  [COMPATIBILITY_UPDATE_SELECTED_NODE_SUCCESS](state, { node }) {
    return Object.assign({}, state, { selectedNode: node });
  },
  [COMPATIBILITY_UPDATE_SELECTED_NODE_FAILURE](state, { error }) {
    _showError(COMPATIBILITY_UPDATE_SELECTED_NODE_FAILURE, error);
    return state;
  },
  [COMPATIBILITY_UPDATE_SETTINGS_VISIBILITY](state, { visibility }) {
    return Object.assign({}, state, { isSettingsVisible: visibility });
  },
  [COMPATIBILITY_UPDATE_TARGET_BROWSERS_START](state) {
    return Object.assign({}, state, {
      isTopLevelTargetProcessing: true,
      topLevelTargetIssues: [],
    });
  },
  [COMPATIBILITY_UPDATE_TARGET_BROWSERS_SUCCESS](state, { targetBrowsers }) {
    return Object.assign({}, state, { targetBrowsers });
  },
  [COMPATIBILITY_UPDATE_TARGET_BROWSERS_FAILURE](state, { error }) {
    _showError(COMPATIBILITY_UPDATE_TARGET_BROWSERS_FAILURE, error);
    return state;
  },
  [COMPATIBILITY_UPDATE_TARGET_BROWSERS_COMPLETE](state) {
    return Object.assign({}, state, { isTopLevelTargetProcessing: false });
  },
  [COMPATIBILITY_UPDATE_TOP_LEVEL_TARGET_START](state) {
    return Object.assign({}, state, {
      isTopLevelTargetProcessing: true,
      topLevelTargetIssues: [],
    });
  },
  [COMPATIBILITY_UPDATE_TOP_LEVEL_TARGET_SUCCESS](state, { target }) {
    return Object.assign({}, state, { topLevelTarget: target });
  },
  [COMPATIBILITY_UPDATE_TOP_LEVEL_TARGET_FAILURE](state, { error }) {
    _showError(COMPATIBILITY_UPDATE_TOP_LEVEL_TARGET_FAILURE, error);
    return state;
  },
  [COMPATIBILITY_UPDATE_TOP_LEVEL_TARGET_COMPLETE](state, { target }) {
    return Object.assign({}, state, { isTopLevelTargetProcessing: false });
  },
};

function _appendTopLevelTargetIssues(targetIssues, node, issues) {
  targetIssues = [...targetIssues];
  for (const issue of issues) {
    const index = _indexOfIssue(targetIssues, issue);
    if (index < 0) {
      issue.nodes = [node];
      targetIssues.push(issue);
      continue;
    }

    const targetIssue = targetIssues[index];
    const nodeIndex = targetIssue.nodes.findIndex(
      issueNode => issueNode.actorID === node.actorID
    );

    if (nodeIndex < 0) {
      targetIssue.nodes = [...targetIssue.nodes, node];
    }
  }
  return targetIssues;
}

function _clearDestroyedNodes(targetIssues) {
  return targetIssues.reduce((newIssues, targetIssue) => {
    const retainedNodes = targetIssue.nodes.filter(
      n => n.targetFront && !n.targetFront.isDestroyed()
    );

    // All the nodes for a given issue are destroyed
    if (retainedNodes.length === 0) {
      // Remove issue.
      return newIssues;
    }

    targetIssue.nodes = retainedNodes;
    return [...newIssues, targetIssue];
  }, []);
}

function _indexOfIssue(issues, issue) {
  return issues.findIndex(
    i => i.type === issue.type && i.property === issue.property
  );
}

function _indexOfNode(issue, node) {
  return issue.nodes.findIndex(n => n.actorID === node.actorID);
}

function _removeNodeOrIssues(targetIssues, node, issues) {
  return targetIssues.reduce((newIssues, targetIssue) => {
    if (issues.length && _indexOfIssue(issues, targetIssue) >= 0) {
      // The targetIssue is still in the node.
      return [...newIssues, targetIssue];
    }

    const indexOfNodeInTarget = _indexOfNode(targetIssue, node);
    if (indexOfNodeInTarget < 0) {
      // The targetIssue does not have the node to remove.
      return [...newIssues, targetIssue];
    }

    // This issue on the updated node is gone.
    if (targetIssue.nodes.length === 1) {
      // Remove issue.
      return newIssues;
    }

    // Remove node from the nodes.
    targetIssue.nodes = [
      ...targetIssue.nodes.slice(0, indexOfNodeInTarget),
      ...targetIssue.nodes.slice(indexOfNodeInTarget + 1),
    ];
    return [...newIssues, targetIssue];
  }, []);
}

function _updateTopLevelTargetIssues(targetIssues, node, issues) {
  // Remove issues or node.
  targetIssues = _removeNodeOrIssues(targetIssues, node, issues);

  // Append issues or node.
  const appendables = issues.filter(issue => {
    const indexOfIssue = _indexOfIssue(targetIssues, issue);
    return (
      indexOfIssue < 0 || _indexOfNode(targetIssues[indexOfIssue], node) < 0
    );
  });
  targetIssues = _appendTopLevelTargetIssues(targetIssues, node, appendables);

  // Update fields.
  for (const issue of issues) {
    const indexOfIssue = _indexOfIssue(targetIssues, issue);

    if (indexOfIssue < 0) {
      continue;
    }

    const targetIssue = targetIssues[indexOfIssue];
    targetIssues[indexOfIssue] = Object.assign(issue, {
      nodes: targetIssue.nodes,
    });
  }

  return targetIssues;
}

function _showError(action, error) {
  console.error(`[${action}] ${error.message}`);
  console.error(error.stack);
}

module.exports = function (state = INITIAL_STATE, action) {
  const reducer = reducers[action.type];
  return reducer ? reducer(state, action) : state;
};
