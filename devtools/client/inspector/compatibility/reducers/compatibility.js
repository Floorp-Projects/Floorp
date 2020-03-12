/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const {
  COMPATIBILITY_APPEND_NODE,
  COMPATIBILITY_UPDATE_SELECTED_NODE_SUCCESS,
  COMPATIBILITY_UPDATE_SELECTED_NODE_FAILURE,
  COMPATIBILITY_UPDATE_SELECTED_NODE_ISSUES,
  COMPATIBILITY_UPDATE_TARGET_BROWSERS_SUCCESS,
  COMPATIBILITY_UPDATE_TARGET_BROWSERS_FAILURE,
  COMPATIBILITY_UPDATE_TOP_LEVEL_TARGET_START,
  COMPATIBILITY_UPDATE_TOP_LEVEL_TARGET_SUCCESS,
  COMPATIBILITY_UPDATE_TOP_LEVEL_TARGET_FAILURE,
} = require("devtools/client/inspector/compatibility/actions/index");

const INITIAL_STATE = {
  selectedNode: null,
  selectedNodeIssues: [],
  topLevelTarget: null,
  topLevelTargetIssues: [],
  targetBrowsers: [],
};

const reducers = {
  [COMPATIBILITY_APPEND_NODE](state, { node, issues }) {
    const topLevelTargetIssues = [...state.topLevelTargetIssues];
    for (const issue of issues) {
      const index = topLevelTargetIssues.findIndex(
        i => i.type === issue.type && i.property === issue.property
      );

      if (index < 0) {
        issue.nodes = [node];
        topLevelTargetIssues.push(issue);
      } else {
        const topLevelTargetIssue = topLevelTargetIssues[index];
        topLevelTargetIssue.nodes = [...topLevelTargetIssue.nodes, node];
      }
    }
    return Object.assign({}, state, { topLevelTargetIssues });
  },
  [COMPATIBILITY_UPDATE_SELECTED_NODE_SUCCESS](state, { node }) {
    return Object.assign({}, state, { selectedNode: node });
  },
  [COMPATIBILITY_UPDATE_SELECTED_NODE_FAILURE](state, { error }) {
    _showError(COMPATIBILITY_UPDATE_SELECTED_NODE_FAILURE, error);
    return state;
  },
  [COMPATIBILITY_UPDATE_SELECTED_NODE_ISSUES](state, { issues }) {
    return Object.assign({}, state, { selectedNodeIssues: issues });
  },
  [COMPATIBILITY_UPDATE_TARGET_BROWSERS_SUCCESS](state, { targetBrowsers }) {
    return Object.assign({}, state, { targetBrowsers });
  },
  [COMPATIBILITY_UPDATE_TARGET_BROWSERS_FAILURE](state, { error }) {
    _showError(COMPATIBILITY_UPDATE_TARGET_BROWSERS_FAILURE, error);
    return state;
  },
  [COMPATIBILITY_UPDATE_TOP_LEVEL_TARGET_START](state) {
    return Object.assign({}, state, { topLevelTargetIssues: [] });
  },
  [COMPATIBILITY_UPDATE_TOP_LEVEL_TARGET_SUCCESS](state, { target }) {
    return Object.assign({}, state, { topLevelTarget: target });
  },
  [COMPATIBILITY_UPDATE_TOP_LEVEL_TARGET_FAILURE](state, { error }) {
    _showError(COMPATIBILITY_UPDATE_TOP_LEVEL_TARGET_FAILURE, error);
    return state;
  },
};

function _showError(action, error) {
  console.error(`[${action}] ${error.message}`);
  console.error(error.stack);
}

module.exports = function(state = INITIAL_STATE, action) {
  const reducer = reducers[action.type];
  return reducer ? reducer(state, action) : state;
};
