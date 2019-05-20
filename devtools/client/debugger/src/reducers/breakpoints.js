/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */

// @flow

/**
 * Breakpoints reducer
 * @module reducers/breakpoints
 */

import { isGeneratedId } from "devtools-source-map";
import { isEqual } from "lodash";

import { makeBreakpointId } from "../utils/breakpoint";

// eslint-disable-next-line max-len
import { getBreakpointsList as getBreakpointsListSelector } from "../selectors/breakpoints";

import type {
  XHRBreakpoint,
  Breakpoint,
  BreakpointId,
  SourceLocation,
} from "../types";
import type { Action } from "../actions/types";

export type BreakpointsMap = { [BreakpointId]: Breakpoint };
export type XHRBreakpointsList = $ReadOnlyArray<XHRBreakpoint>;

export type BreakpointsState = {
  breakpoints: BreakpointsMap,
  xhrBreakpoints: XHRBreakpointsList,
  breakpointsDisabled: boolean,
};

export function initialBreakpointsState(
  xhrBreakpoints?: XHRBreakpointsList = []
): BreakpointsState {
  return {
    breakpoints: {},
    xhrBreakpoints: xhrBreakpoints,
    breakpointsDisabled: false,
  };
}

function update(
  state: BreakpointsState = initialBreakpointsState(),
  action: Action
): BreakpointsState {
  switch (action.type) {
    case "SET_BREAKPOINT": {
      return setBreakpoint(state, action);
    }

    case "REMOVE_BREAKPOINT": {
      return removeBreakpoint(state, action);
    }

    case "NAVIGATE": {
      return initialBreakpointsState(state.xhrBreakpoints);
    }

    case "SET_XHR_BREAKPOINT": {
      return addXHRBreakpoint(state, action);
    }

    case "REMOVE_XHR_BREAKPOINT": {
      return removeXHRBreakpoint(state, action);
    }

    case "UPDATE_XHR_BREAKPOINT": {
      return updateXHRBreakpoint(state, action);
    }

    case "ENABLE_XHR_BREAKPOINT": {
      return updateXHRBreakpoint(state, action);
    }

    case "DISABLE_XHR_BREAKPOINT": {
      return updateXHRBreakpoint(state, action);
    }
  }

  return state;
}

function addXHRBreakpoint(state, action) {
  const { xhrBreakpoints } = state;
  const { breakpoint } = action;
  const { path, method } = breakpoint;

  const existingBreakpointIndex = state.xhrBreakpoints.findIndex(
    bp => bp.path === path && bp.method === method
  );

  if (existingBreakpointIndex === -1) {
    return {
      ...state,
      xhrBreakpoints: [...xhrBreakpoints, breakpoint],
    };
  } else if (xhrBreakpoints[existingBreakpointIndex] !== breakpoint) {
    const newXhrBreakpoints = [...xhrBreakpoints];
    newXhrBreakpoints[existingBreakpointIndex] = breakpoint;
    return {
      ...state,
      xhrBreakpoints: newXhrBreakpoints,
    };
  }

  return state;
}

function removeXHRBreakpoint(state, action) {
  const { breakpoint } = action;
  const { xhrBreakpoints } = state;

  if (action.status === "start") {
    return state;
  }

  return {
    ...state,
    xhrBreakpoints: xhrBreakpoints.filter(bp => !isEqual(bp, breakpoint)),
  };
}

function updateXHRBreakpoint(state, action) {
  const { breakpoint, index } = action;
  const { xhrBreakpoints } = state;
  const newXhrBreakpoints = [...xhrBreakpoints];
  newXhrBreakpoints[index] = breakpoint;
  return {
    ...state,
    xhrBreakpoints: newXhrBreakpoints,
  };
}

function setBreakpoint(state, { breakpoint }): BreakpointsState {
  const id = makeBreakpointId(breakpoint.location);
  const breakpoints = { ...state.breakpoints, [id]: breakpoint };
  return { ...state, breakpoints };
}

function removeBreakpoint(state, { location }): BreakpointsState {
  const id = makeBreakpointId(location);
  const breakpoints = { ...state.breakpoints };
  delete breakpoints[id];
  return { ...state, breakpoints };
}

function isMatchingLocation(location1, location2) {
  return isEqual(location1, location2);
}

// Selectors
// TODO: these functions should be moved out of the reducer

type OuterState = { breakpoints: BreakpointsState };

export function getBreakpointsMap(state: OuterState): BreakpointsMap {
  return state.breakpoints.breakpoints;
}

export function getBreakpointsList(state: OuterState): Breakpoint[] {
  return getBreakpointsListSelector((state: any));
}

export function getBreakpointCount(state: OuterState): number {
  return getBreakpointsList(state).length;
}

export function getBreakpoint(
  state: OuterState,
  location: SourceLocation
): ?Breakpoint {
  const breakpoints = getBreakpointsMap(state);
  return breakpoints[makeBreakpointId(location)];
}

export function getBreakpointsDisabled(state: OuterState): boolean {
  const breakpoints = getBreakpointsList(state);
  return breakpoints.every(breakpoint => breakpoint.disabled);
}

export function getBreakpointsForSource(
  state: OuterState,
  sourceId: string,
  line: ?number
): Breakpoint[] {
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

export function getBreakpointForLocation(
  state: OuterState,
  location: SourceLocation | null
): ?Breakpoint {
  if (!location || !location.sourceId) {
    return undefined;
  }

  const isGeneratedSource = isGeneratedId(location.sourceId);
  return getBreakpointsList(state).find(bp => {
    const loc = isGeneratedSource ? bp.generatedLocation : bp.location;
    return isMatchingLocation(loc, location);
  });
}

export function getHiddenBreakpoint(state: OuterState): ?Breakpoint {
  const breakpoints = getBreakpointsList(state);
  return breakpoints.find(bp => bp.options.hidden);
}

export default update;
