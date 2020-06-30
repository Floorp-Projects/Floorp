/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const Services = require("Services");

const {
  AUDIT,
  ENABLE,
  DISABLE,
  RESET,
  SELECT,
  HIGHLIGHT,
  UNHIGHLIGHT,
  UPDATE_CAN_BE_DISABLED,
  UPDATE_CAN_BE_ENABLED,
  UPDATE_PREF,
  UPDATE_DETAILS,
  PREF_KEYS,
  PREFS,
} = require("devtools/client/accessibility/constants");

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
    expanded: new Set(),
    [PREFS.SCROLL_INTO_VIEW]: Services.prefs.getBoolPref(
      PREF_KEYS[PREFS.SCROLL_INTO_VIEW],
      false
    ),
    supports: {},
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
    case UPDATE_PREF:
      return onPrefChange(state, action);
    case UPDATE_DETAILS:
      return onUpdateDetails(state, action);
    case HIGHLIGHT:
      return onHighlight(state, action);
    case AUDIT:
      return onAudit(state, action);
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

function updateExpandedNodes(expanded, ancestry) {
  expanded = new Set(expanded);
  const path = ancestry.reduceRight((accPath, { accessible }) => {
    accPath = TreeView.subPath(accPath, accessible.actorID);
    expanded.add(accPath);
    return accPath;
  }, "");

  return { path, expanded };
}

function onAudit(state, { response: ancestries, error }) {
  if (error) {
    console.warn("Error running audit", error);
    return state;
  }

  let expanded = new Set(state.expanded);
  for (const ancestry of ancestries) {
    ({ expanded } = updateExpandedNodes(expanded, ancestry));
  }

  return {
    ...state,
    expanded,
  };
}

function onHighlight(state, { accessible, response: ancestry, error }) {
  if (error) {
    console.warn("Error fetching ancestry", error);
    return state;
  }

  const { expanded } = updateExpandedNodes(state.expanded, ancestry);
  return Object.assign({}, state, { expanded, highlighted: accessible });
}

function onSelect(state, { accessible, response: ancestry, error }) {
  if (error) {
    console.warn("Error fetching ancestry", error);
    return state;
  }

  const { path, expanded } = updateExpandedNodes(state.expanded, ancestry);
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
 * Handle pref update for accessibility panel.
 * @param  {Object}  state   Current ui state.
 * @param  {Object}  action  Redux action object
 * @return {Object}  updated state
 */
function onPrefChange(state, { name, value }) {
  return {
    ...state,
    [name]: value,
  };
}

/**
 * Handle reset action for the accessibility panel UI.
 * @param  {Object}  state   Current ui state.
 * @param  {Object}  action  Redux action object
 * @return {Object}  updated state
 */
function onReset(state, { enabled, canBeDisabled, canBeEnabled, supports }) {
  const newState = {
    ...getInitialState(),
    enabled,
    canBeDisabled,
    canBeEnabled,
    supports,
  };

  return newState;
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

  return Object.assign({}, state, { enabled });
}

exports.ui = ui;
