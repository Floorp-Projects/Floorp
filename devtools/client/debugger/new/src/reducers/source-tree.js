"use strict";

Object.defineProperty(exports, "__esModule", {
  value: true
});
exports.InitialState = InitialState;
exports.default = update;
exports.getExpandedState = getExpandedState;

/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */

/**
 * Source tree reducer
 * @module reducers/source-tree
 */
function InitialState() {
  return {
    expanded: null
  };
}

function update(state = InitialState(), action) {
  switch (action.type) {
    case "SET_EXPANDED_STATE":
      return updateExpanded(state, action);
  }

  return state;
}

function updateExpanded(state, action) {
  return { ...state,
    expanded: new Set(action.expanded)
  };
}

function getExpandedState(state) {
  return state.sourceTree.expanded;
}