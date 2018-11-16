/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */

// @flow

import { sortBy, uniq } from "lodash";
import { createSelector } from "reselect";
import { getSources, getBreakpoints, getSelectedSource } from "../selectors";
import { isGenerated, getFilename } from "../utils/source";
import { getSelectedLocation } from "../utils/source-maps";

import type { Source, Breakpoint, Location } from "../types";
import type { SourcesMap, BreakpointsMap } from "../reducers/types";

export type BreakpointSources = Array<{
  source: Source,
  breakpoints: FormattedBreakpoint[]
}>;

export type FormattedBreakpoint = {|
  condition: ?string,
  disabled: boolean,
  text: string,
  selectedLocation: Location
|};

function formatBreakpoint(
  breakpoint: Breakpoint,
  selectedSource: Source
): FormattedBreakpoint {
  const { condition, disabled } = breakpoint;

  return {
    condition,
    disabled,
    text:
      selectedSource && isGenerated(selectedSource)
        ? breakpoint.text
        : breakpoint.originalText,
    selectedLocation: getSelectedLocation(breakpoint, selectedSource)
  };
}

function getBreakpointsForSource(
  source: Source,
  selectedSource: Source,
  breakpoints: BreakpointsMap
): Breakpoint[] {
  const bpList = breakpoints.valueSeq();
  return bpList
    .map(bp => formatBreakpoint(bp, selectedSource))
    .filter(
      bp =>
        bp.selectedLocation.sourceId == source.id &&
        !bp.hidden &&
        !bp.loading &&
        (bp.text || bp.originalText || bp.condition || bp.disabled)
    )
    .sortBy(bp => bp.selectedLocation.line)
    .toJS();
}

function findBreakpointSources(
  sources: SourcesMap,
  selectedSource: Source,
  breakpoints: BreakpointsMap
): Source[] {
  const sourceIds: string[] = uniq(
    breakpoints
      .valueSeq()
      .map(bp => getSelectedLocation(bp, selectedSource).sourceId)
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
  getSelectedSource,
  (breakpoints: BreakpointsMap, sources: SourcesMap, selectedSource: Source) =>
    findBreakpointSources(sources, selectedSource, breakpoints)
      .map(source => ({
        source,
        breakpoints: getBreakpointsForSource(
          source,
          selectedSource,
          breakpoints
        )
      }))
      .filter(({ breakpoints: bpSources }) => bpSources.length > 0)
);
