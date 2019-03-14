// @flow
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */

import { groupBy } from "lodash";
import { createSelector } from "reselect";

import {
  getViewport,
  getSource,
  getSelectedSource,
  getBreakpointPositions,
  getBreakpointPositionsForSource
} from "../selectors";
import { getVisibleBreakpoints } from "./visibleBreakpoints";
import { getSelectedLocation } from "../utils/source-maps";
import type { Selector, State } from "../reducers/types";

import type {
  SourceLocation,
  PartialPosition,
  Breakpoint,
  Range,
  BreakpointPositions,
  BreakpointPosition,
  Source
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

function groupBreakpoints(breakpoints, selectedSource) {
  if (!breakpoints) {
    return {};
  }
  const map: any = groupBy(
    breakpoints,
    breakpoint => getSelectedLocation(breakpoint, selectedSource).line
  );

  for (const line in map) {
    map[line] = groupBy(
      map[line],
      breakpoint => getSelectedLocation(breakpoint, selectedSource).column
    );
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

function filterByLineCount(positions, selectedSource) {
  const lineCount = {};

  for (const breakpoint of positions) {
    const { line } = getSelectedLocation(breakpoint, selectedSource);
    if (!lineCount[line]) {
      lineCount[line] = 0;
    }

    lineCount[line] = lineCount[line] + 1;
  }

  return positions.filter(
    breakpoint =>
      lineCount[getSelectedLocation(breakpoint, selectedSource).line] > 1
  );
}

function filterVisible(positions, selectedSource, viewport) {
  return positions.filter(columnBreakpoint => {
    const location = getSelectedLocation(columnBreakpoint, selectedSource);
    return viewport && contains(location, viewport);
  });
}

function filterByBreakpoints(positions, selectedSource, breakpointMap) {
  return positions.filter(position => {
    const location = getSelectedLocation(position, selectedSource);
    return breakpointMap[location.line];
  });
}

function formatPositions(
  positions: BreakpointPositions,
  selectedSource,
  breakpointMap
) {
  return (positions: any).map((position: BreakpointPosition) => {
    const location = getSelectedLocation(position, selectedSource);
    return {
      location,
      breakpoint: findBreakpoint(location, breakpointMap)
    };
  });
}

export function getColumnBreakpoints(
  positions: ?BreakpointPositions,
  breakpoints: ?(Breakpoint[]),
  viewport: Range,
  selectedSource: ?Source
) {
  if (!positions) {
    return [];
  }

  // We only want to show a column breakpoint if several conditions are matched
  // - it is the first breakpoint to appear at an the original location
  // - the position is in the current viewport
  // - there is atleast one other breakpoint on that line
  // - there is a breakpoint on that line
  const breakpointMap = groupBreakpoints(breakpoints, selectedSource);

  positions = filterByLineCount(positions, selectedSource);
  positions = filterVisible(positions, selectedSource, viewport);
  positions = filterByBreakpoints(positions, selectedSource, breakpointMap);

  return formatPositions(positions, selectedSource, breakpointMap);
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

export function getFirstBreakpointPosition(
  state: State,
  { line, sourceId }: SourceLocation
) {
  const positions = getBreakpointPositionsForSource(state, sourceId);
  const source = getSource(state, sourceId);

  if (!source || !positions) {
    return;
  }

  return positions.find(
    position => getSelectedLocation(position, source).line == line
  );
}
