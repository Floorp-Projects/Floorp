/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */

import { groupBy, hasIn } from "lodash";
import { createSelector } from "reselect";

import { getViewport } from "../selectors";
import { getVisibleBreakpoints } from "./visibleBreakpoints";
import { getVisiblePausePoints } from "./visiblePausePoints";

import type { Location } from "../types";

export type ColumnBreakpoint = {|
  +location: Location,
  +enabled: boolean
|};

function contains(location, range) {
  return (
    location.line >= range.start.line &&
    location.line <= range.end.line &&
    location.column >= range.start.column &&
    location.column <= range.end.column
  );
}

function groupBreakpoints(breakpoints) {
  const map = groupBy(breakpoints, ({ location }) => location.line);
  for (const line in map) {
    map[line] = groupBy(map[line], ({ location }) => location.column);
  }

  return map;
}

function isEnabled(location, breakpointMap) {
  const { line, column } = location;
  return hasIn(breakpointMap, [line, column]);
}

export function formatColumnBreakpoints(columnBreakpoints) {
  console.log(
    "Column Breakpoints\n\n",
    columnBreakpoints
      .map(
        ({ location, enabled }) =>
          `(${location.line}, ${location.column}) ${enabled}`
      )
      .join("\n")
  );
}

export function getColumnBreakpoints(pausePoints, breakpoints, viewport) {
  if (!pausePoints) {
    return [];
  }

  const breakpointMap = groupBreakpoints(breakpoints);
  const columnBreakpoints = pausePoints
    .filter(({ types }) => types.break)
    .filter(({ location }) => breakpointMap[location.line])
    .filter(({ location }) => viewport && contains(location, viewport));

  return columnBreakpoints.map(({ location }) => ({
    location,
    enabled: isEnabled(location, breakpointMap)
  }));
}

export const visibleColumnBreakpoints = createSelector(
  getVisiblePausePoints,
  getVisibleBreakpoints,
  getViewport,
  getColumnBreakpoints
);
