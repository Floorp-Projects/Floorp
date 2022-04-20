/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */

import { createSelector } from "reselect";

import { isGeneratedId } from "devtools-source-map";
import { makeBreakpointId } from "../utils/breakpoint";

// This method is only used from the main test helper
export function getBreakpointsMap(state) {
  return state.breakpoints.breakpoints;
}

export const getBreakpointsList = createSelector(
  state => state.breakpoints.breakpoints,
  breakpoints => Object.values(breakpoints)
);

export function getBreakpointCount(state) {
  return getBreakpointsList(state).length;
}

export function getBreakpoint(state, location) {
  if (!location) {
    return undefined;
  }

  const breakpoints = getBreakpointsMap(state);
  return breakpoints[makeBreakpointId(location)];
}

export function getBreakpointsForSource(state, sourceId, line) {
  if (!sourceId) {
    return [];
  }

  const isGeneratedSource = isGeneratedId(sourceId);
  const breakpoints = getBreakpointsList(state);
  return breakpoints.filter(bp => {
    const location = isGeneratedSource ? bp.generatedLocation : bp.location;
    return location.sourceId === sourceId && (!line || line == location.line);
  });
}

export function getHiddenBreakpoint(state) {
  const breakpoints = getBreakpointsList(state);
  return breakpoints.find(bp => bp.options.hidden);
}

export function hasLogpoint(state, location) {
  const breakpoint = getBreakpoint(state, location);
  return breakpoint?.options.logValue;
}

export function getXHRBreakpoints(state) {
  return state.breakpoints.xhrBreakpoints;
}

export const shouldPauseOnAnyXHR = createSelector(
  getXHRBreakpoints,
  xhrBreakpoints => {
    const emptyBp = xhrBreakpoints.find(({ path }) => path.length === 0);
    if (!emptyBp) {
      return false;
    }

    return !emptyBp.disabled;
  }
);
