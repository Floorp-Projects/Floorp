/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */

/**
 * Pending breakpoints reducer
 * @module reducers/pending-breakpoints
 */

import {
  createPendingBreakpoint,
  makePendingLocationId,
} from "../utils/breakpoint";

import { isPrettyURL } from "../utils/source";

function update(state = {}, action) {
  switch (action.type) {
    case "SET_BREAKPOINT":
      if (action.status === "start") {
        return setBreakpoint(state, action);
      }
      return state;

    case "REMOVE_BREAKPOINT":
      if (action.status === "start") {
        return removeBreakpoint(state, action);
      }
      return state;

    case "REMOVE_PENDING_BREAKPOINT":
      return removeBreakpoint(state, action);

    case "REMOVE_BREAKPOINTS": {
      return {};
    }
  }

  return state;
}

/**
 * Return a location id representing a breakpoint's original location, or for
 * pretty-printed sources, its generated location.
 * @param {{ location: Location, originalLocation?: Location }} breakpoint
 */
function makePendingLocationIdFromBreakpoint(breakpoint) {
  const location =
    !breakpoint.location.sourceUrl || isPrettyURL(breakpoint.location.sourceUrl)
      ? breakpoint.generatedLocation
      : breakpoint.location;
  return makePendingLocationId(location);
}

function setBreakpoint(state, { breakpoint }) {
  if (breakpoint.options.hidden) {
    return state;
  }
  const locationId = makePendingLocationIdFromBreakpoint(breakpoint);
  const pendingBreakpoint = createPendingBreakpoint(breakpoint);

  return { ...state, [locationId]: pendingBreakpoint };
}

function removeBreakpoint(state, { breakpoint }) {
  const locationId = makePendingLocationIdFromBreakpoint(breakpoint);
  state = { ...state };

  delete state[locationId];
  return state;
}

// Selectors
// TODO: these functions should be moved out of the reducer

export function getPendingBreakpoints(state) {
  return state.pendingBreakpoints;
}

export function getPendingBreakpointList(state) {
  return Object.values(getPendingBreakpoints(state));
}

export function getPendingBreakpointsForSource(state, source) {
  return getPendingBreakpointList(state).filter(pendingBreakpoint => {
    return (
      pendingBreakpoint.location.sourceUrl === source.url ||
      pendingBreakpoint.generatedLocation.sourceUrl == source.url
    );
  });
}

export default update;
