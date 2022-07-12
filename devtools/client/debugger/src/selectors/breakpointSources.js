/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */

import { createSelector } from "reselect";
import { getSelectedSource, getSourcesMap } from "./sources";
import { getBreakpointsList } from "./breakpoints";
import { getBlackBoxRanges } from "./source-blackbox";
import { getFilename } from "../utils/source";
import { getSelectedLocation } from "../utils/selected-location";
import { sortSelectedBreakpoints } from "../utils/breakpoint";

// Returns all the breakpoints for the given selected source
// Depending on the selected source, this will match original or generated
// location of the given selected source.
function _getBreakpointsForSource(visibleBreakpoints, source, selectedSource) {
  return visibleBreakpoints.filter(
    bp => getSelectedLocation(bp, selectedSource).sourceId == source.id
  );
}

// Returns a sorted list of sources for which we have breakpoints
// We will return generated or original source IDs based on the currently selected source.
const _getSourcesForBreakpoints = (
  breakpoints,
  sourcesMap,
  selectedSource,
  blackBoxRanges
) => {
  const breakpointSourceIds = breakpoints.map(
    breakpoint => getSelectedLocation(breakpoint, selectedSource).sourceId
  );

  const sources = [];
  // We may have more than one breakpoint per sourceId,
  // so use a Set to have a unique list of source IDs.
  for (const sourceId of [...new Set(breakpointSourceIds)]) {
    const source = sourcesMap.get(sourceId);

    // Ignore any source that is no longer in the sources reducer
    // or blackboxed sources.
    if (!source || blackBoxRanges[source.url]) {
      continue;
    }

    const bps = _getBreakpointsForSource(breakpoints, source, selectedSource);

    // Ignore sources which have no breakpoints
    if (bps.length === 0) {
      continue;
    }

    sources.push({
      source,
      breakpoints: bps,
      filename: getFilename(source),
    });
  }

  return sources.sort((a, b) => a.filename.localeCompare(b.filename));
};

// Returns a list of sources with their related breakpoints:
//   [{ source, breakpoints [breakpoint1, ...] }, ...]
//
// This only returns sources for which we have a visible breakpoint.
// This will return either generated or original source based on the currently
// selected source.
export const getBreakpointSources = createSelector(
  getBreakpointsList,
  getSourcesMap,
  getSelectedSource,
  getBlackBoxRanges,
  (breakpoints, sourcesMap, selectedSource, blackBoxRanges) => {
    const visibleBreakpoints = breakpoints.filter(
      bp =>
        !bp.options.hidden &&
        (bp.text || bp.originalText || bp.options.condition || bp.disabled)
    );

    const sortedVisibleBreakpoints = sortSelectedBreakpoints(
      visibleBreakpoints,
      selectedSource
    );

    return _getSourcesForBreakpoints(
      sortedVisibleBreakpoints,
      sourcesMap,
      selectedSource,
      blackBoxRanges
    );
  }
);
