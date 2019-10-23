/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

loader.lazyGetter(this, "mdnCompatibility", () => {
  const MDNCompatibility = require("devtools/client/inspector/compatibility/lib/MDNCompatibility");
  const cssPropertiesCompatData = require("devtools/client/inspector/compatibility/lib/dataset/css-properties.json");
  return new MDNCompatibility(cssPropertiesCompatData);
});

const { COMPATIBILITY_UPDATE_SELECTED_NODE } = require("../actions/index");

const INITIAL_STATE = {
  selectedNodeIssues: [],
};

const reducers = {
  [COMPATIBILITY_UPDATE_SELECTED_NODE](state, { declarationBlocks }) {
    const issues = [];

    for (const declarationBlock of declarationBlocks) {
      issues.push(
        ...mdnCompatibility.getCSSDeclarationBlockIssues(declarationBlock, [])
      );
    }

    return Object.assign({}, state, {
      selectedNodeIssues: issues,
    });
  },
};

module.exports = function(state = INITIAL_STATE, action) {
  const reducer = reducers[action.type];
  return reducer ? reducer(state, action) : state;
};
