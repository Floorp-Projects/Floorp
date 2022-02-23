/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */

import { createSelector } from "reselect";
import { uniqBy } from "lodash";

import { getBreakpointsList } from "../reducers/breakpoints";
import { getSelectedSource } from "../reducers/sources";

import { sortSelectedBreakpoints } from "../utils/breakpoint";
import { getSelectedLocation } from "../utils/selected-location";

/*
 * Finds the breakpoints, which appear in the selected source.
 */
export const getVisibleBreakpoints = createSelector(
  getSelectedSource,
  getBreakpointsList,
  (selectedSource, breakpoints) => {
    if (!selectedSource) {
      return null;
    }

    return breakpoints.filter(
      bp =>
        selectedSource &&
        getSelectedLocation(bp, selectedSource).sourceId === selectedSource.id
    );
  }
);

/*
 * Finds the first breakpoint per line, which appear in the selected source.
 */
export const getFirstVisibleBreakpoints = createSelector(
  getVisibleBreakpoints,
  getSelectedSource,
  (breakpoints, selectedSource) => {
    if (!breakpoints || !selectedSource) {
      return [];
    }

    return uniqBy(
      sortSelectedBreakpoints(breakpoints, selectedSource),
      bp => getSelectedLocation(bp, selectedSource).line
    );
  }
);
