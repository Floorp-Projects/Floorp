/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */

import { createSelector } from "reselect";
import { getSelectedSource, getSourceFromId } from "./sources";
import { getBreakpointsList } from "./breakpoints";
import { getFilename } from "../utils/source";
import { getSelectedLocation } from "../utils/selected-location";
import { sortSelectedBreakpoints } from "../utils/breakpoint";

function getBreakpointsForSource(source, selectedSource, breakpoints) {
  return sortSelectedBreakpoints(breakpoints, selectedSource)
    .filter(
      bp =>
        !bp.options.hidden &&
        (bp.text || bp.originalText || bp.options.condition || bp.disabled)
    )
    .filter(
      bp => getSelectedLocation(bp, selectedSource).sourceId == source.id
    );
}

const getSourcesForBreakpoints = state => {
  const selectedSource = getSelectedSource(state);
  const breakpointSourceIds = getBreakpointsList(state).map(
    breakpoint => getSelectedLocation(breakpoint, selectedSource).sourceId
  );

  return [...new Set(breakpointSourceIds)]
    .map(sourceId => {
      const source = getSourceFromId(state, sourceId);
      const filename = getFilename(source);
      return { source, filename };
    })
    .filter(({ source }) => source && !source.isBlackBoxed)
    .sort((a, b) => a.filename - b.filename)
    .map(({ source }) => source);
};

export const getBreakpointSources = createSelector(
  getBreakpointsList,
  getSourcesForBreakpoints,
  getSelectedSource,
  (breakpoints, sources, selectedSource) => {
    return sources
      .map(source => ({
        source,
        breakpoints: getBreakpointsForSource(
          source,
          selectedSource,
          breakpoints
        ),
      }))
      .filter(({ breakpoints: bps }) => bps.length > 0);
  }
);
