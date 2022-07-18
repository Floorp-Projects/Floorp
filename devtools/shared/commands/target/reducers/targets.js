/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */
"use strict";

const initialReducerState = {
  // Array of targetFront
  targets: [],
  // The selected targetFront instance
  selected: null,
  // timestamp of the last time a target was updated (i.e. url/title was updated).
  // This is used by the EvaluationContextSelector component to re-render the list of
  // targets when the list itself did not change (no addition/removal)
  lastTargetRefresh: Date.now(),
};

function update(state = initialReducerState, action) {
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
        targets: [...state.targets, action.targetFront],
      };
    }

    case "REFRESH_TARGETS": {
      // The data _in_ targetFront was updated, so we only need to mutate the state,
      // while keeping the same values.
      return {
        ...state,
        lastTargetRefresh: Date.now(),
      };
    }

    case "UNREGISTER_TARGET": {
      const targets = state.targets.filter(
        target => target !== action.targetFront
      );

      let { selected } = state;
      if (selected === action.targetFront) {
        selected = null;
      }

      return { ...state, targets, selected };
    }
  }
  return state;
}
module.exports = update;
