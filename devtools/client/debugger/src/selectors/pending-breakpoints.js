/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */

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
