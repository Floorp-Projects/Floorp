/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
"use strict";

/* global gToolbox */

const {
  ENABLE,
  DISABLE,
  RESET,
  SELECT,
  HIGHLIGHT,
  UNHIGHLIGHT,
  UPDATE_CAN_BE_DISABLED,
  UPDATE_CAN_BE_ENABLED,
  UPDATE_DETAILS
} = require("../constants");

const TreeView = require("devtools/client/shared/components/tree/TreeView");

/**
 * Initial state definition
 */
function getInitialState() {
  return {
    enabled: false,
    canBeDisabled: true,
    canBeEnabled: true,
    selected: null,
    highlighted: null,
    expanded: new Set()
  };
}

/**
 * Maintain ui components of the accessibility panel.
 */
function ui(state = getInitialState(), action) {
  switch (action.type) {
    case ENABLE:
      return onToggle(state, action, true);
    case DISABLE:
      return onToggle(state, action, false);
    case UPDATE_CAN_BE_DISABLED:
      return onCanBeDisabledChange(state, action);
    case UPDATE_CAN_BE_ENABLED:
      return onCanBeEnabledChange(state, action);
    case UPDATE_DETAILS:
      return onUpdateDetails(state, action);
    case HIGHLIGHT:
      return onHighlight(state, action);
    case UNHIGHLIGHT:
      return onUnhighlight(state, action);
    case SELECT:
      return onSelect(state, action);
    case RESET:
      return onReset(state, action);
    default:
      return state;
  }
}

function onUpdateDetails(state) {
  if (!state.selected) {
    return state;
  }

  // Clear selected state that should only be set when select action is
  // performed.
  return Object.assign({}, state, { selected: null });
}

function onUnhighlight(state) {
  return Object.assign({}, state, { highlighted: null });
}

function updateExpandedNodes(state, ancestry) {
  const expanded = new Set(state.expanded);
  const path = ancestry.reduceRight((accPath, { accessible }) => {
    accPath = TreeView.subPath(accPath, accessible.actorID);
    expanded.add(accPath);
    return accPath;
  }, "");

  return { path, expanded };
}

function onHighlight(state, { accessible, response: ancestry, error }) {
  if (error) {
    console.warn("Error fetching ancestry", accessible, error);
    return state;
  }

  const { expanded } = updateExpandedNodes(state, ancestry);
  return Object.assign({}, state, { expanded, highlighted: accessible });
}

function onSelect(state, { accessible, response: ancestry, error }) {
  if (error) {
    console.warn("Error fetching ancestry", accessible, error);
    return state;
  }

  const { path, expanded } = updateExpandedNodes(state, ancestry);
  const selected = TreeView.subPath(path, accessible.actorID);

  return Object.assign({}, state, { expanded, selected });
}

/**
 * Handle "canBeDisabled" flag update for accessibility service
 * @param  {Object}  state   Current ui state
 * @param  {Object}  action  Redux action object
 * @return {Object}  updated state
 */
function onCanBeDisabledChange(state, { canBeDisabled }) {
  return Object.assign({}, state, { canBeDisabled });
}

/**
 * Handle "canBeEnabled" flag update for accessibility service
 * @param  {Object}  state   Current ui state.
 * @param  {Object}  action  Redux action object
 * @return {Object}  updated state
 */
function onCanBeEnabledChange(state, { canBeEnabled }) {
  return Object.assign({}, state, { canBeEnabled });
}

/**
 * Handle reset action for the accessibility panel UI.
 * @param  {Object}  state   Current ui state.
 * @param  {Object}  action  Redux action object
 * @return {Object}  updated state
 */
function onReset(state, { accessibility }) {
  const { enabled, canBeDisabled, canBeEnabled } = accessibility;
  toggleHighlightTool(enabled);
  return Object.assign({}, state, { enabled, canBeDisabled, canBeEnabled });
}

/**
 * Handle accessibilty service enabling/disabling.
 * @param {Object}  state   Current accessibility services enabled state.
 * @param {Object}  action  Redux action object
 * @param {Boolean} enabled New enabled state.
 * @return {Object}  updated state
 */
function onToggle(state, { error }, enabled) {
  if (error) {
    console.warn("Error enabling accessibility service: ", error);
    return state;
  }

  toggleHighlightTool(enabled);
  return Object.assign({}, state, { enabled });
}

function toggleHighlightTool(enabled) {
  if (enabled) {
    gToolbox.highlightTool("accessibility");
  } else {
    gToolbox.unhighlightTool("accessibility");
  }
}

exports.ui = ui;
