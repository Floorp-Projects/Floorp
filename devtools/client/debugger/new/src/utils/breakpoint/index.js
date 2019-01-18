/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */

// @flow

import { sortBy } from "lodash";

import { getBreakpoint } from "../../selectors";
import assert from "../assert";
import { features } from "../prefs";

export { getASTLocation, findScopeByName } from "./astBreakpointLocation";

import type { FormattedBreakpoint } from "../../selectors/breakpointSources";
import type {
  SourceLocation,
  PendingLocation,
  Breakpoint,
  PendingBreakpoint
} from "../../types";

import type { State } from "../../reducers/types";

// Return the first argument that is a string, or null if nothing is a
// string.
export function firstString(...args: string[]) {
  for (const arg of args) {
    if (typeof arg === "string") {
      return arg;
    }
  }
  return null;
}

export function locationMoved(
  location: SourceLocation,
  newLocation: SourceLocation
) {
  return (
    location.line !== newLocation.line || location.column !== newLocation.column
  );
}

export function makeLocationId(location: SourceLocation) {
  const { sourceId, line, column } = location;
  const columnString = column || "";
  return `${sourceId}:${line}:${columnString}`;
}

export function getLocationWithoutColumn(location: SourceLocation) {
  const { sourceId, line } = location;
  return `${sourceId}:${line}`;
}

export function makePendingLocationId(location: SourceLocation) {
  assertPendingLocation(location);
  const { sourceUrl, line, column } = location;
  const sourceUrlString = sourceUrl || "";
  const columnString = column || "";

  return `${sourceUrlString}:${line}:${columnString}`;
}

export function assertBreakpoint(breakpoint: Breakpoint) {
  assertLocation(breakpoint.location);
  assertLocation(breakpoint.generatedLocation);
}

export function assertPendingBreakpoint(pendingBreakpoint: PendingBreakpoint) {
  assertPendingLocation(pendingBreakpoint.location);
  assertPendingLocation(pendingBreakpoint.generatedLocation);
}

export function assertLocation(location: SourceLocation) {
  assertPendingLocation(location);
  const { sourceId } = location;
  assert(!!sourceId, "location must have a source id");
}

export function assertPendingLocation(location: PendingLocation) {
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

// syncing
export function breakpointAtLocation(
  breakpoints: Breakpoint[],
  { line, column }: SourceLocation
) {
  return breakpoints.find(breakpoint => {
    const sameLine = breakpoint.location.line === line;
    if (!sameLine) {
      return false;
    }

    // NOTE: when column breakpoints are disabled we want to find
    // the first breakpoint
    if (!features.columnBreakpoints) {
      return true;
    }

    return breakpoint.location.column === column;
  });
}

export function breakpointExists(state: State, location: SourceLocation) {
  const currentBp = getBreakpoint(state, location);
  return currentBp && !currentBp.disabled;
}

export function createBreakpoint(
  location: SourceLocation,
  overrides: Object = {}
): Breakpoint {
  const {
    condition,
    disabled,
    hidden,
    generatedLocation,
    astLocation,
    id,
    text,
    originalText,
    log
  } = overrides;

  const defaultASTLocation = {
    name: undefined,
    offset: location,
    index: 0
  };
  const properties = {
    id,
    condition: condition || null,
    log: log || false,
    disabled: disabled || false,
    hidden: hidden || false,
    loading: false,
    astLocation: astLocation || defaultASTLocation,
    generatedLocation: generatedLocation || location,
    location,
    text,
    originalText
  };

  return properties;
}

export function createXHRBreakpoint(
  path: string,
  method: string,
  overrides?: Object = {}
) {
  const properties = {
    path,
    method,
    disabled: false,
    loading: false,
    text: L10N.getFormatStr("xhrBreakpoints.item.label", path)
  };

  return { ...properties, ...overrides };
}

function createPendingLocation(location: PendingLocation) {
  const { sourceUrl, line, column } = location;
  return { sourceUrl, line, column };
}

export function createPendingBreakpoint(bp: Breakpoint) {
  const pendingLocation = createPendingLocation(bp.location);
  const pendingGeneratedLocation = createPendingLocation(bp.generatedLocation);

  assertPendingLocation(pendingLocation);

  return {
    condition: bp.condition,
    log: bp.log,
    disabled: bp.disabled,
    location: pendingLocation,
    astLocation: bp.astLocation,
    generatedLocation: pendingGeneratedLocation
  };
}

export function sortFormattedBreakpoints(breakpoints: FormattedBreakpoint[]) {
  return _sortBreakpoints(breakpoints, "selectedLocation");
}

export function sortBreakpoints(breakpoints: Breakpoint[]) {
  return _sortBreakpoints(breakpoints, "location");
}

function _sortBreakpoints(
  breakpoints: Array<Object>,
  property: string
): Array<Object> {
  // prettier-ignore
  return sortBy(
    breakpoints,
    [
      // Priority: line number, undefined column, column number
      `${property}.line`,
      bp => {
        return bp[property].column === undefined || bp[property].column;
      }
    ]
  );
}
