/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */

// @flow

import { sortBy, uniq } from "lodash";
import { createSelector } from "reselect";
import {
  getSources,
  getSourceInSources,
  getBreakpointsList,
  getSelectedSource,
} from "../selectors";
import { getFilename } from "../utils/source";
import { getSelectedLocation } from "../utils/selected-location";
import { sortSelectedBreakpoints } from "../utils/breakpoint";

import type { Source, Breakpoint } from "../types";
import type { Selector, SourceResourceState } from "../reducers/types";

export type BreakpointSources = Array<{
  source: Source,
  breakpoints: Breakpoint[],
}>;

function getBreakpointsForSource(
  source: Source,
  selectedSource: ?Source,
  breakpoints: Breakpoint[]
) {
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

function findBreakpointSources(
  sources: SourceResourceState,
  breakpoints: Breakpoint[],
  selectedSource: ?Source
): Source[] {
  const sourceIds: string[] = uniq(
    breakpoints.map(bp => getSelectedLocation(bp, selectedSource).sourceId)
  );

  const breakpointSources = sourceIds.reduce((acc, id) => {
    const source = getSourceInSources(sources, id);
    if (source && !source.isBlackBoxed) {
      acc.push(source);
    }
    return acc;
  }, []);

  return sortBy(breakpointSources, (source: Source) => getFilename(source));
}

export const getBreakpointSources: Selector<BreakpointSources> = createSelector(
  getBreakpointsList,
  getSources,
  getSelectedSource,
  (
    breakpoints: Breakpoint[],
    sources: SourceResourceState,
    selectedSource: ?Source
  ) =>
    findBreakpointSources(sources, breakpoints, selectedSource)
      .map(source => ({
        source,
        breakpoints: getBreakpointsForSource(
          source,
          selectedSource,
          breakpoints
        ),
      }))
      .filter(({ breakpoints: bpSources }) => bpSources.length > 0)
);
