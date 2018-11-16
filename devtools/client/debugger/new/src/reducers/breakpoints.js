/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */

// @flow

/**
 * Breakpoints reducer
 * @module reducers/breakpoints
 */

import * as I from "immutable";
import makeRecord from "../utils/makeRecord";

import { isGeneratedId } from "devtools-source-map";
import { makeLocationId } from "../utils/breakpoint";

import type { XHRBreakpoint, Breakpoint, Location } from "../types";
import type { Action, DonePromiseAction } from "../actions/types";
import type { Record } from "../utils/makeRecord";

export type BreakpointsMap = I.Map<string, Breakpoint>;
export type XHRBreakpointsList = I.List<XHRBreakpoint>;

export type BreakpointsState = {
  breakpoints: BreakpointsMap,
  xhrBreakpoints: XHRBreakpointsList
};

export function initialBreakpointsState(
  xhrBreakpoints?: any[] = []
): Record<BreakpointsState> {
  return makeRecord(
    ({
      breakpoints: I.Map(),
      xhrBreakpoints: I.List(xhrBreakpoints),
      breakpointsDisabled: false
    }: BreakpointsState)
  )();
}

function update(
  state: Record<BreakpointsState> = initialBreakpointsState(),
  action: Action
) {
  switch (action.type) {
    case "ADD_BREAKPOINT": {
      return addBreakpoint(state, action);
    }

    case "SYNC_BREAKPOINT": {
      return syncBreakpoint(state, action);
    }

    case "ENABLE_BREAKPOINT": {
      return addBreakpoint(state, action);
    }

    case "DISABLE_BREAKPOINT": {
      return updateBreakpoint(state, action);
    }

    case "DISABLE_ALL_BREAKPOINTS": {
      return updateAllBreakpoints(state, action);
    }

    case "ENABLE_ALL_BREAKPOINTS": {
      return updateAllBreakpoints(state, action);
    }

    case "SET_BREAKPOINT_CONDITION": {
      return updateBreakpoint(state, action);
    }

    case "REMOVE_BREAKPOINT": {
      return removeBreakpoint(state, action);
    }

    case "REMAP_BREAKPOINTS": {
      return remapBreakpoints(state, action);
    }

    case "NAVIGATE": {
      return initialBreakpointsState();
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
    return state.set("xhrBreakpoints", xhrBreakpoints.push(breakpoint));
  } else if (xhrBreakpoints.get(existingBreakpointIndex) !== breakpoint) {
    return state.set(
      "xhrBreakpoints",
      xhrBreakpoints.set(existingBreakpointIndex, breakpoint)
    );
  }

  return state;
}

function removeXHRBreakpoint(state, action) {
  const {
    breakpoint: { path, method }
  } = action;
  const { xhrBreakpoints } = state;

  const index = xhrBreakpoints.findIndex(
    bp => bp.path === path && bp.method === method
  );

  return state.set("xhrBreakpoints", xhrBreakpoints.delete(index));
}

function updateXHRBreakpoint(state, action) {
  const { breakpoint, index } = action;
  const { xhrBreakpoints } = state;
  return state.set("xhrBreakpoints", xhrBreakpoints.set(index, breakpoint));
}

function addBreakpoint(state, action) {
  if (action.status === "start" && action.breakpoint) {
    const { breakpoint } = action;
    const locationId = makeLocationId(breakpoint.location);
    return state.setIn(["breakpoints", locationId], breakpoint);
  }

  // when the action completes, we can commit the breakpoint
  if (action.status === "done") {
    const { value } = ((action: any): DonePromiseAction);
    return syncBreakpoint(state, value);
  }

  // Remove the optimistic update
  if (action.status === "error" && action.breakpoint) {
    const locationId = makeLocationId(action.breakpoint.location);
    return state.deleteIn(["breakpoints", locationId]);
  }

  return state;
}

function syncBreakpoint(state, data) {
  const { breakpoint, previousLocation } = data;

  if (previousLocation) {
    state = state.deleteIn(["breakpoints", makeLocationId(previousLocation)]);
  }

  if (!breakpoint) {
    return state;
  }

  const locationId = makeLocationId(breakpoint.location);
  return state.setIn(["breakpoints", locationId], breakpoint);
}

function updateBreakpoint(state, action) {
  const { breakpoint } = action;
  const locationId = makeLocationId(breakpoint.location);
  return state.setIn(["breakpoints", locationId], breakpoint);
}

function updateAllBreakpoints(state, action) {
  const { breakpoints } = action;
  breakpoints.forEach(breakpoint => {
    const locationId = makeLocationId(breakpoint.location);
    state = state.setIn(["breakpoints", locationId], breakpoint);
  });
  return state;
}

function remapBreakpoints(state, action) {
  const breakpoints = action.breakpoints.reduce(
    (updatedBreakpoints, breakpoint) => {
      const locationId = makeLocationId(breakpoint.location);
      return { ...updatedBreakpoints, [locationId]: breakpoint };
    },
    {}
  );

  return state.set("breakpoints", I.Map(breakpoints));
}

function removeBreakpoint(state, action) {
  const { breakpoint } = action;
  const id = makeLocationId(breakpoint.location);
  return state.deleteIn(["breakpoints", id]);
}

// Selectors
// TODO: these functions should be moved out of the reducer

type OuterState = { breakpoints: Record<BreakpointsState> };

export function getBreakpoints(state: OuterState) {
  return state.breakpoints.breakpoints;
}

export function getBreakpoint(
  state: OuterState,
  location: Location
): Breakpoint {
  const breakpoints = getBreakpoints(state);
  return breakpoints.get(makeLocationId(location));
}

export function getBreakpointsDisabled(state: OuterState): boolean {
  return state.breakpoints.breakpoints.every(x => x.disabled);
}

export function getBreakpointsLoading(state: OuterState) {
  const breakpoints = getBreakpoints(state);
  const isLoading = !!breakpoints
    .valueSeq()
    .filter(bp => bp.loading)
    .first();

  return breakpoints.size > 0 && isLoading;
}

export function getBreakpointsForSource(state: OuterState, sourceId: string) {
  if (!sourceId) {
    return I.Map();
  }

  const isGeneratedSource = isGeneratedId(sourceId);
  const breakpoints = getBreakpoints(state);

  return breakpoints.filter(bp => {
    const location = isGeneratedSource
      ? bp.generatedLocation || bp.location
      : bp.location;
    return location.sourceId === sourceId;
  });
}

export function getBreakpointForLine(
  state: OuterState,
  sourceId: string,
  line: number | null
): ?Breakpoint {
  if (!sourceId) {
    return I.Map();
  }
  const breakpoints = getBreakpointsForSource(state, sourceId);
  return breakpoints.find(breakpoint => breakpoint.location.line === line);
}

export function getHiddenBreakpoint(state: OuterState) {
  return getBreakpoints(state)
    .valueSeq()
    .filter(breakpoint => breakpoint.hidden)
    .first();
}

export function getHiddenBreakpointLocation(state: OuterState) {
  const hiddenBreakpoint = getHiddenBreakpoint(state);
  if (!hiddenBreakpoint) {
    return null;
  }
  return hiddenBreakpoint.location;
}

export default update;
