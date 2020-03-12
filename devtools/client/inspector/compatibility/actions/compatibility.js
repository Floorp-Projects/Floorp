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
  COMPATIBILITY_UPDATE_SELECTED_NODE_START,
  COMPATIBILITY_UPDATE_SELECTED_NODE_SUCCESS,
  COMPATIBILITY_UPDATE_SELECTED_NODE_FAILURE,
  COMPATIBILITY_UPDATE_SELECTED_NODE_COMPLETE,
  COMPATIBILITY_UPDATE_SELECTED_NODE_ISSUES,
  COMPATIBILITY_UPDATE_TARGET_BROWSERS_START,
  COMPATIBILITY_UPDATE_TARGET_BROWSERS_SUCCESS,
  COMPATIBILITY_UPDATE_TARGET_BROWSERS_FAILURE,
  COMPATIBILITY_UPDATE_TARGET_BROWSERS_COMPLETE,
} = require("devtools/client/inspector/compatibility/actions/index");

function updateSelectedNode(node) {
  return async ({ dispatch, getState }) => {
    dispatch({ type: COMPATIBILITY_UPDATE_SELECTED_NODE_START });

    try {
      const { targetBrowsers } = getState().compatibility;
      await _updateSelectedNodeIssues(node, targetBrowsers, dispatch);

      dispatch({
        type: COMPATIBILITY_UPDATE_SELECTED_NODE_SUCCESS,
        node,
      });
    } catch (error) {
      dispatch({
        type: COMPATIBILITY_UPDATE_SELECTED_NODE_FAILURE,
        error,
      });
    }

    dispatch({ type: COMPATIBILITY_UPDATE_SELECTED_NODE_COMPLETE });
  };
}

function updateTargetBrowsers(targetBrowsers) {
  return async ({ dispatch, getState }) => {
    dispatch({ type: COMPATIBILITY_UPDATE_TARGET_BROWSERS_START });

    try {
      const { selectedNode } = getState().compatibility;

      if (selectedNode) {
        await _updateSelectedNodeIssues(selectedNode, targetBrowsers, dispatch);
      }

      dispatch({
        type: COMPATIBILITY_UPDATE_TARGET_BROWSERS_SUCCESS,
        targetBrowsers,
      });
    } catch (error) {
      dispatch({ type: COMPATIBILITY_UPDATE_TARGET_BROWSERS_FAILURE, error });
    }

    dispatch({ type: COMPATIBILITY_UPDATE_TARGET_BROWSERS_COMPLETE });
  };
}

async function _getNodeIssues(node, targetBrowsers) {
  const pageStyle = node.inspectorFront.pageStyle;
  const styles = await pageStyle.getApplied(node, {
    skipPseudo: false,
  });

  const declarationBlocks = styles
    .map(({ rule }) => rule.declarations.filter(d => !d.commentOffsets))
    .filter(declarations => declarations.length);

  return declarationBlocks
    .map(declarationBlock =>
      mdnCompatibility.getCSSDeclarationBlockIssues(
        declarationBlock,
        targetBrowsers
      )
    )
    .flat()
    .reduce((issues, issue) => {
      // Get rid of duplicate issue
      return issues.find(
        i => i.type === issue.type && i.property === issue.property
      )
        ? issues
        : [...issues, issue];
    }, []);
}

async function _updateSelectedNodeIssues(node, targetBrowsers, dispatch) {
  const issues = await _getNodeIssues(node, targetBrowsers);

  dispatch({
    type: COMPATIBILITY_UPDATE_SELECTED_NODE_ISSUES,
    issues,
  });
}

module.exports = {
  updateSelectedNode,
  updateTargetBrowsers,
};
