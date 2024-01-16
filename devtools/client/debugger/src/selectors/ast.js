/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */

import { makeBreakpointId } from "../utils/breakpoint/index";

export function getSymbols(state, location) {
  if (!location) {
    return null;
  }
  if (location.source.isOriginal) {
    return (
      state.ast.mutableOriginalSourcesSymbols[location.source.id]?.value || null
    );
  }
  if (!location.sourceActor) {
    throw new Error(
      "Expects a location with a source actor when passing non-original sources to getSymbols"
    );
  }
  return (
    state.ast.mutableSourceActorSymbols[location.sourceActor.id]?.value || null
  );
}

export function getInScopeLines(state, location) {
  return state.ast.mutableInScopeLines[makeBreakpointId(location)]?.lines;
}

export function hasInScopeLines(state, location) {
  return !!getInScopeLines(state, location);
}
