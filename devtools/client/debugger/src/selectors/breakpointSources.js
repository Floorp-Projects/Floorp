/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */

import { createSelector } from "devtools/client/shared/vendor/reselect";
import { getSelectedSource } from "./sources";
import { getBreakpointsList } from "./breakpoints";
import { getFilename } from "../utils/source";
import { getSelectedLocation } from "../utils/selected-location";

// Returns a list of sources with their related breakpoints:
//   [{ source, breakpoints: [breakpoint1, ...] }, ...]
//
// This only returns sources for which we have a visible breakpoint.
// This will return either generated or original source based on the currently
// selected source.
export const getBreakpointSources = createSelector(
  getBreakpointsList,
  getSelectedSource,
  (breakpoints, selectedSource) => {
    const visibleBreakpoints = breakpoints.filter(
      bp =>
        !bp.options.hidden &&
        (bp.text || bp.originalText || bp.options.condition || bp.disabled)
    );

    const sources = new Map();
    for (const breakpoint of visibleBreakpoints) {
      // Depending on the selected source, this will match the original or generated
      // location of the given selected source.
      const location = getSelectedLocation(breakpoint, selectedSource);
      const { source } = location;

      // We may have more than one breakpoint per source,
      // so use the map to have a unique entry per source.
      if (!sources.has(source)) {
        sources.set(source, {
          source,
          breakpoints: [breakpoint],
          filename: getFilename(source),
        });
      } else {
        sources.get(source).breakpoints.push(breakpoint);
      }
    }

    // Returns an array of breakpoints info per source, sorted by source's filename
    return [...sources.values()].sort((a, b) =>
      a.filename.localeCompare(b.filename)
    );
  }
);
