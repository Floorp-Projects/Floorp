/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */

/**
 * Ast reducer
 * @module reducers/ast
 */

import { makeBreakpointId } from "../utils/breakpoint";

export function initialASTState() {
  return {
    // We are using mutable objects as we never return the dictionary as-is from the selectors
    // but only their values .

    // We have two maps, a first one for original sources.
    // This is keyed by source id.
    mutableOriginalSourcesSymbols: {},

    // And another one, for generated sources.
    // This is keyed by source actor id.
    mutableSourceActorSymbols: {},

    mutableInScopeLines: {},
  };
}

function update(state = initialASTState(), action) {
  switch (action.type) {
    case "SET_SYMBOLS": {
      const { location } = action;
      if (action.status === "start") {
        return state;
      }

      const value = action.value;
      if (location.source.isOriginal) {
        state.mutableOriginalSourcesSymbols[location.source.id] = value;
      } else {
        if (!location.sourceActor) {
          throw new Error(
            "Expects a location with a source actor when adding symbols for non-original sources"
          );
        }
        state.mutableSourceActorSymbols[location.sourceActor.id] = value;
      }
      return {
        ...state,
      };
    }

    case "IN_SCOPE_LINES": {
      state.mutableInScopeLines[makeBreakpointId(action.location)] =
        action.lines;
      return {
        ...state,
      };
    }

    case "RESUME": {
      return { ...state, mutableInScopeLines: {} };
    }

    case "NAVIGATE": {
      return initialASTState();
    }

    default: {
      return state;
    }
  }
}

export default update;
