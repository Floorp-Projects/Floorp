/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */

/**
 * Breakpoints reducer
 * @module reducers/breakpoints
 */

import { isGeneratedId } from "devtools-source-map";
import { isEqual } from "lodash";

import { makeBreakpointId } from "../utils/breakpoint";

// eslint-disable-next-line max-len
import { getBreakpointsList as getBreakpointsListSelector } from "../selectors/breakpoints";

export function initialBreakpointsState(xhrBreakpoints = []) {
  return {
    breakpoints: {},
    xhrBreakpoints,
    breakpointsDisabled: false,
  };
}

function update(state = initialBreakpointsState(), action) {
  switch (action.type) {
    case "SET_BREAKPOINT": {
      if (action.status === "start") {
        return setBreakpoint(state, action);
      }
      return state;
    }

    case "REMOVE_BREAKPOINT": {
      if (action.status === "start") {
        return removeBreakpoint(state, action);
      }
      return state;
    }

    case "REMOVE_BREAKPOINTS": {
      return { ...state, breakpoints: {} };
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

function setBreakpoint(state, { breakpoint }) {
  const id = makeBreakpointId(breakpoint.location);
  const breakpoints = { ...state.breakpoints, [id]: breakpoint };
  return { ...state, breakpoints };
}

function removeBreakpoint(state, { breakpoint }) {
  const id = makeBreakpointId(breakpoint.location);
  const breakpoints = { ...state.breakpoints };
  delete breakpoints[id];
  return { ...state, breakpoints };
}

function isMatchingLocation(location1, location2) {
  return isEqual(location1, location2);
}

// Selectors
// TODO: these functions should be moved out of the reducer

export function getBreakpointsMap(state) {
  return state.breakpoints.breakpoints;
}

export function getBreakpointsList(state) {
  return getBreakpointsListSelector(state);
}

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

export function getBreakpointsDisabled(state) {
  const breakpoints = getBreakpointsList(state);
  return breakpoints.every(breakpoint => breakpoint.disabled);
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

export function getBreakpointForLocation(state, location) {
  if (!location) {
    return undefined;
  }

  const isGeneratedSource = isGeneratedId(location.sourceId);
  return getBreakpointsList(state).find(bp => {
    const loc = isGeneratedSource ? bp.generatedLocation : bp.location;
    return isMatchingLocation(loc, location);
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

export default update;
