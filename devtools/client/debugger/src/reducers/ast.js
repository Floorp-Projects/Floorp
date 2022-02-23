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
    symbols: {},
    inScopeLines: {},
  };
}

function update(state = initialASTState(), action) {
  switch (action.type) {
    case "SET_SYMBOLS": {
      const { sourceId } = action;
      if (action.status === "start") {
        return {
          ...state,
          symbols: { ...state.symbols, [sourceId]: { loading: true } },
        };
      }

      const value = action.value;
      return {
        ...state,
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

// NOTE: we'd like to have the app state fully typed
// https://github.com/firefox-devtools/debugger/blob/master/src/reducers/sources.js#L179-L185

export function getSymbols(state, source) {
  if (!source) {
    return null;
  }

  return state.ast.symbols[source.id] || null;
}

export function hasSymbols(state, source) {
  const symbols = getSymbols(state, source);

  if (!symbols) {
    return false;
  }

  return !symbols.loading;
}

export function getFramework(state, source) {
  const symbols = getSymbols(state, source);
  if (symbols && !symbols.loading) {
    return symbols.framework;
  }
}

export function isSymbolsLoading(state, source) {
  const symbols = getSymbols(state, source);
  if (!symbols) {
    return false;
  }

  return symbols.loading;
}

export function getInScopeLines(state, location) {
  return state.ast.inScopeLines[makeBreakpointId(location)];
}

export function hasInScopeLines(state, location) {
  return !!getInScopeLines(state, location);
}

export default update;
