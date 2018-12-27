/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */

// @flow

import { sortBy, uniq } from "lodash";
import { createSelector } from "reselect";
import {
  getSources,
  getBreakpointsList,
  getSelectedSource
} from "../selectors";
import { isGenerated, getFilename } from "../utils/source";
import { getSelectedLocation } from "../utils/source-maps";

import type {
  Source,
  Breakpoint,
  BreakpointId,
  SourceLocation
} from "../types";
import type { Selector, SourcesMap } from "../reducers/types";

export type BreakpointSources = Array<{
  source: Source,
  breakpoints: FormattedBreakpoint[]
}>;

export type FormattedBreakpoint = {|
  id: BreakpointId,
  condition: ?string,
  disabled: boolean,
  text: string,
  selectedLocation: SourceLocation
|};

function formatBreakpoint(
  breakpoint: Breakpoint,
  selectedSource: ?Source
): FormattedBreakpoint {
  const { id, condition, disabled } = breakpoint;

  return {
    id,
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
  selectedSource: ?Source,
  breakpoints: Breakpoint[]
) {
  return breakpoints
    .sort((a, b) => a.location.line - b.location.line)
    .filter(
      bp =>
        !bp.hidden &&
        !bp.loading &&
        (bp.text || bp.originalText || bp.condition || bp.disabled)
    )
    .map(bp => formatBreakpoint(bp, selectedSource))
    .filter(bp => bp.selectedLocation.sourceId == source.id);
}

function findBreakpointSources(
  sources: SourcesMap,
  breakpoints: Breakpoint[]
): Source[] {
  const sourceIds: string[] = uniq(breakpoints.map(bp => bp.location.sourceId));

  const breakpointSources = sourceIds
    .map(id => sources[id])
    .filter(source => source && !source.isBlackBoxed);

  return sortBy(breakpointSources, (source: Source) => getFilename(source));
}

export const getBreakpointSources: Selector<BreakpointSources> = createSelector(
  getBreakpointsList,
  getSources,
  getSelectedSource,
  (breakpoints: Breakpoint[], sources: SourcesMap, selectedSource: ?Source) =>
    findBreakpointSources(sources, breakpoints)
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
