/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */

// @flow

import { createSelector } from "reselect";
import { uniqBy } from "lodash";

import { getBreakpointsList } from "../reducers/breakpoints";
import { getSelectedSource } from "../reducers/sources";

import { sortBreakpoints } from "../utils/breakpoint";
import { getSelectedLocation } from "../utils/source-maps";

import type { Breakpoint, Source } from "../types";
import type { Selector } from "../reducers/types";

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
  (selectedSource: ?Source, breakpoints: Breakpoint[]) => {
    if (selectedSource == null) {
      return null;
    }

    // FIXME: Even though selectedSource is checked above, it fails type
    // checking for isVisible
    const source: Source = selectedSource;
    return breakpoints.filter(bp => isVisible(bp, source));
  }
);

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
