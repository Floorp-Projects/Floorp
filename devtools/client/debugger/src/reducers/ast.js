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
    // Internal map of the source id to the specific source actor
    // that loads the text for these symbols.
    actors: {},
    symbols: {},
    inScopeLines: {},
  };
}

function update(state = initialASTState(), action) {
  switch (action.type) {
    case "SET_SYMBOLS": {
      const { sourceId, sourceActorId } = action;
      if (action.status === "start") {
        return state;
      }

      const value = action.value;
      return {
        ...state,
        actors: { ...state.actors, [sourceId]: sourceActorId },
        symbols: { ...state.symbols, [sourceId]: value },
      };
    }

    case "IN_SCOPE_LINES": {
      return {
        ...state,
        inScopeLines: {
          ...state.inScopeLines,
          [makeBreakpointId(action.location)]: action.lines,
        },
      };
    }

    case "RESUME": {
      return { ...state, inScopeLines: {} };
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
