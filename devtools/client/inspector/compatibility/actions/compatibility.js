/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const nodeConstants = require("resource://devtools/shared/dom-node-constants.js");

const UserSettings = require("resource://devtools/shared/compatibility/compatibility-user-settings.js");

const {
  COMPATIBILITY_APPEND_NODE_START,
  COMPATIBILITY_APPEND_NODE_SUCCESS,
  COMPATIBILITY_APPEND_NODE_FAILURE,
  COMPATIBILITY_APPEND_NODE_COMPLETE,
  COMPATIBILITY_CLEAR_DESTROYED_NODES,
  COMPATIBILITY_INIT_USER_SETTINGS_START,
  COMPATIBILITY_INIT_USER_SETTINGS_SUCCESS,
  COMPATIBILITY_INIT_USER_SETTINGS_FAILURE,
  COMPATIBILITY_INIT_USER_SETTINGS_COMPLETE,
  COMPATIBILITY_INTERNAL_APPEND_NODE,
  COMPATIBILITY_INTERNAL_NODE_UPDATE,
  COMPATIBILITY_INTERNAL_REMOVE_NODE,
  COMPATIBILITY_INTERNAL_UPDATE_SELECTED_NODE_ISSUES,
  COMPATIBILITY_REMOVE_NODE_START,
  COMPATIBILITY_REMOVE_NODE_SUCCESS,
  COMPATIBILITY_REMOVE_NODE_FAILURE,
  COMPATIBILITY_REMOVE_NODE_COMPLETE,
  COMPATIBILITY_UPDATE_NODE_START,
  COMPATIBILITY_UPDATE_NODE_SUCCESS,
  COMPATIBILITY_UPDATE_NODE_FAILURE,
  COMPATIBILITY_UPDATE_NODE_COMPLETE,
  COMPATIBILITY_UPDATE_NODES_START,
  COMPATIBILITY_UPDATE_NODES_SUCCESS,
  COMPATIBILITY_UPDATE_NODES_FAILURE,
  COMPATIBILITY_UPDATE_NODES_COMPLETE,
  COMPATIBILITY_UPDATE_SELECTED_NODE_START,
  COMPATIBILITY_UPDATE_SELECTED_NODE_SUCCESS,
  COMPATIBILITY_UPDATE_SELECTED_NODE_FAILURE,
  COMPATIBILITY_UPDATE_SELECTED_NODE_COMPLETE,
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

function appendNode(node) {
  return async ({ dispatch, getState }) => {
    dispatch({ type: COMPATIBILITY_APPEND_NODE_START });

    try {
      const { targetBrowsers, topLevelTarget } = getState().compatibility;
      const { walker } = await topLevelTarget.getFront("inspector");
      await _inspectNode(node, targetBrowsers, walker, dispatch);
      dispatch({ type: COMPATIBILITY_APPEND_NODE_SUCCESS });
    } catch (error) {
      dispatch({
        type: COMPATIBILITY_APPEND_NODE_FAILURE,
        error,
      });
    }

    dispatch({ type: COMPATIBILITY_APPEND_NODE_COMPLETE });
  };
}

function clearDestroyedNodes() {
  return { type: COMPATIBILITY_CLEAR_DESTROYED_NODES };
}

function initUserSettings() {
  return async ({ dispatch, getState }) => {
    dispatch({ type: COMPATIBILITY_INIT_USER_SETTINGS_START });

    try {
      const [defaultTargetBrowsers, targetBrowsers] = await Promise.all([
        UserSettings.getBrowsersList(),
        UserSettings.getTargetBrowsers(),
      ]);

      dispatch({
        type: COMPATIBILITY_INIT_USER_SETTINGS_SUCCESS,
        defaultTargetBrowsers,
        targetBrowsers,
      });
    } catch (error) {
      dispatch({
        type: COMPATIBILITY_INIT_USER_SETTINGS_FAILURE,
        error,
      });
    }

    dispatch({ type: COMPATIBILITY_INIT_USER_SETTINGS_COMPLETE });
  };
}

function removeNode(node) {
  return async ({ dispatch, getState }) => {
    dispatch({ type: COMPATIBILITY_REMOVE_NODE_START });

    try {
      const { topLevelTarget } = getState().compatibility;
      const { walker } = await topLevelTarget.getFront("inspector");
      await _removeNode(node, walker, dispatch);
      dispatch({ type: COMPATIBILITY_REMOVE_NODE_SUCCESS });
    } catch (error) {
      dispatch({
        type: COMPATIBILITY_REMOVE_NODE_FAILURE,
        error,
      });
    }

    dispatch({ type: COMPATIBILITY_REMOVE_NODE_COMPLETE });
  };
}

function updateNodes(selector) {
  return async ({ dispatch, getState }) => {
    dispatch({ type: COMPATIBILITY_UPDATE_NODES_START });

    try {
      const { selectedNode, topLevelTarget, targetBrowsers } =
        getState().compatibility;
      const { walker } = await topLevelTarget.getFront("inspector");
      const nodeList = await walker.querySelectorAll(walker.rootNode, selector);

      for (const node of await nodeList.items()) {
        await _updateNode(node, selectedNode, targetBrowsers, dispatch);
      }
      dispatch({ type: COMPATIBILITY_UPDATE_NODES_SUCCESS });
    } catch (error) {
      dispatch({
        type: COMPATIBILITY_UPDATE_NODES_FAILURE,
        error,
      });
    }

    dispatch({ type: COMPATIBILITY_UPDATE_NODES_COMPLETE });
  };
}

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

function updateSettingsVisibility(visibility) {
  return {
    type: COMPATIBILITY_UPDATE_SETTINGS_VISIBILITY,
    visibility,
  };
}

function updateTargetBrowsers(targetBrowsers) {
  return async ({ dispatch, getState }) => {
    dispatch({ type: COMPATIBILITY_UPDATE_TARGET_BROWSERS_START });

    try {
      UserSettings.setTargetBrowsers(targetBrowsers);

      const { selectedNode, topLevelTarget } = getState().compatibility;

      if (selectedNode) {
        await _updateSelectedNodeIssues(selectedNode, targetBrowsers, dispatch);
      }

      if (topLevelTarget) {
        await _updateTopLevelTargetIssues(
          topLevelTarget,
          targetBrowsers,
          dispatch
        );
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

function updateTopLevelTarget(target) {
  return async ({ dispatch, getState }) => {
    dispatch({ type: COMPATIBILITY_UPDATE_TOP_LEVEL_TARGET_START });

    try {
      const { targetBrowsers } = getState().compatibility;
      await _updateTopLevelTargetIssues(target, targetBrowsers, dispatch);

      dispatch({ type: COMPATIBILITY_UPDATE_TOP_LEVEL_TARGET_SUCCESS, target });
    } catch (error) {
      dispatch({ type: COMPATIBILITY_UPDATE_TOP_LEVEL_TARGET_FAILURE, error });
    }

    dispatch({ type: COMPATIBILITY_UPDATE_TOP_LEVEL_TARGET_COMPLETE });
  };
}

function updateNode(node) {
  return async ({ dispatch, getState }) => {
    dispatch({ type: COMPATIBILITY_UPDATE_NODE_START });

    try {
      const { selectedNode, targetBrowsers } = getState().compatibility;
      await _updateNode(node, selectedNode, targetBrowsers, dispatch);
      dispatch({ type: COMPATIBILITY_UPDATE_NODE_SUCCESS });
    } catch (error) {
      dispatch({
        type: COMPATIBILITY_UPDATE_NODE_FAILURE,
        error,
      });
    }

    dispatch({ type: COMPATIBILITY_UPDATE_NODE_COMPLETE });
  };
}

async function _getNodeIssues(node, targetBrowsers) {
  const compatibility = await node.inspectorFront.getCompatibilityFront();
  const declarationBlocksIssues = await compatibility.getNodeCssIssues(
    node,
    targetBrowsers
  );

  return declarationBlocksIssues;
}

async function _inspectNode(node, targetBrowsers, walker, dispatch) {
  if (node.nodeType !== nodeConstants.ELEMENT_NODE) {
    return;
  }

  const issues = await _getNodeIssues(node, targetBrowsers);

  if (issues.length) {
    dispatch({
      type: COMPATIBILITY_INTERNAL_APPEND_NODE,
      node,
      issues,
    });
  }

  const { nodes: children } = await walker.children(node);
  for (const child of children) {
    await _inspectNode(child, targetBrowsers, walker, dispatch);
  }
}

async function _removeNode(node, walker, dispatch) {
  if (node.nodeType !== nodeConstants.ELEMENT_NODE) {
    return;
  }

  dispatch({
    type: COMPATIBILITY_INTERNAL_REMOVE_NODE,
    node,
  });

  const { nodes: children } = await walker.children(node);
  for (const child of children) {
    await _removeNode(child, walker, dispatch);
  }
}

async function _updateNode(node, selectedNode, targetBrowsers, dispatch) {
  if (selectedNode.actorID === node.actorID) {
    await _updateSelectedNodeIssues(node, targetBrowsers, dispatch);
  }

  const issues = await _getNodeIssues(node, targetBrowsers);
  dispatch({
    type: COMPATIBILITY_INTERNAL_NODE_UPDATE,
    node,
    issues,
  });
}

async function _updateSelectedNodeIssues(node, targetBrowsers, dispatch) {
  const issues = await _getNodeIssues(node, targetBrowsers);

  dispatch({
    type: COMPATIBILITY_INTERNAL_UPDATE_SELECTED_NODE_ISSUES,
    issues,
  });
}

async function _updateTopLevelTargetIssues(target, targetBrowsers, dispatch) {
  const { walker } = await target.getFront("inspector");
  const documentElement = await walker.documentElement();
  await _inspectNode(documentElement, targetBrowsers, walker, dispatch);
}

module.exports = {
  appendNode,
  clearDestroyedNodes,
  initUserSettings,
  removeNode,
  updateNodes,
  updateSelectedNode,
  updateSettingsVisibility,
  updateTargetBrowsers,
  updateTopLevelTarget,
  updateNode,
};
