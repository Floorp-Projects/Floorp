/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */

import { makeBreakpointId } from "../utils/breakpoint";

export function getSymbols(state, location) {
  if (!location) {
    return null;
  }

  return state.ast.symbols[location.source.id] || null;
}

export function getSourceActorForSymbols(state, location) {
  if (!location) {
    return null;
  }

  return state.ast.actors[location.source.id] || null;
}

export function getInScopeLines(state, location) {
  return state.ast.inScopeLines[makeBreakpointId(location)];
}

export function hasInScopeLines(state, location) {
  return !!getInScopeLines(state, location);
}
