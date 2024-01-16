/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */

import { createSelector } from "devtools/client/shared/vendor/reselect";

import { getBreakpointsList } from "./breakpoints";
import { getSelectedSource } from "./sources";

import { sortSelectedBreakpoints } from "../utils/breakpoint/index";
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
        getSelectedLocation(bp, selectedSource)?.source.id === selectedSource.id
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

    // Filter the array so it only return the first breakpoint when there's multiple
    // breakpoints on the same line.
    const handledLines = new Set();
    return sortSelectedBreakpoints(breakpoints, selectedSource).filter(bp => {
      const line = getSelectedLocation(bp, selectedSource).line;
      if (handledLines.has(line)) {
        return false;
      }
      handledLines.add(line);
      return true;
    });
  }
);
