/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */

import { getSelectedSource } from "../reducers/sources";
import { getBreakpoints } from "../reducers/breakpoints";
import { isGenerated } from "../utils/source";

function getColumn(column, selectedSource) {
  if (column) {
    return column;
  }

  return isGenerated(selectedSource) ? undefined : 0;
}

function getLocation(bp, selectedSource) {
  return isGenerated(selectedSource)
    ? bp.generatedLocation || bp.location
    : bp.location;
}

function getBreakpointsForSource(state: OuterState, selectedSource: Source) {
  const breakpoints = getBreakpoints(state);

  return breakpoints.filter(bp => {
    const location = getLocation(bp, selectedSource);
    return location.sourceId === selectedSource.id;
  });
}

function findBreakpointAtLocation(
  breakpoints,
  selectedSource,
  { line, column }
) {
  return breakpoints.find(breakpoint => {
    const location = getLocation(breakpoint, selectedSource);
    const sameLine = location.line === line;
    if (!sameLine) {
      return false;
    }

    if (column === undefined) {
      return true;
    }

    return location.column === getColumn(column, selectedSource);
  });
}

/*
 * Finds a breakpoint at a location (line, column) of the
 * selected source.
 *
 * This is useful for finding a breakpoint when the
 * user clicks in the gutter or on a token.
 */
export function getBreakpointAtLocation(state, location) {
  const selectedSource = getSelectedSource(state);
  const breakpoints = getBreakpointsForSource(state, selectedSource);

  return findBreakpointAtLocation(breakpoints, selectedSource, location);
}

export function getBreakpointsAtLine(state: OuterState, line: number) {
  const selectedSource = getSelectedSource(state);
  const breakpoints = getBreakpointsForSource(state, selectedSource);

  return breakpoints.filter(
    breakpoint => getLocation(breakpoint, selectedSource).line === line
  );
}
