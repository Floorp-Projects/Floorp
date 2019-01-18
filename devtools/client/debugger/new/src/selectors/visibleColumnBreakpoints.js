/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */

// @flow

import { groupBy, sortedUniqBy } from "lodash";
import { createSelector } from "reselect";

import { getViewport } from "../selectors";
import { getVisibleBreakpoints } from "./visibleBreakpoints";
import { getVisiblePausePoints } from "./visiblePausePoints";
import { makeLocationId } from "../utils/breakpoint";
import type { Selector, PausePoint } from "../reducers/types";

import type {
  SourceLocation,
  PartialPosition,
  Breakpoint,
  Range
} from "../types";

export type ColumnBreakpoint = {|
  +location: SourceLocation,
  +breakpoint: ?Breakpoint
|};

export type ColumnBreakpoints = Array<ColumnBreakpoint>;

function contains(location: PartialPosition, range: Range) {
  return (
    location.line >= range.start.line &&
    location.line <= range.end.line &&
    (!location.column ||
      (location.column >= range.start.column &&
        location.column <= range.end.column))
  );
}

function groupBreakpoints(breakpoints) {
  if (!breakpoints) {
    return {};
  }
  const map: any = groupBy(breakpoints, ({ location }) => location.line);
  for (const line in map) {
    map[line] = groupBy(map[line], ({ location }) => location.column);
  }

  return map;
}

function findBreakpoint(location, breakpointMap) {
  const { line, column } = location;
  const breakpoints = breakpointMap[line] && breakpointMap[line][column];

  if (breakpoints) {
    return breakpoints[0];
  }
}

function getLineCount(columnBreakpoints) {
  const lineCount = {};
  columnBreakpoints.forEach(({ location: { line } }) => {
    if (!lineCount[line]) {
      lineCount[line] = 0;
    }

    lineCount[line] = lineCount[line] + 1;
  });

  return lineCount;
}

export function formatColumnBreakpoints(columnBreakpoints: ColumnBreakpoints) {
  console.log(
    "Column Breakpoints\n\n",
    columnBreakpoints
      .map(
        ({ location, breakpoint }) =>
          `(${location.line}, ${location.column || ""}) ${
            breakpoint && breakpoint.disabled ? "disabled" : ""
          }`
      )
      .join("\n")
  );
}

export function getColumnBreakpoints(
  pausePoints: ?(PausePoint[]),
  breakpoints: ?(Breakpoint[]),
  viewport: Range
) {
  if (!pausePoints) {
    return [];
  }

  const breakpointMap = groupBreakpoints(breakpoints);

  // We only want to show a column breakpoint if several conditions are matched
  // 1. it is a "break" point and not a "step" point
  // 2. there is a breakpoint on that line
  // 3. the breakpoint is in the current viewport
  // 4. it is the first breakpoint to appear at that generated location
  // 5. there is atleast one other breakpoint on that line

  let columnBreakpoints = pausePoints.filter(
    ({ types, location }) =>
      // 1. check that the pause point is a "break" point
      types.break &&
      // 2. check that there is a registered breakpoint on the line
      breakpointMap[location.line] &&
      // 3. check that the breakpoint is visible
      viewport &&
      contains(location, viewport)
  );

  // 4. Only show one column breakpoint per generated location
  columnBreakpoints = sortedUniqBy(columnBreakpoints, ({ generatedLocation }) =>
    makeLocationId(generatedLocation)
  );

  // 5. Check that there is atleast one other possible breakpoint on the line
  const lineCount = getLineCount(columnBreakpoints);
  columnBreakpoints = columnBreakpoints.filter(
    ({ location: { line } }) => lineCount[line] > 1
  );

  return (columnBreakpoints: any).map(({ location }) => ({
    location,
    breakpoint: findBreakpoint(location, breakpointMap)
  }));
}

export const visibleColumnBreakpoints: Selector<
  ColumnBreakpoints
> = createSelector(
  getVisiblePausePoints,
  getVisibleBreakpoints,
  getViewport,
  getColumnBreakpoints
);
