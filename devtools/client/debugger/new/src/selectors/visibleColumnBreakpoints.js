// @flow
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */

import { groupBy, sortedUniqBy } from "lodash";
import { createSelector } from "reselect";

import {
  getViewport,
  getSelectedSource,
  getBreakpointPositions
} from "../selectors";
import { getVisibleBreakpoints } from "./visibleBreakpoints";
import { makeBreakpointId } from "../utils/breakpoint";
import type { Selector } from "../reducers/types";

import type {
  SourceLocation,
  PartialPosition,
  Breakpoint,
  Range,
  BreakpointPositions
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

function inViewport(viewport, location) {
  return viewport && contains(location, viewport);
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
  positions: ?BreakpointPositions,
  breakpoints: ?(Breakpoint[]),
  viewport: Range
) {
  if (!positions) {
    return [];
  }

  const breakpointMap = groupBreakpoints(breakpoints);

  // We only want to show a column breakpoint if several conditions are matched
  // 1. there is a breakpoint on that line
  // 2. the position is in the current viewport
  // 4. it is the first breakpoint to appear at that generated location
  // 5. there is atleast one other breakpoint on that line

  let columnBreakpoints = positions.filter(
    ({ location }) =>
      breakpointMap[location.line] && inViewport(viewport, location)
  );

  // 4. Only show one column breakpoint per generated location
  columnBreakpoints = sortedUniqBy(columnBreakpoints, ({ generatedLocation }) =>
    makeBreakpointId(generatedLocation)
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

const getVisibleBreakpointPositions = createSelector(
  getSelectedSource,
  getBreakpointPositions,
  (source, positions) => source && positions[source.id]
);

export const visibleColumnBreakpoints: Selector<
  ColumnBreakpoints
> = createSelector(
  getVisibleBreakpointPositions,
  getVisibleBreakpoints,
  getViewport,
  getSelectedSource,
  getColumnBreakpoints
);
