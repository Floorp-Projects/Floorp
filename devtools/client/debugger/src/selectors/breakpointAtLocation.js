/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */

import { getSelectedSource, getBreakpointPositionsForLine } from "./sources";
import { getBreakpointsList } from "./breakpoints";

function getColumn(column, selectedSource) {
  if (column) {
    return column;
  }

  return !selectedSource.isOriginal ? undefined : 0;
}

function getLocation(bp, selectedSource) {
  return !selectedSource.isOriginal
    ? bp.generatedLocation || bp.location
    : bp.location;
}

function getBreakpointsForSource(state, selectedSource) {
  const breakpoints = getBreakpointsList(state);

  return breakpoints.filter(bp => {
    const location = getLocation(bp, selectedSource);
    return location.source.id === selectedSource.id;
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

// returns the closest active column breakpoint
function findClosestBreakpoint(breakpoints, column) {
  if (!breakpoints || !breakpoints.length) {
    return null;
  }

  const firstBreakpoint = breakpoints[0];
  return breakpoints.reduce((closestBp, currentBp) => {
    const currentColumn = currentBp.generatedLocation.column;
    const closestColumn = closestBp.generatedLocation.column;
    // check that breakpoint has a column.
    if (column && currentColumn && closestColumn) {
      const currentDistance = Math.abs(currentColumn - column);
      const closestDistance = Math.abs(closestColumn - column);

      return currentDistance < closestDistance ? currentBp : closestBp;
    }
    return closestBp;
  }, firstBreakpoint);
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
  if (!selectedSource) {
    throw new Error("no selectedSource");
  }
  const breakpoints = getBreakpointsForSource(state, selectedSource);

  return findBreakpointAtLocation(breakpoints, selectedSource, location);
}

export function getBreakpointsAtLine(state, line) {
  const selectedSource = getSelectedSource(state);
  if (!selectedSource) {
    throw new Error("no selectedSource");
  }
  const breakpoints = getBreakpointsForSource(state, selectedSource);

  return breakpoints.filter(
    breakpoint => getLocation(breakpoint, selectedSource).line === line
  );
}

export function getClosestBreakpoint(state, position) {
  const columnBreakpoints = getBreakpointsAtLine(state, position.line);
  const breakpoint = findClosestBreakpoint(columnBreakpoints, position.column);
  return breakpoint;
}

export function getClosestBreakpointPosition(state, position) {
  const selectedSource = getSelectedSource(state);
  if (!selectedSource) {
    throw new Error("no selectedSource");
  }

  const columnBreakpoints = getBreakpointPositionsForLine(
    state,
    selectedSource.id,
    position.line
  );

  return findClosestBreakpoint(columnBreakpoints, position.column);
}
