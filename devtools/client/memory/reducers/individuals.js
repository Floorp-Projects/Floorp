/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */
"use strict";

const { assert, immutableUpdate } = require("devtools/shared/DevToolsUtils");
const { actions, individualsState, viewState } = require("../constants");

const handlers = Object.create(null);

handlers[actions.POP_VIEW] = function (_state, _action) {
  return null;
};

handlers[actions.CHANGE_VIEW] = function (individuals, { newViewState }) {
  if (newViewState === viewState.INDIVIDUALS) {
    assert(!individuals,
           "Should not switch to individuals view when already in individuals view");
    return Object.freeze({
      state: individualsState.COMPUTING_DOMINATOR_TREE,
    });
  }

  return null;
};

handlers[actions.FOCUS_INDIVIDUAL] = function (individuals, { node }) {
  assert(individuals, "Should have individuals");
  return immutableUpdate(individuals, { focused: node });
};

handlers[actions.FETCH_INDIVIDUALS_START] = function (individuals, action) {
  assert(individuals, "Should have individuals");
  return Object.freeze({
    state: individualsState.FETCHING,
    focused: individuals.focused,
  });
};

handlers[actions.FETCH_INDIVIDUALS_END] = function (individuals, action) {
  assert(individuals, "Should have individuals");
  assert(!individuals.nodes, "Should not have nodes");
  assert(individuals.state === individualsState.FETCHING,
         "Should only end fetching individuals after starting.");

  const focused = individuals.focused
    ? action.nodes.find(n => n.nodeId === individuals.focused.nodeId)
    : null;

  return Object.freeze({
    state: individualsState.FETCHED,
    nodes: action.nodes,
    id: action.id,
    censusBreakdown: action.censusBreakdown,
    indices: action.indices,
    labelDisplay: action.labelDisplay,
    focused,
    dominatorTree: action.dominatorTree,
  });
};

handlers[actions.INDIVIDUALS_ERROR] = function (_, { error }) {
  return Object.freeze({
    error,
    nodes: null,
    state: individualsState.ERROR,
  });
};

module.exports = function (individuals = null, action) {
  const handler = handlers[action.type];
  return handler ? handler(individuals, action) : individuals;
};
