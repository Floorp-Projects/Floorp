/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */

// @flow

import { createSelector } from "reselect";
import { uniqBy } from "lodash";

import {
  getBreakpointsList,
  getBreakpointPositionsForLine,
  getBreakpointPositions
} from "../reducers/breakpoints";
import { getPausePoints } from "../reducers/ast";
import { getSelectedSource } from "../reducers/sources";

import { sortBreakpoints } from "../utils/breakpoint";
import { getSelectedLocation } from "../utils/source-maps";

import type {
  Breakpoint,
  Source,
  SourceLocation,
  BreakpointPositions
} from "../types";
import type { Selector, State } from "../reducers/types";

function isVisible(breakpoint: Breakpoint, selectedSource: Source) {
  const location = getSelectedLocation(breakpoint, selectedSource);
  return location.sourceId === selectedSource.id;
}

/*
 * Finds the breakpoints, which appear in the selected source.
 */
export const getVisibleBreakpoints: Selector<?(Breakpoint[])> = createSelector(
  getSelectedSource,
  getBreakpointsList,
  getBreakpointPositions,
  (
    selectedSource: ?Source,
    breakpoints: Breakpoint[],
    positions: ?BreakpointPositions
  ) => {
    if (selectedSource == null) {
      return null;
    }

    // FIXME: Even though selectedSource is checked above, it fails type
    // checking for isVisible
    const source: Source = selectedSource;
    return breakpoints.filter(bp => isVisible(bp, source));
  }
);

export function getFirstVisibleBreakpointPosition(
  state: State,
  location: SourceLocation
): ?SourceLocation {
  const { sourceId, line } = location;
  const pausePoints = getPausePoints(state, location.sourceId);
  const positions = getBreakpointPositionsForLine(state, sourceId, line);

  if (!pausePoints || !positions) {
    return null;
  }

  const pausesAtLine = pausePoints.filter(
    p => p.location.line == line && positions.includes(p.location.column)
  );

  if (pausesAtLine.length > 0) {
    const firstPausePoint = pausesAtLine.find(
      pausePoint => pausePoint.types.break
    );
    if (firstPausePoint) {
      return { ...firstPausePoint.location, sourceId };
    }
  }
  return location;
}

/*
 * Finds the first breakpoint per line, which appear in the selected source.
 */
export const getFirstVisibleBreakpoints: Selector<
  Breakpoint[]
> = createSelector(getVisibleBreakpoints, breakpoints => {
  if (!breakpoints) {
    return [];
  }

  return (uniqBy(sortBreakpoints(breakpoints), bp => bp.location.line): any);
});
