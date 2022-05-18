/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */

import { getLocationSource, getSourceActorsForSource } from "../../selectors";
import { isGenerated } from "../source";
import { sortSelectedLocations } from "../location";
import assert from "../assert";
export * from "./breakpointPositions";

// The ID for a Breakpoint is derived from its location in its Source.
export function makeBreakpointId(location) {
  const { sourceId, line, column } = location;
  const columnString = column || "";
  return `${sourceId}:${line}:${columnString}`;
}

export function getLocationWithoutColumn(location) {
  const { sourceId, line } = location;
  return `${sourceId}:${line}`;
}

export function makePendingLocationId(location) {
  assertPendingLocation(location);
  const { sourceUrl, line, column } = location;
  const sourceUrlString = sourceUrl || "";
  const columnString = column || "";

  return `${sourceUrlString}:${line}:${columnString}`;
}

export function makeBreakpointLocation(state, location) {
  const source = getLocationSource(state, location);
  if (!source) {
    throw new Error("no source");
  }
  const breakpointLocation = {
    line: location.line,
    column: location.column,
  };
  if (source.url) {
    breakpointLocation.sourceUrl = source.url;
  } else {
    breakpointLocation.sourceId = getSourceActorsForSource(
      state,
      source.id
    )[0].id;
  }
  return breakpointLocation;
}

export function assertPendingBreakpoint(pendingBreakpoint) {
  assertPendingLocation(pendingBreakpoint.location);
  assertPendingLocation(pendingBreakpoint.generatedLocation);
}

export function assertPendingLocation(location) {
  assert(!!location, "location must exist");

  const { sourceUrl } = location;

  // sourceUrl is null when the source does not have a url
  assert(sourceUrl !== undefined, "location must have a source url");
  assert(location.hasOwnProperty("line"), "location must have a line");
  assert(
    location.hasOwnProperty("column") != null,
    "location must have a column"
  );
}

export function createXHRBreakpoint(path, method, overrides = {}) {
  const properties = {
    path,
    method,
    disabled: false,
    loading: false,
    text: L10N.getFormatStr("xhrBreakpoints.item.label", path),
  };

  return { ...properties, ...overrides };
}

function createPendingLocation(location) {
  const { sourceUrl, line, column } = location;
  return { sourceUrl, line, column };
}

export function createPendingBreakpoint(bp) {
  const pendingLocation = createPendingLocation(bp.location);
  const pendingGeneratedLocation = createPendingLocation(bp.generatedLocation);

  assertPendingLocation(pendingLocation);

  return {
    options: bp.options,
    disabled: bp.disabled,
    location: pendingLocation,
    generatedLocation: pendingGeneratedLocation,
  };
}

export function getSelectedText(breakpoint, selectedSource) {
  return !!selectedSource && isGenerated(selectedSource)
    ? breakpoint.text
    : breakpoint.originalText;
}

export function sortSelectedBreakpoints(breakpoints, selectedSource) {
  return sortSelectedLocations(breakpoints, selectedSource);
}
