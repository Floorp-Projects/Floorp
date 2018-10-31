/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */

// @flow

import { sortBy, uniq } from "lodash";
import { createSelector } from "reselect";
import { getSources, getBreakpoints } from "../selectors";
import { getFilename } from "../utils/source";

import type { Source, Breakpoint } from "../types";
import type { SourcesMap, BreakpointsMap } from "../reducers/types";

export type BreakpointSources = Array<{
  source: Source,
  breakpoints: Breakpoint[]
}>;

function getBreakpointsForSource(
  source: Source,
  breakpoints: BreakpointsMap
): Breakpoint[] {
  const bpList = breakpoints.valueSeq();

  return bpList
    .filter(
      bp =>
        bp.location.sourceId == source.id &&
        !bp.hidden &&
        (bp.text || bp.originalText || bp.condition || bp.disabled)
    )
    .sortBy(bp => bp.location.line)
    .toJS();
}

function findBreakpointSources(
  sources: SourcesMap,
  breakpoints: BreakpointsMap
): Source[] {
  const sourceIds: string[] = uniq(
    breakpoints
      .valueSeq()
      .map(bp => bp.location.sourceId)
      .toJS()
  );

  const breakpointSources = sourceIds
    .map(id => sources[id])
    .filter(source => source && !source.isBlackBoxed);

  return sortBy(breakpointSources, (source: Source) => getFilename(source));
}

export const getBreakpointSources = createSelector(
  getBreakpoints,
  getSources,
  (breakpoints: BreakpointsMap, sources: SourcesMap) =>
    findBreakpointSources(sources, breakpoints)
      .map(source => ({
        source,
        breakpoints: getBreakpointsForSource(source, breakpoints)
      }))
      .filter(({ breakpoints: bpSources }) => bpSources.length > 0)
);
