/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */
"use strict";

const initialReducerState = {
  targets: [],
  selected: null,
};

exports.reducer = targetsReducer;
function targetsReducer(state = initialReducerState, action) {
  switch (action.type) {
    case "SELECT_TARGET": {
      const { targetActorID } = action;

      if (state.selected?.actorID === targetActorID) {
        return state;
      }

      const selectedTarget = state.targets.find(
        target => target.actorID === targetActorID
      );

      // It's possible that the target reducer is missing a target
      // e.g. workers, remote iframes, etc. (Bug 1594754)
      if (!selectedTarget) {
        return state;
      }

      return { ...state, selected: selectedTarget };
    }

    case "REGISTER_TARGET": {
      return {
        ...state,
        targets: [...state.targets, action.target],
      };
    }

    case "CLEAR_TARGET": {
      const targets = state.targets.filter(
        target => target._targetFront !== action.targetFront
      );

      let { selected } = state;
      if (selected?._targetFront === action.targetFront) {
        selected = null;
      }

      return { ...state, targets, selected };
    }
  }
  return state;
}

exports.getToolboxTargets = getToolboxTargets;
function getToolboxTargets(state) {
  return state.targets.targets;
}

exports.getSelectedTarget = getSelectedTarget;
function getSelectedTarget(state) {
  return state.targets.selected;
}
