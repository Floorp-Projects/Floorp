/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */

// @flow

import { getBreakpoints } from "../reducers/breakpoints";
import { getSelectedSource } from "../reducers/sources";
import { isGeneratedId } from "devtools-source-map";
import { createSelector } from "reselect";
import memoize from "../utils/memoize";

import type { BreakpointsMap } from "../reducers/types";
import type { Source } from "../types";

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
  getBreakpoints,
  (selectedSource: Source, breakpoints: BreakpointsMap) => {
    if (!selectedSource) {
      return null;
    }

    return breakpoints
      .filter(bp => isVisible(bp, selectedSource))
      .map(bp => formatBreakpoint(bp, selectedSource));
  }
);
