/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */

// @flow

import { isGeneratedId } from "devtools-source-map";
import { createSelector } from "../utils/createSelector";
import { uniqBy } from "lodash";

import { getBreakpointsList } from "../reducers/breakpoints";
import { getSelectedSource } from "../reducers/sources";

import memoize from "../utils/memoize";
import { sortBreakpoints } from "../utils/breakpoint";

import type { Breakpoint, Source } from "../types";

function getLocation(breakpoint, isGeneratedSource) {
  return isGeneratedSource
    ? breakpoint.generatedLocation || breakpoint.location
    : breakpoint.location;
}

const formatBreakpoint = memoize(function(breakpoint, selectedSource) {
  const { condition, loading, disabled, hidden } = breakpoint;
  const sourceId = selectedSource.id;
  const isGeneratedSource = isGeneratedId(sourceId);

  return {
    location: getLocation(breakpoint, isGeneratedSource),
    condition,
    loading,
    disabled,
    hidden
  };
});

function isVisible(breakpoint, selectedSource) {
  const sourceId = selectedSource.id;
  const isGeneratedSource = isGeneratedId(sourceId);

  const location = getLocation(breakpoint, isGeneratedSource);
  return location.sourceId === sourceId;
}

/*
 * Finds the breakpoints, which appear in the selected source.
 */
export const getVisibleBreakpoints = createSelector(
  getSelectedSource,
  getBreakpointsList,
  (selectedSource: Source, breakpoints: Breakpoint[]) => {
    if (!selectedSource) {
      return null;
    }

    return breakpoints
      .filter(bp => isVisible(bp, selectedSource))
      .map(bp => formatBreakpoint(bp, selectedSource));
  }
);

/*
 * Finds the first breakpoint per line, which appear in the selected source.
 */
export const getFirstVisibleBreakpoints = createSelector(
  getVisibleBreakpoints,
  breakpoints => {
    if (!breakpoints) {
      return null;
    }

    return uniqBy(sortBreakpoints(breakpoints), bp => bp.location.line);
  }
);
