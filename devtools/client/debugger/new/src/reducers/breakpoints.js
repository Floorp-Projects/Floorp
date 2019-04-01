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

import { makeBreakpointId, findPosition } from "../utils/breakpoint";
import { findEmptyLines } from "../utils/empty-lines";
import { isInlineScript } from "../utils/source";

// eslint-disable-next-line max-len
import { getBreakpointsList as getBreakpointsListSelector } from "../selectors/breakpoints";

import type {
  XHRBreakpoint,
  Breakpoint,
  BreakpointId,
  MappedLocation,
  SourceLocation,
  BreakpointPositions
} from "../types";
import type { Action } from "../actions/types";

export type BreakpointsMap = { [BreakpointId]: Breakpoint };
export type XHRBreakpointsList = $ReadOnlyArray<XHRBreakpoint>;
export type BreakpointPositionsMap = { [string]: BreakpointPositions };

export type BreakpointsState = {
  breakpoints: BreakpointsMap,
  breakpointPositions: BreakpointPositionsMap,
  xhrBreakpoints: XHRBreakpointsList,
  breakpointsDisabled: boolean,
  emptyLines: { [string]: number[] }
};

export function initialBreakpointsState(
  xhrBreakpoints?: XHRBreakpointsList = []
): BreakpointsState {
  return {
    breakpoints: {},
    xhrBreakpoints: xhrBreakpoints,
    breakpointPositions: {},
    breakpointsDisabled: false,
    emptyLines: {}
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

    case "ADD_BREAKPOINT_POSITIONS": {
      const { source, positions } = action;
      const emptyLines = findEmptyLines(source, positions);

      return {
        ...state,
        breakpointPositions: {
          ...state.breakpointPositions,
          [source.id]: positions
        },
        emptyLines: {
          ...state.emptyLines,
          [source.id]: emptyLines
        }
      };
    }

    case "ADD_SOURCES": {
      const { sources } = action;

      const scriptSources = sources.filter(source => isInlineScript(source));

      if (scriptSources.length > 0) {
        const { ...breakpointPositions } = state.breakpointPositions;

        // If new HTML sources are being added, we need to clear the breakpoint
        // positions since the new source is a <script> with new breakpoints.
        for (const source of scriptSources) {
          delete breakpointPositions[source.id];
        }

        state = { ...state, breakpointPositions };
      }

      return state;
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
      xhrBreakpoints: [...xhrBreakpoints, breakpoint]
    };
  } else if (xhrBreakpoints[existingBreakpointIndex] !== breakpoint) {
    const newXhrBreakpoints = [...xhrBreakpoints];
    newXhrBreakpoints[existingBreakpointIndex] = breakpoint;
    return {
      ...state,
      xhrBreakpoints: newXhrBreakpoints
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
    xhrBreakpoints: xhrBreakpoints.filter(bp => !isEqual(bp, breakpoint))
  };
}

function updateXHRBreakpoint(state, action) {
  const { breakpoint, index } = action;
  const { xhrBreakpoints } = state;
  const newXhrBreakpoints = [...xhrBreakpoints];
  newXhrBreakpoints[index] = breakpoint;
  return {
    ...state,
    xhrBreakpoints: newXhrBreakpoints
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

export function getBreakpointPositions(
  state: OuterState
): BreakpointPositionsMap {
  return state.breakpoints.breakpointPositions;
}

export function getBreakpointPositionsForSource(
  state: OuterState,
  sourceId: string
): ?BreakpointPositions {
  const positions = getBreakpointPositions(state);
  return positions && positions[sourceId];
}

export function hasBreakpointPositions(
  state: OuterState,
  sourceId: string
): boolean {
  return !!getBreakpointPositionsForSource(state, sourceId);
}

export function getBreakpointPositionsForLocation(
  state: OuterState,
  location: SourceLocation
): ?MappedLocation {
  const { sourceId } = location;
  const positions = getBreakpointPositionsForSource(state, sourceId);
  return findPosition(positions, location);
}

export function isEmptyLineInSource(
  state: OuterState,
  line: number,
  selectedSourceId: string
) {
  const emptyLines = getEmptyLines(state, selectedSourceId);
  return emptyLines && emptyLines.includes(line);
}

export function getEmptyLines(state: OuterState, sourceId: string) {
  if (!sourceId) {
    return null;
  }

  return state.breakpoints.emptyLines[sourceId];
}

export default update;
