/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */

// @flow

/**
 * Pending breakpoints reducer
 * @module reducers/pending-breakpoints
 */

import { getSourcesByURL } from "./sources";

import {
  createPendingBreakpoint,
  makePendingLocationId
} from "../utils/breakpoint";
import { isGenerated } from "../utils/source";

import type { SourcesState } from "./sources";
import type { PendingBreakpoint, Source } from "../types";
import type { Action, DonePromiseAction } from "../actions/types";

export type PendingBreakpointsState = { [string]: PendingBreakpoint };

function update(state: PendingBreakpointsState = {}, action: Action) {
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

    case "SET_BREAKPOINT_OPTIONS": {
      return updateBreakpoint(state, action);
    }

    case "REMOVE_BREAKPOINT": {
      if (action.breakpoint.options.hidden) {
        return state;
      }
      return removeBreakpoint(state, action);
    }
  }

  return state;
}

function addBreakpoint(state, action) {
  if (action.breakpoint.options.hidden || action.status !== "done") {
    return state;
  }
  // when the action completes, we can commit the breakpoint
  const breakpoint = ((action: any): DonePromiseAction).value;

  const locationId = makePendingLocationId(breakpoint.location);
  const pendingBreakpoint = createPendingBreakpoint(breakpoint);

  return { ...state, [locationId]: pendingBreakpoint };
}

function syncBreakpoint(state, action) {
  const { breakpoint, previousLocation } = action;

  if (previousLocation) {
    const previousLocationId = makePendingLocationId(previousLocation);
    state = deleteBreakpoint(state, previousLocationId);
  }

  if (!breakpoint) {
    return state;
  }

  const locationId = makePendingLocationId(breakpoint.location);
  const pendingBreakpoint = createPendingBreakpoint(breakpoint);

  return { ...state, [locationId]: pendingBreakpoint };
}

function updateBreakpoint(state, action) {
  const { breakpoint } = action;
  const locationId = makePendingLocationId(breakpoint.location);
  const pendingBreakpoint = createPendingBreakpoint(breakpoint);

  return { ...state, [locationId]: pendingBreakpoint };
}

function updateAllBreakpoints(state, action) {
  const { breakpoints } = action;
  breakpoints.forEach(breakpoint => {
    const locationId = makePendingLocationId(breakpoint.location);
    const pendingBreakpoint = createPendingBreakpoint(breakpoint);

    state = { ...state, [locationId]: pendingBreakpoint };
  });
  return state;
}

function removeBreakpoint(state, action) {
  const { breakpoint } = action;

  const locationId = makePendingLocationId(breakpoint.location);
  const pendingBp = state[locationId];

  if (!pendingBp && action.status == "start") {
    return {};
  }

  return deleteBreakpoint(state, locationId);
}

function deleteBreakpoint(state, locationId) {
  state = { ...state };
  delete state[locationId];
  return state;
}

// Selectors
// TODO: these functions should be moved out of the reducer

type OuterState = {
  pendingBreakpoints: PendingBreakpointsState,
  sources: SourcesState
};

export function getPendingBreakpoints(state: OuterState) {
  return state.pendingBreakpoints;
}

export function getPendingBreakpointList(
  state: OuterState
): PendingBreakpoint[] {
  return (Object.values(getPendingBreakpoints(state)): any);
}

export function getPendingBreakpointsForSource(
  state: OuterState,
  source: Source
): PendingBreakpoint[] {
  const sources = getSourcesByURL(state, source.url);
  if (sources.length > 1 && isGenerated(source)) {
    // Don't return pending breakpoints for duplicated generated sources
    return [];
  }

  return getPendingBreakpointList(state).filter(pendingBreakpoint => {
    return (
      pendingBreakpoint.location.sourceUrl === source.url ||
      pendingBreakpoint.generatedLocation.sourceUrl == source.url
    );
  });
}

export default update;
