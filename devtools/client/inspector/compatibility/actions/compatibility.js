/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const {
  COMPATIBILITY_UPDATE_SELECTED_NODE_START,
  COMPATIBILITY_UPDATE_SELECTED_NODE_SUCCESS,
  COMPATIBILITY_UPDATE_SELECTED_NODE_FAILURE,
  COMPATIBILITY_UPDATE_SELECTED_NODE_COMPLETE,
  COMPATIBILITY_UPDATE_TARGET_BROWSERS,
} = require("devtools/client/inspector/compatibility/actions/index");

function updateSelectedNode(nodeFront) {
  return async ({ dispatch }) => {
    dispatch({ type: COMPATIBILITY_UPDATE_SELECTED_NODE_START });

    try {
      const pageStyle = nodeFront.inspectorFront.pageStyle;
      const styles = await pageStyle.getApplied(nodeFront, {
        skipPseudo: false,
      });
      const declarationBlocks = styles
        .map(({ rule }) => rule.declarations.filter(d => !d.commentOffsets))
        .filter(declarations => declarations.length);

      dispatch({
        type: COMPATIBILITY_UPDATE_SELECTED_NODE_SUCCESS,
        declarationBlocks,
      });
    } catch (error) {
      dispatch({ type: COMPATIBILITY_UPDATE_SELECTED_NODE_FAILURE, error });
    }

    dispatch({ type: COMPATIBILITY_UPDATE_SELECTED_NODE_COMPLETE });
  };
}

function updateTargetBrowsers(targetBrowsers) {
  return {
    type: COMPATIBILITY_UPDATE_TARGET_BROWSERS,
    targetBrowsers,
  };
}

module.exports = {
  updateSelectedNode,
  updateTargetBrowsers,
};
