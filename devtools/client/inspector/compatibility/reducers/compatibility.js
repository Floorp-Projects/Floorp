/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

loader.lazyGetter(this, "mdnCompatibility", () => {
  const MDNCompatibility = require("devtools/client/inspector/compatibility/lib/MDNCompatibility");
  const cssPropertiesCompatData = require("devtools/client/inspector/compatibility/lib/dataset/css-properties.json");
  return new MDNCompatibility(cssPropertiesCompatData);
});

const {
  COMPATIBILITY_UPDATE_SELECTED_NODE_SUCCESS,
  COMPATIBILITY_UPDATE_SELECTED_NODE_FAILURE,
  COMPATIBILITY_UPDATE_TARGET_BROWSERS,
} = require("devtools/client/inspector/compatibility/actions/index");

const INITIAL_STATE = {
  selectedNodeIssues: [],
  declarationBlocks: [],
  targetBrowsers: [],
};

const reducers = {
  [COMPATIBILITY_UPDATE_SELECTED_NODE_SUCCESS](state, data) {
    return updateSelectedNodeIssues(state, data);
  },
  [COMPATIBILITY_UPDATE_SELECTED_NODE_FAILURE](state, { error }) {
    console.error(
      `[COMPATIBILITY_UPDATE_SELECTED_NODE_FAILURE] ${error.message}`
    );
    console.error(error.stack);
    return state;
  },
  [COMPATIBILITY_UPDATE_TARGET_BROWSERS](state, data) {
    return updateSelectedNodeIssues(state, data);
  },
};

function updateSelectedNodeIssues(state, data) {
  const declarationBlocks = data.declarationBlocks || state.declarationBlocks;
  const targetBrowsers = data.targetBrowsers || state.targetBrowsers;

  const selectedNodeIssues = [];
  for (const declarationBlock of declarationBlocks) {
    selectedNodeIssues.push(
      ...mdnCompatibility.getCSSDeclarationBlockIssues(
        declarationBlock,
        targetBrowsers
      )
    );
  }

  return { selectedNodeIssues, declarationBlocks, targetBrowsers };
}

module.exports = function(state = INITIAL_STATE, action) {
  const reducer = reducers[action.type];
  return reducer ? reducer(state, action) : state;
};
